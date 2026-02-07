#pragma once

#include <cstdlib>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#elif defined(__APPLE__)
#include <pwd.h>
#include <unistd.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace huira {

    inline std::filesystem::path get_log_file_path(const std::string& filename) {
        std::string actual_filename = filename;

        if (actual_filename.empty()) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::tm tm_buf;
#ifdef _WIN32
            localtime_s(&tm_buf, &time_t);
#else
            localtime_r(&time_t, &tm_buf);
#endif
            std::ostringstream oss;
            oss << "huira_log_"
                << std::put_time(&tm_buf, "%Y%m%d_%H%M%S")
                << ".txt";
            actual_filename = oss.str();
        }

        // Check environment variable for custom log directory
#ifdef _MSC_VER
        #pragma warning(push)
        #pragma warning(disable: 4996)  // 'getenv': This function may be unsafe
#endif
        const char* custom_log_dir = std::getenv("HUIRA_LOG_DIR");
#ifdef _MSC_VER
        #pragma warning(pop)
#endif
        if (custom_log_dir) {
            return std::filesystem::path(custom_log_dir) / actual_filename;
        }

        // Default: current working directory for developer tools
        return std::filesystem::current_path() / actual_filename;
    }

} // namespace huira
