#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {
constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
constexpr double INF = std::numeric_limits<double>::infinity();
constexpr float NaN_F = std::numeric_limits<float>::quiet_NaN();
constexpr float INF_F = std::numeric_limits<float>::infinity();

using SpectralIrradiance = units::SpectralWattsPerMeterSquared<RGB>;

/// An unresolved object that resolves to the irradiances it is given, as a subclass resolves its
/// own.
class ResolvesTo : public UnresolvedObject<RGB> {
  public:
    ResolvesTo(const ResolvesTo&) = delete;
    ResolvesTo& operator=(const ResolvesTo&) = delete;
    ResolvesTo(ResolvesTo&&) = delete;
    ResolvesTo& operator=(ResolvesTo&&) = delete;

    explicit ResolvesTo(std::vector<RGB> irradiances) : irradiances_(std::move(irradiances)) {}

    std::vector<RGB> resolve_irradiance(const std::vector<Transform<float>>& /*self_transforms*/,
                                        const std::vector<Time>& /*times*/,
                                        const SceneView<RGB>& /*scene_view*/,
                                        RandomSampler<float>& /*sampler*/) const override
    {
        return irradiances_;
    }

  private:
    std::vector<RGB> irradiances_;
};

/// Each bin of a matches b's, to a relative tolerance.
void check_bins(const RGB& a, const RGB& b, float tolerance = 1e-6f)
{
    for (std::size_t i = 0; i < RGB::size(); ++i) {
        INFO("bin " << i);
        CHECK_THAT(a[i], WithinRel(b[i], tolerance));
    }
}

/// The scene, seen over an exposure (from 0 to 1 s, by default) by a camera at its origin (with
/// its true state: nothing is displaced by light time or aberration), resolving its unresolved
/// objects.
SceneView<RGB> view_of(const Scene<RGB>& scene,
                       const InstanceHandle<RGB>& camera,
                       std::optional<std::size_t> temporal_samples = std::nullopt,
                       Interval exposure = Interval{Time::from_et(0.0), Time::from_et(1.0)})
{
    return SceneView<RGB>(scene, exposure, camera, ObservationMode::TRUE_STATE, temporal_samples);
}

/// An instance of an unresolved object that moves along z (so that it has as many temporal
/// samples as a view asks for).
InstanceHandle<RGB> moving_instance(Scene<RGB>& scene,
                                    std::shared_ptr<UnresolvedObject<RGB>> object)
{
    auto instance = scene.root.new_instance(scene.add_unresolved_object(std::move(object)));
    instance.set_position(0_m, 0_m, 1_m);
    instance.set_velocity(0_mps, 0_mps, 1_mps);
    return instance;
}
} // namespace

TEST_CASE("An unresolved object has the irradiance it is given", "[assets][unresolved]")
{
    const UnresolvedObject<RGB> dark;
    CHECK(dark.get_irradiance() == RGB{0.f});

    // A total irradiance is spread over the bins, by their widths:
    const UnresolvedObject<RGB> total(units::WattsPerMeterSquared(3.0));
    CHECK(total.get_irradiance() == RGB::from_total(3.f));
    CHECK_THAT(total.get_irradiance().total(), WithinRel(3.f, 1e-6f));

    const UnresolvedObject<RGB> spectral(SpectralIrradiance(RGB{1.f, 2.f, 3.f}));
    CHECK(spectral.get_irradiance() == RGB{1.f, 2.f, 3.f});

    CHECK(dark.type() == "UnresolvedObject");
}

TEST_CASE("An unresolved object's irradiance must be finite and not negative",
          "[assets][unresolved]")
{
    CHECK_THROWS(UnresolvedObject<RGB>(units::WattsPerMeterSquared(-1.0)));
    CHECK_THROWS(UnresolvedObject<RGB>(units::WattsPerMeterSquared(NaN)));
    CHECK_THROWS(UnresolvedObject<RGB>(units::WattsPerMeterSquared(INF)));
    CHECK_THROWS(UnresolvedObject<RGB>(SpectralIrradiance(RGB{1.f, -1.f, 1.f})));
    CHECK_THROWS(UnresolvedObject<RGB>(SpectralIrradiance(RGB{NaN_F})));
    CHECK_THROWS(UnresolvedObject<RGB>(SpectralIrradiance(RGB{INF_F})));

    // Setting a bad irradiance leaves the one it had:
    Scene<RGB> scene;
    auto object = scene.new_unresolved_object(units::WattsPerMeterSquared(2.0));
    CHECK_THROWS(object.set_irradiance(units::WattsPerMeterSquared(-2.0)));
    CHECK_THROWS(object.set_irradiance(SpectralIrradiance(RGB{NaN_F})));
    CHECK(object.get_irradiance() == RGB::from_total(2.f));

    object.set_irradiance(SpectralIrradiance(RGB{4.f, 5.f, 6.f}));
    CHECK(object.get_irradiance() == RGB{4.f, 5.f, 6.f});
    object.set_irradiance(units::WattsPerMeterSquared(1.0));
    CHECK(object.get_irradiance() == RGB::from_total(1.f));
}

TEST_CASE("An unresolved object made from a magnitude is 100 times fainter 5 magnitudes on",
          "[assets][unresolved]")
{
    Scene<RGB> scene;
    const RGB bright = scene.new_unresolved_object_from_magnitude(1.0).get_irradiance();
    const RGB faint = scene.new_unresolved_object_from_magnitude(6.0).get_irradiance();
    REQUIRE(faint.total() > 0.f);
    check_bins(bright / faint, RGB{100.f}, 1e-4f);
}

TEST_CASE("A plain unresolved object gives its irradiance wherever it is placed, and a view keeps "
          "what it resolved",
          "[assets][unresolved]")
{
    Scene<RGB> scene;
    auto camera = scene.root.new_instance(scene.new_camera_model());
    auto object = scene.new_unresolved_object(SpectralIrradiance(RGB{1.f, 2.f, 3.f}));
    auto near = scene.root.new_instance(object);
    near.set_position(0_m, 0_m, 10_m);
    auto far = scene.root.new_instance(object);
    far.set_position(0_m, 0_m, 1000_m);

    const SceneView<RGB> view = view_of(scene, camera);
    CHECK(view.unresolved_irradiance(near, Time::from_et(0.5)) == RGB{1.f, 2.f, 3.f});
    CHECK(view.unresolved_irradiance(far, Time::from_et(0.5)) == RGB{1.f, 2.f, 3.f});

    // A new irradiance is resolved by the next view:
    object.set_irradiance(SpectralIrradiance(RGB{4.f, 5.f, 6.f}));
    CHECK(view.unresolved_irradiance(near, Time::from_et(0.5)) == RGB{1.f, 2.f, 3.f});
    const SceneView<RGB> next = view_of(scene, camera);
    CHECK(next.unresolved_irradiance(near, Time::from_et(0.5)) == RGB{4.f, 5.f, 6.f});

    // Only an instance of an unresolved object in the view has one:
    CHECK_THROWS(view.unresolved_irradiance(camera, Time::from_et(0.5)));
    auto later = scene.root.new_instance(object);
    CHECK_THROWS(view.unresolved_irradiance(later, Time::from_et(0.5)));
}

TEST_CASE("A resolved irradiance is interpolated through the exposure, and held past its ends",
          "[assets][unresolved]")
{
    // Three temporal samples, at 0, 0.5 and 1 s:
    Scene<RGB> scene;
    auto camera = scene.root.new_instance(scene.new_camera_model());
    auto ramp = moving_instance(
        scene, std::make_shared<ResolvesTo>(std::vector<RGB>{RGB{0.f}, RGB{10.f}, RGB{20.f}}));
    auto constant =
        moving_instance(scene, std::make_shared<ResolvesTo>(std::vector<RGB>{RGB{7.f}}));

    const SceneView<RGB> view = view_of(scene, camera, 3);
    const auto at = [&](double et) { return view.unresolved_irradiance(ramp, Time::from_et(et)); };
    CHECK(at(0.0) == RGB{0.f});
    CHECK(at(0.5) == RGB{10.f});
    CHECK(at(1.0) == RGB{20.f});
    CHECK_THAT(at(0.25)[0], WithinAbs(5.f, 1e-5f));
    CHECK_THAT(at(0.875)[1], WithinAbs(17.5f, 1e-5f));
    CHECK(at(-1.0) == RGB{0.f});
    CHECK(at(3.0) == RGB{20.f});

    // One irradiance, for all the samples, holds through the exposure:
    CHECK(view.unresolved_irradiance(constant, Time::from_et(0.75)) == RGB{7.f});

    // An exposure with no length has no time to interpolate over: it is its start.
    const SceneView<RGB> instant =
        view_of(scene, camera, 3, Interval{Time::from_et(4.0), Time::from_et(4.0)});
    CHECK(instant.unresolved_irradiance(ramp, Time::from_et(4.0)) == RGB{0.f});
}

TEST_CASE("A resolved irradiance is one for each temporal sample, or one for all, and valid",
          "[assets][unresolved]")
{
    const auto resolve = [](std::vector<RGB> irradiances) {
        Scene<RGB> scene;
        auto camera = scene.root.new_instance(scene.new_camera_model());
        moving_instance(scene, std::make_shared<ResolvesTo>(std::move(irradiances)));
        const SceneView<RGB> view = view_of(scene, camera, 2);
    };
    CHECK_NOTHROW(resolve({RGB{1.f}, RGB{2.f}}));
    CHECK_NOTHROW(resolve({RGB{1.f}}));
    CHECK_THROWS(resolve({}));
    CHECK_THROWS(resolve({RGB{1.f}, RGB{2.f}, RGB{3.f}}));
    CHECK_THROWS(resolve({RGB{1.f}, RGB{-2.f}}));
    CHECK_THROWS(resolve({RGB{NaN_F}}));
}

TEST_CASE("An unresolved emitter's irradiance falls off as the square of its distance",
          "[assets][unresolved]")
{
    // An isotropic emitter of power P gives P / (4 pi d^2) at a distance d.
    Scene<RGB> scene;
    auto camera = scene.root.new_instance(scene.new_camera_model());

    auto total = scene.new_unresolved_emitter(units::Watt(4.0 * PI<double>()));
    auto total_instance = scene.root.new_instance(total);
    total_instance.set_position(0_m, 0_m, 2_m);

    const RGB P{1.f, 2.f, 3.f};
    auto spectral = scene.new_unresolved_emitter(units::SpectralWatts<RGB>(P));
    auto spectral_instance = scene.root.new_instance(spectral);
    spectral_instance.set_position(3_m, 0_m, 4_m); // 5 m away

    const SceneView<RGB> view = view_of(scene, camera);
    const Time t = Time::from_et(0.5);
    CHECK_THAT(view.unresolved_irradiance(total_instance, t).total(), WithinRel(0.25f, 1e-5f));
    check_bins(view.unresolved_irradiance(total_instance, t), RGB::from_total(0.25f), 1e-5f);
    check_bins(
        view.unresolved_irradiance(spectral_instance, t), P / (4.f * PI<float>() * 25.f), 1e-5f);
    CHECK(total.type() == "UnresolvedEmitter");
}

TEST_CASE("An unresolved emitter's power must be finite and not negative", "[assets][unresolved]")
{
    CHECK_THROWS(UnresolvedEmitter<RGB>(units::Watt(-1.0)));
    CHECK_THROWS(UnresolvedEmitter<RGB>(units::Watt(NaN)));
    CHECK_THROWS(UnresolvedEmitter<RGB>(units::Watt(INF)));
    CHECK_THROWS(UnresolvedEmitter<RGB>(units::SpectralWatts<RGB>(RGB{-1.f})));
    CHECK_THROWS(UnresolvedEmitter<RGB>(units::SpectralWatts<RGB>(RGB{1.f, NaN_F, 1.f})));
    CHECK_NOTHROW(UnresolvedEmitter<RGB>(units::Watt(0.0)));

    // A new power is used the next time the emitter is resolved:
    Scene<RGB> scene;
    auto camera = scene.root.new_instance(scene.new_camera_model());
    auto emitter = std::make_shared<UnresolvedEmitter<RGB>>(units::Watt(1.0));
    auto instance = scene.root.new_instance(scene.add_unresolved_object(emitter));
    instance.set_position(0_m, 0_m, 1_m);

    CHECK_THROWS(emitter->set_spectral_power(units::Watt(-1.0)));
    CHECK_THROWS(emitter->set_spectral_power(units::SpectralWatts<RGB>(RGB{INF_F})));
    emitter->set_spectral_power(units::SpectralWatts<RGB>(RGB{4.f * PI<float>()}));
    {
        const SceneView<RGB> view = view_of(scene, camera);
        check_bins(view.unresolved_irradiance(instance, Time::from_et(0.0)), RGB{1.f}, 1e-5f);
    }
    emitter->set_spectral_power(units::Watt(8.0 * PI<double>()));
    {
        const SceneView<RGB> view = view_of(scene, camera);
        check_bins(
            view.unresolved_irradiance(instance, Time::from_et(0.0)), RGB::from_total(2.f), 1e-5f);
    }
}

TEST_CASE("A moving emitter's irradiance follows its distance through the exposure",
          "[assets][unresolved]")
{
    // From 1 m to 2 m away over the exposure, sampled at its start, middle and end:
    Scene<RGB> scene;
    auto camera = scene.root.new_instance(scene.new_camera_model());
    auto instance = moving_instance(scene,
                                    std::make_shared<UnresolvedEmitter<RGB>>(
                                        units::SpectralWatts<RGB>(RGB{4.f * PI<float>()})));

    const SceneView<RGB> view = view_of(scene, camera, 3);
    const auto at = [&](double et) {
        return view.unresolved_irradiance(instance, Time::from_et(et))[0];
    };
    CHECK_THAT(at(0.0), WithinRel(1.f, 1e-5f));
    CHECK_THAT(at(0.5), WithinRel(1.f / 2.25f, 1e-5f));
    CHECK_THAT(at(1.0), WithinRel(0.25f, 1e-5f));
    // Between the samples, the irradiance is interpolated:
    CHECK_THAT(at(0.25), WithinRel((1.f + 1.f / 2.25f) / 2.f, 1e-5f));
}

TEST_CASE("An emitter placed twice is resolved for each place", "[assets][unresolved]")
{
    // Before, a view resolved an unresolved object into the object, so that each of its instances
    // was drawn with the irradiance of whichever was resolved last: here, both at 2 m.
    Scene<RGB> scene;
    auto camera = scene.root.new_instance(scene.new_camera_model());
    auto emitter = scene.new_unresolved_emitter(units::SpectralWatts<RGB>(RGB{4.f * PI<float>()}));
    auto near = scene.root.new_instance(emitter);
    near.set_position(0_m, 0_m, 1_m);
    auto far = scene.root.new_instance(emitter);
    far.set_position(0_m, 0_m, 2_m);

    const SceneView<RGB> view = view_of(scene, camera);
    check_bins(view.unresolved_irradiance(near, Time::from_et(0.5)), RGB{1.f}, 1e-5f);
    check_bins(view.unresolved_irradiance(far, Time::from_et(0.5)), RGB{0.25f}, 1e-5f);
}

TEST_CASE("Each view of an emitter has its own irradiance", "[assets][unresolved]")
{
    // Before, the last view made set the irradiance for every view: here, the far camera's for
    // both, so that the near camera's frame, rendered after the far camera's view was made, was a
    // quarter as bright as it should have been.
    const auto near_frame = [](bool far_view_too) {
        Scene<RGB> scene;
        auto emitter =
            scene.new_unresolved_emitter(units::SpectralWatts<RGB>(RGB{4.f * PI<float>()}));
        auto instance = scene.root.new_instance(emitter);
        instance.set_position(0_m, 0_m, 1_m);
        auto near_model = scene.new_camera_model();
        near_model.configure_sensor_from_pitch({32, 32}, units::Micrometer(8.5));
        auto near_camera = scene.root.new_instance(near_model);
        auto far_camera = scene.root.new_instance(scene.new_camera_model());
        far_camera.set_position(0_m, 0_m, -1_m);

        SceneView<RGB> near_view = view_of(scene, near_camera); // rendered, so not const
        std::optional<SceneView<RGB>> far_view;
        if (far_view_too) {
            far_view.emplace(scene,
                             Interval{Time::from_et(0.0), Time::from_et(1.0)},
                             far_camera,
                             ObservationMode::TRUE_STATE);
            check_bins(
                far_view->unresolved_irradiance(instance, Time::from_et(0.5)), RGB{0.25f}, 1e-5f);
        }
        check_bins(near_view.unresolved_irradiance(instance, Time::from_et(0.5)), RGB{1.f}, 1e-5f);

        auto frame_buffer = near_model.make_frame_buffer();
        frame_buffer.enable_received_power();
        Renderer<RGB> renderer;
        renderer.render(near_view, frame_buffer);
        double total = 0.0;
        for (std::size_t i = 0; i < frame_buffer.received_power().size(); ++i) {
            total += static_cast<double>(frame_buffer.received_power()[i].total());
        }
        return total;
    };
    const double alone = near_frame(false);
    REQUIRE(alone > 0.0);
    CHECK_THAT(near_frame(true), WithinRel(alone, 1e-6));
}

TEST_CASE("An emitter where the camera is has no irradiance to give", "[assets][unresolved]")
{
    // At no distance, the inverse square law gives an infinite irradiance, which is refused.
    Scene<RGB> scene;
    auto camera = scene.root.new_instance(scene.new_camera_model());
    scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(1.0)));
    CHECK_THROWS(view_of(scene, camera));
}
