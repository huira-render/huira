#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

#include "glm/glm.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/physics.hpp"
#include "huira/core/rotation.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/core/types.hpp"
#include "huira/scene/node.hpp"
#include "huira/scene/state_callbacks/state_callbacks.hpp"
#include "huira/util/logger.hpp"

namespace huira {

/**
 * @brief Shared implementation of the "orient toward a target" rotation callbacks.
 *
 * Both look-at variants compute an identical orientation and finite-difference
 * angular velocity; they differ only in how the aim target's world position is
 * resolved at a given time (a tracked Node vs. a fixed point). That single
 * difference is the pure-virtual resolve_target_position_(); everything else --
 * parent-frame resolution, basis construction, the world->local conversion, and
 * the central-difference angular velocity for motion blur -- lives here.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class LookAtCallbackBase : public RotationCallback {
  public:
    LookAtCallbackBase(const Node<TSpectral>* self, const Vec3<double>& up_vector, bool is_blender)
        : self_(self), up_(up_vector), is_blender_(is_blender)
    {
    }

    [[nodiscard]] RotationState evaluate(const PoseContext& context,
                                         const PositionState& state) const override
    {
        const CycleGuard_ guard(this);
        const Vec3<double>& local_pos = state.position;
        const Vec3<double>& local_vel = state.velocity;
        RotationState result;
        result.rotation = compute_local_rotation_(context, local_pos, local_vel);

        // The angular velocity, by a central finite difference of the orientation, from which
        // the velocities of the node's children follow. (Motion blur does not use it: the scene
        // view evaluates the orientation itself at each temporal sample.) The +/- samples
        // advance both time and the linearly extrapolated local position.
        constexpr double epsilon = 1e-4;
        const PoseContext plus = context.at(Time::from_et(context.time.et() + epsilon));
        const PoseContext minus = context.at(Time::from_et(context.time.et() - epsilon));
        const Vec3<double> pos_plus = local_pos + local_vel * epsilon;
        const Vec3<double> pos_minus = local_pos - local_vel * epsilon;

        const Rotation<double> R_plus = compute_local_rotation_(plus, pos_plus, local_vel);
        const Rotation<double> R_minus = compute_local_rotation_(minus, pos_minus, local_vel);
        const Rotation<double> delta_R = R_plus * R_minus.inverse();

        const double angle = delta_R.angle().to_si();
        if (std::abs(angle) < 1e-12) {
            result.angular_velocity = Vec3<double>{0.0, 0.0, 0.0};
        } else {
            result.angular_velocity = delta_R.axis() * (angle / (2.0 * epsilon));
        }
        return result;
    }

  protected:
    /// World-space (SSB) position to aim at, as seen by @p observer: the looking node's own SSB
    /// state (its position and velocity) in @p context. @p parent is the node's parent's SSB
    /// transform (the identity if it has none). This is the only thing that differs between the
    /// tracked-node and fixed-point variants.
    virtual Vec3<double> resolve_target_position_(const PoseContext& context,
                                                  const Transform<double>& observer,
                                                  const Transform<double>& parent) const = 0;

  private:
    /**
     * @brief Marks a look-at callback as being evaluated on this thread, and throws if it already
     * is: its target's position then depends on its own node's orientation, as when two nodes look
     * at each other, directly or through others, and evaluating it would recurse until the stack
     * overflowed. Poses are evaluated on one thread each, so the mark is the thread's own.
     */
    class CycleGuard_ {
      public:
        explicit CycleGuard_(const LookAtCallbackBase* callback)
        {
            std::vector<const LookAtCallbackBase*>& active = active_();
            if (std::find(active.begin(), active.end(), callback) != active.end()) {
                HUIRA_THROW_ERROR(
                    callback->self_->get_info() +
                    " - look_at: where its target is depends on how this node is turned, so it "
                    "cannot be aimed at it. Its targets lead back to it: nodes look at each "
                    "other, directly or through others.");
            }
            active.push_back(callback);
        }
        ~CycleGuard_() { active_().pop_back(); }

        CycleGuard_(const CycleGuard_&) = delete;
        CycleGuard_& operator=(const CycleGuard_&) = delete;

      private:
        static std::vector<const LookAtCallbackBase*>& active_()
        {
            thread_local std::vector<const LookAtCallbackBase*> active;
            return active;
        }
    };

    /// Below this squared cross-product length the forward/up pair is treated as
    /// parallel. forward is unit, so this is ~ (|up| * sin(theta))^2; with a unit
    /// up it corresponds to theta < ~1e-6 rad.
    static constexpr double kDegenerateFrameEps2 = 1e-12;

    /// Builds the node's local orientation in @p context, given its local position and velocity.
    Rotation<double> compute_local_rotation_(const PoseContext& context,
                                             const Vec3<double>& pos,
                                             const Vec3<double>& vel) const
    {
        // This node's SSB state (through its parent, if any), placed as the scene view places
        // it. Its position and velocity do not depend on its own rotation, which is what is
        // being found.
        Transform<double> local;
        local.position = pos;
        local.velocity = vel;
        Transform<double> parent_ssb;
        Transform<double> self_ssb = local;
        const bool has_parent = self_->get_parent().valid();
        if (has_parent) {
            parent_ssb = self_->get_parent().get()->get_ssb_transform_(context);
            self_ssb = parent_ssb * local;
        }

        const Vec3<double> forward = glm::normalize(
            resolve_target_position_(context, self_ssb, parent_ssb) - self_ssb.position);

        // Right vector, guarded against the classic look-at singularity where the
        // requested up is (near-)parallel to forward -- e.g. aiming straight along
        // the up axis. There the cross product collapses to ~0 and normalizing it
        // would yield NaN, so fall back to a world axis that cannot be parallel to
        // forward. Roll is genuinely undefined in that pose, so any consistent
        // choice is acceptable. (A zero-length up degenerates the same way and is
        // caught by the same test.)
        Vec3<double> up = up_;
        Vec3<double> right_unnorm = glm::cross(forward, up);
        if (glm::dot(right_unnorm, right_unnorm) < kDegenerateFrameEps2) {
            up = (std::abs(forward.x) < 0.9) ? Vec3<double>{1.0, 0.0, 0.0}
                                             : Vec3<double>{0.0, 1.0, 0.0};
            right_unnorm = glm::cross(forward, up);
        }
        const Vec3<double> right = glm::normalize(right_unnorm);

        // z runs opposite the view direction under Blender's camera convention,
        // along it otherwise. right is always derived from forward (not z), so the
        // handedness matches the original per-convention construction.
        const Vec3<double> z_axis = is_blender_ ? -forward : forward;
        const Vec3<double> true_up = glm::cross(z_axis, right);

        const Rotation<double> world_rotation =
            Rotation<double>::from_basis_vectors(right, true_up, z_axis);

        // Convert the world orientation into the node's parent-relative frame.
        return has_parent ? (parent_ssb.rotation.inverse() * world_rotation) : world_rotation;
    }

    const Node<TSpectral>* self_;
    Vec3<double> up_;
    bool is_blender_;
};

/**
 * @brief Orients a node to track another (possibly moving) node.
 *
 * The target is re-resolved at every evaluated time, so a moving target is
 * followed continuously (including across the motion-blur interval). It is placed as the scene
 * view places it (poses set by hand hold at its reference time), and aimed at as seen from this
 * node in an observation mode: by default the scene view's own, so that a camera keeps the
 * target where it renders it, or one given to look_at.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class LookAtCallback : public LookAtCallbackBase<TSpectral> {
  public:
    LookAtCallback(const Node<TSpectral>* self,
                   const Node<TSpectral>& target,
                   const Vec3<double>& up_vector,
                   bool is_blender,
                   std::optional<ObservationMode> observation_mode = std::nullopt)
        : LookAtCallbackBase<TSpectral>(self, up_vector, is_blender),
          target_(target.weak_from_this()), observation_mode_(observation_mode)
    {
    }

  protected:
    /// The target is held weakly: it may be deleted from the scene while this node still looks
    /// at it, and is then reported rather than read from freed memory.
    Vec3<double> resolve_target_position_(const PoseContext& context,
                                          const Transform<double>& observer,
                                          const Transform<double>& /*parent*/) const override
    {
        const std::shared_ptr<const Node<TSpectral>> target = target_.lock();
        if (!target || !target->is_scene_owned()) {
            HUIRA_THROW_ERROR("look_at - The node this one looks at has been deleted from the "
                              "scene. Call look_at() again with another target, or set the "
                              "node's rotation.");
        }
        return target
            ->get_apparent_transform(observation_mode_.value_or(context.observation_mode),
                                     context.reference,
                                     context.time,
                                     observer)
            .position;
    }

  private:
    std::weak_ptr<const Node<TSpectral>> target_;

    /// The observation mode to aim in, or none for the scene view's.
    std::optional<ObservationMode> observation_mode_;
};

/**
 * @brief Orients a node toward a fixed point.
 *
 * The point is in the node's parent's frame (TargetFrame::PARENT), so it moves with the parent,
 * or in the scene's root frame (TargetFrame::ROOT). It is aimed at as seen from the node in an
 * observation mode: where it is (TRUE_STATE, the default; GEOMETRIC_STATE is the same, a point
 * having no light time), or shifted by the aberration of the node's velocity (ABERRATED_STATE),
 * or, given no mode, in the scene view's.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class LookAtPositionCallback : public LookAtCallbackBase<TSpectral> {
  public:
    LookAtPositionCallback(
        const Node<TSpectral>* self,
        const Vec3<double>& target_position,
        const Vec3<double>& up_vector,
        bool is_blender,
        TargetFrame frame = TargetFrame::PARENT,
        std::optional<ObservationMode> observation_mode = ObservationMode::TRUE_STATE)
        : LookAtCallbackBase<TSpectral>(self, up_vector, is_blender),
          target_position_(target_position), frame_(frame), observation_mode_(observation_mode)
    {
    }

  protected:
    Vec3<double> resolve_target_position_(const PoseContext& context,
                                          const Transform<double>& observer,
                                          const Transform<double>& parent) const override
    {
        const Vec3<double> point = frame_ == TargetFrame::PARENT
                                       ? parent.apply_to_point(target_position_)
                                       : target_position_;
        if (observation_mode_.value_or(context.observation_mode) !=
            ObservationMode::ABERRATED_STATE) {
            return point;
        }
        // As Node::get_apparent_transform() aberrates a body: the direction to the point, seen
        // by the moving node, at the same distance.
        const Vec3<double> relative = point - observer.position;
        const double distance = glm::length(relative);
        if (!(distance > 1e-8)) {
            return point;
        }
        return observer.position +
               compute_aberrated_direction(relative / distance, observer.velocity) * distance;
    }

  private:
    Vec3<double> target_position_;
    TargetFrame frame_;

    /// The observation mode to aim in, or none for the scene view's.
    std::optional<ObservationMode> observation_mode_;
};

} // namespace huira
