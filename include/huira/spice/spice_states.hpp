#pragma once

#include <string>
#include <utility>

#include "cspice/SpiceUsr.h"

#include "huira/core/time.hpp"
#include "huira/core/rotation.hpp"
#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/spice/spice_default.hpp"
#include "huira/spice/spice_error.hpp"

namespace huira::spice {

    template <huira::IsFloatingPoint T>
    inline std::tuple<Vec3<T>, Vec3<T>, double> spkezr(const std::string& TARGET, const huira::Time& time, const std::string& FRAME,
        const std::string& OBSERVER, const std::string& ABCORR = "NONE") {
        SpiceDouble et = time.et();
        SpiceDouble state[6];
        SpiceDouble lt;

        call_spice(spkezr_c, TARGET.c_str(), et, FRAME.c_str(), ABCORR.c_str(), OBSERVER.c_str(), state, &lt);

        Vec3<T> position{ static_cast<T>(state[0]), static_cast<T>(state[1]), static_cast<T>(state[2]) };
        Vec3<T> velocity{ static_cast<T>(state[3]), static_cast<T>(state[4]), static_cast<T>(state[5]) };
        return { position, velocity, static_cast<double>(lt) };
    }

    template <huira::IsFloatingPoint T>
    inline huira::Rotation<T> pxform(const std::string& FROM, const std::string& TO, const huira::Time& time) {
        SpiceDouble et = time.et();
        SpiceDouble matrix[3][3];

        call_spice(pxform_c, FROM.c_str(), TO.c_str(), et, matrix);

        huira::Mat3<T> rotation{
            static_cast<T>(matrix[0][0]), static_cast<T>(matrix[0][1]), static_cast<T>(matrix[0][2]),
            static_cast<T>(matrix[1][0]), static_cast<T>(matrix[1][1]), static_cast<T>(matrix[1][2]),
            static_cast<T>(matrix[2][0]), static_cast<T>(matrix[2][1]), static_cast<T>(matrix[2][2])
        };

        return huira::Rotation<T>{ rotation };
    }

    template <huira::IsFloatingPoint T>
    inline std::pair<huira::Rotation<T>, huira::Vec3<T>> sxform(
        const std::string& FROM,
        const std::string& TO,
        const huira::Time& time
    ) {
        SpiceDouble et = time.et();
        SpiceDouble state_xform[6][6];
        SpiceDouble rotation[3][3];
        SpiceDouble angular_velocity[3];

        // Get state transformation matrix (includes rotation + derivatives)
        call_spice(sxform_c, FROM.c_str(), TO.c_str(), et, state_xform);

        // Extract rotation matrix and angular velocity vector
        xf2rav_c(state_xform, rotation, angular_velocity);

        huira::Mat3<T> rot{
            static_cast<T>(rotation[0][0]), static_cast<T>(rotation[0][1]), static_cast<T>(rotation[0][2]),
            static_cast<T>(rotation[1][0]), static_cast<T>(rotation[1][1]), static_cast<T>(rotation[1][2]),
            static_cast<T>(rotation[2][0]), static_cast<T>(rotation[2][1]), static_cast<T>(rotation[2][2])
        };

        huira::Vec3<T> ang_vel{
            static_cast<T>(angular_velocity[0]),
            static_cast<T>(angular_velocity[1]),
            static_cast<T>(angular_velocity[2])
        };

        return { huira::Rotation<T>{ rot }, ang_vel };
    }
}
