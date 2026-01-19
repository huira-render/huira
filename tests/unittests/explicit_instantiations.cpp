#include "huira/camera/distortion/brown_distortion.hpp"
#include "huira/camera/distortion/opencv_distortion.hpp"
#include "huira/camera/distortion/owen_distortion.hpp"

#include "huira/core/ray.hpp"
#include "huira/core/rotation.hpp"
#include "huira/core/transform.hpp"

#include "huira/handles/frame_handle.hpp"
#include "huira/handles/root_frame_handle.hpp"
#include "huira/handles/unresolved_handle.hpp"
#include "huira/handles/point_light_handle.hpp"

#include "huira/lights/point_light.hpp"

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

    template class Ray<TestSpectral, TestFloat>;
    template class Rotation<TestFloat>;
    template struct Transform<TestFloat>;

    template class FrameHandle<TestSpectral, TestFloat>;
    template class RootFrameHandle<TestSpectral, TestFloat>;
    template class UnresolvedHandle<TestSpectral, TestFloat>;
    template class PointLightHandle<TestSpectral, TestFloat>;

    template class PointLight<TestSpectral, TestFloat>;

    template class Scene<TestSpectral, TestFloat>;
    template class Node<TestSpectral, TestFloat>;
    template class UnresolvedObject<TestSpectral, TestFloat>;
    
}
