#include "huira/core/ray.hpp"
//#include "huira/core/ray_hit.hpp"
#include "huira/core/rotation.hpp"
#include "huira/core/scene.hpp"
#include "huira/core/spectral_bins.hpp"
//#include "huira/core/spice.hpp"
//#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/core/types.hpp"
//#include "huira/core/units.hpp"

#include "huira/handles/camera_handle.hpp"
#include "huira/handles/frame_handle.hpp"
#include "huira/handles/handle.hpp"
#include "huira/handles/node_handle.hpp"
#include "huira/handles/point_handle.hpp"
#include "huira/handles/point_light_handle.hpp"
#include "huira/handles/root_frame_handle.hpp"
#include "huira/handles/unresolved_handle.hpp"

#include "huira/images/image.hpp"

#include "huira/objects/cameras/camera.hpp"
#include "huira/objects/cameras/distortion/brown_distortion.hpp"
#include "huira/objects/cameras/distortion/opencv_distortion.hpp"
#include "huira/objects/cameras/distortion/owen_distortion.hpp"

#include "huira/objects/lights/point_light.hpp"

#include "huira/objects/scene_graph/frame_node.hpp"
#include "huira/objects/scene_graph/node.hpp"

#include "huira/objects/unresolved_object.hpp"

#include "huira/render/unresolved_render.hpp"

namespace huira {
    using TestSpectral = RGB;
    using TestFloat = float;

    // Explicit instantiations - forces code generation for code coverage purposes

    template class BrownDistortion<TestSpectral, TestFloat>;
    template class OpenCVDistortion<TestSpectral, TestFloat>;
    template class OwenDistortion<TestSpectral, TestFloat>;
    template class Camera<TestSpectral, TestFloat>;

    template class Ray<TestSpectral, TestFloat>;
    template class Rotation<TestFloat>;
    template struct Transform<TestFloat>;

    template class FrameHandle<TestSpectral, TestFloat>;
    template class RootFrameHandle<TestSpectral, TestFloat>;
    template class CameraHandle<TestSpectral, TestFloat>;
    template class UnresolvedObjectHandle<TestSpectral, TestFloat>;
    template class PointLightHandle<TestSpectral, TestFloat>;

    template class Image<std::int8_t>;
    template class Image<float>;
    template class Image<Vec3<float>>;
    template class Image<TestSpectral>;

    template class PointLight<TestSpectral, TestFloat>;

    template UnresolvedRenderResult<TestSpectral, TestFloat> unresolved_render<TestSpectral, TestFloat>(
        const Scene<TestSpectral, TestFloat>&,
        const CameraHandle<TestSpectral, TestFloat>&,
        UnresolvedRenderSettings);

    template class Scene<TestSpectral, TestFloat>;
    template class Node<TestSpectral, TestFloat>;
    template class UnresolvedObject<TestSpectral, TestFloat>;
    
}
