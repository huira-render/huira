# CustomSanitizeTargets.cmake
#
# Some third-party packages ship exported CMake configs or pkg-config files
# that unconditionally list the Unix math library ('m' / '-lm') in their link
# interface. MSVC has no m.lib (math functions live in the C runtime), so any
# such entry that leaks into a Windows link line fails with:
#
#     LINK : fatal error LNK1181: cannot open input file 'm.lib'
#
# Known offenders in the conda-forge / vcpkg ecosystems include ICU's
# pkg-config files (reached transitively via libcurl -> libpsl -> icu, see
# conda-forge icu 78.3 win-64, and microsoft/vcpkg#22311) and historically
# FFTW's exported config (conda-forge/fftw-feedstock#74).
#
# huira_strip_unix_libm(<target> [<target> ...])
#   For each existing target, removes bare 'm' and '-lm' entries from its
#   INTERFACE_LINK_LIBRARIES. No-op on non-Windows platforms, on targets
#   that don't exist, and on targets whose link interface is already clean,
#   so it is always safe to call.

function(huira_strip_unix_libm)
    if(NOT WIN32)
        return()
    endif()

    foreach(_tgt IN LISTS ARGN)
        if(NOT TARGET ${_tgt})
            continue()
        endif()

        get_target_property(_libs ${_tgt} INTERFACE_LINK_LIBRARIES)
        if(NOT _libs)
            continue()
        endif()

        set(_cleaned "${_libs}")
        list(REMOVE_ITEM _cleaned m -lm)

        if(NOT _cleaned STREQUAL _libs)
            set_target_properties(${_tgt} PROPERTIES
                INTERFACE_LINK_LIBRARIES "${_cleaned}"
            )
            message(STATUS
                "Removed Unix libm from link interface of ${_tgt} (no m.lib on MSVC)")
        endif()
    endforeach()
endfunction()
