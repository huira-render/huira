#pragma once

/**
 * @file interval.hpp
 * @brief Defines an exposure time interval for rendering with optional motion blur.
 *
 * Interval represents a continuous time span during which a sensor
 * integrates light. It can be constructed from various common parameterizations:
 * center time + duration, start time + duration, or explicit start/end bounds.
 *
 * When the interval has non-zero duration and motion blur is enabled, SceneView
 * will provide multiple time-step transforms to Embree for instance-level
 * motion blur interpolation.
 */

#include "huira/core/time.hpp"
#include "huira/core/units/units.hpp"

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
     * - **instant**: a zero-duration snapshot at a single time
     *
     * A zero-duration interval (start == end) implies no motion blur.
     */
    struct Interval {
        Interval(Time t0, Time tf) : start(t0), end(tf) {}

        Time start;
        Time end;

        /**
         * @brief Construct an exposure interval centered on a given time.
         *
         * @param center The midpoint of the exposure.
         * @param duration The total integration time.
         * @return Interval spanning [center - duration/2, center + duration/2].
         */
        static Interval from_centered(const Time& center, units::Second duration) {
            return {
                center + units::Second(-duration.value() / 2.0),
                center + units::Second(duration.value() / 2.0)
            };
        }

        /**
         * @brief Construct an exposure interval from a start time and duration.
         *
         * @param start The beginning of the exposure.
         * @param duration The total integration time.
         * @return Interval spanning [start, start + duration].
         */
        static Interval from_start(const Time& start, units::Second duration) {
            return { start, start + duration };
        }

        /**
         * @brief Construct an exposure interval from explicit start and end times.
         *
         * @param start The beginning of the exposure.
         * @param end The end of the exposure.
         * @return Interval spanning [start, end].
         */
        static Interval from_bounds(const Time& start, const Time& end) {
            return { start, end };
        }

        /** @brief The midpoint of the exposure interval. */
        Time center() const {
            return start + units::Second((end.et() - start.et()) / 2.0);
        }

        /** @brief The total duration of the exposure in seconds. */
        units::Second duration() const {
            return units::Second(end.et() - start.et());
        }
    };

}
