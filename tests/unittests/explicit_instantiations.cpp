// assets/
#include "huira/assets/io/model_loader.hpp"
#include "huira/assets/lights/sphere_light.hpp"
#include "huira/assets/model.hpp"
#include "huira/assets/unresolved/unresolved_object.hpp"
#include "huira/geometry/mesh.hpp"

// cameras/
#include "huira/cameras/camera_model.hpp"
#include "huira/cameras/distortion/brown_distortion.hpp"
#include "huira/cameras/distortion/opencv_distortion.hpp"
#include "huira/cameras/distortion/owen_distortion.hpp"
#include "huira/cameras/sensors/sensor_model.hpp"
#include "huira/cameras/sensors/simple_sensor.hpp"

// core/
#include "huira/core/rotation.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/core/types.hpp"
#include "huira/units/units.hpp"

// ephemeris/
#include "huira/core/spice.hpp"

// handles/
#include "huira/handles/assets/light_handle.hpp"
#include "huira/handles/assets/model_handle.hpp"
#include "huira/handles/assets/unresolved_handle.hpp"
#include "huira/handles/camera_handle.hpp"
#include "huira/handles/geometry/mesh_handle.hpp"
#include "huira/handles/handle.hpp"
#include "huira/handles/scene/frame_handle.hpp"
#include "huira/handles/scene/instance_handle.hpp"
#include "huira/handles/scene/node_handle.hpp"
#include "huira/handles/scene/root_frame_handle.hpp"

// images/
#include "huira/images/image.hpp"

// render/
#include "huira/geometry/ray.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/render/interaction.hpp"
#include "huira/render/renderer.hpp"
#include "huira/render/sampler.hpp"

// scene/
#include "huira/scene/frame_node.hpp"
#include "huira/scene/instance.hpp"
#include "huira/scene/node.hpp"
#include "huira/scene/scene.hpp"
#include "huira/scene/scene_view.hpp"

// stars/
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
template class SimpleSensor<TestSpectral>;
template class CameraModel<TestSpectral>;

template class Ray<TestSpectral>;
template class Rotation<TestFloat>;
template struct Transform<TestFloat>;

template class CameraModelHandle<TestSpectral>;
template class FrameHandle<TestSpectral>;
template class InstanceHandle<TestSpectral>;
template class MeshHandle<TestSpectral>;
template class LightHandle<TestSpectral>;
template class RootFrameHandle<TestSpectral>;
template class UnresolvedObjectHandle<TestSpectral>;

template class Image<std::int8_t>;
template class Image<float>;
template class Image<Vec3<float>>;
template class Image<TestSpectral>;

template class SphereLight<TestSpectral>;

template class Renderer<TestSpectral>;

template class Node<TestSpectral>;
template class UnresolvedObject<TestSpectral>;

} // namespace huira
