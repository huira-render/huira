# FindIndicators.cmake - indicators discovery for huira
#
# This module finds indicators (Activity Indicators for Modern C++)
#
# Sets:
#   indicators_FOUND         - True if indicators was found
#   INDICATORS_INCLUDE_DIRS  - The include directories
#
# Provides:
#   indicators::indicators   - Imported target

include(FindPackageHandleStandardArgs)

# Skip search if already found
if(TARGET indicators::indicators)
    return()
endif()

# Try config mode first (vcpkg, etc.)
find_package(indicators CONFIG QUIET)

if(indicators_FOUND)
    # Already provides indicators::indicators target
    return()
endif()

# Fall back to manual discovery
find_path(INDICATORS_INCLUDE_DIR
    NAMES indicators/indicators.hpp
    HINTS
        ${INDICATORS_ROOT}
        $ENV{INDICATORS_ROOT}
        ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES
        include
)

find_package_handle_standard_args(indicators
    REQUIRED_VARS INDICATORS_INCLUDE_DIR
)

if(indicators_FOUND)
    set(INDICATORS_INCLUDE_DIRS "${INDICATORS_INCLUDE_DIR}")

    add_library(indicators::indicators INTERFACE IMPORTED)
    set_target_properties(indicators::indicators PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${INDICATORS_INCLUDE_DIRS}"
    )
endif()

mark_as_advanced(INDICATORS_INCLUDE_DIR)