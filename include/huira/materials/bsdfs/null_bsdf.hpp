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
 * It reflects no light. A path passes a surface with it by the surface's transmission alone,
 * without scattering there: one with a transmission of 1 lets all light through unchanged, as
 * the boundary of a medium does; one with a transmission of 0 is black and opaque.
 *
 * This BSDF is stateless and a single instance can be shared across all
 * materials in the scene that use it.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class NullBSDF final : public BSDF<TSpectral> {
  public:
    NullBSDF(const NullBSDF&) = delete;
    NullBSDF& operator=(const NullBSDF&) = delete;
    NullBSDF(NullBSDF&&) = delete;
    NullBSDF& operator=(NullBSDF&&) = delete;

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

    [[nodiscard]] bool reflects() const override { return false; }

    std::string type() const override { return "NullBSDF"; }
};

} // namespace huira

#include "huira_impl/materials/bsdfs/null_bsdf.ipp"
