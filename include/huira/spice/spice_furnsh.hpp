#pragma once

#include <filesystem>

#include "cspice/SpiceUsr.h"

#include "huira/spice/spice_error.hpp"

namespace fs = std::filesystem;

namespace huira::spice {
    inline void furnsh(const fs::path& file_path) {
        call_spice(furnsh_c, file_path.string().c_str());
    }

    inline void furnsh_relative_to_file(const fs::path& kernel_path) {
        if (!kernel_path.has_parent_path()) {
            furnsh(kernel_path);
            return;
        }

        struct DirectoryGuard {
            fs::path original;
            DirectoryGuard() : original(fs::current_path()) {}
            ~DirectoryGuard() { fs::current_path(original); }
        };

        DirectoryGuard guard;
        fs::current_path(kernel_path.parent_path());
        call_spice([&]() { furnsh_c(kernel_path.filename().string().c_str()); });
    }
}
