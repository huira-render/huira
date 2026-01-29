# FindCSPICE.cmake - CSPICE discovery for huira
#
# This module finds CSPICE (NASA's SPICE toolkit)
#
# Sets:
#   CSPICE_FOUND          - True if CSPICE was found
#   CSPICE_INCLUDE_DIRS   - The include directories
#   CSPICE_LIBRARIES      - The libraries to link against
#
# Provides:
#   CSPICE::cspice        - Imported target

include(FindPackageHandleStandardArgs)

# Skip search if already found
if(TARGET CSPICE::cspice)
    return()
endif()

find_library(CSPICE_LIBRARY
    NAMES cspice
    HINTS
        ${CSPICE_ROOT}
        $ENV{CSPICE_ROOT}
        ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES
        lib
        Library/lib
)

find_path(CSPICE_INCLUDE_DIR
    NAMES SpiceUsr.h
    HINTS
        ${CSPICE_ROOT}
        $ENV{CSPICE_ROOT}
        ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES
        include/cspice
        include
        Library/include/cspice
        Library/include
)

find_package_handle_standard_args(CSPICE
    REQUIRED_VARS CSPICE_LIBRARY CSPICE_INCLUDE_DIR
)

if(CSPICE_FOUND)
    # Determine correct include path for #include <cspice/SpiceUsr.h>
    if(CSPICE_INCLUDE_DIR MATCHES "/cspice$")
        get_filename_component(CSPICE_INCLUDE_DIRS "${CSPICE_INCLUDE_DIR}" DIRECTORY)
    else()
        set(CSPICE_INCLUDE_DIRS "${CSPICE_INCLUDE_DIR}")
    endif()

    set(CSPICE_LIBRARIES "${CSPICE_LIBRARY}")

    # Create imported target
    add_library(CSPICE::cspice UNKNOWN IMPORTED)
    set_target_properties(CSPICE::cspice PROPERTIES
        IMPORTED_LOCATION "${CSPICE_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${CSPICE_INCLUDE_DIRS}"
    )

    if(UNIX)
        set_property(TARGET CSPICE::cspice APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES m
        )
    endif()
endif()

mark_as_advanced(CSPICE_LIBRARY CSPICE_INCLUDE_DIR)