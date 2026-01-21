#include "huira/images/image.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/scene/scene.hpp"
#include "huira/camera/camera.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    UnresolvedRenderResult<TSpectral, TFloat> unresolved_render(
        const Scene<TSpectral, TFloat>& scene,
        const CameraHandle<TSpectral, TFloat>& camera,
        UnresolvedRenderSettings settings)
    {
        // Get the camera to look at the object:
        (void)scene;
        (void)camera;
        (void)settings;

        UnresolvedRenderResult<TSpectral, TFloat> result;

        return result;
    }
}
