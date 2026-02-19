# x64-windows-native.cmake
# Custom vcpkg triplet for building all dependencies with native CPU optimizations on Windows.
#
# Usage:
#   cmake -B build \
#     -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake \
#     -DVCPKG_OVERLAY_TRIPLETS=cmake/triplets \
#     -DVCPKG_TARGET_TRIPLET=x64-windows-native

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

# AVX2 is a safe bet for modern desktops (2013+).
# Change to /arch:AVX512 if you know the target supports it.
set(VCPKG_C_FLAGS "/arch:AVX2")
set(VCPKG_CXX_FLAGS "/arch:AVX2")
set(VCPKG_C_FLAGS_RELEASE "/O2 /DNDEBUG")
set(VCPKG_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
