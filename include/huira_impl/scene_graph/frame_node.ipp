#include <memory>
#include <string>
#include <algorithm>

#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/detail/logger.hpp"

#include "huira/assets/lights/point_light.hpp"
#include "huira/assets/unresolved_object.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    FrameNode<TSpectral>::FrameNode(Scene<TSpectral>* scene)
        : Node<TSpectral>(scene)
    {

    }

    template <IsSpectral TSpectral>
    std::weak_ptr<FrameNode<TSpectral>> FrameNode<TSpectral>::new_child()
    {
        auto child = std::make_shared<FrameNode<TSpectral>>(this->scene_);
        child->set_parent_(this);
        children_.push_back(child);

        HUIRA_LOG_INFO(this->get_info() + " - Added: " + child->get_info());

        return child;
    }

    template <IsSpectral TSpectral>
    void FrameNode<TSpectral>::delete_child(std::weak_ptr<Node<TSpectral>> child_weak)
    {
        auto child = child_weak.lock();
        if (!child) {
            HUIRA_THROW_ERROR(this->get_info() + " - delete_child() called with expired weak_ptr");
        }

        if (child->parent_ != this) {
            HUIRA_THROW_ERROR(this->get_info() + " - delete_child() called with a child that does not belong to this node");
        }

        auto it = std::find(children_.begin(), children_.end(), child);
        if (it != children_.end()) {
            HUIRA_LOG_INFO(this->get_info() + " - Deleting " + child->get_info());
            children_.erase(it);
        }
    }

    template <IsSpectral TSpectral>
    std::weak_ptr<Camera<TSpectral>> FrameNode<TSpectral>::new_camera()
    {
        auto child = std::make_shared<Camera<TSpectral>>(this->scene_);
        child->set_parent_(this);

        HUIRA_LOG_INFO(this->get_info() + " - Added: " + child->get_info());

        children_.push_back(child);
        return child;
    }

    template <IsSpectral TSpectral>
    std::weak_ptr<Instance<TSpectral>> FrameNode<TSpectral>::new_instance(Mesh<TSpectral>* mesh)
    {
        auto child = std::make_shared<Instance<TSpectral>>(this->scene_, mesh);
        child->set_parent_(this);

        HUIRA_LOG_INFO(this->get_info() + " - Added: " + child->get_info());

        children_.push_back(child);
        return child;
    }

    template <IsSpectral TSpectral>
    std::weak_ptr<Instance<TSpectral>> FrameNode<TSpectral>::new_instance(Light<TSpectral>* light)
    {
        auto child = std::make_shared<Instance<TSpectral>>(this->scene_, light);
        child->set_parent_(this);

        HUIRA_LOG_INFO(this->get_info() + " - Added: " + child->get_info());

        children_.push_back(child);
        return child;
    }

    template <IsSpectral TSpectral>
    std::weak_ptr<Instance<TSpectral>> FrameNode<TSpectral>::new_instance(Model<TSpectral>* model)
    {
        auto child = std::make_shared<Instance<TSpectral>>(this->scene_, model);
        child->set_parent_(this);

        HUIRA_LOG_INFO(this->get_info() + " - Added: " + child->get_info());

        children_.push_back(child);
        return child;
    }

    template <IsSpectral TSpectral>
    std::weak_ptr<Instance<TSpectral>> FrameNode<TSpectral>::new_instance(UnresolvedObject<TSpectral>* unresolved_object)
    {
        auto child = std::make_shared<Instance<TSpectral>>(this->scene_, unresolved_object);
        child->set_parent_(this);
        HUIRA_LOG_INFO(this->get_info() + " - Added: " + child->get_info());
        children_.push_back(child);
        return child;
    }

    template <IsSpectral TSpectral>
    std::weak_ptr<Instance<TSpectral>> FrameNode<TSpectral>::new_instance(CameraModel<TSpectral>* camera_model)
    {
        auto child = std::make_shared<Instance<TSpectral>>(this->scene_, camera_model);
        child->set_parent_(this);
        HUIRA_LOG_INFO(this->get_info() + " - Added: " + child->get_info());
        children_.push_back(child);
        return child;
    }

    // ========================= //
    // === Protected Members === //
    // ========================= //
    template <IsSpectral TSpectral>
    bool FrameNode<TSpectral>::position_can_be_manual_() const
    {
        for (const auto& child : children_) {
            if (child->position_mode_ == TransformMode::SPICE_TRANSFORM) {
                return false;
            }
        }
        return true;
    }

    template <IsSpectral TSpectral>
    bool FrameNode<TSpectral>::rotation_can_be_manual_() const
    {
        for (const auto& child : children_) {
            if (child->rotation_mode_ == TransformMode::SPICE_TRANSFORM) {
                return false;
            }
        }
        return true;
    }
}
