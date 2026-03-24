#pragma once

// cubemap_builder.hpp
//
// Builds a cubemap tile hierarchy from a set of DEM rasters. Each face of the
// cube is a gnomonic projection of a sphere. The hierarchy is a quadtree per
// face, where each node stores a tile_size x tile_size height tile.
//
// Output:
//   .hrcm  -- Serialized tree (metadata, OBBs, normal cones, file refs).
//              Loaded at render time.
//   .hrct  -- Binary tile data files. Raw Image<float> pixel data, appended
//              sequentially. Multiple files created past the size limit.
//
// Usage:
//   CubeMapBuildSettings settings;
//   settings.max_depth = 12;
//   settings.sphere_radius = 1737400.0; // Moon
//   auto result = build_cubemap(dem_paths, settings);

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "gdal_priv.h"
#include "gdalwarper.h"
#include "gdal_utils.h"
#include "ogr_spatialref.h"

#include "huira/images/image.hpp"
#include "huira/images/io/png_io.hpp"

#include "huira/core/constants.hpp"

#include "huira/util/ensure_proj_data.hpp"

namespace fs = std::filesystem;

namespace huira {

    static constexpr int NUM_FACES = 6;
    static constexpr float NO_RASTER_DATA = -9999.0f;

    // -----------------------------------------------------------------------
    // Settings
    // -----------------------------------------------------------------------

    struct CubeMapBuildSettings {
        std::size_t   max_depth       = 12;
        std::size_t   tile_size       = 512;
        double        sphere_radius   = 0.;
        std::uint64_t max_file_bytes  = 2ULL * 1024 * 1024 * 1024;
        fs::path      output_dir      = ".";
        std::string   output_name     = "cubemap";
    };

    // -----------------------------------------------------------------------
    // On-disk types (serialized into .hrcm)
    // -----------------------------------------------------------------------

    struct TileAddress {
        std::uint8_t  face;
        std::uint16_t level;
        std::uint16_t x, y;
    };

    template <IsFloatingPoint TFloat>
    struct OBBData {
        std::array<TFloat, 3> center;
        std::array<TFloat, 3> half_extents;
        std::array<TFloat, 9> rotation; // column-major 3x3
    };

    struct NormalCone {
        std::array<double, 3> axis;
        double half_angle;
    };

    struct CubeMapFileHeader {
        std::array<char, 8> magic = {'H','U','I','R','A','C','M','\0'};
        std::uint32_t       version  = 1;
        double              sphere_radius;
        std::uint32_t       tile_size;
        std::uint32_t       max_level;
        std::uint64_t       num_data_files;
    };

    // -----------------------------------------------------------------------
    // In-memory build node
    // -----------------------------------------------------------------------

    struct BuildNode {
        BuildNode() = default;
        BuildNode(const BuildNode&) = delete;
        BuildNode& operator=(const BuildNode&) = delete;

        TileAddress   address    = {};
        std::uint64_t file_id    = 0;
        std::uint64_t byte_offset = 0;
        bool          has_data   = false;
        OBBData<double>       obb        = {};
        NormalCone    normal_cone = {};
        std::array<std::unique_ptr<BuildNode>, 4> children;

        std::uint8_t child_mask() const {
            std::uint8_t m = 0;
            for (std::size_t i = 0; i < 4; ++i)
                if (children[i]) {
                    m |= (1u << i);
                }
            return m;
        }
    };

    // -----------------------------------------------------------------------
    // Build result
    // -----------------------------------------------------------------------

    struct CubeMapBuildResult {
        bool                  success = false;
        std::string           error;
        fs::path              tree_file;
        std::vector<fs::path> data_files;
        std::uint64_t         total_tiles   = 0;
        std::uint32_t         deepest_level = 0;
    };

    // -----------------------------------------------------------------------
    // Cube face projection definitions
    //
    // Each face is a gnomonic projection centered on one of the six cardinal
    // directions. The projection plane extends [-R, R] on both axes (since
    // tan(45 deg) = 1).
    // -----------------------------------------------------------------------

    struct FaceCenter { double lat_0; double lon_0; };

    static constexpr std::array<FaceCenter, 6> FACE_CENTERS = {{
        {  0.0,    0.0 },   // +X
        {  0.0,  180.0 },   // -X
        {  0.0,   90.0 },   // +Y
        {  0.0,  -90.0 },   // -Y
        { 90.0,    0.0 },   // +Z  (north pole)
        {-90.0,    0.0 },   // -Z  (south pole)
    }};

    inline std::string face_proj_string(std::size_t face, double radius) {
        auto& c = FACE_CENTERS[face];
        std::ostringstream s;
        s << "+proj=gnom"
          << " +lat_0=" << c.lat_0
          << " +lon_0=" << c.lon_0
          << " +R="     << radius
          << " +units=m +no_defs";
        return s.str();
    }

    inline double face_half_extent(double radius) {
        return radius; // R * tan(pi/4) = R
    }

    // -----------------------------------------------------------------------
    // Gnomonic inverse projection
    //
    // Maps a point on the gnomonic plane back to geographic coordinates.
    // face_lat0/face_lon0 are the face center in radians.
    // -----------------------------------------------------------------------

    inline void gnomonic_inverse(double gx, double gy, double radius,
                                 double face_lat0, double face_lon0,
                                 double& out_lat_deg, double& out_lon_deg)
    {
        double rho = std::sqrt(gx*gx + gy*gy);
        double c = std::atan2(rho, radius);
        double lat_r, lon_r;
        if (rho < 1e-12) {
            lat_r = face_lat0;
            lon_r = face_lon0;
        } else {
            lat_r = std::asin(std::cos(c)*std::sin(face_lat0)
                            + gy*std::sin(c)*std::cos(face_lat0)/rho);
            lon_r = face_lon0 + std::atan2(
                gx * std::sin(c),
                rho*std::cos(face_lat0)*std::cos(c) - gy*std::sin(face_lat0)*std::sin(c));
        }
        out_lat_deg = lat_r * 180.0 / PI<double>();
        out_lon_deg = lon_r * 180.0 / PI<double>();
    }

    inline void to_cartesian(double lat_deg, double lon_deg, double r,
                             double& ox, double& oy, double& oz)
    {
        double lat = lat_deg * PI<double>() / 180.0;
        double lon = lon_deg * PI<double>() / 180.0;
        ox = r * std::cos(lat) * std::cos(lon);
        oy = r * std::cos(lat) * std::sin(lon);
        oz = r * std::sin(lat);
    }

    // -----------------------------------------------------------------------
    // Source DEM metadata
    //
    // Collected once per input file before tiling begins. Stores the native
    // resolution (on the sphere's surface) and geographic bounding box.
    // -----------------------------------------------------------------------

    struct SourceDEMInfo {
        fs::path path;
        double   native_gsd_meters;
        double   lat_min, lat_max;
        double   lon_min, lon_max;
    };

    inline SourceDEMInfo scan_dem(const fs::path& path, double radius) {
        GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(path.string().c_str(), GA_ReadOnly));
        if (!ds)
            throw std::runtime_error("Failed to open: " + path.string());

        SourceDEMInfo info;
        info.path = path;

        double gt[6];
        ds->GetGeoTransform(gt);
        int nx = ds->GetRasterXSize();
        int ny = ds->GetRasterYSize();

        OGRSpatialReference src_srs, geo_srs;
        src_srs.importFromWkt(ds->GetProjectionRef());
        geo_srs.importFromProj4(
            ("+proj=longlat +R=" + std::to_string(radius) + " +no_defs").c_str());

        auto* ct = OGRCreateCoordinateTransformation(&src_srs, &geo_srs);
        if (ct) {
            double xs[4] = { gt[0], gt[0]+nx*gt[1], gt[0],           gt[0]+nx*gt[1] };
            double ys[4] = { gt[3], gt[3],           gt[3]+ny*gt[5], gt[3]+ny*gt[5] };
            ct->Transform(4, xs, ys);

            info.lon_min = *std::min_element(xs, xs+4);
            info.lon_max = *std::max_element(xs, xs+4);
            info.lat_min = *std::min_element(ys, ys+4);
            info.lat_max = *std::max_element(ys, ys+4);

            double mid_lat = (info.lat_min + info.lat_max) * 0.5 * PI<double>() / 180.0;
            double mx = (info.lon_max - info.lon_min) * PI<double>() / 180.0 * radius * std::cos(mid_lat);
            double my = (info.lat_max - info.lat_min) * PI<double>() / 180.0 * radius;
            info.native_gsd_meters = std::max(mx / nx, my / ny);

            OGRCoordinateTransformation::DestroyCT(ct);
        } else {
            info.lon_min = gt[0];
            info.lon_max = gt[0] + nx*gt[1];
            info.lat_max = gt[3];
            info.lat_min = gt[3] + ny*gt[5];

            double mid_lat = (info.lat_min + info.lat_max) * 0.5 * PI<double>() / 180.0;
            double mx = (info.lon_max - info.lon_min) * PI<double>() / 180.0 * radius * std::cos(mid_lat);
            double my = (info.lat_max - info.lat_min) * PI<double>() / 180.0 * radius;
            info.native_gsd_meters = std::max(mx / nx, my / ny);
        }

        GDALClose(ds);
        return info;
    }

    // -----------------------------------------------------------------------
    // Resolution queries
    // -----------------------------------------------------------------------

    // Ground sample distance of a tile at the given quadtree level.
    inline double tile_gsd(int level, std::size_t tile_size, double radius) {
        double face_edge = 2.0 * face_half_extent(radius);
        return (face_edge / static_cast<double>(1 << level)) / static_cast<double>(tile_size);
    }

    // Geographic bounding box of a tile (from its gnomonic corner projections).
    struct GeoBBox { double lat_min, lat_max, lon_min, lon_max; };

    inline GeoBBox tile_geo_bbox(const TileAddress& addr, std::size_t tile_size, double radius) {
        (void)tile_size; // TODO Unused?

        double half = face_half_extent(radius);
        double te   = (2.0 * half) / static_cast<double>(1 << addr.level);

        double gx_min = -half + addr.x * te;
        double gy_max =  half - addr.y * te;
        double gx_max = gx_min + te;
        double gy_min = gy_max - te;

        auto& fc = FACE_CENTERS[addr.face];
        double lat0 = fc.lat_0 * PI<double>() / 180.0;
        double lon0 = fc.lon_0 * PI<double>() / 180.0;

        double sx[5] = { gx_min, gx_max, gx_min, gx_max, (gx_min+gx_max)*0.5 };
        double sy[5] = { gy_min, gy_min, gy_max, gy_max, (gy_min+gy_max)*0.5 };

        GeoBBox bb;
        bb.lat_min =  1e18; bb.lat_max = -1e18;
        bb.lon_min =  1e18; bb.lon_max = -1e18;
        for (int i = 0; i < 5; ++i) {
            double lat, lon;
            gnomonic_inverse(sx[i], sy[i], radius, lat0, lon0, lat, lon);
            bb.lat_min = std::min(bb.lat_min, lat);
            bb.lat_max = std::max(bb.lat_max, lat);
            bb.lon_min = std::min(bb.lon_min, lon);
            bb.lon_max = std::max(bb.lon_max, lon);
        }
        return bb;
    }

    inline bool bboxes_overlap(const GeoBBox& a, double lat_min, double lat_max,
                               double lon_min, double lon_max)
    {
        return !(a.lon_max < lon_min || lon_max < a.lon_min ||
                 a.lat_max < lat_min || lat_max < a.lat_min);
    }

    // Returns true if any source DEM covers part of this tile's region at a
    // resolution finer than the tile currently provides.
    inline bool finer_data_available(const TileAddress& addr, std::size_t tile_size,
                                     double radius,
                                     const std::vector<SourceDEMInfo>& sources)
    {
        double current_gsd = tile_gsd(addr.level, tile_size, radius);
        GeoBBox bb = tile_geo_bbox(addr, tile_size, radius);

        for (auto& src : sources) {
            if (src.native_gsd_meters >= current_gsd)
                continue;
            if (bboxes_overlap(bb, src.lat_min, src.lat_max, src.lon_min, src.lon_max))
                return true;
        }
        return false;
    }

    // -----------------------------------------------------------------------
    // Tile data writer
    //
    // Appends raw tile data to .hrct files, rolling to a new file when the
    // current one would exceed the byte limit.
    // -----------------------------------------------------------------------

    class TileDataWriter {
    public:
        TileDataWriter(const TileDataWriter&) = delete;
        TileDataWriter& operator=(const TileDataWriter&) = delete;
        TileDataWriter(const fs::path& dir, const std::string& base,
                       std::uint64_t max_bytes, std::size_t tile_size)
            : dir_(dir), base_(base), max_bytes_(max_bytes)
            , tile_bytes_(static_cast<std::uint64_t>(tile_size * tile_size * sizeof(float)))
        {
            roll_();
        }

        ~TileDataWriter() {
            if (stream_.is_open()) {
                stream_.close();
            }
        }

        struct Ref {
            std::uint64_t file_id;
            std::uint64_t byte_offset;
        };

        Ref write(const Image<float>& tile) {
            if (offset_ + tile_bytes_ > max_bytes_) {
                roll_();
            }

            Ref ref;
            ref.file_id     = paths_.size() - 1;
            ref.byte_offset = offset_;

            stream_.write(reinterpret_cast<const char*>(tile.data()),
                          static_cast<std::streamsize>(tile_bytes_));
            offset_ += tile_bytes_;
            return ref;
        }

        const std::vector<fs::path>& paths() const { return paths_; }

    private:
        fs::path dir_;
        std::string base_;
        std::uint64_t max_bytes_, tile_bytes_, offset_ = 0;
        std::ofstream stream_;
        std::vector<fs::path> paths_;

        void roll_() {
            if (stream_.is_open()) stream_.close();
            std::ostringstream name;
            name << base_ << "_" << paths_.size() << ".hrct";
            fs::path p = dir_ / name.str();
            stream_.open(p, std::ios::binary | std::ios::trunc);
            if (!stream_) {
                HUIRA_THROW_ERROR("Cannot open: " + p.string());
            }
            paths_.push_back(p);
            offset_ = 0;
        }
    };

    // -----------------------------------------------------------------------
    // Face VRT construction
    //
    // For a given cube face, warp all input DEMs into that face's gnomonic
    // projection and return the result as a single GDAL dataset (backed by a
    // VRT, so no pixel data is materialized until read).
    // -----------------------------------------------------------------------
    inline GDALDataset* build_face_vrt(const std::vector<GDALDatasetH>& srcs,
        std::size_t face, double radius)
    {
        std::string dst_proj = face_proj_string(face, radius);
        double half = face_half_extent(radius);

        std::vector<std::string> warp_args = {
            "-t_srs",     dst_proj,
            "-te",        std::to_string(-half), std::to_string(-half),
                          std::to_string(half),  std::to_string(half),
            "-ts",        "1024", "1024",
            "-ot",        "Float32",
            "-r",         "bilinear",
            "-of",        "VRT",
            "-wo",        "SAMPLE_GRID=YES",
            "-wo",        "SOURCE_EXTRA=100",
            "-srcnodata", std::to_string(NO_RASTER_DATA),
            "-dstnodata", std::to_string(NO_RASTER_DATA),
        };
        std::vector<const char*> warp_argv;
        for (auto& a : warp_args) {
            warp_argv.push_back(a.c_str());
        }
        warp_argv.push_back(nullptr);

        GDALWarpAppOptions* warp_opts = GDALWarpAppOptionsNew(
            const_cast<char**>(warp_argv.data()), nullptr);

        int err = 0;
        GDALDatasetH out = GDALWarp("", nullptr,
            static_cast<int>(srcs.size()),
            const_cast<GDALDatasetH*>(srcs.data()),
            warp_opts, &err);
        GDALWarpAppOptionsFree(warp_opts);

        return static_cast<GDALDataset*>(out);
    }

    // -----------------------------------------------------------------------
    // Reading a tile from the VRT
    //
    // Maps the tile's gnomonic extent to pixel coordinates in the VRT and
    // uses RasterIO to resample into a tile_size x tile_size Image<float>.
    // -----------------------------------------------------------------------

    inline Image<float> read_tile_from_vrt(GDALDataset* vrt,
                                           const TileAddress& addr,
                                           std::size_t tile_size, double radius)
    {
        double half = face_half_extent(radius);
        double te   = (2.0 * half) / static_cast<double>(1 << addr.level);

        double gx_min = -half + addr.x * te;
        double gy_max =  half - addr.y * te;
        double gx_max = gx_min + te;
        double gy_min = gy_max - te;

        double gt[6];
        vrt->GetGeoTransform(gt);

        // Projection coords -> VRT pixel coords
        double px0 = (gx_min - gt[0]) / gt[1];
        double py0 = (gy_max - gt[3]) / gt[5]; // gt[5] is negative
        double px1 = (gx_max - gt[0]) / gt[1];
        double py1 = (gy_min - gt[3]) / gt[5];

        if (px0 > px1) {
            std::swap(px0, px1);
        }
        if (py0 > py1) {
            std::swap(py0, py1);
        }

        int sx = static_cast<int>(std::floor(px0));
        int sy = static_cast<int>(std::floor(py0));
        int sw = static_cast<int>(std::ceil(px1)) - sx;
        int sh = static_cast<int>(std::ceil(py1)) - sy;

        int dw = vrt->GetRasterXSize();
        int dh = vrt->GetRasterYSize();
        sx = std::clamp(sx, 0, dw - 1);
        sy = std::clamp(sy, 0, dh - 1);
        sw = std::min(sw, dw - sx);
        sh = std::min(sh, dh - sy);

        Image<float> tile(static_cast<int>(tile_size), static_cast<int>(tile_size), NO_RASTER_DATA);
        if (sw <= 0 || sh <= 0 || vrt->GetRasterCount() == 0) {
            return tile;
        }

        auto count = vrt->GetRasterCount();
        auto rb = vrt->GetRasterBand(1);
        auto err = rb->RasterIO(
            GF_Read, sx, sy, sw, sh,
            tile.data(), static_cast<int>(tile_size), static_cast<int>(tile_size),
            GDT_Float32, 0, 0);
        (void)err;
        (void)count;

        for (std::size_t i = 0; i < tile.size(); ++i) {
            if (tile[i] != NO_RASTER_DATA) {
                tile[i] -= static_cast<float>(radius);
            }
        }

        return tile;
    }

    inline bool tile_has_valid_data(const Image<float>& tile) {
        for (std::size_t i = 0; i < tile.size(); ++i) {
            if (tile[i] != NO_RASTER_DATA) {
                return true;
            }
        }
        return false;
    }

    // -----------------------------------------------------------------------
    // OBB computation
    //
    // Computes an axis-aligned bounding box (stored as an OBB with identity
    // rotation) from the tile's four gnomonic corners projected onto the
    // sphere at the min and max heights found in the tile.
    // -----------------------------------------------------------------------

    inline OBBData<double> compute_leaf_obb(const TileAddress& addr, std::size_t tile_size,
                                    double radius, const Image<float>& heights)
    {
        (void)tile_size;

        float h_min = std::numeric_limits<float>::max();
        float h_max = std::numeric_limits<float>::lowest();
        for (std::size_t i = 0; i < heights.size(); ++i) {
            float v = heights[i];

            if (v == NO_RASTER_DATA) {
                continue;
            }

            h_min = std::min(h_min, v);
            h_max = std::max(h_max, v);
        }

        if (h_min > h_max) {
            h_min = 0.0f; h_max = 0.0f;
        }

        double half = face_half_extent(radius);
        double te   = (2.0 * half) / static_cast<double>(1 << addr.level);
        double gx_min = -half + addr.x * te;
        double gy_max =  half - addr.y * te;
        double gx_max = gx_min + te;
        double gy_min = gy_max - te;

        auto& fc = FACE_CENTERS[addr.face];
        double lat0 = fc.lat_0 * PI<double>() / 180.0;
        double lon0 = fc.lon_0 * PI<double>() / 180.0;

        double gx[4] = { gx_min, gx_max, gx_min, gx_max };
        double gy[4] = { gy_min, gy_min, gy_max, gy_max };

        double xs[8], ys[8], zs[8];
        for (int i = 0; i < 4; ++i) {
            double lat_d, lon_d;
            gnomonic_inverse(gx[i], gy[i], radius, lat0, lon0, lat_d, lon_d);
            to_cartesian(lat_d, lon_d, radius + h_min, xs[i*2],   ys[i*2],   zs[i*2]);
            to_cartesian(lat_d, lon_d, radius + h_max, xs[i*2+1], ys[i*2+1], zs[i*2+1]);
        }

        OBBData<double> obb;
        double lo[3], hi[3];
        lo[0] = *std::min_element(xs, xs+8); hi[0] = *std::max_element(xs, xs+8);
        lo[1] = *std::min_element(ys, ys+8); hi[1] = *std::max_element(ys, ys+8);
        lo[2] = *std::min_element(zs, zs+8); hi[2] = *std::max_element(zs, zs+8);
        for (std::size_t i = 0; i < 3; ++i) {
            obb.center[i]       = (lo[i] + hi[i]) * 0.5;
            obb.half_extents[i] = (hi[i] - lo[i]) * 0.5;
        }
        std::memset(obb.rotation.data(), 0, sizeof(obb.rotation));
        obb.rotation[0] = obb.rotation[4] = obb.rotation[8] = 1.0;

        return obb;
    }

    // -----------------------------------------------------------------------
    // Normal cone computation
    //
    // Uses the tile center's radial direction as the cone axis. The half-angle
    // is a conservative estimate based on the tile's angular extent on the
    // sphere. This can be tightened later by analyzing actual height gradients.
    // -----------------------------------------------------------------------

    inline NormalCone compute_leaf_normal_cone(const TileAddress& addr, std::size_t tile_size,
                                              double radius, const Image<float>& heights)
    {
        (void)tile_size;
        (void)heights;

        double half = face_half_extent(radius);
        double te   = (2.0 * half) / static_cast<double>(1 << addr.level);
        double cx   = -half + (addr.x + 0.5) * te;
        double cy   =  half - (addr.y + 0.5) * te;

        auto& fc = FACE_CENTERS[addr.face];
        double lat0 = fc.lat_0 * PI<double>() / 180.0;
        double lon0 = fc.lon_0 * PI<double>() / 180.0;

        double lat_d, lon_d;
        gnomonic_inverse(cx, cy, radius, lat0, lon0, lat_d, lon_d);

        double lat = lat_d * PI<double>() / 180.0;
        double lon = lon_d * PI<double>() / 180.0;

        NormalCone cone;
        cone.axis[0] = std::cos(lat) * std::cos(lon);
        cone.axis[1] = std::cos(lat) * std::sin(lon);
        cone.axis[2] = std::sin(lat);

        // Half-diagonal angular radius of the tile, with 1.5x safety margin.
        double half_diag = te * 0.5 * std::sqrt(2.0);
        cone.half_angle = std::atan2(half_diag, radius) * 1.5;

        return cone;
    }

    // -----------------------------------------------------------------------
    // Bound merging (for bottom-up accumulation through the tree)
    // -----------------------------------------------------------------------

    inline OBBData<double> merge_obbs(const OBBData<double>& a, const OBBData<double>& b) {
        OBBData<double> out;
        for (std::size_t i = 0; i < 3; ++i) {
            double lo = std::min(a.center[i] - a.half_extents[i],
                                 b.center[i] - b.half_extents[i]);
            double hi = std::max(a.center[i] + a.half_extents[i],
                                 b.center[i] + b.half_extents[i]);
            out.center[i]       = (lo + hi) * 0.5;
            out.half_extents[i] = (hi - lo) * 0.5;
        }
        std::memset(out.rotation.data(), 0, sizeof(out.rotation));
        out.rotation[0] = out.rotation[4] = out.rotation[8] = 1.0;
        return out;
    }

    inline NormalCone merge_normal_cones(const NormalCone& a, const NormalCone& b) {
        double dot = a.axis[0]*b.axis[0] + a.axis[1]*b.axis[1] + a.axis[2]*b.axis[2];
        dot = std::clamp(dot, -1.0, 1.0);
        double between = std::acos(dot);

        NormalCone out;
        if (between < 1e-12) {
            std::memcpy(out.axis.data(), a.axis.data(), sizeof(out.axis));
            out.half_angle = std::max(a.half_angle, b.half_angle);
        } else {
            out.half_angle = (between + a.half_angle + b.half_angle) * 0.5;
            double t = 0.5 + (b.half_angle - a.half_angle) / (2.0 * between);
            t = std::clamp(t, 0.0, 1.0);
            for (std::size_t i = 0; i < 3; ++i) {
                out.axis[i] = a.axis[i] * (1.0 - t) + b.axis[i] * t;
            }
            double len = std::sqrt(out.axis[0]*out.axis[0]
                                 + out.axis[1]*out.axis[1]
                                 + out.axis[2]*out.axis[2]);
            if (len > 1e-12) {
                for (std::size_t i = 0; i < 3; ++i) {
                    out.axis[i] /= len;
                }
            }
        }
        out.half_angle = std::min(out.half_angle, PI<double>());
        return out;
    }

    inline void accumulate_bounds(BuildNode& node) {
        bool init = false;
        for (auto& child : node.children) {
            if (!child) {
                continue;
            }

            accumulate_bounds(*child);

            if (!init) {
                node.obb         = child->obb;
                node.normal_cone = child->normal_cone;
                init = true;
            } else {
                node.obb         = merge_obbs(node.obb, child->obb);
                node.normal_cone = merge_normal_cones(node.normal_cone, child->normal_cone);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Serialization
    // -----------------------------------------------------------------------

    inline void serialize_node(std::ofstream& out, const BuildNode& node) {
        out.write(reinterpret_cast<const char*>(&node.address), sizeof(TileAddress));
        out.write(reinterpret_cast<const char*>(&node.file_id), sizeof(std::uint64_t));
        out.write(reinterpret_cast<const char*>(&node.byte_offset), sizeof(std::uint64_t));
        std::uint8_t has = node.has_data ? 1 : 0;
        out.write(reinterpret_cast<const char*>(&has), 1);
        out.write(reinterpret_cast<const char*>(&node.obb), sizeof(OBBData<double>));
        out.write(reinterpret_cast<const char*>(&node.normal_cone), sizeof(NormalCone));
        std::uint8_t mask = node.child_mask();
        out.write(reinterpret_cast<const char*>(&mask), 1);
        for (std::size_t i = 0; i < 4; ++i) {
            if (node.children[i]) {
                serialize_node(out, *node.children[i]);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Core recursive builder
    //
    // For a single face tree: sample the VRT at this node's level, write the
    // tile if it contains data, then recurse into four children if the source
    // DEMs have finer resolution to offer in this region.
    // -----------------------------------------------------------------------

    inline void build_face_recursive(BuildNode& node,
                                     GDALDataset* vrt,
                                     TileDataWriter& writer,
                                     const CubeMapBuildSettings& settings,
                                     const std::vector<SourceDEMInfo>& sources,
                                     std::uint32_t& deepest,
                                     std::uint64_t& total)
    {
        Image<float> tile = read_tile_from_vrt(
            vrt, node.address, settings.tile_size, settings.sphere_radius);

        if (!tile_has_valid_data(tile)) {
            return;
        }

        // DEBUG: Write out the tile as a PNG for inspection
        //Image<float> output_tile = tile;
        //float max = 0.f;
        //float min = std::numeric_limits<float>::max();
        //for (std::size_t i = 0; i < tile.size(); ++i) {
        //    if (tile[i] == NO_RASTER_DATA) {
        //        continue;
        //    }
        //    if (tile[i] > max) {
        //        max = tile[i];
        //    }
        //    if (tile[i] < min) {
        //        min = tile[i];
        //    }
        //}
        //for (std::size_t i = 0; i < tile.size(); ++i) {
        //    if (tile[i] == NO_RASTER_DATA) {
        //        output_tile[i] = 0.f;
        //    } else {
        //        output_tile[i] = (tile[i] - min) / (max - min);
        //    }
        //}
        //std::string filename = ("tile_" + std::to_string(node.address.face) + "_" + std::to_string(node.address.level) + "_" + std::to_string(node.address.x) + "_" + std::to_string(node.address.y) + ".png");
        //huira::write_image_png(settings.output_dir / filename, output_tile);

        auto ref         = writer.write(tile);
        node.file_id     = ref.file_id;
        node.byte_offset = ref.byte_offset;
        node.has_data    = true;
        node.obb         = compute_leaf_obb(node.address, settings.tile_size,
                                            settings.sphere_radius, tile);
        node.normal_cone = compute_leaf_normal_cone(node.address, settings.tile_size,
                                                    settings.sphere_radius, tile);

        deepest = std::max(deepest, static_cast<std::uint32_t>(node.address.level));
        ++total;

        if (node.address.level >= settings.max_depth) {
            return;
        }

        if (!finer_data_available(node.address, settings.tile_size,
            settings.sphere_radius, sources)) {
            return;
        }

        for (std::size_t i = 0; i < 4; ++i) {
            auto child = std::make_unique<BuildNode>();
            child->address.face  = node.address.face;
            child->address.level = static_cast<std::uint16_t>(node.address.level + 1);
            child->address.x     = static_cast<std::uint16_t>(node.address.x * 2 + (i & 1));
            child->address.y     = static_cast<std::uint16_t>(node.address.y * 2 + ((i >> 1) & 1));
            node.children[i] = std::move(child);
        }

        for (auto& child : node.children) {
            build_face_recursive(*child, vrt, writer, settings, sources, deepest, total);
        }
    }

    // -----------------------------------------------------------------------
    // Entry point
    // -----------------------------------------------------------------------

    inline CubeMapBuildResult build_cubemap(const std::vector<fs::path>& dem_paths,
                                            CubeMapBuildSettings settings)
    {
        ensure_proj_data({});
        CubeMapBuildResult result;
        try {
            GDALAllRegister();

            CPLSetErrorHandler([](CPLErr eClass, CPLErrorNum err_no, const char* msg) noexcept {
                switch (eClass) {
                case CE_Debug:
                    HUIRA_LOG_DEBUG("GDAL CE_Debug: " + std::to_string(err_no) + "|" + msg);
                    break;
                case CE_Warning:
                    HUIRA_LOG_INFO("GDAL CE_Warning: " + std::to_string(err_no) + "|" + msg);
                    break;
                case CE_Failure:
                    HUIRA_LOG_INFO("GDAL CE_Failure: " + std::to_string(err_no) + "|" + msg);
                    break;
                case CE_Fatal:
                    HUIRA_LOG_ERROR("GDAL CE_Fatal: " + std::to_string(err_no) + "|" + msg);
                    std::abort();
                    break;
                case CE_None:
                    break;
                default:
                    break;
                }
                });

            fs::create_directories(settings.output_dir);

            // Expand directories into individual DEM file paths
            std::vector<fs::path> all_dems;
            for (auto& p : dem_paths) {
                if (fs::is_directory(p)) {
                    for (auto& entry : fs::recursive_directory_iterator(p)) {
                        if (!entry.is_regular_file()) {
                            continue;
                        }

                        auto ext = entry.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(),
                            [](unsigned char c) {
                                return static_cast<char>(std::tolower(c));
                            });

                        if (ext == ".tif" || ext == ".tiff" || ext == ".img"
                            || ext == ".hgt" || ext == ".dt2" || ext == ".grd") {

                            all_dems.push_back(entry.path());
                        }
                    }
                } else {
                    all_dems.push_back(p);
                }
            }
            if (all_dems.empty()) {
                result.error = "No DEM files found";
                return result;
            }

            // Auto-detect sphere radius if not provided
            if (settings.sphere_radius == 0.0) {
                GDALDataset* ds = static_cast<GDALDataset*>(
                    GDALOpen(all_dems[0].string().c_str(), GA_ReadOnly));
                if (ds) {
                    OGRSpatialReference srs;
                    srs.importFromWkt(ds->GetProjectionRef());
                    settings.sphere_radius = srs.GetSemiMajor();
                    GDALClose(ds);
                }
            }

            // Scan each DEM for geographic extent and native resolution
            std::vector<SourceDEMInfo> sources;
            for (auto& p : all_dems) {
                try {
                    sources.push_back(scan_dem(p, settings.sphere_radius));
                } catch (const std::exception& e) {
                    HUIRA_LOG_WARNING("Skipping " + p.string() + ": " + e.what());
                }
            }

            TileDataWriter writer(settings.output_dir, settings.output_name,
                                  settings.max_file_bytes, settings.tile_size);

            std::array<BuildNode, NUM_FACES> roots;
            std::uint32_t deepest = 0;
            std::uint64_t total   = 0;

            // Open and unscale all source datasets once
            std::vector<GDALDatasetH> raw_datasets;
            std::vector<GDALDatasetH> prepared_sources;

            std::vector<std::string> translate_args = {
                "-ot", "Float32",
                "-unscale",
                "-of", "VRT",
            };
            std::vector<const char*> translate_argv;
            for (auto& a : translate_args) {
                translate_argv.push_back(a.c_str());
            }
            translate_argv.push_back(nullptr);

            GDALTranslateOptions* translate_opts = GDALTranslateOptionsNew(
                const_cast<char**>(translate_argv.data()), nullptr);

            for (auto& p : all_dems) {
                GDALDatasetH raw = GDALOpen(p.string().c_str(), GA_ReadOnly);
                if (!raw) {
                    continue;
                }
                raw_datasets.push_back(raw);

                GDALDataset* raw_ds = static_cast<GDALDataset*>(raw);
                bool needs_unscale = false;
                for (int b = 1; b <= raw_ds->GetRasterCount(); ++b) {
                    int has_scale = 0;
                    int has_offset = 0;
                    double scale = raw_ds->GetRasterBand(b)->GetScale(&has_scale);
                    double offset = raw_ds->GetRasterBand(b)->GetOffset(&has_offset);
                    if ((has_scale && scale != 1.0) || (has_offset && offset != 0.0)) {
                        needs_unscale = true;
                        break;
                    }
                }

                if (needs_unscale) {
                    int terr = 0;
                    GDALDatasetH unscaled = GDALTranslate("", raw, translate_opts, &terr);
                    if (unscaled) {
                        prepared_sources.push_back(unscaled);
                    }
                    else {
                        prepared_sources.push_back(raw);
                    }
                }
                else {
                    prepared_sources.push_back(raw);
                }
            }
            GDALTranslateOptionsFree(translate_opts);

            for (std::size_t face = 0; face < NUM_FACES; ++face) {
                roots[face].address = {
                    static_cast<std::uint8_t>(face), 0, 0, 0
                };

                GDALDataset* vrt = build_face_vrt(
                    prepared_sources, face, settings.sphere_radius);
                if (!vrt) {
                    continue;
                }

                build_face_recursive(roots[face], vrt, writer,
                    settings, sources, deepest, total);
                accumulate_bounds(roots[face]);
                GDALClose(vrt);
            }

            // After the face loop and tree serialization:
            for (auto h : prepared_sources) {
                GDALClose(h);
            }
            // Close raw datasets that were wrapped by translate VRTs
            // (the unwrapped ones were already closed as part of prepared_sources)
            for (std::size_t i = 0; i < raw_datasets.size(); ++i) {
                if (raw_datasets[i] != prepared_sources[i]) {
                    GDALClose(raw_datasets[i]);
                }
            }

            // Write tree file
            fs::path tree_path = settings.output_dir / (settings.output_name + ".hrcm");
            std::ofstream tree_out(tree_path, std::ios::binary);
            if (!tree_out) {
                HUIRA_THROW_ERROR("Cannot create: " + tree_path.string());
            }

            CubeMapFileHeader header;
            header.sphere_radius  = settings.sphere_radius;
            header.tile_size      = static_cast<std::uint32_t>(settings.tile_size);
            header.max_level      = static_cast<std::uint32_t>(deepest);
            header.num_data_files = static_cast<std::uint32_t>(writer.paths().size());
            tree_out.write(reinterpret_cast<const char*>(&header), sizeof(header));

            for (auto& fp : writer.paths()) {
                std::string s = fp.filename().string();
                auto len = static_cast<std::uint32_t>(s.size());
                tree_out.write(reinterpret_cast<const char*>(&len), sizeof(len));
                tree_out.write(s.data(), len);
            }

            for (auto& root : roots) {
                serialize_node(tree_out, root);
            }

            tree_out.close();

            result.success       = true;
            result.tree_file     = tree_path;
            result.data_files    = writer.paths();
            result.total_tiles   = total;
            result.deepest_level = deepest;

        } catch (const std::exception& e) {
            result.error = e.what();
        }
        return result;
    }

}
