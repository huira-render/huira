#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "huira/util/logger.hpp"
#include "lz4.h"
#include "ogr_spatialref.h"

namespace fs = std::filesystem;

namespace huira::qld {
namespace {

constexpr std::uint16_t CLASS_FLOAT = 1;
constexpr std::uint16_t CLASS_DOUBLE = 2;
constexpr std::uint16_t CLASS_UINT8 = 3;
constexpr std::uint16_t CLASS_UINT16 = 4;
constexpr std::uint16_t CLASS_DEM_PYRAMID = 2002;
constexpr std::uint8_t CONSTANT_ALBEDO = 0;
constexpr std::uint8_t FLOAT_IMAGE_ALBEDO = 1;
constexpr std::uint8_t COLOR_IMAGE_ALBEDO = 2;

struct Projection {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::array<double, 2> pixel_scale{0.0, 0.0};
    std::array<std::uint32_t, 2> tile_definitions{0, 0};
    std::uint32_t tile_row = 0;
    std::uint32_t tile_col = 0;
    double xoff = 0.0;
    double yoff = 0.0;
    double dx = 1.0;
    double dy = 1.0;
    bool is_ogr = false;
    std::string proj_ref;
    std::array<double, 2> row_col_rotation{0.0, 0.0};
    std::array<double, 2> model_tie_point{0.0, 0.0};
};

struct TileMesh {
    huira::IndexBuffer indices;
    huira::VertexBuffer<huira::Visible8> vertices;
    huira::Vec3<double> position{0.0};
    huira::Mat3<double> rotation{1.0};
    huira::Vec3<double> scale{1.0};
};
static huira::Tangent tangent_from_normal_(const huira::Vec3<float>& normal)
{
    const float sign = std::copysign(1.0f, normal.z);
    const float a = -1.0f / (sign + normal.z);
    const float b = normal.x * normal.y * a;
    huira::Tangent tangent;
    tangent.tangent = huira::Vec3<float>{1.0f + sign * normal.x * normal.x * a,
                                         sign * b,
                                         -sign * normal.x};
    tangent.bitangent = glm::cross(normal, tangent.tangent);
    return tangent;
}

template <typename T>
static void read_value_(std::ifstream& file, T& value)
{
    file.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!file) {
        HUIRA_THROW_ERROR("Unexpected EOF while reading qld file");
    }
}

static void read_exact_(std::ifstream& file, char* dst, std::size_t n)
{
    file.read(dst, static_cast<std::streamsize>(n));
    if (!file) {
        HUIRA_THROW_ERROR("Unexpected EOF while reading qld buffer");
    }
}

template <typename T, int N>
static huira::Vec<N, T> read_vec_(std::ifstream& file)
{
    std::uint16_t class_id;
    std::uint8_t n_load;
    read_value_(file, class_id);
    read_value_(file, n_load);
    if (n_load != N) {
        HUIRA_THROW_ERROR("Unexpected qld vector dimension");
    }

    huira::Vec<N, T> out{0};
    for (int i = 0; i < N; ++i) {
        if (class_id == CLASS_FLOAT) {
            float v;
            read_value_(file, v);
            out[i] = static_cast<T>(v);
        } else if (class_id == CLASS_DOUBLE) {
            double v;
            read_value_(file, v);
            out[i] = static_cast<T>(v);
        } else {
            HUIRA_THROW_ERROR("Unsupported qld vector scalar type");
        }
    }
    return out;
}

template <typename T, int N, int M>
static huira::Mat<N, M, T> read_mat_(std::ifstream& file)
{
    std::uint16_t class_id;
    std::uint8_t n_load, m_load;
    read_value_(file, class_id);
    read_value_(file, n_load);
    read_value_(file, m_load);
    if (n_load != N || m_load != M) {
        HUIRA_THROW_ERROR("Unexpected qld matrix dimension");
    }

    huira::Mat<N, M, T> out{0};
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < M; ++j) {
            if (class_id == CLASS_FLOAT) {
                float v;
                read_value_(file, v);
                out[i][j] = static_cast<T>(v);
            } else if (class_id == CLASS_DOUBLE) {
                double v;
                read_value_(file, v);
                out[i][j] = static_cast<T>(v);
            } else {
                HUIRA_THROW_ERROR("Unsupported qld matrix scalar type");
            }
        }
    }
    return out;
}

static std::string read_string_(std::ifstream& file)
{
    std::uint32_t size;
    read_value_(file, size);
    std::string out(size, '\0');
    if (size > 0) {
        read_exact_(file, out.data(), size);
    }
    return out;
}

static Projection read_projection_(std::ifstream& file)
{
    Projection p;
    read_value_(file, p.width);
    read_value_(file, p.height);
    read_value_(file, p.pixel_scale);
    read_value_(file, p.tile_definitions[0]);
    read_value_(file, p.tile_definitions[1]);
    read_value_(file, p.tile_row);
    read_value_(file, p.tile_col);
    read_value_(file, p.xoff);
    read_value_(file, p.yoff);
    read_value_(file, p.dx);
    read_value_(file, p.dy);
    read_value_(file, p.is_ogr);
    if (p.is_ogr) {
        p.proj_ref = read_string_(file);
        read_value_(file, p.row_col_rotation);
        read_value_(file, p.model_tie_point);
    }
    return p;
}

template <typename T>
static std::vector<T> read_image_(std::ifstream& file, std::uint32_t& width, std::uint32_t& height)
{
    std::uint16_t class_id;
    read_value_(file, class_id);
    if ((std::is_same_v<T, float> && class_id != CLASS_FLOAT) ||
        (std::is_same_v<T, std::uint8_t> && class_id != CLASS_UINT8) ||
        (std::is_same_v<T, std::uint16_t> && class_id != CLASS_UINT16)) {
        HUIRA_THROW_ERROR("Unexpected qld image scalar type");
    }

    read_value_(file, width);
    read_value_(file, height);
    bool compressed = false;
    read_value_(file, compressed);

    std::vector<T> image(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    if (compressed) {
        std::uint64_t uncompressed_size = 0;
        std::uint64_t compressed_size = 0;
        read_value_(file, uncompressed_size);
        read_value_(file, compressed_size);
        std::vector<char> compressed_buffer(compressed_size);
        read_exact_(file, compressed_buffer.data(), compressed_buffer.size());
        if (uncompressed_size != image.size() * sizeof(T)) {
            HUIRA_THROW_ERROR("Unexpected qld compressed image size");
        }
        int decoded = LZ4_decompress_safe(
            compressed_buffer.data(),
            reinterpret_cast<char*>(image.data()),
            static_cast<int>(compressed_size),
            static_cast<int>(uncompressed_size));
        if (decoded < 0) {
            HUIRA_THROW_ERROR("Failed to decompress qld image");
        }
    } else {
        for (std::uint32_t i = 0; i < width; ++i) {
            for (std::uint32_t j = 0; j < height; ++j) {
                read_value_(file, image[static_cast<std::size_t>(j) * width + i]);
            }
        }
    }
    return image;
}

template <typename T>
static std::vector<float> fixed_to_float_(const std::vector<T>& fixed, std::array<float, 2> minmax, float max_value)
{
    std::vector<float> values(fixed.size());
    float delta = minmax[1] - minmax[0];
    float denominator = max_value - 1.0f;
    for (std::size_t i = 0; i < fixed.size(); ++i) {
        if (fixed[i] == 0) {
            values[i] = std::numeric_limits<float>::infinity();
        } else {
            values[i] = minmax[0] + delta * (static_cast<float>(fixed[i]) / denominator);
        }
    }
    return values;
}

static std::vector<float> read_fixed_u8_image_as_float_(std::ifstream& file, std::uint32_t& width, std::uint32_t& height)
{
    std::array<float, 2> minmax;
    read_value_(file, minmax);
    return fixed_to_float_(read_image_<std::uint8_t>(file, width, height), minmax, 255.0f);
}

static std::vector<huira::Visible8>
read_albedos_(std::ifstream& file, std::uint32_t expected_width, std::uint32_t expected_height)
{
    std::uint8_t albedo_type = CONSTANT_ALBEDO;
    read_value_(file, albedo_type);
    std::size_t expected_size = static_cast<std::size_t>(expected_width) * expected_height;

    if (albedo_type == CONSTANT_ALBEDO) {
        huira::Visible8 constant;
        read_value_(file, constant);
        return std::vector<huira::Visible8>(expected_size, constant);
    }

    if (albedo_type == FLOAT_IMAGE_ALBEDO) {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        auto mono = read_fixed_u8_image_as_float_(file, width, height);
        if (width != expected_width || height != expected_height) {
            HUIRA_THROW_ERROR("qld float albedo resolution mismatch");
        }
        std::vector<huira::Visible8> albedos(mono.size());
        for (std::size_t i = 0; i < mono.size(); ++i) {
            albedos[i] = huira::Visible8{mono[i]};
        }
        return albedos;
    }

    if (albedo_type == COLOR_IMAGE_ALBEDO) {
        std::uint32_t num_channels = 0;
        read_value_(file, num_channels);
        if (num_channels != huira::Visible8::size()) {
            HUIRA_THROW_ERROR("qld color albedo channel count mismatch");
        }
        bool compressed = false;
        read_value_(file, compressed);
        (void)compressed;

        std::vector<huira::Visible8> albedos(expected_size, huira::Visible8{0.0f});
        for (std::uint32_t channel = 0; channel < num_channels; ++channel) {
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            auto values = read_fixed_u8_image_as_float_(file, width, height);
            if (width != expected_width || height != expected_height) {
                HUIRA_THROW_ERROR("qld color albedo resolution mismatch");
            }
            for (std::size_t i = 0; i < values.size(); ++i) {
                albedos[i][channel] = values[i];
            }
        }
        return albedos;
    }

    HUIRA_THROW_ERROR("Unsupported qld albedo type");
}

static std::vector<float> read_heights_(std::ifstream& file, std::uint32_t& width, std::uint32_t& height)
{
    std::array<float, 2> minmax;
    read_value_(file, minmax);
    std::uint16_t precision;
    read_value_(file, precision);

    if (precision == CLASS_FLOAT) {
        return read_image_<float>(file, width, height);
    }

    if (precision == CLASS_UINT8) {
        return fixed_to_float_(read_image_<std::uint8_t>(file, width, height), minmax, 255.0f);
    }
    if (precision == CLASS_UINT16) {
        return fixed_to_float_(read_image_<std::uint16_t>(file, width, height), minmax, 65535.0f);
    }
    HUIRA_THROW_ERROR("Unsupported qld height precision");
}

static huira::Vec3<float> ellipsoid_to_cartesian_(double lon, double lat, double alt, double a, double b, double c = -1.0)
{
    if (c < 0.0) {
        c = a;
    }
    double ex2 = (a * a - c * c) / (a * a);
    double ee2 = (a * a - b * b) / (a * a);
    constexpr double deg2rad = 3.14159265358979323846 / 180.0;
    double clat = std::cos(lat * deg2rad);
    double slat = std::sin(lat * deg2rad);
    double clon = std::cos(lon * deg2rad);
    double slon = std::sin(lon * deg2rad);
    double v = a / std::sqrt(1.0 - ex2 * (slat * slat) - ee2 * (clat * clat) * (slon * slon));
    return huira::Vec3<float>{
        static_cast<float>((v + alt) * clon * clat),
        static_cast<float>((v * (1.0 - ee2) + alt) * slon * clat),
        static_cast<float>((v * (1.0 - ex2) + alt) * slat)};
}

static huira::Vec3<float> project_to_tile_(const Projection& p, double x, double y, float z)
{
    double tile_size_x = (static_cast<double>(p.width) - 1.0) / 2.0;
    double tile_size_y = (static_cast<double>(p.height) - 1.0) / 2.0;
    double start_x = -tile_size_x;
    double start_y = -tile_size_y;
    double scale_x = (tile_size_x + 0.5) / static_cast<double>(p.width);
    double scale_y = (tile_size_y + 0.5) / static_cast<double>(p.height);

    double ix = x + (p.xoff / p.dx);
    double iy = y + (p.yoff / p.dy);
    double x_step = ix * scale_x;
    double y_step = -iy * scale_y;

    return huira::Vec3<float>{
        static_cast<float>(p.dx * p.pixel_scale[0] * (start_x + (2.0 * x_step))),
        static_cast<float>(p.dy * p.pixel_scale[1] * (-start_y + (2.0 * y_step))),
        z};
}

static TileMesh read_tile_(const fs::path& path, int requested_lod, double gsd)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        HUIRA_THROW_ERROR("Failed to open qld tile: " + path.string());
    }
    char magic[5];
    read_exact_(file, magic, 5);
    if (std::string(magic, 5) != "QUIPU") {
        HUIRA_THROW_ERROR("Not a qld/quipu file: " + path.string());
    }
    std::uint16_t class_id;
    read_value_(file, class_id);
    if (class_id != CLASS_DEM_PYRAMID) {
        HUIRA_THROW_ERROR("Unsupported qld class in: " + path.string());
    }

    TileMesh tile;
    tile.position = read_vec_<double, 3>(file);
    tile.rotation = read_mat_<double, 3, 3>(file);
    tile.scale = read_vec_<double, 3>(file);

    std::uint16_t lod_count = 0;
    bool has_albedos = false;
    read_value_(file, lod_count);
    read_value_(file, has_albedos);
    (void)read_vec_<float, 3>(file); // normal cone center
    float cone_angle = 0.0f;
    read_value_(file, cone_angle);
    (void)cone_angle;

    std::vector<double> gsds(lod_count);
    std::vector<std::uint64_t> offsets(lod_count);
    for (std::uint16_t i = 0; i < lod_count; ++i) {
        read_value_(file, gsds[i]);
        read_value_(file, offsets[i]);
    }

    int lod = std::clamp(requested_lod, 0, static_cast<int>(lod_count) - 1);
    if (gsd > 0.0) {
        lod = 0;
        for (std::uint16_t i = 1; i < lod_count && gsds[i] <= gsd; ++i) lod = i;
    }
    file.seekg(static_cast<std::streamoff>(offsets[static_cast<std::size_t>(lod)]), std::ios::beg);

    Projection proj = read_projection_(file);
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<float> heights = read_heights_(file, width, height);
    std::vector<huira::Visible8> albedos(heights.size(), huira::Visible8{1.0f});
    if (has_albedos) {
        albedos = read_albedos_(file, width, height);
    }
    if (width != proj.width || height != proj.height) {
        HUIRA_THROW_ERROR("qld projection/image resolution mismatch");
    }

    tile.vertices.resize(heights.size());
    std::unique_ptr<OGRCoordinateTransformation, void (*)(OGRCoordinateTransformation*)> ogr_transform(
        nullptr,
        [](OGRCoordinateTransformation* p) {
            if (p) OGRCoordinateTransformation::DestroyCT(p);
        });
    double semi_major = 0.0;
    double semi_minor = 0.0;
    OGRSpatialReference src_srs;
    OGRSpatialReference dst_srs;
    if (proj.is_ogr) {
        src_srs = OGRSpatialReference(proj.proj_ref.c_str());
        semi_major = src_srs.GetSemiMajor();
        semi_minor = src_srs.GetSemiMinor();
        if (!std::getenv("PROJ_DATA")) {
            if (const char* prefix = std::getenv("CONDA_PREFIX")) {
                setenv("PROJ_DATA", (std::string(prefix) + "/share/proj").c_str(), 0);
            }
        }
        setenv("PROJ_IGNORE_CELESTIAL_BODY", "YES", 1);
        CPLSetConfigOption("PROJ_IGNORE_CELESTIAL_BODY", "YES");
        std::ostringstream lonlat_proj;
        lonlat_proj << "+proj=longlat +a=" << semi_major << " +b=" << semi_minor << " +no_defs";
        if (dst_srs.importFromProj4(lonlat_proj.str().c_str()) != OGRERR_NONE) {
            HUIRA_THROW_ERROR("Failed to create lon/lat projection for qld tile");
        }
        ogr_transform.reset(OGRCreateCoordinateTransformation(&src_srs, &dst_srs));
        if (!ogr_transform) {
            HUIRA_THROW_ERROR("Failed to create OGR coordinate transform for qld tile");
        }
    }

    for (std::uint32_t j = 0; j < height; ++j) {
        for (std::uint32_t i = 0; i < width; ++i) {
            std::size_t idx = static_cast<std::size_t>(j) * width + i;
            if (proj.is_ogr) {
                double ix = (proj.dx * static_cast<double>(i)) + proj.xoff + 0.5;
                double iy = (proj.dy * static_cast<double>(j)) + proj.yoff + 0.5;
                double x_geo = proj.model_tie_point[0] + (ix * proj.pixel_scale[0]) + (iy * proj.row_col_rotation[0]);
                double y_geo = proj.model_tie_point[1] + (ix * proj.row_col_rotation[1]) + (iy * proj.pixel_scale[1]);
                if (!ogr_transform->Transform(1, &x_geo, &y_geo)) {
                    tile.vertices[idx].position = huira::Vec3<float>{std::numeric_limits<float>::infinity()};
                } else {
                    // Match DEM::makeVertexBuffer(offset): OGR-projected DEM
                    // vertices are stored relative to the quipu transform position,
                    // then the quipu transform is applied by the scene instance.
                    huira::Vec3<float> world_position =
                        ellipsoid_to_cartesian_(x_geo, y_geo, heights[idx], semi_major, semi_minor);
                    tile.vertices[idx].position = world_position - huira::Vec3<float>{tile.position};
                }
            } else {
                tile.vertices[idx].position = project_to_tile_(proj, i, j, heights[idx]);
            }
            tile.vertices[idx].albedo = albedos[idx];
            tile.vertices[idx].uv = huira::Vec2<float>{
                width > 1 ? static_cast<float>(i) / static_cast<float>(width - 1) : 0.0f,
                height > 1 ? static_cast<float>(j) / static_cast<float>(height - 1) : 0.0f};
        }
    }

    for (std::uint32_t i = 0; i + 1 < width; ++i) {
        for (std::uint32_t j = 0; j + 1 < height; ++j) {
            std::uint32_t f0 = i + (j + 1) * width;
            std::uint32_t f1 = i + 1 + j * width;
            std::uint32_t f2 = i + j * width;
            if (std::isfinite(heights[f0]) && std::isfinite(heights[f1]) && std::isfinite(heights[f2])) {
                tile.indices.insert(tile.indices.end(), {f0, f1, f2});
            }

            f0 = i + 1 + j * width;
            f1 = i + (j + 1) * width;
            f2 = i + 1 + (j + 1) * width;
            if (std::isfinite(heights[f0]) && std::isfinite(heights[f1]) && std::isfinite(heights[f2])) {
                tile.indices.insert(tile.indices.end(), {f0, f1, f2});
            }
        }
    }

    for (std::size_t k = 0; k + 2 < tile.indices.size(); k += 3) {
        auto& a = tile.vertices[tile.indices[k + 0]];
        auto& b = tile.vertices[tile.indices[k + 1]];
        auto& c = tile.vertices[tile.indices[k + 2]];
        huira::Vec3<float> n = glm::normalize(glm::cross(b.position - a.position, c.position - a.position));
        if (!std::isnan(n.x) && !std::isnan(n.y) && !std::isnan(n.z)) {
            a.normal += n;
            b.normal += n;
            c.normal += n;
        }
    }
    for (auto& v : tile.vertices) {
        float len = glm::length(v.normal);
        v.normal = len > 0.0f ? v.normal / len : huira::Vec3<float>{0.0f, 0.0f, 1.0f};
    }

    return tile;
}

} // namespace

template <typename FrameHandle>
void add_tiles(
    huira::Scene<huira::Visible8>& scene,
    FrameHandle& frame,
    const fs::path& dir,
    huira::MaterialHandle<huira::Visible8>& material,
    int lod,
    double unit_scale,
    const std::string& name,
    double gsd)
{
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".qld") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    if (files.empty()) {
        HUIRA_THROW_ERROR("No .qld tiles found in " + dir.string());
    }

    huira::IndexBuffer indices;
    huira::VertexBuffer<huira::Visible8> vertices;
    huira::TangentBuffer tangents;
    for (const auto& file : files) {
        TileMesh tile = read_tile_(file, lod, gsd);
        std::uint32_t vertex_offset = static_cast<std::uint32_t>(vertices.size());

        for (auto& v : tile.vertices) {
            if (std::isfinite(v.position.x) &&
                std::isfinite(v.position.y) &&
                std::isfinite(v.position.z)) {
                huira::Vec3<double> p =
                    tile.rotation * (huira::Vec3<double>{v.position} * tile.scale) + tile.position;
                v.position = huira::Vec3<float>{p / unit_scale};
                v.normal = glm::normalize(huira::Vec3<float>{
                    tile.rotation * huira::Vec3<double>{v.normal}});
            }
            vertices.push_back(v);
            tangents.push_back(tangent_from_normal_(v.normal));
        }
        for (auto idx : tile.indices) {
            indices.push_back(vertex_offset + idx);
        }
    }

    auto mesh = scene.add_mesh(indices, vertices, tangents, name + "_mesh");
    auto primitive = scene.add_primitive(mesh, material, name + "_primitive");
    auto instance = frame.new_instance(primitive);
    instance.set_scale(unit_scale, unit_scale, unit_scale);
}
} // namespace huira::qld
