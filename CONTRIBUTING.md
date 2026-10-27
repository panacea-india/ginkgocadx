# Contributing to Ginkgo CADx

Thanks for helping. Bug reports, fixes and improvements are all welcome.

Ginkgo CADx is an old code base that was recently moved onto current
libraries. [docs/modernization-plan.md](docs/modernization-plan.md) explains
where things stand, what changed and why, and lists known issues under
"Later". It is a good place to find something to work on.

## Reporting a bug

Open an issue using the bug report template. The most useful reports say:

- what you did, step by step
- what you expected and what happened instead
- the commit or version you built, and your OS and graphics driver
- the log (run from a terminal; Ginkgo writes to standard output)

Please don't attach patient data. Use anonymized DICOM files or public sample
data.

## Building and running

Everything builds in a Docker container, so you only need Docker on an x86_64
machine. See [README.md](README.md) for the commands. In short:

```
docker build -t ginkgocadx-build-bookworm -f ci/Dockerfile.bookworm ci
docker run --rm -e JOBS=4 -v "$PWD":/src ginkgocadx-build-bookworm ci/build.sh
```

`JOBS` limits parallel compile jobs; with all cores the build can run out of
memory. `ci/run.sh` installs the build inside the container and starts it (the
container needs a display; the README shows how on Linux and on Windows with
WSLg).

Each new container starts like a fresh install, so Ginkgo shows its license
dialog first.

## Making a change

1. Fork the repository and create a branch from the default branch.
2. Keep each pull request to one topic. Large changes are easier to review as a
   series of smaller commits.
3. Build in the container and run the tests (`ctest`, see below) before opening
   the pull request.
4. If you change what the application draws or how it behaves, check it on
   screen and say in the pull request what you checked.
5. Describe what changed and why in the pull request. The template has a short
   checklist.

### Code style

There is no formatter configuration; match the code around your change:

- 8-space indentation, no tabs
- the opening brace of a function on its own line, other braces on the same
  line as the statement
- comments explain why, not what
- new files carry the LGPL 3 header used by the existing ones

Much of the code is named and commented in Spanish. New code should use
English. Renaming existing code is welcome but best done separately from
functional changes.

### OpenGL

Widgets draw through `GNC::GCS::GLDraw` (`cadxcore/vtk/gldraw.h`), which
implements the small fixed-function subset they need on top of modern OpenGL.
Don't add new fixed-function calls (`glBegin`, `glMatrixMode`, ...): they don't
exist in core-profile contexts.

### Line endings

The repository stores text files with LF line endings, and `.gitattributes`
forces LF for shell scripts. On Windows, keep `core.autocrlf` consistent so
diffs don't show every line as changed.

## Tests

Tests use CTest and live in `tests/`. After a build:

```
docker run --rm -v "$PWD":/src -w /src/build ginkgocadx-build-bookworm ctest --output-on-failure
```

The suite is small for now. New tests for code you touch are very welcome,
especially for logic that doesn't need a display.

## License

Ginkgo CADx is licensed under the GNU Lesser General Public License v3. By
contributing you agree that your contribution is licensed under the same terms.
