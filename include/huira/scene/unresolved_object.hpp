#pragma once

#include <string>
#include <memory>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/scene/node.hpp"

namespace huira {
    // Forward declare:
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Scene;

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class UnresolvedObject : public Node<TSpectral, TFloat> {
    public:
        UnresolvedObject(Scene<TSpectral, TFloat>* scene)
            : Node<TSpectral, TFloat>(scene)
        {

        }

        // Remove functionality from the base class that doesn't make sense for UnresolvedObject:
        void set_rotation(const Rotation<TFloat>& rotation) = delete;
        void set_scale(const Vec3<TFloat>& scale) = delete;

        void set_angular_velocity(const Vec3<TFloat>& angular_velocity) = delete;

        void set_spice_frame(const std::string& spice_frame) = delete;
        void set_spice(const std::string& spice_origin, const std::string& spice_frame) = delete;

        std::string get_spice_frame() = delete;

        std::weak_ptr<Node<TSpectral, TFloat>> new_child() = delete;
        void delete_child(std::weak_ptr<Node<TSpectral, TFloat>> child) = delete;

        std::weak_ptr<UnresolvedObject<TSpectral, TFloat>> new_unresolved_object() = delete;


    protected:
        std::string get_type_name() const override { return "UnresolvedObject"; }

        friend class Scene<TSpectral, TFloat>;
    };
}

#include "huira_impl/scene/unresolved_object.ipp"
