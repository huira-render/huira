#include <memory>
#include <string>

#include "huira/core/time.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    Node<TSpectral, TFloat>::Node(Scene<TSpectral, TFloat>* scene)
        : scene_(scene)
    {

    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    std::weak_ptr<Node<TSpectral, TFloat>> Node<TSpectral, TFloat>::new_child(std::string name)
    {
        if (scene_->is_locked()) {
            throw std::runtime_error("Attempted to add a Node to a locked scene");
        }

        auto child = std::make_shared<Node<TSpectral, TFloat>>(scene_);
        child->set_parent(this);

        scene_->add_node_name(name, child);

        children_.push_back(child);
        return child;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_position(const Vec3<TFloat>& position)
    {
        if (has_spice_descendant_positions()) {
            throw std::runtime_error("Cannot set manual position: node has descendants with SPICE positions");
        }
        this->local_transform_.translation = position;
        this->position_source_ = TransformSource::Manual;
        this->spice_origin_ = "";
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_orientation(const Rotation<TFloat>& orientation)
    {
        if (has_spice_descendant_orientations()) {
            throw std::runtime_error("Cannot set manual orientation: node has descendants with SPICE orientations");
        }
        this->local_transform_.rotation = orientation;
        this->orientation_source_ = TransformSource::Manual;
        this->spice_ref_ = "";
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_scale(const Vec3<TFloat>& scale)
    {
        this->local_transform_.scale = scale;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_position_from_spice(const std::string& spice_origin)
    {
        if (!parent_) {
            throw std::runtime_error("Cannot set SPICE position on a root node");
        }

        if (parent_->position_source_ == TransformSource::Manual) {
            throw std::runtime_error("Cannot set SPICE position: parent node has manual position");
        }

        this->spice_origin_ = spice_origin;
        this->position_source_ = TransformSource::Spice;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_orientation_from_spice(const std::string& spice_ref)
    {
        if (!parent_) {
            throw std::runtime_error("Cannot set SPICE orientation on a root node");
        }

        if (parent_->orientation_source_ == TransformSource::Manual) {
            throw std::runtime_error("Cannot set SPICE orientation: parent node has manual orientation");
        }

        this->spice_ref_ = spice_ref;
        this->orientation_source_ = TransformSource::Spice;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_spice(const std::string& spice_origin, const std::string& spice_ref)
    {
        this->set_position_from_spice(spice_origin);
        this->set_orientation_from_spice(spice_ref);
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::update_spice_transform(const Time& time)
    {
        // TODO
        (void)time;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    bool Node<TSpectral, TFloat>::has_spice_descendant_positions() const
    {
        for (auto& child : children_) {
            if (child->position_source_ == TransformSource::Spice) {
                return true;
            }
        }
        return false;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    bool Node<TSpectral, TFloat>::has_spice_descendant_orientations() const
    {
        for (auto& child : children_) {
            if (child->orientation_source_ == TransformSource::Spice) {
                return true;
            }
        }
        return false;
    }
}
