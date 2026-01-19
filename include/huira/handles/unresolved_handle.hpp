#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/handles/handle.hpp"
#include "huira/scene/unresolved_object.hpp"

namespace huira {
    // Forward Declare:
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class FrameHandle;

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class UnresolvedHandle : public Handle<UnresolvedObject<TSpectral, TFloat>> {
    public:
        UnresolvedHandle() = delete;
        using Handle<UnresolvedObject<TSpectral, TFloat>>::Handle;

        void set_position(const Vec3<TFloat>& position) const { this->get()->set_position(position); }
        void set_position(double x, double y, double z) const { this->get()->set_position(Vec3<TFloat>{x, y, z}); }

        void set_spice_origin(const std::string& spice_origin) const { this->get()->set_spice_origin(spice_origin); }

        void move_to(FrameHandle<TSpectral, TFloat> new_parent) const {
            this->get()->change_parent(std::weak_ptr<UnresolvedObject<TSpectral, TFloat>>(this->ptr_), new_parent.get().get());
        }
    };

}
