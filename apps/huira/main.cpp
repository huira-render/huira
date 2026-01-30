#include "cli.hpp"

int main(int argc, char** argv) {
    return huira::cli::Registry::instance().dispatch(argc, argv);
}
