include(FetchPybind)

# The bindings are split over several translation units (see src/bindings.hpp), so that no one
# file needs more memory to compile than many machines have.
#
# Each spectral submodule (huira.rgb and the others) is bound in parts, in this order: each part
# is a header in src/ (bind_spectral_assets.hpp and so on), compiled for each spectral type in a
# file of its own, made from src/bind_spectral_part.cpp.in. A submodule is listed by its name and
# its spectral type.
set(HUIRA_PYTHON_SPECTRAL_SUBMODULES rgb:RGB visible8:Visible8 swir8:SWIR8)
set(HUIRA_PYTHON_SPECTRAL_PARTS values assets nodes scene render)

set(_huira_spectral_sources)
set(HUIRA_SPECTRAL_SUBMODULES "")
foreach(_submodule IN LISTS HUIRA_PYTHON_SPECTRAL_SUBMODULES)
    string(REPLACE ":" ";" _submodule "${_submodule}")
    list(GET _submodule 0 HUIRA_SPECTRAL_NAME)
    list(GET _submodule 1 HUIRA_SPECTRAL_TYPE)
    set(_calls "")
    string(APPEND HUIRA_SPECTRAL_SUBMODULES "\n")
    foreach(HUIRA_SPECTRAL_PART IN LISTS HUIRA_PYTHON_SPECTRAL_PARTS)
        set(_source "${CMAKE_CURRENT_BINARY_DIR}/src/${HUIRA_SPECTRAL_NAME}_${HUIRA_SPECTRAL_PART}.cpp")
        configure_file(src/bind_spectral_part.cpp.in "${_source}" @ONLY)
        list(APPEND _huira_spectral_sources "${_source}")
        set(_part "bind_${HUIRA_SPECTRAL_NAME}_${HUIRA_SPECTRAL_PART}")
        string(APPEND HUIRA_SPECTRAL_SUBMODULES "void ${_part}(pybind11::module_& m);\n")
        string(APPEND _calls "    ${_part}(m);\n")
    endforeach()
    string(APPEND HUIRA_SPECTRAL_SUBMODULES
        "\n/// The huira.${HUIRA_SPECTRAL_NAME} submodule.\n"
        "inline void bind_${HUIRA_SPECTRAL_NAME}(pybind11::module_& m)\n{\n${_calls}}\n")
endforeach()
configure_file(src/spectral_submodules.hpp.in
               "${CMAKE_CURRENT_BINARY_DIR}/src/spectral_submodules.hpp" @ONLY)

pybind11_add_module(_huira
    src/huira_module.cpp
    src/core.cpp
    ${_huira_spectral_sources}
)
set_target_properties(_huira PROPERTIES CXX_SCAN_FOR_MODULES OFF)

target_link_libraries(_huira PRIVATE huira::huira)
target_include_directories(_huira PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/src
)
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
    huira/swir8.py
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
install(FILES huira/swir8.py DESTINATION ${HUIRA_PYTHON_INSTALL_DIR})
install(FILES huira/cli.py DESTINATION ${HUIRA_PYTHON_INSTALL_DIR})
install(DIRECTORY "${CMAKE_SOURCE_DIR}/data/"
    DESTINATION ${HUIRA_PYTHON_INSTALL_DIR}/data
)
