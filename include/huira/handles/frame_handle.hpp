#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/handles/handle.hpp"
#include "huira/scene/node.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class FrameHandle : public Handle<Node<TSpectral, TFloat>> {
    public:
        FrameHandle() = delete;
        using Handle<Node<TSpectral, TFloat>>::Handle;

        // Wrap Node methods:
        FrameHandle<TSpectral, TFloat> new_subframe(std::string name = "") const {
            return FrameHandle<TSpectral, TFloat>{ this->safe_get()->new_child(name) };
        }

        FrameHandle<TSpectral, TFloat> new_spice_subframe(const std::string& spice_origin, const std::string& spice_frame, std::string name = "") const
        {
            FrameHandle<TSpectral, TFloat> subframe = this->new_subframe(name);
            subframe.set_spice_origin(spice_origin);
            subframe.set_spice_frame(spice_frame);
            return subframe;
        }

        const std::string& name() const { return this->safe_get()->name(); }

        void set_position(const Vec3<TFloat>& position) const { this->safe_get()->set_position(position); }
        void set_position(double x, double y, double z) const { this->safe_get()->set_position(Vec3<TFloat>{x, y, z}); }

        void set_rotation(const Rotation<TFloat>& rotation) const { this->safe_get()->set_rotation(rotation); }

        void set_scale(const Vec3<TFloat>& scale) const { this->safe_get()->set_scale(scale); }
        void set_scale(double sx, double sy, double sz) const { this->safe_get()->set_scale(Vec3<TFloat>{sx, sy, sz}); }
        void set_scale(double s) const { this->safe_get()->set_scale(Vec3<TFloat>{s, s, s}); }

        void set_spice_origin(const std::string& spice_origin) const { this->safe_get()->set_spice_origin(spice_origin); }
        void set_spice_frame(const std::string& spice_frame) const { this->safe_get()->set_spice_frame(spice_frame); }
        void set_spice(const std::string& spice_origin, const std::string& spice_frame) const {
            this->safe_get()->set_spice_origin(spice_origin);
            this->safe_get()->set_spice_frame(spice_frame);
        }
    };
}
