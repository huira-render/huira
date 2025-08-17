# Set the alternative vcpkg.json directory:
set(VCPKG_MANIFEST_DIR "${CMAKE_SOURCE_DIR}/dependencies")

# Disable vcpkg if using an alternative toolchain
set(USING_ALTERNATIVE_PKG_MGR FALSE)
if(CMAKE_TOOLCHAIN_FILE MATCHES "conda")
    set(USING_ALTERNATIVE_PKG_MGR TRUE)
    message(STATUS "Conda toolchain detected - disabling vcpkg integration")
elseif(CMAKE_TOOLCHAIN_FILE MATCHES "conan")
    set(USING_ALTERNATIVE_PKG_MGR TRUE)
    message(STATUS "Conan toolchain detected - disabling vcpkg integration")
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
    # Suppress developer warnings when using vcpkg
    set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE BOOL "" FORCE)

    if (HUIRA_TESTS)
        list(APPEND VCPKG_MANIFEST_FEATURES "tests")
    endif()
    
    message(STATUS "Using vcpkg For Dependencies")
endif()