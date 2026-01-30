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
        // Show help if no arguments provided
        if (argc < 2) {
            print_help(std::cout);
            return 1;
        }

        try {
            // Build the list of allowed subcommands for the help message
            std::vector<std::string> allowed_cmds;
            for (const auto& [name, _] : commands_) {
                allowed_cmds.push_back(name);
            }

            TCLAP::CmdLine cmd("huira - CLI tool", ' ', HUIRA_VERSION);

            // Global options
            TCLAP::SwitchArg verbose_arg("v", "verbose", "Enable verbose output", cmd, false);

            // Unlabeled argument for the subcommand
            TCLAP::UnlabeledValueArg<std::string> subcommand_arg("command", "Subcommand to run", true, "", "command", cmd);

            // Unlabeled multi-arg to capture remaining arguments for the subcommand
            TCLAP::UnlabeledMultiArg<std::string> remaining_args("args", "Arguments for the subcommand", false, "args", cmd);

            // Parse up to but not including subcommand args
            // We need to use ignoreRest to allow subcommands to have their own parsing
            cmd.setExceptionHandling(false);
            cmd.parse(argc, argv);

            // Build context from parsed global flags
            Context ctx{};
            ctx.verbose = verbose_arg.getValue();

            // Find and dispatch to subcommand
            std::string cmd_name = subcommand_arg.getValue();
            auto it = commands_.find(cmd_name);
            if (it == commands_.end()) {
                std::cerr << "Unknown command: " << cmd_name << "\n\n";
                print_help(std::cerr);
                return 1;
            }

            // Reconstruct argv for subcommand: [subcommand, remaining_args...]
            std::vector<char*> sub_argv;
            std::string cmd_name_copy = cmd_name;
            sub_argv.push_back(cmd_name_copy.data());

            std::vector<std::string> remaining = remaining_args.getValue();
            for (auto& arg : remaining) {
                sub_argv.push_back(arg.data());
            }

            return it->second.run(ctx, static_cast<int>(sub_argv.size()), sub_argv.data());

        } catch (TCLAP::ArgException& e) {
            std::cerr << "Error: " << e.error() << " for arg " << e.argId() << "\n";
            return 1;
            
        } catch (TCLAP::ExitException& e) {
            // TCLAP throws this for --help and --version
            return e.getExitStatus();
        }
    }

    void Registry::print_help(std::ostream& os) const {
        os << "\nUsage: huira [global options] <command> [command options]\n\n";
        os << "Commands:\n";
        for (const auto& [_, cmd] : commands_) {
            os << "  " << cmd.name << "\t" << cmd.description << "\n";
        }
        os << "\nGlobal options:\n"
        << "  -h, --help       Show this help message\n"
        << "  -v, --verbose    Enable verbose output\n"
        << "  --version        Show version information\n\n";
    }

}