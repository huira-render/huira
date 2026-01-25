// Core API header for the Huira library.
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

// detail/ is not part of the public API

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

// objects/ is not part of the public API

// Rendering interfaces:
#include "huira/render/unresolved_render.hpp"
