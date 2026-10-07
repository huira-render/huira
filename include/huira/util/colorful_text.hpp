#pragma once

#include <string>

#include "huira/util/macros.hpp"

#ifdef _WIN32
#include <io.h>

#include "huira/platform/win32.hpp"
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

namespace huira {

// Initialize Windows console for ANSI color support
inline bool initialize_console_colors()
{
#ifdef _WIN32
    HUIRA_PER_MODULE_STATE_BEGIN
    static bool initialized = false;
    HUIRA_PER_MODULE_STATE_END
    if (!initialized) {
        namespace w = huira::win32;
        w::HANDLE hOut = w::GetStdHandle(w::std_output_handle);
        w::HANDLE hErr = w::GetStdHandle(w::std_error_handle);

        w::DWORD modeOut = 0;
        w::DWORD modeErr = 0;

        if (w::GetConsoleMode(hOut, &modeOut)) {
            w::SetConsoleMode(hOut, modeOut | w::enable_virtual_terminal_processing);
        }
        if (w::GetConsoleMode(hErr, &modeErr)) {
            w::SetConsoleMode(hErr, modeErr | w::enable_virtual_terminal_processing);
        }

        initialized = true;
    }
    return true;
#else
    return true;
#endif
}

// Check if output is a terminal (not redirected to file)
inline bool is_terminal()
{
    HUIRA_PER_MODULE_STATE_BEGIN
#ifdef _WIN32
    static bool is_tty = _isatty(_fileno(stderr)) != 0;
#else
    static bool is_tty = isatty(fileno(stderr)) != 0;
#endif
    HUIRA_PER_MODULE_STATE_END
    return is_tty;
}

inline std::string colorize(const std::string& text, const char* code)
{
    HUIRA_PER_MODULE_STATE_BEGIN
    static bool init = initialize_console_colors();
    HUIRA_PER_MODULE_STATE_END(void) init;

    if (!is_terminal()) {
        return text; // No colors if redirected to file
    }

    return std::string("\033[") + code + "m" + text + "\033[0m";
}

inline std::string red(const std::string& text)
{
    return colorize(text, "31");
}

inline std::string yellow(const std::string& text)
{
    return colorize(text, "33");
}

inline std::string blue(const std::string& text)
{
    return colorize(text, "34");
}

inline std::string green(const std::string& text)
{
    return colorize(text, "32");
}

inline std::string magenta(const std::string& text)
{
    return colorize(text, "35");
}

inline std::string cyan(const std::string& text)
{
    return colorize(text, "36");
}

inline std::string white(const std::string& text)
{
    return colorize(text, "37");
}

inline std::string on_red(const std::string& text)
{
    return colorize(text, "41");
}

inline std::string on_green(const std::string& text)
{
    return colorize(text, "42");
}

inline std::string on_yellow(const std::string& text)
{
    return colorize(text, "43");
}

inline std::string on_blue(const std::string& text)
{
    return colorize(text, "44");
}

inline std::string on_magenta(const std::string& text)
{
    return colorize(text, "45");
}

inline std::string on_cyan(const std::string& text)
{
    return colorize(text, "46");
}

inline std::string grey(const std::string& text)
{
    return colorize(text, "90");
}

// Bright variants
inline std::string bright_red(const std::string& text)
{
    return colorize(text, "91");
}

inline std::string bright_yellow(const std::string& text)
{
    return colorize(text, "93");
}

inline std::string bright_blue(const std::string& text)
{
    return colorize(text, "94");
}

inline std::string bright_green(const std::string& text)
{
    return colorize(text, "92");
}

} // namespace huira
