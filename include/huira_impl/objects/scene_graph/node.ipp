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
        if (auto spice_child = child_spice_origins_()) {
            HUIRA_THROW_ERROR(this->get_info() +
                " - cannot manually set position when child has a spice_origin_ (see child "
                + spice_child->get_info() + ")");
        }

        this->local_transform_.position = position;
        this->position_mode_ = TransformMode::MANUAL_TRANSFORM;
        this->spice_origin_ = "";
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_rotation(const Rotation<double>& rotation)
    {
        if (auto spice_child = child_spice_frames_()) {
            HUIRA_THROW_ERROR(this->get_info() +
                " - cannot manually set rotation when child has a spice_frame_ (see child "
                + spice_child->get_info() + ")");
        }

        this->local_transform_.rotation = rotation;
        this->rotation_mode_ = TransformMode::MANUAL_TRANSFORM;
        this->spice_frame_ = "";
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_scale(const Vec3<double>& scale)
    {
        HUIRA_LOG_INFO(this->get_info() + " - set_scale(" +
            std::to_string(scale[0]) + ", " +
            std::to_string(scale[1]) + ", " +
            std::to_string(scale[2]) + ")");

        this->local_transform_.scale = scale;
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_velocity(const Vec3<double>& velocity)
    {
        if (this->position_mode_ == TransformMode::SPICE_TRANSFORM) {
            HUIRA_THROW_ERROR(this->get_info() + " - cannot manually set velocity when node uses SPICE for position " +
                "(spice_origin_=" + spice_origin_ + ")");
        }

        this->local_transform_.velocity = velocity;
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_angular_velocity(const Vec3<double>& angular_velocity)
    {
        if (this->rotation_mode_ == TransformMode::SPICE_TRANSFORM) {
            HUIRA_THROW_ERROR(this->get_info() + " - cannot manually set angular velocity when node uses SPICE for rotation " +
                "(spice_frame_=" + spice_frame_ + ")");
        }

        this->local_transform_.angular_velocity = angular_velocity;
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_spice_origin(const std::string& spice_origin)
    {
        validate_spice_origin_allowed_();
        HUIRA_LOG_INFO(this->get_info() + " - set_spice_origin('" + spice_origin + "')");

        this->spice_origin_ = spice_origin;
        this->position_mode_ = TransformMode::SPICE_TRANSFORM;
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_spice_frame(const std::string& spice_frame)
    {
        validate_spice_frame_allowed_();
        HUIRA_LOG_INFO(this->get_info() + " - set_spice_frame('" + spice_frame + "')");

        this->spice_frame_ = spice_frame;
        this->rotation_mode_ = TransformMode::SPICE_TRANSFORM;
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_spice(const std::string& spice_origin, const std::string& spice_frame)
    {
        validate_spice_origin_allowed_();
        validate_spice_frame_allowed_();
        HUIRA_LOG_INFO(this->get_info() + " - set_spice('" + spice_origin + ", " + spice_frame + "')");

        this->spice_origin_ = spice_origin;
        this->spice_frame_ = spice_frame;
        this->position_mode_ = TransformMode::SPICE_TRANSFORM;
        this->rotation_mode_ = TransformMode::SPICE_TRANSFORM;
    }



    template <IsSpectral TSpectral>
    std::string Node<TSpectral>::get_info() const {
        std::string info = get_type_name() + "[" + std::to_string(this->id()) + "]";
        return info;
    }



    template <IsSpectral TSpectral>
    Vec3<double> Node<TSpectral>::get_static_position() const
    {
        if (position_mode_ != TransformMode::MANUAL_TRANSFORM) {
            HUIRA_THROW_ERROR(this->get_info() +
                " - cannot get static position when position mode is not MANUAL_TRANSFORM");
        }
        return local_transform_.position;
    }
    template <IsSpectral TSpectral>
    Rotation<double> Node<TSpectral>::get_static_rotation() const
    {
        if (position_mode_ != TransformMode::MANUAL_TRANSFORM) {
            HUIRA_THROW_ERROR(this->get_info() +
                " - cannot get static rotation when rotation mode is not MANUAL_TRANSFORM");
        }
        return local_transform_.rotation;
    }
    template <IsSpectral TSpectral>
    Vec3<double> Node<TSpectral>::get_static_scale() const
    {
        return local_transform_.scale;
    }
    template <IsSpectral TSpectral>
    Vec3<double> Node<TSpectral>::get_static_velocity() const
    {
        if (position_mode_ != TransformMode::MANUAL_TRANSFORM) {
            HUIRA_THROW_ERROR(this->get_info() +
                " - cannot get static velocity when position mode is not MANUAL_TRANSFORM");
        }
        return local_transform_.velocity;
    }
    template <IsSpectral TSpectral>
    Vec3<double> Node<TSpectral>::get_static_angular_velocity() const
    {
        if (position_mode_ != TransformMode::MANUAL_TRANSFORM) {
            HUIRA_THROW_ERROR(this->get_info() +
                " - cannot get static angular velocity when rotation mode is not MANUAL_TRANSFORM");
        }
        return local_transform_.angular_velocity;
    }


    template <IsSpectral TSpectral>
    std::string Node<TSpectral>::get_spice_origin() const {
        if (position_mode_ != TransformMode::SPICE_TRANSFORM) {
            HUIRA_THROW_ERROR(this->get_info() +
                " - cannot get spice origin when position mode is not SPICE_TRANSFORM");
        }
        return spice_origin_;
    }

    template <IsSpectral TSpectral>
    std::string Node<TSpectral>::get_spice_frame() const
    {
        if (position_mode_ != TransformMode::SPICE_TRANSFORM) {
            HUIRA_THROW_ERROR(this->get_info() +
                " - cannot get spice frame when rotation mode is not SPICE_TRANSFORM");
        }
        return spice_frame_;
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
            if (current->position_mode_ == TransformMode::SPICE_TRANSFORM &&
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
            if (current->rotation_mode_ == TransformMode::SPICE_TRANSFORM &&
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


    // ========================= //
    // === Protected Members === //
    // ========================= //
    template <IsSpectral TSpectral>
    void Node<TSpectral>::validate_spice_origin_allowed_()
    {
        if (parent_) {
            if (parent_->position_mode_ != TransformMode::SPICE_TRANSFORM) {
                HUIRA_THROW_ERROR(this->get_info() + " - cannot set SPICE origin: parent node (" +
                    parent_->get_info() + ") has manually set position");
            }
        }
    }

    template <IsSpectral TSpectral>
    void Node<TSpectral>::validate_spice_frame_allowed_()
    {
        if (parent_) {
            if (parent_->rotation_mode_ != TransformMode::SPICE_TRANSFORM) {
                HUIRA_THROW_ERROR(this->get_info() + " - cannot set SPICE frame: parent node (" +
                    parent_->get_info() + ") has manually set rotation");
            }
        }
    }
}
