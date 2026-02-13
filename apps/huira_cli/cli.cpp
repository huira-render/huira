#include "huira_cli/cli.hpp"

#include "tclap/CmdLine.h"

namespace huira::cli {

    Registry& Registry::instance() {
        static Registry r;
        return r;
    }

    void Registry::add(Command cmd) {
        commands_.emplace(cmd.name, std::move(cmd));
    }

    int Registry::dispatch(int argc, char** argv) {
        if (argc < 2) {
            print_help(std::cout);
            return 1;
        }

        Context ctx{};
        int sub_start = 1; // index of the subcommand in argv

        // Manually scan for global flags before the subcommand
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-v" || arg == "--verbose") {
                ctx.verbose = true;
                ++sub_start;
            }
            else if (arg == "--version") {
                std::cout << "huira " << HUIRA_VERSION << "\n";
                return 0;
            }
            else if (arg == "-h" || arg == "--help") {
                print_help(std::cout);
                return 0;
            }
            else {
                // First non-flag argument is the subcommand
                break;
            }
        }

        if (sub_start >= argc) {
            print_help(std::cout);
            return 1;
        }

        std::string cmd_name = argv[sub_start];
        auto it = commands_.find(cmd_name);
        if (it == commands_.end()) {
            std::cerr << "Unknown command: " << cmd_name << "\n\n";
            print_help(std::cerr);
            return 1;
        }

        // Forward everything from the subcommand name onward
        int sub_argc = argc - sub_start;
        char** sub_argv = argv + sub_start;

        return it->second.run(ctx, sub_argc, sub_argv);
    }

    void Registry::print_help(std::ostream& os) const {
        // Determine max size of command name:
        std::size_t max_name_size = 0;
        for (const auto& [_, cmd] : commands_) {
            if (cmd.name.size() > max_name_size) {
                max_name_size = cmd.name.size();
            }
        }

        os << "\nUsage: huira [global options] <command> [command options]\n\n";
        os << "Commands:\n";
        for (const auto& [_, cmd] : commands_) {
            std::size_t length = max_name_size - cmd.name.size() + 4;
            std::string spaces(length, ' ');
            os << "  " << cmd.name << spaces << cmd.description << "\n";
        }
        os << "\nGlobal options:\n"
        << "  -h, --help       Show this help message\n"
        << "  -v, --verbose    Enable verbose output\n"
        << "  --version        Show version information\n\n";
    }

}
