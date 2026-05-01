#pragma once
#include "huira/util/logger.hpp"
#include "pybind11/pybind11.h"
#include "pybind11/stl/filesystem.h"

namespace py = pybind11;
namespace huira {

inline void bind_logger(py::module_& m)
{
    // Bind the LogLevel enum
    py::enum_<LogLevel>(m, "LogLevel", "Severity levels for Huira log messages")
        .value("Debug", LogLevel::Debug)
        .value("Info", LogLevel::Info)
        .value("Warning", LogLevel::Warning)
        .value("Error", LogLevel::Error)
        .export_values();

    // Bind Configuration Functions
    m.def("set_log_level",
          &set_log_level,
          py::arg("level"),
          "Set the minimum log level for the global logger.");

    m.def("set_log_buffer_size",
          &set_log_buffer_size,
          py::arg("size"),
          "Set the circular buffer size for the global logger.");

    // pybind11 requires explicit py::arg() default values if they exist in C++
    m.def("dump_log",
          &dump_log,
          py::arg("filepath") = "",
          "Dump all buffered log entries to a file.");

    // Console toggles
    m.def("enable_console_debug", &enable_console_debug, py::arg("enable") = true);
    m.def("enable_console_info", &enable_console_info, py::arg("enable") = true);
    m.def("enable_console_warning", &enable_console_warning, py::arg("enable") = true);

    // Expose Logging Capability to Python
    m.def(
        "log",
        [](LogLevel level, const std::string& message) {
            // Route Python strings directly into the C++ singleton
            Logger::log(level, message);
        },
        py::arg("level"),
        py::arg("message"),
        "Log a message from Python to the Huira logger.");
}
} // namespace huira
