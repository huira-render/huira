#pragma once

#include "huira/materials/bsdfs/bsdf.hpp"

namespace huira {

/**
 * @brief Lambertian (perfectly diffuse) BSDF.
 *
 * f(wo, wi) = base_color / pi
 *
 * Uses cosine-weighted hemisphere sampling. Reads only base_color from
 * ShadingParams. This BSDF is stateless and a single instance can be
 * shared across all Lambertian materials in the scene.
 *
 * @tparam TSpectral The spectral type used in the rendering pipeline
 */
template <IsSpectral TSpectral>
class LambertianBSDF : public BSDF<TSpectral> {
  public:
    LambertianBSDF() noexcept = default;
    ~LambertianBSDF() override = default;

    [[nodiscard]] BSDFRequirements requirements() const override;

    [[nodiscard]] TSpectral eval(const Vec3<float>& wo,
                                 const Vec3<float>& wi,
                                 const Interaction<TSpectral>& isect,
                                 const ShadingParams<TSpectral>& params) const override;

    [[nodiscard]] BSDFSample<TSpectral> sample(const Vec3<float>& wo,
                                               const Interaction<TSpectral>& isect,
                                               const ShadingParams<TSpectral>& params,
                                               float u1,
                                               float u2) const override;

    [[nodiscard]] float pdf(const Vec3<float>& wo,
                            const Vec3<float>& wi,
                            const Interaction<TSpectral>& isect,
                            const ShadingParams<TSpectral>& params) const override;

    std::string type() const override { return "LambertianBSDF"; }
};

} // namespace huira

#include "huira_impl/materials/bsdfs/lambertian_bsdf.ipp"
