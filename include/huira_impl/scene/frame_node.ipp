#include <algorithm>
#include <memory>
#include <string>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/util/logger.hpp"

#include "huira/assets/lights/point_light.hpp"
#include "huira/assets/unresolved/unresolved_object.hpp"


namespace huira {

    /**
     * @brief Construct a FrameNode and attach to a Scene.
     * @param scene Pointer to the owning Scene
     */
    template <IsSpectral TSpectral>
    FrameNode<TSpectral>::FrameNode(Scene<TSpectral>* scene)
        : Node<TSpectral>(scene)
    {
    }


    /**
     * @brief Create a new child FrameNode and attach it to this node.
     * @return std::weak_ptr<FrameNode<TSpectral>> Weak pointer to new child
     */
    template <IsSpectral TSpectral>
    std::weak_ptr<FrameNode<TSpectral>> FrameNode<TSpectral>::new_child()
    {
        auto child = std::make_shared<FrameNode<TSpectral>>(this->scene_);
        child->set_parent_(this);
        children_.push_back(child);

        HUIRA_LOG_INFO(this->get_info() + " - Added: " + child->get_info());

        return child;
    }


    /**
     * @brief Delete a child node from this FrameNode.
     * @param child_weak Weak pointer to child node
     */
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
            this->scene_->node_registry_.remove(child);
            children_.erase(it);
        }
    }


    /**
     * @brief Create a new Camera leaf node and attach it to this FrameNode.
     * @return std::weak_ptr<Camera<TSpectral>> Weak pointer to new camera
     */
    template <IsSpectral TSpectral>
    std::weak_ptr<Camera<TSpectral>> FrameNode<TSpectral>::new_camera()
    {
        auto child = std::make_shared<Camera<TSpectral>>(this->scene_);
        child->set_parent_(this);

        HUIRA_LOG_INFO(this->get_info() + " - Added: " + child->get_info());

        children_.push_back(child);
        return child;
    }


    /**
     * @brief Create a new Instance leaf node for a mesh and attach it to this FrameNode.
     * @param mesh Mesh pointer
     * @return std::weak_ptr<Instance<TSpectral>> Weak pointer to new instance
     */
    template <IsSpectral TSpectral>
    std::weak_ptr<Instance<TSpectral>> FrameNode<TSpectral>::new_instance(Mesh<TSpectral>* mesh)
    {
        auto child = std::make_shared<Instance<TSpectral>>(this->scene_, mesh);
        child->set_parent_(this);

        HUIRA_LOG_INFO(this->get_info() + " - Added: " + child->get_info());

        children_.push_back(child);
        return child;
    }


    /**
     * @brief Create a new Instance leaf node for a light and attach it to this FrameNode.
     * @param light Light pointer
     * @return std::weak_ptr<Instance<TSpectral>> Weak pointer to new instance
     */
    template <IsSpectral TSpectral>
    std::weak_ptr<Instance<TSpectral>> FrameNode<TSpectral>::new_instance(Light<TSpectral>* light)
    {
        auto child = std::make_shared<Instance<TSpectral>>(this->scene_, light);
        child->set_parent_(this);

        HUIRA_LOG_INFO(this->get_info() + " - Added: " + child->get_info());

        children_.push_back(child);
        return child;
    }


    /**
     * @brief Create a new Instance leaf node for a model and attach it to this FrameNode.
     * @param model Model pointer
     * @return std::weak_ptr<Instance<TSpectral>> Weak pointer to new instance
     */
    template <IsSpectral TSpectral>
    std::weak_ptr<Instance<TSpectral>> FrameNode<TSpectral>::new_instance(Model<TSpectral>* model)
    {
        auto child = std::make_shared<Instance<TSpectral>>(this->scene_, model);
        child->set_parent_(this);

        HUIRA_LOG_INFO(this->get_info() + " - Added: " + child->get_info());

        children_.push_back(child);
        return child;
    }


    /**
     * @brief Create a new Instance leaf node for an unresolved object and attach it to this FrameNode.
     * @param unresolved_object UnresolvedObject pointer
     * @return std::weak_ptr<Instance<TSpectral>> Weak pointer to new instance
     */
    template <IsSpectral TSpectral>
    std::weak_ptr<Instance<TSpectral>> FrameNode<TSpectral>::new_instance(UnresolvedObject<TSpectral>* unresolved_object)
    {
        auto child = std::make_shared<Instance<TSpectral>>(this->scene_, unresolved_object);
        child->set_parent_(this);
        HUIRA_LOG_INFO(this->get_info() + " - Added: " + child->get_info());
        children_.push_back(child);
        return child;
    }


    /**
     * @brief Create a new Instance leaf node for a camera model and attach it to this FrameNode.
     * @param camera_model CameraModel pointer
     * @return std::weak_ptr<Instance<TSpectral>> Weak pointer to new instance
     */
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

    /**
     * @brief Check if manual position is allowed (no child uses SPICE).
     * @return bool True if allowed
     */
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


    /**
     * @brief Check if manual rotation is allowed (no child uses SPICE).
     * @return bool True if allowed
     */
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
