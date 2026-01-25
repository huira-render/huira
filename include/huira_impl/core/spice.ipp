#include <atomic>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "cspice/SpiceUsr.h"

#include "huira/core/time.hpp"
#include "huira/detail/logger.hpp"
#include "huira/detail/paths.hpp"

namespace fs = std::filesystem;

namespace huira::spice {

    // ==================================== //
    // === SPICE error handling utility === //
    // ==================================== //
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
        std::cerr << "SPICE ERROR: ";
        std::cerr << error << std::endl;
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


    // =============================== //
    // === SPICE furnsh interfaces === //
    // =============================== //
    inline void furnsh(const fs::path& file_path) {
        HUIRA_LOG_INFO("SPCIE Furnsh: " + file_path.string());
        call_spice(furnsh_c, file_path.string().c_str());
    }

    inline void furnsh_relative_to_file(const fs::path& kernel_path) {
        if (!kernel_path.has_parent_path()) {
            furnsh(kernel_path);
            return;
        }

        HUIRA_LOG_INFO("SPCIE Furnsh (relative): " + kernel_path.string());

        struct DirectoryGuard {
            fs::path original;
            DirectoryGuard() : original(fs::current_path()) {}
            ~DirectoryGuard() { fs::current_path(original); }
        };

        DirectoryGuard guard;
        fs::current_path(kernel_path.parent_path());
        call_spice([&]() { furnsh_c(kernel_path.filename().string().c_str()); });
    }


    // ======================================= //
    // === Default SPICE kernel management === //
    // ======================================= //
    inline fs::path get_default_lsk_path() {
        fs::path data_directory = huira::data_dir();
        return data_directory / "kernels" / "lsk" / "naif0012.tls";
    }

    inline void ensure_lsk_loaded() {
        if (lsk_loaded.load(std::memory_order_acquire)) {
            return;
        }
        std::call_once(lsk_init_flag, []() {
            // Save current error action and set to RETURN mode
            SpiceChar oldAction[16];
            erract_c("GET", sizeof(oldAction), oldAction);

            SpiceChar action[] = "RETURN";
            erract_c("SET", 0, action);

            // Disable automatic error message printing
            SpiceChar none[] = "NONE";
            errprt_c("SET", 0, none);

            // Test if an LSK is already loaded
            SpiceDouble et;
            str2et_c("2000-001T12:00:00", &et);

            if (failed_c()) {
                reset_c();
                // No LSK loaded - load our default
                std::string message = "Loading default LSK from: " + get_default_lsk_path().string();
                std::cout << "\n" + message + "\n";
                HUIRA_LOG_INFO(message);
                furnsh_c(get_default_lsk_path().string().c_str());
                if (failed_c()) {
                    SpiceChar msg[1841];
                    getmsg_c("LONG", 1841, msg);
                    reset_c();
                    erract_c("SET", 0, oldAction);  // Restore before throwing
                    throw SpiceError(std::string("Failed to load default LSK: ") + msg);
                }
            }

            // Restore original error action
            erract_c("SET", 0, oldAction);
            lsk_loaded.store(true, std::memory_order_release);
            });
    }

    inline fs::path get_default_pck_path() {
        fs::path data_directory = huira::data_dir();
        return data_directory / "kernels" / "pck" / "pck00011.tpc";
    }

    inline void load_default_pck() {
        HUIRA_LOG_INFO("Default PCK loaded from: " + get_default_pck_path().string());
        furnsh(get_default_pck_path());
    }


    // ============================= //
    // === SPICE time interfaces === //
    // ============================= //
    inline double string_to_et(const std::string& time_string) {
        ensure_lsk_loaded();

        SpiceDouble et;
        call_spice(str2et_c, time_string.c_str(), &et);
        return static_cast<double>(et);
    }

    inline double et_to_julian_date(double et, const std::string& scale) {
        ensure_lsk_loaded();

        SpiceDouble result = call_spice(unitim_c, et, "ET", scale.c_str());
        return static_cast<double>(result);
    }

    inline double julian_date_to_et(double jd, const std::string& scale) {
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


    // ============================== //
    // === SPICE State interfaces === //
    // ============================== //
    template <huira::IsFloatingPoint T>
    inline std::tuple<Vec3<T>, Vec3<T>, double> spkezr(
        const std::string& TARGET,
        const huira::Time& time,
        const std::string& FRAME,
        const std::string& ABCORR,
        const std::string& OBSERVER
        )
    {
        SpiceDouble et = time.et();
        SpiceDouble state[6];
        SpiceDouble lt;

        call_spice(spkezr_c, TARGET.c_str(), et, FRAME.c_str(), ABCORR.c_str(), OBSERVER.c_str(), state, &lt);

        Vec3<T> position{ static_cast<T>(state[0]), static_cast<T>(state[1]), static_cast<T>(state[2]) };
        Vec3<T> velocity{ static_cast<T>(state[3]), static_cast<T>(state[4]), static_cast<T>(state[5]) };
        return { position, velocity, static_cast<double>(lt) };
    }

    template <huira::IsFloatingPoint T>
    inline huira::Rotation<T> pxform(
        const std::string& FROM,
        const std::string& TO,
        const huira::Time& time)
    {
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
