#!/bin/sh
# Configure and build Ginkgo CADx. Run from the repository root, normally inside
# the container described by ci/Dockerfile.
#
# Usage: ci/build.sh [Release|Debug] [build-dir]
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
    -DCMAKE_MODULE_LINKER_FLAGS="$LDFLAGS"

cmake --build "$BUILD_DIR" -- -j"$(nproc)"
