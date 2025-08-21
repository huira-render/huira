#pragma once

#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <fstream>
#include <vector>

namespace huira::detail {

    // Breadcrumb entry structure
    struct BreadcrumbEntry {
        std::chrono::system_clock::time_point timestamp;
        std::string message;
        std::string level;
        std::thread::id thread_id;

        BreadcrumbEntry(const std::string& msg, const std::string& lvl = "INFO");
    };

    // Singleton breadcrumb logger
    class BreadcrumbLogger {
    public:
        ~BreadcrumbLogger();

        static BreadcrumbLogger* getInstance();

        void addBreadcrumb(const std::string& message, const std::string& level = "INFO");

        void clearBreadcrumbs();

        size_t getBreadcrumbCount() const;

        void handleFatalError(const std::string& message, const std::string& details = "");


    private:
        std::vector<BreadcrumbEntry> breadcrumbs_;
        mutable std::mutex breadcrumbs_mutex_;
        std::string temp_log_path_;
        std::atomic<bool> crash_occurred_{ false };
        std::atomic<bool> signals_registered_{ false };

        static std::mutex instance_mutex_;
        static std::unique_ptr<BreadcrumbLogger> instance_;
        
        BreadcrumbLogger();

        // Explicitly delete copy constructor and assignment operator
        BreadcrumbLogger(const BreadcrumbLogger&) = delete;
        BreadcrumbLogger& operator=(const BreadcrumbLogger&) = delete;
        BreadcrumbLogger(BreadcrumbLogger&&) = delete;
        BreadcrumbLogger& operator=(BreadcrumbLogger&&) = delete;

        void registerHandlers();

        // Signal handler must be C-compatible (noexcept and no exceptions)
        static void signalHandler(int signal) noexcept;

        // Exit handler must be C-compatible (noexcept and no exceptions)
        static void exitHandler() noexcept;

        void handleCrash(const std::string& reason);

        std::string generateLogFileName() const;

        void dumpToLogFile(const std::string& crash_reason) const;

        void writeCrashHeader(std::ofstream& file, const std::string& crash_reason) const;

        void writeBreadcrumbs(std::ofstream& file) const;

        void writeEnvironmentInfo(std::ofstream& file) const;

        void writeBuildInfo(std::ofstream& file) const;

        void writeStackTrace(std::ofstream& file) const;

        void cleanup();
    };

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
}

// Convenience macros for easy logging
#define HUIRA_LOG_DEBUG(msg) huira::detail::logDebug(msg)
#define HUIRA_LOG_INFO(msg) huira::detail::logInfo(msg)
#define HUIRA_LOG_WARNING(msg) huira::detail::logWarning(msg)
#define HUIRA_LOG_BREADCRUMB(msg, level) huira::detail::logBreadcrumb(msg, level)