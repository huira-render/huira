#pragma once

#include <string>
#include <iostream>

#include "termcolor/termcolor.hpp"

namespace huira::detail {
    template<typename ColorManip>
    struct ColoredText {
        std::string text;
        ColorManip color;

        ColoredText(const std::string& str, ColorManip c) : text(str), color(c) {}
    };

    template<typename ColorManip>
    std::ostream& operator<<(std::ostream& os, const ColoredText<ColorManip>& ct) {
        return os << ct.color << ct.text << termcolor::reset;
    }

    template<typename ColorManip>
    ColoredText<ColorManip> colored(const std::string& str, ColorManip c) {
        return ColoredText<ColorManip>(str, c);
    }

    static inline auto green(const std::string& str) {
        return colored(str, termcolor::green);
    }

    static inline auto yellow(const std::string& str) {
        return colored(str, termcolor::yellow);
    }

    static inline auto red(const std::string& str) {
        return colored(str, termcolor::red);
    }

    static inline auto blue(const std::string& str) {
        return colored(str, termcolor::blue);
    }

    static inline auto hyperlink(const std::string& str) {
        return blue(str);
    }

    static inline void printError(const std::string& message, std::string sub_message = "") {
        std::cerr << red("ERROR: " + message) << std::endl;
        if (!sub_message.empty()) {
            std::cerr << yellow(sub_message) << std::endl;
        }
    }
}