#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

constexpr int SIZE = 32;
constexpr float SKY = 1e-3f;

/// A BSDF whose every value is NaN, as a broken material's might be.
class NaNBSDF final : public BSDF<RGB> {
  public:
    NaNBSDF() = default;

    NaNBSDF(const NaNBSDF&) = delete;
    NaNBSDF& operator=(const NaNBSDF&) = delete;
    NaNBSDF(NaNBSDF&&) = delete;
    NaNBSDF& operator=(NaNBSDF&&) = delete;

    [[nodiscard]] BSDFRequirements requirements() const override { return {}; }

    [[nodiscard]] RGB eval(const Vec3<float>&,
                           const Vec3<float>&,
                           const Interaction<RGB>&,
                           const ShadingParams<RGB>&) const override
    {
        return RGB{std::numeric_limits<float>::quiet_NaN()};
    }

    [[nodiscard]] BSDFSample<RGB> sample(const Vec3<float>& wo,
                                         const Interaction<RGB>& isect,
                                         const ShadingParams<RGB>&,
                                         float,
                                         float) const override
    {
        BSDFSample<RGB> result;
        result.wi = glm::reflect(-wo, isect.normal_s);
        result.value = RGB{std::numeric_limits<float>::quiet_NaN()};
        result.pdf = 1.f;
        return result;
    }

    [[nodiscard]] float pdf(const Vec3<float>&,
                            const Vec3<float>&,
                            const Interaction<RGB>&,
                            const ShadingParams<RGB>&) const override
    {
        return 1.f;
    }

    std::string type() const override { return "NaNBSDF"; }
};

struct Counts {
    std::size_t not_finite = 0;
    std::size_t pixels = 0;
};

Counts count_not_finite(const Image<RGB>& image)
{
    Counts counts;
    for (std::size_t i = 0; i < image.size(); ++i) {
        ++counts.pixels;
        if (!std::isfinite(image[i][0]) || !std::isfinite(image[i][1]) ||
            !std::isfinite(image[i][2])) {
            ++counts.not_finite;
        }
    }
    return counts;
}

} // namespace

TEST_CASE("The sky seen through a transparent surface", "[render][nan]")
{
    // The camera is inside a sphere that every ray passes through (its alpha is 0). Passing
    // through a surface enters its primitive's medium (a vacuum by default), and a ray that then
    // left the scene had an infinite path through it, whose transmittance came out as
    // exp(-0 * inf) / exp(-0 * inf): NaN. Every sample was NaN, so every pixel was too.
    auto render = [](bool sphere) {
        Scene<RGB> scene;
        auto camera_model = scene.new_camera_model();
        camera_model.configure_sensor_from_pitch({SIZE, SIZE}, 10_um);
        camera_model.set_focal_length(50_mm);
        camera_model.delete_psf();
        camera_model.enable_depth_of_field(false);
        auto camera = scene.root.new_instance(camera_model);

        if (sphere) {
            auto material = scene.new_material(scene.new_bsdf_lambertian());
            material.set_alpha_factor(0.f);
            scene.root.new_instance(
                scene.add_primitive(scene.add_ellipsoid(10_m, 10_m, 10_m), material));
        }
        scene.set_background_radiance(SKY);

        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_received_power();
        Renderer<RGB> renderer;
        renderer.set_samples_per_pixel(4);
        Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
        SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
        renderer.render(scene_view, frame_buffer);
        return frame_buffer.received_power();
    };

    const Image<RGB> through = render(true);
    const Image<RGB> sky = render(false);
    REQUIRE(count_not_finite(through).not_finite == 0);
    for (std::size_t i = 0; i < sky.size(); ++i) {
        CHECK(std::abs(through[i][1] - sky[i][1]) <= 1e-5f * sky[i][1]);
    }
}

TEST_CASE("A pixel that is not finite stays where it is through the PSF convolution",
          "[render][nan]")
{
    // A ball whose material gives NaN, a few pixels across, in a sunlit frame with a sky,
    // convolved with the aperture's PSF (the default). The ball's pixels have no finite sample,
    // so are NaN. Convolution through FFTs used to spread them to every pixel.
    Scene<RGB> scene;
    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch({SIZE, SIZE}, 10_um);
    camera_model.set_focal_length(50_mm);
    camera_model.set_fstop(8.f);
    camera_model.use_aperture_psf(4, 2); // small, so quick to build
    camera_model.set_psf_convolution_radius(16);
    camera_model.enable_depth_of_field(false);
    auto camera = scene.root.new_instance(camera_model);

    auto material = scene.new_material(scene.add_bsdf(std::make_shared<NaNBSDF>()));
    auto ball = scene.root.new_instance(
        scene.add_primitive(scene.add_ellipsoid(5_mm, 5_mm, 5_mm), material));
    ball.set_position(0_m, 0_m, 10_m); // 2.5 px in radius
    auto sun = scene.root.new_instance(scene.new_sun_light());
    sun.set_position(0_m, 0_m, -1_au);
    scene.set_background_radiance(SKY);

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    renderer.set_samples_per_pixel(4);
    Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
    SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
    renderer.render(scene_view, frame_buffer);

    const Counts counts = count_not_finite(frame_buffer.received_power());
    INFO(counts.not_finite << " of " << counts.pixels << " pixels are not finite");
    CHECK(counts.not_finite > 0);
    CHECK(counts.not_finite <= 36); // the ball's pixels, and no more
}
