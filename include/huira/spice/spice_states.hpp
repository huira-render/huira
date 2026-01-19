#pragma once

#include <string>
#include <array>

#include "cspice/SpiceUsr.h"

#include "huira/core/time.hpp"
#include "huira/core/rotation.hpp"
#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/spice/spice_default.hpp"
#include "huira/spice/spice_error.hpp"

namespace huira::spice {

    template <huira::IsFloatingPoint T>
    inline std::array<T, 6> spkezr(const std::string& TARGET, const huira::Time& time, const std::string& FRAME,
        const std::string& OBSERVER, const std::string& ABCORR = "NONE") {
        SpiceDouble et = time.et();
        SpiceDouble state[6];
        SpiceDouble lt;

        call_spice(spkezr_c, TARGET.c_str(), et, FRAME.c_str(), ABCORR.c_str(), OBSERVER.c_str(), state, &lt);
        //HUIRA_THROW_ERROR("spkezr() failed for target '" + TARGET + "', observer '" + OBSERVER + "': " + e.what());

        return std::array<T, 6>{
            static_cast<T>(state[0]),
            static_cast<T>(state[1]),
            static_cast<T>(state[2]),
            static_cast<T>(state[3]),
            static_cast<T>(state[4]),
            static_cast<T>(state[5])
        };
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

}
