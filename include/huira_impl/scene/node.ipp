#include <memory>
#include <string>

#include "huira/core/time.hpp"
#include "huira/core/types.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/detail/logger.hpp"
#include "huira/detail/validate.hpp"
#include "huira/spice/spice_states.hpp"

#include "huira/scene/unresolved_object.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    Node<TSpectral, TFloat>::Node(Scene<TSpectral, TFloat>* scene)
        : id_(next_id_++), scene_(scene)
    {

    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_position(const Vec3<TFloat>& position)
    {
        validate_scene_unlocked_("set_position()");
        if (auto spice_child = child_spice_origins_()) {
            HUIRA_THROW_ERROR(this->get_info_() +
                " - cannot manually set position when child has a spice_origin_ (see child "
                + spice_child->get_info_() + ")");
        }

        this->local_transform_.position = position;
        this->position_source_ = TransformSource::MANUAL_TRANSFORM;
        this->spice_origin_ = "";
        this->update_global_transform_();
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_rotation(const Rotation<TFloat>& rotation)
    {
        validate_scene_unlocked_("set_rotation()");
        if (auto spice_child = child_spice_frames_()) {
            HUIRA_THROW_ERROR(this->get_info_() +
                " - cannot manually set rotation when child has a spice_frame_ (see child "
                + spice_child->get_info_() + ")");
        }

        this->local_transform_.rotation = rotation;
        this->rotation_source_ = TransformSource::MANUAL_TRANSFORM;
        this->spice_frame_ = "";
        this->update_global_transform_();
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_scale(const Vec3<TFloat>& scale)
    {
        validate_scene_unlocked_("set_scale()");
        HUIRA_LOG_INFO(this->get_info_() + " - set_scale(" +
            std::to_string(scale[0]) + ", " +
            std::to_string(scale[1]) + ", " +
            std::to_string(scale[2]) + ")");

        this->local_transform_.scale = scale;
        this->update_global_transform_();
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_velocity(const Vec3<TFloat>& velocity)
    {
        validate_scene_unlocked_("set_velocity()");
        if (this->position_source_ == TransformSource::SPICE_TRANSFORM) {
            HUIRA_THROW_ERROR(this->get_info_() + " - cannot manually set velocity when node uses SPICE for position " +
                "(spice_origin_=" + spice_origin_ + ")");
        }
        
        this->local_transform_.velocity = velocity;
        this->update_global_transform_();
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_angular_velocity(const Vec3<TFloat>& angular_velocity)
    {
        validate_scene_unlocked_("set_angular_velocity()");
        if (this->rotation_source_ == TransformSource::SPICE_TRANSFORM) {
            HUIRA_THROW_ERROR(this->get_info_() + " - cannot manually set angular velocity when node uses SPICE for rotation " +
                "(spice_frame_=" + spice_frame_ + ")");
        }
        
        this->local_transform_.angular_velocity = angular_velocity;
        this->update_global_transform_();
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_spice_origin(const std::string& spice_origin)
    {
        validate_scene_unlocked_("set_spice_origin()");
        validate_spice_origin_allowed_();
        HUIRA_LOG_INFO(this->get_info_() + " - set_spice_origin('" + spice_origin + "')");

        this->spice_origin_ = spice_origin;
        this->position_source_ = TransformSource::SPICE_TRANSFORM;
        this->update_spice_transform_();
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_spice_frame(const std::string& spice_frame)
    {
        validate_scene_unlocked_("set_spice_frame()");
        validate_spice_frame_allowed_();
        HUIRA_LOG_INFO(this->get_info_() + " - set_spice_frame('" + spice_frame + "')");

        this->spice_frame_ = spice_frame;
        this->rotation_source_ = TransformSource::SPICE_TRANSFORM;
        this->update_spice_transform_();
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_spice(const std::string& spice_origin, const std::string& spice_frame)
    {
        validate_scene_unlocked_("set_spice()");
        validate_spice_origin_allowed_();
        validate_spice_frame_allowed_();
        HUIRA_LOG_INFO(this->get_info_() + " - set_spice('" + spice_origin + ", " + spice_frame + "')");

        this->spice_origin_ = spice_origin;
        this->spice_frame_ = spice_frame;
        this->position_source_ = TransformSource::SPICE_TRANSFORM;
        this->rotation_source_ = TransformSource::SPICE_TRANSFORM;
        this->update_spice_transform_();
    }


    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    std::weak_ptr<Node<TSpectral, TFloat>> Node<TSpectral, TFloat>::new_child()
    {
        validate_scene_unlocked_("new_child()");

        auto child = std::make_shared<Node<TSpectral, TFloat>>(scene_);
        child->set_parent_(this);
        children_.push_back(child);

        HUIRA_LOG_INFO(this->get_info_() + " - new child added: " + child->get_info_());

        return child;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::delete_child(std::weak_ptr<Node<TSpectral, TFloat>> child_weak)
    {
        validate_scene_unlocked_("delete_child()");

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
    void Node<TSpectral, TFloat>::change_parent(std::weak_ptr<Node<TSpectral, TFloat>> self_weak, Node<TSpectral, TFloat>* new_parent)
    {
        validate_scene_unlocked_("change_parent()");

        auto self = self_weak.lock();
        if (!self || self.get() != this) {
            HUIRA_THROW_ERROR(this->get_info_() + " - change_parent() called with invalid weak_ptr");
        }

        if (!new_parent) {
            HUIRA_THROW_ERROR(this->get_info_() + " - change_parent() called with null new_parent");
        }

        if (!parent_) {
            HUIRA_THROW_ERROR(this->get_info_() + " - change_parent() called on root node");
        }

        if (parent_ == new_parent) {
            return; // Already the parent, nothing to do
        }

        // Check SPICE constraints for position
        if (this->position_source_ == TransformSource::SPICE_TRANSFORM) {
            if (new_parent->position_source_ != TransformSource::SPICE_TRANSFORM) {
                HUIRA_THROW_ERROR(this->get_info_() + " - cannot change parent: node uses SPICE for position " +
                    "(spice_origin_=" + spice_origin_ + ") but new parent (" +
                    new_parent->get_info_() + ") has manually set position");
            }
        }

        // Check SPICE constraints for rotation
        if (this->rotation_source_ == TransformSource::SPICE_TRANSFORM) {
            if (new_parent->rotation_source_ != TransformSource::SPICE_TRANSFORM) {
                HUIRA_THROW_ERROR(this->get_info_() + " - cannot change parent: node uses SPICE for rotation " +
                    "(spice_frame_=" + spice_frame_ + ") but new parent (" +
                    new_parent->get_info_() + ") has manually set rotation");
            }
        }

        // Remove from old parent's children
        auto& old_children = parent_->children_;
        auto it = std::find(old_children.begin(), old_children.end(), self);
        if (it != old_children.end()) {
            old_children.erase(it);
            HUIRA_LOG_INFO(parent_->get_info_() + " - child detached: " + this->get_info_());
        }

        // Add to new parent's children
        new_parent->children_.push_back(self);
        HUIRA_LOG_INFO(new_parent->get_info_() + " - child attached: " + this->get_info_());

        // Update parent pointer
        parent_ = new_parent;

        // Recalculate transforms since hierarchy changed
        if (this->position_source_ == TransformSource::SPICE_TRANSFORM || this->rotation_source_ == TransformSource::SPICE_TRANSFORM) {
            this->update_spice_transform_();
        }
        else {
            this->update_global_transform_();
        }
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    std::weak_ptr<UnresolvedObject<TSpectral, TFloat>> Node<TSpectral, TFloat>::new_unresolved_object()
    {
        validate_scene_unlocked_("new_unresolved()");


        auto child = std::make_shared<UnresolvedObject<TSpectral, TFloat>>(scene_);
        child->set_parent_(this);

        HUIRA_LOG_INFO(this->get_info_() + " - new unresolved added: " + child->get_info_());

        children_.push_back(child);
        return child;
    }

    // ========================= //
    // === Protected Members === //
    // ========================= //

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    std::shared_ptr<Node<TSpectral, TFloat>> Node<TSpectral, TFloat>::child_spice_origins_() const
    {
        for (const auto& child : children_) {
            if (child->position_source_ == TransformSource::SPICE_TRANSFORM) {
                return child;
            }
        }
        return nullptr;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    std::shared_ptr<Node<TSpectral, TFloat>> Node<TSpectral, TFloat>::child_spice_frames_() const
    {
        for (const auto& child : children_) {
            if (child->rotation_source_ == TransformSource::SPICE_TRANSFORM) {
                return child;
            }
        }
        return nullptr;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::update_spice_transform_()
    {
        if (parent_ == nullptr) {
            return;
        }

        if (this->position_source_ == TransformSource::SPICE_TRANSFORM) {
            compute_global_spice_position_();
            compute_local_position_from_global_();
        }

        if (this->rotation_source_ == TransformSource::SPICE_TRANSFORM) {
            compute_global_spice_rotation_();
            compute_local_rotation_from_global_();
        }

        // Process-children:
        for (auto& child : children_) {
            child->update_global_transform_();
        }
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::update_all_spice_transforms_()
    {
        // Root node: just propagate to children
        if (parent_ == nullptr) {
            for (auto& child : children_) {
                child->update_all_spice_transforms_();
            }
            return;
        }

        if (this->position_source_ == TransformSource::SPICE_TRANSFORM) {
            compute_global_spice_position_();
            compute_local_position_from_global_();
        }
        else {
            compute_global_position_from_local_();
        }

        if (this->rotation_source_ == TransformSource::SPICE_TRANSFORM) {
            compute_global_spice_rotation_();
            compute_local_rotation_from_global_();
        }
        else {
            compute_global_rotation_from_local_();
        }

        this->global_transform_.scale = parent_->global_transform_.scale * this->local_transform_.scale;

        for (auto& child : children_) {
            child->update_all_spice_transforms_();
        }
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::update_global_transform_()
    {
        // Compute global from parent (only called for manual nodes)
        if (this->parent_) {
            if (this->position_source_ == TransformSource::SPICE_TRANSFORM) {
                compute_local_position_from_global_();
            }
            else {
                compute_global_position_from_local_();
            }

            if (this->rotation_source_ == TransformSource::SPICE_TRANSFORM) {
                compute_local_rotation_from_global_();
            }
            else {
                compute_global_rotation_from_local_();
            }

            this->global_transform_.scale = parent_->global_transform_.scale * this->local_transform_.scale;
        }

        // Recursively propagate to all children (handles both SPICE and Manual)
        for (auto& child : children_) {
            child->update_global_transform_();
        }
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    std::string Node<TSpectral, TFloat>::get_info_() {
        std::string info = get_type_name_() + "[" + std::to_string(this->id()) + "]";
        return info;
    }



    // ======================= //
    // === Private Members === //
    // ======================= //
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::validate_scene_unlocked_(const std::string function_name)
    {
        if (scene_->is_locked()) {
            HUIRA_THROW_ERROR(this->get_info_() + " - " + function_name + " was called with a locked scene");
        }
    };

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::validate_spice_origin_allowed_()
    {
        if (parent_) {
            if (parent_->position_source_ != TransformSource::SPICE_TRANSFORM) {
                HUIRA_THROW_ERROR(this->get_info_() + " - cannot set SPICE origin: parent node (" +
                    parent_->get_info_() + ") has manually set position");
            }
        }
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::validate_spice_frame_allowed_()
    {
        if (parent_) {
            if (parent_->rotation_source_ != TransformSource::SPICE_TRANSFORM) {
                HUIRA_THROW_ERROR(this->get_info_() + " - cannot set SPICE frame: parent node (" +
                    parent_->get_info_() + ") has manually set rotation");
            }
        }
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::compute_global_spice_position_()
    {
        auto [position, velocity, _] = spice::spkezr<TFloat>(
            this->spice_origin_, scene_->get_time(),
            scene_->root.get_spice_frame(), scene_->root.get_spice_origin()
        );
        this->global_transform_.position = position;
        this->global_transform_.velocity = velocity;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::compute_global_spice_rotation_()
    {
        auto [rotation, angular_velocity] = spice::sxform<TFloat>(
            this->spice_frame_, scene_->root.get_spice_frame(), scene_->get_time()
        );
        this->global_transform_.rotation = rotation;
        this->global_transform_.angular_velocity = angular_velocity;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::compute_local_position_from_global_()
    {
        this->local_transform_.position =
            parent_->global_transform_.rotation.inverse() *
            (this->global_transform_.position - parent_->global_transform_.position);

        this->local_transform_.velocity =
            parent_->global_transform_.rotation.inverse() *
            (this->global_transform_.velocity - parent_->global_transform_.velocity);
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::compute_local_rotation_from_global_()
    {
        this->local_transform_.rotation =
            parent_->global_transform_.rotation.inverse() * this->global_transform_.rotation;

        this->local_transform_.angular_velocity =
            parent_->global_transform_.rotation.inverse() *
            (this->global_transform_.angular_velocity - parent_->global_transform_.angular_velocity);
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::compute_global_position_from_local_()
    {
        this->global_transform_.position =
            parent_->global_transform_.position +
            parent_->global_transform_.rotation * this->local_transform_.position;
        this->global_transform_.velocity =
            parent_->global_transform_.velocity +
            parent_->global_transform_.rotation * this->local_transform_.velocity;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::compute_global_rotation_from_local_()
    {
        this->global_transform_.rotation =
            parent_->global_transform_.rotation * this->local_transform_.rotation;

        this->global_transform_.angular_velocity =
            parent_->global_transform_.angular_velocity +
            parent_->global_transform_.rotation * this->local_transform_.angular_velocity;
    }

}
