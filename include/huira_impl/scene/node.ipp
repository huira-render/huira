#include <memory>
#include <string>

#include "glm/glm.hpp"

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/ephemeris/spice.hpp"
#include "huira/core/physics.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/core/types.hpp"
#include "huira/util/logger.hpp"
#include "huira/util/validate.hpp"


namespace huira {

    /**
     * @brief Construct a Node and assign a unique ID.
     * @param scene Pointer to the owning Scene
     */
    template <IsSpectral TSpectral>
    Node<TSpectral>::Node(Scene<TSpectral>* scene)
        : id_(next_id_++), scene_(scene)
    {
    }


    /**
     * @brief Set the node's position manually.
     * @param position Position vector in meters
     */
    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_position(const Vec3<double>& position)
    {
        if (!this->position_can_be_manual_()) {
            HUIRA_THROW_ERROR(this->get_info() +
                " - cannot manually set position when child has a spice_origin");
        }

        this->local_transform_.position = position;
        this->position_mode_ = TransformMode::MANUAL_TRANSFORM;
        this->spice_origin_ = "";
    }


    /**
     * @brief Set the node's position manually using unit types.
     * @param x X position (meters)
     * @param y Y position (meters)
     * @param z Z position (meters)
     */
    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_position(units::Meter x, units::Meter y, units::Meter z)
    {
        if (!this->position_can_be_manual_()) {
            HUIRA_THROW_ERROR(this->get_info() +
                " - cannot manually set position when child has a spice_origin");
        }

        HUIRA_LOG_INFO(this->get_info() + " - set_position(" +
            std::to_string(x.to_si()) + ", " +
            std::to_string(y.to_si()) + ", " +
            std::to_string(z.to_si()) + ")");

        this->local_transform_.position = Vec3<double>{
            x.to_si(), y.to_si(), z.to_si()
        };
        this->position_mode_ = TransformMode::MANUAL_TRANSFORM;
        this->spice_origin_ = "";
    }


    /**
     * @brief Set the node's rotation manually.
     * @param rotation Rotation object
     */
    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_rotation(const Rotation<double>& rotation)
    {
        if (!this->rotation_can_be_manual_()) {
            HUIRA_THROW_ERROR(this->get_info() +
                " - cannot manually set rotation when child has a spice_frame");
        }

        HUIRA_LOG_INFO(this->get_info() + " - set_rotation(" + rotation.to_string());

        this->local_transform_.rotation = rotation;
        this->rotation_mode_ = TransformMode::MANUAL_TRANSFORM;
        this->spice_frame_ = "";
    }



    /**
     * @brief Set the node's scale.
     * @param scale Scale vector
     */
    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_scale(const Vec3<double>& scale)
    {
        HUIRA_LOG_INFO(this->get_info() + " - set_scale(" +
            std::to_string(scale[0]) + ", " +
            std::to_string(scale[1]) + ", " +
            std::to_string(scale[2]) + ")");

        this->local_transform_.scale = scale;
    }


    /**
     * @brief Set the node's scale using individual values.
     * @param sx X scale
     * @param sy Y scale
     * @param sz Z scale
     */
    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_scale(double sx, double sy, double sz)
    {
        HUIRA_LOG_INFO(this->get_info() + " - set_scale(" +
            std::to_string(sx) + ", " +
            std::to_string(sy) + ", " +
            std::to_string(sz) + ")");

        this->local_transform_.scale = Vec3<double>{ sx, sy, sz };
    }



    /**
     * @brief Set the node's velocity manually.
     * @param velocity Velocity vector in m/s
     */
    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_velocity(const Vec3<double>& velocity)
    {
        if (this->position_mode_ != TransformMode::MANUAL_TRANSFORM) {
            HUIRA_THROW_ERROR(this->get_info() + " - cannot manually set velocity when node does not use manual position");
        }

        this->local_transform_.velocity = velocity;
    }


    /**
     * @brief Set the node's velocity manually using unit types.
     * @param vx X velocity (m/s)
     * @param vy Y velocity (m/s)
     * @param vz Z velocity (m/s)
     */
    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_velocity(units::MetersPerSecond vx, units::MetersPerSecond vy, units::MetersPerSecond vz)
    {
        if (this->position_mode_ != TransformMode::MANUAL_TRANSFORM) {
            HUIRA_THROW_ERROR(this->get_info() + " - cannot manually set velocity when node does not use manual position");
        }

        HUIRA_LOG_INFO(this->get_info() + " - set_velocity(" +
            std::to_string(vx.to_si()) + ", " +
            std::to_string(vy.to_si()) + ", " +
            std::to_string(vz.to_si()) + ")");

        this->local_transform_.velocity = Vec3<double>{ vx.to_si(), vy.to_si(), vz.to_si() };
    }



    /**
     * @brief Set the node's angular velocity manually.
     * @param angular_velocity Angular velocity vector in rad/s
     */
    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_angular_velocity(const Vec3<double>& angular_velocity)
    {
        if (this->rotation_mode_ != TransformMode::MANUAL_TRANSFORM) {
            HUIRA_THROW_ERROR(this->get_info() + " - cannot manually set angular velocity when node does not use manual rotation");
        }

        this->local_transform_.angular_velocity = angular_velocity;
    }


    /**
     * @brief Set the node's angular velocity manually using unit types.
     * @param wx X angular velocity (rad/s)
     * @param wy Y angular velocity (rad/s)
     * @param wz Z angular velocity (rad/s)
     */
    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_angular_velocity(units::RadiansPerSecond wx, units::RadiansPerSecond wy, units::RadiansPerSecond wz)
    {
        if (this->rotation_mode_ != TransformMode::MANUAL_TRANSFORM) {
            HUIRA_THROW_ERROR(this->get_info() + " - cannot manually set angular velocity when node does not use manual rotation");
        }

        HUIRA_LOG_INFO(this->get_info() + " - set_angular_velocity(" +
            std::to_string(wx.to_si()) + ", " +
            std::to_string(wy.to_si()) + ", " +
            std::to_string(wz.to_si()) + ")");

        this->local_transform_.angular_velocity = Vec3<double>{ wx.to_si(), wy.to_si(), wz.to_si() };
    }


    /**
     * @brief Set the node's SPICE origin for ephemeris-based transforms.
     * @param spice_origin SPICE origin string
     */
    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_spice_origin(const std::string& spice_origin)
    {
        if (!position_can_be_spice_()) {
            HUIRA_THROW_ERROR(this->get_info() + " - cannot set SPICE progom: parent node (" +
                parent_->get_info() + ") has manually set position");
        }
        HUIRA_LOG_INFO(this->get_info() + " - set_spice_origin('" + spice_origin + "')");

        this->spice_origin_ = spice_origin;
        this->position_mode_ = TransformMode::SPICE_TRANSFORM;
    }


    /**
     * @brief Set the node's SPICE frame for rotation-based transforms.
     * @param spice_frame SPICE frame string
     */
    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_spice_frame(const std::string& spice_frame)
    {
        if (!rotation_can_be_spice_()) {
            HUIRA_THROW_ERROR(this->get_info() + " - cannot set SPICE frame: parent node (" +
                parent_->get_info() + ") has manually set rotation");
        }
        HUIRA_LOG_INFO(this->get_info() + " - set_spice_frame('" + spice_frame + "')");

        this->spice_frame_ = spice_frame;
        this->rotation_mode_ = TransformMode::SPICE_TRANSFORM;
    }


    /**
     * @brief Set both SPICE origin and frame for the node.
     * @param spice_origin SPICE origin string
     * @param spice_frame SPICE frame string
     */
    template <IsSpectral TSpectral>
    void Node<TSpectral>::set_spice(const std::string& spice_origin, const std::string& spice_frame)
    {
        if (!position_can_be_spice_()) {
            HUIRA_THROW_ERROR(this->get_info() + " - cannot set SPICE progom: parent node (" +
                parent_->get_info() + ") has manually set position");
        }
        if (!rotation_can_be_spice_()) {
            HUIRA_THROW_ERROR(this->get_info() + " - cannot set SPICE frame: parent node (" +
                parent_->get_info() + ") has manually set rotation");
        }
        HUIRA_LOG_INFO(this->get_info() + " - set_spice('" + spice_origin + ", " + spice_frame + "')");

        this->spice_origin_ = spice_origin;
        this->spice_frame_ = spice_frame;
        this->position_mode_ = TransformMode::SPICE_TRANSFORM;
        this->rotation_mode_ = TransformMode::SPICE_TRANSFORM;
    }



    /**
     * @brief Get the apparent transform of the node for a given observation mode and time.
     * @param obs_mode Observation mode (true, geometric, aberrated)
     * @param t_obs Observation time
     * @param observer_ssb_state Observer's SSB transform
     * @return Transform<double> Apparent transform
     */
    template <IsSpectral TSpectral>
    Transform<double> Node<TSpectral>::get_apparent_transform(ObservationMode obs_mode, const Time& t_obs, const Transform<double>& observer_ssb_state) const
    {
        bool iterate = (obs_mode != ObservationMode::TRUE_STATE);
        auto [apparent_state, _] = get_geometric_state_(t_obs, observer_ssb_state, iterate);

        if (obs_mode == ObservationMode::ABERRATED_STATE) {
            // Geometric Direction:
            Vec3<double> P_ssb = apparent_state.position;
            Vec3<double> P_obs = observer_ssb_state.position;
            Vec3<double> P_rel = P_ssb - P_obs;

            double dist = glm::length(P_rel);

            // Safety check for degenerate geometry:
            if (dist > 1e-8) {
                Vec3<double> u = P_rel / dist;
                Vec3<double> u_app = compute_aberrated_direction(u, observer_ssb_state.velocity);
                apparent_state.position = P_obs + (u_app * dist);
            }
        }

        return apparent_state;
    }


    /**
     * @brief Get the geometric state (transform and light time) of the node.
     * @param t_obs Observation time
     * @param observer_ssb_state Observer's SSB transform
     * @param iterate Whether to iterate for aberration
     * @param tol Tolerance for iteration
     * @return std::pair<Transform<double>, double> {transform, light time}
     */
    template <IsSpectral TSpectral>
    std::pair<Transform<double>, double> Node<TSpectral>::get_geometric_state_(const Time& t_obs, const Transform<double>& observer_ssb_state, bool iterate, double tol) const
    {
        if (!iterate) {
            return { this->get_ssb_transform_(t_obs), 0.0 };
        }

        Transform<double> full_ssb_transform = this->get_ssb_transform_(t_obs);
        double dt = glm::length(observer_ssb_state.position - full_ssb_transform.position) / SPEED_OF_LIGHT<double>();
        for (std::size_t i = 0; i < 10; ++i) {
            full_ssb_transform = this->get_ssb_transform_(t_obs, dt);

            const double new_dt = glm::length(observer_ssb_state.position - full_ssb_transform.position) / SPEED_OF_LIGHT<double>();

            if (std::abs(new_dt - dt) < tol) {
                dt = new_dt;
                break;
            }

            dt = new_dt;

        }

        return { full_ssb_transform, dt };
    }


    /**
     * @brief Get the node's transform in the Solar System Barycenter (SSB) frame.
     * @param t_obs Observation time
     * @param dt Light time delay
     * @return Transform<double> SSB transform
     */
    template <IsSpectral TSpectral>
    Transform<double> Node<TSpectral>::get_ssb_transform_(const Time& t_obs, double dt) const
    {
        // The time at which the object emitted the light we are seeing now.
        Time t_emit = Time::from_et(t_obs.et() - dt);

        Transform<double> ssb_state{};

        if (position_mode_ == TransformMode::SPICE_TRANSFORM) {
            auto [pos, vel, _] = spice::spkezr<double>(this->spice_origin_, t_emit, "J2000", "NONE", "SSB");
            ssb_state.position = pos;
            ssb_state.velocity = vel;
        }
        else {
            // Recurse for non-spice position:
            if (!parent_) {
                // This should never happen
                HUIRA_THROW_ERROR(this->get_info() +
                    " - cannot compute SSB transform: node has MANUAL position but no parent");
            }
            Transform<double> parent_ssb = parent_->get_ssb_transform_(t_obs, dt);
            Transform<double> local = this->get_local_position_at_(t_obs, dt);
            ssb_state = parent_ssb * local;
        }

        if (rotation_mode_ == TransformMode::SPICE_TRANSFORM) {
            auto [rotation, ang_vel] = spice::sxform<double>("J2000", this->spice_frame_, t_emit);
            ssb_state.rotation = rotation;
            ssb_state.angular_velocity = ang_vel;
        }
        else {
            // Recurse for non-spice rotation:
            Transform<double> parent_ssb_rot;
            if (!parent_) {
                // This should never happen
                HUIRA_THROW_ERROR(this->get_info() +
                    " - cannot compute SSB transform: node has MANUAL rotation but no parent");
            }
            parent_ssb_rot = parent_->get_ssb_transform_(t_obs, dt);

            Transform<double> local = this->get_local_rotation_at_(t_obs, dt);
            ssb_state.rotation = parent_ssb_rot.rotation * local.rotation;
            ssb_state.angular_velocity = parent_ssb_rot.angular_velocity + (parent_ssb_rot.rotation * local.angular_velocity);
        }

        return ssb_state;
    }


    /**
     * @brief Get the node's local position transform at a given time.
     * @param t_obs Observation time
     * @param dt Light time delay
     * @return Transform<double> Local position transform
     */
    template <IsSpectral TSpectral>
    Transform<double> Node<TSpectral>::get_local_position_at_(const Time& t_obs, double dt) const
    {
        (void)t_obs; // Not used for manual.  Would be used for custom ball back functions.
        Transform<double> local_transform_at_time{};
        if (position_mode_ == TransformMode::MANUAL_TRANSFORM) {
            local_transform_at_time.position = local_transform_.position - dt * local_transform_.velocity;
            local_transform_at_time.velocity = local_transform_.velocity;
        }
        else {
            HUIRA_THROW_ERROR("get_local_position_at_ - Unknown position_mode_ TransformMode");
        }
        return local_transform_at_time;
    }


    /**
     * @brief Get the node's local rotation transform at a given time.
     * @param t_obs Observation time
     * @param dt Light time delay
     * @return Transform<double> Local rotation transform
     */
    template <IsSpectral TSpectral>
    Transform<double> Node<TSpectral>::get_local_rotation_at_(const Time& t_obs, double dt) const
    {
        (void)t_obs; // Not used for manual.  Would be used for custom ball back functions.
        Transform<double> local_transform_at_time{};
        if (rotation_mode_ == TransformMode::MANUAL_TRANSFORM) {
            // Approximate rotation by treat angular_velocity * dt as euler angles
            Vec3<double> euler_angles = local_transform_.angular_velocity * dt;
            Rotation<double> delta_rotation = Rotation<double>::intrinsic_euler_angles(
                units::Radian{ euler_angles[0] },
                units::Radian{ euler_angles[1] },
                units::Radian{ euler_angles[2] });
            local_transform_at_time.rotation = delta_rotation * local_transform_.rotation;
            local_transform_at_time.angular_velocity = local_transform_.angular_velocity;
        }
        else {
            HUIRA_THROW_ERROR("get_local_rotation_at_ - Unknown rotation_mode_ TransformMode");
        }
        return local_transform_at_time;
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
    bool Node<TSpectral>::position_can_be_spice_() const
    {
        if (parent_) {
            if (parent_->position_mode_ != TransformMode::SPICE_TRANSFORM) {
                return false;
            }
        }
        return true;
    }

    template <IsSpectral TSpectral>
    bool Node<TSpectral>::rotation_can_be_spice_() const
    {
        if (parent_) {
            if (parent_->rotation_mode_ != TransformMode::SPICE_TRANSFORM) {
                return false;
            }
        }
        return true;
    }


    /**
     * @brief Gets a handle to the parent node.
     *
     * Returns a base NodeHandle to the parent. This always returns the parent as a base
     * Node type, even if the parent is actually a more specific type like FrameNode.
     *
     * @return NodeHandle<TSpectral, Node<TSpectral>> Handle to the parent node
     * @throws std::runtime_error If this node has no parent (e.g., root node)
     */
    template <IsSpectral TSpectral>
    NodeHandle<TSpectral, Node<TSpectral>> Node<TSpectral>::get_parent() const
    {
        if (!parent_) {
            HUIRA_THROW_ERROR(this->get_info() + " - node has no parent");
        }

        // Find the shared_ptr for the parent from the scene
        // We need to search through the scene graph to find the shared_ptr
        auto parent_shared = scene_->find_node_shared_ptr_(parent_);
        
        if (!parent_shared) {
            HUIRA_THROW_ERROR(this->get_info() + " - failed to find parent's shared_ptr");
        }

        return NodeHandle<TSpectral, Node<TSpectral>>(parent_shared);
    }


    /**
     * @brief Gets a handle to the parent node with a specific type.
     *
     * Returns a handle to the parent cast to the specified node type. This performs
     * a dynamic cast to verify the parent is actually of the requested type at runtime.
     *
     * @tparam TParentNode The expected type of the parent node (e.g., FrameNode<TSpectral>)
     * @return NodeHandle<TSpectral, TParentNode> Handle to the parent with the specified type
     * @throws std::runtime_error If this node has no parent
     * @throws std::runtime_error If the parent is not of type TParentNode
     */
    template <IsSpectral TSpectral>
    template <typename TParentNode>
    NodeHandle<TSpectral, TParentNode> Node<TSpectral>::get_parent_as() const
    {
        if (!parent_) {
            HUIRA_THROW_ERROR(this->get_info() + " - node has no parent");
        }

        // Verify that parent is of the requested type
        TParentNode* typed_parent = dynamic_cast<TParentNode*>(parent_);
        if (!typed_parent) {
            HUIRA_THROW_ERROR(this->get_info() + " - parent is not of the requested type");
        }

        // Find the shared_ptr for the parent from the scene
        auto parent_shared = scene_->find_node_shared_ptr_(parent_);
        
        if (!parent_shared) {
            HUIRA_THROW_ERROR(this->get_info() + " - failed to find parent's shared_ptr");
        }

        // Cast the shared_ptr to the specific type
        auto typed_parent_shared = std::dynamic_pointer_cast<TParentNode>(parent_shared);
        if (!typed_parent_shared) {
            HUIRA_THROW_ERROR(this->get_info() + " - failed to cast parent to requested type");
        }

        return NodeHandle<TSpectral, TParentNode>(typed_parent_shared);
    }
}
