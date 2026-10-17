#include <filesystem>

#include "huira/platform/get_exe_path.hpp"
#include "huira/util/macros.hpp"

namespace fs = std::filesystem;

// These come from CMake
#ifndef HUIRA_DEFAULT_DATA_DIR
#define HUIRA_DEFAULT_DATA_DIR ""
#endif

namespace huira {

inline Paths& Paths::instance()
{
    HUIRA_PER_MODULE_STATE_BEGIN
    static Paths instance;
    HUIRA_PER_MODULE_STATE_END
    return instance;
}

inline fs::path Paths::executable_path()
{
    return get_executable_path();
}

inline fs::path Paths::executable_dir()
{
    return std::filesystem::path(executable_path()).parent_path().string();
}

inline fs::path Paths::relative_to_executable(const fs::path& relativePath)
{
    namespace fs = std::filesystem;
    auto resolved = fs::path(executable_dir()) / relativePath;
    return fs::weakly_canonical(resolved).string();
}

inline fs::path Paths::data_dir() const
{
    if (data_dir_override_) {
        return *data_dir_override_;
    }
    return HUIRA_DEFAULT_DATA_DIR;
}

inline void Paths::set_data_dir(const fs::path& path)
{
    namespace fs = std::filesystem;

    // If it's a relative path, resolve it relative to executable
    fs::path p(path);
    if (p.is_relative()) {
        data_dir_override_ = relative_to_executable(path);
    } else {
        data_dir_override_ = path;
    }
}

inline void Paths::reset_data_dir()
{
    data_dir_override_.reset();
}

inline void make_path(fs::path path)
{
    if (path.empty()) {
        return;
    }

    if (path.has_extension()) {
        // Looks like a file, create parent directory
        if (path.has_parent_path()) {
            fs::create_directories(path.parent_path());
        }
    } else {
        // No extension, treat as directory
        fs::create_directories(path);
    }
};
} // namespace huira
