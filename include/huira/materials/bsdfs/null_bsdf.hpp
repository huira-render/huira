#pragma once

#include <memory>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/materials/bsdfs/bsdf.hpp"
#include "huira/materials/shading_params.hpp"
#include "huira/render/interaction.hpp"

namespace huira {

/**
 * @brief Null BSDF (does nothing).
 *
 * This BSDF is stateless and a single instance can be shared across all
 * materials in the scene that use it.
 *
 * @tparam TSpectral The spectral type used in the rendering pipeline
 */
template <IsSpectral TSpectral>
class NullBSDF final : public BSDF<TSpectral> {
  public:
    NullBSDF() noexcept = default;

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

    std::string type() const override { return "NullBSDF"; }
};

} // namespace huira

#include "huira_impl/materials/bsdfs/null_bsdf.ipp"
