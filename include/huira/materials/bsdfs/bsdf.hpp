#pragma once

#include <cstdint>

#include "huira/core/types.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/materials/shading_params.hpp"
#include "huira/render/interaction.hpp"

namespace huira {
    /**
     * @brief Result of a BSDF sample operation.
     *
     * BSDFSample::value is pre-weighted: f(wo, wi) * |cos(theta_i)| / pdf.
     * The integrator accumulates sample.value * Li directly.
     *
     * @tparam TSpectral The spectral type used in the rendering pipeline
     */
    template <IsSpectral TSpectral>
    struct BSDFSample {
        Vec3<float> wi;         ///< Sampled incoming light direction (world space)
        TSpectral   value;      ///< f(wo, wi) * |cos(theta_i)| / pdf
        float       pdf = 0.0f; ///< Probability density of the sampled direction

        [[nodiscard]] bool is_valid() const noexcept { return pdf > 0.0f; }
    };

    /**
     * @brief Abstract BSDF interface.
     *
     * A BSDF is stateless with respect to surface point. All spatially-varying
     * parameters arrive through ShadingParams, evaluated from textures by the
     * Material. A single BSDF instance is constructed once and shared across
     * all surface interactions using that shading model.
     *
     * All directions are in **world space**. The BSDF uses the Interaction's
     * tangent frame for local-space transformations as needed.
     *
     * ## Implementing a custom BSDF
     *
     * Subclass this and implement the three methods. Store model-specific
     * constants as member variables set at construction. Read spatially-varying
     * data (albedo, roughness, etc.) from ShadingParams at evaluation time.
     *
     * @tparam TSpectral The spectral type used in the rendering pipeline
     */
    template <IsSpectral TSpectral>
    class BSDF {
    public:
        virtual ~BSDF() = default;

        /**
         * @brief Evaluate the BSDF: f(wo, wi).
         *
         * Does NOT include the cosine foreshortening factor |cos(theta_i)|.
         *
         * @param wo     Outgoing direction (toward camera), world space, normalized
         * @param wi     Incoming direction (toward light), world space, normalized
         * @param isect  Surface interaction (normals, tangent frame)
         * @param params Texture-evaluated shading parameters
         * @return BSDF value f(wo, wi) [1/sr]
         */
        [[nodiscard]] virtual TSpectral eval(
            const Vec3<float>& wo,
            const Vec3<float>& wi,
            const Interaction<TSpectral>& isect,
            const ShadingParams<TSpectral>& params) const = 0;

        /**
         * @brief Importance-sample an incoming direction.
         *
         * BSDFSample::value = f(wo, wi) * |cos(theta_i)| / pdf(wo, wi)
         *
         * @param wo     Outgoing direction (toward camera), world space, normalized
         * @param isect  Surface interaction
         * @param params Texture-evaluated shading parameters
         * @param u1     Uniform random number in [0, 1)
         * @param u2     Uniform random number in [0, 1)
         * @return BSDFSample with sampled direction, throughput, PDF
         */
        [[nodiscard]] virtual BSDFSample<TSpectral> sample(
            const Vec3<float>& wo,
            const Interaction<TSpectral>& isect,
            const ShadingParams<TSpectral>& params,
            float u1, float u2) const = 0;

        /**
         * @brief Probability density of sampling direction wi given wo.
         *
         * Must be consistent with sample(). Returns 0 for delta distributions.
         *
         * @return PDF value in solid angle measure [1/sr]
         */
        [[nodiscard]] virtual float pdf(
            const Vec3<float>& wo,
            const Vec3<float>& wi,
            const Interaction<TSpectral>& isect,
            const ShadingParams<TSpectral>& params) const = 0;
    };

}
