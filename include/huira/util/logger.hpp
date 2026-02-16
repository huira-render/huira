#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace huira {

    /**
     * @brief Severity levels for log messages.
     * 
     * Ordered from least to most severe. The logger can be configured to filter
     * messages below a certain level.
     */
    enum class LogLevel {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3
    };

    /**
     * @brief A single log entry with timestamp, level, message, and thread ID.
     * 
     * LogEntry represents an immutable log record that can be stored in the logger's
     * circular buffer and formatted for output.
     */
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

    /**
     * @brief Thread-safe singleton logger for application-wide logging.
     * 
     * Logger provides a centralized logging system with support for:
     * - Circular buffer for efficient log storage
     * - Configurable severity filtering
     * - Custom output sinks
     * - Automatic crash handling and log dumping
     * - Per-level console output configuration
     * - Cross-platform crash reporting (signals and SEH)
     * 
     * The logger is thread-safe and uses lock-free atomic operations for performance.
     * Log entries are stored in a circular buffer and can be dumped to a file on demand
     * or automatically during crashes.
     */
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

    /**
     * @brief Set the minimum log level for the global logger.
     * 
     * @param level Minimum severity level to log
     */
    inline void set_log_level(LogLevel level) {
        Logger::instance().set_level(level);
    }

    /**
     * @brief Set the circular buffer size for the global logger.
     * 
     * @param size Number of log entries to retain in memory
     */
    inline void set_log_buffer_size(size_t size) {
        Logger::instance().set_buffer_size(size);
    }

    /**
     * @brief Set a custom output sink for the global logger.
     * 
     * @param sink Callback function to receive log entries
     */
    inline void set_log_sink(Logger::CustomSink sink) {
        Logger::instance().set_custom_sink(std::move(sink));
    }

    /**
     * @brief Dump all buffered log entries to a file.
     * 
     * @param filepath Path to output file (empty for platform-specific default)
     * @return std::string Actual path where log was written, or empty on failure
     */
    inline std::string dump_log(const std::string& filepath = "") {
        return Logger::instance().dump_to_file(filepath);
    }

    /**
     * @brief Enable or disable debug-level console output.
     * 
     * @param enable True to enable debug console output
     */
    inline void enable_console_debug(bool enable = true) {
        Logger::instance().enable_console_debug(enable);
    }

    /**
     * @brief Enable or disable info-level console output.
     * 
     * @param enable True to enable info console output
     */
    inline void enable_console_info(bool enable = true) {
        Logger::instance().enable_console_info(enable);
    }

    /**
     * @brief Enable or disable warning-level console output.
     * 
     * @param enable True to enable warning console output
     */
    inline void enable_console_warning(bool enable = true) {
        Logger::instance().enable_console_warning(enable);
    }

}

/**
 * @brief Log a debug-level message.
 * 
 * Evaluates the message only if debug logging is enabled. Outputs to console if
 * console debug output is enabled.
 * 
 * @param msg Message to log (can be a string or expression that converts to string)
 */
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

/**
 * @brief Log an info-level message.
 * 
 * Evaluates the message only if info logging is enabled. Outputs to console if
 * console info output is enabled.
 * 
 * @param msg Message to log (can be a string or expression that converts to string)
 */
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

/**
 * @brief Log a warning-level message.
 * 
 * Evaluates the message only if warning logging is enabled. Outputs to console (in yellow)
 * if console warning output is enabled.
 * 
 * @param msg Message to log (can be a string or expression that converts to string)
 */
#define HUIRA_LOG_WARNING(msg) \
    do { \
        if (huira::Logger::instance().get_level() <= huira::LogLevel::Warning) { \
            std::string _huira_warning_msg = (msg); \
            huira::Logger::instance().log(huira::LogLevel::Warning, _huira_warning_msg); \
            if (huira::Logger::instance().is_console_warning_enabled()) { \
                std::cerr << huira::yellow("[WARNING] " + _huira_warning_msg) << std::endl; \
            } \
        } \
    } while(0)

/**
 * @brief Log an error-level message.
 * 
 * Always outputs to console in red. Error messages are never filtered.
 * 
 * @param msg Message to log (can be a string or expression that converts to string)
 */
#define HUIRA_LOG_ERROR(msg) \
    do { \
        if (huira::Logger::instance().get_level() <= huira::LogLevel::Error) { \
            std::string _huira_error_msg = (msg); \
            huira::Logger::instance().log(huira::LogLevel::Error, _huira_error_msg); \
            std::cerr << huira::red("[ERROR] " + _huira_error_msg) << std::endl; \
        } \
    } while(0)

/**
 * @brief Log a variadic message with specified log level.
 * 
 * Accepts multiple arguments that are concatenated using the << operator.
 * 
 * @param level LogLevel for this message
 * @param ... Variadic arguments to concatenate into the log message
 */
#define HUIRA_LOG(level, ...) \
    do { \
        if (huira::Logger::instance().get_level() <= level) { \
            std::ostringstream _huira_log_oss; \
            (_huira_log_oss << ... << __VA_ARGS__); \
            huira::Logger::instance().log(level, _huira_log_oss.str()); \
        } \
    } while(0)

/**
 * @brief Log an error message and throw a runtime_error with the same message.
 * 
 * This macro ensures the error is logged before throwing the exception, and outputs
 * to console in red.
 * 
 * @param msg Error message to log and throw
 */
#define HUIRA_THROW_ERROR(msg) \
    do { \
        std::string _huira_error_msg = (msg); \
        huira::Logger::instance().log(huira::LogLevel::Error, _huira_error_msg); \
        std::cerr << huira::red("[ERROR] " + _huira_error_msg) << std::endl; \
        throw std::runtime_error(_huira_error_msg); \
    } while(0)

#include "huira_impl/util/logger.ipp"
