#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <fnmatch.h>

#include "gdal_priv.h"
#include "cpl_conv.h"
#include "ogr_spatialref.h"
#include "tclap/CmdLine.h"

#include "huira_cli/cli.hpp"
#include "huira_cli/commands/qld_writer.hpp"
#include "huira_cli/compact_output.hpp"

namespace fs = std::filesystem;

namespace huira::cli {
namespace {

struct InputData {
    fs::path file;
    std::vector<fs::path> albedo_files;
    std::vector<bool> albedo_files_lonwrap;
    fs::path append_right;
    fs::path output = "qpu";
    double z_scale = 1.0;
    std::size_t max_vertices = 1'000'000;
    float default_albedo = 0.12f;
};

void validate_input(const InputData& input)
{
    if (input.file.empty()) throw std::runtime_error("No DEM input provided");
    if (input.max_vertices < 16) throw std::runtime_error("--max-vertices must be at least 16");
    if (input.default_albedo < 0.0f || input.default_albedo > 1.0f || std::isnan(input.default_albedo)) {
        throw std::runtime_error("--default-albedo must be between 0 and 1");
    }
}

fs::path resolve_path(const fs::path& root, const fs::path& path)
{
    if (path.is_absolute()) return path;
    return root / path;
}

std::vector<fs::path> expand_paths(const fs::path& root, const fs::path& pattern)
{
    fs::path resolved = resolve_path(root, pattern);
    std::string name = resolved.filename().string();
    if (name.find_first_of("*?[") == std::string::npos) return {resolved};
    fs::path parent = resolved.parent_path();
    std::vector<fs::path> out;
    for (const auto& entry : fs::directory_iterator(parent)) {
        if (entry.is_regular_file() && fnmatch(name.c_str(), entry.path().filename().string().c_str(), 0) == 0) {
            out.push_back(entry.path());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

void configure_proj()
{
    if (!std::getenv("PROJ_DATA")) {
        if (const char* prefix = std::getenv("CONDA_PREFIX")) {
            setenv("PROJ_DATA", (std::string(prefix) + "/share/proj").c_str(), 0);
        }
    }
    setenv("PROJ_IGNORE_CELESTIAL_BODY", "YES", 1);
    CPLSetConfigOption("PROJ_IGNORE_CELESTIAL_BODY", "YES");
}

std::string dataset_projection(GDALDataset& dataset)
{
    const char* ref = dataset.GetProjectionRef();
    return ref ? std::string(ref) : std::string{};
}

std::array<double, 3> ellipsoid_to_cartesian(double lon, double lat, double alt, double a, double b)
{
    constexpr double deg2rad = 3.14159265358979323846 / 180.0;
    double c = a;
    double ex2 = (a * a - c * c) / (a * a);
    double ee2 = (a * a - b * b) / (a * a);
    double clat = std::cos(lat * deg2rad);
    double slat = std::sin(lat * deg2rad);
    double clon = std::cos(lon * deg2rad);
    double slon = std::sin(lon * deg2rad);
    double v = a / std::sqrt(1.0 - ex2 * (slat * slat) - ee2 * (clat * clat) * (slon * slon));
    return {(v + alt) * clon * clat,
            (v * (1.0 - ee2) + alt) * slon * clat,
            (v * (1.0 - ex2) + alt) * slat};
}

std::array<double, 3> compute_ogr_origin(const std::vector<float>& heights,
                                         std::uint32_t width,
                                         std::uint32_t height,
                                         const std::array<double, 6>& gt,
                                         const std::string& projection,
                                         double xoff,
                                         double yoff)
{
    configure_proj();
    if (projection.empty()) return {0.0, 0.0, 0.0};

    float z = 0.0f;
    for (float h : heights) {
        if (std::isfinite(h)) {
            z = h;
            break;
        }
    }

    OGRSpatialReference src_srs(projection.c_str());
    double semi_major = src_srs.GetSemiMajor();
    double semi_minor = src_srs.GetSemiMinor();
    OGRSpatialReference dst_srs;
    std::ostringstream lonlat_proj;
    lonlat_proj << "+proj=longlat +a=" << semi_major << " +b=" << semi_minor << " +no_defs";
    if (dst_srs.importFromProj4(lonlat_proj.str().c_str()) != OGRERR_NONE) {
        throw std::runtime_error("Failed to create lon/lat projection for qld origin");
    }
    std::unique_ptr<OGRCoordinateTransformation, void (*)(OGRCoordinateTransformation*)> transform(
        OGRCreateCoordinateTransformation(&src_srs, &dst_srs),
        [](OGRCoordinateTransformation* p) {
            if (p) OGRCoordinateTransformation::DestroyCT(p);
        });
    if (!transform) throw std::runtime_error("Failed to create OGR transform for qld origin");

    double pixel_x = static_cast<double>(width) / 2.0;
    double pixel_y = static_cast<double>(height) / 2.0;
    double ix = pixel_x + xoff + 0.5;
    double iy = pixel_y + yoff + 0.5;
    double x_geo = gt[0] + (ix * gt[1]) + (iy * gt[2]);
    double y_geo = gt[3] + (ix * gt[4]) + (iy * gt[5]);
    if (!transform->Transform(1, &x_geo, &y_geo)) return {0.0, 0.0, 0.0};
    return ellipsoid_to_cartesian(x_geo, y_geo, z, semi_major, semi_minor);
}

struct Raster {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::array<double, 6> gt{0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    std::string projection;
    std::vector<float> data;
};

struct HeightMap {
    fs::path path;
    std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset{nullptr, GDALClose};
    GDALRasterBand* band = nullptr;
    std::unique_ptr<GDALDataset, decltype(&GDALClose)> append_right_dataset{nullptr, GDALClose};
    GDALRasterBand* append_right_band = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t effective_width = 0;
    std::array<double, 6> gt{0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    std::string projection;
    double band_scale = 1.0;
};

struct HeightTile {
    std::uint32_t row = 0;
    std::uint32_t col = 0;
    std::uint32_t rows = 1;
    std::uint32_t cols = 1;
    std::uint32_t xoff = 0;
    std::uint32_t yoff = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<float> heights;
    std::vector<float> albedos;
};

void apply_lonwrap(Raster& raster)
{
    configure_proj();
    OGRSpatialReference srs(raster.projection.c_str());
    std::ostringstream latlon_proj;
    latlon_proj << "+proj=longlat +a=" << srs.GetSemiMajor() << " +b=" << srs.GetSemiMinor()
                << " +lon_0=" << srs.GetProjParm("central_meridian");
    OGRSpatialReference lonlat_srs;
    if (lonlat_srs.importFromProj4(latlon_proj.str().c_str()) != OGRERR_NONE) {
        throw std::runtime_error("Failed to create lonwrap lon/lat SRS");
    }
    std::unique_ptr<OGRCoordinateTransformation, void (*)(OGRCoordinateTransformation*)> transform(
        OGRCreateCoordinateTransformation(&lonlat_srs, &srs),
        [](OGRCoordinateTransformation* p) {
            if (p) OGRCoordinateTransformation::DestroyCT(p);
        });
    if (!transform) throw std::runtime_error("Failed to create lonwrap transform");
    double lon0 = 0.0;
    double lat0 = 0.0;
    if (!transform->Transform(1, &lon0, &lat0)) throw std::runtime_error("Failed lonwrap origin transform");
    double lon180 = 180.0;
    double lat180 = 0.0;
    if (!transform->Transform(1, &lon180, &lat180)) throw std::runtime_error("Failed lonwrap target transform");
    if (srs.SetProjParm("central_meridian", 180.0) != OGRERR_NONE) {
        throw std::runtime_error("Failed to set lonwrap central_meridian");
    }
    if (srs.SetProjParm("false_easting", lon180 - lon0) != OGRERR_NONE) {
        throw std::runtime_error("Failed to set lonwrap false_easting");
    }
    char* wkt = nullptr;
    if (srs.exportToWkt(&wkt) != OGRERR_NONE || !wkt) throw std::runtime_error("Failed exporting lonwrap WKT");
    raster.projection = std::string(wkt);
    CPLFree(wkt);
}

Raster load_raster(const fs::path& path, bool allow_negative)
{
    std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset(
        static_cast<GDALDataset*>(GDALOpen(path.string().c_str(), GA_ReadOnly)), GDALClose);
    if (!dataset) throw std::runtime_error("Failed to open raster: " + path.string());
    GDALRasterBand* band = dataset->GetRasterBand(1);
    if (!band) throw std::runtime_error("Raster has no band: " + path.string());
    Raster r;
    r.width = static_cast<std::uint32_t>(dataset->GetRasterXSize());
    r.height = static_cast<std::uint32_t>(dataset->GetRasterYSize());
    if (dataset->GetGeoTransform(r.gt.data()) != CE_None) throw std::runtime_error("Raster has no geotransform: " + path.string());
    r.projection = dataset_projection(*dataset);
    if (r.projection.empty()) throw std::runtime_error("Raster has no projection: " + path.string());
    r.data.resize(static_cast<std::size_t>(r.width) * r.height);
    CPLErr err = band->RasterIO(GF_Read, 0, 0, static_cast<int>(r.width), static_cast<int>(r.height),
                                r.data.data(), static_cast<int>(r.width), static_cast<int>(r.height),
                                GDT_Float32, 0, 0);
    if (err != CE_None) throw std::runtime_error("Failed to read raster: " + path.string());
    double scale = band->GetScale();
    if (scale == 0.0) scale = 1.0;
    for (float& v : r.data) {
        v = static_cast<float>(static_cast<double>(v) * scale);
        if (!allow_negative && v < 0.0f) v = std::numeric_limits<float>::infinity();
    }
    return r;
}

float bilinear(const Raster& r, double x, double y)
{
    if (x < 0.0 || y < 0.0 || x >= static_cast<double>(r.width) || y >= static_cast<double>(r.height)) {
        return std::numeric_limits<float>::infinity();
    }
    auto x0 = static_cast<std::uint32_t>(std::floor(x));
    auto y0 = static_cast<std::uint32_t>(std::floor(y));
    auto x1 = std::min<std::uint32_t>(x0 + 1, r.width - 1);
    auto y1 = std::min<std::uint32_t>(y0 + 1, r.height - 1);
    double tx = x - static_cast<double>(x0);
    double ty = y - static_cast<double>(y0);
    float v00 = r.data[static_cast<std::size_t>(y0) * r.width + x0];
    float v10 = r.data[static_cast<std::size_t>(y0) * r.width + x1];
    float v01 = r.data[static_cast<std::size_t>(y1) * r.width + x0];
    float v11 = r.data[static_cast<std::size_t>(y1) * r.width + x1];
    if (!std::isfinite(v00) || !std::isfinite(v10) || !std::isfinite(v01) || !std::isfinite(v11)) {
        return std::numeric_limits<float>::infinity();
    }
    double a = static_cast<double>(v00) * (1.0 - tx) + static_cast<double>(v10) * tx;
    double b = static_cast<double>(v01) * (1.0 - tx) + static_cast<double>(v11) * tx;
    return static_cast<float>(a * (1.0 - ty) + b * ty);
}

void sample_albedo_from(const Raster& albedo,
                        const std::array<double, 6>& dem_gt,
                        const std::string& dem_projection,
                        double xoff,
                        double yoff,
                        std::uint32_t width,
                        std::uint32_t height,
                        std::vector<float>& out)
{
    configure_proj();
    OGRSpatialReference src_srs(dem_projection.c_str());
    OGRSpatialReference dst_srs(albedo.projection.c_str());
    std::unique_ptr<OGRCoordinateTransformation, void (*)(OGRCoordinateTransformation*)> transform(
        OGRCreateCoordinateTransformation(&src_srs, &dst_srs),
        [](OGRCoordinateTransformation* p) {
            if (p) OGRCoordinateTransformation::DestroyCT(p);
        });
    if (!transform) throw std::runtime_error("Failed creating DEM-to-albedo transform");

    const std::array<double, 2> p{albedo.gt[1], albedo.gt[5]};
    const std::array<double, 2> rot{albedo.gt[2], albedo.gt[4]};
    const std::array<double, 2> m{albedo.gt[0], albedo.gt[3]};
    double denom = rot[0] * rot[1] - p[0] * p[1];

    for (std::uint32_t j = 0; j < height; ++j) {
        for (std::uint32_t i = 0; i < width; ++i) {
            std::size_t idx = static_cast<std::size_t>(j) * width + i;
            double ix = static_cast<double>(i) + xoff + 0.5;
            double iy = static_cast<double>(j) + yoff + 0.5;
            double x_geo = dem_gt[0] + (ix * dem_gt[1]) + (iy * dem_gt[2]);
            double y_geo = dem_gt[3] + (ix * dem_gt[4]) + (iy * dem_gt[5]);
            if (!transform->Transform(1, &x_geo, &y_geo)) continue;
            double xm = (((y_geo - m[1]) * rot[0]) - (x_geo - m[0]) * p[1]);
            double px = (xm / denom) - 0.5;
            double ym = ((y_geo - m[1]) - ((px + 0.5) * rot[1]));
            double py = (ym / p[1]) - 0.5;
            float value = bilinear(albedo, px, py);
            if (std::isfinite(value)) out[idx] = value;
        }
    }
}

HeightMap load_height_map(const fs::path& root, const InputData& input)
{
    HeightMap dem;
    dem.path = resolve_path(root, input.file);
    dem.dataset.reset(static_cast<GDALDataset*>(GDALOpen(dem.path.string().c_str(), GA_ReadOnly)));
    if (!dem.dataset) throw std::runtime_error("Failed to open DEM: " + dem.path.string());

    dem.band = dem.dataset->GetRasterBand(1);
    if (!dem.band) throw std::runtime_error("DEM has no raster band: " + dem.path.string());

    if (dem.dataset->GetGeoTransform(dem.gt.data()) != CE_None) {
        std::cerr << "dem2qld: warning: DEM has no geotransform; using pixel coordinates\n";
    }
    dem.projection = dataset_projection(*dem.dataset);
    dem.width = static_cast<std::uint32_t>(dem.dataset->GetRasterXSize());
    dem.height = static_cast<std::uint32_t>(dem.dataset->GetRasterYSize());
    dem.effective_width = dem.width;
    dem.band_scale = dem.band->GetScale();
    if (dem.band_scale == 0.0) dem.band_scale = 1.0;
    return dem;
}

void append_right(HeightMap& dem, const fs::path& root, const InputData& input)
{
    if (input.append_right.empty()) return;

    fs::path append_path = resolve_path(root, input.append_right);
    dem.append_right_dataset.reset(static_cast<GDALDataset*>(GDALOpen(append_path.string().c_str(), GA_ReadOnly)));
    if (!dem.append_right_dataset) throw std::runtime_error("Failed to open append-right DEM: " + append_path.string());
    if (dem.append_right_dataset->GetRasterYSize() != static_cast<int>(dem.height)) {
        throw std::runtime_error("append-right DEM height does not match source DEM");
    }
    dem.append_right_band = dem.append_right_dataset->GetRasterBand(1);
    if (!dem.append_right_band) throw std::runtime_error("append-right DEM has no raster band");
    dem.effective_width += 1;
}

std::vector<HeightTile> tile_height_map(const HeightMap& dem, const InputData& input)
{
    double w = static_cast<double>(dem.effective_width);
    double h = static_cast<double>(dem.height);
    double aspect = w / h;
    double n = std::ceil(w / std::sqrt(static_cast<double>(input.max_vertices)));
    std::uint32_t tile_cols = static_cast<std::uint32_t>(n);
    std::uint32_t tile_rows = static_cast<std::uint32_t>(std::round(n / aspect));
    tile_rows = std::max<std::uint32_t>(1, tile_rows);
    tile_cols = std::max<std::uint32_t>(1, tile_cols);
    std::uint32_t tile_w = static_cast<std::uint32_t>(std::floor(w / static_cast<double>(tile_cols)));
    std::uint32_t tile_h = static_cast<std::uint32_t>(std::floor(h / static_cast<double>(tile_rows)));
    if (static_cast<std::uint64_t>(tile_w) * tile_h > input.max_vertices) {
        ++tile_rows;
        ++tile_cols;
        tile_w = static_cast<std::uint32_t>(std::floor(w / static_cast<double>(tile_cols)));
        tile_h = static_cast<std::uint32_t>(std::floor(h / static_cast<double>(tile_rows)));
    }

    std::vector<HeightTile> tiles;
    tiles.reserve(static_cast<std::size_t>(tile_cols) * tile_rows);
    for (std::uint32_t tc = 0; tc < tile_cols; ++tc) {
        for (std::uint32_t tr = 0; tr < tile_rows; ++tr) {
            HeightTile tile;
            tile.row = tr;
            tile.col = tc;
            tile.rows = tile_rows;
            tile.cols = tile_cols;
            tile.xoff = tc * (tile_w - 1);
            tile.yoff = tr * (tile_h - 1);
            tile.width = (tc == tile_cols - 1) ? (dem.effective_width - tile.xoff) : tile_w;
            tile.height = (tr == tile_rows - 1) ? (dem.height - tile.yoff) : tile_h;
            tiles.push_back(std::move(tile));
        }
    }
    return tiles;
}

void read_height_tile(HeightTile& tile, const HeightMap& dem, const InputData& input)
{
    tile.heights.assign(static_cast<std::size_t>(tile.width) * tile.height, 0.0f);

    std::uint32_t main_w = std::min(tile.width, dem.width > tile.xoff ? dem.width - tile.xoff : 0u);
    if (main_w > 0) {
        CPLErr err = dem.band->RasterIO(GF_Read,
                                        static_cast<int>(tile.xoff),
                                        static_cast<int>(tile.yoff),
                                        static_cast<int>(main_w),
                                        static_cast<int>(tile.height),
                                        tile.heights.data(),
                                        static_cast<int>(main_w),
                                        static_cast<int>(tile.height),
                                        GDT_Float32,
                                        sizeof(float),
                                        static_cast<GSpacing>(sizeof(float) * tile.width));
        if (err != CE_None) throw std::runtime_error("Failed reading DEM tile");
    }
    if (main_w < tile.width) {
        if (!dem.append_right_band) throw std::runtime_error("tile requires append-right data but none was provided");
        std::uint32_t append_w = tile.width - main_w;
        CPLErr err = dem.append_right_band->RasterIO(GF_Read,
                                                     0,
                                                     static_cast<int>(tile.yoff),
                                                     static_cast<int>(append_w),
                                                     static_cast<int>(tile.height),
                                                     tile.heights.data() + main_w,
                                                     static_cast<int>(append_w),
                                                     static_cast<int>(tile.height),
                                                     GDT_Float32,
                                                     sizeof(float),
                                                     static_cast<GSpacing>(sizeof(float) * tile.width));
        if (err != CE_None) throw std::runtime_error("Failed reading append-right DEM tile");
    }

    for (float& ht : tile.heights) {
        ht = static_cast<float>(static_cast<double>(ht) * dem.band_scale * input.z_scale);
    }
}

std::vector<Raster> load_albedo_maps(const fs::path& root, const InputData& input)
{
    std::vector<Raster> albedo_rasters;
    for (std::size_t i = 0; i < input.albedo_files.size(); ++i) {
        for (const auto& path : expand_paths(root, input.albedo_files[i])) {
            std::cerr << "dem2qld: loading albedo " << path << "\n";
            Raster albedo = load_raster(path, false);
            if (input.albedo_files_lonwrap[i]) apply_lonwrap(albedo);
            albedo_rasters.push_back(std::move(albedo));
        }
    }
    return albedo_rasters;
}

void sample_albedos(HeightTile& tile, const HeightMap& dem, const std::vector<Raster>& albedo_rasters, float default_albedo)
{
    if (albedo_rasters.empty()) return;

    tile.albedos.assign(static_cast<std::size_t>(tile.width) * tile.height, std::numeric_limits<float>::infinity());
    for (const auto& albedo : albedo_rasters) {
        sample_albedo_from(albedo,
                           dem.gt,
                           dem.projection,
                           static_cast<double>(tile.xoff),
                           static_cast<double>(tile.yoff),
                           tile.width,
                           tile.height,
                           tile.albedos);
    }
    for (float& a : tile.albedos) {
        if (!std::isfinite(a)) a = default_albedo;
    }
}

void write_quipu_tiles(const fs::path& root, const InputData& input)
{
    HeightMap dem = load_height_map(root, input);
    append_right(dem, root, input);
    std::vector<HeightTile> tiles = tile_height_map(dem, input);
    std::vector<Raster> albedo_rasters = load_albedo_maps(root, input);

    fs::path output_dir = resolve_path(root, input.output);
    fs::create_directories(output_dir);

    std::cerr << "dem2qld: " << dem.path << " -> " << output_dir << " ("
              << tiles.front().cols << "x" << tiles.front().rows << " tiles)\n";

    std::uint32_t tile_index = 1;
    for (HeightTile& tile : tiles) {
        read_height_tile(tile, dem, input);
        sample_albedos(tile, dem, albedo_rasters, input.default_albedo);

        auto origin = compute_ogr_origin(tile.heights,
                                         tile.width,
                                         tile.height,
                                         dem.gt,
                                         dem.projection,
                                         tile.xoff,
                                         tile.yoff);
        fs::path tile_path = output_dir / (dem.path.stem().string() + "_tile-" + std::to_string(tile_index++) + ".qld");

        qld::write_dem_tile({tile_path,
                                                &tile.heights,
                                                tile.width,
                                                tile.height,
                                                dem.gt,
                                                dem.projection,
                                                origin,
                                                tile.row,
                                                tile.col,
                                                tile.rows,
                                                tile.cols,
                                                static_cast<double>(tile.xoff),
                                                static_cast<double>(tile.yoff),
                                                input.default_albedo,
                                                tile.albedos.empty() ? nullptr : &tile.albedos});
    }
}

struct ParsedArgs {
    fs::path root;
    InputData input;
};

ParsedArgs parse_input(int argc, char** argv)
{
    TCLAP::CmdLine cmd("Convert DEM/albedo rasters to QLD tiles", ' ', HUIRA_VERSION);
    TCLAP::UnlabeledValueArg<std::string> dem_arg("dem", "Input DEM raster", true, "", "dem", cmd);
    TCLAP::UnlabeledValueArg<std::string> output_arg("output", "Output QLD directory", true, "", "dir", cmd);
    TCLAP::ValueArg<std::string> root_arg("r", "root-path", "Root path for relative input/output paths", false, "", "dir", cmd);
    TCLAP::ValueArg<int> max_vertices_arg("", "max-vertices", "Maximum vertices per QLD tile", false, 1000000, "int", cmd);
    TCLAP::ValueArg<float> default_albedo_arg("", "default-albedo", "Fallback albedo", false, 0.12f, "float", cmd);
    TCLAP::ValueArg<float> z_scale_arg("", "z-scale", "DEM z scale multiplier", false, 1.0f, "float", cmd);
    TCLAP::MultiArg<std::string> albedo_arg("", "albedo", "Albedo raster or glob", false, "path", cmd);
    TCLAP::MultiArg<std::string> albedo_lonwrap_arg("", "albedo-lonwrap", "Albedo raster/glob requiring 180-degree lonwrap", false, "path", cmd);
    TCLAP::ValueArg<std::string> append_right_arg("", "append-right", "DEM whose first column is appended to the right edge", false, "", "dem", cmd);
    TCLAP::SwitchArg moon_ldem16_arg("", "moon-ldem16", "Use the bundled LDEM16/WAC Moon defaults", cmd, false);
    CompactOutput compact;
    cmd.setOutput(&compact);

    cmd.parse(argc, argv);

    ParsedArgs parsed;
    parsed.root = root_arg.getValue().empty() ? fs::current_path() : fs::path(root_arg.getValue());

    InputData input;
    input.file = dem_arg.getValue();
    input.output = output_arg.getValue();
    input.max_vertices = static_cast<std::size_t>(max_vertices_arg.getValue());
    input.default_albedo = default_albedo_arg.getValue();
    input.z_scale = z_scale_arg.getValue();
    if (!append_right_arg.getValue().empty()) input.append_right = append_right_arg.getValue();
    for (const auto& path : albedo_arg.getValue()) {
        input.albedo_files.emplace_back(path);
        input.albedo_files_lonwrap.push_back(false);
    }
    for (const auto& path : albedo_lonwrap_arg.getValue()) {
        input.albedo_files.emplace_back(path);
        input.albedo_files_lonwrap.push_back(true);
    }
    if (moon_ldem16_arg.getValue()) {
        if (input.append_right.empty()) input.append_right = input.file;
        input.albedo_files.emplace_back("albedo/WAC_EMP_643NM_P900N0000_304P.IMG");
        input.albedo_files_lonwrap.push_back(false);
        input.albedo_files.emplace_back("albedo/WAC_EMP_643NM_P900S0000_304P.IMG");
        input.albedo_files_lonwrap.push_back(false);
        input.albedo_files.emplace_back("albedo/WAC_EMP_643NM_E300*_064P.IMG");
        input.albedo_files_lonwrap.push_back(true);
    }

    validate_input(input);
    parsed.input = std::move(input);
    return parsed;
}

static int run_dem2qld(const Context&, int argc, char** argv)
{
    try {
        GDALAllRegister();
        ParsedArgs parsed = parse_input(argc, argv);
        write_quipu_tiles(parsed.root, parsed.input);
        std::cerr << "dem2qld: wrote native Huira QLD tiles\n";
        return 0;
    } catch (const TCLAP::ArgException& e) {
        std::cerr << "dem2qld: " << e.error() << " for " << e.argId() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "dem2qld: " << e.what() << "\n";
        return 1;
    }
}

struct RegisterDem2Qld {
    RegisterDem2Qld()
    {
        Registry::instance().add({"dem2qld", "Convert DEM/albedo rasters to QLD tiles", run_dem2qld});
    }
};

static RegisterDem2Qld reg;

} // namespace
} // namespace huira::cli
