#include "huira/images/image.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/core/scene.hpp"
#include "huira/handles/camera_handle.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    UnresolvedRenderResult<TSpectral> unresolved_render(
        const Scene<TSpectral>& scene,
        const InstanceHandle<TSpectral>& camera_instance,
        UnresolvedRenderSettings settings)
    {
        // Get the camera to look at the object:
        (void)scene;
        (void)camera_instance;
        (void)settings;

        UnresolvedRenderResult<TSpectral> result;

        return result;
    }
}
