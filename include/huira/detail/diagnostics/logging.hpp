#pragma once

#include <stdexcept>
#include <string>
#include <chrono>
#include <sstream>
#include <fstream>
#include <thread>
#include <iostream>
#include <filesystem>
#include <vector>
#include <mutex>
#include <memory>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <atomic>

#include "cpptrace/cpptrace.hpp"
#include "huira/detail/platform/info.hpp"
#include "huira/detail/platform/paths.hpp"
#include "huira/detail/text/colors.hpp"

namespace fs = std::filesystem;

namespace huira::detail {

    // Breadcrumb entry structure
    struct BreadcrumbEntry {
        std::chrono::system_clock::time_point timestamp;
        std::string message;
        std::string level;
        std::thread::id thread_id;

        BreadcrumbEntry(const std::string& msg, const std::string& lvl = "INFO")
            : timestamp(std::chrono::system_clock::now())
            , message(msg)
            , level(lvl)
            , thread_id(std::this_thread::get_id())
        {
        }
    };

    // Singleton breadcrumb logger
    class BreadcrumbLogger {
    public:
        ~BreadcrumbLogger() {
            if (!crash_occurred_) {
                cleanup();
            }
        }

        static BreadcrumbLogger* getInstance() {
            std::lock_guard<std::mutex> lock(instance_mutex_);
            if (!instance_) {
                instance_ = std::unique_ptr<BreadcrumbLogger>(new BreadcrumbLogger());
            }
            return instance_.get();
        }

        void addBreadcrumb(const std::string& message, const std::string& level = "INFO") {
            std::lock_guard<std::mutex> lock(breadcrumbs_mutex_);
            breadcrumbs_.emplace_back(message, level);

            // Keep only the last 1000 breadcrumbs to avoid memory issues
            if (breadcrumbs_.size() > 1000) {
                breadcrumbs_.erase(breadcrumbs_.begin(), breadcrumbs_.begin() + 100);
            }
        }

        void clearBreadcrumbs() {
            std::lock_guard<std::mutex> lock(breadcrumbs_mutex_);
            breadcrumbs_.clear();
        }

        size_t getBreadcrumbCount() const {
            std::lock_guard<std::mutex> lock(breadcrumbs_mutex_);
            return breadcrumbs_.size();
        }

        // Handle FatalError crashes
        void handleFatalError(const std::string& message, const std::string& details = "") {
            handleCrash("FatalError: " + message + (details.empty() ? "" : " (" + details + ")"));
        }


    private:
        std::vector<BreadcrumbEntry> breadcrumbs_;
        mutable std::mutex breadcrumbs_mutex_;
        std::string temp_log_path_;
        std::atomic<bool> crash_occurred_{ false };
        std::atomic<bool> signals_registered_{ false };

        static std::unique_ptr<BreadcrumbLogger> instance_;
        static std::mutex instance_mutex_;

        BreadcrumbLogger() {
            // Create temporary log file path
            temp_log_path_ = (fs::temp_directory_path() /
                ("huira_breadcrumbs_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".tmp")).string();

            // Register signal handlers and exit handler
            registerHandlers();
        }

        // Explicitly delete copy constructor and assignment operator
        BreadcrumbLogger(const BreadcrumbLogger&) = delete;
        BreadcrumbLogger& operator=(const BreadcrumbLogger&) = delete;
        BreadcrumbLogger(BreadcrumbLogger&&) = delete;
        BreadcrumbLogger& operator=(BreadcrumbLogger&&) = delete;

        void registerHandlers() {
            if (signals_registered_.exchange(true)) {
                return; // Already registered
            }

            // Register signal handlers for crashes (C-style function pointers)
            std::signal(SIGSEGV, signalHandler);
            std::signal(SIGABRT, signalHandler);
            std::signal(SIGFPE, signalHandler);
            std::signal(SIGILL, signalHandler);
#ifdef SIGBUS
            std::signal(SIGBUS, signalHandler);
#endif
#ifdef SIGSYS
            std::signal(SIGSYS, signalHandler);
#endif

            // Register normal exit handler (C-style function pointer)
            std::atexit(exitHandler);
        }

        // Signal handler must be C-compatible (noexcept and no exceptions)
        static void signalHandler(int signal) noexcept {
            try {
                auto logger = getInstance();
                if (logger) {
                    logger->handleCrash("Signal " + std::to_string(signal) + " received");
                }
            }
            catch (...) {
                // Cannot throw from signal handler - just continue
            }

            // Re-raise the signal with default handler
            std::signal(signal, SIG_DFL);
            std::raise(signal);
        }

        // Exit handler must be C-compatible (noexcept and no exceptions)
        static void exitHandler() noexcept {
            try {
                auto logger = getInstance();
                if (logger && !logger->crash_occurred_) {
                    // Normal exit - clean up temp file
                    logger->cleanup();
                }
            }
            catch (...) {
                // Cannot throw from exit handler - just continue
            }
        }

        void handleCrash(const std::string& reason) {
            if (crash_occurred_.exchange(true)) {
                return; // Already handling a crash
            }

            try {
                dumpToLogFile(reason);
            }
            catch (...) {
                // Last resort - try to write something to stderr
                std::cerr << detail::red("FATAL: Crash occurred but failed to write log file") << std::endl;
            }
        }

        std::string generateLogFileName() const {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;

            std::ostringstream oss;
            oss << "crash_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
                << "_" << std::setfill('0') << std::setw(3) << ms.count() << ".log";
            return oss.str();
        }

        void dumpToLogFile(const std::string& crash_reason) const {
            try {
                // Ensure log-files directory exists
                fs::path log_dir = fs::current_path() / "log-files";
                fs::create_directories(log_dir);

                // Generate unique log file name
                fs::path log_file_path = log_dir / generateLogFileName();

                // Write comprehensive crash log
                std::ofstream log_file(log_file_path);
                if (!log_file.is_open()) {
                    throw std::runtime_error("Could not open log file: " + log_file_path.string());
                }

                writeCrashHeader(log_file, crash_reason);
                writeBreadcrumbs(log_file);
                writeEnvironmentInfo(log_file);
                writeBuildInfo(log_file);
                writeStackTrace(log_file);

                log_file.flush();

                // Notify user
                std::cerr << detail::red("FATAL ERROR OCCURRED\n");
                std::cerr << detail::yellow("Crash log saved to: ") << log_file_path.string() << std::endl;

                // Check if stack trace quality is poor and suggest debug build
#ifndef HAS_DEBUG_SYMBOLS
                std::cerr << "\nNOTE: Stack trace may be incomplete due to missing debug symbols.\n";
                std::cerr << "Before submitting a bug report, consider rebuilding with debug symbols for better crash diagnostics:\n";
                std::cerr << "  GCC/Clang: -g -O2 (or full debug: -g -O0)\n";
                std::cerr << "  MSVC: /Zi /DEBUG (Release with debug info)\n\n";
#endif

                std::cerr << detail::yellow("Please report this issue at: ") <<
                    detail::hyperlink("https://github.com/huira-render/huira/issues/new?template=bug_report.md") <<
                    detail::yellow("\nInclude the log file when reporting.") << std::endl;

            }
            catch (const std::exception& e) {
                std::cerr << "Failed to write crash log: " << e.what() << std::endl;
            }
        }

        void writeCrashHeader(std::ofstream& file, const std::string& crash_reason) const {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);

            file << "=== HUIRA CRASH LOG ===" << std::endl;
            file << "Timestamp: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << std::endl;
            file << "Crash Reason: " << crash_reason << std::endl;
            file << "Process ID: " << getpid() << std::endl;
            file << std::endl;
        }

        void writeBreadcrumbs(std::ofstream& file) const {
            std::lock_guard<std::mutex> lock(breadcrumbs_mutex_);

            file << "=== BREADCRUMB TRAIL ===" << std::endl;
            file << "Total entries: " << breadcrumbs_.size() << std::endl;
            file << std::endl;

            if (breadcrumbs_.empty()) {
                file << "No breadcrumbs recorded." << std::endl;
            }
            else {
                for (const auto& entry : breadcrumbs_) {
                    auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        entry.timestamp.time_since_epoch()) % 1000;

                    file << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S")
                        << "." << std::setfill('0') << std::setw(3) << ms.count()
                        << "] [" << entry.level << "] [Thread-" << entry.thread_id << "] "
                        << entry.message << std::endl;
                }
            }
            file << std::endl;
        }

        void writeEnvironmentInfo(std::ofstream& file) const {
            file << "=== ENVIRONMENT ===" << std::endl;
            file << getPlatform() << std::endl;
            file << "Working directory: " << fs::current_path().string() << std::endl;
            file << "Executable path: " << getExecutablePath() << std::endl;
            file << "CPU cores: " << std::thread::hardware_concurrency() << std::endl;
            file << getMemoryUsage() << std::endl;

            std::error_code ec;
            auto space = fs::space(fs::current_path(), ec);
            if (!ec) {
                file << "Available disk space: " << (space.available / 1024 / 1024) << " MB" << std::endl;
            }
            else {
                file << "Available disk space: UNKNOWN" << std::endl;
            }
            file << std::endl;
        }

        void writeBuildInfo(std::ofstream& file) const {
            file << "=== BUILD INFO ===" << std::endl;
            file << "Huira version: " << HUIRA_VERSION << std::endl;
            file << "Compiler: " << getCompilerInfo() << std::endl;
#ifdef NDEBUG
            file << "Build type: Release" << std::endl;
#else
            file << "Build type: Debug" << std::endl;
#endif
            file << std::endl;
        }

        void writeStackTrace(std::ofstream& file) const {
            file << "=== CALL STACK ===" << std::endl;
            try {
                auto st = cpptrace::stacktrace::current();

                st.print(file);
            }
            catch (...) {
                file << "Failed to capture stack trace" << std::endl;
            }
            file << std::endl;
        }

        bool analyzeStackTrace(const cpptrace::stacktrace& st) const {
            if (st.frames.empty()) {
                return false; // No frames at all
            }

            // Count frames with meaningful symbol information
            size_t frames_with_symbols = 0;
            size_t frames_with_source_info = 0;
            size_t total_relevant_frames = 0;

            for (const auto& frame : st.frames) {
                // Skip system/library frames that we don't care about
                if (isSystemFrame(frame)) {
                    continue;
                }

                total_relevant_frames++;

                // Check if we have a meaningful symbol name
                if (hasUsefulSymbol(frame)) {
                    frames_with_symbols++;
                }

                // Check if we have source file and line information
                if (hasSourceInfo(frame)) {
                    frames_with_source_info++;
                }
            }

            if (total_relevant_frames == 0) {
                return false; // Only system frames
            }

            // Consider the stacktrace useful if we have symbols for at least 60% of frames
            // or source info for at least 30% of frames
            double symbol_ratio = static_cast<double>(frames_with_symbols) / static_cast<double>(total_relevant_frames);
            double source_ratio = static_cast<double>(frames_with_source_info) / static_cast<double>(total_relevant_frames);

            return (symbol_ratio >= 0.6) || (source_ratio >= 0.3);
        }

        bool isSystemFrame(const cpptrace::stacktrace_frame& frame) const {
            const std::string& symbol = frame.symbol;
            const std::string& filename = frame.filename;

            // Skip frames without any identifying information
            if (symbol.empty() && filename.empty()) {
                return true;
            }

            // Common system/runtime patterns to ignore
            static const std::vector<std::string> system_patterns = {
                "__libc_start_main",
                "_start",
                "main",  // Sometimes we want to skip the main function
                "__static_initialization_and_destruction",
                "__cxa_",
                "_dl_",
                "ld-linux",
                "libc.so",
                "libstdc++.so",
                "libgcc_s.so",
                "kernel32.dll",
                "ntdll.dll",
                "msvcrt.dll"
            };

            // Check symbol patterns
            for (const auto& pattern : system_patterns) {
                if (symbol.find(pattern) != std::string::npos ||
                    filename.find(pattern) != std::string::npos) {
                    return true;
                }
            }

            // Platform-specific system paths
#ifdef _WIN32
            if (filename.find("C:\\Windows\\") == 0 ||
                filename.find("c:\\windows\\") == 0) {
                return true;
            }
#else
            if (filename.find("/usr/") == 0 ||
                filename.find("/lib/") == 0 ||
                filename.find("/lib64/") == 0) {
                return true;
            }
#endif

            return false;
        }

        bool hasUsefulSymbol(const cpptrace::stacktrace_frame& frame) const {
            const std::string& symbol = frame.symbol;

            if (symbol.empty()) {
                return false;
            }

            // Check for mangled C++ symbols (usually indicates good debug info)
            if (symbol.find("_Z") == 0) {
                return true;
            }

            // Check for readable function names (not just addresses)
            if (symbol.find("0x") == 0) {
                return false; // Just a raw address
            }

            // Look for patterns that indicate meaningful symbols
            if (symbol.find("::") != std::string::npos ||  // C++ scope resolution
                symbol.find("(") != std::string::npos ||   // Function parameters
                symbol.length() > 8) {                     // Reasonable function name length
                return true;
            }

            return false;
        }

        bool hasSourceInfo(const cpptrace::stacktrace_frame& frame) const {
            // Check if we have both filename and line number
            return !frame.filename.empty() &&
                frame.line.has_value() &&
                frame.line.value() > 0 &&
                frame.filename != "<unknown>";
        }

        void cleanup() {
            // Remove temporary log file if it exists
            try {
                if (fs::exists(temp_log_path_)) {
                    fs::remove(temp_log_path_);
                }
            }
            catch (...) {
                // Ignore cleanup errors
            }
        }
    };

    // Static member definitions
    std::unique_ptr<BreadcrumbLogger> BreadcrumbLogger::instance_;
    std::mutex BreadcrumbLogger::instance_mutex_;

    // Initialization function for explicit setup
    inline void initializeBreadcrumbLogger() {
        BreadcrumbLogger::getInstance(); // Force creation and signal handler registration
    }

    // Convenience functions
    inline void logBreadcrumb(const std::string& message, const std::string& level = "INFO") {
        if (auto logger = BreadcrumbLogger::getInstance()) {
            logger->addBreadcrumb(message, level);
        }
    }

    inline void logDebug(const std::string& message) {
        logBreadcrumb(message, "DEBUG");
    }

    inline void logInfo(const std::string& message) {
        logBreadcrumb(message, "INFO");
    }

    inline void logWarning(const std::string& message) {
        logBreadcrumb(message, "WARNING");
    }

    inline void logError(const std::string& message) {
        logBreadcrumb(message, "ERROR");
    }

    // Updated FatalError class
    class FatalError : public std::exception {
    public:
        [[noreturn]] FatalError(const std::string& message, const std::string& details = "")
            : message_(message), details_(details)
        {
            // Log the fatal error as a breadcrumb first
            logBreadcrumb("FATAL ERROR: " + message + (details.empty() ? "" : " (" + details + ")"), "FATAL");

            // Trigger crash log generation
            if (auto logger = BreadcrumbLogger::getInstance()) {
                logger->handleFatalError(message, details);
            }

            // Terminate the program
            std::terminate();
        }

        const char* what() const noexcept override {
            return message_.c_str();
        }

    private:
        std::string message_;
        std::string details_;
    };
}

// Convenience macros for easy logging
#define HUIRA_LOG_DEBUG(msg) huira::detail::logDebug(msg)
#define HUIRA_LOG_INFO(msg) huira::detail::logInfo(msg)
#define HUIRA_LOG_WARNING(msg) huira::detail::logWarning(msg)
#define HUIRA_LOG_BREADCRUMB(msg, level) huira::detail::logBreadcrumb(msg, level)