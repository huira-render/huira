# FindCFITSIO.cmake
#
# Tries four strategies in order:
#   1. Official cfitsio CMake config  (cfitsio >= 4.4, vcpkg >= #50344)
#   2. Legacy vcpkg CMake config      (unofficial-cfitsio, older vcpkg)
#   3. pkg-config                     (cfitsio.pc — conda, apt, brew, etc.)
#   4. Manual path search             (fallback)
#
# Sets:
#   CFITSIO_FOUND          - True if CFITSIO was found
#   CFITSIO_INCLUDE_DIRS   - The include directories
#   CFITSIO_LIBRARIES      - The libraries to link
#   CFITSIO_VERSION        - Version string (if available)
#
# Provides:
#   CFITSIO::CFITSIO       - Imported target (always created)

include(FindPackageHandleStandardArgs)

# Early exit if we already have a target from a prior find.
if(TARGET CFITSIO::CFITSIO)
    set(CFITSIO_FOUND TRUE)
    return()
endif()


# Strategy 1: Official cfitsio CMake config 
find_package(cfitsio CONFIG QUIET)

if(cfitsio_FOUND AND TARGET CFITSIO::CFITSIO)
    # The official config already provides CFITSIO::CFITSIO — nothing to wrap.
    get_target_property(_inc CFITSIO::CFITSIO INTERFACE_INCLUDE_DIRECTORIES)
    if(_inc)
        set(CFITSIO_INCLUDE_DIRS "${_inc}")
    endif()

    set(CFITSIO_FOUND TRUE)

    find_package_handle_standard_args(CFITSIO
        REQUIRED_VARS CFITSIO_FOUND
    )
    return()
endif()

if(cfitsio_FOUND AND TARGET cfitsio::cfitsio AND NOT TARGET CFITSIO::CFITSIO)
    add_library(CFITSIO::CFITSIO INTERFACE IMPORTED)
    set_target_properties(CFITSIO::CFITSIO PROPERTIES
        INTERFACE_LINK_LIBRARIES cfitsio::cfitsio
    )

    get_target_property(_inc cfitsio::cfitsio INTERFACE_INCLUDE_DIRECTORIES)
    if(_inc)
        set(CFITSIO_INCLUDE_DIRS "${_inc}")
    endif()

    set(CFITSIO_FOUND TRUE)

    find_package_handle_standard_args(CFITSIO
        REQUIRED_VARS CFITSIO_FOUND
    )
    return()
endif()


# Strategy 2: Legacy vcpkg CMake config (unofficial-cfitsio)
find_package(unofficial-cfitsio CONFIG QUIET)

if(unofficial-cfitsio_FOUND OR TARGET cfitsio)
    if(NOT TARGET CFITSIO::CFITSIO)
        add_library(CFITSIO::CFITSIO INTERFACE IMPORTED)
        set_target_properties(CFITSIO::CFITSIO PROPERTIES
            INTERFACE_LINK_LIBRARIES cfitsio
        )
    endif()

    # Pull include dirs from the target for the standard-args check.
    get_target_property(_inc cfitsio INTERFACE_INCLUDE_DIRECTORIES)
    if(_inc)
        set(CFITSIO_INCLUDE_DIRS "${_inc}")
    endif()

    set(CFITSIO_FOUND TRUE)

    find_package_handle_standard_args(CFITSIO
        REQUIRED_VARS CFITSIO_FOUND
    )
    return()
endif()


# Strategy 3: pkg-config 
find_package(PkgConfig QUIET)

if(PKG_CONFIG_FOUND)
    pkg_check_modules(_CFITSIO QUIET cfitsio)

    if(_CFITSIO_FOUND)
        set(CFITSIO_INCLUDE_DIRS "${_CFITSIO_INCLUDE_DIRS}")
        set(CFITSIO_LIBRARIES    "${_CFITSIO_LINK_LIBRARIES}")
        set(CFITSIO_VERSION      "${_CFITSIO_VERSION}")

        # pkg-config may only give -lcfitsio without a full path.
        # If LINK_LIBRARIES is empty, fall back to LIBRARIES.
        if(NOT CFITSIO_LIBRARIES)
            set(CFITSIO_LIBRARIES "${_CFITSIO_LIBRARIES}")
        endif()

        if(NOT TARGET CFITSIO::CFITSIO)
            add_library(CFITSIO::CFITSIO INTERFACE IMPORTED)
            set_target_properties(CFITSIO::CFITSIO PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${CFITSIO_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES      "${CFITSIO_LIBRARIES}"
            )
        endif()

        # pkg-config on conda-forge may include 'm' in the link interface,
        # which doesn't exist on Windows (math functions are in the CRT).
        if(WIN32)
            get_target_property(_cfitsio_libs CFITSIO::CFITSIO INTERFACE_LINK_LIBRARIES)
            if(_cfitsio_libs)
                list(REMOVE_ITEM _cfitsio_libs "m")
                set_target_properties(CFITSIO::CFITSIO PROPERTIES
                    INTERFACE_LINK_LIBRARIES "${_cfitsio_libs}"
                )
            endif()
        endif()

        find_package_handle_standard_args(CFITSIO
            REQUIRED_VARS CFITSIO_INCLUDE_DIRS CFITSIO_LIBRARIES
            VERSION_VAR   CFITSIO_VERSION
        )
        return()
    endif()
endif()


# Strategy 4: Manual search
find_path(CFITSIO_INCLUDE_DIRS
    NAMES fitsio.h
    HINTS
        ${CFITSIO_ROOT}
        $ENV{CFITSIO_ROOT}
        $ENV{CONDA_PREFIX}
        ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES
        include
        include/cfitsio
)

find_library(CFITSIO_LIBRARIES
    NAMES cfitsio
    HINTS
        ${CFITSIO_ROOT}
        $ENV{CFITSIO_ROOT}
        $ENV{CONDA_PREFIX}
        ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES
        lib
        lib64
)

# Try to extract a version from fitsio.h if we found it.
if(CFITSIO_INCLUDE_DIRS AND EXISTS "${CFITSIO_INCLUDE_DIRS}/fitsio.h")
    file(STRINGS "${CFITSIO_INCLUDE_DIRS}/fitsio.h" _ver_line
         REGEX "#define[ \t]+CFITSIO_VERSION[ \t]+")
    if(_ver_line)
        string(REGEX REPLACE ".*CFITSIO_VERSION[ \t]+([0-9.]+).*" "\\1"
               CFITSIO_VERSION "${_ver_line}")
    endif()
endif()

find_package_handle_standard_args(CFITSIO
    REQUIRED_VARS CFITSIO_INCLUDE_DIRS CFITSIO_LIBRARIES
    VERSION_VAR   CFITSIO_VERSION
)

if(CFITSIO_FOUND AND NOT TARGET CFITSIO::CFITSIO)
    add_library(CFITSIO::CFITSIO INTERFACE IMPORTED)
    set_target_properties(CFITSIO::CFITSIO PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CFITSIO_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES      "${CFITSIO_LIBRARIES}"
    )

    # Manual search on conda-forge Windows may also include 'm'.
    if(WIN32)
        get_target_property(_cfitsio_libs CFITSIO::CFITSIO INTERFACE_LINK_LIBRARIES)
        if(_cfitsio_libs)
            list(REMOVE_ITEM _cfitsio_libs "m")
            set_target_properties(CFITSIO::CFITSIO PROPERTIES
                INTERFACE_LINK_LIBRARIES "${_cfitsio_libs}"
            )
        endif()
    endif()
endif()

mark_as_advanced(CFITSIO_INCLUDE_DIRS CFITSIO_LIBRARIES)
