#include <memory>
#include <string>

#include "huira/util/logger.hpp"
#include "huira/util/validate.hpp"

namespace huira {
/**
 * @brief Evaluate all material textures at the given interaction point.
 *
 * Samples all image slots (no branching), applies scalar factors,
 * incorporates vertex albedo, and perturbs the shading normal from
 * the normal map. Returns everything the BSDF needs on the stack.
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

template <IsSpectral TSpectral>
void Material<TSpectral>::set_albedo(std::shared_ptr<Image<TSpectral>> albedo_image)
{
    check_image_(albedo_image, "albedo");
    albedo_image_ = albedo_image;
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_albedo_factor(TSpectral albedo_factor)
{
    validate_spectrum(albedo_factor, this->get_info() + " albedo factor");
    albedo_factor_ = albedo_factor;
}

template <IsSpectral TSpectral>
void Material<TSpectral>::reset_albedo()
{
    albedo_image_ = default_albedo_image_;
    albedo_factor_ = TSpectral{1.0f};
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_alpha(std::shared_ptr<Image<float>> alpha_image)
{
    check_image_(alpha_image, "alpha");
    alpha_image_ = alpha_image;
    has_alpha_ = true;
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_alpha_factor(float alpha_factor)
{
    validate_unit_interval(alpha_factor, this->get_info() + " alpha factor");
    alpha_factor_ = alpha_factor;
    if (alpha_factor_ < 1.0f) {
        has_alpha_ = true;
    }
}

template <IsSpectral TSpectral>
void Material<TSpectral>::reset_alpha()
{
    alpha_image_ = default_alpha_image_;
    alpha_factor_ = 1.0f;
    has_alpha_ = false;
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_metallic_image(std::shared_ptr<Image<float>> metallic_image)
{
    check_image_(metallic_image, "metallic");
    metallic_image_ = metallic_image;
    if (metallic_factor_ == 0.f) {
        metallic_factor_ = 1.f;
    }
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
    metallic_factor_ = 0.0f;
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

template <IsSpectral TSpectral>
void Material<TSpectral>::set_transmission_image(
    std::shared_ptr<Image<TSpectral>> transmission_image)
{
    check_image_(transmission_image, "transmission");
    transmission_image_ = transmission_image;
    if (transmission_factor_ == TSpectral{0.f}) {
        transmission_factor_ = TSpectral{1.f};
    }
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
    transmission_factor_ = TSpectral{0.0f};
}

template <IsSpectral TSpectral>
void Material<TSpectral>::set_emissive_image(std::shared_ptr<Image<TSpectral>> emissive_image)
{
    check_image_(emissive_image, "emissive");
    emissive_image_ = emissive_image;
    if (emissive_factor_ == TSpectral{0.f}) {
        emissive_factor_ = TSpectral{1.f};
    }
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
    emissive_factor_ = TSpectral{0.0f};
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
};
} // namespace huira
