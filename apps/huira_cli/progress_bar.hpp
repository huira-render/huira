#include <memory>

#include "indicators/progress_bar.hpp"
#include "indicators/cursor_control.hpp"

namespace huira::cli {
    inline std::unique_ptr<indicators::ProgressBar> make_progress_bar(const std::string& title, size_t total) {
        indicators::show_console_cursor(false);
        return std::make_unique<indicators::ProgressBar>(
                indicators::option::BarWidth{50},
                indicators::option::Start{"["},
                indicators::option::Fill{"="},
                indicators::option::Lead{">"},
                indicators::option::Remainder{" "},
                indicators::option::End{"]"},
                indicators::option::PrefixText{title},
                indicators::option::ShowPercentage{true},
                indicators::option::ShowElapsedTime{true},
                indicators::option::MaxProgress{static_cast<int>(total) + 1}
            );
    }

    inline std::string truncate_postfix(const std::string& postfix, size_t max_len = 40) {
        if (postfix.length() <= max_len) {
            return postfix;
        }
        return postfix.substr(0, max_len - 3) + "...";
    }

    inline void update_bar(std::unique_ptr<indicators::ProgressBar>& bar, const std::string& postfix) {
        bar->set_option(indicators::option::PostfixText{ truncate_postfix(postfix) });
        bar->tick();
    }

    inline void finish_bar(std::unique_ptr<indicators::ProgressBar>& bar, const std::string& postfix) {
        bar->set_option(indicators::option::PostfixText{ postfix });
        if (!bar->is_completed()) {
            bar->tick();
        }
        indicators::show_console_cursor(true);
    }
}
