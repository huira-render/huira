#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "huira/util/logger.hpp"
#include "huira/util/validate.hpp"

namespace huira {

namespace detail {

/**
 * @brief The shading normal n, with its tilt from the geometric normal ng reduced, if need be, so
 * that the mirror reflection of the viewer's direction wo in it is above the geometric surface.
 *
 * Blender's Cycles does this for every shading normal (ensure_valid_reflection() in its kernel).
 * The reflection must be at least min(0.9 wo.ng, 0.01) above the surface: a reflection may be as
 * shallow as the view itself. A normal that meets that is returned as it is. Otherwise, the normal
 * is turned toward ng, in the plane of the two, until its reflection of wo is at that height; the
 * viewer is then above the normal too. The result is continuous in n and wo.
 *
 * In a frame with ng as z, and n in the x-z plane, the turned normal is (sqrt(1 - z^2), 0, z) for
 * the z at which 2 (wo_x sqrt(1 - z^2) + wo_z z) z - wo_z, the height of the reflection, is the
 * threshold t. Squaring gives a quadratic in z^2, a z^4 - b z^2 + c / 4 = 0, with
 * a = wo_x^2 + wo_z^2, b = a + wo_z t and c = (wo_z + t)^2. Of its two roots, the smaller solves
 * the equation before squaring where wo_x >= 0, and the larger where wo_x < 0.
 *
 * @param ng The geometric normal, on the viewer's side (unit).
 * @param wo The direction to the viewer (unit).
 * @param n The shading normal (unit).
 */
[[nodiscard]] inline Vec3<float>
valid_reflection_normal(const Vec3<float>& ng, const Vec3<float>& wo, const Vec3<float>& n) noexcept
{
    const float wo_z = glm::dot(wo, ng);
    if (!(wo_z > 0.0f)) {
        return n; // no viewer, or one the geometric normal does not face
    }
    const float threshold = std::min(0.9f * wo_z, 0.01f);
    const float n_z = glm::dot(n, ng);
    if (2.0f * glm::dot(n, wo) * n_z - wo_z >= threshold) {
        return n;
    }

    const Vec3<float> along = n - n_z * ng;
    const float along_length = glm::length(along);
    if (!(along_length > 1e-6f)) {
        return ng; // n is -ng, or nearly: any turn toward ng is as good as another
    }
    const Vec3<float> x = along / along_length;
    const float wo_x = glm::dot(wo, x);
    const float a = wo_x * wo_x + wo_z * wo_z;
    const float b = a + wo_z * threshold;
    const float c = (wo_z + threshold) * (wo_z + threshold);
    const float root = std::sqrt(std::max(0.0f, b * b - a * c));
    // (The smaller root as c / (2 (b + root)), which loses no precision when it is small.)
    const float z2 =
        std::clamp(wo_x < 0.0f ? (b + root) / (2.0f * a) : c / (2.0f * (b + root)), 0.0f, 1.0f);
    return std::sqrt(1.0f - z2) * x + std::sqrt(z2) * ng;
}

} // namespace detail

/**
 * @brief Evaluate all material textures at the given interaction point.
 *
 * Samples all image slots (no branching), applies scalar factors,
 * incorporates vertex albedo, and perturbs the shading normal from
 * the normal map. Returns everything the BSDF needs on the stack.
 *
 * Where the shading normal is tilted so far from the geometric normal that the viewer, isect.wo,
 * is below it, or its mirror reflection is below the geometric surface, its tilt is reduced until
 * neither is (see detail::valid_reflection_normal()), so that the point is not black.
 *
 * @param isect The surface interaction from the ray-geometry hit
 * @return MaterialEval<TSpectral> with ShadingParams and modified Interaction
 */
template <IsSpectral TSpectral>
MaterialEval<TSpectral> Material<TSpectral>::evaluate(const Interaction<TSpectral>& isect) const
{
    const Vec2<float>& uv = isect.uv;

    ShadingParams<TSpectral> params;

    if (eval_albedo_) {
        params.albedo =
            albedo_image_->sample_bilinear(uv.x, uv.y) * albedo_factor_ * isect.vertex_albedo;
    }

    // if (has_alpha_) {
    params.opacity = alpha_image_->sample_bilinear(uv.x, uv.y) * alpha_factor_;
    //}

    if (eval_metallic_) {
        params.metallic = metallic_image_->sample_bilinear(uv.x, uv.y) * metallic_factor_;
    }

    if (eval_roughness_) {
        params.roughness = roughness_image_->sample_bilinear(uv.x, uv.y) * roughness_factor_;
    }

    params.transmission = transmission_image_->sample_bilinear(uv.x, uv.y) * transmission_factor_;

    params.emission = emissive_image_->sample_bilinear(uv.x, uv.y) * emissive_factor_;

    Interaction<TSpectral> shading_isect = isect;

    if (eval_normal_) {
        if (isect.tangent != Vec3<float>{0.0f}) {
            Vec3<float> ts_normal = glm::normalize(normal_image_->sample_bilinear(uv.x, uv.y));

            ts_normal.x *= normal_factor_;
            ts_normal.y *= normal_factor_;
            ts_normal = glm::normalize(ts_normal);

            Vec3<float> perturbed = isect.tangent * ts_normal.x + isect.bitangent * ts_normal.y +
                                    isect.normal_s * ts_normal.z;
            shading_isect.normal_s = glm::normalize(perturbed);
        }

        // A normal map, or normals interpolated across a coarse mesh, can tilt the shading normal
        // away from the viewer, who always sees the geometric surface from its front. Every BSDF
        // reflects nothing toward a viewer below its surface, so the point would be black; and
        // a glossy BSDF would reflect the view into the surface. Reduce the tilt until neither
        // happens. (An interaction with no viewer, wo = 0, is left as it is.)
        shading_isect.normal_s =
            detail::valid_reflection_normal(isect.normal_g, isect.wo, shading_isect.normal_s);
    }

    // Keep the original tangent/bitangent, just orthogonalize against new normal
    shading_isect.tangent = glm::normalize(
        isect.tangent - glm::dot(isect.tangent, shading_isect.normal_s) * shading_isect.normal_s);
    shading_isect.bitangent = glm::cross(shading_isect.normal_s, shading_isect.tangent);

    MaterialEval<TSpectral> result{};
    result.params = params;
    result.isect = shading_isect;
    return result;
}

template <IsSpectral TSpectral>
TSpectral Material<TSpectral>::bsdf_eval(const Vec3<float>& wo,
                                         const Vec3<float>& wi,
                                         const MaterialEval<TSpectral>& eval) const
{
    return bsdf_->eval(wo, wi, eval.isect, eval.params);
}

template <IsSpectral TSpectral>
BSDFSample<TSpectral> Material<TSpectral>::bsdf_sample(const Vec3<float>& wo,
                                                       const MaterialEval<TSpectral>& eval,
                                                       float u1,
                                                       float u2) const
{
    return bsdf_->sample(wo, eval.isect, eval.params, u1, u2);
}

template <IsSpectral TSpectral>
float Material<TSpectral>::bsdf_pdf(const Vec3<float>& wo,
                                    const Vec3<float>& wi,
                                    const MaterialEval<TSpectral>& eval) const
{
    return bsdf_->pdf(wo, wi, eval.isect, eval.params);
}

/// Throws if an image set on the material is null or has no pixels, which sampling it would read
/// past. Its pixels were checked when it was made a texture (see Texture).
template <IsSpectral TSpectral>
template <typename TPixel>
void Material<TSpectral>::check_image_(const std::shared_ptr<Image<TPixel>>& image,
                                       const char* what) const
{
    if (image == nullptr || image->width() < 1 || image->height() < 1) {
        HUIRA_THROW_ERROR(this->get_info() + " - The " + what + " image must not be null or empty");
    }
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_bsdf(std::shared_ptr<BSDF<TSpectral>> bsdf)
{
    if (bsdf == nullptr) {
        HUIRA_THROW_ERROR(this->get_info() + " - The BSDF must not be null");
    }
    bsdf_ = bsdf;

    // Cache the flags flat in the material memory layout
    auto reqs = bsdf_->requirements();
    eval_albedo_ = reqs.needs_albedo;
    eval_metallic_ = reqs.needs_metallic;
    eval_roughness_ = reqs.needs_roughness;
    eval_normal_ = reqs.needs_normal;
}

// Albedo: a reflectance, so its values and factor are from 0 to 1.

template <IsSpectral TSpectral>
void Material<TSpectral>::set_albedo(TSpectral albedo)
{
    validate_spectral_ratio(albedo, this->get_info() + " albedo");
    albedo_image_ = std::make_shared<Image<TSpectral>>(1, 1, albedo);
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_albedo_image(std::shared_ptr<Image<TSpectral>> albedo_image)
{
    check_image_(albedo_image, "albedo");
    albedo_image_ = albedo_image;
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_albedo_factor(TSpectral albedo_factor)
{
    validate_spectral_ratio(albedo_factor, this->get_info() + " albedo factor");
    albedo_factor_ = albedo_factor;
}

template <IsSpectral TSpectral>
void Material<TSpectral>::reset_albedo()
{
    albedo_image_ = default_albedo_image_;
    albedo_factor_ = TSpectral{1.0f};
}

// Alpha: an opacity from 0 to 1.

template <IsSpectral TSpectral>
void Material<TSpectral>::update_has_alpha_()
{
    has_alpha_ = alpha_image_ != default_alpha_image_ || alpha_factor_ < 1.0f;
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_alpha(float alpha)
{
    validate_unit_interval(alpha, this->get_info() + " alpha");
    alpha_image_ = std::make_shared<Image<float>>(1, 1, alpha);
    update_has_alpha_();
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_alpha_image(std::shared_ptr<Image<float>> alpha_image)
{
    check_image_(alpha_image, "alpha");
    alpha_image_ = alpha_image;
    update_has_alpha_();
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_alpha_factor(float alpha_factor)
{
    validate_unit_interval(alpha_factor, this->get_info() + " alpha factor");
    alpha_factor_ = alpha_factor;
    update_has_alpha_();
}

template <IsSpectral TSpectral>
void Material<TSpectral>::reset_alpha()
{
    alpha_image_ = default_alpha_image_;
    alpha_factor_ = 1.0f;
    update_has_alpha_();
}

// Metallic: from 0 to 1.

template <IsSpectral TSpectral>
void Material<TSpectral>::set_metallic(float metallic)
{
    validate_unit_interval(metallic, this->get_info() + " metallic");
    metallic_image_ = std::make_shared<Image<float>>(1, 1, metallic);
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_metallic_image(std::shared_ptr<Image<float>> metallic_image)
{
    check_image_(metallic_image, "metallic");
    metallic_image_ = metallic_image;
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_metallic_factor(float metallic_factor)
{
    validate_unit_interval(metallic_factor, this->get_info() + " metallic factor");
    metallic_factor_ = metallic_factor;
}

template <IsSpectral TSpectral>
void Material<TSpectral>::reset_metallic()
{
    metallic_image_ = default_metallic_image_;
    metallic_factor_ = 1.0f;
}

// Roughness: from 0 to 1.

template <IsSpectral TSpectral>
void Material<TSpectral>::set_roughness(float roughness)
{
    validate_unit_interval(roughness, this->get_info() + " roughness");
    roughness_image_ = std::make_shared<Image<float>>(1, 1, roughness);
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_roughness_image(std::shared_ptr<Image<float>> roughness_image)
{
    check_image_(roughness_image, "roughness");
    roughness_image_ = roughness_image;
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_roughness_factor(float roughness_factor)
{
    validate_unit_interval(roughness_factor, this->get_info() + " roughness factor");
    roughness_factor_ = roughness_factor;
}

template <IsSpectral TSpectral>
void Material<TSpectral>::reset_roughness()
{
    roughness_image_ = default_roughness_image_;
    roughness_factor_ = 1.0f;
}

// Normal: a tangent-space normal map; its factor scales the perturbation. There is no
// constant form, as a uniform tilt of every normal is not a material property.

template <IsSpectral TSpectral>
void Material<TSpectral>::set_normal_image(std::shared_ptr<Image<Vec3<float>>> normal_image)
{
    check_image_(normal_image, "normal");
    normal_image_ = normal_image;
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_normal_factor(float normal_factor)
{
    validate_real(normal_factor, this->get_info() + " normal factor");
    normal_factor_ = normal_factor;
}

template <IsSpectral TSpectral>
void Material<TSpectral>::reset_normal()
{
    normal_image_ = default_normal_image_;
    normal_factor_ = 1.0f;
}

// Transmission: the fraction of light that passes through, from 0 to 1.

template <IsSpectral TSpectral>
void Material<TSpectral>::set_transmission(TSpectral transmission)
{
    validate_spectral_ratio(transmission, this->get_info() + " transmission");
    transmission_image_ = std::make_shared<Image<TSpectral>>(1, 1, transmission);
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_transmission_image(
    std::shared_ptr<Image<TSpectral>> transmission_image)
{
    check_image_(transmission_image, "transmission");
    transmission_image_ = transmission_image;
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_transmission_factor(TSpectral transmission_factor)
{
    validate_spectral_ratio(transmission_factor, this->get_info() + " transmission factor");
    transmission_factor_ = transmission_factor;
}

template <IsSpectral TSpectral>
void Material<TSpectral>::reset_transmission()
{
    transmission_image_ = default_transmission_image_;
    transmission_factor_ = TSpectral{1.0f};
}

// Emission: a radiance, so finite and non-negative, with no upper bound.

template <IsSpectral TSpectral>
void Material<TSpectral>::set_emissive(TSpectral emissive)
{
    validate_spectrum(emissive, this->get_info() + " emissive");
    emissive_image_ = std::make_shared<Image<TSpectral>>(1, 1, emissive);
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_emissive_image(std::shared_ptr<Image<TSpectral>> emissive_image)
{
    check_image_(emissive_image, "emissive");
    emissive_image_ = emissive_image;
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_emissive_factor(TSpectral emissive_factor)
{
    validate_spectrum(emissive_factor, this->get_info() + " emissive factor");
    emissive_factor_ = emissive_factor;
}

template <IsSpectral TSpectral>
void Material<TSpectral>::reset_emissive()
{
    emissive_image_ = default_emissive_image_;
    emissive_factor_ = TSpectral{1.0f};
}

template <IsSpectral TSpectral>
Material<TSpectral>::Material(std::shared_ptr<BSDF<TSpectral>> bsdf,
                              std::shared_ptr<Image<TSpectral>> albedo_image,
                              std::shared_ptr<Image<float>> alpha_image,
                              std::shared_ptr<Image<float>> metallic_image,
                              std::shared_ptr<Image<float>> roughness_image,
                              std::shared_ptr<Image<Vec3<float>>> normal_image,
                              std::shared_ptr<Image<TSpectral>> transmission_image,
                              std::shared_ptr<Image<TSpectral>> emissive_image)
    : default_albedo_image_{albedo_image}, default_alpha_image_{alpha_image},
      default_metallic_image_{metallic_image}, default_roughness_image_{roughness_image},
      default_normal_image_{normal_image}, default_transmission_image_{transmission_image},
      default_emissive_image_{emissive_image}
{
    albedo_image_ = default_albedo_image_;
    alpha_image_ = default_alpha_image_;
    metallic_image_ = default_metallic_image_;
    roughness_image_ = default_roughness_image_;
    normal_image_ = default_normal_image_;
    transmission_image_ = default_transmission_image_;
    emissive_image_ = default_emissive_image_;

    set_bsdf(bsdf);
}
} // namespace huira
