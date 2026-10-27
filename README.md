# Ginkgo CADx

Ginkgo CADx is a DICOM viewer and dicomizer. It can also convert PNG, JPEG,
BMP, PDF and TIFF files to DICOM.

This repository continues the free version originally developed by
[MetaEmotion](http://ginkgo-cadx.com/en/), through the forks listed in
[docs/modernization-plan.md](docs/modernization-plan.md). It is licensed under
the GNU Lesser General Public License v3.

## Status

The code builds against Debian 12 (bookworm) libraries: wxWidgets 3.2, ITK 5,
DCMTK 3.6.7, OpenSSL 3 and VTK 9.1, and needs OpenGL 3.2 or newer. See
[docs/modernization-plan.md](docs/modernization-plan.md) for the plan.

Changes compared with Ginkgo CADx 3.8.8:

- Builds on current libraries (see Status) in a Docker container.
- Image display and the measurement and annotation tools draw with modern
  OpenGL (shaders) instead of fixed-function OpenGL.
- DICOM TLS client certificates are actually used; before, they were silently
  ignored.
- Multiple selections for downloading, and the download button is enabled when
  more than one item is selected.
- One date style (Y-m-d).

## Build

You need Docker on an x86_64 machine.

```
docker build -t ginkgocadx-build-bookworm -f ci/Dockerfile.bookworm ci
docker run --rm -e JOBS=4 -v "$PWD":/src ginkgocadx-build-bookworm ci/build.sh
```

The binary ends up at `build/ginkgocadx/ginkgocadx`. `ci/build.sh` takes an
optional build type (`Release` or `Debug`) and build directory. `JOBS` limits
parallel compile jobs; with all cores the build can run out of memory.

`ci/Dockerfile` is the older Debian 10 (buster) environment for the original
3.8.8 code, kept on the `phase0-buildable` branch.

On Windows, use a checkout where shell scripts keep LF line endings (the
included `.gitattributes` takes care of this).

## Tests

The build also compiles the unit tests in `tests/` (GoogleTest, run through
CTest). They need no display:

```
docker run --rm -v "$PWD":/src -w /src/build ginkgocadx-build-bookworm ctest --output-on-failure
```

## Run

`ci/run.sh` installs the build inside the container and starts the program, so
the container needs a display. On Windows 11 with Docker Desktop's WSL2 backend,
use WSLg:

```
docker run --rm -v "$PWD":/src \
  -v /run/desktop/mnt/host/wslg/.X11-unix:/tmp/.X11-unix -e DISPLAY=:0 \
  ginkgocadx-build-bookworm ci/run.sh
```

On a Linux desktop, mount `/tmp/.X11-unix` and pass your own `DISPLAY`.

## Building without Docker

On Debian 12 the build dependencies are listed in `ci/Dockerfile.bookworm`.
Install them and run `ci/build.sh` from the repository root.

## AppImage

`ci/appimage.sh` packages a build as an AppImage, in the same container
(after `ci/build.sh`):

```
docker run --rm -v "$PWD":/src ginkgocadx-build-bookworm sh -c \
  'apt-get update && apt-get install -y wget file libglib2.0-bin && ci/appimage.sh'
```

It writes `dist/Ginkgo_CADx-x86_64.AppImage`. The script downloads pinned
versions of [linuxdeploy](https://github.com/linuxdeploy/linuxdeploy) and its
GTK plugin and checks their SHA-256 before running them.

The AppImage runs on x86_64 Linux with glibc 2.36 or newer (Debian 12, Ubuntu
22.10 and later). Like any OpenGL application it uses the system's graphics
driver, so it needs the OpenGL libraries a desktop normally has (on Debian and
Ubuntu: `libgl1`, `libopengl0`, `libegl1`). It was checked on Ubuntu 24.04.
Without FUSE, run it with `APPIMAGE_EXTRACT_AND_RUN=1`.

`AppImageBuilder.yml` is the earlier recipe, which packaged an installed
Debian 11 package; `ci/appimage.sh` replaces it.

## Contributing

Bug reports and patches are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for
how to report a bug, build, test and send a change, and
[AUTHORS](AUTHORS) for who has worked on Ginkgo CADx.
