#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// A square of the given half side, in the plane z = depth, facing along z.
PrimitiveHandle<RGB>
add_square(Scene<RGB>& scene, float half_side, const MaterialHandle<RGB>& material)
{
    const auto vertex = [](float x, float y) {
        Vertex<RGB> v;
        v.position = Vec3<float>{x, y, 0.f};
        v.normal = Vec3<float>{0.f, 0.f, -1.f};
        return v;
    };
    const VertexBuffer<RGB> vertices{vertex(-half_side, -half_side),
                                     vertex(half_side, -half_side),
                                     vertex(half_side, half_side),
                                     vertex(-half_side, half_side)};
    return scene.add_primitive(scene.add_mesh(IndexBuffer{0, 2, 1, 0, 3, 2}, vertices), material);
}

/// A wall 10 m in front of a camera that sees 2 m of it, 32 pixels square, lit only by a
/// glowing panel 6 m square behind the camera, which the camera does not see: about 1 path in
/// 11 from the wall finds the panel, so 16 samples all find nothing for about 1 pixel in 5.
/// Rendered with the given samples, dynamic sampling on or off, and the panel's emission scaled.
Image<RGB> render_wall(int samples,
                       bool dynamic,
                       float emission = 1.f,
                       std::optional<float> noise_threshold = std::nullopt)
{
    Scene<RGB> scene;
    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_size({32, 32}, 36_mm);
    camera_model.set_focal_length(180_mm);
    camera_model.enable_diffraction(false);
    camera_model.enable_depth_of_field(false);
    auto camera = scene.root.new_instance(camera_model);

    auto wall_material = scene.new_material(scene.new_bsdf_lambertian());
    wall_material.set_albedo(RGB{0.8f});
    scene.root.new_instance(add_square(scene, 10.f, wall_material)).set_position(0_m, 0_m, 10_m);

    auto glow = scene.new_material(scene.new_bsdf_lambertian());
    glow.set_albedo(RGB{0.f});
    glow.set_emissive(RGB{emission});
    scene.root.new_instance(add_square(scene, 3.f, glow))
        .set_position(0_m, 0_m, units::Meter(-0.5));

    SceneView<RGB> view(scene,
                        Interval::from_centered(Time::from_et(0.0), 1_ms),
                        camera,
                        ObservationMode::TRUE_STATE);
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    renderer.set_samples_per_pixel(samples);
    renderer.set_max_bounces(2);
    renderer.set_dynamic_sampling(dynamic);
    renderer.set_min_samples(16);
    if (noise_threshold) {
        renderer.set_noise_threshold(*noise_threshold);
    }
    renderer.render(view, frame_buffer);
    return frame_buffer.received_power();
}

std::size_t dark_pixels(const Image<RGB>& image)
{
    std::size_t dark = 0;
    for (std::size_t i = 0; i < image.size(); ++i) {
        dark += image[i].total() > 0.f ? std::size_t{0} : std::size_t{1};
    }
    return dark;
}

double mean_total(const Image<RGB>& image)
{
    double sum = 0.0;
    for (std::size_t i = 0; i < image.size(); ++i) {
        sum += static_cast<double>(image[i].total());
    }
    return sum / static_cast<double>(image.size());
}

} // namespace

TEST_CASE("Dynamic sampling leaves no pixel dark where only some paths find light",
          "[render][dynamic_sampling]")
{
    // With 16 samples each, about 1 pixel in 5 finds no light: judged alone, as dynamic
    // sampling used to judge them, those pixels have no spread, look exact, and stop dark.
    const Image<RGB> sixteen = render_wall(16, false);
    INFO(dark_pixels(sixteen) << " of " << sixteen.size() << " pixels dark with 16 samples each");
    CHECK(dark_pixels(sixteen) > sixteen.size() / 10);

    // Judged with their neighbours, which found light, they sample on:
    const Image<RGB> dynamic = render_wall(256, true);
    CHECK(dark_pixels(dynamic) == 0);

    // And the wall is as bright, on average, as with every pixel's 256 samples:
    const double expected = mean_total(render_wall(256, false));
    CHECK(std::abs(mean_total(dynamic) / expected - 1.0) < 0.03);
}

TEST_CASE("Dynamic sampling stops a pixel at the same noise however bright the scene",
          "[render][dynamic_sampling]")
{
    // A threshold the wall's pixels reach after 64 to 256 samples, depending on their luck:
    // they stop at different rounds. Rendered again with the panel a thousand times dimmer, the
    // same pixels stop at the same rounds, with the same samples. (Judged by its variance over
    // its mean, as dynamic sampling used to judge it, a dim pixel stopped sooner than a bright
    // one: the dim wall stopped after 16 samples, the bright one never.)
    const float threshold = 0.3f;
    const Image<RGB> bright = render_wall(256, true, 1.f, threshold);
    const Image<RGB> dim = render_wall(256, true, 0.001f, threshold);
    const Image<RGB> all_samples = render_wall(256, false, 1.f);

    std::size_t proportional = 0;
    std::size_t stopped_early = 0;
    for (std::size_t i = 0; i < bright.size(); ++i) {
        const float b = bright[i].total();
        const float d = dim[i].total();
        proportional += std::abs(1000.f * d - b) <= 1e-4f * b ? std::size_t{1} : std::size_t{0};
        stopped_early += b != all_samples[i].total() ? std::size_t{1} : std::size_t{0};
    }
    INFO(proportional << " of " << bright.size() << " pixels a thousand times as bright, and "
                      << stopped_early << " stopped before 256 samples");
    CHECK(stopped_early > bright.size() / 10);
    CHECK(proportional == bright.size());
}

TEST_CASE("Dynamic sampling renders the same image every time", "[render][dynamic_sampling]")
{
    // However the tiles are shared among threads, each round's samples come from the same
    // seeds.
    const Image<RGB> first = render_wall(256, true, 1.f, 0.3f);
    const Image<RGB> second = render_wall(256, true, 1.f, 0.3f);
    std::size_t different = 0;
    for (std::size_t i = 0; i < first.size(); ++i) {
        different += first[i].total() != second[i].total() ? std::size_t{1} : std::size_t{0};
    }
    CHECK(different == 0);
}
