#include "huira/detail/platform/paths.hpp"

// These come from CMake
#ifndef HUIRA_DEFAULT_DATA_DIR
#define HUIRA_DEFAULT_DATA_DIR ""
#endif

namespace huira {

    Paths& Paths::instance() {
        static Paths instance;
        return instance;
    }

    std::string Paths::executable_path() {
        return detail::get_executable_path();
    }

    std::string Paths::executable_dir() {
        return std::filesystem::path(executable_path()).parent_path().string();
    }

    std::string Paths::relative_to_executable(const std::string& relativePath) {
        namespace fs = std::filesystem;
        auto resolved = fs::path(executable_dir()) / relativePath;
        return fs::weakly_canonical(resolved).string();
    }

    std::string Paths::data_dir() const {
        if (data_dir_override_) {
            return *data_dir_override_;
        }
        return HUIRA_DEFAULT_DATA_DIR;
    }

    void Paths::set_data_dir(const std::string& path) {
        namespace fs = std::filesystem;

        // If it's a relative path, resolve it relative to executable
        fs::path p(path);
        if (p.is_relative()) {
            data_dir_override_ = relative_to_executable(path);
        }
        else {
            data_dir_override_ = path;
        }
    }

    void Paths::reset_data_dir() {
        data_dir_override_.reset();
    }

}
