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
 * Material is a concrete class. The Scene owns BSDFs and Materials, and the images that
 * textures are made from; a Material holds shared pointers to them.
 *
 * Every material value (albedo, alpha, metallic, roughness, normal, transmission, emission)
 * is an image times a factor, and the three ways to set one never touch each other:
 *
 *   - set_X(value) makes the image a 1x1 of that value: a constant across the surface.
 *   - set_X_image(image) makes the image a texture.
 *   - set_X_factor(f) sets the factor, a multiplier on whichever image is set. It is 1 by
 *     default, so an image shows as it is unless the factor is changed on purpose.
 *   - reset_X() restores the default image and a factor of 1.
 *
 * Every image slot is a non-null pointer. The Scene provides the default 1x1 images, each
 * holding the value a material has until that slot is set:
 *
 *   - albedo:        1 (white)
 *   - alpha:         1 (opaque)
 *   - metallic:      0
 *   - roughness:     1
 *   - normal:        {0, 0, 1} (unperturbed)
 *   - transmission:  0
 *   - emissive:      0
 *
 * Every setter validates its argument: albedo, alpha, metallic, roughness and transmission
 * values and factors are from 0 to 1; emission is finite and non-negative; the normal
 * factor is finite.
 *
 * The emission is the radiance the surface emits, from either side, in W/(m^2 sr) in each
 * spectral bin: the emissive image times the emissive factor. It is added where a ray meets
 * the surface, on top of the light it reflects.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
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

    void set_albedo(TSpectral albedo);
    void set_albedo_image(std::shared_ptr<Image<TSpectral>> albedo_image);
    void set_albedo_factor(TSpectral albedo_factor);
    void reset_albedo();

    void set_alpha(float alpha);
    void set_alpha_image(std::shared_ptr<Image<float>> alpha_image);
    void set_alpha_factor(float alpha_factor);
    void reset_alpha();

    void set_metallic(float metallic);
    void set_metallic_image(std::shared_ptr<Image<float>> metallic_image);
    void set_metallic_factor(float metallic_factor);
    void reset_metallic();

    void set_roughness(float roughness);
    void set_roughness_image(std::shared_ptr<Image<float>> roughness_image);
    void set_roughness_factor(float roughness_factor);
    void reset_roughness();

    void set_normal_image(std::shared_ptr<Image<Vec3<float>>> normal_image);
    void set_normal_factor(float normal_factor);
    void reset_normal();

    void set_transmission(TSpectral transmission);
    void set_transmission_image(std::shared_ptr<Image<TSpectral>> transmission_image);
    void set_transmission_factor(TSpectral transmission_factor);
    void reset_transmission();

    void set_emissive(TSpectral emissive);
    void set_emissive_image(std::shared_ptr<Image<TSpectral>> emissive_image);
    void set_emissive_factor(TSpectral emissive_factor);
    void reset_emissive();

    TSpectral albedo_factor() const noexcept { return albedo_factor_; }
    float alpha_factor() const noexcept { return alpha_factor_; }
    float metallic_factor() const noexcept { return metallic_factor_; }
    float roughness_factor() const noexcept { return roughness_factor_; }
    float normal_factor() const noexcept { return normal_factor_; }
    TSpectral transmission_factor() const noexcept { return transmission_factor_; }
    TSpectral emissive_factor() const noexcept { return emissive_factor_; }

    std::string type() const override { return "Material"; }

    // TODO Move this to private (access data only!)
    std::shared_ptr<Image<TSpectral>> albedo_image_;
    std::shared_ptr<Image<float>> alpha_image_;
    std::shared_ptr<Image<float>> metallic_image_;
    std::shared_ptr<Image<float>> roughness_image_;
    std::shared_ptr<Image<Vec3<float>>> normal_image_;
    std::shared_ptr<Image<TSpectral>> transmission_image_;
    std::shared_ptr<Image<TSpectral>> emissive_image_;

    bool has_alpha() const noexcept { return has_alpha_; }
    bool has_alpha_texture() const noexcept { return alpha_image_ != default_alpha_image_; }

  private:
    template <typename TPixel>
    void check_image_(const std::shared_ptr<Image<TPixel>>& image, const char* what) const;
    void check_ratio_image_(const std::shared_ptr<Image<TSpectral>>& image, const char* what) const;

    /// Whether anything about the alpha can make the surface less than opaque.
    void update_has_alpha_();

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

    // Factors multiply their image, so they default to 1 for every slot; the default images
    // hold the values a material starts with.
    TSpectral albedo_factor_{1.0f};
    float alpha_factor_ = 1.0f;
    float metallic_factor_ = 1.0f;
    float roughness_factor_ = 1.0f;
    float normal_factor_ = 1.0f;
    TSpectral transmission_factor_{1.0f};
    TSpectral emissive_factor_{1.0f};

    friend class Scene<TSpectral>;
};

} // namespace huira

#include "huira_impl/materials/material.ipp"
