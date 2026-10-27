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

### Phase 0: buildable as-is (done, branch `phase0-buildable`)

Build on the old stack in Docker so contributors have something that compiles.

- [x] `ci/Dockerfile`: Debian 10 (buster) from archive.debian.org, with the
      same build dependencies as Debian's package
- [x] `ci/build.sh`: configure and build with Debian's packaging flags
- [x] Verify the image and a full build on x86_64 (builds cleanly; the only
      linker warning is `tempnam` in `incomingdicomassociationcommand.cpp`)
- [x] GitHub Actions workflow that builds the image and runs `ci/build.sh`
      (Release and Debug; untested until first push)
- [x] Remove `.travis.yml` and `model_ginkgo.cpp`
- [x] Raise `cmake_minimum_required` to 3.5 in all four `CMakeLists.txt` files
- [x] Rewrite the README so building starts from this repo

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

### Phase 1: the easier dependencies (builds and starts, branch `phase1-modern-deps`)

Build on Debian 12 (bookworm) with its wxWidgets 3.2, ITK 5, DCMTK 3.6.7 and
OpenSSL 3. VTK 6.3 is still built from source with the legacy OpenGL backend
(see `ci/Dockerfile.bookworm`) until Phase 2. This branch no longer builds on
the buster image; `phase0-buildable` stays as the old-stack baseline.

- [x] `ci/Dockerfile.bookworm`: Debian 12 packages plus VTK 6.3 from source
- [x] wxWidgets 3.2 and GTK3
- [x] ITK 5
- [x] DCMTK 3.6.7, including the copied `libi2d` code
- [x] OpenSSL 3
- [x] Fix the TLS issue: `GTLSTransportLayer` now loads the client
      certificate, key and system trust store through OpenSSL and returns real
      errors; callers check them. Not yet tested against a TLS server.
- [x] Remove the bundled SQLite (the build was compiling against its 2011
      header while linking the system library)
- [x] Remove the Visual Studio projects, `install_open_source/` and the forced
      `i386` macOS architecture
- [x] C++ clean-ups: `throw()` to `noexcept`, `OFauto_ptr` to
      `OFunique_ptr`, `register`, bounded `snprintf`/`strlcpy`, `tempnam` to
      `mkstemp`
- [x] Open a DICOM file and exercise the main tools on this build (image
      display, zoom, pan, measurement tools)

Findings:

- wxWidgets 3.2 keeps asserts on even with `NDEBUG`, and its sizer flag
  checks fired at startup and stopped the program.
  `wxDISABLE_ASSERTS_IN_RELEASE_BUILD()` in `InitWX` restores the 3.0
  behaviour. The sizer flags themselves are still wrong in the GUI code.
- VTK 6.3's CMake files only recognise GCC 3 to 5; the Dockerfile widens the
  version regex.
- Debian's DCMTK doesn't ship `libi2d`, so the copied version stays.
- Building VTK and Ginkgo with all cores can exhaust memory; use `JOBS=4`
  (`docker run -e JOBS=4 ...`).

Open issues:

- The application once exited (code 1) after the log level was set to Debug
  and the logs were copied from the log window. The log from that session was
  cut off, so the cause is unknown; it hasn't been reproduced.
- Containers have no D-Bus session, so GTK prints dconf warnings unless
  `GSETTINGS_BACKEND=memory` is set (the bookworm image sets it).

To build and run:

```
docker build -t ginkgocadx-build-bookworm -f ci/Dockerfile.bookworm ci
docker run --rm -e JOBS=4 -v "$PWD":/src ginkgocadx-build-bookworm \
  ci/build.sh Release build-bookworm
docker run --rm -v "$PWD":/src \
  -v /run/desktop/mnt/host/wslg/.X11-unix:/tmp/.X11-unix -e DISPLAY=:0 \
  ginkgocadx-build-bookworm ci/run.sh build-bookworm
```

### Phase 2: VTK 9 port (done, branch `phase2-vtk9`)

Done in two steps so the application keeps working throughout.

**Step 1: run on VTK 9 with the existing drawing code (done).** Debian 12's
VTK 9.1 package replaces the source-built VTK 6.3.

- [x] `wxVTKRenderWindowInteractor` uses `vtkGenericOpenGLRenderWindow` drawing
      into the wx canvas' own GL context instead of VTK creating one on the X
      window
- [x] That context is a compatibility profile (OpenGL 3.2+), so the legacy
      `glBegin`/`glEnd` drawing still works next to VTK 9
- [x] `GNC::GCS::LegacyGLScope` (`cadxcore/vtk/legacyglscope.cpp`) recreates the
      fixed-function state VTK 6 left behind (camera in the matrix stack, no
      shader, texture unit 0) and resynchronises VTK's state cache afterwards;
      `WidgetsActor` and the image actor draw inside it (replaced in step 2 by
      `GLDraw::FrameScope`)
- [x] The image plane is drawn by `vtkGinkgoLegacyImageActor`, because
      `vtkGinkgoOpenGLTexture` binds its own texture and shader, which VTK 9's
      mappers would bypass
- [x] `vtkgl::` names replaced by GLEW's; removed `vtkSmartVolumeMapper` copy
      (unused) and VTK 6-only Carbon/Cocoa code
- [x] Image display and the ruler, angle and arrow tools work on screen;
      line and label colours re-checked on the ruler after the texture-unit fix

**Step 2: replace the legacy OpenGL drawing (done).**

- [x] `GNC::GCS::GLDraw` (`cadxcore/vtk/gldraw.cpp`): the subset of
      fixed-function OpenGL the widgets use (immediate mode, matrix stack,
      colour, line width and stipple, point size, 2D texturing, attribute
      stack), drawn with one shader through VTK's shader cache and buffer
      classes. Wide and stippled lines are built on the CPU in window
      coordinates.
- [x] All widget drawing converted (about 1,130 calls in 28 files, by renaming
      `glFoo` to `GLDraw::Foo`); the `welevacion` display list replaced by a
      direct draw. Checked on screen: image, ruler, angle, arrow, points and
      labels.
- [x] Image display: `vtkGinkgoOpenGLTexture::Load` only uploads, and `Draw`
      renders with one GLSL 1.50 shader (CPU-mapped colours, grey through the
      lookup table, RGB brightness/contrast). Grey textures are stored as RED
      or RG with a swizzle. `vtkGinkgoImageActor` replaces the legacy actor.
      Checked on screen: display, zoom, window/level.
- [x] `LegacyGLScope` replaced by `GLDraw::FrameScope`, which only saves and
      restores VTK's cached state; the `GL_RENDER_MODE` check is gone. No
      fixed-function OpenGL is left outside `GLDraw`.
- [x] The context check accepts any OpenGL 3.2+ context, core or compatibility
- [x] Checked on screen: corner annotations, colour maps, rotate and flip,
      metric grid, window/level, markers (ellipse, polygon, free form,
      trapezoid), text note, pan and zoom
- ECG widgets and a core-profile run are under [Later](#later).

Findings:

- A plain `wxGLContext` gets the driver's highest compatibility profile on
  Linux (Mesa: 4.5). Asking wx 3.2.2 for one with `wxGLContextAttrs` fails on
  some X servers: wx makes a temporary `glXCreateContext` call that returns
  `BadValue`. `InitializeGL` checks the version and profile it actually got.
- `vtkGenericOpenGLRenderWindow::Render` silently skips the frame unless the
  `WindowIsCurrentEvent` observer answers yes.
- `vtkCamera::GetProjectionTransformMatrix(vtkRenderer*)` returns a matrix that
  is already transposed for OpenGL; the `(aspect, near, far)` overload doesn't.
- VTK 9 leaves other texture units active; legacy code drawing text as
  textured quads then shows black boxes.
- Each container starts as a first run, so Ginkgo shows its license dialog.
- The brightness/contrast tool (Ctrl+B) is registered but no view adds it to
  its toolbar in this code, so it can't be selected; it would only affect RGB
  images anyway. The magnifier widget (`wlupa`) isn't used by any tool. Both
  are the same in 3.8.8.
- VTK 9 clears to the background colour with alpha 0. Copied to the wx
  canvas, that left earlier frames visible behind the image when panning or
  zooming; `wxVTKRenderWindowInteractor::Render` makes every renderer's
  background opaque.

### Phase 3: open the doors (in progress, branch `phase3-open-doors`)

Make it practical for other people to build, run and contribute.

- [ ] `CONTRIBUTING.md`
- [ ] Issue and pull request templates
- [ ] Updated `AUTHORS` and README
- [ ] A start on a test suite (CTest), run in the build container
- [ ] An AppImage build from the container (CI integration comes with the
      GitHub workflow work)

## Later

Known issues that aren't pressing. Pick them up when they get in someone's
way.

- **wxWidgets sizer flags.** The GUI code passes flag combinations wx 3.2
  rejects. Release builds hide the assertions
  (`wxDISABLE_ASSERTS_IN_RELEASE_BUILD`), but the flags are still wrong and
  debug builds stop on them.
- **TLS client certificates** (`GTLSTransportLayer`) have never been tested
  against a real TLS-enabled PACS.
- **Exit after copying debug logs.** Seen once in Phase 1 (log level Debug,
  then copying from the log window); not reproduced since.
- **Core-profile OpenGL.** The drawing code no longer needs a compatibility
  profile, but it has only ever run in one. Running in a core profile is what
  macOS would need.
- **High-DPI screens.** `wxVTKRenderWindowInteractor` sizes the GL surface in
  logical pixels, which will probably look wrong with display scaling.
- **ECG widgets** (`visualizator/widgets/wecg*.cpp`) haven't been checked on
  screen since the drawing changes; that needs an ECG file.
- **Unreachable tools.** The brightness/contrast tool and the magnifier
  (`wlupa`) exist but no view offers them. Wire them up or remove them.
- **GTK3 cosmetics.** The grid configuration dialog's spin boxes are too small
  (GtkSpinButton warnings); some menu items warn about image menu items.
- **Spanish identifiers and comments.** Much of the code is named in Spanish.
  Renaming would help new contributors but is a large change; best done file
  by file alongside other work.
- **GitHub workflow.** `.github/workflows/build.yml` builds on every push but
  hasn't been checked since the move to Debian 12 and VTK 9.
