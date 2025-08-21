#include "huira/detail/diagnostics/logging.hpp"

#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <filesystem>
#include <memory>
#include <sstream>
#include <csignal>
#include <cstdlib>
#include <iomanip>

#include "cpptrace/cpptrace.hpp"
#include "huira/detail/platform/info.hpp"
#include "huira/detail/platform/paths.hpp"
#include "huira/detail/text/colors.hpp"

namespace fs = std::filesystem;

namespace huira::detail {
    // =================================== //
    // === BreadcrumbEntry Constructor === //
    // =================================== //
    BreadcrumbEntry::BreadcrumbEntry(const std::string& msg, const std::string& lvl)
        : timestamp(std::chrono::system_clock::now())
        , message(msg)
        , level(lvl)
        , thread_id(std::this_thread::get_id())
    {

    }


    // ======================================= //
    // === BreadcrumbLogger Public Members === //
    // ======================================= //
    BreadcrumbLogger::~BreadcrumbLogger()
    {
        if (!crash_occurred_) {
            cleanup();
        }
    }

    BreadcrumbLogger* BreadcrumbLogger::getInstance() {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (!instance_) {
            instance_ = std::unique_ptr<BreadcrumbLogger>(new BreadcrumbLogger());
        }
        return instance_.get();
    }

    void BreadcrumbLogger::addBreadcrumb(const std::string& message, const std::string& level) {
        std::lock_guard<std::mutex> lock(breadcrumbs_mutex_);
        breadcrumbs_.emplace_back(message, level);

        // Keep only the last 1000 breadcrumbs to avoid memory issues
        if (breadcrumbs_.size() > 1000) {
            breadcrumbs_.erase(breadcrumbs_.begin(), breadcrumbs_.begin() + 100);
        }
    }

    void BreadcrumbLogger::clearBreadcrumbs() {
        std::lock_guard<std::mutex> lock(breadcrumbs_mutex_);
        breadcrumbs_.clear();
    }

    size_t BreadcrumbLogger::getBreadcrumbCount() const {
        std::lock_guard<std::mutex> lock(breadcrumbs_mutex_);
        return breadcrumbs_.size();
    }

    // Handle FatalError crashes
    void BreadcrumbLogger::handleFatalError(const std::string& message, const std::string& details) {
        handleCrash("FatalError: " + message + (details.empty() ? "" : " (" + details + ")"));
    }


    // ======================================== //
    // === BreadcrumbLogger Private Members === //
    // ============================-=========== //
    std::unique_ptr<BreadcrumbLogger> BreadcrumbLogger::instance_;
    std::mutex BreadcrumbLogger::instance_mutex_;

    BreadcrumbLogger::BreadcrumbLogger() {
        // Create temporary log file path
        temp_log_path_ = (fs::temp_directory_path() /
            ("huira_breadcrumbs_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".tmp")).string();

        // Register signal handlers and exit handler
        registerHandlers();
    }

    void BreadcrumbLogger::registerHandlers() {
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
    void BreadcrumbLogger::signalHandler(int signal) noexcept {
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
    void BreadcrumbLogger::exitHandler() noexcept {
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

    void BreadcrumbLogger::handleCrash(const std::string& reason) {
        if (crash_occurred_.exchange(true)) {
            return; // Already handling a crash
        }

        try {
            dumpToLogFile(reason);
        }
        catch (...) {
            // Last resort - try to write something to stderr
            std::cerr << red("FATAL: Crash occurred but failed to write log file") << std::endl;
        }
    }

    std::string BreadcrumbLogger::generateLogFileName() const {
        std::ostringstream oss;
        oss << "crash_" + getTimeAsString("%Y%m%d%H%M%S") + ".txt";
        return oss.str();
    }

    void BreadcrumbLogger::dumpToLogFile(const std::string& crash_reason) const {
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
            std::cerr << red("FATAL ERROR OCCURRED\n");
            std::cerr << yellow("Crash log saved to: ") << log_file_path.string() << std::endl;

            // Check if stack trace quality is poor and suggest debug build
#ifndef HAS_DEBUG_SYMBOLS
            std::cerr << "\nNOTE: Stack trace may be incomplete due to missing debug symbols.\n";
            std::cerr << "Before submitting a bug report, consider rebuilding with debug symbols for better crash diagnostics:\n";
            std::cerr << "  GCC/Clang: -g -O2 (or full debug: -g -O0)\n";
            std::cerr << "  MSVC: /Zi /DEBUG (Release with debug info)\n\n";
#endif

            std::cerr << yellow("Please report this issue at: ") <<
                hyperlink("https://github.com/huira-render/huira/issues/new?template=bug_report.md") <<
                yellow("\nInclude the log file when reporting.") << std::endl;

        }
        catch (const std::exception& e) {
            std::cerr << "Failed to write crash log: " << e.what() << std::endl;
        }
    }

    void BreadcrumbLogger::writeCrashHeader(std::ofstream& file, const std::string& crash_reason) const {
        file << "=== HUIRA CRASH LOG ===" << std::endl;
        file << "Timestamp: " << getTimeAsString() << std::endl;
        file << "Crash Reason: " << crash_reason << std::endl;
        file << "Process ID: " << GET_PID() << std::endl;
        file << std::endl;
    }

    void BreadcrumbLogger::writeBreadcrumbs(std::ofstream& file) const {
        std::lock_guard<std::mutex> lock(breadcrumbs_mutex_);

        file << "=== BREADCRUMB TRAIL ===" << std::endl;
        file << "Total entries: " << breadcrumbs_.size() << std::endl;
        file << std::endl;

        if (breadcrumbs_.empty()) {
            file << "No breadcrumbs recorded." << std::endl;
        }
        else {
            for (const auto& entry : breadcrumbs_) {
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    entry.timestamp.time_since_epoch()) % 1000;
                
                file << "[" << getTimeAsString(entry.timestamp)
                    << "." << std::setfill('0') << std::setw(3) << ms.count()
                    << "] [" << entry.level << "] [Thread-" << entry.thread_id << "] "
                    << entry.message << std::endl;
            }
        }
        file << std::endl;
    }

    void BreadcrumbLogger::writeEnvironmentInfo(std::ofstream& file) const {
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

    void BreadcrumbLogger::writeBuildInfo(std::ofstream& file) const {
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

    void BreadcrumbLogger::writeStackTrace(std::ofstream& file) const {
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

    void BreadcrumbLogger::cleanup() {
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
}