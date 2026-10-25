// 08 - Lights
//
// Demonstrates:
//   - Sphere lights, given by their power in each spectral bin (or their total power, in W)
//   - Light adds up: three colored lights, and the colors of the shadows where some are blocked
//   - Several lights in one scene. Huira samples each light directly at every bounce (next event
//     estimation), and weighs that against the BSDF's own samples (multiple importance
//     sampling), so several lights converge as quickly as one
//   - Indirect sources: marking a sunlit body as a reflector to sample, for planetshine
//   - Focusing the camera at a distance (example 10 covers focus)
//
// Builds on: 05 Planetary BRDFs
// Data: none
//
// Run:
//   ./08_lights
//
// Writes:
//   output/08_lights.png               three colored lights over three spheres
//   output/08_lights_planetshine.png   a satellite's night side, lit by the planet below it

#include <filesystem>
#include <string>
#include <utility>

#include "huira/huira.hpp"

namespace fs = std::filesystem;

using namespace huira::units::literals;

using TSpectral = huira::RGB;

static void render(huira::Scene<TSpectral>& scene,
                   const huira::InstanceHandle<TSpectral>& camera,
                   const huira::CameraModelHandle<TSpectral>& camera_model,
                   const huira::Interval& exposure,
                   const std::string& filename,
                   int samples)
{
    huira::SceneView<TSpectral> scene_view(scene, exposure, camera);
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_sensor_response();

    huira::Renderer<TSpectral> renderer;
    renderer.set_samples_per_pixel(samples);
    renderer.set_max_bounces(4);
    camera_model.precompute();
    renderer.render(scene_view, frame_buffer);

    fs::create_directories("output");
    huira::write_image_png(fs::path("output") / filename,
                           huira::linear_to_srgb(frame_buffer.sensor_response()));
}

static void colored_lights()
{
    huira::Scene<TSpectral> scene;
    auto lambertian = scene.new_bsdf_lambertian();

    // A floor: an ellipsoid flattened into a disk 20 m across and 10 cm thick.
    auto floor_material = scene.new_material(lambertian);
    floor_material.set_albedo_factor(TSpectral{0.5f});
    auto floor_shape = scene.add_ellipsoid(10_m, 10_m, 0.05_m);
    scene.root.new_instance(scene.add_primitive(floor_shape, floor_material));

    // Three white spheres resting on it:
    auto ball_material = scene.new_material(lambertian);
    ball_material.set_albedo_factor(TSpectral{0.6f});
    auto ball = scene.add_primitive(scene.add_ellipsoid(0.5_m, 0.5_m, 0.5_m), ball_material);
    for (double x : {-1.5, 0.0, 1.5}) {
        scene.root.new_instance(ball).set_position(huira::units::Meter(x), 0_m, 0.5_m);
    }

    // A sphere light is a glowing sphere: give it a radius and the power it emits. Here the
    // power is given in each spectral bin (red, green, blue), so each light emits in one bin
    // only: one red, one green and one blue light, 300 W each, in a row behind the spheres and
    // above the row of spheres. (A total power, 300_W, is spread evenly over the bins: a
    // white light.) They are small, 5 cm, so their shadows are sharp; a larger light casts
    // softer ones.
    const std::pair<double, TSpectral> colors[] = {
        {-2.5, TSpectral{300.f, 0.f, 0.f}},
        {0.0, TSpectral{0.f, 300.f, 0.f}},
        {2.5, TSpectral{0.f, 0.f, 300.f}},
    };
    for (const auto& [x, power] : colors) {
        auto light = scene.new_sphere_light(0.05_m, huira::units::SpectralWatts<TSpectral>(power));
        scene.root.new_instance(light).set_position(huira::units::Meter(x), 4_m, 1.75_m);
    }

    // Light adds up. Where all three lights reach the floor, it is white. Each sphere casts a
    // shadow from each light, toward the camera, and only the other two lights reach into it:
    // the red light's shadow is cyan (green and blue), the green light's magenta, and the blue
    // light's yellow. Where two shadows overlap, one light is left, and where all three overlap,
    // none: black.

    auto camera_model = scene.new_camera_model();
    camera_model.use_blender_convention();
    camera_model.set_focal_length(35_mm);
    camera_model.configure_sensor_from_size({1280, 800}, 36_mm);
    camera_model.set_fstop(4);

    // At f/4 the depth of field is shallow, and the camera focuses at infinity by default:
    // focus on the spheres instead.
    camera_model.set_focus_distance(8_m);

    auto camera = scene.root.new_instance(camera_model);
    camera.set_position(0_m, -7_m, 4_m);
    camera.look_at(huira::Vec3<double>{0.0, 0.0, 0.5}, huira::Vec3<double>{0.0, 0.0, 1.0});

    const huira::Time time("2026-01-01T00:00:00");
    render(
        scene, camera, camera_model, huira::Interval::from_start(time, 5_ms), "08_lights.png", 256);
}

constexpr double EQUATORIAL_RADIUS_KM = 6378.137;
constexpr double POLAR_RADIUS_KM = 6356.752;

static huira::EllipsoidHandle<TSpectral> earth_shell(huira::Scene<TSpectral>& scene,
                                                     double altitude_km)
{
    const huira::units::Kilometer a(EQUATORIAL_RADIUS_KM + altitude_km);
    const huira::units::Kilometer b(POLAR_RADIUS_KM + altitude_km);
    return scene.add_ellipsoid(a, a, b);
}

static void planetshine()
{
    huira::Scene<TSpectral> scene;
    auto lambertian = scene.new_bsdf_lambertian();

    fs::path daymap_path = "C:/Users/chris/huira_data/models/earth/8k_earth_daymap.jpg";
    fs::path cloud_map_path = "C:/Users/chris/huira_data/models/earth/8k_earth_clouds.jpg";

    // The surface: the daymap, on a Lambertian BSDF. (Example 06 builds the full material.)
    auto surface = scene.new_material(scene.new_bsdf_lambertian());
    auto daymap_srgb = huira::read_image(daymap_path);
    auto daymap = huira::srgb_to_linear(daymap_srgb); // JPEG textures are in sRGB color space
    surface.set_albedo_image(scene.add_texture(huira::rgb_to_spectral<TSpectral>(daymap.image)));
    auto earth = scene.root.new_instance(scene.add_primitive(earth_shell(scene, 0.0), surface));

    // The clouds are a second shell, 6 km up, with a white Lambertian material. Its alpha image
    // (the cloud map, read as one channel) says how opaque each point is: where it is 0, light
    // passes through to the surface below.
    auto cloud_material = scene.new_material(scene.new_bsdf_lambertian());
    auto cloud_map = huira::read_image_mono(cloud_map_path);
    cloud_material.set_alpha_image(scene.add_texture(std::move(cloud_map.image)));
    scene.root.new_instance(scene.add_primitive(earth_shell(scene, 6.0), cloud_material));

    // The atmosphere is a medium: a volume that absorbs and scatters light along the paths
    // through it. A medium is a density field (how much it absorbs and scatters, per meter, in
    // each spectral bin) with a phase function (which way scattered light goes). This one is
    // uniform, scattering blue more than red, and scatters equally in all directions.
    const TSpectral absorption{0.f};                       // per meter
    const TSpectral scattering{0.5e-6f, 1.0e-6f, 2.0e-6f}; // per meter (red, green, blue)
    auto density = scene.new_constant_density_field(absorption, scattering);
    auto phase_function = scene.new_isotropic_phase_function();
    auto air = scene.new_medium(density, phase_function);

    auto boundary = scene.new_material(scene.new_bsdf_null());
    boundary.set_transmission(TSpectral{1.f});
    scene.root.new_instance(scene.add_primitive(earth_shell(scene, 20.0), boundary, air));

    auto satellite_material = scene.new_material(lambertian);
    satellite_material.set_albedo_factor(TSpectral{0.8f});
    auto satellite_shape = scene.add_ellipsoid(1737_Km, 1737_Km, 1737_Km);
    auto moon = scene.root.new_instance(scene.add_primitive(satellite_shape, satellite_material));
    moon.set_position(-380000_Km, 50000_Km, 10000_Km);

    // The Sun lights the planet's +x half, and the satellite's +x side.
    auto sun = scene.root.new_instance(scene.new_sun_light());
    sun.set_position(1_au, 0_m, 0_m);

    // The satellite's other side is lit only by sunlight reflected off the planet. Marking the
    // planet as an indirect source makes the renderer sample it like a light (it emits nothing
    // itself), as it does the Sun. That matters most when the reflector is small in the sky,
    // such as the Moon seen from the Earth, where paths would rarely find it by chance.
    moon.set_indirect_source(true);

    // The camera looks up at the satellite from below its night side:
    auto camera_model = scene.new_camera_model();
    camera_model.use_blender_convention();
    camera_model.set_focal_length(50_mm);
    camera_model.configure_sensor_from_size({1024, 1024}, 36_mm);
    camera_model.set_fstop(16);

    auto camera = scene.root.new_instance(camera_model);
    camera.set_position(-50000_Km, 0_m, 0_m);
    camera.look_at(earth, huira::Vec3<double>{0.0, 0.0, 1.0});

    const huira::Time time("2026-01-01T00:00:00");
    render(scene,
           camera,
           camera_model,
           huira::Interval::from_start(time, 2_s),
           "08_lights_planetshine.png",
           256);
}

int main()
{
    // colored_lights();
    planetshine();
}
