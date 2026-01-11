#pragma once

#include <stdexcept>
#include <string>

namespace huira {

    class FatalError : public std::runtime_error {
    public:
        FatalError(const std::string& message, const std::string& detail)
            : std::runtime_error(message + ": " + detail) {}
    };

}