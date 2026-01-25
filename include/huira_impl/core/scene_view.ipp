#include <memory>
#include <vector>

#include "huira/assets/mesh.hpp"
#include "huira/assets/lights/light.hpp"
#include "huira/core/scene.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/handles/camera_handle.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    SceneView<TSpectral>::SceneView(const Scene<TSpectral>& scene, const Time& time, const CameraHandle<TSpectral>& camera)
    {

    }
}
