#include <memory>
#include <string>

#include "huira/core/time.hpp"
#include "huira/core/types.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/detail/logger.hpp"
#include "huira/detail/validate.hpp"
#include "huira/spice/spice_states.hpp"

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
            HUIRA_THROW_ERROR(this->get_info() +
                " - cannot manually set position when child has a spice_origin_ (see child "
                + spice_child->get_info() + ")");
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
            HUIRA_THROW_ERROR(this->get_info() +
                " - cannot manually set rotation when child has a spice_frame_ (see child "
                + spice_child->get_info() + ")");
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
        HUIRA_LOG_INFO(this->get_info() + " - set_scale(" +
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
            HUIRA_THROW_ERROR(this->get_info() + " - cannot manually set velocity when node uses SPICE for position " +
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
            HUIRA_THROW_ERROR(this->get_info() + " - cannot manually set angular velocity when node uses SPICE for rotation " +
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
        HUIRA_LOG_INFO(this->get_info() + " - set_spice_origin('" + spice_origin + "')");

        this->spice_origin_ = spice_origin;
        this->position_source_ = TransformSource::SPICE_TRANSFORM;
        this->update_spice_transform_();
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::set_spice_frame(const std::string& spice_frame)
    {
        validate_scene_unlocked_("set_spice_frame()");
        validate_spice_frame_allowed_();
        HUIRA_LOG_INFO(this->get_info() + " - set_spice_frame('" + spice_frame + "')");

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
        HUIRA_LOG_INFO(this->get_info() + " - set_spice('" + spice_origin + ", " + spice_frame + "')");

        this->spice_origin_ = spice_origin;
        this->spice_frame_ = spice_frame;
        this->position_source_ = TransformSource::SPICE_TRANSFORM;
        this->rotation_source_ = TransformSource::SPICE_TRANSFORM;
        this->update_spice_transform_();
    }



    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    std::string Node<TSpectral, TFloat>::get_info() {
        std::string info = get_type_name() + "[" + std::to_string(this->id()) + "]";
        return info;
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

        // Notify derived classes (FrameNode will propagate to children)
        on_transform_changed_();
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::update_all_spice_transforms_()
    {
        // For leaf nodes, just update this node's transform
        // FrameNode overrides this to also propagate to children
        if (parent_ == nullptr) {
            // Root node: nothing to compute for self, children handled by FrameNode override
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
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::update_global_transform_()
    {
        // Compute global from parent
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

        // Notify derived classes (FrameNode will propagate to children)
        on_transform_changed_();
    }


    // ========================= //
    // === Protected Members === //
    // ========================= //

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::validate_scene_unlocked_(const std::string function_name)
    {
        if (scene_->is_locked()) {
            HUIRA_THROW_ERROR(this->get_info() + " - " + function_name + " was called with a locked scene");
        }
    };

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::validate_spice_origin_allowed_()
    {
        if (parent_) {
            if (parent_->position_source_ != TransformSource::SPICE_TRANSFORM) {
                HUIRA_THROW_ERROR(this->get_info() + " - cannot set SPICE origin: parent node (" +
                    parent_->get_info() + ") has manually set position");
            }
        }
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Node<TSpectral, TFloat>::validate_spice_frame_allowed_()
    {
        if (parent_) {
            if (parent_->rotation_source_ != TransformSource::SPICE_TRANSFORM) {
                HUIRA_THROW_ERROR(this->get_info() + " - cannot set SPICE frame: parent node (" +
                    parent_->get_info() + ") has manually set rotation");
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
