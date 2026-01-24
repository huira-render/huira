#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    void CameraHandle<TSpectral>::set_focal_length(double focal_length) const {
        this->get()->set_focal_length(focal_length);
    }
}
