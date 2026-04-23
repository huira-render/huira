#pragma once

#include "huira/handles/geometry/geometry_handle.hpp"
#include "huira/geometry/ellipsoid.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class EllipsoidHandle : public GeometryHandle<TSpectral> {
    public:
        using GeometryHandle<TSpectral>::GeometryHandle;

        std::shared_ptr<Ellipsoid<TSpectral>> get_ellipsoid_shared() const {
            return this->get_ellipsoid_();
        }

    private:
        std::shared_ptr<Ellipsoid<TSpectral>> get_ellipsoid_() const {
            auto ptr = this->template get<Ellipsoid<TSpectral>>();
            if (ptr) {
                return ptr;
            }
            else {
                HUIRA_THROW_ERROR("EllipsoidHandle::get_ellipsoid_ - Invalid handle or does not contain an Ellipsoid");
            }
        }
    };
}
