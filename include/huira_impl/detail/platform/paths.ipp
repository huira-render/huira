#include <string>
#include <limits.h>

#ifdef _WIN32
#include <Windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#elif defined(__linux__)
#include <unistd.h>
#elif defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#endif

namespace huira::detail {
    std::string getExecutablePath() {
#ifdef _WIN32
        char path[MAX_PATH];
        DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
        if (length == 0) {
            return "";
        }
        return std::string(path, length);

#elif defined(__APPLE__)
        char path[PATH_MAX];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) != 0) {
            return "";
        }

        // Resolve any symbolic links
        char resolved[PATH_MAX];
        if (realpath(path, resolved) == nullptr) {
            return std::string(path);
        }
        return std::string(resolved);

#elif defined(__linux__)
        char path[PATH_MAX];
        ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (length == -1) {
            return "";
        }
        path[length] = '\0';
        return std::string(path);

#elif defined(__FreeBSD__)
        char path[PATH_MAX];
        size_t length = sizeof(path);
        int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };

        if (sysctl(mib, 4, path, &length, NULL, 0) != 0) {
            return "";
        }
        return std::string(path);

#else
        // Fallback for other Unix-like systems
        char path[PATH_MAX];
        ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (length != -1) {
            path[length] = '\0';
            return std::string(path);
        }

        // If /proc/self/exe doesn't work, try argv[0] approach
        // Note: This requires the program to save argv[0] at startup
        return "";
#endif
    }
}
