add_executable(huira_cli
    main.cpp
    cli.cpp

    commands/process_tycho2.cpp
)

find_package(TCLAP REQUIRED)

target_link_libraries(huira_cli PRIVATE
    huira::huira
    TCLAP::TCLAP
)

target_compile_features(huira_cli PRIVATE cxx_std_20)

set_target_properties(huira_cli PROPERTIES
    OUTPUT_NAME huira
)

# Install CLI
include(GNUInstallDirs)
install(TARGETS huira_cli
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
