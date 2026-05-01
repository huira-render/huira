#pragma once

#include <memory>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/images/image.hpp"
#include "huira/materials/bsdfs/bsdf.hpp"
#include "huira/materials/shading_params.hpp"
#include "huira/render/interaction.hpp"
#include "huira/scene/scene_object.hpp"

namespace huira {
// Forward Declare
template <IsSpectral TSpectral>
class Scene;

/**
 * @brief Result of texture evaluation at a surface point.
 */
template <IsSpectral TSpectral>
struct MaterialEval {
    ShadingParams<TSpectral> params;
    Interaction<TSpectral> isect;
};

/**
 * @brief Surface material: holds image pointers and a BSDF pointer, provides
 *        the primary shading interface for integrators.
 *
 * Material is a concrete class. It does not own any of its referenced data —
 * the Scene owns all Images, BSDFs, and Materials. Material holds non-owning
 * pointers to Scene-managed assets.
 *
 * Every image slot is a non-null pointer. For slots without a texture, the
 * Scene provides a 1x1 Image filled with the appropriate default value:
 *
 *   - base_color_image:  1x1 white (TSpectral{1})
 *   - metallic_image:    1x1 with metallic_factor (typically 0.0)
 *   - roughness_image:   1x1 with roughness_factor (typically 0.5)
 *   - normal_image:      1x1 with {0.5, 0.5, 1.0} (unperturbed normal)
 *   - emissive_image:    1x1 black (TSpectral{0})
 *
 * @tparam TSpectral The spectral type used in the rendering pipeline
 */
template <IsSpectral TSpectral>
class Material : public SceneObject<Material<TSpectral>> {
  public:
    Material(std::shared_ptr<BSDF<TSpectral>> bsdf,
             std::shared_ptr<Image<TSpectral>> albedo_image,
             std::shared_ptr<Image<float>> alpha_image,
             std::shared_ptr<Image<float>> metallic_image,
             std::shared_ptr<Image<float>> roughness_image,
             std::shared_ptr<Image<Vec3<float>>> normal_image,
             std::shared_ptr<Image<TSpectral>> transmission_image,
             std::shared_ptr<Image<TSpectral>> emissive_image);

    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;

    [[nodiscard]] MaterialEval<TSpectral> evaluate(const Interaction<TSpectral>& isect) const;

    [[nodiscard]] TSpectral bsdf_eval(const Vec3<float>& wo,
                                      const Vec3<float>& wi,
                                      const MaterialEval<TSpectral>& eval) const;

    [[nodiscard]] BSDFSample<TSpectral> bsdf_sample(const Vec3<float>& wo,
                                                    const MaterialEval<TSpectral>& eval,
                                                    float u1,
                                                    float u2) const;

    [[nodiscard]] float bsdf_pdf(const Vec3<float>& wo,
                                 const Vec3<float>& wi,
                                 const MaterialEval<TSpectral>& eval) const;

    void set_bsdf(std::shared_ptr<BSDF<TSpectral>> bsdf);

    void set_albedo(std::shared_ptr<Image<TSpectral>> albedo_image);
    void set_albedo_factor(TSpectral albedo_factor);
    void reset_albedo();

    void set_alpha(std::shared_ptr<Image<float>> alpha_image);
    void set_alpha_factor(float alpha_factor);
    void reset_alpha();

    void set_metallic_image(std::shared_ptr<Image<float>> metallic_image);
    void set_metallic_factor(float metallic_factor);
    void reset_metallic();

    void set_roughness_image(std::shared_ptr<Image<float>> roughness_image);
    void set_roughness_factor(float roughness_factor);
    void reset_roughness();

    void set_normal_image(std::shared_ptr<Image<Vec3<float>>> normal_image);
    void set_normal_factor(float normal_factor);
    void reset_normal();

    void set_transmission_image(std::shared_ptr<Image<TSpectral>> transmission_image);
    void set_transmission_factor(TSpectral transmission_factor);
    void reset_transmission();

    void set_emissive_image(std::shared_ptr<Image<TSpectral>> emissive_image);
    void set_emissive_factor(TSpectral emissive_factor);
    void reset_emissive();

    std::string type() const override { return "Material"; }

    // TODO Move this to private (access data only!)
    std::shared_ptr<Image<TSpectral>> albedo_image_;
    std::shared_ptr<Image<float>> alpha_image_;
    std::shared_ptr<Image<float>> metallic_image_;
    std::shared_ptr<Image<float>> roughness_image_;
    std::shared_ptr<Image<Vec3<float>>> normal_image_;
    std::shared_ptr<Image<TSpectral>> transmission_image_;
    std::shared_ptr<Image<TSpectral>> emissive_image_;

    float alpha_factor() const noexcept { return alpha_factor_; }
    bool has_alpha() const noexcept { return has_alpha_; }
    bool has_alpha_texture() const noexcept { return alpha_image_ != default_alpha_image_; }

  private:
    bool eval_albedo_ = true;
    std::shared_ptr<Image<TSpectral>> default_albedo_image_;

    bool has_alpha_ = false;
    std::shared_ptr<Image<float>> default_alpha_image_;

    bool eval_metallic_ = true;
    std::shared_ptr<Image<float>> default_metallic_image_;

    bool eval_roughness_ = true;
    std::shared_ptr<Image<float>> default_roughness_image_;

    bool eval_normal_ = true;
    std::shared_ptr<Image<Vec3<float>>> default_normal_image_;

    std::shared_ptr<Image<TSpectral>> default_transmission_image_;

    std::shared_ptr<Image<TSpectral>> default_emissive_image_;

    std::shared_ptr<BSDF<TSpectral>> bsdf_;

    TSpectral albedo_factor_{1.0f};
    float alpha_factor_ = 1.0f;
    float metallic_factor_ = 0.0f;
    float roughness_factor_ = 1.0f;
    float normal_factor_ = 1.0f;
    TSpectral transmission_factor_{0.0f};
    TSpectral emissive_factor_{0.0f};

    friend class Scene<TSpectral>;
};

} // namespace huira

#include "huira_impl/materials/material.ipp"
