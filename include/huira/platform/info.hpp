#pragma once

#include <chrono>
#include <string>

namespace huira {

    std::string get_platform();

    std::string get_compiler_info();

    std::string get_memory_usage();

#ifdef _WIN32
#include <process.h>
#define GET_PID() _getpid()
#else
#include <unistd.h>
#define GET_PID() getpid()
#endif

    std::string get_time_as_string(const std::chrono::system_clock::time_point& tp, const std::string fmt = "%Y-%m-%d %H:%M:%S");

    std::string get_time_as_string(const std::string& fmt = "%Y-%m-%d %H:%M:%S");
}

#include "huira_impl/platform/info.ipp"
