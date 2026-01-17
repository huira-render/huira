#pragma once

#include <string>

namespace huira::detail {
	std::string get_executable_path();
}

#include "huira_impl/detail/platform/get_exe_path.ipp"
