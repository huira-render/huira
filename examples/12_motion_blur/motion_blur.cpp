// 12 - Motion blur
//
// Demonstrates:
//   - Spinning an instance (set_angular_velocity) and moving it (set_velocity), so that it moves
//     during the exposure
//   - Exposure time against motion: a short exposure freezes it, a long one smears it, and the
//     f-number keeps the image equally bright
//   - Temporal samples: the times at which the SceneView places each thing in view, chosen for
//     each, as many as its motion needs
//   - Tracking a moving target with look_at, which turns the camera through the exposure
//   - Turning a camera's motion blur off (enable_motion_blur), for a still frame
//
// Builds on: 03 Load a model
// Data: gateway.glb, NASA's model of the Gateway (see README.md)
//
// Run:
//   ./12_motion_blur gateway.glb
//
// Writes:
//   output/12_motion_blur_short.png      the Gateway spinning, frozen by a 0.25 ms exposure
//   output/12_motion_blur_long.png       the same spin over 16 ms: its outer parts smear into arcs
//   output/12_motion_blur_passing.png    the Gateway passing a camera that stares: it smears
//   output/12_motion_blur_tracking.png   ...and a camera that tracks it: it stays sharp
//   output/12_motion_blur_still.png      ...and with motion blur off: a still frame

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>

#include "huira/huira.hpp"

namespace fs = std::filesystem;

using namespace huira::units::literals;

using TSpectral = huira::RGB;

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: 12_motion_blur <gateway.glb>\nREADME.md says where to get it.\n";
        return 1;
    }
    const fs::path model_path = argv[1];
    fs::create_directories("output");

    huira::Scene<TSpectral> scene;

    // The Gateway at the origin, lit by the Sun, and the camera of example 03, 57 m away:
    auto gateway = scene.root.new_instance(scene.load_model(model_path, "gateway"));

    auto sun = scene.root.new_instance(scene.new_sun_light());
    sun.set_position(0_m, -1_au, 0_m);
    scene.set_background_radiance(1e-7f);

    auto camera_model = scene.new_camera_model();
    camera_model.use_blender_convention();
    camera_model.set_focal_length(50_mm);
    camera_model.configure_sensor_from_size({1920, 1080}, 36_mm);

    auto camera = scene.root.new_instance(camera_model);
    camera.set_position(40_m, -40_m, 0_m);
    camera.set_euler_angles(95_deg, 0_deg, 45_deg);

    // A second camera, with a 500 mm lens, 600 m from the Gateway in the same direction, which
    // shows it as large as the first. It watches the Gateway pass, below.
    auto telephoto_model = scene.new_camera_model();
    telephoto_model.use_blender_convention();
    telephoto_model.set_focal_length(500_mm);
    telephoto_model.configure_sensor_from_size({1920, 1080}, 36_mm);

    auto telephoto = scene.root.new_instance(telephoto_model);
    const huira::units::Meter distance(600.0 / std::sqrt(2.0));
    telephoto.set_position(distance, -distance, 0_m);

    // The path tracing settings of 03:
    huira::Renderer<TSpectral> renderer;
    renderer.set_samples_per_pixel(256);
    renderer.set_max_bounces(3);
    renderer.set_indirect_clamp(10.0f);
    renderer.set_dynamic_sampling(true);
    renderer.set_min_samples(16);

    const huira::Time time("2026-01-01T00:00:00");

    auto render = [&](const huira::CameraModelHandle<TSpectral>& model,
                      const huira::InstanceHandle<TSpectral>& viewer,
                      huira::units::Second duration,
                      float fstop,
                      const std::string& name) {
        model.set_fstop(fstop);
        model.precompute();

        // The exposure is centered on the time: what is placed by hand is where it was put at
        // the middle of the exposure, and its velocities move it from there.
        const huira::Interval exposure = huira::Interval::from_centered(time, duration);

        // The SceneView places each thing in view at times spread over the exposure, its
        // temporal samples, and each of a pixel's samples is traced at its own time between
        // them, so what moves blurs. It chooses them for each thing: one for what does not move
        // in view, and otherwise as many as keep it within a tenth of a pixel of where it truly
        // is (the motion tolerance, which can be given after the number of samples). A number
        // of samples can be given instead, the same for everything.
        huira::SceneView<TSpectral> scene_view(scene, exposure, viewer);
        const std::size_t samples = scene_view.max_temporal_samples();
        std::cout << name << ": up to " << samples << " temporal sample"
                  << (samples == 1 ? "" : "s") << "\n";

        auto frame_buffer = model.make_frame_buffer();
        frame_buffer.enable_sensor_response();
        renderer.render(scene_view, frame_buffer);
        huira::write_image_png(fs::path("output") / ("12_motion_blur_" + name + ".png"),
                               huira::linear_to_srgb(frame_buffer.sensor_response()));
    };

    // The Gateway spins once a second (60 rpm), about the line from it toward the camera.
    // set_angular_velocity takes the rate about each of the parent's axes, here the scene's: a
    // vector along the spin axis, as long as the rate. An instance turns about its own origin,
    // so the axis runs through the model's origin. Points on it stay put, and the farther a
    // point is from it, the faster it moves.
    const huira::units::DegreesPerSecond rate(360.0 / std::sqrt(2.0));
    gateway.set_angular_velocity(rate, -rate, 0_degps);

    // The light a pixel collects goes as the exposure time over the square of the f-number, so
    // each exposure below is paired with an f-number that keeps the image as bright as 03's
    // (1 ms at f/16).

    // A short exposure: 0.25 ms at f/8. The Gateway turns 0.09 degrees, and a point 10 m from the
    // axis moves 1.6 cm, under a pixel: the spin is frozen.
    render(camera_model, camera, 0.25_ms, 8.f, "short");

    // A long exposure: 16 ms, 64 times as long, at f/64, which lets in 1/64 as much light. The
    // Gateway turns 5.8 degrees, and a point 10 m from the axis moves 1 m, about 50 pixels: the
    // parts far from the axis smear into arcs around it, while those near it stay sharp. (At
    // f/64 diffraction spreads light over a couple of pixels, as example 10 shows, which softens
    // the whole image a little: far less than the motion blurs it.)
    render(camera_model, camera, 16_ms, 64.f, "long");

    // Now the Gateway stops spinning, and passes at 60 m/s instead, across the view of the
    // telephoto camera. Seen from nearby, the Gateway's near parts would cross the view faster
    // than its far parts; from far away, they all cross it at nearly the same rate, as one.
    gateway.set_angular_velocity(0_degps, 0_degps, 0_degps);
    const huira::units::MetersPerSecond speed(60.0 / std::sqrt(2.0));
    gateway.set_velocity(speed, speed, 0_mps);

    // A camera that stares at where the Gateway is at the middle of the exposure. In 16 ms, the
    // Gateway moves 0.96 m, about 43 pixels, and smears along its path. (look_at given a point
    // turns the camera to face it once; +z is up in the image.)
    const huira::Vec3<double> up{0.0, 0.0, 1.0};
    telephoto.look_at(huira::Vec3<double>{0.0, 0.0, 0.0}, up);
    render(telephoto_model, telephoto, 16_ms, 64.f, "passing");

    // A camera that tracks it. look_at given an instance follows it through the exposure: the
    // camera turns as the Gateway passes, keeping it at the center of the image, and it stays
    // sharp. Anything else in view, such as the stars behind it, would smear instead, as the
    // camera turned past them.
    telephoto.look_at(gateway, up);
    render(telephoto_model, telephoto, 16_ms, 64.f, "tracking");

    // Motion blur can be turned off for a camera: a view through it then renders a still frame,
    // at the middle of the exposure, however things move (as one temporal sample does, whatever
    // number is given). It is quicker, for previews, and shows where everything is. The camera
    // stares again, and the Gateway is seen sharp, as it is midway through its pass.
    telephoto.look_at(huira::Vec3<double>{0.0, 0.0, 0.0}, up);
    telephoto_model.enable_motion_blur(false);
    render(telephoto_model, telephoto, 16_ms, 64.f, "still");
}
