#include "huira/core/rotation.hpp"
#include "huira/scene/state_callbacks/state_callbacks.hpp"

namespace huira {

template <IsSpectral TSpectral>
class LookAtCallback : public RotationCallback {
  public:
    LookAtCallback(const Node<TSpectral>* self,
                   const Node<TSpectral>* target,
                   const Vec3<double>& up_vector,
                   bool is_blender)
        : self_(self), target_(target), up_(up_vector), is_blender_(is_blender)
    {
    }

    void evaluate(const Time& t_emit,
                  const Vec3<double>& local_pos,
                  const Vec3<double>& local_vel) override
    {
        auto compute_rotation_at = [&](const Time& t, const Vec3<double>& pos) -> Rotation<double> {
            Transform<double> target_ssb = target_->get_ssb_transform_(t, t);

            Vec3<double> current_ssb_pos;
            Transform<double> parent_ssb;

            if (self_->get_parent().valid()) {
                parent_ssb = self_->get_parent().get()->get_ssb_transform_(t, t);
                current_ssb_pos = parent_ssb.apply_to_point(pos);
            } else {
                current_ssb_pos = pos;
            }

            // Determine forward direction in world space
            Vec3<double> forward = glm::normalize(target_ssb.position - current_ssb_pos);

            // Compute basis vectors based on camera convention
            Vec3<double> right, true_up, z_axis;
            if (is_blender_) {
                z_axis = -forward;
                right = glm::normalize(glm::cross(forward, up_));
                true_up = glm::cross(z_axis, right);
            } else {
                z_axis = forward;
                right = glm::normalize(glm::cross(forward, up_));
                true_up = glm::cross(z_axis, right);
            }

            // Construct global rotation directly from basis vectors
            Rotation<double> world_rotation =
                Rotation<double>::from_basis_vectors(right, true_up, z_axis);

            // Convert world rotation to local rotation relative to parent
            Rotation<double> local_rotation;
            if (self_->get_parent().valid()) {
                local_rotation = parent_ssb.rotation.inverse() * world_rotation;
            } else {
                local_rotation = world_rotation;
            }
            return local_rotation;
        };

        this->rotation = compute_rotation_at(t_emit, local_pos);
        const double epsilon = 1e-4;

        Time t_plus = Time::from_et(t_emit.et() + epsilon);
        Time t_minus = Time::from_et(t_emit.et() - epsilon);

        // Approximate local positions at t +/- epsilon
        Vec3<double> pos_plus = local_pos + (local_vel * epsilon);
        Rotation<double> R_plus = compute_rotation_at(t_plus, pos_plus);

        Vec3<double> pos_minus = local_pos - (local_vel * epsilon);
        Rotation<double> R_minus = compute_rotation_at(t_minus, pos_minus);
        Rotation<double> delta_R = R_plus * R_minus.inverse();

        double angle = delta_R.angle().to_si();
        Vec3<double> axis = delta_R.axis();

        if (std::abs(angle) < 1e-12) {
            this->angular_velocity = Vec3<double>{0.0, 0.0, 0.0};
        } else {
            double dt = 2.0 * epsilon; // Total time difference
            this->angular_velocity = axis * (angle / dt);
        }
    }

  private:
    const Node<TSpectral>* self_;
    const Node<TSpectral>* target_;
    Vec3<double> up_;
    bool is_blender_;
};

template <IsSpectral TSpectral>
class LookAtPositionCallback : public RotationCallback {
  public:
    LookAtPositionCallback(const Node<TSpectral>* self,
                           const Vec3<double>& target_position,
                           const Vec3<double>& up_vector,
                           bool is_blender)
        : self_(self), target_position_(target_position), up_(up_vector), is_blender_(is_blender)
    {
    }

    void evaluate(const Time& t_emit,
                  const Vec3<double>& local_pos,
                  const Vec3<double>& local_vel) override
    {
        auto compute_rotation_at = [&](const Time& t, const Vec3<double>& pos) -> Rotation<double> {
            Vec3<double> current_ssb_pos;
            Transform<double> parent_ssb;

            // Resolve our current global position
            if (self_->get_parent().valid()) {
                parent_ssb = self_->get_parent().get()->get_ssb_transform_(t, t);
                current_ssb_pos = parent_ssb.apply_to_point(pos);
            } else {
                current_ssb_pos = pos;
            }

            // Target is a fixed static position
            Vec3<double> forward = glm::normalize(target_position_ - current_ssb_pos);

            // Compute basis vectors
            Vec3<double> right, true_up, z_axis;
            if (is_blender_) {
                z_axis = -forward;
                right = glm::normalize(glm::cross(forward, up_));
                true_up = glm::cross(z_axis, right);
            } else {
                z_axis = forward;
                right = glm::normalize(glm::cross(forward, up_));
                true_up = glm::cross(z_axis, right);
            }

            Rotation<double> world_rotation =
                Rotation<double>::from_basis_vectors(right, true_up, z_axis);

            // Convert world rotation to local rotation
            Rotation<double> local_rotation;
            if (self_->get_parent().valid()) {
                local_rotation = parent_ssb.rotation.inverse() * world_rotation;
            } else {
                local_rotation = world_rotation;
            }
            return local_rotation;
        };

        this->rotation = compute_rotation_at(t_emit, local_pos);

        const double epsilon = 1e-4;
        Time t_plus = Time::from_et(t_emit.et() + epsilon);
        Vec3<double> pos_plus = local_pos + (local_vel * epsilon);

        Time t_minus = Time::from_et(t_emit.et() - epsilon);
        Vec3<double> pos_minus = local_pos - (local_vel * epsilon);

        Rotation<double> R_plus = compute_rotation_at(t_plus, pos_plus);
        Rotation<double> R_minus = compute_rotation_at(t_minus, pos_minus);
        Rotation<double> delta_R = R_plus * R_minus.inverse();

        double angle = delta_R.angle().to_si();
        Vec3<double> axis = delta_R.axis();

        if (std::abs(angle) < 1e-12) {
            this->angular_velocity = Vec3<double>{0.0, 0.0, 0.0};
        } else {
            double dt = 2.0 * epsilon;
            this->angular_velocity = axis * (angle / dt);
        }
    }

  private:
    const Node<TSpectral>* self_;
    Vec3<double> target_position_;
    Vec3<double> up_;
    bool is_blender_;
};

} // namespace huira
