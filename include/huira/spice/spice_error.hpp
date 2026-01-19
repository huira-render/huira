#pragma once

#include <stdexcept>
#include <string>
#include <type_traits>

#include "cspice/SpiceUsr.h"

#include "huira/detail/logger.hpp"

namespace huira::spice {

    class SpiceError : public std::runtime_error {
    public:
        explicit SpiceError(const std::string& msg) : std::runtime_error(msg) {}
    };

    inline void check_spice_error() {
        if (!failed_c()) {
            return;
        }

        constexpr int MAX_MSG_LEN = 1841;
        SpiceChar short_msg[MAX_MSG_LEN];
        SpiceChar long_msg[MAX_MSG_LEN];

        getmsg_c("SHORT", MAX_MSG_LEN, short_msg);
        getmsg_c("LONG", MAX_MSG_LEN, long_msg);
        reset_c();

        std::string error = std::string(short_msg);
        if (long_msg[0] != '\0') {
            error += ": " + std::string(long_msg);
        }

        HUIRA_THROW_ERROR(error);
    }

    template <typename Func, typename... Args>
    auto call_spice(Func func, Args&&... args) {
        // Initialize error handling on first call
        static bool initialized = []() {
            SpiceChar action[] = "RETURN";
            erract_c("SET", 0, action);

            // Disable automatic error message printing
            SpiceChar none[] = "NONE";
            errprt_c("SET", 0, none);

            return true;
            }();
        (void)initialized;

        if (failed_c()) {
            reset_c();
        }

        using ReturnType = decltype(func(std::forward<Args>(args)...));

        if constexpr (std::is_void_v<ReturnType>) {
            func(std::forward<Args>(args)...);
            check_spice_error();
        }
        else {
            auto result = func(std::forward<Args>(args)...);
            check_spice_error();
            return result;
        }
    }

}
