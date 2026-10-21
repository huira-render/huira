#pragma once

#include "huira/core/time.hpp"
#include "huira/units/units.hpp"
#include "huira/util/logger.hpp"

namespace huira {

/**
 * @brief Represents a time interval over which a sensor exposure occurs.
 *
 * Internally stores explicit start and end times. Provides named factory
 * methods for the common parameterizations used in space imaging:
 *
 * - **centered**: midpoint time + total duration (e.g., "observe at T ± dt/2")
 * - **from_start**: start time + integration length
 * - **from_bounds**: explicit start and end times
 *
 * A zero-duration interval (start == end) implies no motion blur.
 *
 * Poses set by hand (a node's position, rotation, velocity and angular velocity, as opposed to
 * SPICE) hold at the interval's reference time, and the velocities carry them from there through
 * the exposure. The reference is the time the interval was made from: the start for from_start,
 * from_bounds and the constructor, and the center for from_centered. A render without motion blur
 * (one temporal sample) is taken at the exposure's center, so it shows what a motion-blurred
 * render shows on average: an object that streaks with motion blur is seen at the middle of its
 * streak without it.
 */
struct Interval {
    /// Its reference time is its start. @throws std::runtime_error if it ends before it starts.
    Interval(Time t0, Time tf) : start(t0), end(tf), reference(t0) { check(); }

    Time start;
    Time end;

    /// The time that poses set by hand refer to (see above): the start, or the center for an
    /// interval made with from_centered. It can be set to any time.
    Time reference;

    /// Throws if the interval ends before it starts, which would give a negative exposure. (Its
    /// times are finite, as every Time is.) SceneView checks it again, since the fields can be
    /// changed.
    void check() const
    {
        if (!(end >= start)) {
            HUIRA_THROW_ERROR("Interval - The interval must not end before it starts: from " +
                              std::to_string(start.et()) + " to " + std::to_string(end.et()) +
                              " s past J2000");
        }
    }

    /**
     * @brief Construct an exposure interval centered on a given time.
     *
     * @param center The midpoint of the exposure, and its reference time.
     * @param duration The total integration time: non-negative.
     * @return Interval spanning [center - duration/2, center + duration/2].
     * @throws std::runtime_error if the duration is negative or not finite.
     */
    static Interval from_centered(const Time& center, units::Second duration)
    {
        Interval interval{center + units::Second(-duration.value() / 2.0),
                          center + units::Second(duration.value() / 2.0)};
        interval.reference = center;
        return interval;
    }

    /**
     * @brief Construct an exposure interval from a start time and duration.
     *
     * @param start The beginning of the exposure, and its reference time.
     * @param duration The total integration time: non-negative.
     * @return Interval spanning [start, start + duration].
     * @throws std::runtime_error if the duration is negative or not finite.
     */
    static Interval from_start(const Time& start, units::Second duration)
    {
        return {start, start + duration};
    }

    /**
     * @brief Construct an exposure interval from explicit start and end times.
     *
     * @param start The beginning of the exposure, and its reference time.
     * @param end The end of the exposure.
     * @return Interval spanning [start, end].
     */
    static Interval from_bounds(const Time& start, const Time& end) { return {start, end}; }

    /// The midpoint of the exposure interval.
    Time center() const { return start + units::Second((end.et() - start.et()) / 2.0); }

    /// The total duration of the exposure in seconds.
    units::Second duration() const { return units::Second(end.et() - start.et()); }
};

} // namespace huira
