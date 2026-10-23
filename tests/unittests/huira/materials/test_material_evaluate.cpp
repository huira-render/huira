// Material::evaluate(): what it reads from its images for its BSDF, where on them, and how a
// normal map turns the shading frame; and the bsdf_*() calls that shade with what it returns.

#include <array>
#include <cmath>
#include <limits>
#include <numbers>

#include "bsdf_testing.hpp"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::test;
using Catch::Matchers::WithinAbs;

namespace {

/// The material's values at the center of a flat surface.
ShadingParams<RGB> values(const MaterialHandle<RGB>& material)
{
    return material.get()->evaluate(surface()).params;
}

bool near(const Vec3<float>& a, const Vec3<float>& b, float tolerance = 1e-5f)
{
    return glm::length(a - b) < tolerance;
}

bool near(const RGB& a, const RGB& b, float tolerance = 1e-6f)
{
    return std::abs(a[0] - b[0]) < tolerance && std::abs(a[1] - b[1]) < tolerance &&
           std::abs(a[2] - b[2]) < tolerance;
}

/// Checks that a shading frame is orthonormal and right-handed.
void check_frame(const Interaction<RGB>& isect)
{
    CHECK(std::abs(glm::length(isect.normal_s) - 1.f) < 1e-5f);
    CHECK(std::abs(glm::length(isect.tangent) - 1.f) < 1e-5f);
    CHECK(std::abs(glm::dot(isect.tangent, isect.normal_s)) < 1e-5f);
    CHECK(near(isect.bitangent, glm::cross(isect.normal_s, isect.tangent)));
}

/// Surfaces facing up, sideways, down and askew, each with its default tangent frame.
const std::array<Vec3<float>, 4> NORMALS{
    Vec3<float>{0.f, 0.f, 1.f},
    Vec3<float>{-1.f, 0.f, 0.f},
    Vec3<float>{0.f, 0.f, -1.f},
    Vec3<float>{0.26726124f, 0.53452248f, 0.80178373f}, // (1, 2, 3) / sqrt(14)
};

} // namespace

TEST_CASE("A material evaluates the values its BSDF reads, and set_bsdf() changes which",
          "[materials]")
{
    // The others are left at ShadingParams' defaults, without sampling their images.
    const ShadingParams<RGB> defaults;
    Scene<RGB> scene;
    auto material = scene.new_material(scene.new_bsdf_lambertian());
    material.set_albedo(RGB{0.5f, 0.25f, 0.125f});
    material.set_metallic(0.7f);
    material.set_roughness(0.3f);
    material.set_alpha(0.6f);
    material.set_transmission(RGB{0.2f});
    material.set_emissive(RGB{3.f});

    // Opacity, transmission and emission are evaluated for every BSDF:
    const auto check_always_read = [&](const ShadingParams<RGB>& p) {
        CHECK_THAT(p.opacity, WithinAbs(0.6f, 1e-6f));
        CHECK(near(p.transmission, RGB{0.2f}));
        CHECK(p.emission == RGB{3.f});
    };

    SECTION("Lambertian reads the albedo")
    {
        const ShadingParams<RGB> p = values(material);
        CHECK(p.albedo == RGB{0.5f, 0.25f, 0.125f});
        CHECK(p.metallic == defaults.metallic);
        CHECK(p.roughness == defaults.roughness);
        check_always_read(p);
    }
    SECTION("Oren-Nayar reads the albedo and roughness")
    {
        material.set_bsdf(scene.new_bsdf_oren_nayar().get());
        const ShadingParams<RGB> p = values(material);
        CHECK(p.albedo == RGB{0.5f, 0.25f, 0.125f});
        CHECK(p.metallic == defaults.metallic);
        CHECK_THAT(p.roughness, WithinAbs(0.3f, 1e-6f));
        check_always_read(p);
    }
    SECTION("Cook-Torrance reads them all")
    {
        material.set_bsdf(scene.new_bsdf_oren_nayar().get());
        material.set_bsdf(scene.new_bsdf_cook_torrance().get());
        const ShadingParams<RGB> p = values(material);
        CHECK(p.albedo == RGB{0.5f, 0.25f, 0.125f});
        CHECK_THAT(p.metallic, WithinAbs(0.7f, 1e-6f));
        CHECK_THAT(p.roughness, WithinAbs(0.3f, 1e-6f));
        check_always_read(p);
    }
    SECTION("the null BSDF reads none of them, nor the normal map")
    {
        material.set_bsdf(scene.new_bsdf_null().get());
        material.set_normal_image(
            scene.add_texture(Image<Vec3<float>>(1, 1, glm::normalize(Vec3<float>{1, 0, 1}))));
        const MaterialEval<RGB> eval = material.get()->evaluate(surface());
        CHECK(eval.params.albedo == defaults.albedo);
        CHECK(eval.params.metallic == defaults.metallic);
        CHECK(eval.params.roughness == defaults.roughness);
        CHECK(eval.isect.normal_s == surface().normal_s);
        check_always_read(eval.params);
    }
}

TEST_CASE("A material samples its images at the hit's texture coordinates, and its albedo by "
          "the vertex color",
          "[materials]")
{
    // Three pixels in a row, sampled bilinearly: u = 0 is the first, 0.5 the second, and 0.25
    // halfway between them.
    Scene<RGB> scene;
    auto material = scene.new_material(scene.new_bsdf_cook_torrance());

    Image<RGB> albedo(3, 1);
    albedo(0, 0) = RGB{1.f, 0.f, 0.f};
    albedo(1, 0) = RGB{0.f, 1.f, 0.f};
    albedo(2, 0) = RGB{0.f, 0.f, 1.f};
    material.set_albedo_image(scene.add_texture(std::move(albedo)));
    Image<float> ramp(3, 1);
    ramp(0, 0) = 0.f;
    ramp(1, 0) = 0.5f;
    ramp(2, 0) = 1.f;
    material.set_roughness_image(scene.add_texture(Image<float>(ramp)));
    material.set_metallic_image(scene.add_texture(Image<float>(ramp)));
    material.set_alpha_image(scene.add_texture(std::move(ramp)));

    Interaction<RGB> isect = surface();
    isect.vertex_albedo = RGB{0.5f, 1.f, 1.f};
    struct Expected {
        float u;
        RGB albedo; // before the vertex color
        float ramp;
    };
    for (const Expected& e : {Expected{0.f, RGB{1.f, 0.f, 0.f}, 0.f},
                              Expected{0.25f, RGB{0.5f, 0.5f, 0.f}, 0.25f},
                              Expected{0.5f, RGB{0.f, 1.f, 0.f}, 0.5f}}) {
        isect.uv = Vec2<float>{e.u, 0.5f};
        const ShadingParams<RGB> p = material.get()->evaluate(isect).params;
        INFO("u " << e.u);
        CHECK(near(p.albedo, e.albedo * isect.vertex_albedo));
        CHECK_THAT(p.roughness, WithinAbs(e.ramp, 1e-6f));
        CHECK_THAT(p.metallic, WithinAbs(e.ramp, 1e-6f));
        CHECK_THAT(p.opacity, WithinAbs(e.ramp, 1e-6f));
    }

    // and the factors multiply what is sampled:
    material.set_albedo_factor(RGB{0.5f});
    material.set_roughness_factor(0.5f);
    material.set_alpha_factor(0.5f);
    isect.uv = Vec2<float>{0.5f, 0.5f};
    const ShadingParams<RGB> p = material.get()->evaluate(isect).params;
    CHECK(near(p.albedo, RGB{0.f, 0.5f, 0.f}));
    CHECK_THAT(p.roughness, WithinAbs(0.25f, 1e-6f));
    CHECK_THAT(p.opacity, WithinAbs(0.25f, 1e-6f));
}

TEST_CASE("A normal map tilts the shading normal within the surface's tangent frame", "[materials]")
{
    // A tangent-space normal tilted by t toward the tangent, (sin t, 0, cos t), turns the shading
    // normal to T sin t + N cos t, and the tangent with it, to T cos t - N sin t. The normal
    // factor scales the tilt's tangential part, (k sin t, 0, cos t), and 0 leaves the normal flat.
    const float t = 0.4f;
    Scene<RGB> scene;
    auto material = scene.new_material(scene.new_bsdf_lambertian());
    material.set_normal_image(
        scene.add_texture(Image<Vec3<float>>(2, 2, Vec3<float>{std::sin(t), 0.f, std::cos(t)})));

    for (const Vec3<float>& normal : NORMALS) {
        const Interaction<RGB> isect = surface(normal);
        const Vec3<float>& n = isect.normal_s;
        const Vec3<float>& tangent = isect.tangent;
        INFO("surface normal (" << n.x << ", " << n.y << ", " << n.z << ")");

        material.set_normal_factor(1.f);
        MaterialEval<RGB> eval = material.get()->evaluate(isect);
        CHECK(near(eval.isect.normal_s, tangent * std::sin(t) + n * std::cos(t)));
        CHECK(near(eval.isect.tangent, tangent * std::cos(t) - n * std::sin(t)));
        CHECK(eval.isect.normal_g == isect.normal_g);
        check_frame(eval.isect);

        for (float k : {0.5f, 2.f}) {
            material.set_normal_factor(k);
            eval = material.get()->evaluate(isect);
            INFO("normal factor " << k);
            CHECK(near(eval.isect.normal_s,
                       glm::normalize(tangent * (k * std::sin(t)) + n * std::cos(t))));
            check_frame(eval.isect);
        }

        material.set_normal_factor(0.f);
        CHECK(near(material.get()->evaluate(isect).isect.normal_s, n));

        material.set_normal_factor(2.f);
        material.reset_normal();
        CHECK(material.get()->normal_factor() == 1.f);
        eval = material.get()->evaluate(isect);
        CHECK(near(eval.isect.normal_s, n));
        CHECK(near(eval.isect.tangent, tangent));
        material.set_normal_image(scene.add_texture(
            Image<Vec3<float>>(2, 2, Vec3<float>{std::sin(t), 0.f, std::cos(t)})));
    }
}

TEST_CASE("A normal map added as colors is decoded from (n + 1) / 2", "[materials]")
{
    // The same tilt as an 8-bit-style color image: each component of n mapped from [-1, 1] to
    // [0, 1], and normalized after decoding.
    const float t = 0.4f;
    const Vec3<float> tilted{std::sin(t), 0.f, std::cos(t)};
    Scene<RGB> scene;
    auto from_vectors = scene.new_material(scene.new_bsdf_lambertian());
    auto from_colors = scene.new_material(scene.new_bsdf_lambertian());
    const Vec3<float> encoded = (tilted * 0.98f + Vec3<float>{1.f}) * 0.5f; // not unit length
    from_vectors.set_normal_image(scene.add_normal_texture(Image<Vec3<float>>(1, 1, encoded)));
    from_colors.set_normal_image(
        scene.add_normal_texture(Image<RGB>(1, 1, RGB{encoded.x, encoded.y, encoded.z})));

    const Interaction<RGB> isect = surface(NORMALS[3]);
    const Vec3<float> expected = isect.tangent * std::sin(t) + isect.normal_s * std::cos(t);
    CHECK(near(from_vectors.get()->evaluate(isect).isect.normal_s, expected));
    CHECK(near(from_colors.get()->evaluate(isect).isect.normal_s, expected));

    // A pixel of (0.5, 0.5, 0.5) encodes no direction, and is refused, as is one not finite:
    CHECK_THROWS(scene.add_normal_texture(Image<Vec3<float>>(1, 1, Vec3<float>{0.5f})));
    CHECK_THROWS(scene.add_normal_texture(Image<RGB>(1, 1, RGB{0.5f})));
    CHECK_THROWS(scene.add_normal_texture(
        Image<Vec3<float>>(1, 1, Vec3<float>{std::numeric_limits<float>::quiet_NaN()})));
}

TEST_CASE("A material shades with its BSDF, in the frame its normal map gives", "[materials]")
{
    // bsdf_eval(), bsdf_pdf() and bsdf_sample() are the BSDF's, given what evaluate() returned:
    // the material's values, and the normal map's frame. A light 1 rad past the flat normal, on
    // the side the map tilts away from, is behind the tilted one.
    const float t = 0.7f;
    Scene<RGB> scene;
    auto material = scene.new_material(scene.new_bsdf_lambertian());
    material.set_albedo(RGB{0.6f, 0.4f, 0.2f});
    material.set_normal_image(
        scene.add_texture(Image<Vec3<float>>(1, 1, Vec3<float>{std::sin(t), 0.f, std::cos(t)})));

    const LambertianBSDF<RGB> lambertian;
    for (const Vec3<float>& normal : NORMALS) {
        const Interaction<RGB> isect = surface(normal);
        const MaterialEval<RGB> eval = material.get()->evaluate(isect);
        const Vec3<float> wo = direction(isect, 0.2, 0.0);
        INFO("surface normal (" << normal.x << ", " << normal.y << ", " << normal.z << ")");

        for (double phi : {0.0, 1.5, 3.14159}) {
            const Vec3<float> wi = direction(isect, 1.0, phi);
            INFO("light at azimuth " << phi);
            CHECK(material.get()->bsdf_eval(wo, wi, eval) ==
                  lambertian.eval(wo, wi, eval.isect, eval.params));
            const float cos_tilted = glm::dot(wi, eval.isect.normal_s);
            CHECK_THAT(material.get()->bsdf_pdf(wo, wi, eval),
                       WithinAbs(std::max(cos_tilted, 0.f) / std::numbers::pi_v<float>, 1e-6f));
        }
        // Light from behind the tilted normal is not reflected:
        const Vec3<float> behind = direction(isect, 1.0, 3.14159);
        CHECK(material.get()->bsdf_eval(wo, behind, eval) == RGB{0.f});

        const BSDFSample<RGB> s = material.get()->bsdf_sample(wo, eval, 0.3f, 0.8f);
        const BSDFSample<RGB> direct = lambertian.sample(wo, eval.isect, eval.params, 0.3f, 0.8f);
        CHECK(s.wi == direct.wi);
        CHECK(s.value == RGB{0.6f, 0.4f, 0.2f});
        CHECK(glm::dot(s.wi, eval.isect.normal_s) > 0.f);
    }
}
