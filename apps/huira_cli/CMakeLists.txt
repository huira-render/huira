# The huira command line application: a dispatcher (cli.cpp) and a set of subcommands.
# Each subcommand is a source file in commands/ that registers itself with the Registry
# (see cli.hpp). There are none at the moment; the data files huira needs are built once and
# published on media.huira.space, by the tools behind HUIRA_BUILD_TOOLS.
add_executable(huira_cli
    main.cpp
    cli.cpp
)

find_package(TCLAP REQUIRED)
find_package(Indicators REQUIRED)

target_link_libraries(huira_cli PRIVATE
    huira::huira
    TCLAP::TCLAP
    indicators::indicators
)

target_include_directories(huira_cli PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/
)

target_compile_features(huira_cli PRIVATE cxx_std_20)

set_target_properties(huira_cli PROPERTIES CXX_SCAN_FOR_MODULES OFF)

include(GNUInstallDirs)

if(DEFINED SKBUILD)
    set_target_properties(huira_cli PROPERTIES OUTPUT_NAME huira_bin)
    install(TARGETS huira_cli
        DESTINATION "${HUIRA_PYTHON_INSTALL_DIR}"
        COMPONENT python
    )
else()
    set_target_properties(huira_cli PROPERTIES OUTPUT_NAME huira)
    install(TARGETS huira_cli
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
