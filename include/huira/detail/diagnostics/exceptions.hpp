#pragma once

#include <stdexcept>
#include <string>
#include <chrono>
#include <sstream>
#include <fstream>
#include <thread>
#include <iostream>
#include <filesystem>

#include "cpptrace/cpptrace.hpp"

#include "huira/detail/platform/info.hpp"
#include "huira/detail/platform/paths.hpp"

namespace fs = std::filesystem;

namespace huira {
    class FatalError : public std::exception {
    public:
        FatalError(const std::string& message, const std::string& details = "")
			: message_(message), details_(details) 
        {
            captureStackTrace();
            logToFile();
            logToConsole();
        }

        const char* what() const noexcept override {
            return message_.c_str();
        }

        const std::string& getFullDiagnostics() const {
            static std::string full_diagnostics;
            if (full_diagnostics.empty()) {
                std::ostringstream oss;

                oss << "\n=== FATAL ERROR ===" << std::endl;
				oss << "Timestamp: " << std::chrono::system_clock::now() << std::endl;
                oss << "Message: " << message_ << std::endl;
                if (!details_.empty()) {
                    oss << "Details: " << details_ << std::endl;
                }

                oss << "\n=== ENVIRONMENT ===" << std::endl;
                oss << getEnvironmentDetails();

				oss << "\n=== BUILD INFO ===" << std::endl;
                oss << getBuildInfo();

                oss << "\n=== CALL STACK ===" << std::endl;
                oss << stack_trace_ << std::endl;

                full_diagnostics = oss.str();
            }
            return full_diagnostics;
        }

    protected:
        std::string message_;
        std::string details_;

        std::string stack_trace_;
        std::thread::id thread_id_;

        std::string gatherSpecificDiagnostics() const { return ""; }

    private:
        void captureStackTrace() {
            auto st = cpptrace::stacktrace::current();

            std::ostringstream oss;
            st.print(oss);
            stack_trace_ = oss.str();
        }

        std::string getEnvironmentDetails() const {
            std::ostringstream oss;
            
            oss << getPlatform() << std::endl;

            oss << "Working directory: " << fs::current_path().string() << std::endl;
            oss << "Executable path: " << getExecutablePath() << std::endl;
			oss << "CPU cores: " << std::thread::hardware_concurrency() << std::endl;

            oss << getMemoryUsage() << std::endl;

            std::error_code ec;
            auto space = fs::space(fs::current_path(), ec);
            if (!ec) {
                oss << "Available disk space: " << (space.available / 1024 / 1024) << " MB" << std::endl;
            }
            else {
                oss << "Available disk space: UNKNOWN" << std::endl;
            }

            return oss.str();
		}

        std::string getBuildInfo() const {
            std::ostringstream oss;
            oss << "Huira version: " << HUIRA_VERSION << std::endl;
			oss << "Compiler: " << getCompilerInfo() << std::endl;

#ifdef NDEBUG
            oss << "Build type: Release" << std::endl;
#else
            oss << "Build type: Debug" << std::endl;
#endif

            return oss.str();
		}

        void logToFile() const {
            try {
                // Use crash-safe append mode
				std::string log_name = "fatal_errors.log";
                std::ofstream log_file(log_name, std::ios::app);
                if (log_file.is_open()) {
                    log_file << getFullDiagnostics() << std::endl << std::endl;
                    log_file.flush();
                }

                fs::path output_path = fs::current_path() / log_name;
                std::cerr << "Huira encountered a fatal error.  Error log has been saved to: " << output_path.string() << std::endl;
            }
            catch (...) {
                std::cerr << "Huira encountered a fatal error, but failed to write to log file!  " <<
                    "Check permissions/storage of " << fs::current_path().string() << std::endl;
            }
        }

        void logToConsole() const {
            std::cerr << getFullDiagnostics() << std::endl;
        }
    };
}