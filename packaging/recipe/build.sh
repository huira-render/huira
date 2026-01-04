#!/bin/bash
set -ex

echo "Building ${PKG_NAME} version ${PKG_VERSION}"

cmake -B build \
    -G Ninja \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${PREFIX}"

cmake --build build --parallel ${CPU_COUNT}

cmake --install build