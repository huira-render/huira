#include <memory>
#include <string>

#include "huira/core/spice.hpp"
#include "huira/core/time.hpp"
#include "huira/core/types.hpp"

#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/detail/logger.hpp"
#include "huira/detail/validate.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    Node<TSpectral>::Node(Scene<TSpectral>* scene)
        : id_(next_id_++), scene_(scene)
    {

    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_position(const Vec3<double>& position)
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

    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_rotation(const Rotation<double>& rotation)
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

    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_scale(const Vec3<double>& scale)
    {
        validate_scene_unlocked_("set_scale()");
        HUIRA_LOG_INFO(this->get_info() + " - set_scale(" +
            std::to_string(scale[0]) + ", " +
            std::to_string(scale[1]) + ", " +
            std::to_string(scale[2]) + ")");

        this->local_transform_.scale = scale;
        this->update_global_transform_();
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_velocity(const Vec3<double>& velocity)
    {
        validate_scene_unlocked_("set_velocity()");
        if (this->position_source_ == TransformSource::SPICE_TRANSFORM) {
            HUIRA_THROW_ERROR(this->get_info() + " - cannot manually set velocity when node uses SPICE for position " +
                "(spice_origin_=" + spice_origin_ + ")");
        }

        this->local_transform_.velocity = velocity;
        this->update_global_transform_();
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_angular_velocity(const Vec3<double>& angular_velocity)
    {
        validate_scene_unlocked_("set_angular_velocity()");
        if (this->rotation_source_ == TransformSource::SPICE_TRANSFORM) {
            HUIRA_THROW_ERROR(this->get_info() + " - cannot manually set angular velocity when node uses SPICE for rotation " +
                "(spice_frame_=" + spice_frame_ + ")");
        }

        this->local_transform_.angular_velocity = angular_velocity;
        this->update_global_transform_();
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_spice_origin(const std::string& spice_origin)
    {
        validate_scene_unlocked_("set_spice_origin()");
        validate_spice_origin_allowed_();
        HUIRA_LOG_INFO(this->get_info() + " - set_spice_origin('" + spice_origin + "')");

        this->spice_origin_ = spice_origin;
        this->position_source_ = TransformSource::SPICE_TRANSFORM;
        this->update_spice_transform_();
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_spice_frame(const std::string& spice_frame)
    {
        validate_scene_unlocked_("set_spice_frame()");
        validate_spice_frame_allowed_();
        HUIRA_LOG_INFO(this->get_info() + " - set_spice_frame('" + spice_frame + "')");

        this->spice_frame_ = spice_frame;
        this->rotation_source_ = TransformSource::SPICE_TRANSFORM;
        this->update_spice_transform_();
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_spice(const std::string& spice_origin, const std::string& spice_frame)
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



    template <IsSpectral TSpectral>
    std::string Node<TSpectral>::get_info() const {
        std::string info = get_type_name() + "[" + std::to_string(this->id()) + "]";
        return info;
    }



    /**
     * @brief Get position relative to a SPICE origin in a SPICE frame
     * @param target_origin SPICE body to measure position relative to (e.g., "SSB", "EARTH", "SUN")
     * @param target_frame SPICE reference frame for the result (e.g., "J2000", "ECLIPJ2000")
     * @return Position vector in the target frame relative to target origin
     * @throws std::runtime_error if no SPICE-enabled ancestor is found in scene graph
     */
    template <IsSpectral TSpectral>
    Vec3<double> Node<TSpectral>::get_position_in_frame(const std::string& target_origin, const std::string& target_frame) const
    {
        auto [pos, vel] = get_state_in_frame(target_origin, target_frame);
        return pos;
    }

    /**
     * @brief Get velocity relative to a SPICE origin in a SPICE frame
     * @param target_origin SPICE body to measure velocity relative to (e.g., "SSB", "EARTH", "SUN")
     * @param target_frame SPICE reference frame for the result (e.g., "J2000", "ECLIPJ2000")
     * @return Velocity vector in the target frame relative to target origin
     * @throws std::runtime_error if no SPICE-enabled ancestor is found in scene graph
     */
    template <IsSpectral TSpectral>
    Vec3<double> Node<TSpectral>::get_velocity_in_frame(const std::string& target_origin, const std::string& target_frame) const
    {
        auto [pos, vel] = get_state_in_frame(target_origin, target_frame);
        return vel;
    }

    /**
     * @brief Get rotation relative to a SPICE frame
     * @param target_frame SPICE reference frame (e.g., "J2000", "ECLIPJ2000", "IAU_EARTH")
     * @return Rotation from target frame to this node's orientation
     * @throws std::runtime_error if no SPICE-enabled ancestor is found in scene graph
     */
    template <IsSpectral TSpectral>
    Rotation<double> Node<TSpectral>::get_rotation_in_frame(const std::string& target_frame) const
    {
        auto [rot, ang_vel] = get_attitude_in_frame(target_frame);
        return rot;
    }

    /**
     * @brief Get angular velocity relative to a SPICE frame
     * @param target_frame SPICE reference frame (e.g., "J2000", "ECLIPJ2000", "IAU_EARTH")
     * @return Angular velocity vector in the target frame
     * @throws std::runtime_error if no SPICE-enabled ancestor is found in scene graph
     */
    template <IsSpectral TSpectral>
    Vec3<double> Node<TSpectral>::get_angular_velocity_in_frame(const std::string& target_frame) const
    {
        auto [rot, ang_vel] = get_attitude_in_frame(target_frame);
        return ang_vel;
    }

    /**
     * @brief Get complete state (position + velocity) relative to a SPICE origin and frame
     * @param target_origin SPICE body to measure state relative to
     * @param target_frame SPICE reference frame for the result
     * @return Pair of (position, velocity) in the target frame relative to target origin
     * @throws std::runtime_error if no SPICE-enabled ancestor is found in scene graph
     */
    template <IsSpectral TSpectral>
    std::pair<Vec3<double>, Vec3<double>> Node<TSpectral>::get_state_in_frame(
        const std::string& target_origin,
        const std::string& target_frame) const
    {
        // Find the first SPICE-enabled ancestor
        auto [spice_ancestor, accumulated_transform] = find_spice_origin_ancestor_();

        // Get the SPICE ancestor's state in the target frame
        auto [spice_pos, spice_vel, lt] = spice::spkezr<double>(
            spice_ancestor->get_spice_origin(),
            scene_->get_time(),
            target_frame,
            target_origin
        );

        // We need the rotation from target_frame to the SPICE ancestor's frame
        // to properly transform the accumulated offset
        auto [frame_rotation, frame_ang_vel] = spice::sxform<double>(
            target_frame,
            spice_ancestor->get_spice_frame(),
            scene_->get_time()
        );

        // Transform accumulated position: rotate from ancestor frame to target frame
        Vec3<double> position = spice_pos + frame_rotation.inverse() * accumulated_transform.position;

        // Transform accumulated velocity: rotate and account for frame rotation
        // v_target = v_spice + R^-1 * v_accumulated
        // Note: We're not adding rotational velocity contribution (omega x r) since we're
        // assuming the accumulated velocity is already in the proper reference
        Vec3<double> velocity = spice_vel + frame_rotation.inverse() * accumulated_transform.velocity;

        return { position, velocity };
    }

    /**
     * @brief Get complete attitude (rotation + angular velocity) relative to a SPICE frame
     * @param target_frame SPICE reference frame for the result
     * @return Pair of (rotation, angular_velocity) in the target frame
     * @throws std::runtime_error if no SPICE-enabled ancestor is found in scene graph
     */
    template <IsSpectral TSpectral>
    std::pair<Rotation<double>, Vec3<double>> Node<TSpectral>::get_attitude_in_frame(const std::string& target_frame) const
    {
        // Find the first SPICE-enabled ancestor
        auto [spice_ancestor, accumulated_rotation_data] = find_spice_frame_ancestor_();
        auto [accumulated_rotation, accumulated_ang_vel] = accumulated_rotation_data;

        // Get the rotation from target_frame to the SPICE ancestor's frame
        auto [spice_rotation, spice_ang_vel] = spice::sxform<double>(
            target_frame,
            spice_ancestor->get_spice_frame(),
            scene_->get_time()
        );

        // Compose rotations: R_total = R_spice * R_accumulated
        // This gives rotation from target_frame to this node's frame
        Rotation<double> rotation = spice_rotation * accumulated_rotation;

        // Transform angular velocity to target frame
        // ω_target = R_spice^-1 * (ω_spice + ω_accumulated)
        Vec3<double> angular_velocity = spice_rotation.inverse() * (spice_ang_vel + accumulated_ang_vel);

        return { rotation, angular_velocity };
    }


    /**
     * @brief Find the first ancestor (including self) with a SPICE origin
     * @return Pair of (ancestor node, accumulated transform from this to ancestor)
     * @throws std::runtime_error if no SPICE origin found in ancestry
     */
    template <IsSpectral TSpectral>
    std::pair<const Node<TSpectral>*, Transform<double>> Node<TSpectral>::find_spice_origin_ancestor_() const
    {
        Transform<double> accumulated;
        accumulated.position = Vec3<double>{ 0, 0, 0 };
        accumulated.velocity = Vec3<double>{ 0, 0, 0 };
        accumulated.rotation = Rotation<double>{};
        accumulated.scale = Vec3<double>{ 1, 1, 1 };

        const Node<TSpectral>* current = this;

        // Walk up the scene graph
        while (current != nullptr) {
            // Check if this node has a SPICE origin
            if (current->position_source_ == TransformSource::SPICE_TRANSFORM &&
                !current->spice_origin_.empty()) {
                return { current, accumulated };
            }

            // If this is not the starting node, accumulate its transform
            if (current != this) {
                // Accumulate position in parent's frame
                accumulated.position = current->local_transform_.position +
                    current->local_transform_.rotation * accumulated.position;

                // Accumulate velocity
                accumulated.velocity = current->local_transform_.velocity +
                    current->local_transform_.rotation * accumulated.velocity;

                // Accumulate rotation
                accumulated.rotation = current->local_transform_.rotation * accumulated.rotation;

                // Accumulate scale (component-wise multiplication)
                accumulated.scale = current->local_transform_.scale * accumulated.scale;
            }
            else {
                // For the starting node, initialize with its local transform
                accumulated = this->local_transform_;
            }

            // Move to parent
            current = current->parent_;
        }

        // No SPICE origin found in the entire ancestry chain
        HUIRA_THROW_ERROR(this->get_info() +
            " - cannot query SPICE frame: no ancestor with SPICE origin found in scene graph");
    }

    /**
     * @brief Find the first ancestor (including self) with a SPICE frame
     * @return Pair of (ancestor node, accumulated rotation from this to ancestor)
     * @throws std::runtime_error if no SPICE frame found in ancestry
     */
    template <IsSpectral TSpectral>
    std::pair<const Node<TSpectral>*, std::pair<Rotation<double>, Vec3<double>>> Node<TSpectral>::find_spice_frame_ancestor_() const
    {
        Rotation<double> accumulated_rotation = Rotation<double>{};
        Vec3<double> accumulated_ang_vel{ 0, 0, 0 };

        const Node<TSpectral>* current = this;

        // Walk up the scene graph
        while (current != nullptr) {
            // Check if this node has a SPICE frame
            if (current->rotation_source_ == TransformSource::SPICE_TRANSFORM &&
                !current->spice_frame_.empty()) {
                return { current, {accumulated_rotation, accumulated_ang_vel} };
            }

            // Accumulate rotation
            if (current != this) {
                // Compose rotations going up the tree
                accumulated_rotation = current->local_transform_.rotation * accumulated_rotation;

                // Accumulate angular velocity
                // ω_total = ω_parent + R_parent * ω_child
                accumulated_ang_vel = current->local_transform_.angular_velocity +
                    current->local_transform_.rotation * accumulated_ang_vel;
            }
            else {
                // For the starting node, initialize with its local transform
                accumulated_rotation = this->local_transform_.rotation;
                accumulated_ang_vel = this->local_transform_.angular_velocity;
            }

            // Move to parent
            current = current->parent_;
        }

        // No SPICE frame found in the entire ancestry chain
        HUIRA_THROW_ERROR(this->get_info() +
            " - cannot query SPICE frame: no ancestor with SPICE rotation frame found in scene graph");
    }



    template <IsSpectral TSpectral>
    void Node<TSpectral>::update_spice_transform_()
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

    template <IsSpectral TSpectral>
    void Node<TSpectral>::update_all_spice_transforms_()
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

    template <IsSpectral TSpectral>
    void Node<TSpectral>::update_global_transform_()
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

    template <IsSpectral TSpectral>
    void Node<TSpectral>::validate_scene_unlocked_(const std::string function_name)
    {
        if (scene_->is_locked()) {
            HUIRA_THROW_ERROR(this->get_info() + " - " + function_name + " was called with a locked scene");
        }
    };

    template <IsSpectral TSpectral>
    void Node<TSpectral>::validate_spice_origin_allowed_()
    {
        if (parent_) {
            if (parent_->position_source_ != TransformSource::SPICE_TRANSFORM) {
                HUIRA_THROW_ERROR(this->get_info() + " - cannot set SPICE origin: parent node (" +
                    parent_->get_info() + ") has manually set position");
            }
        }
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::validate_spice_frame_allowed_()
    {
        if (parent_) {
            if (parent_->rotation_source_ != TransformSource::SPICE_TRANSFORM) {
                HUIRA_THROW_ERROR(this->get_info() + " - cannot set SPICE frame: parent node (" +
                    parent_->get_info() + ") has manually set rotation");
            }
        }
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::compute_global_spice_position_()
    {
        auto [position, velocity, _] = spice::spkezr<double>(
            this->spice_origin_, scene_->get_time(),
            scene_->root.get_spice_frame(), scene_->root.get_spice_origin()
        );
        this->global_transform_.position = position;
        this->global_transform_.velocity = velocity;
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::compute_global_spice_rotation_()
    {
        auto [rotation, angular_velocity] = spice::sxform<double>(
            this->spice_frame_, scene_->root.get_spice_frame(), scene_->get_time()
        );
        this->global_transform_.rotation = rotation;
        this->global_transform_.angular_velocity = angular_velocity;
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::compute_local_position_from_global_()
    {
        this->local_transform_.position =
            parent_->global_transform_.rotation.inverse() *
            (this->global_transform_.position - parent_->global_transform_.position);

        this->local_transform_.velocity =
            parent_->global_transform_.rotation.inverse() *
            (this->global_transform_.velocity - parent_->global_transform_.velocity);
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::compute_local_rotation_from_global_()
    {
        this->local_transform_.rotation =
            parent_->global_transform_.rotation.inverse() * this->global_transform_.rotation;

        this->local_transform_.angular_velocity =
            parent_->global_transform_.rotation.inverse() *
            (this->global_transform_.angular_velocity - parent_->global_transform_.angular_velocity);
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::compute_global_position_from_local_()
    {
        this->global_transform_.position =
            parent_->global_transform_.position +
            parent_->global_transform_.rotation * this->local_transform_.position;
        this->global_transform_.velocity =
            parent_->global_transform_.velocity +
            parent_->global_transform_.rotation * this->local_transform_.velocity;
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::compute_global_rotation_from_local_()
    {
        this->global_transform_.rotation =
            parent_->global_transform_.rotation * this->local_transform_.rotation;

        this->global_transform_.angular_velocity =
            parent_->global_transform_.angular_velocity +
            parent_->global_transform_.rotation * this->local_transform_.angular_velocity;
    }

}
