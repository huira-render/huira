add_executable(huira_cli
    main.cpp
    cli.cpp

    commands/fetch_tycho2.cpp
    commands/process_tycho2.cpp
    
)

find_package(TCLAP REQUIRED)
find_package(CURL REQUIRED)
find_package(indicators REQUIRED)

target_link_libraries(huira_cli PRIVATE
    huira::huira
    TCLAP::TCLAP
    CURL::libcurl
    indicators::indicators
)

target_include_directories(huira_cli PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/
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
