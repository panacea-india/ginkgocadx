#!/bin/sh
# Install the build made by ci/build.sh into the container and start Ginkgo CADx.
# Run from the repository root inside the container described by ci/Dockerfile,
# with a display available (see "To run" in docs/modernization-plan.md).
#
# Usage: ci/run.sh [build-dir]
set -eu

BUILD_DIR="${1:-build}"

# Resources are only copied into place by "make install". The binary looks for
# them under /usr/local/share/ginkgocadx, while the build installs to /usr.
(cd "$BUILD_DIR" && make install > /dev/null)
mkdir -p /usr/local/share
ln -sfn /usr/share/ginkgocadx /usr/local/share/ginkgocadx

exec /usr/bin/ginkgocadx
