#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <iostream>

namespace huira::cli {

    struct Context {
        bool verbose = false;
        bool dry_run = false;
    };

    using CommandFn = std::function<int(const Context&, int argc, char** argv)>;

    struct Command {
        std::string name;
        std::string description;
        CommandFn   run;
    };

    class Registry {
    public:
        static Registry& instance();

        void add(Command cmd);
        int dispatch(int argc, char** argv);

        void print_help(std::ostream& os) const;

    private:
        std::unordered_map<std::string, Command> commands_;
    };

}