# FindTCLAP.cmake - TCLAP discovery for huira
#
# This module finds TCLAP (Templatized C++ Command Line Parser Library)
#
# Sets:
#   TCLAP_FOUND         - True if TCLAP was found
#   TCLAP_INCLUDE_DIRS  - The include directories
#
# Provides:
#   TCLAP::TCLAP        - Imported target

include(FindPackageHandleStandardArgs)

# Skip search if already found
if(TCLAP_INCLUDE_DIRS)
    set(TCLAP_FIND_QUIETLY TRUE)
endif()

find_path(TCLAP_INCLUDE_DIRS
    NAMES tclap/CmdLine.h
    HINTS
        ${TCLAP_ROOT}
        $ENV{TCLAP_ROOT}
        ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES
        include
)

find_package_handle_standard_args(TCLAP
    REQUIRED_VARS TCLAP_INCLUDE_DIRS
)

# Create imported target for modern CMake usage
if(TCLAP_FOUND AND NOT TARGET TCLAP::TCLAP)
    add_library(TCLAP::TCLAP INTERFACE IMPORTED)
    set_target_properties(TCLAP::TCLAP PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${TCLAP_INCLUDE_DIRS}"
    )
endif()

mark_as_advanced(TCLAP_INCLUDE_DIRS)