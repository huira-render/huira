#pragma once

#include <string>

namespace huira::detail {

    std::string getPlatform();

    std::string getCompilerInfo();

    std::string getMemoryUsage();
}