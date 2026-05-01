#pragma once

#include <filesystem>
#include <string>

namespace huira {
// Get a full path for a log file with the given filename
std::filesystem::path get_log_file_path(const std::string& filename = "");
} // namespace huira

#include "huira_impl/platform/get_log_path.ipp"
