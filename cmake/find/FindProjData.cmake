# FindProjDB.cmake
#
# Locates proj.db at configure time using variables that package managers
# already provide (VCPKG_INSTALLED_DIR, CONDA_PREFIX, etc).
#
# Outputs:
#   PROJ_DATA_DIR  — absolute path to the directory containing proj.db,
#                    or empty string if not found.
#
# Usage:
#   include(FindProjDB)
#   # Then later, on your interface target:
#   if(PROJ_DATA_DIR)
#       target_compile_definitions(your_target INTERFACE HUIRA_PROJ_DIR="${PROJ_DATA_DIR}")
#   endif()

set(PROJ_DATA_DIR "")

# 1. vcpkg — VCPKG_INSTALLED_DIR and VCPKG_TARGET_TRIPLET are set
#    automatically by the vcpkg toolchain.
if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    set(_candidate "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/proj")
    if(EXISTS "${_candidate}/proj.db")
        set(PROJ_DATA_DIR "${_candidate}")
    endif()
endif()

# 2. Conda — check CONDA_PREFIX env var.
if(NOT PROJ_DATA_DIR)
    if(DEFINED ENV{CONDA_PREFIX})
        foreach(_rel "share/proj" "Library/share/proj")
            set(_candidate "$ENV{CONDA_PREFIX}/${_rel}")
            if(EXISTS "${_candidate}/proj.db")
                set(PROJ_DATA_DIR "${_candidate}")
                break()
            endif()
        endforeach()
    endif()
endif()

# 3. System paths and CMAKE_PREFIX_PATH.
if(NOT PROJ_DATA_DIR)
    set(_search_roots ${CMAKE_PREFIX_PATH} "${CMAKE_INSTALL_PREFIX}" "/usr/local" "/usr" "/opt/homebrew")
    foreach(_root ${_search_roots})
        set(_candidate "${_root}/share/proj")
        if(EXISTS "${_candidate}/proj.db")
            set(PROJ_DATA_DIR "${_candidate}")
            break()
        endif()
    endforeach()
endif()

# 4. PROJ target properties — covers Conan or other find_package setups.
if(NOT PROJ_DATA_DIR)
    foreach(_target PROJ::proj proj::proj PROJ4::proj)
        if(TARGET ${_target})
            get_target_property(_inc_dirs ${_target} INTERFACE_INCLUDE_DIRECTORIES)
            if(_inc_dirs)
                foreach(_inc ${_inc_dirs})
                    if(NOT _inc MATCHES "^\\$<")
                        get_filename_component(_prefix "${_inc}" DIRECTORY)
                        foreach(_rel "share/proj" "res")
                            if(EXISTS "${_prefix}/${_rel}/proj.db")
                                set(PROJ_DATA_DIR "${_prefix}/${_rel}")
                                break()
                            endif()
                        endforeach()
                        if(PROJ_DATA_DIR)
                            break()
                        endif()
                    endif()
                endforeach()
            endif()
            if(PROJ_DATA_DIR)
                break()
            endif()
        endif()
    endforeach()
endif()

# Report result.
if(PROJ_DATA_DIR)
    get_filename_component(PROJ_DATA_DIR "${PROJ_DATA_DIR}" ABSOLUTE)
    message(STATUS "FindProjDB: found proj.db at ${PROJ_DATA_DIR}")
else()
    message(WARNING "FindProjDB: could not find proj.db — PROJ_DATA will need to be set at runtime")
endif()
