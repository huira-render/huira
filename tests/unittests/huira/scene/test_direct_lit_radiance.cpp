// SceneView::direct_lit_radiance() estimates the direct light a point reflects by sampling the
// lights alone. Unless its caller traces a ray sampled from the point's BSDF too, which finds part
// of the light, the samples are all there is of it, and are counted in full.

#include <cmath>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;
using Catch::Matchers::WithinRel;

TEST_CASE("direct_lit_radiance() gives all of the direct light a point reflects", "[scene]")
{
    using Radiance = units::SpectralWattsPerMeterSquaredSteradian<RGB>;

    // A white Lambertian sphere, 1 m in radius, under a sphere light 3 m in radius, 5 m from its
    // center, that radiates 1 W/(m^2 sr). The camera is at the sphere's center, so that the view's
    // frame, the camera's, is centered on it too; nothing is rendered.
    Scene<RGB> scene;
    scene.root.new_instance(scene.add_primitive(scene.add_ellipsoid(1_m, 1_m, 1_m),
                                                scene.new_material(scene.new_bsdf_lambertian())));
    scene.root.new_instance(scene.new_sphere_light(3_m, Radiance(RGB{1.f})))
        .set_position(0_m, 5_m, 0_m);
    auto camera = scene.root.new_instance(scene.new_camera_model());
    SceneView<RGB> view(scene,
                        Interval{Time::from_et(0.0), Time::from_et(1e-3)},
                        camera,
                        ObservationMode::GEOMETRIC_STATE);

    // A ray from the light's center to the sphere's meets it where the light is straight
    // overhead: a sphere of radius R at distance d, wholly above the horizon, gives an irradiance
    // of pi L (R / d)^2 there, which a white Lambertian surface reflects as a radiance of
    // L (R / d)^2: (3 / 4)^2.
    const Vec3<float> light_center =
        instance_transform_at(view.lights().front().transforms, 0.5f).position;
    const Ray<RGB> ray(light_center, glm::normalize(-light_center));
    constexpr unsigned int GEOMETRY_ONLY = 0x01; // Not the light's sphere, which the ray starts in
    const HitRecord hit = view.intersect(ray, 0.5f, GEOMETRY_ONLY);
    REQUIRE(hit.hit());
    const float expected = (3.f / 4.f) * (3.f / 4.f);

    constexpr int SAMPLES = 20000;
    RandomSampler<float> sampler(119);
    double full = 0.0;
    double weighted = 0.0;
    for (int i = 0; i < SAMPLES; ++i) {
        full += view.direct_lit_radiance(ray, hit, sampler)[1];
        weighted += view.direct_lit_radiance(ray, hit, sampler, 0.5f, {}, true)[1];
    }
    full /= SAMPLES;
    weighted /= SAMPLES;

    // By default, all of it. Before, the lights' samples were weighted against a BSDF ray that
    // nothing traced, and gave 0.42 of the 0.5625.
    CHECK_THAT(full, WithinRel(static_cast<double>(expected), 0.01));

    // With a BSDF ray to follow, the lights' share of it: here the BSDF's is a quarter.
    CHECK(weighted < 0.8 * full);
}
