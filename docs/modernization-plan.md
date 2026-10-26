# Modernization plan

Ginkgo CADx no longer builds from a clean checkout on any current Linux
distribution. Debian removed it from testing in 2021
([tracker](https://tracker.debian.org/pkg/ginkgocadx)). This plan gets it back
to a working build first, then moves it onto current libraries.

## History of this code

| Layer | Who | What it added |
|---|---|---|
| Original | MetaEmotion (Spain) | Ginkgo CADx; comments and identifiers are mostly in Spanish |
| Fork 1 | Gert Wollny ([gerddie/ginkgocadx](https://github.com/gerddie/ginkgocadx)), Debian maintainer | Up to v3.8.8 (2019): C++11, GTK3, DCMTK 3.6.4 |
| Fork 2 | SgAkErRu (GitLab) | 2024: multi-select download, Y-m-d dates, AppImage recipe |
| This repo | panacea-india | Continues from the `improvements` branch |

The `trying-build-with-updated-deps` branch builds against Debian 12 libraries,
but its commit message says parts of the app are broken. It isn't used as the
baseline.

## Dependency status

| Dependency | Required here | Current | Status |
|---|---|---|---|
| VTK | 6.2 / 7.0, legacy OpenGL backend | 9.x, OpenGL 3.2+ only | Biggest blocker: legacy backend removed by 8.1 |
| ITK | 4.8+ | 5.x | ITK 4 is end-of-life; 5.x changed the threading API (`itk::MultiThreader`) |
| wxWidgets | 3.0.2 | 3.2.x | 3.0 unmaintained; moving to 3.2 is moderate work |
| DCMTK | 3.6.4 | 3.6.8+ | Reachable; TLS and association code needs updates |
| OpenSSL | 1.1 | 3.x | 1.1 end-of-life since Sept 2023 |
| CMake | `cmake_minimum_required(2.8)` | 4.x | CMake 4 refuses anything below 3.5 |
| Bundled SQLite | 3.7.6.2 (2011) | — | System SQLite is the default; the bundled copy should go |

### The OpenGL rendering problem

The measurement and annotation tools draw with old-style OpenGL calls (about
1,300 `gl*` calls across 32 files: `cadxcore/widgets/w*.cpp`,
`openglhelper.cpp`, `vtkopenglginkgotexture.cpp`, the ECG widgets in
`visualizator`). That only works with VTK's legacy backend, and
`vtkOpenGLExtensionManager` no longer exists in VTK. Gert Wollny tried VTK 7 in
2019 and reverted it because the app didn't work. Moving to VTK 9 means
rewriting this drawing layer.

### Other known issues

1. **CI is dead:** `.travis.yml` targets travis-ci.org, a personal PPA and a
   Coverity token for gerddie's account.
2. **No tests** in the build.
3. **Dead build files:** Visual Studio 2012 projects, NSIS scripts
   (`install_open_source/`), forced `i386` macOS architecture in
   `CMakeLists.txt`.
4. **Hard-coded paths:** `/usr/include/jsoncpp` in `cadxcore/CMakeLists.txt`,
   `/home/user/GinkgoAppImage` in `AppImageBuilder.yml`.
5. **Stale metadata:** README, `AUTHORS`, `model_ginkgo.cpp` (a Coverity helper).
6. **No `CONTRIBUTING.md`**, code of conduct, or issue/PR templates.
7. **DICOM TLS security issue:** in
   `cadxcore/main/controllers/dcmtk/tls/gtlslayer.cc`,
   `setCertificateFromString` / `setPrivateKeyFromString` do nothing unless
   `USE_PATCHED_LIBS` is set (it isn't by default) but still return success. A
   user who configures a client certificate gets a connection without it and
   no error.
8. **C++ clean-ups:** `throw()` specifications, `OFauto_ptr`, a `register`
   variable in the copied DCMTK `libi2d` code, many `sprintf`/`strcpy` calls.

## Phases

### Phase 0: buildable as-is (in progress, branch `phase0-buildable`)

Build on the old stack in Docker so contributors have something that compiles.

- [x] `ci/Dockerfile`: Debian 10 (buster) from archive.debian.org, with the
      same build dependencies as Debian's package
- [x] `ci/build.sh`: configure and build with Debian's packaging flags
- [x] Verify the image and a full build on x86_64 (builds cleanly; the only
      linker warning is `tempnam` in `incomingdicomassociationcommand.cpp`)
- [ ] GitHub Actions workflow that builds the image and runs `ci/build.sh`
- [ ] Remove `.travis.yml` and `model_ginkgo.cpp`
- [ ] Raise `cmake_minimum_required` in all four `CMakeLists.txt` files
- [ ] Rewrite the README so building starts from this repo

Findings so far:

- Debian's buster package (3.8.8-1) carries **no patches**; upstream 3.8.8
  built as-is. The only things to match are its flags: `-DNDEBUG` (turns off
  wxWidgets 3.0 debug assertions, which otherwise crash the app), hardening
  flags and `-Wl,--as-needed`. `ci/build.sh` applies these.
- `libinsighttoolkit4-dev` exists only for amd64 and i386 in buster, so the
  build environment is **x86_64 only**. Under emulation on Apple Silicon it is
  very slow; build on an x86 machine. arm64 is optional for now.

- On Windows, git's `autocrlf` checks shell scripts out with CRLF, and `sh`
  then fails on `set -eu`. `.gitattributes` forces LF for `*.sh`.

To build:

```
docker build -t ginkgocadx-build -f ci/Dockerfile ci
docker run --rm -v "$PWD":/src ginkgocadx-build ci/build.sh
```

To run the result on Windows 11 with Docker Desktop (WSL2 backend), using
WSLg's X socket for the display:

```
docker run --rm -v "$PWD":/src \
  -v /run/desktop/mnt/host/wslg/.X11-unix:/tmp/.X11-unix -e DISPLAY=:0 \
  ginkgocadx-build ci/run.sh
```

On a Linux desktop, mount `/tmp/.X11-unix` and pass your own `DISPLAY` instead.

### Phase 1: the easier dependencies (weeks)

wxWidgets 3.2, ITK 5, current DCMTK, OpenSSL 3. Remove the bundled SQLite and
the dead Windows/macOS build files. Fix the TLS issue.

### Phase 2: VTK 9 port

Rewrite the OpenGL drawing layer described above. Until this is done the app
depends on an old base image. A good first large project for new contributors.

### Phase 3: open the doors

`CONTRIBUTING.md`, templates, updated `AUTHORS` and README, a start on a test
suite, and an AppImage or Flatpak build in CI.
