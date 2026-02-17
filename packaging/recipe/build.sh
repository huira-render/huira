#!/bin/bash
set -ex

echo "Building ${PKG_NAME} version ${PKG_VERSION}"

cmake -B build \
    -G Ninja \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${PREFIX}" \
    -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    -DHUIRA_APPS=ON \
    -DHUIRA_PYTHON=ON \
    -DHUIRA_NATIVE_ARCH=OFF \
    -DPython_EXECUTABLE="${PYTHON}" \
    -DPython_ROOT_DIR="${PREFIX}"

cmake --build build --parallel ${CPU_COUNT}

cmake --install build
