# Ginkgo CADx

Ginkgo CADx is a DICOM viewer and dicomizer. It can also convert PNG, JPEG,
BMP, PDF and TIFF files to DICOM.

This repository continues the free version originally developed by
[MetaEmotion](http://ginkgo-cadx.com/en/), through the forks listed in
[docs/modernization-plan.md](docs/modernization-plan.md). It is licensed under
the GNU Lesser General Public License v3.

## Status

The code needs an old dependency stack (VTK 6, ITK 4, wxWidgets 3.0, DCMTK
3.6.4) that current Linux distributions no longer ship. Until it is ported, the
supported way to build it is the Debian 10 (buster) container in `ci/`. See
[docs/modernization-plan.md](docs/modernization-plan.md) for the plan to move
to current libraries.

Changes in this branch on top of Ginkgo CADx 3.8.8:

- Multiple selections for downloading, and the download button is enabled when
  more than one item is selected.
- One date style (Y-m-d).
- An `AppImageBuilder.yml` recipe.

## Build

You need Docker on an x86_64 machine. The build image is amd64 only because
buster's ITK 4 package is not built for other architectures.

```
docker build -t ginkgocadx-build -f ci/Dockerfile ci
docker run --rm -v "$PWD":/src ginkgocadx-build ci/build.sh
```

The binary ends up at `build/ginkgocadx/ginkgocadx`. `ci/build.sh` takes an
optional build type (`Release` or `Debug`) and build directory.

On Windows, use a checkout where shell scripts keep LF line endings (the
included `.gitattributes` takes care of this).

## Run

`ci/run.sh` installs the build inside the container and starts the program, so
the container needs a display. On Windows 11 with Docker Desktop's WSL2 backend,
use WSLg:

```
docker run --rm -v "$PWD":/src \
  -v /run/desktop/mnt/host/wslg/.X11-unix:/tmp/.X11-unix -e DISPLAY=:0 \
  ginkgocadx-build ci/run.sh
```

On a Linux desktop, mount `/tmp/.X11-unix` and pass your own `DISPLAY`.

## Building without Docker

On Debian 10 the build dependencies are listed in `ci/Dockerfile`. Run
`ci/build.sh` from the repository root. Newer distributions don't package the
required VTK, ITK and wxWidgets versions.

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
