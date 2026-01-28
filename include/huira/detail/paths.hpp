#pragma once

#include <optional>
#include <filesystem>

namespace fs = std::filesystem;

namespace huira {
    class Paths {
    public:
        static Paths& instance();

        fs::path data_dir() const;

        void set_data_dir(const fs::path& path);

        void reset_data_dir();

        static fs::path executable_path();

        static fs::path executable_dir();

        static fs::path relative_to_executable(const fs::path& relativePath);

    private:
        Paths() = default;
        std::optional<fs::path> data_dir_override_;
    };

    // Convenience free functions
    inline fs::path data_dir() { return Paths::instance().data_dir(); }
    inline void set_data_dir(const fs::path& p) { Paths::instance().set_data_dir(p); }
    inline void reset_data_dir() { Paths::instance().reset_data_dir(); }

    inline void make_path(fs::path path);
}

#include "huira_impl/detail/paths.ipp"
