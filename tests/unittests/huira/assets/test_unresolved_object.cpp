#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
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

/// An unresolved object whose irradiance is resolved by hand, as a subclass resolves it.
class ResolvedByHand : public UnresolvedObject<RGB> {
  public:
    using UnresolvedObject<RGB>::set_resolved_irradiance_;
};

std::vector<Time> times_at(std::initializer_list<double> ets)
{
    std::vector<Time> times;
    for (const double et : ets) {
        times.push_back(Time::from_et(et));
    }
    return times;
}

/// Each bin of a matches b's, to a relative tolerance.
void check_bins(const RGB& a, const RGB& b, float tolerance = 1e-6f)
{
    for (std::size_t i = 0; i < RGB::size(); ++i) {
        INFO("bin " << i);
        CHECK_THAT(a[i], WithinRel(b[i], tolerance));
    }
}

/// The scene, seen over an exposure from 0 to 1 s by a camera at its origin (with its true
/// state: nothing is displaced by light time or aberration), resolving its unresolved objects.
SceneView<RGB> view_of(const Scene<RGB>& scene,
                       const InstanceHandle<RGB>& camera,
                       std::optional<std::size_t> temporal_samples = std::nullopt)
{
    return SceneView<RGB>(scene,
                          Interval{Time::from_et(0.0), Time::from_et(1.0)},
                          camera,
                          ObservationMode::TRUE_STATE,
                          temporal_samples);
}
} // namespace

TEST_CASE("An unresolved object has the irradiance it is given, at every time",
          "[assets][unresolved]")
{
    const UnresolvedObject<RGB> dark;
    CHECK(dark.get_irradiance(Time::from_et(0.0)) == RGB{0.f});

    // A total irradiance is spread over the bins, by their widths:
    const UnresolvedObject<RGB> total(units::WattsPerMeterSquared(3.0));
    CHECK(total.get_irradiance(Time::from_et(0.0)) == RGB::from_total(3.f));
    CHECK_THAT(total.get_irradiance(Time::from_et(1e6)).total(), WithinRel(3.f, 1e-6f));

    const UnresolvedObject<RGB> spectral(SpectralIrradiance(RGB{1.f, 2.f, 3.f}));
    CHECK(spectral.get_irradiance(Time::from_et(-5.0)) == RGB{1.f, 2.f, 3.f});

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
    CHECK(object.get_irradiance(Time::from_et(0.0)) == RGB::from_total(2.f));

    object.set_irradiance(SpectralIrradiance(RGB{4.f, 5.f, 6.f}));
    CHECK(object.get_irradiance(Time::from_et(0.0)) == RGB{4.f, 5.f, 6.f});
    object.set_irradiance(units::WattsPerMeterSquared(1.0));
    CHECK(object.get_irradiance(Time::from_et(0.0)) == RGB::from_total(1.f));
}

TEST_CASE("An unresolved object made from a magnitude is 100 times fainter 5 magnitudes on",
          "[assets][unresolved]")
{
    Scene<RGB> scene;
    const RGB bright = scene.new_unresolved_object_from_magnitude(1.0).get_irradiance(Time{});
    const RGB faint = scene.new_unresolved_object_from_magnitude(6.0).get_irradiance(Time{});
    REQUIRE(faint.total() > 0.f);
    check_bins(bright / faint, RGB{100.f}, 1e-4f);
}

TEST_CASE("A resolved irradiance is interpolated through the exposure, and held past its ends",
          "[assets][unresolved]")
{
    ResolvedByHand object;
    object.set_resolved_irradiance_({RGB{0.f}, RGB{10.f}, RGB{20.f}}, times_at({0.0, 1.0, 2.0}));

    CHECK(object.get_irradiance(Time::from_et(0.0)) == RGB{0.f});
    CHECK(object.get_irradiance(Time::from_et(1.0)) == RGB{10.f});
    CHECK(object.get_irradiance(Time::from_et(2.0)) == RGB{20.f});
    CHECK_THAT(object.get_irradiance(Time::from_et(0.5))[0], WithinAbs(5.f, 1e-5f));
    CHECK_THAT(object.get_irradiance(Time::from_et(1.75))[1], WithinAbs(17.5f, 1e-5f));
    CHECK(object.get_irradiance(Time::from_et(-1.0)) == RGB{0.f});
    CHECK(object.get_irradiance(Time::from_et(3.0)) == RGB{20.f});

    // One irradiance, for any number of times, holds through the exposure:
    object.set_resolved_irradiance_({RGB{7.f}}, times_at({0.0, 1.0, 2.0}));
    CHECK(object.get_irradiance(Time::from_et(1.5)) == RGB{7.f});

    // An exposure with no length has no time to interpolate over: it is its start.
    object.set_resolved_irradiance_({RGB{1.f}, RGB{2.f}}, times_at({4.0, 4.0}));
    CHECK(object.get_irradiance(Time::from_et(4.0)) == RGB{1.f});

    // Setting an irradiance replaces the resolved ones:
    object.set_irradiance(units::WattsPerMeterSquared(3.0));
    CHECK(object.get_irradiance(Time::from_et(4.0)) == RGB::from_total(3.f));
}

TEST_CASE("A resolved irradiance is one for each time, or one for all of them, and valid",
          "[assets][unresolved]")
{
    ResolvedByHand object;
    object.set_resolved_irradiance_({RGB{1.f}, RGB{2.f}}, times_at({0.0, 1.0}));

    CHECK_THROWS(object.set_resolved_irradiance_({}, times_at({0.0, 1.0})));
    CHECK_THROWS(object.set_resolved_irradiance_({RGB{1.f}, RGB{2.f}}, times_at({0.0, 0.5, 1.0})));
    CHECK_THROWS(object.set_resolved_irradiance_({RGB{1.f}, RGB{2.f}}, {}));
    CHECK_THROWS(object.set_resolved_irradiance_({RGB{1.f}, RGB{-2.f}}, times_at({0.0, 1.0})));
    CHECK_THROWS(object.set_resolved_irradiance_({RGB{NaN_F}}, times_at({0.0, 1.0})));

    // None of which changed it:
    CHECK(object.get_irradiance(Time::from_et(0.0)) == RGB{1.f});
    CHECK(object.get_irradiance(Time::from_et(1.0)) == RGB{2.f});
}

TEST_CASE("A plain unresolved object keeps its irradiance when a scene view resolves it",
          "[assets][unresolved]")
{
    Scene<RGB> scene;
    auto camera = scene.root.new_instance(scene.new_camera_model());
    auto object = scene.new_unresolved_object(SpectralIrradiance(RGB{1.f, 2.f, 3.f}));
    scene.root.new_instance(object).set_position(0_m, 0_m, 10_m);

    const SceneView<RGB> view = view_of(scene, camera);
    CHECK(object.get_irradiance(Time::from_et(0.5)) == RGB{1.f, 2.f, 3.f});
}

TEST_CASE("An unresolved emitter's irradiance falls off as the square of its distance",
          "[assets][unresolved]")
{
    // An isotropic emitter of power P gives P / (4 pi d^2) at a distance d.
    Scene<RGB> scene;
    auto camera = scene.root.new_instance(scene.new_camera_model());

    auto total = scene.new_unresolved_emitter(units::Watt(4.0 * PI<double>()));
    scene.root.new_instance(total).set_position(0_m, 0_m, 2_m);

    const RGB P{1.f, 2.f, 3.f};
    auto spectral = scene.new_unresolved_emitter(units::SpectralWatts<RGB>(P));
    scene.root.new_instance(spectral).set_position(3_m, 0_m, 4_m); // 5 m away

    // Until it is seen, it gives nothing:
    CHECK(total.get_irradiance(Time::from_et(0.0)) == RGB{0.f});

    const SceneView<RGB> view = view_of(scene, camera);
    const Time t = Time::from_et(0.5);
    CHECK_THAT(total.get_irradiance(t).total(), WithinRel(0.25f, 1e-5f));
    check_bins(total.get_irradiance(t), RGB::from_total(0.25f), 1e-5f);
    check_bins(spectral.get_irradiance(t), P / (4.f * PI<float>() * 25.f), 1e-5f);
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
    scene.root.new_instance(scene.add_unresolved_object(emitter)).set_position(0_m, 0_m, 1_m);

    CHECK_THROWS(emitter->set_spectral_power(units::Watt(-1.0)));
    CHECK_THROWS(emitter->set_spectral_power(units::SpectralWatts<RGB>(RGB{INF_F})));
    emitter->set_spectral_power(units::SpectralWatts<RGB>(RGB{4.f * PI<float>()}));
    {
        const SceneView<RGB> view = view_of(scene, camera);
        check_bins(emitter->get_irradiance(Time::from_et(0.0)), RGB{1.f}, 1e-5f);
    }
    emitter->set_spectral_power(units::Watt(8.0 * PI<double>()));
    {
        const SceneView<RGB> view = view_of(scene, camera);
        check_bins(emitter->get_irradiance(Time::from_et(0.0)), RGB::from_total(2.f), 1e-5f);
    }
}

TEST_CASE("A moving emitter's irradiance follows its distance through the exposure",
          "[assets][unresolved]")
{
    // From 1 m to 2 m away over the exposure, sampled at its start, middle and end:
    Scene<RGB> scene;
    auto camera = scene.root.new_instance(scene.new_camera_model());
    auto emitter = scene.new_unresolved_emitter(units::SpectralWatts<RGB>(RGB{4.f * PI<float>()}));
    auto instance = scene.root.new_instance(emitter);
    instance.set_position(0_m, 0_m, 1_m);
    instance.set_velocity(0_mps, 0_mps, 1_mps);

    const SceneView<RGB> view = view_of(scene, camera, 3);
    CHECK_THAT(emitter.get_irradiance(Time::from_et(0.0))[0], WithinRel(1.f, 1e-5f));
    CHECK_THAT(emitter.get_irradiance(Time::from_et(0.5))[0], WithinRel(1.f / 2.25f, 1e-5f));
    CHECK_THAT(emitter.get_irradiance(Time::from_et(1.0))[0], WithinRel(0.25f, 1e-5f));
    // Between the samples, the irradiance is interpolated:
    CHECK_THAT(emitter.get_irradiance(Time::from_et(0.25))[0],
               WithinRel((1.f + 1.f / 2.25f) / 2.f, 1e-5f));
}

TEST_CASE("An emitter where the camera is has no irradiance to give", "[assets][unresolved]")
{
    // At no distance, the inverse square law gives an infinite irradiance, which is refused.
    Scene<RGB> scene;
    auto camera = scene.root.new_instance(scene.new_camera_model());
    scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(1.0)));
    CHECK_THROWS(view_of(scene, camera));
}
