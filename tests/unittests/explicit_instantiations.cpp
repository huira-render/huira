// assets/
#include "huira/assets/io/model_loader.hpp"

#include "huira/assets/lights/point_light.hpp"

#include "huira/assets/mesh.hpp"
#include "huira/assets/model.hpp"
#include "huira/assets/unresolved_object.hpp"

// cameras/
#include "huira/cameras/distortion/brown_distortion.hpp"
#include "huira/cameras/distortion/opencv_distortion.hpp"
#include "huira/cameras/distortion/owen_distortion.hpp"

#include "huira/cameras/sensors/sensor_model.hpp"
#include "huira/cameras/sensors/simple_rgb_sensor.hpp"
#include "huira/cameras/sensors//simple_sensor.hpp"

#include "huira/cameras/camera_model.hpp"


// core/
#include "huira/core/units/units.hpp"

#include "huira/core/rotation.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/core/types.hpp"

// ephemeris/
#include "huira/ephemeris/spice.hpp"

// handles/
#include "huira/handles/camera_handle.hpp"
#include "huira/handles/frame_handle.hpp"
#include "huira/handles/handle.hpp"
#include "huira/handles/instance_handle.hpp"
#include "huira/handles/mesh_handle.hpp"
#include "huira/handles/model_handle.hpp"
#include "huira/handles/node_handle.hpp"
#include "huira/handles/point_light_handle.hpp"
#include "huira/handles/root_frame_handle.hpp"
#include "huira/handles/unresolved_handle.hpp"

// images/
#include "huira/images/image.hpp"

// render/
#include "huira/render/frame_buffer.hpp"
#include "huira/render/interaction.hpp"
#include "huira/render/raster_renderer.hpp"
#include "huira/render/ray.hpp"
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
    template class SimpleRGBSensor<TestSpectral>;
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

    template class PointLight<TestSpectral>;

    template class RasterRenderer<TestSpectral>;

    template class Node<TestSpectral>;
    template class UnresolvedObject<TestSpectral>;
    
}
