include(FetchPybind)

pybind11_add_module(_huira src/huira_module.cpp)
set_target_properties(_huira PROPERTIES CXX_SCAN_FOR_MODULES OFF)

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

# Copy Python source files into the build tree so the package is importable
# during the build (e.g. for Sphinx autodoc) without needing pip install.
set(_huira_py_sources
    huira/__init__.py
    huira/units.py
    huira/rgb.py
    huira/visible8.py
)

foreach(_src ${_huira_py_sources})
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/${_src}"
        "${CMAKE_CURRENT_BINARY_DIR}/${_src}"
        COPYONLY
    )
endforeach()

# Handle multi-config generators (Visual Studio, Xcode)
foreach(config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config} CONFIG_UPPER)
    set_target_properties(_huira PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_CURRENT_BINARY_DIR}/huira"
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${CMAKE_CURRENT_BINARY_DIR}/huira"
    )
endforeach()

# Determine the Python install destination.
# scikit-build-core sets the install prefix so relative paths work for wheels.
# For conda-build (or plain cmake --install), we need the full site-packages path.
if(DEFINED SKBUILD)
    set(HUIRA_PYTHON_INSTALL_DIR "huira")
    set(HUIRA_PYTHON_INSTALL_DIR "huira" PARENT_SCOPE)
else()
    execute_process(
        COMMAND "${Python_EXECUTABLE}" -c
            "import sysconfig; print(sysconfig.get_path('purelib'))"
        OUTPUT_VARIABLE _python_site_packages
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    # Make it relative to CMAKE_INSTALL_PREFIX
    file(RELATIVE_PATH _rel_site "${CMAKE_INSTALL_PREFIX}" "${_python_site_packages}")
    set(HUIRA_PYTHON_INSTALL_DIR "${_rel_site}/huira")
    set(HUIRA_PYTHON_INSTALL_DIR "${_rel_site}/huira" PARENT_SCOPE)
endif()

# Install rules — used by scikit-build-core to populate the wheel,
# and by cmake --install for manual installations
install(TARGETS _huira LIBRARY DESTINATION ${HUIRA_PYTHON_INSTALL_DIR})
install(FILES huira/__init__.py DESTINATION ${HUIRA_PYTHON_INSTALL_DIR})
install(FILES huira/units.py DESTINATION ${HUIRA_PYTHON_INSTALL_DIR})
install(FILES huira/rgb.py DESTINATION ${HUIRA_PYTHON_INSTALL_DIR})
install(FILES huira/visible8.py DESTINATION ${HUIRA_PYTHON_INSTALL_DIR})
install(FILES huira/cli.py DESTINATION ${HUIRA_PYTHON_INSTALL_DIR})
install(DIRECTORY "${CMAKE_SOURCE_DIR}/data/"
    DESTINATION ${HUIRA_PYTHON_INSTALL_DIR}/data
)
