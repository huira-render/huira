include(FetchPybind)

pybind11_add_module(_huira src/huira_module.cpp)
target_link_libraries(_huira PRIVATE huira::huira)
target_include_directories(_huira PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(_huira PRIVATE PYBIND11_DETAILED_ERROR_MESSAGES)

# Output directory for local development (cmake --build)
# This gives you a usable package in the build tree without needing pip install
set_target_properties(_huira PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/huira"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/huira"
)

# Handle multi-config generators (Visual Studio, Xcode)
foreach(config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config} CONFIG_UPPER)
    set_target_properties(_huira PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_CURRENT_BINARY_DIR}/huira"
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_CURRENT_BINARY_DIR}/huira"
    )
endforeach()

# Install rules — used by scikit-build-core to populate the wheel,
# and by cmake --install for manual installations
install(TARGETS _huira LIBRARY DESTINATION huira)
install(FILES huira/__init__.py DESTINATION huira)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/data/"
    DESTINATION huira/data
)
