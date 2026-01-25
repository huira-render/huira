#pragma once

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
    struct MeshBatch {
        std::shared_ptr<const Mesh<TSpectral>> mesh;
        std::vector<Transform<float>> instances;
    };

    template <IsSpectral TSpectral>
    struct LightInstance {
        std::shared_ptr<const Light<TSpectral>> light;
        Transform<float> world_transform;
    };

    template <IsSpectral TSpectral>
    class SceneView {
    public:
        SceneView(const Scene<TSpectral>& scene, const Time& time, const CameraHandle<TSpectral>& camera);

    private:
        std::vector<MeshBatch<TSpectral>> geometry_;
        std::vector<LightInstance<TSpectral>> lights_;
    };
}

#include "huira_impl/core/scene_view.ipp"
