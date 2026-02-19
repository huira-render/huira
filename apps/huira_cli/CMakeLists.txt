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

set_target_properties(huira_cli PROPERTIES CXX_SCAN_FOR_MODULES OFF)
set_target_properties(huira_cli PROPERTIES OUTPUT_NAME huira)

# Install CLI
include(GNUInstallDirs)
install(TARGETS huira_cli
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(HUIRA_PYTHON)
    install(TARGETS huira_cli
        DESTINATION "${SKBUILD_SCRIPTS_DIR}"
        COMPONENT python
    )
    
    # Set RPATH so the CLI can find shared libraries at runtime
    if(APPLE)
        set_target_properties(huira_cli PROPERTIES
            INSTALL_RPATH "@loader_path/../lib;@loader_path/../huira/.dylibs"
            BUILD_WITH_INSTALL_RPATH OFF
            BUILD_RPATH "${CMAKE_BINARY_DIR}"
        )
    elseif(UNIX)
        set_target_properties(huira_cli PROPERTIES
            INSTALL_RPATH "$ORIGIN/../lib"
            BUILD_WITH_INSTALL_RPATH OFF
            BUILD_RPATH "${CMAKE_BINARY_DIR}"
        )
    endif()
endif()
