#include "huira/camera/camera.hpp"
#include "huira/handles/node_handle.hpp"
#include "huira/handles/unresolved_handle.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void CameraHandle<TSpectral, TFloat>::set_focal_length(TFloat focal_length) const {
        this->get()->set_focal_length(focal_length);
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void CameraHandle<TSpectral, TFloat>::look_at(const UnresolvedObjectHandle<TSpectral, TFloat>& target,
        Vec3<TFloat> up) const {
        this->look_at(target.get_global_position(), up);
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void CameraHandle<TSpectral, TFloat>::look_at(const Vec3<TFloat>& target_position,
        Vec3<TFloat> up) const {
        this->get()->look_at(target_position, up);
    }
}
