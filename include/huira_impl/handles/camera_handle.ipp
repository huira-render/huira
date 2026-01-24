#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/handles/node_handle.hpp"
#include "huira/handles/unresolved_handle.hpp"
#include "huira/objects/cameras/camera.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    void CameraHandle<TSpectral>::set_focal_length(double focal_length) const {
        this->get()->set_focal_length(focal_length);
    }
}
