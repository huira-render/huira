// Core API header for the Huira library.

// assets/ is not part of the public API

// cameras/ is not part of the public API

// concepts/ is not part of the public API

// Core components:
// #include "huira/core/constants.hpp"    // Not part of public API
#include "huira/core/rotation.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/core/time.hpp"
// #include "huira/core/transform.hpp"    // Not part of the public API
#include "huira/core/types.hpp"

// Ephemeris interfaces:
#include "huira/ephemeris/spice.hpp"

// Handles for various scene elements
#include "huira/handles/camera_handle.hpp"
#include "huira/handles/frame_handle.hpp"
//#include "huira/handles/handle.hpp"            // Not part of public API
#include "huira/handles/instance_handle.hpp"
#include "huira/handles/mesh_handle.hpp"
//#include "huira/handles/node_handle.hpp"       // Not part of public API
//#include "huira/handles/point_handle.hpp"      // Not part of public API
#include "huira/handles/point_light_handle.hpp"
//#include "huira/handles/root_frame_handle.hpp" // Not part of public API
#include "huira/handles/unresolved_handle.hpp"

// Image interfaces
#include "huira/images/image.hpp"
#include "huira/images/io/read_image.hpp"
#include "huira/images/io/png_io.hpp"
#include "huira/images/io/jpeg_io.hpp"
#include "huira/images/io/fits_io.hpp"
#include "huira/images/io/tiff_io.hpp"

// platform/ is not part of the public API

// Rendering interfaces:
#include "huira/render/frame_buffer.hpp"
#include "huira/render/interaction.hpp"
#include "huira/render/raster_renderer.hpp"
#include "huira/render/ray.hpp"
//#include "huira/render/renderer.hpp"       // Not part of the public API
//#include "huira/render/sampler.hpp"        // Not part of the public API

// Scene management
//#include "huira/scene/frame_node.hpp"  // Not part of the public API
//#include "huira/scene/instance.hpp"    // Not part of the public API
//#include "huira/scene/node.hpp"        // Not part of the public API
#include "huira/scene/scene.hpp"
#include "huira/scene/scene_view.hpp"

// Stars Interfaces:
//#include "huira/stars/star.hpp"
//#include "huira/stars/tycho_processor.hpp"

// Units:
#include "huira/core/units/units.hpp"

// util/ is not part of the public API
