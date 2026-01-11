#pragma once

#include <chrono>
#include <string>

namespace huira::detail {

    std::string getPlatform();

    std::string getCompilerInfo();

    std::string getMemoryUsage();

#ifdef _WIN32
#include <process.h>
#define GET_PID() _getpid()
#else
#include <unistd.h>
#define GET_PID() getpid()
#endif

    std::string getTimeAsString(const std::chrono::system_clock::time_point& tp, const std::string fmt = "%Y-%m-%d %H:%M:%S");

    std::string getTimeAsString(const std::string& fmt = "%Y-%m-%d %H:%M:%S");
}

#include "huira_impl/detail/platform/info.ipp"
