# Set the alternative vcpkg.json directory:
set(VCPKG_MANIFEST_DIR "${CMAKE_SOURCE_DIR}/packaging")

# Disable vcpkg if using an alternative toolchain
set(USING_ALTERNATIVE_PKG_MGR FALSE)
if(CMAKE_TOOLCHAIN_FILE MATCHES "conda")
    set(USING_ALTERNATIVE_PKG_MGR TRUE)
    message(STATUS "Conda toolchain detected - disabling vcpkg integration")
elseif(CMAKE_TOOLCHAIN_FILE MATCHES "conan")
    set(USING_ALTERNATIVE_PKG_MGR TRUE)
    message(STATUS "Conan toolchain detected - disabling vcpkg integration")
elseif(DEFINED ENV{CONDA_BUILD})
    set(USING_ALTERNATIVE_PKG_MGR TRUE)
    message(STATUS "conda-build detected - disabling vcpkg integration")
endif()

# Disable vcpkg only if using alternative toolchain
if(USING_ALTERNATIVE_PKG_MGR)
    set(VCPKG_MANIFEST_MODE OFF CACHE BOOL "" FORCE)
    set(VCPKG_TARGET_TRIPLET "")
    unset(ENV{VCPKG_ROOT})
    return()
endif()


# If using vcpkg, apply settings
if(DEFINED ENV{VCPKG_ROOT} OR CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE BOOL "" FORCE)

    # Auto-select native arch triplet when HUIRA_NATIVE_ARCH is ON
    # and the user hasn't already specified a custom triplet.
    if(HUIRA_NATIVE_ARCH AND NOT VCPKG_TARGET_TRIPLET)
        set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_SOURCE_DIR}/cmake/triplets"
            CACHE STRING "vcpkg triplet overlay directory" FORCE)

        if(WIN32)
            set(VCPKG_TARGET_TRIPLET "x64-windows-native" CACHE STRING "" FORCE)
        elseif(APPLE)
            # Apple Silicon uses NEON by default; Intel Macs can use a custom triplet
            # For now, fall through to default triplet on Apple
        elseif(UNIX)
            set(VCPKG_TARGET_TRIPLET "x64-linux-native" CACHE STRING "" FORCE)
        endif()

        if(VCPKG_TARGET_TRIPLET)
            message(STATUS "vcpkg: using native-arch triplet '${VCPKG_TARGET_TRIPLET}'")
        endif()
    endif()

    if(HUIRA_TESTS)
        list(APPEND VCPKG_MANIFEST_FEATURES "tests")
    endif()

    if(HUIRA_APPS)
        list(APPEND VCPKG_MANIFEST_FEATURES "apps")
    endif()

    message(STATUS "Using vcpkg for dependencies")
endif()
