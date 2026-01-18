#pragma once

#include <string>

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
    #define ISATTY _isatty
    #define FILENO _fileno
#else
    #include <unistd.h>
    #define ISATTY isatty
    #define FILENO fileno
#endif

namespace huira::detail {
    
    namespace {
        // Initialize Windows console for ANSI color support
        inline bool initialize_console_colors() {
#ifdef _WIN32
            static bool initialized = false;
            if (!initialized) {
                HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
                HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
                
                DWORD modeOut = 0;
                DWORD modeErr = 0;
                
                if (GetConsoleMode(hOut, &modeOut)) {
                    SetConsoleMode(hOut, modeOut | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
                }
                if (GetConsoleMode(hErr, &modeErr)) {
                    SetConsoleMode(hErr, modeErr | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
                }
                
                initialized = true;
            }
            return true;
#else
            return true;
#endif
        }

        // Check if output is a terminal (not redirected to file)
        inline bool is_terminal() {
            static bool is_tty = ISATTY(FILENO(stderr)) != 0;
            return is_tty;
        }

        inline std::string colorize(const std::string& text, const char* code) {
            static bool init = initialize_console_colors();
            (void)init;
            
            if (!is_terminal()) {
                return text;  // No colors if redirected to file
            }
            
            return std::string("\033[") + code + "m" + text + "\033[0m";
        }
    }
    
    inline std::string red(const std::string& text) {
        return colorize(text, "31");
    }

    inline std::string yellow(const std::string& text) {
        return colorize(text, "33");
    }

    inline std::string blue(const std::string& text) {
        return colorize(text, "34");
    }

    inline std::string green(const std::string& text) {
        return colorize(text, "32");
    }

    inline std::string magenta(const std::string& text) {
        return colorize(text, "35");
    }

    inline std::string cyan(const std::string& text) {
        return colorize(text, "36");
    }

    inline std::string white(const std::string& text) {
        return colorize(text, "37");
    }

    inline std::string grey(const std::string& text) {
        return colorize(text, "90");
    }

    // Bright variants
    inline std::string bright_red(const std::string& text) {
        return colorize(text, "91");
    }

    inline std::string bright_yellow(const std::string& text) {
        return colorize(text, "93");
    }

    inline std::string bright_blue(const std::string& text) {
        return colorize(text, "94");
    }

    inline std::string bright_green(const std::string& text) {
        return colorize(text, "92");
    }

}
