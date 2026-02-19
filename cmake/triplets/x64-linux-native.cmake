# x64-linux-native.cmake
# Custom vcpkg triplet for building all dependencies with native CPU optimizations.
#
# Usage:
#   cmake -B build \
#     -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
#     -DVCPKG_OVERLAY_TRIPLETS=cmake/triplets \
#     -DVCPKG_TARGET_TRIPLET=x64-linux-native
#
# This ensures Embree, TBB, and all other vcpkg dependencies are compiled
# with -march=native, not just your own project code.

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Native arch + aggressive optimization flags for all ports
set(VCPKG_C_FLAGS "-march=native -mtune=native -O3")
set(VCPKG_CXX_FLAGS "-march=native -mtune=native -O3")
set(VCPKG_C_FLAGS_RELEASE "-O3 -DNDEBUG")
set(VCPKG_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Embree-specific: ensure it builds all ISA targets for runtime dispatch
# plus native compilation for its non-dispatched code paths.
# (Embree's own CMake will handle EMBREE_ISA_* flags; the -march=native
# here ensures the framework/BVH code uses native instructions.)
