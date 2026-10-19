// 03 - Load a model
//
// Demonstrates:
//   - Loading a 3D model from a file (glTF/GLB here; any format Assimp reads), with its
//     meshes, materials and textures
//   - Instancing a model, and printing its internal graph
//   - The camera's intrinsic matrix
//   - A background radiance, for the sky behind the model
//   - Path tracing settings: samples per pixel, bounces, and clamping indirect light
//   - Dynamic sampling: stopping early on pixels that have converged
//
// Builds on: 02 Scene graph
// Data: gateway.glb, NASA's model of the Gateway (see README.md)
//
// Run:
//   ./03_load_a_model gateway.glb
//
// Writes:
//   output/03_load_a_model.png

#include <filesystem>
#include <iostream>

#include "huira/huira.hpp"

namespace fs = std::filesystem;

using namespace huira::units::literals;

using TSpectral = huira::RGB;

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: 03_load_a_model <gateway.glb>\nREADME.md says where to get it.\n";
        return 1;
    }
    const fs::path model_path = argv[1];

    huira::enable_console_info();

    huira::Scene<TSpectral> scene;

    // load_model reads a model file into the scene as one asset: its meshes, materials and
    // textures, and the graph of nodes that places them. Huira reads models with Assimp, so
    // besides glTF it reads OBJ, FBX, PLY, STL and many other formats.
    auto gateway_model = scene.load_model(model_path, "gateway");
    gateway_model.print_graph();

    // Like any asset, a model is placed with an instance. Here, at the origin:
    auto gateway = scene.root.new_instance(gateway_model);

    auto sun = scene.root.new_instance(scene.new_sun_light());
    sun.set_position(0_m, -1_au, 0_m);

    // Rays that hit nothing see the background radiance, given here as one value for every
    // spectral bin: a faint glow, rather than black. It can also be an image (an environment).
    scene.set_background_radiance(1e-7f);

    auto camera_model = scene.new_camera_model();
    camera_model.use_blender_convention();
    camera_model.set_focal_length(50_mm);
    camera_model.configure_sensor_from_size({1920, 1080}, 36_mm);
    camera_model.set_fstop(16);

    // The camera matrix, in pixels, as set_intrinsic_matrix() would take it (Mat3 is indexed
    // [column][row]):
    const huira::Mat3<float> K = camera_model.intrinsic_matrix();
    std::cout << "fx = " << K[0][0] << ", fy = " << K[1][1] << ", cx = " << K[2][0]
              << ", cy = " << K[2][1] << " (pixels)\n";

    auto camera = scene.root.new_instance(camera_model);
    camera.set_position(40_m, -40_m, 0_m);
    camera.set_euler_angles(95_deg, 0_deg, 45_deg);

    scene.print_contents();

    huira::Time time("2026-01-01T00:00:00");
    huira::Interval exposure = huira::Interval::from_start(time, 1_ms);
    huira::SceneView<TSpectral> scene_view(scene, exposure, camera);

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_sensor_response();

    // Path tracing follows each camera ray as it bounces from surface to surface:
    //   - samples per pixel: rays traced through each pixel (more is less noisy)
    //   - max bounces: how many surfaces a path may hit (1 is direct light only)
    //   - indirect clamp: a cap on the light any one bounce adds, which trades a little bias
    //     for much less "firefly" noise
    huira::Renderer<TSpectral> renderer;
    renderer.set_samples_per_pixel(256);
    renderer.set_max_bounces(3);
    renderer.set_indirect_clamp(10.0f);

    // Dynamic sampling stops tracing a pixel once its noise (the uncertainty in its brightness,
    // relative to it) and its neighbours' is below a threshold (0.01, about 1%, by default),
    // after a minimum number of samples: flat and empty parts of the image finish early, and
    // the samples go where the noise is.
    renderer.set_dynamic_sampling(true);
    renderer.set_min_samples(16);
    renderer.set_noise_threshold(0.01f);

    // The camera's optics are precomputed before the first render. Doing it here keeps that time
    // out of the render (and is when any problem with the settings is reported):
    camera_model.precompute();

    renderer.render(scene_view, frame_buffer);

    fs::create_directories("output");
    huira::write_image_png("output/03_load_a_model.png",
                           huira::linear_to_srgb(frame_buffer.sensor_response()));
}
