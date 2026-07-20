#pragma once

#include <cmath>

#include "glm/glm.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/rotation.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/core/types.hpp"
#include "huira/scene/node.hpp"
#include "huira/scene/state_callbacks/state_callbacks.hpp"

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
 * @tparam TSpectral The spectral representation type.
 */
template <IsSpectral TSpectral>
class LookAtCallbackBase : public RotationCallback {
  public:
    LookAtCallbackBase(const Node<TSpectral>* self, const Vec3<double>& up_vector, bool is_blender)
        : self_(self), up_(up_vector), is_blender_(is_blender)
    {
    }

    void evaluate(const Time& t_emit,
                  const Vec3<double>& local_pos,
                  const Vec3<double>& local_vel) override
    {
        this->rotation = compute_local_rotation_(t_emit, local_pos);

        // Central finite difference of the orientation for a continuous angular
        // velocity (needed for accurate motion blur). The +/- samples advance
        // both time and the linearly extrapolated local position.
        constexpr double epsilon = 1e-4;
        const Time t_plus = Time::from_et(t_emit.et() + epsilon);
        const Time t_minus = Time::from_et(t_emit.et() - epsilon);
        const Vec3<double> pos_plus = local_pos + local_vel * epsilon;
        const Vec3<double> pos_minus = local_pos - local_vel * epsilon;

        const Rotation<double> R_plus = compute_local_rotation_(t_plus, pos_plus);
        const Rotation<double> R_minus = compute_local_rotation_(t_minus, pos_minus);
        const Rotation<double> delta_R = R_plus * R_minus.inverse();

        const double angle = delta_R.angle().to_si();
        if (std::abs(angle) < 1e-12) {
            this->angular_velocity = Vec3<double>{0.0, 0.0, 0.0};
        } else {
            this->angular_velocity = delta_R.axis() * (angle / (2.0 * epsilon));
        }
    }

  protected:
    /// World-space (SSB) position of the aim target at time @p t. This is the
    /// only thing that differs between the tracked-node and fixed-point variants.
    virtual Vec3<double> resolve_target_position_(const Time& t) const = 0;

  private:
    /// Below this squared cross-product length the forward/up pair is treated as
    /// parallel. forward is unit, so this is ~ (|up| * sin(theta))^2; with a unit
    /// up it corresponds to theta < ~1e-6 rad.
    static constexpr double kDegenerateFrameEps2 = 1e-12;

    /// Builds the node's local orientation at time @p t, given its local position.
    Rotation<double> compute_local_rotation_(const Time& t, const Vec3<double>& pos) const
    {
        // Resolve this node's world position at time t (through its parent, if any).
        Vec3<double> current_ssb_pos;
        Transform<double> parent_ssb;
        const bool has_parent = self_->get_parent().valid();
        if (has_parent) {
            parent_ssb = self_->get_parent().get()->get_ssb_transform_(t, t);
            current_ssb_pos = parent_ssb.apply_to_point(pos);
        } else {
            current_ssb_pos = pos;
        }

        const Vec3<double> forward = glm::normalize(resolve_target_position_(t) - current_ssb_pos);

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
 * followed continuously (including across the motion-blur interval).
 *
 * @tparam TSpectral The spectral representation type.
 */
template <IsSpectral TSpectral>
class LookAtCallback : public LookAtCallbackBase<TSpectral> {
  public:
    LookAtCallback(const Node<TSpectral>* self,
                   const Node<TSpectral>* target,
                   const Vec3<double>& up_vector,
                   bool is_blender)
        : LookAtCallbackBase<TSpectral>(self, up_vector, is_blender), target_(target)
    {
    }

  protected:
    Vec3<double> resolve_target_position_(const Time& t) const override
    {
        return target_->get_ssb_transform_(t, t).position;
    }

  private:
    const Node<TSpectral>* target_;
};

/**
 * @brief Orients a node toward a fixed world-space position.
 * @tparam TSpectral The spectral representation type.
 */
template <IsSpectral TSpectral>
class LookAtPositionCallback : public LookAtCallbackBase<TSpectral> {
  public:
    LookAtPositionCallback(const Node<TSpectral>* self,
                           const Vec3<double>& target_position,
                           const Vec3<double>& up_vector,
                           bool is_blender)
        : LookAtCallbackBase<TSpectral>(self, up_vector, is_blender),
          target_position_(target_position)
    {
    }

  protected:
    Vec3<double> resolve_target_position_(const Time& /*t*/) const override
    {
        return target_position_;
    }

  private:
    Vec3<double> target_position_;
};

} // namespace huira
