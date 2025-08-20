include(FetchPybind)

pybind11_add_module(pyhuira huira_module.cpp)
target_link_libraries(pyhuira PRIVATE huira::huira Python::Python)
target_compile_definitions(pyhuira PRIVATE PYBIND11_DETAILED_ERROR_MESSAGES)

# Create the pyhuira directory structure in build
set(PYTHON_BINDINGS_OUTPUT "${CMAKE_BINARY_DIR}/huira_python_bindings")
file(MAKE_DIRECTORY ${PYTHON_BINDINGS_OUTPUT})

set(PYTHON_PACKAGE_DIR "${PYTHON_BINDINGS_OUTPUT}/huira")
file(MAKE_DIRECTORY ${PYTHON_PACKAGE_DIR})

# Set output properties for the Python module with platform-specific logic
if(WIN32)
    set_target_properties(pyhuira PROPERTIES 
        SUFFIX ".pyd"
        OUTPUT_NAME "pyhuira"
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}"
    )
else()
    set_target_properties(pyhuira PROPERTIES 
        SUFFIX ".so"
        OUTPUT_NAME "pyhuira"
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}"
    )
endif()

# Set for all configurations
foreach(config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config} CONFIG_UPPER)
    set_target_properties(pyhuira PROPERTIES 
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${PYTHON_PACKAGE_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${PYTHON_PACKAGE_DIR}"
    )
endforeach()

# Also handle single-config generators
if(CMAKE_BUILD_TYPE)
    string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE_UPPER)
    set_target_properties(pyhuira PROPERTIES 
        LIBRARY_OUTPUT_DIRECTORY_${BUILD_TYPE_UPPER} "${PYTHON_PACKAGE_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_${BUILD_TYPE_UPPER} "${PYTHON_PACKAGE_DIR}"
    )
endif()

# Generate version file from the root project version
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/_version.py.in"
    "${PYTHON_PACKAGE_DIR}/_version.py"
    @ONLY
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/__init__.py ${PYTHON_PACKAGE_DIR}/__init__.py COPYONLY)


# Copy packaging files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml ${CMAKE_BINARY_DIR}/pyproject.toml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/setup.py ${CMAKE_BINARY_DIR}/setup.py COPYONLY)

# Copy README and License
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/README.md ${PYTHON_PACKAGE_DIR}/README.md COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/LICENSE ${PYTHON_PACKAGE_DIR}/LICENSE COPYONLY)