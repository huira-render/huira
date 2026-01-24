#include <memory>
#include <string>
#include <algorithm>

#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/detail/logger.hpp"

#include "huira/objects/lights/point_light.hpp"
#include "huira/objects/unresolved_object.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    FrameNode<TSpectral>::FrameNode(Scene<TSpectral>* scene)
        : Node<TSpectral>(scene)
    {

    }

    template <IsSpectral TSpectral>
    std::weak_ptr<FrameNode<TSpectral>> FrameNode<TSpectral>::new_child()
    {
        this->validate_scene_unlocked_("new_child()");

        auto child = std::make_shared<FrameNode<TSpectral>>(this->scene_);
        child->set_parent_(this);
        children_.push_back(child);

        HUIRA_LOG_INFO(this->get_info() + " - new FrameNode added: " + child->get_info());

        return child;
    }

    template <IsSpectral TSpectral>
    void FrameNode<TSpectral>::delete_child(std::weak_ptr<Node<TSpectral>> child_weak)
    {
        this->validate_scene_unlocked_("delete_child()");

        auto child = child_weak.lock();
        if (!child) {
            HUIRA_THROW_ERROR(this->get_info() + " - delete_child() called with expired weak_ptr");
        }

        if (child->parent_ != this) {
            HUIRA_THROW_ERROR(this->get_info() + " - delete_child() called with a child that does not belong to this node");
        }

        auto it = std::find(children_.begin(), children_.end(), child);
        if (it != children_.end()) {
            HUIRA_LOG_INFO(this->get_info() + " - deleting child: " + child->get_info());
            children_.erase(it);
        }
    }

    template <IsSpectral TSpectral>
    std::weak_ptr<UnresolvedObject<TSpectral>> FrameNode<TSpectral>::new_unresolved_object()
    {
        this->validate_scene_unlocked_("new_unresolved_object()");

        auto child = std::make_shared<UnresolvedObject<TSpectral>>(this->scene_);
        child->set_parent_(this);

        HUIRA_LOG_INFO(this->get_info() + " - new UnresolvedObject added: " + child->get_info());

        children_.push_back(child);
        return child;
    }

    template <IsSpectral TSpectral>
    std::weak_ptr<PointLight<TSpectral>> FrameNode<TSpectral>::new_point_light(TSpectral spectral_intensity)
    {
        this->validate_scene_unlocked_("new_point_light()");

        auto child = std::make_shared<PointLight<TSpectral>>(this->scene_, spectral_intensity);
        child->set_parent_(this);

        HUIRA_LOG_INFO(this->get_info() + " - new PointLight added: " + child->get_info());

        children_.push_back(child);
        return child;
    }

    template <IsSpectral TSpectral>
    std::weak_ptr<Camera<TSpectral>> FrameNode<TSpectral>::new_camera()
    {
        this->validate_scene_unlocked_("new_camera()");

        auto child = std::make_shared<Camera<TSpectral>>(this->scene_);
        child->set_parent_(this);

        HUIRA_LOG_INFO(this->get_info() + " - new Camera added: " + child->get_info());

        children_.push_back(child);
        return child;
    }

    // ========================= //
    // === Protected Members === //
    // ========================= //

    template <IsSpectral TSpectral>
    void FrameNode<TSpectral>::on_transform_changed_()
    {
        // Propagate transform updates to all children
        for (auto& child : children_) {
            child->update_global_transform_();
        }
    }

    template <IsSpectral TSpectral>
    std::shared_ptr<Node<TSpectral>> FrameNode<TSpectral>::child_spice_origins_() const
    {
        for (const auto& child : children_) {
            if (child->position_source_ == TransformSource::SPICE_TRANSFORM) {
                return child;
            }
        }
        return nullptr;
    }

    template <IsSpectral TSpectral>
    std::shared_ptr<Node<TSpectral>> FrameNode<TSpectral>::child_spice_frames_() const
    {
        for (const auto& child : children_) {
            if (child->rotation_source_ == TransformSource::SPICE_TRANSFORM) {
                return child;
            }
        }
        return nullptr;
    }

    template <IsSpectral TSpectral>
    void FrameNode<TSpectral>::update_all_spice_transforms_()
    {
        // First update this node's transforms (parent class logic)
        Node<TSpectral>::update_all_spice_transforms_();

        // Then propagate to all children
        for (auto& child : children_) {
            child->update_all_spice_transforms_();
        }
    }

}
