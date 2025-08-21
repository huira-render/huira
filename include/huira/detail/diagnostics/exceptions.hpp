#pragma once

#include <stdexcept>

#include "huira/detail/diagnostics/logging.hpp"

namespace huira::detail {
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