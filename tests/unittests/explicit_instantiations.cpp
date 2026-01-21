#include "huira/camera/distortion/brown_distortion.hpp"
#include "huira/camera/distortion/opencv_distortion.hpp"
#include "huira/camera/distortion/owen_distortion.hpp"
#include "huira/camera/camera.hpp"

#include "huira/core/ray.hpp"
#include "huira/core/rotation.hpp"
#include "huira/core/transform.hpp"
#include "huira/core/types.hpp"

#include "huira/handles/frame_handle.hpp"
#include "huira/handles/root_frame_handle.hpp"
#include "huira/handles/unresolved_handle.hpp"
#include "huira/handles/point_light_handle.hpp"
#include "huira/handles/camera_handle.hpp"

#include "huira/images/image.hpp"

#include "huira/lights/point_light.hpp"

#include "huira/render/unresolved_render.hpp"

#include "huira/scene/node.hpp"
#include "huira/scene/scene.hpp"
#include "huira/scene/unresolved_object.hpp"

#include "huira/spectral/spectral_bins.hpp"

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
