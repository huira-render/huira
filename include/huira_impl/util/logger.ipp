#include <array>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include "huira/util/colorful_text.hpp"
#include "huira/platform/info.hpp"
#include "huira/platform/get_log_path.hpp"
#include "huira/platform/windows_minmax.hpp"

namespace huira {

    // ===== LogEntry Implementation =====

    std::string LogEntry::to_string() const {
        std::ostringstream oss;
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()) % 1000;

        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t);
#else
        localtime_r(&time_t, &tm_buf);
#endif

        oss << std::put_time(&tm_buf, "%H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count()
            << " [" << level_to_string(level) << "] "
            << "[Thread " << thread_id << "] "
            << message;
        return oss.str();
    }

    const char* LogEntry::level_to_string(LogLevel level) {
        switch (level) {
            case LogLevel::Debug:   return "DEBUG";
            case LogLevel::Info:    return "INFO ";
            case LogLevel::Warning: return "WARN ";
            case LogLevel::Error:   return "ERROR";
            default:                return "UNKNOWN";
        }
    }

    // ===== Logger Implementation =====
    // Ensures only one crash report is printed per process
    inline std::atomic<bool> Logger::crash_reported_{false};

    Logger::Logger()
        : buffer_(1000)
        , write_index_(0)
        , min_level_(LogLevel::Info)
        , crash_handler_enabled_(false)
        , console_debug_(false)
        , console_info_(false)
        , console_warning_(true)  // Warning is on by default
        , custom_sink_(nullptr) {
        // Automatically enable crash handler
        enable_crash_handler(true);
    }

    Logger::~Logger() = default;

    void Logger::set_level(LogLevel level) {
        min_level_.store(level, std::memory_order_relaxed);
    }

    LogLevel Logger::get_level() const {
        return min_level_.load(std::memory_order_relaxed);
    }

    void Logger::set_buffer_size(size_t size) {
        if (size == 0) size = 1;
        std::vector<LogEntry> new_buffer(size);

        // Copy existing entries
        size_t current_write = write_index_.load(std::memory_order_relaxed);
        size_t old_size = buffer_.size();
        size_t entries_to_copy = std::min(current_write, std::min(old_size, size));

        for (size_t i = 0; i < entries_to_copy; ++i) {
            size_t old_idx = (current_write - entries_to_copy + i) % old_size;
            new_buffer[i] = buffer_[old_idx];
        }

        buffer_ = std::move(new_buffer);
        write_index_.store(entries_to_copy, std::memory_order_relaxed);
    }

    size_t Logger::get_buffer_size() const {
        return buffer_.size();
    }

    void Logger::set_custom_sink(CustomSink sink) {
        custom_sink_ = std::move(sink);
    }

    void Logger::clear_custom_sink() {
        custom_sink_ = nullptr;
    }

    void Logger::log(LogLevel level, const std::string& message) {
        if (level < min_level_.load(std::memory_order_relaxed)) {
            return;
        }

        LogEntry entry{
            std::chrono::system_clock::now(),
            level,
            message,
            std::this_thread::get_id()
        };

        // Add to circular buffer
        size_t index = write_index_.fetch_add(1, std::memory_order_relaxed) % buffer_.size();
        buffer_[index] = entry;

        // Call custom sink if set
        if (custom_sink_) {
            try {
                custom_sink_(entry);
            } catch (...) {
                // Ignore exceptions from custom sink to prevent logging from crashing
            }
        }
    }

    std::string Logger::dump_to_file(const std::string& filepath) {
        std::filesystem::path actual_path;

        if (filepath.empty()) {
            // Use platform-appropriate log directory
            actual_path = get_log_file_path();
        } else {
            actual_path = filepath;
        }

        std::ofstream file(actual_path);
        if (!file) {
            // Try current directory as absolute fallback
            actual_path = std::filesystem::current_path() / actual_path.filename();
            file.open(actual_path);
            if (!file) {
                return "";
            }
        }

        std::string platform_str = get_platform();
        std::string compiler_str = get_compiler_info();

        file << "Huira Log\n";
        file << "=========\n\n";
        file << "Platform: " << platform_str << " | Compiler: " << compiler_str << "\n\n";

        size_t current_write = write_index_.load(std::memory_order_relaxed);
        size_t total_entries = std::min(current_write, buffer_.size());
        size_t start_index = (current_write > buffer_.size())
            ? current_write % buffer_.size()
            : 0;

        for (size_t i = 0; i < total_entries; ++i) {
            size_t index = (start_index + i) % buffer_.size();
            const auto& entry = buffer_[index];
            if (!entry.message.empty()) {
                file << entry.to_string() << '\n';
            }
        }

        file.close();
        return actual_path.string();
    }

    void Logger::enable_crash_handler(bool enable) {
        crash_handler_enabled_.store(enable, std::memory_order_relaxed);
        if (enable) {
            install_crash_handlers();
        }
    }

    // Console output configuration with hierarchy enforcement
    void Logger::enable_console_debug(bool enable) {
        console_debug_.store(enable, std::memory_order_relaxed);
        if (enable) {
            // If DEBUG is on, INFO and WARNING must also be on
            console_info_.store(true, std::memory_order_relaxed);
            console_warning_.store(true, std::memory_order_relaxed);
        }
    }

    void Logger::enable_console_info(bool enable) {
        console_info_.store(enable, std::memory_order_relaxed);
        if (enable) {
            // If INFO is on, WARNING must also be on
            console_warning_.store(true, std::memory_order_relaxed);
        } else {
            // If INFO is off, DEBUG must also be off
            console_debug_.store(false, std::memory_order_relaxed);
        }
    }

    void Logger::enable_console_warning(bool enable) {
        console_warning_.store(enable, std::memory_order_relaxed);
        if (!enable) {
            // If WARNING is off, INFO and DEBUG must also be off
            console_info_.store(false, std::memory_order_relaxed);
            console_debug_.store(false, std::memory_order_relaxed);
        }
    }

    bool Logger::is_console_debug_enabled() const {
        return console_debug_.load(std::memory_order_relaxed);
    }

    bool Logger::is_console_info_enabled() const {
        return console_info_.load(std::memory_order_relaxed);
    }

    bool Logger::is_console_warning_enabled() const {
        return console_warning_.load(std::memory_order_relaxed);
    }

    // ===== Crash Handler Implementation =====

    void Logger::output_crash_report(const std::string& log_path) {
        if (!log_path.empty()) {
            std::cerr << red("HUIRA UNCAUGHT EXCEPTION") << "\n";
            std::cerr << yellow(" - Log file written to: " + std::filesystem::absolute(log_path).string()) << "\n";
            std::cerr << yellow(" - If this was a SPICE error, consider reviewing your SPICE configuration\n");
            std::cerr << yellow(" - If you believe this is a bug with Huira, please report this issue:\n");
            std::cerr << "       " << blue("https://github.com/huira-render/huira/issues/new?template=bug_report.md") << "\n";
            std::cerr << yellow(" - Include the log file in your report.") << "\n";
        }
    }

    void Logger::handle_crash(int signal) {
        auto& logger = Logger::instance();
        if (!logger.crash_handler_enabled_.load(std::memory_order_relaxed)) {
            return;
        }
        // Only print crash report once
        bool expected = false;
        if (!Logger::crash_reported_.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
            // Already reported
            std::signal(signal, SIG_DFL);
            std::raise(signal);
            return;
        }
        logger.log(LogLevel::Error, "Crash detected with signal: " + std::to_string(signal));
        std::string log_path = logger.dump_to_file();
        output_crash_report(log_path);

        // Re-raise signal with default handler
        std::signal(signal, SIG_DFL);
        std::raise(signal);
    }

#ifdef _WIN32
    LONG WINAPI Logger::windows_exception_handler(EXCEPTION_POINTERS* exception_info) {
        auto& logger = Logger::instance();
        if (!logger.crash_handler_enabled_.load(std::memory_order_relaxed)) {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        std::ostringstream oss;
        oss << "Windows exception caught: 0x"
            << std::hex << std::uppercase
            << exception_info->ExceptionRecord->ExceptionCode;
        logger.log(LogLevel::Error, oss.str());

        std::string log_path = logger.dump_to_file();
        output_crash_report(log_path);

        return EXCEPTION_CONTINUE_SEARCH;
    }
#endif

    [[noreturn]] void Logger::handle_terminate() {
        auto& logger = Logger::instance();
        if (!logger.crash_handler_enabled_.load(std::memory_order_relaxed)) {
            std::abort();
        }
        // Only print crash report once
        bool expected = false;
        if (!Logger::crash_reported_.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
            std::abort();
        }
        try {
            auto exception_ptr = std::current_exception();
            if (exception_ptr) {
                try {
                    std::rethrow_exception(exception_ptr);
                } catch (const std::exception& e) {
                    logger.log(LogLevel::Error, std::string("Uncaught exception: ") + e.what());
                } catch (...) {
                    logger.log(LogLevel::Error, "Uncaught unknown exception");
                }
            } else {
                logger.log(LogLevel::Error, "Terminate called without active exception");
            }
        } catch (...) {
            // Failed to log the exception
        }
        std::string log_path = logger.dump_to_file();
        output_crash_report(log_path);
        std::abort();
    }

    void Logger::install_crash_handlers() {
        // Suppress C5039: passing potentially-throwing function to extern C function
        // Our handlers are carefully designed to catch all exceptions internally
#ifdef _MSC_VER
        #pragma warning(push)
        #pragma warning(disable: 5039)
#endif

        std::signal(SIGSEGV, handle_crash);
        std::signal(SIGABRT, handle_crash);
        std::signal(SIGFPE, handle_crash);
        std::signal(SIGILL, handle_crash);

#ifndef _WIN32
        std::signal(SIGBUS, handle_crash);  // Unix/macOS only
#endif

        std::set_terminate(handle_terminate);

#ifdef _WIN32
        SetUnhandledExceptionFilter(windows_exception_handler);
#endif

#ifdef _MSC_VER
        #pragma warning(pop)
#endif
    }

}
