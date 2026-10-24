#include <cmath>
#include <limits>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include "huira/huira.hpp"

using namespace huira;
using Catch::Matchers::WithinRel;

namespace {

/// A surface facing +z, at the texture coordinate (0.5, 0.5), seen from overhead.
Interaction<RGB> flat_surface()
{
    Interaction<RGB> isect{};
    isect.normal_g = Vec3<float>{0.f, 0.f, 1.f};
    isect.normal_s = isect.normal_g;
    build_default_tangent_frame(isect.normal_s, isect.tangent, isect.bitangent);
    isect.uv = Vec2<float>{0.5f, 0.5f};
    isect.wo = isect.normal_g;
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

TEST_CASE("A normal map's sample of no length leaves the surface's normal", "[materials][nan]")
{
    // It encodes no direction, and was normalized into NaN, which every pixel it shaded became:
    // a pixel of 0 (a texture made by add_texture(), not add_normal_texture(), may have one), or
    // one in the tangent plane with a factor of 0.
    Scene<RGB> scene;
    auto material = scene.new_material(scene.new_bsdf_lambertian());
    const Vec3<float> up{0.f, 0.f, 1.f};

    material.set_normal_image(scene.add_texture(Image<Vec3<float>>(2, 2, Vec3<float>{0.f})));
    CHECK(material.get()->evaluate(flat_surface()).isect.normal_s == up);

    material.set_normal_image(
        scene.add_texture(Image<Vec3<float>>(2, 2, Vec3<float>{1.f, 0.f, 0.f})));
    material.set_normal_factor(0.f);
    CHECK(material.get()->evaluate(flat_surface()).isect.normal_s == up);

    // ...and a tilted pixel still tilts it (a guard; with the viewer overhead, Material::evaluate()
    // keeps it from tilting past about 45 degrees, where the view would reflect into the surface):
    material.set_normal_factor(1.f);
    const Vec3<float> tilted = material.get()->evaluate(flat_surface()).isect.normal_s;
    CHECK(std::abs(glm::length(tilted) - 1.f) < 1e-6f);
    CHECK(tilted.z < 0.9f);
}

TEST_CASE("Every image slot takes a texture, and reset restores its default", "[materials]")
{
    Scene<RGB> scene;
    auto material = scene.new_material(scene.new_bsdf_cook_torrance());
    const Material<RGB>& m = *material.get();

    material.set_albedo_image(scene.add_texture(Image<RGB>(2, 2, RGB{0.25f})));
    material.set_alpha_image(scene.add_texture(Image<float>(2, 2, 0.5f)));
    material.set_roughness_image(scene.add_texture(Image<float>(2, 2, 0.75f)));
    material.set_transmission_image(scene.add_texture(Image<RGB>(2, 2, RGB{0.125f})));
    material.set_albedo_factor(RGB{0.5f});
    material.set_alpha_factor(0.5f);
    material.set_roughness_factor(0.5f);
    material.set_transmission_factor(RGB{0.5f});
    ShadingParams<RGB> p = values(material);
    CHECK(p.albedo == RGB{0.125f});
    CHECK_THAT(p.opacity, WithinRel(0.25f, 1e-6f));
    CHECK_THAT(p.roughness, WithinRel(0.375f, 1e-6f));
    CHECK(p.transmission == RGB{0.0625f});

    material.reset_albedo();
    material.reset_alpha();
    material.reset_roughness();
    material.reset_transmission();
    p = values(material);
    CHECK(p.albedo == RGB{1.f});
    CHECK(p.opacity == 1.f);
    CHECK(p.roughness == 1.f);
    CHECK(p.transmission == RGB{0.f});
    CHECK(m.albedo_factor() == RGB{1.f});
    CHECK(m.alpha_factor() == 1.f);
    CHECK(m.roughness_factor() == 1.f);
    CHECK(m.transmission_factor() == RGB{1.f});
}

TEST_CASE("A material knows whether its alpha or its transmission can let light through",
          "[materials]")
{
    // has_alpha() is false only where every evaluation gives an opacity of exactly 1, and
    // has_transmission() only where it gives a transmission of exactly 0: a shadow ray stops
    // at such a surface without evaluating it.
    Scene<RGB> scene;
    auto material = scene.new_material(scene.new_bsdf_lambertian());
    const Material<RGB>& m = *material.get();
    CHECK(!m.has_alpha());
    CHECK(!m.has_alpha_texture());
    CHECK(!m.has_transmission());

    SECTION("alpha")
    {
        material.set_alpha_factor(0.5f);
        CHECK(m.has_alpha());
        CHECK(!m.has_alpha_texture());
        material.set_alpha_factor(1.f);
        CHECK(!m.has_alpha());

        material.set_alpha(0.25f);
        CHECK(m.has_alpha());
        material.set_alpha(1.f); // a constant 1 is opaque, as the default is
        CHECK(!m.has_alpha());
        CHECK(values(material).opacity == 1.f);
        material.reset_alpha();
        CHECK(!m.has_alpha());

        material.set_alpha_image(scene.add_texture(Image<float>(1, 1, 1.f)));
        CHECK(!m.has_alpha());
        CHECK(m.has_alpha_texture());
        // A larger image may interpolate to just under 1, even if all its pixels are 1:
        material.set_alpha_image(scene.add_texture(Image<float>(2, 2, 1.f)));
        CHECK(m.has_alpha());
        material.set_alpha_image(scene.add_texture(Image<float>(2, 2, 0.5f)));
        CHECK(m.has_alpha());
        CHECK_THAT(values(material).opacity, WithinRel(0.5f, 1e-6f));
        material.reset_alpha();
        CHECK(!m.has_alpha());
        CHECK(!m.has_alpha_texture());
    }

    SECTION("transmission")
    {
        material.set_transmission(RGB{0.2f});
        CHECK(m.has_transmission());
        material.set_transmission_factor(RGB{0.f}); // no light through, whatever the image
        CHECK(!m.has_transmission());
        CHECK(values(material).transmission == RGB{0.f});
        material.set_transmission_factor(RGB{1.f});
        material.set_transmission(RGB{0.f});
        CHECK(!m.has_transmission());
        material.set_transmission(RGB{0.f, 0.1f, 0.f});
        CHECK(m.has_transmission());
        // An image counts as transmitting unless it is a constant:
        material.set_transmission_image(scene.add_texture(Image<RGB>(2, 2, RGB{0.f})));
        CHECK(m.has_transmission());
        CHECK(values(material).transmission == RGB{0.f});
        material.reset_transmission();
        CHECK(!m.has_transmission());
    }
}

TEST_CASE("A material refuses a null BSDF and a null or empty image", "[materials]")
{
    Scene<RGB> scene;
    auto material = scene.new_material(scene.new_bsdf_lambertian());
    Material<RGB>& m = *material.get();

    CHECK_THROWS(material.set_bsdf(nullptr));
    CHECK_THROWS(m.set_bsdf(nullptr));

    // Sampling a null or empty image would read past it:
    CHECK_THROWS(m.set_albedo_image(nullptr));
    CHECK_THROWS(m.set_alpha_image(nullptr));
    CHECK_THROWS(m.set_metallic_image(nullptr));
    CHECK_THROWS(m.set_roughness_image(nullptr));
    CHECK_THROWS(m.set_normal_image(nullptr));
    CHECK_THROWS(m.set_transmission_image(nullptr));
    CHECK_THROWS(m.set_emissive_image(nullptr));
    CHECK_THROWS(m.set_albedo_image(std::make_shared<Image<RGB>>()));
    CHECK_THROWS(m.set_roughness_image(std::make_shared<Image<float>>(0, 4)));
    CHECK_THROWS(m.set_normal_image(std::make_shared<Image<Vec3<float>>>(4, 0)));
    // and what was set before is kept:
    CHECK(values(material).albedo == RGB{1.f});

    const auto white = std::make_shared<Image<RGB>>(1, 1, RGB{1.f});
    const auto one = std::make_shared<Image<float>>(1, 1, 1.f);
    const auto flat = std::make_shared<Image<Vec3<float>>>(1, 1, Vec3<float>{0.f, 0.f, 1.f});
    CHECK_THROWS(Material<RGB>(nullptr, white, one, one, one, flat, white, white));
    CHECK_NOTHROW(
        Material<RGB>(std::make_shared<NullBSDF<RGB>>(), white, one, one, one, flat, white, white));
}

TEST_CASE("A texture refuses an image without pixels, or with a pixel not finite or a negative "
          "spectrum",
          "[materials]")
{
    // Textures are sampled without checks, so one bad pixel would spread NaN over the frame.
    CHECK_THROWS(Texture<RGB>(Image<RGB>()));
    CHECK_THROWS(Texture<float>(Image<float>(3, 0)));

    Image<float> mono(2, 2, 0.5f);
    mono(1, 1) = NaN_F;
    CHECK_THROWS(Texture<float>(std::move(mono)));
    Image<RGB> spectral(2, 2, RGB{0.5f});
    spectral(0, 1) = RGB{0.5f, -0.1f, 0.5f};
    CHECK_THROWS(Texture<RGB>(std::move(spectral)));
    Image<Vec3<float>> vectors(2, 2, Vec3<float>{0.f, 0.f, 1.f});
    vectors(1, 0) = Vec3<float>{0.f, std::numeric_limits<float>::infinity(), 1.f};
    CHECK_THROWS(Texture<Vec3<float>>(std::move(vectors)));

    // A constant is a 1 x 1 image, checked the same way:
    CHECK_THROWS(Texture<float>(NaN_F));
    CHECK_THROWS(Texture<RGB>(RGB{-1.f}));
    CHECK_THROWS(Texture<Vec3<float>>(Vec3<float>{NaN_F}));
    const Texture<float> constant(0.25f);
    CHECK(constant.resolution().width == 1);
    CHECK(constant.resolution().height == 1);
    CHECK((*constant.shared_image())(0, 0) == 0.25f);
    CHECK(constant.type() == "Texture");

    // Negative values are fine where they are not light: a vector's components, or a float.
    CHECK_NOTHROW(Texture<Vec3<float>>(Vec3<float>{-1.f, 0.f, 0.f}));
    CHECK_NOTHROW(Texture<float>(-2.f));

    // Through the scene, a texture keeps its image's size:
    Scene<RGB> scene;
    const TextureHandle<RGB> texture = scene.add_texture(Image<RGB>(4, 3, RGB{0.5f}));
    CHECK(texture.resolution().width == 4);
    CHECK(texture.resolution().height == 3);
    CHECK_THROWS(scene.add_texture(Image<float>(2, 2, NaN_F)));
}
