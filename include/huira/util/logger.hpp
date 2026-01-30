#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>
#include <stdexcept>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace huira {

    enum class LogLevel {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3
    };

    struct LogEntry {
        std::chrono::system_clock::time_point timestamp;
        LogLevel level;
        std::string message;
        std::thread::id thread_id;

        LogEntry() = default;
        LogEntry(std::chrono::system_clock::time_point ts, LogLevel lvl,
            std::string msg, std::thread::id tid)
            : timestamp(ts), level(lvl), message(std::move(msg)), thread_id(tid) {
        }

        std::string to_string() const;

    private:
        static const char* level_to_string(LogLevel level);
    };

    class Logger {
    public:
        using CustomSink = std::function<void(const LogEntry&)>;

        static Logger& instance() {
            static Logger logger;
            return logger;
        }

        // Prevent copying and moving
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        Logger(Logger&&) = delete;
        Logger& operator=(Logger&&) = delete;

        void set_level(LogLevel level);
        LogLevel get_level() const;
        void set_buffer_size(size_t size);
        size_t get_buffer_size() const;
        void set_custom_sink(CustomSink sink);
        void clear_custom_sink();
        void log(LogLevel level, const std::string& message);
        std::string dump_to_file(const std::string& filepath = "");
        void enable_crash_handler(bool enable = true);

        // Console output configuration
        void enable_console_debug(bool enable = true);
        void enable_console_info(bool enable = true);
        void enable_console_warning(bool enable = true);
        bool is_console_debug_enabled() const;
        bool is_console_info_enabled() const;
        bool is_console_warning_enabled() const;

    private:
        Logger();
        ~Logger();

        // Centralized crash report output
        static void output_crash_report(const std::string& log_path);
        
        static void handle_crash(int signal);
        [[noreturn]] static void handle_terminate();
        void install_crash_handlers();

#ifdef _WIN32
        static LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS* exception_info);
#endif

        std::vector<LogEntry> buffer_;
        std::atomic<size_t> write_index_;
        std::atomic<LogLevel> min_level_;
        std::atomic<bool> crash_handler_enabled_;
        std::atomic<bool> console_debug_;
        std::atomic<bool> console_info_;
        std::atomic<bool> console_warning_;
        CustomSink custom_sink_;

        // Ensures only one crash report is printed per process
        static std::atomic<bool> crash_reported_;
    };

    // Convenience functions
    inline void set_log_level(LogLevel level) {
        Logger::instance().set_level(level);
    }

    inline void set_log_buffer_size(size_t size) {
        Logger::instance().set_buffer_size(size);
    }

    inline void set_log_sink(Logger::CustomSink sink) {
        Logger::instance().set_custom_sink(std::move(sink));
    }

    inline std::string dump_log(const std::string& filepath = "") {
        return Logger::instance().dump_to_file(filepath);
    }

    inline void enable_console_debug(bool enable = true) {
        Logger::instance().enable_console_debug(enable);
    }

    inline void enable_console_info(bool enable = true) {
        Logger::instance().enable_console_info(enable);
    }

    inline void enable_console_warning(bool enable = true) {
        Logger::instance().enable_console_warning(enable);
    }

}


// Convenience macros for logging
#define HUIRA_LOG_DEBUG(msg) \
    do { \
        if (huira::Logger::instance().get_level() <= huira::LogLevel::Debug) { \
            std::string _huira_debug_msg = (msg); \
            huira::Logger::instance().log(huira::LogLevel::Debug, _huira_debug_msg); \
            if (huira::Logger::instance().is_console_debug_enabled()) { \
                std::cout << "[DEBUG] " << _huira_debug_msg << std::endl; \
            } \
        } \
    } while(0)

#define HUIRA_LOG_INFO(msg) \
    do { \
        if (huira::Logger::instance().get_level() <= huira::LogLevel::Info) { \
            std::string _huira_info_msg = (msg); \
            huira::Logger::instance().log(huira::LogLevel::Info, _huira_info_msg); \
            if (huira::Logger::instance().is_console_info_enabled()) { \
                std::cout << "[INFO] " << _huira_info_msg << std::endl; \
            } \
        } \
    } while(0)

#define HUIRA_LOG_WARNING(msg) \
    do { \
        if (huira::Logger::instance().get_level() <= huira::LogLevel::Warning) { \
            std::string _huira_warning_msg = (msg); \
            huira::Logger::instance().log(huira::LogLevel::Warning, _huira_warning_msg); \
            if (huira::Logger::instance().is_console_warning_enabled()) { \
                std::cerr << huira::detail::yellow("[WARNING] " + _huira_warning_msg) << std::endl; \
            } \
        } \
    } while(0)

#define HUIRA_LOG_ERROR(msg) \
    do { \
        if (huira::Logger::instance().get_level() <= huira::LogLevel::Error) { \
            std::string _huira_error_msg = (msg); \
            huira::Logger::instance().log(huira::LogLevel::Error, _huira_error_msg); \
            std::cerr << huira::detail::red("[ERROR] " + _huira_error_msg) << std::endl; \
        } \
    } while(0)

// Variadic version
#define HUIRA_LOG(level, ...) \
    do { \
        if (huira::Logger::instance().get_level() <= level) { \
            std::ostringstream _huira_log_oss; \
            (_huira_log_oss << ... << __VA_ARGS__); \
            huira::Logger::instance().log(level, _huira_log_oss.str()); \
        } \
    } while(0)

// Log error and throw runtime_error with the same message
#define HUIRA_THROW_ERROR(msg) \
    do { \
        std::string _huira_error_msg = (msg); \
        huira::Logger::instance().log(huira::LogLevel::Error, _huira_error_msg); \
        std::cerr << huira::detail::red("[ERROR] " + _huira_error_msg) << std::endl; \
        throw std::runtime_error(_huira_error_msg); \
    } while(0)

#include "huira_impl/util/logger.ipp"
