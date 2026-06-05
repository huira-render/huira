#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace qld {
namespace fs = std::filesystem;

constexpr std::uint16_t CLASS_FLOAT = 1;
constexpr std::uint16_t CLASS_DOUBLE = 2;
constexpr std::uint16_t CLASS_UINT8 = 3;
constexpr std::uint16_t CLASS_DEM_PYRAMID = 2002;
constexpr std::uint8_t CONSTANT_ALBEDO = 0;
constexpr std::uint8_t FLOAT_IMAGE_ALBEDO = 1;

struct DemTileWriteInfo {
    fs::path path;
    const std::vector<float>* heights = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::array<double, 6> geo_transform{0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    std::string projection;
    std::array<double, 3> origin{0.0, 0.0, 0.0};
    std::uint32_t tile_row = 0;
    std::uint32_t tile_col = 0;
    std::uint32_t tile_rows = 1;
    std::uint32_t tile_cols = 1;
    double xoff = 0.0;
    double yoff = 0.0;
    float default_albedo = 0.12f;
    const std::vector<float>* albedo = nullptr;
};

template <typename T>
void write_value(std::ofstream& out, const T& value)
{
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
    if (!out) throw std::runtime_error("Failed while writing qld file");
}

inline void write_string(std::ofstream& out, const std::string& s)
{
    auto n = static_cast<std::uint32_t>(s.size());
    write_value(out, n);
    out.write(s.data(), static_cast<std::streamsize>(s.size()));
    if (!out) throw std::runtime_error("Failed while writing qld string");
}

inline void write_vec3(std::ofstream& out, const std::array<double, 3>& v)
{
    write_value(out, CLASS_DOUBLE);
    std::uint8_t n = 3;
    write_value(out, n);
    for (double x : v) write_value(out, x);
}

inline void write_mat3_identity(std::ofstream& out)
{
    write_value(out, CLASS_DOUBLE);
    std::uint8_t n = 3;
    std::uint8_t m = 3;
    write_value(out, n);
    write_value(out, m);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            double v = (i == j) ? 1.0 : 0.0;
            write_value(out, v);
        }
    }
}

inline void write_float_image(std::ofstream& out, std::uint32_t width, std::uint32_t height, const std::vector<float>& image)
{
    write_value(out, CLASS_FLOAT);
    write_value(out, width);
    write_value(out, height);
    bool compressed = false;
    write_value(out, compressed);
    for (std::uint32_t i = 0; i < width; ++i) {
        for (std::uint32_t j = 0; j < height; ++j) {
            write_value(out, image[static_cast<std::size_t>(j) * width + i]);
        }
    }
}

inline void write_fixed_u8_image(std::ofstream& out, std::uint32_t width, std::uint32_t height, const std::vector<float>& image)
{
    float mn = std::numeric_limits<float>::infinity();
    float mx = -std::numeric_limits<float>::infinity();
    for (float v : image) {
        if (std::isfinite(v)) {
            mn = std::min(mn, v);
            mx = std::max(mx, v);
        }
    }
    if (!std::isfinite(mn)) mn = mx = 0.0f;
    if (mx <= mn) mx = mn + 1.0f;
    std::array<float, 2> minmax{mn, mx};
    write_value(out, minmax);
    write_value(out, CLASS_UINT8);
    write_value(out, width);
    write_value(out, height);
    bool compressed = false;
    write_value(out, compressed);
    float scale = 254.0f / (mx - mn);
    for (std::uint32_t i = 0; i < width; ++i) {
        for (std::uint32_t j = 0; j < height; ++j) {
            float v = image[static_cast<std::size_t>(j) * width + i];
            std::uint8_t u = 0;
            if (std::isfinite(v)) {
                float normalized = std::clamp((v - mn) * scale, 0.0f, 254.0f);
                u = static_cast<std::uint8_t>(normalized + 1.0f);
            }
            write_value(out, u);
        }
    }
}

inline std::vector<float> downsample_2x(const std::vector<float>& src,
                                        std::uint32_t width,
                                        std::uint32_t height,
                                        std::uint32_t& out_width,
                                        std::uint32_t& out_height)
{
    out_width = std::max<std::uint32_t>(1, width / 2);
    out_height = std::max<std::uint32_t>(1, height / 2);
    std::vector<float> dst(static_cast<std::size_t>(out_width) * out_height, 0.0f);
    for (std::uint32_t y = 0; y < out_height; ++y) {
        for (std::uint32_t x = 0; x < out_width; ++x) {
            double sum = 0.0;
            int count = 0;
            for (std::uint32_t dy = 0; dy < 2; ++dy) {
                for (std::uint32_t dx = 0; dx < 2; ++dx) {
                    std::uint32_t sx = std::min(width - 1, x * 2 + dx);
                    std::uint32_t sy = std::min(height - 1, y * 2 + dy);
                    float v = src[static_cast<std::size_t>(sy) * width + sx];
                    if (std::isfinite(v)) {
                        sum += v;
                        ++count;
                    }
                }
            }
            dst[static_cast<std::size_t>(y) * out_width + x] = count > 0 ? static_cast<float>(sum / count)
                                                                         : std::numeric_limits<float>::quiet_NaN();
        }
    }
    return dst;
}

inline void write_dem_tile(const DemTileWriteInfo& tile)
{
    if (!tile.heights) throw std::runtime_error("QLD DEM tile has no heights");

    std::vector<std::vector<float>> levels;
    std::vector<std::vector<float>> albedo_levels;
    std::vector<std::array<std::uint32_t, 2>> sizes;
    levels.push_back(*tile.heights);
    if (tile.albedo) albedo_levels.push_back(*tile.albedo);
    sizes.push_back({tile.width, tile.height});
    while (sizes.back()[0] > 8 && sizes.back()[1] > 8) {
        std::uint32_t w = 0;
        std::uint32_t h = 0;
        auto next = downsample_2x(levels.back(), sizes.back()[0], sizes.back()[1], w, h);
        levels.push_back(std::move(next));
        if (tile.albedo) {
            std::uint32_t aw = 0;
            std::uint32_t ah = 0;
            auto next_albedo = downsample_2x(albedo_levels.back(), sizes.back()[0], sizes.back()[1], aw, ah);
            albedo_levels.push_back(std::move(next_albedo));
        }
        sizes.push_back({w, h});
    }

    std::ofstream out(tile.path, std::ios::binary);
    if (!out) throw std::runtime_error("Failed to create " + tile.path.string());

    out.write("QUIPU", 5);
    write_value(out, CLASS_DEM_PYRAMID);
    write_vec3(out, tile.origin);
    write_mat3_identity(out);
    write_vec3(out, {1.0, 1.0, 1.0});

    auto lod_count = static_cast<std::uint16_t>(levels.size());
    bool has_albedos = true;
    write_value(out, lod_count);
    write_value(out, has_albedos);
    write_value(out, CLASS_FLOAT);
    std::uint8_t normal_n = 3;
    write_value(out, normal_n);
    float normal_center[3] = {0.0f, 0.0f, 1.0f};
    for (float v : normal_center) write_value(out, v);
    float cone_angle = 3.14159265f;
    write_value(out, cone_angle);

    std::vector<std::streampos> offset_positions(lod_count);
    for (std::uint16_t lod = 0; lod < lod_count; ++lod) {
        std::uint32_t width = sizes[lod][0];
        std::uint32_t height = sizes[lod][1];
        double dx = width > 1 ? static_cast<double>(tile.width - 1) / static_cast<double>(width - 1) : 1.0;
        double dy = height > 1 ? static_cast<double>(tile.height - 1) / static_cast<double>(height - 1) : 1.0;
        double gsd = tile.geo_transform[1] * dx;
        write_value(out, gsd);
        offset_positions[lod] = out.tellp();
        std::uint64_t zero = 0;
        write_value(out, zero);
    }

    std::vector<std::uint64_t> offsets(lod_count);
    for (std::uint16_t lod = 0; lod < lod_count; ++lod) {
        offsets[lod] = static_cast<std::uint64_t>(out.tellp());
        std::uint32_t width = sizes[lod][0];
        std::uint32_t height = sizes[lod][1];
        std::array<double, 2> pixel_scale{tile.geo_transform[1], tile.geo_transform[5]};
        double lod_xoff = tile.xoff;
        double lod_yoff = tile.yoff;
        double dx = width > 1 ? static_cast<double>(tile.width - 1) / static_cast<double>(width - 1) : 1.0;
        double dy = height > 1 ? static_cast<double>(tile.height - 1) / static_cast<double>(height - 1) : 1.0;
        bool is_ogr = !tile.projection.empty();
        std::array<double, 2> row_col_rotation{tile.geo_transform[2], tile.geo_transform[4]};
        std::array<double, 2> model_tie_point{tile.geo_transform[0], tile.geo_transform[3]};

        write_value(out, width);
        write_value(out, height);
        write_value(out, pixel_scale);
        write_value(out, tile.tile_cols);
        write_value(out, tile.tile_rows);
        write_value(out, tile.tile_row);
        write_value(out, tile.tile_col);
        write_value(out, lod_xoff);
        write_value(out, lod_yoff);
        write_value(out, dx);
        write_value(out, dy);
        write_value(out, is_ogr);
        if (is_ogr) {
            write_string(out, tile.projection);
            write_value(out, row_col_rotation);
            write_value(out, model_tie_point);
        }

        float mn = std::numeric_limits<float>::infinity();
        float mx = -std::numeric_limits<float>::infinity();
        for (float v : levels[lod]) {
            if (std::isfinite(v)) {
                mn = std::min(mn, v);
                mx = std::max(mx, v);
            }
        }
        if (!std::isfinite(mn)) mn = mx = 0.0f;
        std::array<float, 2> minmax{mn, mx};
        write_value(out, minmax);
        write_value(out, CLASS_FLOAT);
        write_float_image(out, width, height, levels[lod]);

        if (tile.albedo) {
            write_value(out, FLOAT_IMAGE_ALBEDO);
            write_fixed_u8_image(out, width, height, albedo_levels[lod]);
        } else {
            write_value(out, CONSTANT_ALBEDO);
            for (int i = 0; i < 8; ++i) write_value(out, tile.default_albedo);
        }
    }

    for (std::uint16_t lod = 0; lod < lod_count; ++lod) {
        out.seekp(offset_positions[lod]);
        write_value(out, offsets[lod]);
    }
}

} // namespace qld
