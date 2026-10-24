// What BSDF (bsdf.hpp) promises of every BSDF: eval() and pdf() are zero below the surface,
// sample() draws directions with the density pdf() gives them and returns f |cos(theta_i)| / pdf,
// the pdf integrates to the probability that sample() returns a direction, the BSDF is reciprocal,
// depends only on directions relative to its surface, stays finite, and reads only the shading
// parameters its requirements() name. Each BSDF is checked at parameters smooth enough to
// integrate by quadrature; test_diffuse_bsdfs.cpp, test_planetary_bsdfs.cpp and
// test_cook_torrance.cpp check each model's values.

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "bsdf_testing.hpp"
#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::test;

namespace {

/// A BSDF, the shading parameters to check it with, what its requirements() should be, and
/// whether at an albedo of 1 it reflects no more light than it receives.
struct Case {
    std::string name;
    std::shared_ptr<BSDF<RGB>> bsdf;
    ShadingParams<RGB> params;
    BSDFRequirements requirements;
    bool conserves_energy = true;
};

ShadingParams<RGB> shading(RGB albedo, float roughness, float metallic)
{
    ShadingParams<RGB> params;
    params.albedo = albedo;
    params.roughness = roughness;
    params.metallic = metallic;
    return params;
}

/// Every BSDF that reflects, with an albedo that differs between channels.
std::vector<Case> cases()
{
    const RGB albedo{0.8f, 0.5f, 0.2f};
    const auto oren_nayar = std::make_shared<OrenNayarBSDF<RGB>>();
    const auto cook_torrance = std::make_shared<CookTorranceBSDF<RGB>>();
    // A narrow opposition surge and a phase function that favors backscattering:
    const auto hapke = std::make_shared<HapkeBSDF<RGB>>(0.06f, 1.f, 0.3f, 0.6f);
    return {
        {"Lambertian",
         std::make_shared<LambertianBSDF<RGB>>(),
         shading(albedo, 0.5f, 0.f),
         {true, true, false, false}},
        {"Oren-Nayar, smooth", oren_nayar, shading(albedo, 0.f, 0.f), {true, true, false, true}},
        {"Oren-Nayar, rough", oren_nayar, shading(albedo, 1.f, 0.f), {true, true, false, true}},
        {"Lommel-Seeliger",
         std::make_shared<LommelSeeligerBSDF<RGB>>(),
         shading(albedo, 0.5f, 0.f),
         {true, true, false, false}},
        // McEwen's lunar-Lambert function is a fit of I/F, not a physical model: at an albedo
        // of 1 it reflects 9% more light than it receives overhead, and 64% more at 1.5 rad.
        {"McEwen",
         std::make_shared<McEwenBSDF<RGB>>(),
         shading(albedo, 0.5f, 0.f),
         {true, true, false, false},
         false},
        // Hapke's model does not conserve energy: its opposition surge adds light near zero
        // phase, and its approximate H function overestimates multiple scattering. With a single
        // scattering albedo of 1 it reflects up to 10% more light than it receives here.
        {"Hapke", hapke, shading(albedo, 0.5f, 0.f), {true, true, false, false}, false},
        {"Cook-Torrance, dielectric",
         cook_torrance,
         shading(albedo, 0.5f, 0.f),
         {true, true, true, true}},
        {"Cook-Torrance, metal",
         cook_torrance,
         shading(albedo, 0.3f, 1.f),
         {true, true, true, true}},
    };
}

/// Directions toward the viewer, from overhead to grazing, at azimuths that differ from the
/// tangent's so that a BSDF that mistook the frame would show it.
std::vector<Vec3<float>> viewers(const Interaction<RGB>& isect)
{
    return {direction(isect, 0.0, 0.0),
            direction(isect, 0.5, 0.3),
            direction(isect, 1.0, 2.0),
            direction(isect, 1.4, -2.5)};
}

bool close(float a, float b, float relative)
{
    return std::abs(a - b) <= relative * std::max(std::abs(a), std::abs(b)) + 1e-12f;
}

bool close(const RGB& a, const RGB& b, float relative)
{
    return close(a[0], b[0], relative) && close(a[1], b[1], relative) &&
           close(a[2], b[2], relative);
}

/// A rotation of the whole scene: the surface with the normal (1, 2, 3) / sqrt(14), and the map
/// that takes a direction relative to the flat surface (normal +z) to the same direction relative
/// to it.
struct Rotated {
    Interaction<RGB> isect = surface(glm::normalize(Vec3<float>{1.f, 2.f, 3.f}));
    Vec3<float> operator()(const Vec3<float>& w) const
    {
        return sampling::local_to_world(w, isect.tangent, isect.bitangent, isect.normal_s);
    }
};

} // namespace

TEST_CASE("Each BSDF names the shading parameters it reads", "[materials][bsdf]")
{
    for (const Case& c : cases()) {
        INFO(c.name);
        const BSDFRequirements r = c.bsdf->requirements();
        CHECK(r.needs_albedo == c.requirements.needs_albedo);
        CHECK(r.needs_normal == c.requirements.needs_normal);
        CHECK(r.needs_metallic == c.requirements.needs_metallic);
        CHECK(r.needs_roughness == c.requirements.needs_roughness);
    }
    CHECK(LambertianBSDF<RGB>{}.type() == "LambertianBSDF");
    CHECK(OrenNayarBSDF<RGB>{}.type() == "OrenNayarBSDF");
    CHECK(LommelSeeligerBSDF<RGB>{}.type() == "LommelSeeligerBSDF");
    CHECK(McEwenBSDF<RGB>{}.type() == "McEwenBSDF");
    CHECK(HapkeBSDF<RGB>{0.06f, 1.f, 0.3f, 0.6f}.type() == "HapkeBSDF");
    CHECK(CookTorranceBSDF<RGB>{}.type() == "CookTorranceBSDF");
    CHECK(NullBSDF<RGB>{}.type() == "NullBSDF");
}

TEST_CASE("A BSDF does not change with the shading parameters it does not name",
          "[materials][bsdf]")
{
    // Material::evaluate() leaves the parameters a BSDF does not name at their defaults, so a
    // BSDF that read one anyway would not see the material's value.
    const Interaction<RGB> isect = surface();
    for (const Case& c : cases()) {
        ShadingParams<RGB> other = c.params;
        if (!c.requirements.needs_metallic) {
            other.metallic = 1.f - c.params.metallic;
        }
        if (!c.requirements.needs_roughness) {
            other.roughness = 0.9f - c.params.roughness;
        }
        other.opacity = 0.3f;
        other.transmission = RGB{0.4f};
        other.emission = RGB{7.f};
        for (const Vec3<float>& wo : viewers(isect)) {
            for (const Vec3<float>& wi : viewers(isect)) {
                INFO(c.name);
                CHECK(c.bsdf->eval(wo, wi, isect, c.params) == c.bsdf->eval(wo, wi, isect, other));
                CHECK(c.bsdf->pdf(wo, wi, isect, c.params) == c.bsdf->pdf(wo, wi, isect, other));
            }
            const BSDFSample<RGB> a = c.bsdf->sample(wo, isect, c.params, 0.3f, 0.6f);
            const BSDFSample<RGB> b = c.bsdf->sample(wo, isect, other, 0.3f, 0.6f);
            CHECK(a.value == b.value);
            CHECK(a.pdf == b.pdf);
        }
    }
}

TEST_CASE("A BSDF reflects no light from below its surface", "[materials][bsdf]")
{
    const Interaction<RGB> isect = surface();
    for (const Case& c : cases()) {
        for (const Vec3<float>& wo : viewers(isect)) {
            for (double theta : {1.6, 2.2, 3.1}) {
                const Vec3<float> wi = direction(isect, theta, 1.0);
                INFO(c.name << ", wi " << theta << " rad from the normal");
                CHECK(c.bsdf->eval(wo, wi, isect, c.params) == RGB{0.f});
                CHECK(c.bsdf->pdf(wo, wi, isect, c.params) == 0.f);
            }
        }
    }
}

TEST_CASE("A BSDF reflects nothing toward a viewer below its surface", "[materials][bsdf]")
{
    // Material::evaluate() keeps the viewer above the shading normal; a BSDF used without it
    // gives no light to a viewer below, as there is none to give.
    const Interaction<RGB> isect = surface();
    for (const Case& c : cases()) {
        for (double theta : {1.6, 2.2, 3.1}) {
            const Vec3<float> wo = direction(isect, theta, 1.0);
            INFO(c.name << ", wo " << theta << " rad from the normal");
            for (const Vec3<float>& wi : viewers(isect)) {
                CHECK(c.bsdf->eval(wo, wi, isect, c.params) == RGB{0.f});
                CHECK(c.bsdf->pdf(wo, wi, isect, c.params) == 0.f);
            }
            for (float u : {0.1f, 0.5f, 0.9f}) {
                const BSDFSample<RGB> s = c.bsdf->sample(wo, isect, c.params, u, 1.f - u);
                CHECK(!s.is_valid());
                CHECK(s.value == RGB{0.f});
            }
        }
    }
}

TEST_CASE("A BSDF's samples are directions above the surface, with the values and pdfs that "
          "eval() and pdf() give them",
          "[materials][bsdf]")
{
    const Interaction<RGB> isect = surface();
    constexpr int N = 24;
    for (const Case& c : cases()) {
        for (const Vec3<float>& wo : viewers(isect)) {
            int valid = 0;
            for (int i = 0; i < N; ++i) {
                for (int j = 0; j < N; ++j) {
                    const float u1 = (static_cast<float>(i) + 0.5f) / N;
                    const float u2 = (static_cast<float>(j) + 0.5f) / N;
                    const BSDFSample<RGB> s = c.bsdf->sample(wo, isect, c.params, u1, u2);
                    INFO(c.name << ", wo.z " << wo.z << ", u1 " << u1 << ", u2 " << u2);
                    REQUIRE(finite(s.value));
                    REQUIRE(std::isfinite(s.pdf));
                    if (!s.is_valid()) {
                        continue;
                    }
                    ++valid;
                    const float cos_i = glm::dot(s.wi, isect.normal_s);
                    CHECK(std::abs(glm::length(s.wi) - 1.f) < 1e-5f);
                    CHECK(cos_i > 0.f);
                    CHECK(!s.is_delta);
                    CHECK(close(s.pdf, c.bsdf->pdf(wo, s.wi, isect, c.params), 1e-4f));
                    const RGB weighted = c.bsdf->eval(wo, s.wi, isect, c.params) * (cos_i / s.pdf);
                    CHECK(close(s.value, weighted, 1e-4f));
                }
            }
            INFO(c.name << ", wo.z " << wo.z);
            CHECK(valid > N * N / 2);
        }
    }
}

TEST_CASE("A BSDF's pdf integrates to the probability that it samples a direction",
          "[materials][bsdf]")
{
    // 1 for a BSDF that always returns a direction. A specular lobe's microfacet normals can
    // reflect a direction below the surface, which sample() rejects (Cook-Torrance's, as many as
    // 3% of them here), so the pdf of those that remain integrates to less than 1.
    const Interaction<RGB> isect = surface();
    for (const Case& c : cases()) {
        for (const Vec3<float>& wo : viewers(isect)) {
            const double total = total_pdf(*c.bsdf, isect, c.params, wo);
            const double valid = valid_fraction(*c.bsdf, isect, c.params, wo, 256);
            INFO(c.name << ", wo.z " << wo.z << ": the pdf integrates to " << total << ", and "
                        << valid << " of the samples are directions");
            CHECK(std::abs(total - valid) < 5e-3);
            CHECK(total <= 1.0 + 1e-3);
        }
    }
}

TEST_CASE("A BSDF reflects no more light than it receives", "[materials][bsdf]")
{
    const Interaction<RGB> isect = surface();
    for (Case c : cases()) {
        if (!c.conserves_energy) {
            continue;
        }
        c.params.albedo = RGB{1.f};
        for (double theta : {0.0, 0.5, 1.0, 1.3, 1.5}) {
            const Vec3<float> wo = direction(isect, theta, 0.3);
            const double light = reflected(*c.bsdf, isect, c.params, wo);
            INFO(c.name << ", viewer at " << theta << " rad: reflects " << light);
            CHECK(light <= 1.0 + 1e-3);
        }
    }
}

TEST_CASE("A BSDF's samples estimate the light it reflects without bias", "[materials][bsdf]")
{
    // The mean of sample().value over evenly spread (u1, u2) is the light the BSDF reflects only
    // if its directions are drawn with the density its pdf gives. The integral is by quadrature
    // of eval(), which does not depend on sample() or pdf().
    const Interaction<RGB> isect = surface();
    for (const Case& c : cases()) {
        for (const Vec3<float>& wo : viewers(isect)) {
            const double exact = reflected(*c.bsdf, isect, c.params, wo);
            const double estimate = mean_sample_value(*c.bsdf, isect, c.params, wo, 64);
            INFO(c.name << ", wo.z " << wo.z << ": reflects " << exact << ", samples estimate "
                        << estimate);
            CHECK(std::abs(estimate - exact) <= 0.01 * exact);
        }
    }
}

TEST_CASE("A BSDF is reciprocal", "[materials][bsdf]")
{
    // f(wo, wi) = f(wi, wo): light retraces its path.
    const Interaction<RGB> isect = surface();
    for (const Case& c : cases()) {
        for (const Vec3<float>& a : viewers(isect)) {
            for (const Vec3<float>& b : viewers(isect)) {
                INFO(c.name << ", a.z " << a.z << ", b.z " << b.z);
                CHECK(close(c.bsdf->eval(a, b, isect, c.params),
                            c.bsdf->eval(b, a, isect, c.params),
                            1e-5f));
            }
        }
    }
}

TEST_CASE("A BSDF depends only on directions relative to its surface", "[materials][bsdf]")
{
    const Interaction<RGB> flat = surface();
    const Rotated rotate;

    // The same surface, its tangent frame turned 1 rad about the normal: these BSDFs are
    // isotropic, so their values do not depend on it.
    Interaction<RGB> turned = flat;
    turned.tangent = std::cos(1.f) * flat.tangent + std::sin(1.f) * flat.bitangent;
    turned.bitangent = glm::cross(turned.normal_s, turned.tangent);

    for (const Case& c : cases()) {
        for (const Vec3<float>& wo : viewers(flat)) {
            for (const Vec3<float>& wi : viewers(flat)) {
                INFO(c.name << ", wo.z " << wo.z << ", wi.z " << wi.z);
                const RGB f = c.bsdf->eval(wo, wi, flat, c.params);
                const float p = c.bsdf->pdf(wo, wi, flat, c.params);
                CHECK(
                    close(c.bsdf->eval(rotate(wo), rotate(wi), rotate.isect, c.params), f, 1e-4f));
                CHECK(close(c.bsdf->pdf(rotate(wo), rotate(wi), rotate.isect, c.params), p, 1e-4f));
                CHECK(close(c.bsdf->eval(wo, wi, turned, c.params), f, 1e-5f));
                CHECK(close(c.bsdf->pdf(wo, wi, turned, c.params), p, 1e-5f));
            }
            // A sample drawn on the rotated surface is the rotated sample:
            const BSDFSample<RGB> s = c.bsdf->sample(wo, flat, c.params, 0.3f, 0.6f);
            const BSDFSample<RGB> r =
                c.bsdf->sample(rotate(wo), rotate.isect, c.params, 0.3f, 0.6f);
            INFO(c.name << ", wo.z " << wo.z);
            REQUIRE(s.is_valid());
            REQUIRE(r.is_valid());
            CHECK(glm::length(r.wi - rotate(s.wi)) < 1e-4f);
            CHECK(close(r.value, s.value, 1e-4f));
            CHECK(close(r.pdf, s.pdf, 1e-4f));
        }
    }
}

TEST_CASE("A BSDF stays finite at grazing angles and at its parameters' limits",
          "[materials][bsdf]")
{
    const Interaction<RGB> isect = surface();
    // The cosines run down to 1e-6, and the directions include the normal, the viewer's own
    // direction (zero phase) and its mirror image:
    const double grazing = std::acos(1e-6);
    std::vector<double> angles{0.0, 0.7, 1.5, std::acos(1e-3), grazing};
    for (Case c : cases()) {
        for (float albedo : {0.f, 1.f}) {
            for (float roughness : {0.f, 1.f}) {
                c.params.albedo = RGB{albedo};
                if (c.requirements.needs_roughness) {
                    c.params.roughness = roughness;
                }
                for (double theta_o : angles) {
                    const Vec3<float> wo = direction(isect, theta_o, 0.4);
                    std::vector<Vec3<float>> wis{wo, mirror(isect, wo)};
                    for (double theta_i : angles) {
                        wis.push_back(direction(isect, theta_i, 2.9));
                    }
                    for (const Vec3<float>& wi : wis) {
                        INFO(c.name << ", albedo " << albedo << ", roughness " << roughness
                                    << ", wo.z " << wo.z << ", wi (" << wi.x << ", " << wi.y << ", "
                                    << wi.z << ")");
                        const RGB f = c.bsdf->eval(wo, wi, isect, c.params);
                        const float p = c.bsdf->pdf(wo, wi, isect, c.params);
                        CHECK(finite(f));
                        CHECK(f.min() >= 0.f);
                        CHECK(std::isfinite(p));
                        CHECK(p >= 0.f);
                    }
                    for (float u1 : {0.f, 0.5f, 0.99999994f}) {
                        for (float u2 : {0.f, 0.99999994f}) {
                            INFO(c.name << ", albedo " << albedo << ", roughness " << roughness
                                        << ", wo.z " << wo.z << ", u1 " << u1 << ", u2 " << u2);
                            const BSDFSample<RGB> s = c.bsdf->sample(wo, isect, c.params, u1, u2);
                            CHECK(finite(s.value));
                            CHECK(s.value.min() >= 0.f);
                            CHECK(std::isfinite(s.pdf));
                        }
                    }
                }
            }
        }
    }
}

TEST_CASE("A BSDF rejects a direction drawn on the horizon", "[materials][bsdf]")
{
    // u1 = 1, just past the range a sampler gives, makes the cosine-weighted directions these
    // BSDFs draw lie on the horizon, where cos(theta_i) and the pdf are 0. (A metal's specular
    // lobe takes all of u1, so 1 is past it, and is not a direction at all.)
    const Interaction<RGB> isect = surface();
    for (const Case& c : cases()) {
        if (c.name == "Cook-Torrance, metal") {
            continue;
        }
        for (const Vec3<float>& wo : viewers(isect)) {
            const BSDFSample<RGB> s = c.bsdf->sample(wo, isect, c.params, 1.f, 0.3f);
            INFO(c.name << ", wo.z " << wo.z);
            CHECK(!s.is_valid());
            CHECK(s.value == RGB{0.f});
        }
    }
}

TEST_CASE("The null BSDF passes light straight through, as a delta distribution",
          "[materials][bsdf]")
{
    // A surface with the null BSDF is not there: a ray continues through it unchanged. Its
    // sample is that direction, with all the light and the delta's nominal pdf of 1, and as a
    // delta distribution, eval() and pdf() of any direction are 0.
    const NullBSDF<RGB> bsdf;
    const BSDFRequirements r = bsdf.requirements();
    CHECK(!r.needs_albedo);
    CHECK(!r.needs_normal);
    CHECK(!r.needs_metallic);
    CHECK(!r.needs_roughness);

    const Interaction<RGB> isect = surface();
    const ShadingParams<RGB> params = shading(RGB{0.5f}, 0.5f, 0.f);
    for (const Vec3<float>& wo : viewers(isect)) {
        for (float u : {0.f, 0.5f, 0.99f}) {
            const BSDFSample<RGB> s = bsdf.sample(wo, isect, params, u, 1.f - u);
            CHECK(s.wi == -wo);
            CHECK(s.value == RGB{1.f});
            CHECK(s.pdf == 1.f);
            CHECK(s.is_delta);
            CHECK(s.is_valid());
        }
        for (const Vec3<float>& wi : {wo, -wo, mirror(isect, wo)}) {
            CHECK(bsdf.eval(wo, wi, isect, params) == RGB{0.f});
            CHECK(bsdf.pdf(wo, wi, isect, params) == 0.f);
        }
    }
}
