#pragma once

#include <string>

#include "cspice/SpiceUsr.h"

#include "huira/spice/spice_default.hpp"
#include "huira/spice/spice_error.hpp"

namespace huira::spice {

    inline double string_to_et(const std::string& time_string) {
        ensure_lsk_loaded();

        SpiceDouble et;
        call_spice(str2et_c, time_string.c_str(), &et);
        return static_cast<double>(et);
    }

    inline double et_to_julian_date(double et, const std::string& scale = "JDTDB") {
        ensure_lsk_loaded();

        SpiceDouble result = call_spice(unitim_c, et, "ET", scale.c_str());
        return static_cast<double>(result);
    }

    inline double julian_date_to_et(double jd, const std::string& scale = "JDTDB") {
        ensure_lsk_loaded();

        SpiceDouble result = call_spice(unitim_c, jd, scale.c_str(), "ET");
        return static_cast<double>(result);
    }

    inline std::string et_to_string(double et, const std::string& format) {
        ensure_lsk_loaded();

        constexpr int buffer_size = 256;
        char buffer[buffer_size];

        call_spice(timout_c, et, format.c_str(), buffer_size, buffer);
        return std::string(buffer, strnlen(buffer, buffer_size));
    }
}
