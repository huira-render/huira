#include "huira/detail/platform/info.hpp"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <fstream>
#include <sstream>
#elif defined(__APPLE__)
#include <mach/mach.h>
#endif

#include <string>

namespace huira {
    std::string getPlatform() {
#ifdef _WIN32
        return "Platform: Windows";
#elif defined(__linux__)
        return "Platform: Linux";
#elif defined(__APPLE__)
        return "Platform: macOS";
#else
        return "Platform: Unknown";
#endif
    }

    std::string getCompilerInfo() {
#ifdef _MSC_VER
        return "MSVC " + std::to_string(_MSC_VER);
#elif defined(__GNUC__) && !defined(__clang__)
        return "GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#elif defined(__clang__)
        return "Clang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#else
        return "Unknown Compiler";
#endif
    }

    std::string getMemoryUsage() {
        std::string output;

#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            output = "RAM usage: " + std::to_string(pmc.WorkingSetSize / 1024 / 1024) + " MB\n";
            output += "Peak RAM: " + std::to_string(pmc.PeakWorkingSetSize / 1024 / 1024) + " MB";
        }
        else {
            output = "RAM usage: Unable to retrieve memory info";
        }
#elif defined(__linux__)
        std::ifstream status("/proc/self/status");
        std::string line;
        bool found = false;

        if (status.is_open()) {
            while (std::getline(status, line)) {
                if (line.find("VmRSS:") == 0) {
                    output = "RAM usage: " + line.substr(7);
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            output = "RAM usage: Unable to read /proc/self/status";
        }
#elif defined(__APPLE__)
        struct mach_task_basic_info info;
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;

        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS) {
            output = "RAM usage: " + std::to_string(info.resident_size / 1024 / 1024) + " MB";
        }
        else {
            output = "RAM usage: Unable to retrieve memory info";
        }
#else
        output = "RAM usage: Not supported on this platform";
#endif

        return output;
    }
}