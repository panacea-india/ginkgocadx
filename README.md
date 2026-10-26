# Ginkgo CADx

Ginkgo CADx is a DICOM viewer and dicomizer. It can also convert PNG, JPEG,
BMP, PDF and TIFF files to DICOM.

This repository continues the free version originally developed by
[MetaEmotion](http://ginkgo-cadx.com/en/), through the forks listed in
[docs/modernization-plan.md](docs/modernization-plan.md). It is licensed under
the GNU Lesser General Public License v3.

## Status

The code builds against Debian 12 (bookworm) libraries: wxWidgets 3.2, ITK 5,
DCMTK 3.6.7, OpenSSL 3 and VTK 9.1. Its measurement and annotation tools still
draw with legacy OpenGL, so it needs an OpenGL 3.2+ compatibility profile,
which Mesa, NVIDIA and AMD drivers on Linux provide. See
[docs/modernization-plan.md](docs/modernization-plan.md) for the plan.

Changes in this branch on top of Ginkgo CADx 3.8.8:

- Multiple selections for downloading, and the download button is enabled when
  more than one item is selected.
- One date style (Y-m-d).
- An `AppImageBuilder.yml` recipe.

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

`AppImageBuilder.yml` builds an AppImage from an installed Debian package. The
recipe was written for Debian 11, and its paths are still the original author's
(`/home/user/GinkgoAppImage`). Adjust them before use.

1. Build the Debian package from this branch (apply it as a patch on top of
   `apt source ginkgocadx`) and unpack its `data.tar.xz` into
   `GinkgoAppImage/AppDir`.
2. Install [appimage-builder](https://appimage-builder.readthedocs.io/en/latest/intro/install.html).
3. Run `appimage-builder` in the `GinkgoAppImage` directory.

## Contributing

Bug reports and patches are welcome. There are no tests yet; please check that
the container build still passes.
