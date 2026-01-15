#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/scene/handle.hpp"
#include "huira/scene/nodes.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class NodeHandle : public Handle<Node<TSpectral, TFloat>> {
    public:
        NodeHandle() = delete;
        using Handle<Node<TSpectral, TFloat>>::Handle;

        NodeHandle<TSpectral, TFloat> new_child(std::string name = "") const
        {
            return NodeHandle<TSpectral, TFloat>{ this->safe_get()->new_child(name), this->scene_locked_ };
        }

        const std::string& name() const {
            return this->safe_get()->name();
        }

        void set_position(const Vec3<TFloat>& position) const {
            this->safe_get()->set_position(position);
        }

        void set_position(double x, double y, double z) const {
            this->safe_get()->set_position(Vec3<TFloat>{x, y, z});
        }

        void set_orientation(const Rotation<TFloat>& orientation) const {
            this->safe_get()->set_orientation(orientation);
        }

        void set_scale(const Vec3<TFloat>& scale) const {
            this->safe_get()->set_scale(scale);
        }

        void set_scale(double sx, double sy, double sz) const {
            this->safe_get()->set_scale(Vec3<TFloat>{sx, sy, sz});
        }

        void set_scale(double s) const {
            this->safe_get()->set_scale(Vec3<TFloat>{s, s, s});
        }

        void set_position_from_spice(const std::string& spice_origin) const {
            this->safe_get()->set_position_from_spice(spice_origin);
        }

        void set_orientation_from_spice(const std::string& spice_ref) const {
            this->safe_get()->set_orientation_from_spice(spice_ref);
        }

        void set_spice(const std::string& spice_origin, const std::string& spice_ref) const {
            this->set_position_from_spice(spice_origin);
            this->set_orientation_from_spice(spice_ref);
        }
    };
}
