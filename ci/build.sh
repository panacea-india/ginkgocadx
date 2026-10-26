#!/bin/sh
# Configure and build Ginkgo CADx. Run from the repository root, normally inside
# the container described by ci/Dockerfile.
#
# Usage: ci/build.sh [Release|Debug] [build-dir]
# JOBS limits the number of parallel compile jobs (default: all cores); lower it
# if the build runs out of memory.
# Extra CMake arguments can be passed in CMAKE_ARGS. If VTK_DIR is set (the
# bookworm image does this for its source-built VTK) it is passed to CMake.
set -eu

BUILD_TYPE="${1:-Release}"
BUILD_DIR="${2:-build}"

# Same flags Debian used for its package. -DNDEBUG turns off wxWidgets 3.0
# debug assertions, which otherwise crash the application.
CPPFLAGS="-DNDEBUG -D_FORTIFY_SOURCE=2"
CXXFLAGS="-O2 -fstack-protector-strong -Wformat -Werror=format-security"
LDFLAGS="-Wl,-z,relro -Wl,-z,now -Wl,--as-needed"

cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_C_FLAGS="$CPPFLAGS $CXXFLAGS" \
    -DCMAKE_CXX_FLAGS="$CPPFLAGS $CXXFLAGS" \
    -DCMAKE_EXE_LINKER_FLAGS="$LDFLAGS" \
    -DCMAKE_SHARED_LINKER_FLAGS="$LDFLAGS" \
    -DCMAKE_MODULE_LINKER_FLAGS="$LDFLAGS" \
    ${VTK_DIR:+-DVTK_DIR="$VTK_DIR"} \
    ${CMAKE_ARGS:-}

cmake --build "$BUILD_DIR" -- -j"${JOBS:-$(nproc)}"
