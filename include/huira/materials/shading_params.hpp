#pragma once

#include "huira/core/types.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {

    /**
     * @brief Surface shading parameters evaluated at an intersection point.
     *
     * Constructed on the stack by Material::evaluate(), passed by const reference
     * to BSDF eval/sample/pdf methods. Each BSDF reads only the fields it needs.
     *
     * Fields are initialized to physically reasonable defaults so that a
     * partially filled ShadingParams still produces sensible output.
     *
     * @tparam TSpectral The spectral type used in the rendering pipeline
     */
    template <IsSpectral TSpectral>
    struct ShadingParams {
        /// Base color / albedo. Already incorporates vertex_albedo, texture
        /// sampling, and scalar factor multiplication.
        TSpectral base_color{ 1 };

        /// Perceptual roughness in [0, 1]. Squared internally by microfacet BSDFs.
        float roughness = 0.5f;

        /// Metallic factor in [0, 1]. 0 = dielectric, 1 = conductor.
        float metallic = 0.0f;

        /// Opacity in [0, 1]. 1 = fully opaque.
        float opacity = 1.0f;
    };

}
