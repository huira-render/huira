#include <memory>
#include <string>
#include <algorithm>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/detail/logger.hpp"

#include "huira/scene/unresolved_object.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    FrameNode<TSpectral, TFloat>::FrameNode(Scene<TSpectral, TFloat>* scene)
        : Node<TSpectral, TFloat>(scene)
    {

    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    std::weak_ptr<FrameNode<TSpectral, TFloat>> FrameNode<TSpectral, TFloat>::new_child()
    {
        this->validate_scene_unlocked_("new_child()");

        auto child = std::make_shared<FrameNode<TSpectral, TFloat>>(this->scene_);
        child->set_parent_(this);
        children_.push_back(child);

        HUIRA_LOG_INFO(this->get_info_() + " - new child added: " + child->get_info_());

        return child;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void FrameNode<TSpectral, TFloat>::delete_child(std::weak_ptr<Node<TSpectral, TFloat>> child_weak)
    {
        this->validate_scene_unlocked_("delete_child()");

        auto child = child_weak.lock();
        if (!child) {
            HUIRA_THROW_ERROR(this->get_info_() + " - delete_child() called with expired weak_ptr");
        }

        if (child->parent_ != this) {
            HUIRA_THROW_ERROR(this->get_info_() + " - delete_child() called with a child that does not belong to this node");
        }

        auto it = std::find(children_.begin(), children_.end(), child);
        if (it != children_.end()) {
            HUIRA_LOG_INFO(this->get_info_() + " - deleting child: " + child->get_info_());
            children_.erase(it);
        }
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    std::weak_ptr<UnresolvedObject<TSpectral, TFloat>> FrameNode<TSpectral, TFloat>::new_unresolved_object()
    {
        this->validate_scene_unlocked_("new_unresolved_object()");

        auto child = std::make_shared<UnresolvedObject<TSpectral, TFloat>>(this->scene_);
        child->set_parent_(this);

        HUIRA_LOG_INFO(this->get_info_() + " - new unresolved added: " + child->get_info_());

        children_.push_back(child);
        return child;
    }


    // ========================= //
    // === Protected Members === //
    // ========================= //

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void FrameNode<TSpectral, TFloat>::on_transform_changed_()
    {
        // Propagate transform updates to all children
        for (auto& child : children_) {
            child->update_global_transform_();
        }
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    std::shared_ptr<Node<TSpectral, TFloat>> FrameNode<TSpectral, TFloat>::child_spice_origins_() const
    {
        for (const auto& child : children_) {
            if (child->position_source_ == TransformSource::SPICE_TRANSFORM) {
                return child;
            }
        }
        return nullptr;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    std::shared_ptr<Node<TSpectral, TFloat>> FrameNode<TSpectral, TFloat>::child_spice_frames_() const
    {
        for (const auto& child : children_) {
            if (child->rotation_source_ == TransformSource::SPICE_TRANSFORM) {
                return child;
            }
        }
        return nullptr;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void FrameNode<TSpectral, TFloat>::update_all_spice_transforms_()
    {
        // First update this node's transforms (parent class logic)
        Node<TSpectral, TFloat>::update_all_spice_transforms_();

        // Then propagate to all children
        for (auto& child : children_) {
            child->update_all_spice_transforms_();
        }
    }

}
