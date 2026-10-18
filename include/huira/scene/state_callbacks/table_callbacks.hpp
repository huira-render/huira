#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "huira/core/rotation.hpp"
#include "huira/core/time.hpp"
#include "huira/core/types.hpp"
#include "huira/scene/state_callbacks/state_callbacks.hpp"
#include "huira/util/logger.hpp"
#include "huira/util/validate.hpp"

namespace huira {

namespace detail {

/// Checks a table's times (at least two, each later than the one before) and returns them as
/// ephemeris times.
inline std::vector<double>
table_times(const std::vector<Time>& times, std::size_t entries, const std::string& what)
{
    if (times.size() < 2) {
        HUIRA_THROW_ERROR(what + " - A table needs at least two entries, and has " +
                          std::to_string(times.size()));
    }
    if (entries != times.size()) {
        HUIRA_THROW_ERROR(what + " - The table has " + std::to_string(times.size()) +
                          " times and " + std::to_string(entries) + " values");
    }
    std::vector<double> ets(times.size());
    for (std::size_t i = 0; i < times.size(); ++i) {
        ets[i] = times[i].et();
        validate_real(ets[i], what + " table time");
        if (i > 0 && !(ets[i] > ets[i - 1])) {
            HUIRA_THROW_ERROR(what +
                              " - The table's times must each be later than the one before: " +
                              times[i].to_iso_8601() + " follows " + times[i - 1].to_iso_8601());
        }
    }
    return ets;
}

/// The entry that starts the interval of a table holding an ephemeris time, and how far through
/// it the time is (from 0 to 1). @throws std::runtime_error if the time is outside the table.
inline std::pair<std::size_t, double>
table_interval(const std::vector<double>& ets, const Time& time, const std::string& what)
{
    const double et = time.et();
    if (!(et >= ets.front() && et <= ets.back())) {
        HUIRA_THROW_ERROR(
            what + " - Its table runs from " + Time::from_et(ets.front()).to_iso_8601() + " to " +
            Time::from_et(ets.back()).to_iso_8601() + ", and has nothing at " + time.to_iso_8601() +
            ". (With light time, a scene view evaluates poses earlier than the "
            "exposure, by the time light takes to reach the camera.)");
    }
    const auto later = std::upper_bound(ets.begin(), ets.end(), et);
    const std::size_t i =
        std::min(static_cast<std::size_t>(later - ets.begin()), ets.size() - 1) - 1;
    return {i, (et - ets[i]) / (ets[i + 1] - ets[i])};
}

} // namespace detail

/**
 * @brief A node's position from a table of times and positions, with or without velocities, in
 * its parent's frame.
 *
 * Between two entries, the position follows the cubic curve through both that has their
 * velocities there (cubic Hermite interpolation), so the position and velocity are continuous.
 * Velocities that are not given are estimated at each entry from it and its neighbors, by the
 * quadratic through the three (at the ends, through the first or last three): a table taken from
 * motion that is quadratic over each three entries is followed exactly. With two entries, the
 * position runs along the straight line between them.
 *
 * A time outside the table is an error: nothing is extrapolated.
 */
class PositionTableCallback : public PositionCallback {
  public:
    /**
     * @param times When each entry is: at least two, each later than the one before.
     * @param positions The position at each time, in meters.
     * @param velocities The velocity at each time, in meters per second, or none to estimate them.
     * @param what What the position is of, for errors (the node's name).
     * @throws std::runtime_error if there are fewer than two entries, the times are out of order,
     *         the sizes differ, or a value is not finite.
     */
    PositionTableCallback(const std::vector<Time>& times,
                          std::vector<Vec3<double>> positions,
                          std::vector<Vec3<double>> velocities,
                          std::string what)
        : positions_(std::move(positions)), velocities_(std::move(velocities)),
          what_(std::move(what))
    {
        times_ = detail::table_times(times, positions_.size(), what_);
        for (const Vec3<double>& position : positions_) {
            validate_real(position, what_ + " position in its table");
        }
        if (velocities_.empty()) {
            velocities_ = estimate_velocities_();
        } else if (velocities_.size() != positions_.size()) {
            HUIRA_THROW_ERROR(what_ + " - The table has " + std::to_string(positions_.size()) +
                              " positions and " + std::to_string(velocities_.size()) +
                              " velocities");
        }
        for (const Vec3<double>& velocity : velocities_) {
            validate_real(velocity, what_ + " velocity in its table");
        }
    }

    [[nodiscard]] PositionState evaluate(const PoseContext& context,
                                         const RotationState& /*rotation*/) const override
    {
        const auto [i, s] = detail::table_interval(times_, context.time, what_);
        const double dt = times_[i + 1] - times_[i];
        const Vec3<double>& p0 = positions_[i];
        const Vec3<double>& p1 = positions_[i + 1];
        const Vec3<double> m0 = velocities_[i] * dt;
        const Vec3<double> m1 = velocities_[i + 1] * dt;

        const double s2 = s * s;
        const double s3 = s2 * s;
        PositionState state;
        state.position = (2.0 * s3 - 3.0 * s2 + 1.0) * p0 + (s3 - 2.0 * s2 + s) * m0 +
                         (3.0 * s2 - 2.0 * s3) * p1 + (s3 - s2) * m1;
        state.velocity = ((6.0 * s2 - 6.0 * s) * (p0 - p1) + (3.0 * s2 - 4.0 * s + 1.0) * m0 +
                          (3.0 * s2 - 2.0 * s) * m1) /
                         dt;
        return state;
    }

  private:
    std::vector<double> times_;
    std::vector<Vec3<double>> positions_;
    std::vector<Vec3<double>> velocities_;
    std::string what_;

    /// The velocity at each entry, by the derivative of the quadratic through it and its
    /// neighbors (at the ends, the first or last three entries), or of the line between two.
    std::vector<Vec3<double>> estimate_velocities_() const
    {
        const std::size_t n = times_.size();
        std::vector<Vec3<double>> velocities(n);
        if (n == 2) {
            velocities[0] = velocities[1] =
                (positions_[1] - positions_[0]) / (times_[1] - times_[0]);
            return velocities;
        }
        // The derivative at x of the quadratic through (a, pa), (b, pb), (c, pc):
        auto derivative = [](double a,
                             double b,
                             double c,
                             const Vec3<double>& pa,
                             const Vec3<double>& pb,
                             const Vec3<double>& pc,
                             double x) {
            return pa * ((2.0 * x - b - c) / ((a - b) * (a - c))) +
                   pb * ((2.0 * x - a - c) / ((b - a) * (b - c))) +
                   pc * ((2.0 * x - a - b) / ((c - a) * (c - b)));
        };
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t first = std::clamp(i, std::size_t{1}, n - 2) - 1;
            velocities[i] = derivative(times_[first],
                                       times_[first + 1],
                                       times_[first + 2],
                                       positions_[first],
                                       positions_[first + 1],
                                       positions_[first + 2],
                                       times_[i]);
        }
        return velocities;
    }
};

/**
 * @brief A node's rotation from a table of times and rotations, with or without angular
 * velocities, relative to its parent.
 *
 * Between two entries, the rotation turns from one to the next at a steady rate about a fixed
 * axis, the shorter way round (spherical linear interpolation), as in a SPICE type 3 C-kernel. Its
 * angular velocity is that steady rate, or, where angular velocities are given (about the
 * parent's axes), theirs interpolated linearly between the two entries. Entries should be close
 * enough that no turn between two is more than half a turn.
 *
 * A time outside the table is an error: nothing is extrapolated.
 */
class RotationTableCallback : public RotationCallback {
  public:
    /**
     * @param times When each entry is: at least two, each later than the one before.
     * @param rotations The rotation at each time, from the node's frame to its parent's.
     * @param angular_velocities The angular velocity at each time, in radians per second about the
     *        parent's axes, or none to take the rate between entries.
     * @param what What the rotation is of, for errors (the node's name).
     * @throws std::runtime_error if there are fewer than two entries, the times are out of order,
     *         the sizes differ, or an angular velocity is not finite.
     */
    RotationTableCallback(const std::vector<Time>& times,
                          std::vector<Rotation<double>> rotations,
                          std::vector<Vec3<double>> angular_velocities,
                          std::string what)
        : rotations_(std::move(rotations)), angular_velocities_(std::move(angular_velocities)),
          what_(std::move(what))
    {
        times_ = detail::table_times(times, rotations_.size(), what_);
        if (!angular_velocities_.empty() && angular_velocities_.size() != rotations_.size()) {
            HUIRA_THROW_ERROR(what_ + " - The table has " + std::to_string(rotations_.size()) +
                              " rotations and " + std::to_string(angular_velocities_.size()) +
                              " angular velocities");
        }
        for (const Vec3<double>& angular_velocity : angular_velocities_) {
            validate_real(angular_velocity, what_ + " angular velocity in its table");
        }
        turns_.resize(rotations_.size() - 1);
        for (std::size_t i = 0; i + 1 < rotations_.size(); ++i) {
            turns_[i] = rotation_vector(rotations_[i + 1] * rotations_[i].inverse());
        }
    }

    [[nodiscard]] RotationState evaluate(const PoseContext& context,
                                         const PositionState& /*position*/) const override
    {
        const auto [i, s] = detail::table_interval(times_, context.time, what_);
        RotationState state;
        state.rotation = rotation_from_vector(s * turns_[i]) * rotations_[i];
        if (angular_velocities_.empty()) {
            state.angular_velocity = turns_[i] / (times_[i + 1] - times_[i]);
        } else {
            state.angular_velocity =
                (1.0 - s) * angular_velocities_[i] + s * angular_velocities_[i + 1];
        }
        return state;
    }

  private:
    std::vector<double> times_;
    std::vector<Rotation<double>> rotations_;
    std::vector<Vec3<double>> angular_velocities_;
    std::string what_;

    /// The turn from each entry to the next, as a rotation vector about the parent's axes.
    std::vector<Vec3<double>> turns_;
};

} // namespace huira
