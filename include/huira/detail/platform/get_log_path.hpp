#pragma once

#include <string>
#include <filesystem>

namespace huira::detail {
    // Get the appropriate directory for log files
    std::filesystem::path get_log_directory();
    
    // Get a full path for a log file with the given filename
    std::filesystem::path get_log_file_path(const std::string& filename = "");
}

#include "huira_impl/detail/platform/get_log_path.ipp"
