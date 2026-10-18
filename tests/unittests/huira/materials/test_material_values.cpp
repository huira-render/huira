#include <cmath>
#include <limits>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include "huira/huira.hpp"

using namespace huira;
using Catch::Matchers::WithinRel;

namespace {

/// A surface facing +z, at the texture coordinate (0.5, 0.5).
Interaction<RGB> flat_surface()
{
    Interaction<RGB> isect;
    isect.normal_g = Vec3<float>{0.f, 0.f, 1.f};
    isect.normal_s = isect.normal_g;
    build_default_tangent_frame(isect.normal_s, isect.tangent, isect.bitangent);
    isect.uv = Vec2<float>{0.5f, 0.5f};
    return isect;
}

/// The material's values at the center of a flat surface.
ShadingParams<RGB> values(const MaterialHandle<RGB>& material)
{
    return material.get()->evaluate(flat_surface()).params;
}

constexpr float NaN_F = std::numeric_limits<float>::quiet_NaN();

} // namespace

TEST_CASE("A material starts with its default values and factors of 1", "[materials]")
{
    Scene<RGB> scene;
    auto material = scene.new_material(scene.new_bsdf_cook_torrance());
    const ShadingParams<RGB> p = values(material);

    CHECK(p.albedo == RGB{1.f});
    CHECK(p.opacity == 1.f);
    CHECK(p.metallic == 0.f);
    CHECK(p.roughness == 1.f);
    CHECK(p.transmission == RGB{0.f});
    CHECK(p.emission == RGB{0.f});

    const Material<RGB>& m = *material.get();
    CHECK(m.albedo_factor() == RGB{1.f});
    CHECK(m.alpha_factor() == 1.f);
    CHECK(m.metallic_factor() == 1.f);
    CHECK(m.roughness_factor() == 1.f);
    CHECK(m.normal_factor() == 1.f);
    CHECK(m.transmission_factor() == RGB{1.f});
    CHECK(m.emissive_factor() == RGB{1.f});
}

TEST_CASE("A constant, an image and a factor each set one thing", "[materials]")
{
    Scene<RGB> scene;
    auto material = scene.new_material(scene.new_bsdf_cook_torrance());
    const Material<RGB>& m = *material.get();

    SECTION("a constant alone is the value")
    {
        material.set_metallic(0.3f);
        material.set_emissive(RGB{5.f, 6.f, 7.f});
        material.set_transmission(RGB{0.25f});
        material.set_albedo(RGB{0.5f, 0.25f, 0.125f});
        material.set_alpha(0.75f);
        material.set_roughness(0.4f);
        const ShadingParams<RGB> p = values(material);
        CHECK_THAT(p.metallic, WithinRel(0.3f, 1e-6f));
        CHECK(p.emission == RGB{5.f, 6.f, 7.f});
        CHECK(p.transmission == RGB{0.25f});
        CHECK(p.albedo == RGB{0.5f, 0.25f, 0.125f});
        CHECK_THAT(p.opacity, WithinRel(0.75f, 1e-6f));
        CHECK_THAT(p.roughness, WithinRel(0.4f, 1e-6f));
        // and no factor moved:
        CHECK(m.metallic_factor() == 1.f);
        CHECK(m.emissive_factor() == RGB{1.f});
        CHECK(m.transmission_factor() == RGB{1.f});
    }

    SECTION("an image alone shows as it is")
    {
        material.set_emissive_image(scene.add_texture(Image<RGB>(2, 2, RGB{3.f})));
        material.set_metallic_image(scene.add_texture(Image<float>(2, 2, 0.8f)));
        material.set_transmission_image(scene.add_texture(Image<RGB>(2, 2, RGB{0.5f})));
        const ShadingParams<RGB> p = values(material);
        CHECK(p.emission == RGB{3.f});
        CHECK_THAT(p.metallic, WithinRel(0.8f, 1e-6f));
        CHECK(p.transmission == RGB{0.5f});
        CHECK(m.emissive_factor() == RGB{1.f});
        CHECK(m.metallic_factor() == 1.f);
    }

    SECTION("a factor multiplies whichever image is set, and is kept when the image changes")
    {
        material.set_emissive_factor(RGB{0.f});
        material.set_emissive_image(scene.add_texture(Image<RGB>(2, 2, RGB{3.f})));
        CHECK(values(material).emission == RGB{0.f}); // the user said 0, so 0
        CHECK(m.emissive_factor() == RGB{0.f});

        material.set_emissive_factor(RGB{2.f});
        CHECK(values(material).emission == RGB{6.f});

        material.set_emissive(RGB{1.f}); // a constant under the same factor
        CHECK(values(material).emission == RGB{2.f});

        material.set_metallic_image(scene.add_texture(Image<float>(2, 2, 0.5f)));
        material.set_metallic_factor(0.5f);
        CHECK_THAT(values(material).metallic, WithinRel(0.25f, 1e-6f));
    }

    SECTION("reset restores the default value and a factor of 1")
    {
        material.set_emissive_image(scene.add_texture(Image<RGB>(2, 2, RGB{3.f})));
        material.set_emissive_factor(RGB{2.f});
        material.set_metallic(0.9f);
        material.set_metallic_factor(0.5f);
        material.set_albedo(RGB{0.2f});
        material.set_albedo_factor(RGB{0.5f});
        material.reset_emissive();
        material.reset_metallic();
        material.reset_albedo();
        const ShadingParams<RGB> p = values(material);
        CHECK(p.emission == RGB{0.f});
        CHECK(p.metallic == 0.f);
        CHECK(p.albedo == RGB{1.f});
        CHECK(m.emissive_factor() == RGB{1.f});
        CHECK(m.metallic_factor() == 1.f);
        CHECK(m.albedo_factor() == RGB{1.f});
    }
}

TEST_CASE("Material setters validate their arguments", "[materials]")
{
    Scene<RGB> scene;
    auto material = scene.new_material(scene.new_bsdf_cook_torrance());

    // Reflectances and fractions are from 0 to 1, as constants and as factors:
    CHECK_THROWS(material.set_albedo(RGB{1.5f}));
    CHECK_THROWS(material.set_albedo(RGB{-0.1f}));
    CHECK_THROWS(material.set_albedo_factor(RGB{1.5f}));
    CHECK_THROWS(material.set_albedo_factor(RGB{NaN_F}));
    CHECK_THROWS(material.set_alpha(1.5f));
    CHECK_THROWS(material.set_alpha_factor(-0.1f));
    CHECK_THROWS(material.set_metallic(1.5f));
    CHECK_THROWS(material.set_metallic_factor(NaN_F));
    CHECK_THROWS(material.set_roughness(-0.1f));
    CHECK_THROWS(material.set_roughness_factor(2.f));
    CHECK_THROWS(material.set_transmission(RGB{2.f}));
    CHECK_THROWS(material.set_transmission_factor(RGB{-1.f}));
    CHECK_NOTHROW(material.set_albedo(RGB{1.f}));
    CHECK_NOTHROW(material.set_albedo(RGB{0.f}));

    // A radiance is finite and non-negative, with no upper bound:
    CHECK_THROWS(material.set_emissive(RGB{-1.f}));
    CHECK_THROWS(material.set_emissive(RGB{NaN_F}));
    CHECK_THROWS(material.set_emissive_factor(RGB{std::numeric_limits<float>::infinity()}));
    CHECK_NOTHROW(material.set_emissive(RGB{1e6f}));
    CHECK_NOTHROW(material.set_emissive_factor(RGB{1e6f}));

    // The normal map's strength is any finite number:
    CHECK_THROWS(material.set_normal_factor(NaN_F));
    CHECK_NOTHROW(material.set_normal_factor(2.f));
    CHECK_NOTHROW(material.set_normal_factor(0.f));
}
