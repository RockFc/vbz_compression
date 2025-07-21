#!/bin/bash
set -e  # 出错即退出

INSTALL_PATH=/home/fuchao1/workspace/code/vbz_demo/thirdparty/lib/UNIX/vbz

echo "Cleaning build directory..."
rm -rf build
mkdir -p build
cd build

echo "Configuring with CMake..."
cmake \
  -D CMAKE_BUILD_TYPE=Release \
  -D ENABLE_CONAN=OFF \
  -D ENABLE_PERF_TESTING=ON \
  -D ENABLE_PYTHON=OFF \
  -D CMAKE_INSTALL_PREFIX=${INSTALL_PATH} \
  ..

echo "Building and installing..."
cmake --build . -j20 --target install

echo "Installed to ${INSTALL_PATH}"
