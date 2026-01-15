#pragma once

#include <vector>
#include <memory>
#include <string>
#include <stdexcept>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/math/transform.hpp"

namespace huira {
    // Forward declare:
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Scene;

    enum class TransformSource {
        Manual,
        Spice
    };

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Node {
    public:
        Node(Scene<TSpectral, TFloat>* scene);

        std::weak_ptr<Node<TSpectral, TFloat>> new_child(std::string name = "");

        const std::string& name() const { return scene_->name_of(this); }

        void set_position(const Vec3<TFloat>& position);
        void set_orientation(const Rotation<TFloat>& orientation);
        void set_scale(const Vec3<TFloat>& scale);

        void set_position_from_spice(const std::string& spice_origin);
        void set_orientation_from_spice(const std::string& spice_ref);

    protected:
        void set_parent(Node<TSpectral, TFloat>* parent) { parent_ = parent; }

        bool has_spice_descendant_positions() const;
        bool has_spice_descendant_orientations() const;

        Transform<TFloat> local_transform_;
        
        TransformSource position_source_ = TransformSource::Manual;
        TransformSource orientation_source_ = TransformSource::Manual;

        std::string spice_origin_ = "";
        std::string spice_ref_ = "";

    private:
        Scene<TSpectral, TFloat>* scene_;
        Node<TSpectral, TFloat>* parent_ = nullptr;

        std::vector<std::shared_ptr<Node<TSpectral, TFloat>>> children_;

        friend class Scene<TSpectral, TFloat>;
    };
}

#include "huira_impl/scene/nodes.ipp"
