#pragma once

#include "huira/images/image.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/scene/scene.hpp"
#include "huira/camera/camera.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    struct UnresolvedRenderResult {
        Image<TSpectral> received_power;
    };

    struct UnresolvedRenderSettings {

    };

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    UnresolvedRenderResult<TSpectral, TFloat> unresolved_render(
        const Scene<TSpectral, TFloat>& scene,
        const CameraHandle<TSpectral, TFloat>& camera,
        UnresolvedRenderSettings settings = UnresolvedRenderSettings{}
    );
}

#include "huira_impl/render/unresolved_render.ipp"
