#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/handles/handle.hpp"
#include "huira/handles/unresolved_handle.hpp"
#include "huira/scene/node.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class FrameHandle : public Handle<Node<TSpectral, TFloat>> {
    public:
        FrameHandle() = delete;
        using Handle<Node<TSpectral, TFloat>>::Handle;

        void set_position(const Vec3<TFloat>& position) const { this->get()->set_position(position); }
        void set_position(double x, double y, double z) const { this->get()->set_position(Vec3<TFloat>{x, y, z}); }

        void set_rotation(const Rotation<TFloat>& rotation) const { this->get()->set_rotation(rotation); }

        void set_scale(const Vec3<TFloat>& scale) const { this->get()->set_scale(scale); }
        void set_scale(double sx, double sy, double sz) const { this->get()->set_scale(Vec3<TFloat>{sx, sy, sz}); }
        void set_scale(double s) const { this->get()->set_scale(Vec3<TFloat>{s, s, s}); }

        void set_spice_origin(const std::string& spice_origin) const { this->get()->set_spice_origin(spice_origin); }
        void set_spice_frame(const std::string& spice_frame) const { this->get()->set_spice_frame(spice_frame); }
        void set_spice(const std::string& spice_origin, const std::string& spice_frame) const { this->get()->set_spice(spice_origin, spice_frame); }

        std::string get_spice_origin() const { return this->get()->get_spice_origin(); }
        std::string get_spice_frame() const { return this->get()->get_spice_frame(); }

        FrameHandle<TSpectral, TFloat> new_subframe() const {
            return FrameHandle<TSpectral, TFloat>{ this->get()->new_child() };
        }

        FrameHandle<TSpectral, TFloat> new_spice_subframe(const std::string& spice_origin, const std::string& spice_frame) const {
            FrameHandle<TSpectral, TFloat> subframe = this->new_subframe();
            subframe.set_spice(spice_origin, spice_frame);
            return subframe;
        }

        void delete_subframe(FrameHandle<TSpectral, TFloat> subframe) const {
            this->get()->delete_child(std::weak_ptr<Node<TSpectral, TFloat>>(subframe.get()));
        }



        friend class UnresolvedHandle<TSpectral, TFloat>;

        UnresolvedHandle<TSpectral, TFloat> new_unresolved_object() const {
            return UnresolvedHandle<TSpectral, TFloat>{ this->get()->new_unresolved_object() };
        }

        UnresolvedHandle<TSpectral, TFloat> new_spice_unresolved_object(const std::string& spice_origin) const {
            UnresolvedHandle<TSpectral, TFloat> unresolved = this->new_unresolved_object();
            unresolved.set_spice_origin(spice_origin);
            return unresolved;
        }



        void move_to(FrameHandle<TSpectral, TFloat> new_parent) const {
            auto self_shared = this->get();
            auto new_parent_shared = new_parent.get();
            self_shared->change_parent(std::weak_ptr<Node<TSpectral, TFloat>>(self_shared), new_parent_shared.get());
        }
    };
}
