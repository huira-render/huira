// 04 - Truth data
//
// Demonstrates:
//   - The frame buffer's other images: depth, camera and world normals, albedo, geometry IDs,
//     and the power each pixel receives (in total, and directly and indirectly)
//   - Viewing them: depth_map and normal_map, and normalizing an image
//   - Saving float data losslessly as FITS
//   - Ray queries: casting a camera ray through a pixel, intersecting it with the scene, and
//     resolving the hit into the surface's position and normal
//   - Projecting a point onto the image, and pixel coordinate conventions (OpenCV and FITS)
//
// Builds on: 03 Load a model
// Data: gateway.glb, NASA's model of the Gateway (see README.md)
//
// Run:
//   ./04_truth_data gateway.glb
//
// Writes:
//   output/04_truth_data.png             the rendered image
//   output/04_truth_data_depth.png       the distance to each pixel's surface, as a picture...
//   output/04_truth_data_depth.fits      ...and in meters
//   output/04_truth_data_normals.png     the surface normals, in the camera's frame
//   output/04_truth_data_albedo.png      the surface albedo
//   output/04_truth_data_ids.png         which geometry each pixel sees, one color each
//   output/04_truth_data_indirect.png    the light each pixel receives after bouncing

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>

#include "huira/huira.hpp"

namespace fs = std::filesystem;

using namespace huira::units::literals;

using TSpectral = huira::RGB;

// An image scaled so its brightest value is 1.
static huira::Image<huira::RGB> normalized(const huira::Image<huira::RGB>& image)
{
    float peak = 0.f;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            peak = std::max(peak, image(x, y).max());
        }
    }
    const float scale = peak > 0.f ? 1.f / peak : 1.f;

    huira::Image<huira::RGB> output(image.width(), image.height());
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            huira::RGB value = image(x, y);
            value *= scale;
            output(x, y) = value;
        }
    }
    return output;
}

// A color for each geometry ID, and black where a pixel saw nothing.
static huira::Image<huira::RGB> id_colors(const huira::Image<std::uint64_t>& ids)
{
    huira::Image<huira::RGB> output(ids.width(), ids.height());
    for (int y = 0; y < ids.height(); ++y) {
        for (int x = 0; x < ids.width(); ++x) {
            const std::uint64_t id = ids(x, y);
            if (id == std::numeric_limits<std::uint64_t>::max()) {
                output(x, y) = huira::RGB{0.f};
                continue;
            }
            // A repeatable pseudo-random color for each ID:
            const std::uint64_t h = (id + 1) * 0x9E3779B97F4A7C15ull;
            auto channel = [h](int shift) {
                return 0.2f + 0.8f * static_cast<float>((h >> shift) & 0xFF) / 255.f;
            };
            output(x, y) = huira::RGB{channel(16), channel(32), channel(48)};
        }
    }
    return output;
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: 04_truth_data <gateway.glb>\nREADME.md says where to get it.\n";
        return 1;
    }
    const fs::path model_path = argv[1];

    // The scene of example 03:
    huira::Scene<TSpectral> scene;
    auto gateway_model = scene.load_model(model_path);
    auto gateway = scene.root.new_instance(gateway_model);

    auto sun = scene.root.new_instance(scene.new_sun_light());
    sun.set_position(0_m, -1_au, 0_m);

    auto camera_model = scene.new_camera_model();
    camera_model.use_blender_convention();
    camera_model.set_focal_length(50_mm);
    camera_model.configure_sensor_from_size({1920, 1080}, 36_mm);
    camera_model.set_fstop(16);

    auto camera = scene.root.new_instance(camera_model);
    camera.set_position(40_m, -40_m, 0_m);
    camera.set_euler_angles(95_deg, 0_deg, 45_deg);

    huira::Time time("2026-01-01T00:00:00");
    huira::Interval exposure = huira::Interval::from_start(time, 1_ms);
    huira::SceneView<TSpectral> scene_view(
        scene, exposure, camera, huira::ObservationMode::GEOMETRIC_STATE);

    // Besides the sensor response, a frame buffer can hold images of what each pixel sees. Each
    // is filled only if enabled, since each costs memory and time:
    //   - depth: the distance along the ray to the nearest surface, in meters
    //   - camera_normals / world_normals: the surface normal, in the camera's or world's frame
    //   - albedo: the surface albedo
    //   - geometry_ids: which geometry the pixel sees
    //   - received_power: the power the pixel collects (W, in each spectral bin), split into
    //     received_direct_power (straight from the lights) and received_indirect_power (after
    //     bouncing off other surfaces)
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    frame_buffer.enable_depth();
    frame_buffer.enable_camera_normals();
    frame_buffer.enable_world_normals();
    frame_buffer.enable_albedo();
    frame_buffer.enable_geometry_ids();
    frame_buffer.enable_received_direct_power();
    frame_buffer.enable_received_indirect_power();

    huira::Renderer<TSpectral> renderer;
    renderer.set_samples_per_pixel(64);
    renderer.set_max_bounces(3);
    renderer.render(scene_view, frame_buffer);

    fs::create_directories("output");
    huira::write_image_png("output/04_truth_data.png",
                           huira::linear_to_srgb(frame_buffer.sensor_response()));

    // depth_map and normal_map turn depth and normals into pictures. FITS keeps the depth itself,
    // as 32-bit floats:
    huira::write_image_png("output/04_truth_data_depth.png",
                           huira::depth_map(frame_buffer.depth()));
    huira::write_image_fits("output/04_truth_data_depth.fits", frame_buffer.depth());
    huira::write_image_png("output/04_truth_data_normals.png",
                           huira::normal_map(frame_buffer.camera_normals()));

    // The other images are images like any other, to scale or color as needed:
    huira::write_image_png("output/04_truth_data_albedo.png",
                           huira::linear_to_srgb(frame_buffer.albedo()));
    huira::write_image_png("output/04_truth_data_ids.png", id_colors(frame_buffer.geometry_ids()));
    huira::write_image_png(
        "output/04_truth_data_indirect.png",
        huira::linear_to_srgb(normalized(frame_buffer.received_indirect_power())));

    // Ray queries ask the scene what is along a ray, without rendering. Take the pixel nearest the
    // image's center that sees something (its depth is finite; where a pixel sees nothing, the
    // depth is infinite):
    const huira::Image<float>& depth = frame_buffer.depth();
    int x = -1;
    int y = -1;
    double nearest = std::numeric_limits<double>::infinity();
    for (int row = 0; row < depth.height(); ++row) {
        for (int col = 0; col < depth.width(); ++col) {
            if (std::isfinite(depth(col, row))) {
                const double dx = col - depth.width() / 2.0;
                const double dy = row - depth.height() / 2.0;
                if (dx * dx + dy * dy < nearest) {
                    nearest = dx * dx + dy * dy;
                    x = col;
                    y = row;
                }
            }
        }
    }
    if (x < 0) {
        std::cout << "The camera sees nothing\n";
        return 0;
    }

    // cast_ray gives the ray through the pixel's center; SceneView::intersect traces it. Both work
    // in the camera's frame, so the hit point is relative to the camera.
    huira::Ray<TSpectral> ray = camera_model.cast_ray(x, y);
    huira::HitRecord hit = scene_view.intersect(ray);
    if (!hit.hit()) {
        std::cout << "The ray through the center of pixel (" << x << ", " << y << ") misses\n";
        return 0;
    }

    const huira::Vec3<float> point = ray.at(hit.t);
    std::cout << "Pixel (" << x << ", " << y << ") sees geometry " << hit.geom_id << " at " << hit.t
              << " m\n";
    std::cout << "  The depth image has " << depth(x, y)
              << " m there (its nearest hit over the pixel)\n";

    // resolve_hit fills in the surface at the hit: its position, normals and texture coordinates.
    const huira::Interaction<TSpectral> surface = scene_view.resolve_hit(ray, hit);
    std::cout << "  Surface normal: (" << surface.normal_s.x << ", " << surface.normal_s.y << ", "
              << surface.normal_s.z << ")\n";

    // project_point goes the other way, from a point in the camera's frame to the image. Pixel
    // coordinates follow the camera's pixel convention: OpenCV's by default (the first pixel's
    // center at (0, 0), y down), or others such as FITS's (the first pixel's center at (1, 1),
    // y up):
    huira::Pixel opencv_pixel = camera_model.project_point(point);
    camera_model.set_pixel_convention(huira::PixelConvention::fits());
    huira::Pixel fits_pixel = camera_model.project_point(point);
    camera_model.set_pixel_convention(huira::PixelConvention::opencv());

    std::cout << "  It projects to (" << opencv_pixel.x << ", " << opencv_pixel.y
              << ") (OpenCV convention)\n";
    std::cout << "  It projects to (" << fits_pixel.x << ", " << fits_pixel.y
              << ") (FITS convention)\n";
}
