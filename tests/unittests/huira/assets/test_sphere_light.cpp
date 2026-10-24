#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;
using Catch::Matchers::WithinRel;

namespace {
constexpr float NaN_F = std::numeric_limits<float>::quiet_NaN();
constexpr float INF_F = std::numeric_limits<float>::infinity();

using Radiance = units::SpectralWattsPerMeterSquaredSteradian<RGB>;

/// A light's transform, with the light at a position.
Transform<float> placed_at(const Vec3<float>& position)
{
    Transform<float> transform;
    transform.position = position;
    return transform;
}

/// A shading point at a position.
Interaction<RGB> point_at(const Vec3<float>& position)
{
    Interaction<RGB> isect;
    isect.position = position;
    return isect;
}

/// Each bin of a matches b's, to a relative tolerance.
void check_bins(const RGB& a, const RGB& b, float tolerance = 1e-6f)
{
    for (std::size_t i = 0; i < RGB::size(); ++i) {
        INFO("bin " << i);
        CHECK_THAT(a[i], WithinRel(b[i], tolerance));
    }
}

/// A light of another kind, which does not name its type.
class TestLight : public Light<RGB> {
  public:
    TestLight() = default;
    TestLight(const TestLight&) = delete;
    TestLight& operator=(const TestLight&) = delete;
    TestLight(TestLight&&) = delete;
    TestLight& operator=(TestLight&&) = delete;

    std::optional<LightSample<RGB>> sample_li(const Interaction<RGB>& /*ref*/,
                                              const Transform<float>& /*light_to_world*/,
                                              Sampler<float>& /*sampler*/) const override
    {
        return std::nullopt;
    }
    float pdf_li(const Interaction<RGB>& /*ref*/,
                 const Transform<float>& /*light_to_world*/,
                 const Vec3<float>& /*wi*/) const override
    {
        return 0.f;
    }
    RGB radiance(const Vec3<float>& /*point_on_light*/,
                 const Vec3<float>& /*outgoing_direction*/) const override
    {
        return RGB{0.f};
    }
    RGB irradiance_at(const Vec3<float>& /*position*/,
                      const Transform<float>& /*light_to_world*/) const override
    {
        return RGB{0.f};
    }
    LightType get_type() const override { return LightType::Sphere; }
};
} // namespace

TEST_CASE("A sphere light's radius must be finite and not negative", "[assets][lights]")
{
    const Radiance radiance(RGB{1.f});
    CHECK_THROWS(SphereLight<RGB>(-1_m, radiance));
    CHECK_THROWS(SphereLight<RGB>(units::Meter(NaN_F), radiance)); // (as a length, even)
    CHECK_THROWS(SphereLight<RGB>(units::Meter(INF_F), radiance));
    CHECK_THROWS(SphereLight<RGB>(-1_m, units::SpectralWatts<RGB>(RGB{1.f})));
    CHECK_THROWS(SphereLight<RGB>(-1_m, units::Watt(1.0)));
    CHECK_THROWS(SphereLight<RGB>(-1_m, units::Kelvin(5800.0)));

    CHECK(SphereLight<RGB>(2_m, radiance).radius().to_si() == 2.0);
    CHECK(SphereLight<RGB>(25_cm, radiance).radius().to_si() == 0.25f);

    // A point light (radius 0) is given the smallest radius, so that it has an area to spread its
    // power over:
    const SphereLight<RGB> point(0_m, units::Watt(1.0));
    CHECK(point.radius().to_si() == 1e-5f);
    CHECK(point.radiance(Vec3<float>{0.f}, Vec3<float>{0.f, 0.f, 1.f}).valid());
}

TEST_CASE("A sphere light given its radiance has that radiance everywhere, in every direction",
          "[assets][lights]")
{
    const RGB L{0.5f, 1.f, 2.f};
    const SphereLight<RGB> light(1_m, Radiance(L));
    CHECK(light.radiance(Vec3<float>{0.f, 0.f, 1.f}, Vec3<float>{0.f, 0.f, 1.f}) == L);
    CHECK(light.radiance(Vec3<float>{1.f, 0.f, 0.f}, Vec3<float>{0.f, -1.f, 0.f}) == L);

    CHECK_THROWS(SphereLight<RGB>(1_m, Radiance(RGB{-1.f})));
    CHECK_THROWS(SphereLight<RGB>(1_m, Radiance(RGB{1.f, INF_F, 1.f})));
    CHECK_NOTHROW(SphereLight<RGB>(1_m, Radiance(RGB{0.f}))); // a dark light
}

TEST_CASE("A sphere light given its spectral power spreads it over its surface", "[assets][lights]")
{
    // A sphere of radius r emitting a power P has a radiance of P / (4 pi^2 r^2): the power over
    // its surface, 4 pi r^2, is its exitance, which is pi times its radiance, as a Lambertian
    // emitter's is.
    const RGB P{1.f, 2.f, 3.f};
    const float r = 0.5f;
    const SphereLight<RGB> light{units::Meter(r), units::SpectralWatts<RGB>(P)};
    const RGB expected = P / (4.f * PI<float>() * PI<float>() * r * r);
    check_bins(light.radiance(Vec3<float>{0.f}, Vec3<float>{0.f, 0.f, 1.f}), expected);

    // So that, far away, all the power is accounted for: the irradiance is P / (4 pi d^2), as it
    // is from an unresolved emitter of the same power.
    const float d = 20.f;
    const RGB far = light.irradiance_at(Vec3<float>{0.f}, placed_at(Vec3<float>{0.f, 0.f, d}));
    check_bins(far, P / (4.f * PI<float>() * d * d), 1e-5f);

    CHECK_THROWS(SphereLight<RGB>(1_m, units::SpectralWatts<RGB>(RGB{-1.f})));
    CHECK_THROWS(SphereLight<RGB>(1_m, units::SpectralWatts<RGB>(RGB{NaN_F})));
    // A finite power over a tiny sphere can still give a radiance too large for a float:
    CHECK_THROWS(SphereLight<RGB>(0_m, units::SpectralWatts<RGB>(RGB{3e38f})));
}

TEST_CASE("A sphere light's total power must be finite and not negative", "[assets][lights]")
{
    CHECK_THROWS(SphereLight<RGB>(1_m, units::Watt(-1.0)));
    CHECK_THROWS(SphereLight<RGB>(1_m, units::Watt(std::numeric_limits<double>::quiet_NaN())));
    CHECK_THROWS(SphereLight<RGB>(1_m, units::Watt(std::numeric_limits<double>::infinity())));
    CHECK_THROWS(SphereLight<RGB>(0_m, units::Watt(3e38)));
    CHECK_NOTHROW(SphereLight<RGB>(1_m, units::Watt(0.0)));
}

TEST_CASE("A sphere light's total power is spread over its spectral bins", "[assets][lights]")
{
    // Before, the whole power was given to each bin, so that a light of 1 W emitted 3 W in RGB
    // (and 8 W in Visible8), and gave three times the irradiance an unresolved emitter of 1 W
    // gives.
    const SphereLight<RGB> light{1_m, units::Watt(3.0)};
    const SphereLight<RGB> spread{1_m, units::SpectralWatts<RGB>(RGB::from_total(3.f))};
    check_bins(light.radiance(Vec3<float>{0.f}, Vec3<float>{0.f, 0.f, 1.f}),
               spread.radiance(Vec3<float>{0.f}, Vec3<float>{0.f, 0.f, 1.f}));

    // So that, far away, its irradiance is the inverse square law's, P / (4 pi d^2):
    const float d = 100.f;
    const RGB far = light.irradiance_at(Vec3<float>{0.f}, placed_at(Vec3<float>{0.f, 0.f, d}));
    CHECK_THAT(far.total(), WithinRel(3.f / (4.f * PI<float>() * d * d), 1e-5f));

    const SphereLight<Visible8> visible{1_m, units::Watt(3.0)};
    const Visible8 visible_far =
        visible.irradiance_at(Vec3<float>{0.f}, placed_at(Vec3<float>{0.f, 0.f, d}));
    CHECK_THAT(visible_far.total(), WithinRel(3.f / (4.f * PI<float>() * d * d), 1e-5f));

    // And as the scene makes it:
    Scene<RGB> scene;
    auto made = scene.new_sphere_light(1_m, units::Watt(3.0));
    check_bins(made.get()->irradiance_at(Vec3<float>{0.f}, placed_at(Vec3<float>{0.f, 0.f, d})),
               far);
}

TEST_CASE("A sphere light given a temperature glows as a black body", "[assets][lights]")
{
    const SphereLight<RGB> light(1_m, units::Kelvin(5800.0));
    check_bins(light.radiance(Vec3<float>{0.f}, Vec3<float>{0.f, 0.f, 1.f}),
               black_body<RGB>(5800.0, 1000));

    const SphereLight<RGB> hotter(1_m, units::Kelvin(10000.0));
    CHECK(hotter.radiance(Vec3<float>{0.f}, Vec3<float>{0.f, 0.f, 1.f}).total() >
          light.radiance(Vec3<float>{0.f}, Vec3<float>{0.f, 0.f, 1.f}).total());

    CHECK_THROWS(SphereLight<RGB>(1_m, units::Kelvin(-10.0)));
}

TEST_CASE("A sphere light's irradiance falls off as the square of the distance, and is zero "
          "inside it",
          "[assets][lights]")
{
    // A sphere of radiance L, radius r, at a distance d, gives an irradiance of
    // pi L (r / d)^2 on a surface facing it.
    const RGB L{1.f, 2.f, 4.f};
    const float r = 2.f;
    const SphereLight<RGB> light{units::Meter(r), Radiance(L)};
    const Transform<float> light_to_world = placed_at(Vec3<float>{1.f, 2.f, 3.f});

    for (const float d : {2.5f, 10.f, 1000.f}) {
        INFO("d = " << d);
        const Vec3<float> position = light_to_world.position + Vec3<float>{0.f, d, 0.f};
        check_bins(light.irradiance_at(position, light_to_world),
                   L * (PI<float>() * (r * r) / (d * d)),
                   1e-5f);
    }

    CHECK(light.irradiance_at(light_to_world.position, light_to_world) == RGB{0.f});
    CHECK(light.irradiance_at(light_to_world.position + Vec3<float>{0.f, 0.f, 1.9f},
                              light_to_world) == RGB{0.f});
}

TEST_CASE("A sphere light is sampled uniformly over the cone it subtends", "[assets][lights]")
{
    const RGB L{1.f, 2.f, 4.f};
    const float r = 2.f;
    const float d = 10.f;
    const SphereLight<RGB> light{units::Meter(r), Radiance(L)};
    const Transform<float> light_to_world = placed_at(Vec3<float>{0.f, 0.f, d});
    const Interaction<RGB> origin = point_at(Vec3<float>{0.f});

    const double sin2_max = static_cast<double>(r * r) / static_cast<double>(d * d);
    const double cos_max = std::sqrt(1.0 - sin2_max);
    const double solid_angle = 2.0 * PI<double>() * (1.0 - cos_max);

    // Each sample's worst departure from what it should be:
    RandomSampler<float> sampler(7);
    constexpr std::size_t N = 20000;
    RGB irradiance{0.f};
    bool all_sampled = true;
    bool all_radiance = true;
    double least_cos = 1.0;      // of the angle from the light's center
    double worst_length = 0.0;   // of the direction, from 1
    double worst_pdf = 0.0;      // relative, from 1 / solid_angle
    double worst_pdf_li = 0.0;   // relative, of pdf_li() from the sample's
    double worst_on_light = 0.0; // relative, of the sampled point's distance from the center
    for (std::size_t i = 0; i < N; ++i) {
        const std::optional<LightSample<RGB>> sample =
            light.sample_li(origin, light_to_world, sampler);
        if (!sample) {
            all_sampled = false;
            continue;
        }
        all_radiance = all_radiance && sample->Li == L;
        const auto pdf = static_cast<double>(sample->pdf);
        least_cos = std::min(least_cos, static_cast<double>(sample->wi.z));
        worst_length =
            std::max(worst_length, std::abs(static_cast<double>(glm::length(sample->wi)) - 1.0));
        worst_pdf = std::max(worst_pdf, std::abs(pdf * solid_angle - 1.0));
        const auto pdf_li = static_cast<double>(light.pdf_li(origin, light_to_world, sample->wi));
        worst_pdf_li = std::max(worst_pdf_li, std::abs(pdf_li / pdf - 1.0));
        const Vec3<float> on_light = sample->wi * sample->distance;
        const auto from_center =
            static_cast<double>(glm::length(on_light - light_to_world.position));
        worst_on_light = std::max(worst_on_light, std::abs(from_center / r - 1.0));
        irradiance += sample->Li * (sample->wi.z / sample->pdf);
    }
    CHECK(all_sampled);
    CHECK(all_radiance);
    // The directions are within the cone (to the precision of a float), uniformly, and the
    // distances are to where they meet the sphere:
    CHECK(least_cos >= cos_max - 1e-6);
    CHECK(worst_length < 1e-5);
    CHECK(worst_pdf < 1e-4);
    CHECK(worst_pdf_li < 1e-4);
    CHECK(worst_on_light < 1e-3);

    // On average, the samples give the irradiance a surface facing the light receives:
    irradiance /= static_cast<float>(N);
    check_bins(irradiance, light.irradiance_at(origin.position, light_to_world), 1e-3f);

    // Directions that miss the sphere have no density:
    CHECK(light.pdf_li(origin, light_to_world, Vec3<float>{0.f, 0.f, -1.f}) == 0.f);
    CHECK(light.pdf_li(origin, light_to_world, glm::normalize(Vec3<float>{1.f, 0.f, 1.f})) == 0.f);

    // Inside the light, nothing is sampled:
    const Interaction<RGB> inside = point_at(Vec3<float>{0.f, 0.f, d - 1.f});
    CHECK_FALSE(light.sample_li(inside, light_to_world, sampler).has_value());
    CHECK(light.pdf_li(inside, light_to_world, Vec3<float>{0.f, 0.f, 1.f}) == 0.f);
}

TEST_CASE("A sphere light is a light of type Sphere", "[assets][lights]")
{
    const SphereLight<RGB> light(1_m, Radiance(RGB{1.f}));
    CHECK(light.get_type() == LightType::Sphere);
    CHECK(light.type() == "SphereLight");

    Scene<RGB> scene;
    auto sun = scene.new_sun_light();
    CHECK(sun.type() == "SphereLight");
    CHECK(sun.name() == "Sun");
    REQUIRE(sun.get<SphereLight<RGB>>() != nullptr);
    CHECK_THAT(sun.get<SphereLight<RGB>>()->radius().to_si(), WithinRel(6.957e8, 1e-6));

    // A light of another kind is named for the base class unless it names itself:
    auto other = scene.add_light(std::make_shared<TestLight>(), "Other");
    CHECK(other.type() == "Light");
    CHECK(scene.get_light("Other").id() == other.id());

    // Lights are scene objects, with identities: they are neither copied nor moved.
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<SphereLight<RGB>>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<SphereLight<RGB>>);
}
