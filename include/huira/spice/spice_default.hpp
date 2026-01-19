#pragma once

#include <filesystem>
#include <atomic>
#include <mutex>

#include "cspice/SpiceUsr.h"

#include "huira/detail/paths.hpp"
#include "huira/spice/spice_error.hpp"
#include "huira/detail/logger.hpp"
#include "huira/spice/spice_furnsh.hpp"

namespace fs = std::filesystem;

namespace huira::spice {
    inline std::once_flag lsk_init_flag;
    inline std::atomic<bool> lsk_loaded{ false };

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
}
