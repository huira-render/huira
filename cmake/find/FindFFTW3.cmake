# FindFFTW3.cmake
#
# Tries three strategies in order:
#   1. CMake config mode        (vcpkg, or any install that ships FFTW3Config.cmake)
#   2. pkg-config               (fftw3.pc — conda, apt, brew, etc.)
#   3. Manual path search       (fallback)
#
# Sets:
#   FFTW3_FOUND         - True if FFTW3 (double-precision) was found
#   FFTW3_INCLUDE_DIRS  - The include directories
#   FFTW3_LIBRARIES     - The libraries to link
#   FFTW3_VERSION       - Version string (if available)
#
# Provides:
#   FFTW3::fftw3        - Imported target (always created)

include(FindPackageHandleStandardArgs)

# Early exit if we already have a target from a prior find.
if(TARGET FFTW3::fftw3)
    set(FFTW3_FOUND TRUE)
    return()
endif()


# Strategy 1: CMake config mode (vcpkg, etc.)
find_package(FFTW3 CONFIG QUIET)

if(FFTW3_FOUND AND TARGET FFTW3::fftw3)
    # The config already provides FFTW3::fftw3 — nothing to wrap.
    get_target_property(_inc FFTW3::fftw3 INTERFACE_INCLUDE_DIRECTORIES)
    if(_inc)
        set(FFTW3_INCLUDE_DIRS "${_inc}")
    endif()

    set(FFTW3_FOUND TRUE)

    find_package_handle_standard_args(FFTW3
        REQUIRED_VARS FFTW3_FOUND
    )
    return()
endif()


# Strategy 2: pkg-config
find_package(PkgConfig QUIET)

if(PKG_CONFIG_FOUND)
    pkg_check_modules(_FFTW3D QUIET fftw3)

    if(_FFTW3D_FOUND)
        set(FFTW3_INCLUDE_DIRS "${_FFTW3D_INCLUDE_DIRS}")
        set(FFTW3_LIBRARIES    "${_FFTW3D_LINK_LIBRARIES}")
        set(FFTW3_VERSION      "${_FFTW3D_VERSION}")

        # pkg-config may only give -lfftw3 without a full path.
        # If LINK_LIBRARIES is empty, fall back to LIBRARIES.
        if(NOT FFTW3_LIBRARIES)
            set(FFTW3_LIBRARIES "${_FFTW3D_LIBRARIES}")
        endif()

        if(NOT TARGET FFTW3::fftw3)
            add_library(FFTW3::fftw3 INTERFACE IMPORTED)
            set_target_properties(FFTW3::fftw3 PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${FFTW3_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES      "${FFTW3_LIBRARIES}"
            )
        endif()

        find_package_handle_standard_args(FFTW3
            REQUIRED_VARS FFTW3_INCLUDE_DIRS FFTW3_LIBRARIES
            VERSION_VAR   FFTW3_VERSION
        )
        return()
    endif()
endif()


# Strategy 3: Manual search
find_path(FFTW3_INCLUDE_DIRS
    NAMES fftw3.h
    HINTS
        ${FFTW3_ROOT}
        $ENV{FFTW3_ROOT}
        $ENV{CONDA_PREFIX}
        ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES
        include
)

find_library(FFTW3_LIBRARIES
    NAMES fftw3 libfftw3
    HINTS
        ${FFTW3_ROOT}
        $ENV{FFTW3_ROOT}
        $ENV{CONDA_PREFIX}
        ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES
        lib
        lib64
)

# Try to extract a version from fftw3.h if we found it.
if(FFTW3_INCLUDE_DIRS AND EXISTS "${FFTW3_INCLUDE_DIRS}/fftw3.h")
    file(STRINGS "${FFTW3_INCLUDE_DIRS}/fftw3.h" _ver_line
         REGEX "#define[ \t]+FFTW_VERSION[ \t]+")
    if(_ver_line)
        string(REGEX REPLACE ".*FFTW_VERSION[ \t]+\"([0-9.]+)\".*" "\\1"
               FFTW3_VERSION "${_ver_line}")
    endif()
endif()

find_package_handle_standard_args(FFTW3
    REQUIRED_VARS FFTW3_INCLUDE_DIRS FFTW3_LIBRARIES
    VERSION_VAR   FFTW3_VERSION
)

if(FFTW3_FOUND AND NOT TARGET FFTW3::fftw3)
    add_library(FFTW3::fftw3 INTERFACE IMPORTED)
    set_target_properties(FFTW3::fftw3 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${FFTW3_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES      "${FFTW3_LIBRARIES}"
    )

    if(WIN32)
        get_target_property(_fftw3_libs FFTW3::fftw3 INTERFACE_LINK_LIBRARIES)
        if(_fftw3_libs)
            list(REMOVE_ITEM _fftw3_libs "m")
            set_target_properties(FFTW3::fftw3 PROPERTIES
                INTERFACE_LINK_LIBRARIES "${_fftw3_libs}"
            )
        endif()
    endif()
endif()

mark_as_advanced(FFTW3_INCLUDE_DIRS FFTW3_LIBRARIES)
