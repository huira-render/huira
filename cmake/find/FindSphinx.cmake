# FindSphinx.cmake - Sphinx discovery for huira
#
# This module finds Sphinx documentation generator
#
# Sets:
#   Sphinx_FOUND        - True if Sphinx was found
#   SPHINX_EXECUTABLE   - Path to sphinx-build executable
#
# Provides:
#   Sphinx::build       - Imported target for sphinx-build

include(FindPackageHandleStandardArgs)

find_program(SPHINX_EXECUTABLE
    NAMES sphinx-build
    HINTS
        ${SPHINX_ROOT}
        $ENV{SPHINX_ROOT}
        ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES
        bin
        Scripts
    DOC "Path to sphinx-build executable"
)

find_package_handle_standard_args(Sphinx
    REQUIRED_VARS SPHINX_EXECUTABLE
)

if(Sphinx_FOUND AND NOT TARGET Sphinx::build)
    add_executable(Sphinx::build IMPORTED)
    set_target_properties(Sphinx::build PROPERTIES
        IMPORTED_LOCATION "${SPHINX_EXECUTABLE}"
    )
endif()

mark_as_advanced(SPHINX_EXECUTABLE)