include(FetchPybind)

pybind11_add_module(_huira src/huira_module.cpp)
target_link_libraries(_huira PRIVATE huira::huira Python::Python)
target_include_directories(_huira PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(_huira PRIVATE PYBIND11_DETAILED_ERROR_MESSAGES)

# Create the huira directory structure in build
set(PYTHON_BINDINGS_OUTPUT "${CMAKE_BINARY_DIR}/huira_python_bindings")
set(PYTHON_PACKAGE_DIR "${PYTHON_BINDINGS_OUTPUT}/huira")
file(MAKE_DIRECTORY ${PYTHON_PACKAGE_DIR})

# Set output directory for the compiled module
set_target_properties(_huira PROPERTIES 
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}"
)

# Handle multi-config generators (Visual Studio, Xcode)
foreach(config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config} CONFIG_UPPER)
    set_target_properties(_huira PROPERTIES 
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${PYTHON_PACKAGE_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${PYTHON_PACKAGE_DIR}"
    )
endforeach()

# Generate version file from the root project version
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/huira/_version.py.in"
    "${PYTHON_PACKAGE_DIR}/_version.py"
    @ONLY
)

# Copy package files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/huira/__init__.py ${PYTHON_PACKAGE_DIR}/__init__.py COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/README.md ${PYTHON_PACKAGE_DIR}/README.md COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/LICENSE ${PYTHON_PACKAGE_DIR}/LICENSE COPYONLY)

# Copy packaging files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml.in ${PYTHON_BINDINGS_OUTPUT}/pyproject.toml @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/setup.py ${PYTHON_BINDINGS_OUTPUT}/setup.py COPYONLY)