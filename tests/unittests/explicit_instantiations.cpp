#include "huira/assets/lights/point_light.hpp"
#include "huira/assets/mesh.hpp"
#include "huira/assets/unresolved_object.hpp"

#include "huira/cameras/camera_model.hpp"
#include "huira/cameras/distortion/brown_distortion.hpp"
#include "huira/cameras/distortion/opencv_distortion.hpp"
#include "huira/cameras/distortion/owen_distortion.hpp"

#include "huira/core/interaction.hpp"
#include "huira/core/ray.hpp"
#include "huira/core/rotation.hpp"
#include "huira/core/scene.hpp"
#include "huira/core/scene_view.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/core/spice.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/core/types.hpp"
#include "huira/core/units.hpp"

#include "huira/handles/camera_handle.hpp"
#include "huira/handles/frame_handle.hpp"
#include "huira/handles/handle.hpp"
#include "huira/handles/instance_handle.hpp"
#include "huira/handles/mesh_handle.hpp"
#include "huira/handles/node_handle.hpp"
#include "huira/handles/point_light_handle.hpp"
#include "huira/handles/root_frame_handle.hpp"
#include "huira/handles/unresolved_handle.hpp"

#include "huira/images/image.hpp"

#include "huira/render/unresolved_render.hpp"

#include "huira/scene_graph/frame_node.hpp"
#include "huira/scene_graph/instance.hpp"
#include "huira/scene_graph/node.hpp"

#include "huira/stars/star.hpp"

namespace huira {
    using TestSpectral = RGB;
    using TestFloat = float;

    // Explicit instantiations - forces code generation for code coverage purposes

    template class Scene<TestSpectral>;
    template class SceneView<TestSpectral>;

    template class BrownDistortion<TestSpectral>;
    template class OpenCVDistortion<TestSpectral>;
    template class OwenDistortion<TestSpectral>;
    template class CameraModel<TestSpectral>;

    template class Ray<TestSpectral>;
    template class Rotation<TestFloat>;
    template struct Transform<TestFloat>;

    template class CameraModelHandle<TestSpectral>;
    template class FrameHandle<TestSpectral>;
    template class InstanceHandle<TestSpectral>;
    template class MeshHandle<TestSpectral>;
    template class PointLightHandle<TestSpectral>;
    template class RootFrameHandle<TestSpectral>;
    template class UnresolvedObjectHandle<TestSpectral>;

    template class Image<std::int8_t>;
    template class Image<float>;
    template class Image<Vec3<float>>;
    template class Image<TestSpectral>;

    //template class PointLight<TestSpectral>;

    template UnresolvedRenderResult<TestSpectral> unresolved_render<TestSpectral>(
        const Scene<TestSpectral>&,
        const InstanceHandle<TestSpectral>&,
        UnresolvedRenderSettings);

    template class Node<TestSpectral>;
    template class UnresolvedObject<TestSpectral>;
    
}
