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

    inline void update_bar(std::unique_ptr<indicators::ProgressBar>& bar, const std::string& postfix, std::size_t percentage = 0) {
        bar->set_option(indicators::option::PostfixText{postfix});
        if (percentage > 0) {
            bar->set_progress(percentage);
        } else {
            bar->tick();
        }
    }

    inline void finish_bar(std::unique_ptr<indicators::ProgressBar>& bar, const std::string& postfix) {
        bar->set_option(indicators::option::PostfixText{postfix});
        bar->set_progress(100);
        indicators::show_console_cursor(true);
    }
}