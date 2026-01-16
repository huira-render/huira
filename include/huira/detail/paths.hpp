#pragma once

#include <string>
#include <optional>
#include <filesystem>

namespace huira {
    class Paths {
    public:
        static Paths& instance();

        std::string data_dir() const;

        void set_data_dir(const std::string& path);

        void reset_data_dir();

        static std::string executable_path();

        static std::string executable_dir();

        static std::string relative_to_executable(const std::string& relativePath);

    private:
        Paths() = default;
        std::optional<std::string> data_dir_override_;
    };

    // Convenience free functions
    inline std::string data_dir() { return Paths::instance().data_dir(); }
    inline void set_data_dir(const std::string& p) { Paths::instance().set_data_dir(p); }
    inline void reset_data_dir() { Paths::instance().reset_data_dir(); }

}

#include "huira_impl/detail/paths.ipp"
