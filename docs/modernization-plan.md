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

### Phase 3: open the doors (done, branch `phase3-open-doors`)

Make it practical for other people to build, run and contribute.

- [x] `CONTRIBUTING.md`
- [x] Issue and pull request templates
- [x] Updated `AUTHORS` and README
- [x] A start on a test suite: GoogleTest through CTest in `tests/`, run in
      the build container. 17 tests: geometry, GLDraw's matrix stacks and the
      TLS client certificate loading. The overlay test was checked to fail
      with the GL_TEXTURE bug put back.
- [x] An AppImage build from the container: `ci/appimage.sh` with pinned,
      checksum-verified linuxdeploy. Checked on a clean Ubuntu 24.04 (opens
      and displays a DICOM file). CI integration comes with the GitHub workflow
      work.

Findings:

- wx derives the application name, and with it the resources directory
  (`<prefix>/share/<name>`), from the executable name, which is `AppRun`
  inside an AppImage. `Ginkgo`'s constructor now sets it to `ginkgocadx`.
- Debian's DCMTK reads its data dictionary from `/usr/share/libdcmtk17`. The
  AppImage bundles `dicom.dic` and `private.dic` and sets `DCMDICTPATH` in an
  AppRun hook; without them the Modality tag isn't decoded and images don't
  open.
- VTK 9 links the GLVND `libOpenGL.so.0`, which linuxdeploy leaves to the
  host (like all OpenGL libraries). Minimal systems need `libopengl0`.

## Features track: MPR, MIP and 3D

Ginkgo is a 2D viewer. Horos and Weasis also offer multiplanar reconstruction
(MPR), maximum intensity projection (MIP) and 3D rendering. This track adds
them in stages, each usable on its own. The design work is done here so each
stage can be handed to someone (or a model) who follows the steps without
re-deriving the decisions.

### What exists today (checked October 2026)

- **Slice-at-a-time display.** `vtkGinkgoImageViewer`
  (`cadxcore/vtk/vtkginkgoimageviewer.cpp`) shows one 2D image. Moving
  through a series loads the next file through `GNC::StreamingLoader`
  (`cadxcore/main/controllers/streaming/`). No 3D volume of the series is ever
  in memory.
- **A volume loader that nobody calls.**
  `GNC::GCS::ControladorCarga::CargarITKMultidimensional`
  (`cadxcore/main/controllers/controladorcarga.cpp`) reads a list of files
  with `itk::ImageSeriesReader` into one `vtkImageData`, in the files' own
  pixel type, with progress reporting through an `IComando`. Gaps:
  - It reads the files in the order given.
  - It takes the origin from ITK.
  - It doesn't set direction cosines on the `vtkImageData`.
  - It doesn't check that slices are parallel and evenly spaced.
- **Series order isn't geometric.** `HistoryController::GetSeriesSortedFileModels`
  orders by instance number first and position second (`GetFileOrderBySentence`
  in `historycontroller.cpp`). That is fine for paging but not for building a
  volume.
- **A dead reconstruction stub.** `visualizator/visualizator/tools/reconstructiontool.*`
  and `export/ireconstructiontool.h`:
  - menu entries for 3D MPR, MIP 3D, volume rendering, surface rendering and
    endoscopy
  - guarded by `ENABLE_RECONSTRUCTIONTOOL`, which is never defined
  - written against the removed `IHerramienta` API
  - the contract it calls, `IReconstructionContract`, doesn't exist anywhere
  - the hook in `gvistacompleja.cpp` (`/*TODO ... IReconstructionTool`) is
    commented out
  - `visualizator/CMakeLists.txt` still lists `wxvtk/reconstruction/...`
    directories that don't exist
- **Views and tools to copy from.**
  - Opening a view: `OpenMedicalImageViewer` in
    `visualizator/visualizator/controllers/visualizatorcontroller.cpp` creates a
    `VisualizatorStudy`, wraps it in a view, registers it with
    `GetControladorVistas()->Registrar` and starts `CargaAsincrona`.
  - Loading: the view's `CargarEstudio(IComando*)` runs on a worker thread,
    then `IniciarPipeline()` and `OnCargaFinalizada()` run on the UI thread.
  - A one-click tool: `GNC::ShowTagsTool` (`cadxcore/main/tools/showtagstool.*`
    with `export/tools/ishowtagstool.*`) is an `ITool` plus an
    `IViewContractable`, which gives it the active view.
  - Tools for the 2D viewer are added to its toolbar in `gvistacompleja.cpp`
    (the `pToolBar->AddTool(...)` block around line 500).
- **VTK 9.1 has the building blocks** (Debian 12 `libvtk9-dev`):

  | Class | Use | VTK module |
  |---|---|---|
  | `vtkImageResliceMapper` | MPR planes, oblique planes, thick slab MIP/MinIP/average | RenderingImage |
  | `vtkImageSlice`, `vtkImageProperty` | MPR display | RenderingCore |
  | `vtkSmartVolumeMapper` | Volume rendering, 3D MIP | RenderingVolumeOpenGL2 (already linked) |
  | `vtkFlyingEdges3D`, `vtkWindowedSincPolyDataFilter` | Surfaces | FiltersCore |
  | `vtkCornerAnnotation`, `vtkTextActor` | Labels in the new views | RenderingAnnotation, RenderingFreeType |
  | `vtkSTLWriter` | Surface export | IOGeometry |

### Design decisions

1. **A separate MPR view, not a mode of the 2D viewer.** The 2D viewer's
   pipeline, widgets and annotations are built around "one file is one image".
   Feeding it resliced images would mean untangling slice indices, per-file
   widget storage and DICOM-tag annotations. The MPR view is a new tab with
   its own `IVista` subclass and plain VTK 9 props. The cost is that
   measurement tools aren't available in MPR at first (stage F5).
2. **Draw MPR with `vtkImageResliceMapper`, not `vtkResliceImageViewer`.**
   VTK's `vtkResliceImageViewer`/`vtkResliceCursorWidget` gives linked
   crosshairs out of the box. However, it works in the volume's index axes and
   its own camera conventions, so radiological orientation and non-axial
   acquisitions would need fighting. With `vtkImageSlice` +
   `vtkImageResliceMapper` + a `vtkPlane`, the plane and camera are ours. The
   crosshair is a few lines of code. Thick slabs and oblique planes are
   mapper settings.
3. **Patient coordinates everywhere.** The volume is loaded with origin
   (0,0,0) and its spacing. The prop's user matrix maps it into DICOM patient
   space (LPS: +x = patient left, +y = posterior, +z = head):

   ```
   M = | r  c  n  P0 |    r = row direction (first 3 values of 0020,0037)
       | 0  0  0  1  |    c = column direction (last 3 values of 0020,0037)
                          n = r × c (slice normal)
                          P0 = ImagePositionPatient (0020,0032) of the first
                               slice after sorting
   ```

   Set it with `vtkImageSlice::SetUserMatrix` and `vtkVolume::SetUserMatrix`.
   Planes, cameras and the crosshair point are all in LPS, so an oblique or
   coronal acquisition displays the same as an axial one. The F1 spike checks
   that the mapper honours the user matrix. If it doesn't, fall back to
   resampling the volume once into LPS axes with `vtkImageReslice`.
4. **Volume geometry is pure code in CADxCore.** Sorting slices, checking
   spacing and building `M` don't depend on VTK rendering or wx. They go in
   `cadxcore/main/volume/` so the unit tests (which link CADxCore only) can
   test them.
5. **Single-frame, single-component series only at first.** That covers
   ordinary CT, MR, PET and SPECT. Enhanced multi-frame files, RGB and 4D
   (time) series are refused with a clear message (stage F5 lists them).

### Conventions for whoever implements this

- **Branch:** one per stage (`feature-mpr`, `feature-slab-mip`, ...), each
  starting from `improvements`. Commit per numbered step. Messages follow the
  existing style: imperative summary line, plain explanation.
- **Build and test** in the Debian 12 container, as in Phase 2:
  ```
  docker run --rm -e JOBS=4 -v "$PWD":/src ginkgocadx-build-vtk9 ci/build.sh Release build-vtk9
  docker run --rm -v "$PWD":/src ginkgocadx-build-vtk9 sh -c "cd build-vtk9 && ctest --output-on-failure"
  ```
- **Re-run CMake when adding files.** Both `cadxcore/CMakeLists.txt` and
  `visualizator/CMakeLists.txt` collect sources with
  `AUX_SOURCE_DIRECTORY`. A new directory needs its own line, and new files
  need CMake to run again; `ci/build.sh` always configures.
- **Code style:** match the surrounding files. 8-space indentation, the
  LGPL header from `tests/test_gldraw.cpp`, `vtkSmartPointer` for VTK
  objects, `GNC::GCS::Ptr` where the existing API uses it. New identifiers
  and comments in English.
- **Leave the 2D drawing alone.** Don't touch `GLDraw`,
  `vtkGinkgoImageViewer` or the widgets. The new views use standard VTK props
  only.
- **Verify on screen with the user.** Each stage ends with an on-screen check
  by the user (WSLg, `ci/run.sh build-vtk9`). Give them numbered steps, as in
  Phase 2. Rendering problems don't show up in unit tests.
- **Stop and ask** if a step's assumption turns out false: for example, the
  user matrix is ignored, or the render widget doesn't pass mouse events to
  VTK. Don't build around it silently.

### F0: groundwork (small, no visible change)

1. **Remove the dead stub.** Delete:
   - `visualizator/visualizator/tools/reconstructiontool.{h,cpp}`
   - `visualizator/visualizator/export/ireconstructiontool.h`
   - the commented-out `IReconstructionTool` block in `gvistacompleja.cpp`
     (around line 642)
   - the `wxvtk/reconstruction...` lines in `visualizator/CMakeLists.txt`
   - `HGNKVisualizator_Reconstruction` in `export/tools/ivisualizatortools.h`

   Do not reuse ID 4003.
2. **Add the VTK modules** to the `FIND_PACKAGE(VTK ...)` list in the root
   `CMakeLists.txt`: `RenderingImage RenderingAnnotation RenderingFreeType
   FiltersCore IOGeometry`. `vtk_module_autoinit` already uses
   `${VTK_LIBRARIES}`, so the object factories register themselves.
3. **Series geometry** (`cadxcore/main/volume/seriesgeometry.{h,cpp}`, plus
   `AUX_SOURCE_DIRECTORY(main/volume SRCS)` in `cadxcore/CMakeLists.txt`):

   ```cpp
   namespace GNC { namespace GCS {
   struct SliceInfo {
           std::string path;
           double position[3];      // 0020,0032
           double orientation[6];   // 0020,0037
           double pixelSpacing[2];  // 0028,0030: row spacing, column spacing
           int rows, columns;       // 0028,0010 / 0028,0011
           int frames;              // 0028,0008, 1 if absent
           int samplesPerPixel;     // 0028,0002
   };
   struct SeriesGeometry {
           std::vector<std::string> sortedPaths;
           double spacing[3];       // column spacing, row spacing, slice spacing
           double origin[3];        // position of the first sorted slice
           double direction[9];     // row-major 3x3: columns are r, c, n
   };
   // Returns an empty string on success, otherwise a message for the user.
   std::string BuildSeriesGeometry(const std::vector<SliceInfo>& slices, SeriesGeometry& out);
   }}
   ```

   Rules, each with a user-readable failure message:
   - **At least 3 slices.**
   - **Each slice is a single frame and a single sample** (`frames == 1`,
     `samplesPerPixel == 1`).
   - **Same rows, columns and pixel spacing** on every slice.
   - **Same orientation:** each of the 6 values is within 1e-3 of the first
     slice's.
   - **Sorted geometrically:** sort by `dot(position, n)`. Two slices at the
     same position (within 1e-3 mm) means a duplicate or multi-phase series:
     refuse.
   - **Evenly spaced:** the gaps between consecutive slices may differ by at
     most 1% from their mean, or 0.01 mm, whichever is larger. Slice spacing
     is the mean gap. Don't use 0018,0050 or 0018,0088; they are often wrong.
   - **No gantry tilt:** the vector from the first to the last position,
     normalised, must be parallel to `n` (|dot| > 0.999). A tilted series gets
     the message "gantry-tilted series aren't supported yet".
   - **Spacing order:** `spacing[0]` is the column spacing, the second value
     of 0028,0030. `spacing[1]` is the row spacing, the first value. DICOM
     lists row spacing first; getting this backwards stretches non-square
     pixels.
4. **Unit tests** (`tests/test_seriesgeometry.cpp`, added to
   `tests/CMakeLists.txt`). Build the `SliceInfo` lists in code; no files
   needed. Cover:
   - an axial series given in shuffled order: it comes back sorted
   - an axial series stacked feet-first versus head-first: `n` and the order
     agree in both cases
   - a coronal series
   - an oblique (rotated 20° about x) series
   - each refusal: one gap, a duplicate position, mixed orientation, tilt, a
     multi-frame slice, an RGB slice, fewer than 3 slices
   - non-square pixels end up in the right order
5. **Volume loading** (`cadxcore/main/volume/seriesvolume.{h,cpp}`).
   `LoadSeriesVolume(IComando* cmd, const std::list<std::string>& paths,
   vtkSmartPointer<vtkImageData>& volume, SeriesGeometry& geometry)` does
   this:
   - Reads each file's tags with the DICOM manager, as
     `CargarITKMultidimensional` does for the first file
     (`PACSController::Instance()->CrearInstanciaDeDICOMManager()`,
     `CargarFichero`, `getTag`).
   - Fills `SliceInfo` and calls `BuildSeriesGeometry`. On failure it throws
     `ControladorCargaException` with the message.
   - Calls `ControladorCarga::Instance()->CargarITKMultidimensional(cmd,
     sortedPaths, NULL, geometry.spacing)`.
   - Sets the result's origin to (0,0,0). The geometry carries the real
     origin.

   Pixel values come out rescaled (ITK's GDCM reader applies rescale
   slope/intercept), so CT is in Hounsfield units. Check this in the F1 spike
   by reading a CT pixel value in air (about -1000).
6. **Phantom series for on-screen checks.** Add `tests/tools/make_phantom.cpp`,
   built only when `BUILD_TESTING` is on and not run by ctest. It writes
   small CT series with DCMTK into a directory given on the command line:
   - 128×128 pixels, 0.8 mm pixels, 64 slices 2 mm apart, int16, rescale
     intercept -1024
   - background -1000 HU, a 60 mm sphere of 40 HU
   - markers that make orientation unambiguous: a 1000 HU rod along the
     patient's left side (+x) only, a 1000 HU cube at the anterior (−y) edge,
     a small 1000 HU ball near the head end (+z)
   - five variants: `axial` (instance numbers in order), `axial-reversed`
     (instance numbers reversed), `coronal`, `oblique20`, `gap`

   All series share a Frame of Reference and Study, with one Series per
   variant. Import them through Ginkgo's normal file open. In MPR, all but
   `gap` must look the same, and `gap` must be refused with the gap message.
   Reuse the UID and dataset code style from
   `cadxcore/main/controllers/dcmtk/` where possible.

**Done when:** the build is clean, all tests pass (the old 17 plus the new
ones), the app still opens and displays a series as before, and the phantoms
import.

### F1: orthogonal MPR (the main piece)

The user opens a series in the normal viewer and clicks **MPR**. A new tab
shows three linked panes: axial, coronal and sagittal. One click sets the
crosshair point in all three.

1. **Spike first (half a day, throwaway branch).**
   - **Setup:** in a scratch view, put one `wxVTKRenderWindowInteractor`, a
     `vtkRenderer`, and a `vtkImageSlice` with `vtkImageResliceMapper` on the
     `axial` phantom volume.
   - **Things to confirm:**
     - **Display:** it draws.
     - **User matrix:** `SetUserMatrix` with the `oblique20` phantom's `M`
       puts the anatomy upright.
     - **Mouse events:** a `vtkInteractorStyleImage` (or the observer
       approach below) receives mouse events through
       `wxVTKRenderWindowInteractor`.
     - **Text:** `vtkCornerAnnotation` text renders.
     - **Hounsfield units:** values are in HU.
   - **Report** the results to the user before going on.
   - **Possible problem:** `wxVTKRenderWindowInteractor` is also an
     `IWidgetsRenderer`. Check that it renders without a widgets manager
     attached; the history preview panel (`historypanel3base.cpp`) is an
     example of one used outside a full viewer.
2. **Tool.** Define `HGNKVisualizator_MPR 4005` in `ivisualizatortools.h`.
   - **Classes:** add `GNKVisualizator::IMPRTool`
     (`export/tools/imprtool.{h,cpp}`, `ITool` + `GNC::GCS::IViewContractable`,
     family `TFamiliaVisualizacion`, name "MPR"). Add
     `GNKVisualizator::MPRTool` (`tools/mprtool.{h,cpp}`), modelled on
     `ShowTagsTool`.
   - **Registration:** register it in the `ControladorAtencionPrimaria`
     constructor next to `OverlaysTool`. Add
     `pToolBar->AddTool(HGNKVisualizator_MPR, (GNC::GCS::IViewContract*) IVista)`
     in `gvistacompleja.cpp`.
   - **Icon:** a 16×16 PNG next to `ico_menu_imagen.png`, loaded by a new
     `ResourcesManager::ToolIcons::GetIcoMPR()`. Check that the resource
     install rule picks it up.
   - **`ExecuteAction()`:** take the active view's study context. Get the
     active file's SOP Instance UID (tag `0008|0018`) and look up its series
     with `HistoryController::GetFileModelFromSopInstance(uid).series_fk`.
     Then open the MPR view as `OpenMedicalImageViewer` does, with `MPRView`
     in place of `Vista2D`.
   - **Enabled state:** only when the series has at least 3 files and one
     component (`GetNumberOfComponents() == 1` on the 2D viewer). The full
     checks happen while loading, and a failure shows the message in the
     usual "Error opening study" dialog.
3. **View classes** (new directory `visualizator/visualizator/mpr/`, plus an
   `AUX_SOURCE_DIRECTORY` line):
   - **`MPRView`** (`IVista`):
     - `CargarEstudio(cmd)` (worker thread): calls `LoadSeriesVolume`. It must
       not touch wx or render.
     - `IniciarPipeline()` (UI thread): hands the volume and geometry to the
       panel.
     - `DetenerPipeline()`: releases the volume.
     - `SoportaGuardar()` and `SoportaExportar()`: false.
     - `GetWindow()`: the panel.
     - The title: like `Vista2D::GenerarTitulo`, with " (MPR)" appended.
   - **`MPRPanel`** (`wxPanel`): a 2×2 `wxGridSizer`. Three `MPRPane`s and,
     for now, an empty black panel in the fourth cell (F3 puts 3D there). It
     owns the shared state:

     ```cpp
     struct MPRState {
             double cursor[3];          // crosshair point, LPS mm
             double window, level;
             double slabThickness;      // mm, 0 = off (used in F2)
             int    slabMode;           // VTK_IMAGE_SLAB_MAX/MIN/MEAN (F2)
     };
     ```

     It has one shared `vtkImageProperty` (window/level, linear interpolation)
     so the three panes always agree. `SetCursor(const double p[3])` clamps
     `p` to the volume's bounds in patient space (transform the 8 corners
     with `M`) and calls `Update()` on each pane.
   - **`MPRPane`** (`wxVTKRenderWindowInteractor` + `vtkRenderer`): one
     `vtkImageSlice` + `vtkImageResliceMapper` on the shared volume, the
     shared property, and the user matrix `M`. It has its own `vtkPlane`
     through `cursor`, with the normal from the table below. Set
     `SliceFacesCameraOff`, `SliceAtFocalPointOff`, `ResampleToScreenPixelsOn`.

     | Pane | Plane normal | Camera view-up | Camera position (from focal point) | Screen right is |
     |---|---|---|---|---|
     | Axial | (0,0,1) | (0,−1,0) | (0,0,−d): from the feet | patient left |
     | Coronal | (0,1,0) | (0,0,1) | (0,−d,0): from the front | patient left |
     | Sagittal | (1,0,0) | (0,0,1) | (+d,0,0): from the patient's left | posterior |

     These are the usual radiological conventions: axial seen from the feet,
     coronal from the front. Use parallel projection. Reset the zoom to fit
     the volume's projected bounds on first show and on double-click.
4. **Interaction.** Use one custom `vtkInteractorStyle` subclass per pane
   (or observers on the interactor if the spike shows that's easier).
   Defaults chosen to match common viewers:

   | Input | Action |
   |---|---|
   | Left click / drag | Move the crosshair to the point under the mouse (all panes update) |
   | Wheel | Move this pane's plane by ± one slice step along its normal (step = smallest of the three spacings) |
   | Right drag | Window (horizontal) / level (vertical), shared |
   | Middle drag | Pan this pane |
   | Ctrl + wheel, or Ctrl + right drag | Zoom this pane |
   | Double-click | Reset this pane's zoom and pan |

   Picking: `renderer->SetDisplayPoint(x, y, 0); renderer->DisplayToWorld();`,
   then intersect the ray from the camera with this pane's plane. With
   parallel projection, the world point can be projected onto the plane
   along the normal. Remember that VTK's display y runs from the bottom; wx
   gives top-down.
5. **Overlays in each pane.**
   - **Crosshair:** two lines through the cursor showing the other two
     planes, coloured by plane (axial blue, coronal green, sagittal orange; the
     same colour as that pane's border or label). Use a `vtkPolyData` with a
     `vtkActor`, placed 0.1 mm towards the camera so it isn't hidden by the
     slice.
   - **Orientation letters** at the middle of each edge (`vtkTextActor`): for
     the screen direction `v` in LPS, write the letters of its components
     with |value| > 0.3, largest first. The letters are R/L for −x/+x, A/P
     for −y/+y and F/H for −z/+z.
   - **Corner text** (`vtkCornerAnnotation`): plane name, position along the
     normal in mm, W/L, and the slab setting once F2 exists.
6. **Defaults.** Window/level from the first file's 0028,1050/0028,1051 (first
   value if multi-valued), otherwise the volume's scalar range. Keys 1–4 set CT
   presets: soft tissue 400/40, lung 1500/−600, bone 2000/400, brain 80/40.
   The cursor starts at the volume's centre.
7. **Memory and lifetime.** The volume is held once, by the panel. Closing the
   tab must free it; check with `top` on a large series. Refuse up front if
   rows × columns × slices × bytes per voxel is over 2 GB, with a message.

**Done when:**
- **Phantoms:** the five phantoms behave as described in F0.6. In every
  non-refused variant, the rod is on the screen right in the axial and coronal
  panes, the cube is at the top of the axial pane and on the left of the
  sagittal pane, and the head-end ball is at the top of the coronal and
  sagittal panes.
- **Real CT:** a CT series of at least 300 slices opens in under about 10
  seconds in the container, and dragging the crosshair stays smooth.
- **2D viewer:** still works, and its tab and the MPR tab can be switched
  freely.
- **Tests:** unit tests pass, and the user has checked the result on screen.

### F2: thick slab MIP / MinIP / average

1. Add a small toolbar above the MPR grid (a `wxGinkgoToolBar` like
   `BarraHerramientasBotones` in `gvistacompleja.cpp`, or plain wx controls):
   - a mode choice: Off, MIP, MinIP, Average
   - a thickness `wxSpinCtrlDouble`: 1–100 mm, default 10, step 1
2. Apply the setting to every pane's mapper. Set `SetSlabThickness(mm)` (0
   when Off), `SetSlabTypeToMax()`, `SetSlabTypeToMin()` or
   `SetSlabTypeToMean()`, and `SetSlabSampleFactor(2)`. Show it in the corner
   text ("MIP 10 mm").
3. Keys: `M` toggles MIP with the last thickness.

**Done when:** add a phantom variant `dots` with five 1000 HU, 2 mm balls at
different z inside a 20 mm band. With axial MIP at 20 mm centred on the band,
all five show at once; with Off, only the one in the plane shows. CT angiography
or a chest CT should show vessels as continuous lines in a 20–30 mm coronal
MIP.

### F3: volume rendering and 3D MIP

The fourth MPR cell becomes a 3D pane. It's off until the user clicks
**3D**, because it costs GPU time.

1. **Pipeline.**
   - `vtkSmartVolumeMapper` (`SetRequestedRenderModeToDefault`, which picks
     GPU ray casting) on the same volume.
   - `vtkVolume` with the same user matrix `M`.
   - `vtkVolumeProperty`: linear interpolation, `ShadeOn` for composite,
     `ShadeOff` for MIP.
   - Camera: perspective off by default, start looking from the front
     (coronal view, head up).
2. **Presets** (in one table in code, `mpr/volumepresets.cpp`). Each preset is a
   blend mode, a scalar opacity `vtkPiecewiseFunction` and a
   `vtkColorTransferFunction`, with points in HU:
   - **CT bone:** composite; opacity 0 at 150, 0.6 at 400, 0.9 at 1500; colour
     from dark red at 150 to off-white at 1000.
   - **CT soft tissue:** composite; opacity 0 at −200, 0.15 at 0, 0.3 at
     100, 0.8 at 400.
   - **CT angio:** composite; opacity 0 at 100, 0.5 at 250, 0.9 at 600.
   - **MIP:** blend mode maximum intensity; grey ramp across the current
     window/level.
   - **MR default:** composite; ramp across the volume's 5th–99th
     percentiles. Compute them once with a histogram (`vtkImageAccumulate`).
3. **Interaction:**
   - left drag: rotate (trackball camera)
   - middle drag: pan
   - wheel: zoom
   - right drag: shift the transfer function along the value axis, and widen
     or narrow it, the same gesture as window/level
   - a preset choice and a Reset button in the MPR toolbar
   - while the user is dragging, set `vtkSmartVolumeMapper`'s interactive
     sample distance coarser and restore it on release, so rotation stays
     fluid
4. **Crosshair in 3D:** draw the three MPR planes as thin outlined rectangles
   (`vtkOutlineSource` / polylines). Optional: a toggle to clip the volume at
   the axial plane (`vtkSmartVolumeMapper::AddClippingPlane`).

**Known limits:** in the Docker/WSLg setup, Mesa's software renderer
(llvmpipe) is used. Expect a few frames per second on a 512×512×300 CT. That's
acceptable for checking correctness; real use needs a GPU. Record the actual
renderer (`glGetString(GL_RENDERER)`, already logged by `InitializeGL`) in any
performance notes.

**Done when:** the phantom's sphere, rod, cube and ball appear in the right
places in 3D, and a real CT shows bone (bone preset) and vessels (angio or
MIP preset) recognisably. Turning 3D off frees the GPU work (no renders
triggered by MPR interaction).

### F4: surface rendering and STL export

1. In the 3D pane, add a "Surface" mode next to the volume presets.
   - **Iso value:** an HU threshold in a spin control, default 300 (bone).
   - **Pipeline:**
     - `vtkFlyingEdges3D` (`ComputeNormalsOn`)
     - `vtkWindowedSincPolyDataFilter` (20 iterations, pass band 0.1)
     - optionally `vtkQuadricDecimation` (target reduction 0.5) when there
       are over 2 million triangles
     - `vtkPolyDataMapper`
     - `vtkActor` with user matrix `M`
2. **Threading:** run the extraction on a worker thread through
   `GetCommandController()->ProcessAsync` with a progress message; a large CT
   takes seconds. Never update the actor from the worker thread; hand the
   result back on the UI thread.
3. **Export:** "Export surface (STL)…" writes the current surface with
   `vtkSTLWriter`. First transform it into patient mm with
   `vtkTransformPolyDataFilter` and `M`, so the file has real-world size for 3D
   printing.

**Done when:** the phantom sphere exports as an STL that a third-party viewer
measures at 60 mm across, and a real CT gives a recognisable bone surface.

### F5: later, in rough order of value

- **Measurements in MPR.** A distance tool first (`vtkDistanceWidget` or a
  small custom line), in patient mm. Porting the 2D widgets is much bigger.
- **Oblique MPR.**
  - Rotate the crosshair's frame. `MPRState` gets a rotation matrix, and
    each pane's normal and view-up come from its columns instead of the fixed
    table.
  - Ctrl+drag on a crosshair line rotates the other two planes about this
    pane's normal.
- **Link with the 2D viewer.** Clicking in MPR moves the 2D viewer to the
  nearest slice, and the reverse.
- **Export.** A secondary capture or PNG of a pane; a series of slab
  images.
- **More input types:**
  - gantry-tilted CT: resample with a shear
  - enhanced multi-frame CT/MR: one file with per-frame positions in the
    functional groups
  - 4D series: choose a time point
- **Not planned:** virtual endoscopy, PET/CT fusion, curved MPR.

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
