#include <iostream>
#include <cstddef>
#include <string>

#include "huira/platform/windows_minmax.hpp"
#include <algorithm>

#include "tclap/StdOutput.h"

namespace huira::cli {
    class CompactOutput : public TCLAP::StdOutput {
    public:
        void usage(TCLAP::CmdLineInterface& cmd) override {
            std::cout << "\n" << cmd.getMessage() << "\n\n";

            // Build usage line
            std::cout << "Usage: " << cmd.getProgramName();
            auto args = cmd.getArgList();
            for (auto it = args.rbegin(); it != args.rend(); ++it) {
                if ((*it)->getName() == "ignore_rest") continue;
                std::cout << " " << (*it)->shortID();
            }
            std::cout << "\n\n";

            // Print args with aligned descriptions
            std::size_t max_width = 0;
            for (auto it = args.rbegin(); it != args.rend(); ++it) {
                if ((*it)->getName() == "ignore_rest") continue;
                max_width = std::max(max_width, format_flag(**it).size());
            }

            std::cout << "Options:\n";
            for (auto it = args.rbegin(); it != args.rend(); ++it) {
                if ((*it)->getName() == "ignore_rest") continue;
                std::string flag = format_flag(**it);
                std::string pad(max_width - flag.size() + 4, ' ');
                std::cout << "  " << flag << pad << (*it)->getDescription() << "\n";
            }
            std::cout << "\n";
        }

        void failure(TCLAP::CmdLineInterface& cmd, TCLAP::ArgException& e) override {
            std::cerr << "Error: " << e.error() << "\n";
            usage(cmd);
            exit(1);
        }

        void version(TCLAP::CmdLineInterface& cmd) override {
            std::cout << cmd.getVersion() << "\n";
        }

    private:
        static std::string format_flag(TCLAP::Arg& arg) {
            std::string flag = arg.getFlag();
            std::string name = arg.getName();

            // Unlabeled/positional args have no flag AND no "--name" style
            if (flag.empty() && !arg.isRequired() == false) {
                // Better: check if it's truly a positional arg
            }

            // If there's a long name starting with a known switch pattern, treat as flag
            std::string result;
            if (!flag.empty()) {
                result += "-" + flag + ", --" + name;
            }
            else if (name == "help" || name == "version" || name == "ignore_rest") {
                result += "--" + name;
            }
            else {
                // Positional/unlabeled arg
                return "<" + name + ">";
            }
            return result;
        }
    };
}
