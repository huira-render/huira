# FindFFTW3f.cmake
#
# Tries three strategies in order:
#   1. CMake config mode        (vcpkg, or any install that ships FFTW3fConfig.cmake)
#   2. pkg-config               (fftw3f.pc — conda, apt, brew, etc.)
#   3. Manual path search       (fallback)
#
# Sets:
#   FFTW3f_FOUND         - True if FFTW3 (single-precision) was found
#   FFTW3f_INCLUDE_DIRS  - The include directories
#   FFTW3f_LIBRARIES     - The libraries to link
#   FFTW3f_VERSION       - Version string (if available)
#
# Provides:
#   FFTW3::fftw3f        - Imported target (always created)

include(FindPackageHandleStandardArgs)

# Early exit if we already have a target from a prior find.
if(TARGET FFTW3::fftw3f)
    set(FFTW3f_FOUND TRUE)
    return()
endif()


# Strategy 1: CMake config mode (vcpkg, etc.)
find_package(FFTW3f CONFIG QUIET)

if(FFTW3f_FOUND AND TARGET FFTW3::fftw3f)
    # The config already provides FFTW3::fftw3f. Some packaged configs (FFTW built with autotools,
    # with a hand-written or partial targets file) define it without saying where the library is
    # for any build configuration, and CMake then fails at generation with "IMPORTED_LOCATION or
    # IMPORTED_IMPLIB not set". So check that a location is set, and if none is, find the library
    # and set it.
    get_target_property(_fftw3_type FFTW3::fftw3f TYPE)
    set(_fftw3_located FALSE)
    if(_fftw3_type STREQUAL "INTERFACE_LIBRARY")
        set(_fftw3_located TRUE)
    else()
        get_target_property(_fftw3_configs FFTW3::fftw3f IMPORTED_CONFIGURATIONS)
        set(_fftw3_properties IMPORTED_LOCATION IMPORTED_IMPLIB)
        if(_fftw3_configs)
            foreach(_cfg IN LISTS _fftw3_configs)
                string(TOUPPER "${_cfg}" _cfg)
                list(APPEND _fftw3_properties IMPORTED_LOCATION_${_cfg} IMPORTED_IMPLIB_${_cfg})
            endforeach()
        endif()
        foreach(_property IN LISTS _fftw3_properties)
            get_target_property(_value FFTW3::fftw3f ${_property})
            if(_value)
                set(_fftw3_located TRUE)
            endif()
        endforeach()
    endif()

    if(NOT _fftw3_located)
        # The config lives in <prefix>/lib/cmake/fftw3 (or <prefix>/lib64/...):
        find_library(FFTW3f_LIBRARY_PATH
            NAMES fftw3f libfftw3f fftw3f-3 libfftw3f-3
            HINTS
                ${FFTW3f_LIBRARY_DIRS}
                "${FFTW3f_DIR}/../.."
                "${FFTW3f_DIR}/../../.."
                ${FFTW3f_ROOT}
                $ENV{FFTW3f_ROOT}
                ${FFTW3_ROOT}
                $ENV{FFTW3_ROOT}
                $ENV{CONDA_PREFIX}
            PATH_SUFFIXES lib lib64
        )
        if(NOT FFTW3f_LIBRARY_PATH)
            message(FATAL_ERROR
                "FFTW3f's CMake config (${FFTW3f_DIR}) defines FFTW3::fftw3f without a library "
                "location, and libfftw3f was not found near it. Set FFTW3f_LIBRARY_PATH to the "
                "single-precision FFTW library.")
        endif()
        message(STATUS "FFTW3f's CMake config gives no library location; using ${FFTW3f_LIBRARY_PATH}")
        set_target_properties(FFTW3::fftw3f PROPERTIES IMPORTED_LOCATION "${FFTW3f_LIBRARY_PATH}")
        if(WIN32)
            set_target_properties(FFTW3::fftw3f PROPERTIES IMPORTED_IMPLIB "${FFTW3f_LIBRARY_PATH}")
        endif()
        mark_as_advanced(FFTW3f_LIBRARY_PATH)
    endif()

    get_target_property(_inc FFTW3::fftw3f INTERFACE_INCLUDE_DIRECTORIES)
    if(_inc)
        set(FFTW3f_INCLUDE_DIRS "${_inc}")
    endif()

    set(FFTW3f_FOUND TRUE)

    find_package_handle_standard_args(FFTW3f
        REQUIRED_VARS FFTW3f_FOUND
    )
    return()
endif()


# Strategy 2: pkg-config
find_package(PkgConfig QUIET)

if(PKG_CONFIG_FOUND)
    pkg_check_modules(_FFTW3F QUIET fftw3f)

    if(_FFTW3F_FOUND)
        set(FFTW3f_INCLUDE_DIRS "${_FFTW3F_INCLUDE_DIRS}")
        set(FFTW3f_LIBRARIES    "${_FFTW3F_LINK_LIBRARIES}")
        set(FFTW3f_VERSION      "${_FFTW3F_VERSION}")

        # pkg-config may only give -lfftw3 without a full path.
        # If LINK_LIBRARIES is empty, fall back to LIBRARIES.
        if(NOT FFTW3f_LIBRARIES)
            set(FFTW3f_LIBRARIES "${_FFTW3F_LIBRARIES}")
        endif()

        if(NOT TARGET FFTW3::fftw3f)
            add_library(FFTW3::fftw3f INTERFACE IMPORTED)
            set_target_properties(FFTW3::fftw3f PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${FFTW3f_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES      "${FFTW3f_LIBRARIES}"
            )
        endif()

        find_package_handle_standard_args(FFTW3f
            REQUIRED_VARS FFTW3f_INCLUDE_DIRS FFTW3f_LIBRARIES
            VERSION_VAR   FFTW3f_VERSION
        )
        return()
    endif()
endif()


# Strategy 3: Manual search
find_path(FFTW3f_INCLUDE_DIRS
    NAMES fftw3.h
    HINTS
        ${FFTW3f_ROOT}
        $ENV{FFTW3f_ROOT}
        ${FFTW3_ROOT}
        $ENV{FFTW3_ROOT}
        $ENV{CONDA_PREFIX}
        ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES
        include
)

find_library(FFTW3f_LIBRARIES
    NAMES fftw3f libfftw3f
    HINTS
        ${FFTW3f_ROOT}
        $ENV{FFTW3f_ROOT}
        ${FFTW3_ROOT}
        $ENV{FFTW3_ROOT}
        $ENV{CONDA_PREFIX}
        ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES
        lib
        lib64
)

# Try to extract a version from fftw3.h if we found it.
if(FFTW3f_INCLUDE_DIRS AND EXISTS "${FFTW3f_INCLUDE_DIRS}/fftw3.h")
    file(STRINGS "${FFTW3f_INCLUDE_DIRS}/fftw3.h" _ver_line
         REGEX "#define[ \t]+FFTW_VERSION[ \t]+")
    if(_ver_line)
        string(REGEX REPLACE ".*FFTW_VERSION[ \t]+\"([0-9.]+)\".*" "\\1"
               FFTW3f_VERSION "${_ver_line}")
    endif()
endif()

find_package_handle_standard_args(FFTW3f
    REQUIRED_VARS FFTW3f_INCLUDE_DIRS FFTW3f_LIBRARIES
    VERSION_VAR   FFTW3f_VERSION
)

if(FFTW3f_FOUND AND NOT TARGET FFTW3::fftw3f)
    add_library(FFTW3::fftw3f INTERFACE IMPORTED)
    set_target_properties(FFTW3::fftw3f PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${FFTW3f_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES      "${FFTW3f_LIBRARIES}"
    )

    if(WIN32)
        get_target_property(_fftw3_libs FFTW3::fftw3f INTERFACE_LINK_LIBRARIES)
        if(_fftw3_libs)
            list(REMOVE_ITEM _fftw3_libs "m")
            set_target_properties(FFTW3::fftw3f PROPERTIES
                INTERFACE_LINK_LIBRARIES "${_fftw3_libs}"
            )
        endif()
    endif()
endif()

mark_as_advanced(FFTW3f_INCLUDE_DIRS FFTW3f_LIBRARIES)
