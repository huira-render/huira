#include "cli.hpp"

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
            print_help(std::cerr);
            return 1;
        }

        Context ctx{};
        int i = 1;

        // Parse global flags
        for (; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--verbose" || arg == "-v") {
                ctx.verbose = true;
            } else if (arg == "--dry-run") {
                ctx.dry_run = true;
            } else if (arg == "--help" || arg == "-h") {
                print_help(std::cout);
                return 0;
            } else {
                break;
            }
        }

        if (i >= argc) {
            print_help(std::cerr);
            return 1;
        }

        std::string cmd_name = argv[i];
        auto it = commands_.find(cmd_name);
        if (it == commands_.end()) {
            std::cerr << "Unknown command: " << cmd_name << "\n\n";
            print_help(std::cerr);
            return 1;
        }

        return it->second.run(ctx, argc - i, argv + i);
    }

    void Registry::print_help(std::ostream& os) const {
        os << "Usage: huira [global options] <command> [command options]\n\n";
        os << "Commands:\n";
        for (const auto& [_, cmd] : commands_) {
            os << "  " << cmd.name << "\t" << cmd.description << "\n";
        }
        os << "\nGlobal options:\n"
        << "  -v, --verbose\n"
        << "  --dry-run\n"
        << "  -h, --help\n";
    }

}
