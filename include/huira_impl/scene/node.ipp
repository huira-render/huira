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
        if (scene_->is_locked()) {
            HUIRA_THROW_ERROR(this->get_info_() + " - set_position() was called with a locked scene");
        }

        if (auto spice_child = child_spice_origins_()) {
            HUIRA_THROW_ERROR(this->get_info_() +
                " - cannot manually set position when child has a spice_origin_ (see child "
                + spice_child->get_info_() + ")");
        }

        this->local_transform_.position = position;
        this->position_source_ = TransformSource::Manual;
        this->spice_origin_ = "";

        // Update the global pose:
        this->update_global_transform_();
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_rotation(const Rotation<TFloat>& rotation)
    {
        if (scene_->is_locked()) {
            HUIRA_THROW_ERROR(this->get_info_() + " - set_rotation() was called with a locked scene");
        }

        if (auto spice_child = child_spice_frames_()) {
            HUIRA_THROW_ERROR(this->get_info_() +
                " - cannot manually set rotation when child has a spice_frame_ (see child "
                + spice_child->get_info_() + ")");
        }
        this->local_transform_.rotation = rotation;
        this->rotation_source_ = TransformSource::Manual;
        this->spice_frame_ = "";

        // Update the global pose:
        this->update_global_transform_();
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_scale(const Vec3<TFloat>& scale)
    {
        if (scene_->is_locked()) {
            HUIRA_THROW_ERROR(this->get_info_() + " - set_scale() was called with a locked scene");
        }

        this->local_transform_.scale = scale;

        // Update the global pose:
        this->update_global_transform_();
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_velocity(const Vec3<TFloat>& velocity)
    {
        if (scene_->is_locked()) {
            HUIRA_THROW_ERROR(this->get_info_() + " - set_velocity() was called with a locked scene");
        }

        if (this->position_source_ == TransformSource::Spice) {
            HUIRA_THROW_ERROR(this->get_info_() + " - cannot manually set velocity when node uses SPICE for position " +
                "(spice_origin_=" + spice_origin_ + ")");
        }
        this->local_transform_.velocity = velocity;

        // Update the global pose:
        this->update_global_transform_();
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_angular_velocity(const Vec3<TFloat>& angular_velocity)
    {
        if (scene_->is_locked()) {
            HUIRA_THROW_ERROR(this->get_info_() + " - set_angular_velocity() was called with a locked scene");
        }

        if (this->rotation_source_ == TransformSource::Spice) {
            HUIRA_THROW_ERROR(this->get_info_() + " - cannot manually set angular velocity when node uses SPICE for rotation " +
                "(spice_frame_=" + spice_frame_ + ")");
        }
        this->local_transform_.angular_velocity = angular_velocity;

        // Update the global pose:
        this->update_global_transform_();
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_spice_origin(const std::string& spice_origin)
    {
        if (scene_->is_locked()) {
            HUIRA_THROW_ERROR(this->get_info_() + " - set_spice_origin() was called with a locked scene");
        }

        if (parent_) {
            if (parent_->position_source_ != TransformSource::Spice) {
                HUIRA_THROW_ERROR(this->get_info_() + " - cannot set SPICE origin: parent node (" +
                    parent_->get_info_() + ") has manually set position");
            }
        }

        HUIRA_LOG_INFO(this->get_info_() + " - set_spice_origin('" + spice_origin + "')");

        this->spice_origin_ = spice_origin;
        this->position_source_ = TransformSource::Spice;

        this->update_spice_transform_();
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_spice_frame(const std::string& spice_frame)
    {
        if (scene_->is_locked()) {
            HUIRA_THROW_ERROR(this->get_info_() + " - set_spice_frame() was called with a locked scene");
        }

        if (parent_) {
            if (parent_->rotation_source_ != TransformSource::Spice) {
                HUIRA_THROW_ERROR(this->get_info_() + " - cannot set SPICE frame: parent node (" +
                    parent_->get_info_() + ") has manually set rotation");
            }
        }

        HUIRA_LOG_INFO(this->get_info_() + " - set_spice_frame('" + spice_frame + "')");

        this->spice_frame_ = spice_frame;
        this->rotation_source_ = TransformSource::Spice;

        this->update_spice_transform_();
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_spice(const std::string& spice_origin, const std::string& spice_frame)
    {
        if (scene_->is_locked()) {
            HUIRA_THROW_ERROR(this->get_info_() + " - set_spice() was called with a locked scene");
        }

        if (parent_) {
            if (parent_->position_source_ != TransformSource::Spice) {
                HUIRA_THROW_ERROR(this->get_info_() + " - cannot set SPICE origin: parent node (" +
                    parent_->get_info_() + ") has manually set position");
            }

            if (parent_->rotation_source_ != TransformSource::Spice) {
                HUIRA_THROW_ERROR(this->get_info_() + " - cannot set SPICE frame: parent node (" +
                    parent_->get_info_() + ") has manually set rotation");
            }
        }

        HUIRA_LOG_INFO(this->get_info_() + " - set_spice('" + spice_origin + ", " + spice_frame + "')");

        this->spice_origin_ = spice_origin;
        this->position_source_ = TransformSource::Spice;

        this->spice_frame_ = spice_frame;
        this->rotation_source_ = TransformSource::Spice;

        this->update_spice_transform_();
    }


    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    std::weak_ptr<Node<TSpectral, TFloat>> Node<TSpectral, TFloat>::new_child()
    {
        if (scene_->is_locked()) {
            HUIRA_THROW_ERROR(this->get_info_() + " - new_child() was called with a locked scene");
        }


        auto child = std::make_shared<Node<TSpectral, TFloat>>(scene_);
        child->set_parent_(this);

        HUIRA_LOG_INFO(this->get_info_() + " - new child added: " + child->get_info_());

        children_.push_back(child);
        return child;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::delete_child(std::weak_ptr<Node<TSpectral, TFloat>> child_weak)
    {
        if (scene_->is_locked()) {
            HUIRA_THROW_ERROR(this->get_info_() + " - delete_child() was called with a locked scene");
        }

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
        if (scene_->is_locked()) {
            HUIRA_THROW_ERROR(this->get_info_() + " - change_parent() was called with a locked scene");
        }

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
        if (this->position_source_ == TransformSource::Spice) {
            if (new_parent->position_source_ != TransformSource::Spice) {
                HUIRA_THROW_ERROR(this->get_info_() + " - cannot change parent: node uses SPICE for position " +
                    "(spice_origin_=" + spice_origin_ + ") but new parent (" +
                    new_parent->get_info_() + ") has manually set position");
            }
        }

        // Check SPICE constraints for rotation
        if (this->rotation_source_ == TransformSource::Spice) {
            if (new_parent->rotation_source_ != TransformSource::Spice) {
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
        if (this->position_source_ == TransformSource::Spice || this->rotation_source_ == TransformSource::Spice) {
            this->update_spice_transform_();
        }
        else {
            this->update_global_transform_();
        }
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    std::weak_ptr<UnresolvedObject<TSpectral, TFloat>> Node<TSpectral, TFloat>::new_unresolved_object()
    {
        if (scene_->is_locked()) {
            HUIRA_THROW_ERROR(this->get_info_() + " - new_unresolved() was called with a locked scene");
        }


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
            if (child->position_source_ == TransformSource::Spice) {
                return child;
            }
        }
        return nullptr;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    std::shared_ptr<Node<TSpectral, TFloat>> Node<TSpectral, TFloat>::child_spice_frames_() const
    {
        for (const auto& child : children_) {
            if (child->rotation_source_ == TransformSource::Spice) {
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

        if (this->position_source_ == TransformSource::Spice) {
            std::array<TFloat, 6> state = spice::spkezr<TFloat>(
                this->spice_origin_,
                scene_->get_time(),
                scene_->root.get_spice_frame(),
                scene_->root.get_spice_origin()
            );
            this->global_transform_.position = Vec3<TFloat>{ state[0], state[1], state[2] };
            this->global_transform_.velocity = Vec3<TFloat>{ state[3], state[4], state[5] };

            this->local_transform_.position =
                parent_->global_transform_.rotation.inverse() *
                (this->global_transform_.position - parent_->global_transform_.position);
            this->local_transform_.velocity =
                parent_->global_transform_.rotation.inverse() *
                (this->global_transform_.velocity - parent_->global_transform_.velocity);
        }

        if (this->rotation_source_ == TransformSource::Spice) {
            this->global_transform_.rotation = spice::pxform<TFloat>(
                this->spice_frame_,
                scene_->root.get_spice_frame(),
                scene_->get_time()
            );

            this->local_transform_.rotation =
                parent_->global_transform_.rotation.inverse() * this->global_transform_.rotation;

            // TODO: angular velocity
        }

        // Process-children:
        for (auto& child : children_) {
            child->update_global_transform_();
        }
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::update_all_spice_transforms_()
    {
        if (parent_ == nullptr) {
            for (auto& child : children_) {
                child->update_all_spice_transforms_();
            }
            return;
        }

        if (this->position_source_ == TransformSource::Spice) {
            std::array<TFloat, 6> state = spice::spkezr<TFloat>(
                this->spice_origin_,
                scene_->get_time(),
                scene_->root.get_spice_frame(),
                scene_->root.get_spice_origin()
            );
            this->global_transform_.position = Vec3<TFloat>{ state[0], state[1], state[2] };
            this->global_transform_.velocity = Vec3<TFloat>{ state[3], state[4], state[5] };

            // Back-compute local for completeness
            this->local_transform_.position =
                parent_->global_transform_.rotation.inverse() *
                (this->global_transform_.position - parent_->global_transform_.position);
            this->local_transform_.velocity =
                parent_->global_transform_.rotation.inverse() *
                (this->global_transform_.velocity - parent_->global_transform_.velocity);
        }
        else {
            this->global_transform_.position =
                parent_->global_transform_.position +
                parent_->global_transform_.rotation * this->local_transform_.position;
            this->global_transform_.velocity =
                parent_->global_transform_.velocity +
                parent_->global_transform_.rotation * this->local_transform_.velocity;
        }

        if (this->rotation_source_ == TransformSource::Spice) {
            this->global_transform_.rotation = spice::pxform<TFloat>(
                this->spice_frame_,
                scene_->root_node_->spice_frame_,
                scene_->get_time()
            );
            // TODO: angular velocity

            // Back-compute local for completeness
            this->local_transform_.rotation =
                parent_->global_transform_.rotation.inverse() * this->global_transform_.rotation;
        }
        else {
            this->global_transform_.rotation =
                parent_->global_transform_.rotation * this->local_transform_.rotation;
            // TODO: angular velocity
        }

        this->global_transform_.scale =
            parent_->global_transform_.scale * this->local_transform_.scale;

        for (auto& child : children_) {
            child->update_all_spice_transforms_();
        }
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::update_global_transform_()
    {
        // Compute global from parent (only called for manual nodes)
        if (this->parent_) {
            if (this->position_source_ == TransformSource::Spice) {
                this->local_transform_.position =
                    parent_->global_transform_.rotation.inverse() *
                    (this->global_transform_.position - parent_->global_transform_.position);
                this->local_transform_.velocity =
                    parent_->global_transform_.rotation.inverse() *
                    (this->global_transform_.velocity - parent_->global_transform_.velocity);
            }
            else {
                this->global_transform_.position =
                    parent_->global_transform_.position +
                    parent_->global_transform_.rotation * this->local_transform_.position;
                this->global_transform_.velocity =
                    parent_->global_transform_.velocity +
                    parent_->global_transform_.rotation * this->local_transform_.velocity;
            }

            if (this->rotation_source_ == TransformSource::Spice) {
                this->local_transform_.rotation =
                    parent_->global_transform_.rotation.inverse() * this->global_transform_.rotation;
                // TODO: angular velocity
            }
            else {
                this->global_transform_.rotation =
                    parent_->global_transform_.rotation * this->local_transform_.rotation;
                // TODO: angular velocity
            }

            this->global_transform_.scale =
                parent_->global_transform_.scale * this->local_transform_.scale;
        }

        // Recursively propagate to all children (handles both SPICE and Manual)
        for (auto& child : children_) {
            child->update_global_transform_();
        }
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    std::string Node<TSpectral, TFloat>::get_info_() {
        std::string identifier_str = "[" + std::to_string(this->id()) + "]";

        std::string info = "Node" + identifier_str;
        return info;
    }
}
