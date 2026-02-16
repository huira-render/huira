#include <atomic>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "cspice/SpiceUsr.h"

#include "huira/core/time.hpp"
#include "huira/util/logger.hpp"
#include "huira/util/paths.hpp"

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

    /**
     * @brief Load a SPICE kernel file.
     * @param file_path Path to kernel file
     */
    inline void furnsh(const fs::path& file_path) {
        HUIRA_LOG_INFO("SPCIE Furnsh: " + file_path.string());
        call_spice(furnsh_c, file_path.string().c_str());
    }


    /**
     * @brief Load a SPICE kernel file, resolving relative to its parent directory.
     * @param kernel_path Path to kernel file
     */
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

    /**
     * @brief Get the default LSK (leap seconds kernel) path.
     * @return fs::path Path to default LSK
     */
    inline fs::path get_default_lsk_path() {
        fs::path data_directory = huira::data_dir();
        return data_directory / "kernels" / "lsk" / "naif0012.tls";
    }


    /**
     * @brief Ensure the default LSK is loaded.
     */
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


    /**
     * @brief Get the default PCK (planetary constants kernel) path.
     * @return fs::path Path to default PCK
     */
    inline fs::path get_default_pck_path() {
        fs::path data_directory = huira::data_dir();
        return data_directory / "kernels" / "pck" / "pck00011.tpc";
    }


    /**
     * @brief Load the default PCK.
     */
    inline void load_default_pck() {
        HUIRA_LOG_INFO("Default PCK loaded from: " + get_default_pck_path().string());
        furnsh(get_default_pck_path());
    }


    // ============================= //
    // === SPICE time interfaces === //
    // ============================= //

    /**
     * @brief Convert a time string to ephemeris time (ET, seconds past J2000).
     * @param time_string Time string (e.g., "2000-001T12:00:00")
     * @return double Ephemeris time (seconds past J2000)
     */
    inline double str2et(const std::string& time_string)
    {
        ensure_lsk_loaded();
        SpiceDouble et;
        call_spice(str2et_c, time_string.c_str(), &et);
        return static_cast<double>(et);
    }


    /**
     * @brief Compute delta ET for a given epoch and type.
     * @param epoch Epoch (seconds past J2000)
     * @param eptype Type of delta (e.g., "DELTET")
     * @return double Delta ET
     */
    inline double deltet(double epoch, const std::string& eptype)
    {
        ensure_lsk_loaded();
        SpiceDouble delta;
        call_spice(deltet_c, static_cast<SpiceDouble>(epoch), eptype.c_str(), &delta);
        return static_cast<double>(delta);
    }


    /**
     * @brief Convert an epoch from one time system to another.
     * @param epoch Epoch (seconds)
     * @param insys Input time system
     * @param outsys Output time system
     * @return double Converted epoch
     */
    inline double unitim(double epoch, const std::string& insys, const std::string& outsys)
    {
        ensure_lsk_loaded();
        return static_cast<double>(call_spice(unitim_c,
            static_cast<SpiceDouble>(epoch),
            insys.c_str(),
            outsys.c_str()));
    }


    /**
     * @brief Format ephemeris time as a string.
     * @param et Ephemeris time (seconds past J2000)
     * @param pictur Output format string
     * @param lenout Output string length
     * @return std::string Formatted time string
     */
    inline std::string timout(double et, const std::string& pictur, int lenout)
    {
        ensure_lsk_loaded();
        std::string output(static_cast<size_t>(lenout), '\0');
        call_spice(timout_c,
            static_cast<SpiceDouble>(et),
            pictur.c_str(),
            static_cast<SpiceInt>(lenout),
            output.data());
        if (auto pos = output.find('\0'); pos != std::string::npos) {
            output.resize(pos);
        }
        return output;
    }

    // ============================== //
    // === SPICE State interfaces === //
    // ============================== //

    /**
     * @brief returns the state (position and velocity) of a target body relative to an observer.
     *
     * @param TARGET The name of the target body.
     * @param time The time of observation.
     * @param FRAME The reference frame relative to which the output vectors are expressed.
     * @param ABCORR Aberration correction flag.
     * @param OBSERVER The name of the observing body.
     * @return std::tuple<Vec3<T>, Vec3<T>, double> {Position, Velocity, LightTime}
     * - Position is in km (SPICE default).
     * - Velocity is in km/s (SPICE default).
     */
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

        // SPICE Returns values in Km and Km/s:
        Vec3<T> position_km{ static_cast<T>(state[0]), static_cast<T>(state[1]), static_cast<T>(state[2]) };
        Vec3<T> velocity_km{ static_cast<T>(state[3]), static_cast<T>(state[4]), static_cast<T>(state[5]) };

        Vec3<T> position = position_km * static_cast<T>(1000.0); // Convert to meters
        Vec3<T> velocity = velocity_km * static_cast<T>(1000.0); // Convert to meters per second
        return { position, velocity, static_cast<double>(lt) };
    }


    /**
     * @brief Get the rotation from one frame to another at a given time.
     * @tparam T Floating point type
     * @param FROM Source frame
     * @param TO Destination frame
     * @param time Observation time
     * @return huira::Rotation<T> Rotation object
     */
    template <huira::IsFloatingPoint T>
    inline huira::Rotation<T> pxform(
        const std::string& FROM,
        const std::string& TO,
        const huira::Time& time)
    {
        SpiceDouble et = time.et();
        SpiceDouble matrix[3][3];

        call_spice(pxform_c, FROM.c_str(), TO.c_str(), et, matrix);

        // SPICE returns row-major, huira::Mat3 is column-major
        huira::Mat3<T> rotation{
            static_cast<T>(matrix[0][0]), static_cast<T>(matrix[1][0]), static_cast<T>(matrix[2][0]),
            static_cast<T>(matrix[0][1]), static_cast<T>(matrix[1][1]), static_cast<T>(matrix[2][1]),
            static_cast<T>(matrix[0][2]), static_cast<T>(matrix[1][2]), static_cast<T>(matrix[2][2])
        };

        // SPICE represents tha passive rotation of parent-to-local:
        return huira::Rotation<T>::from_parent_to_local(rotation);
    }


    /**
     * @brief Get the rotation and angular velocity from one frame to another at a given time.
     * @tparam T Floating point type
     * @param FROM Source frame
     * @param TO Destination frame
     * @param time Observation time
     * @return std::pair<huira::Rotation<T>, huira::Vec3<T>> {rotation, angular velocity}
     */
    template <huira::IsFloatingPoint T>
    inline std::pair<huira::Rotation<T>, huira::Vec3<T>> sxform(
        const std::string& FROM,
        const std::string& TO,
        const huira::Time& time
    ) {
        SpiceDouble et = time.et();
        SpiceDouble state_xform[6][6];
        SpiceDouble matrix[3][3];
        SpiceDouble angular_velocity[3];

        // Get state transformation matrix (includes rotation + derivatives)
        call_spice(sxform_c, FROM.c_str(), TO.c_str(), et, state_xform);

        // Extract rotation matrix and angular velocity vector
        xf2rav_c(state_xform, matrix, angular_velocity);

        // SPICE returns row-major, huira::Mat3 is column-major
        huira::Mat3<T> rotation{
            static_cast<T>(matrix[0][0]), static_cast<T>(matrix[1][0]), static_cast<T>(matrix[2][0]),
            static_cast<T>(matrix[0][1]), static_cast<T>(matrix[1][1]), static_cast<T>(matrix[2][1]),
            static_cast<T>(matrix[0][2]), static_cast<T>(matrix[1][2]), static_cast<T>(matrix[2][2])
        };

        huira::Vec3<T> ang_vel_local{
            static_cast<T>(angular_velocity[0]),
            static_cast<T>(angular_velocity[1]),
            static_cast<T>(angular_velocity[2])
        };

        Rotation<T> rotation_obj = huira::Rotation<T>::from_parent_to_local(rotation);
        huira::Vec3<T> ang_vel_parent = rotation_obj * ang_vel_local;

        // SPICE represents tha passive rotation of parent-to-local:
        return { rotation_obj, ang_vel_parent };
    }
}
