# Building PocketBook Statistics

PocketBook Statistics is a single ARM ELF that dynamically links the Qt 6.8.2 already
present on the device (in `/ebrmain`), so nothing Qt-related is bundled. The
build runs in a Docker container that provides the cross-compiler and the
matching Qt headers; you don't need Qt or an ARM toolchain on your host.

## Prerequisites

- **Docker** (or OrbStack on macOS). The Qt6 builder image is pulled
  automatically on first `make qt`.
- **git**, **make**.
- Optional: **Python 3 + Pillow** only if you want to regenerate the icons.
- A C compiler and `libsqlite3` on the host for the unit tests (`make test`).

## One-time setup

```bash
make sdk
```

This shallow-clones [fstanis/pocketbook-sdk-qt6](https://github.com/fstanis/pocketbook-sdk-qt6)
into `third_party/` and sparse-checks-out only the four SDK files the build
links against (`inkview.h`, `hwconfig.h` and two stub libraries) — about 2 MB,
not the full 2.6 GB SDK submodule.

## Build

```bash
make qt
```

Produces `build-qt/PocketBookStatistics.app` (an ARM32 softfp ELF). The Makefile runs:

```
docker run --rm -v "$PWD:/src" -w /src \
  ghcr.io/fstanis/pocketbook-sdk-qt6-builder \
  bash -c 'cmake -B build-qt -DCMAKE_TOOLCHAIN_FILE=.../pocketbook.toolchain.cmake \
           && cmake --build build-qt -j8'
```

## Deploy (development)

With the reader mounted over USB:

```bash
make deploy          # copies to $(DEVICE)/applications/PocketBookStatistics.app
```

Adjust `DEVICE` at the top of the `Makefile` to your reader's mount point
(default `/Volumes/NO NAME`).

## Tests

```bash
make test
```

Builds and runs `test/test_tracker.c` on the host — asserts covering the session
derivation logic (idle capping, recovery/backfill, dedupe, page accounting). No
device needed.

## Icons

The launcher icons are checked in under `qt/qml/` (embedded into the binary as a
Qt resource). To regenerate them from `tools/make_icon.py`:

```bash
pip install Pillow
make icons
```

## Project layout

```
src/            shared C core (no Qt): tracker, stats DB, daemon
qt/src/         Qt/C++: main, QML bridges, EPUB cover extraction, icon installer
qt/qml/         the UI (com.pocketbook.controls) + icons
qt/qml/i18n/    one string catalog per language, driven by Tr.qml
qt/third_party/ vendored sqlite3 + miniz (source only)
third_party/    pocketbook-sdk-qt6 (fetched by `make sdk`, git-ignored)
test/           host-side unit tests
tools/          icon generator
```

## How the pieces fit

- `src/*.c` is plain C shared by the daemon and the Qt app: `tracker.c` derives
  sessions from `explorer-3.db`, `stats_db.c` runs the aggregation queries,
  `daemon.c` is the poll loop.
- `qt/src/main.cpp` boots Qt against the device's plugins (QPA `pocketbook2`,
  software rendering), starts the daemon, and loads the QML scene.
- `qt/src/stats_bridge.cpp` exposes the C stats to QML; `epub_cover.cpp` pulls
  covers out of EPUBs with miniz; `installer.cpp` self-registers the launcher
  icon on first run.
- `qt/qml/` is the UI. Text goes through the `Tr` singleton; all
  spacing/colors come from the firmware's `GlobalValues`.

See the toolchain notes in
[fstanis/pocketbook-sdk-qt6](https://github.com/fstanis/pocketbook-sdk-qt6) for
the hard constraints (softfp ABI, `rcc --no-zstd`, exact Qt version match).

## CI

`.github/workflows/build.yml` runs the host tests on every push and builds the
app in the SDK image. Every run uploads `PocketBookStatistics.zip` as a workflow artifact;
pushing a tag that starts with `v` additionally publishes it as a GitHub
release, so installing needs no local toolchain:

```bash
git tag v0.1.0 && git push origin v0.1.0
```

## Translations

Every catalog in `qt/qml/i18n/` is a plain `.js` file: a `strings` map keyed by
the ids the QML uses, plus a `plural(n)` function returning the index into the
`"plural.*"` form arrays (two forms for English and German, three for Russian).
`{placeholder}` markers are filled in by `Tr.t(key, values)`; a catalog is free
to ignore a placeholder its wording doesn't need.

To add a language, copy `en.js` to `<code>.js`, translate it, then:

1. add `<file>i18n/<code>.js</file>` to `qt/qml/pocketbook-statistics.qrc`,
2. add `import "i18n/<code>.js" as <Name>` and one `case "<code>": return <Name>;`
   to `Tr.qml`.

Nothing else changes — call sites only ever name keys. Keys a catalog leaves
out fall back to English, so a partial translation is usable.
