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
- A C compiler and `libsqlite3` on the host for the C tests (`make test`), plus
  node for the catalog check.
- `cmake` and a host Qt 6 for the Qt and QML tests (`make qt-test`) — optional,
  CI runs them on every push.

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

Builds and runs `test/test_tracker.c` on the host. The asserts cover the
session logic (how time is credited, page jumps, sleep, the split at midnight,
recovery when the daemon was down, a reader switched off inside a book), the
aggregates behind the screens (the streak, the year grid, per-book time and
pace, the hand-set totals), the version comparison the updater runs on, the log
rotation, and the two parts of the daemon that have a host build: whether it
recognises one of its own in the pidfile, and the marks it leaves for the next
start. No device needed.

Paths are overridable so the tests can point the code at temporary files:
`POCKETBOOK_STATISTICS_DB`, `_EXPLORER_DB`, `_PIDFILE`, `_PROC` and `_LOG`.

`make test` also runs `tools/check_qml.py`, which catches QML mistakes that pass
qmllint and then break the app on the device, and `tools/check_i18n.mjs` (needs
node), which reads the 29 catalogs and checks each against English: keys, list
lengths, placeholders, plural arity, and the four places a language has to be
listed. Keys no catalog has yet are recorded in `qt/qml/i18n/untranslated.json`;
a new gap fails, and translating one means the row has to go.

```bash
make qt-test
```

The Qt half, against a host Qt 6 — any 6.x, since the device's own 6.8.2 only
matters to the binary that ships. Needs `cmake` and Qt 6 (`Core Gui Qml Quick
Test QuickTest`); CI installs `qt6-base-dev qt6-declarative-dev` plus the
`qml6-module-*` runtime modules. It builds `test/CMakeLists.txt`, a project of
its own that never touches the cross-build, and covers cover extraction for
every format the app reads itself, the launcher entry, the shim's two writes to
the user partition, the update path from the release list to the staged binary,
and all four aggregates of the bridge over a seeded pair of databases — plus the
screens themselves, instantiated against a stub of the firmware's QML module at
the metrics measured on a PB629.

Every device path goes through `devicePath()`, so `POCKETBOOK_STATISTICS_ROOT`
moves the whole of `/mnt/ext1` into a temporary directory; `_APP` names the
installed binary and `_NO_HANDOVER` stops the updater starting the swap script.
`inkview_bridge.cpp` is the one file with no host build — the firmware's own
calls are resolved with `dlsym` out of the reader's `libinkview.so` — and
`test/qt/inkview_stub.cpp` stands in for it, scripted per test.

What still has no host build at all: `run_daemon()` and `spawn_daemon()` (a poll
loop that never returns, and a fork), `main.cpp`, and `inkview_bridge.cpp`.

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
qt/src/         Qt/C++: main, QML bridges, cover extraction, icon installer
qt/qml/         the UI (com.pocketbook.controls) + icons
qt/qml/i18n/    one string catalog per language, driven by Tr.qml
qt/third_party/ vendored sqlite3 + miniz (source only)
third_party/    pocketbook-sdk-qt6 (fetched by `make sdk`, git-ignored)
test/           host-side tests: C asserts, the Qt suites and the QML ones
tools/          icon generator, QML checker, catalog checker
```

## How the pieces fit

- `src/*.c` is plain C shared by the daemon and the Qt app: `tracker.c` derives
  sessions from `explorer-3.db`, `stats_db.c` runs the aggregation queries,
  `daemon.c` is the poll loop.
- `qt/src/main.cpp` boots Qt against the device's plugins (QPA `pocketbook2`,
  software rendering), starts the daemon, and loads the QML scene.
- `qt/src/stats_bridge.cpp` exposes the C stats to QML; `book_cover.cpp` pulls
  covers out of EPUB, FB2 and CBZ files with miniz; `installer.cpp` registers
  the launcher icon on first run; `shim.cpp` installs the small script that
  starts the daemon when a book is opened.
- `qt/qml/` is the UI. Text goes through the `Tr` singleton; all
  spacing/colors come from the firmware's `GlobalValues`.

See the toolchain notes in
[fstanis/pocketbook-sdk-qt6](https://github.com/fstanis/pocketbook-sdk-qt6) for
the hard constraints (softfp ABI, `rcc --no-zstd`, exact Qt version match).

## CI

`.github/workflows/build.yml` runs the host tests on every push and builds the
app in the SDK image. Every run uploads `PocketBookStatistics.zip` as a workflow
artifact.

Pushing a tag that starts with `v` publishes that build as a GitHub release, so
installing needs no local toolchain:

```bash
git tag v2.1.0 && git push origin v2.1.0
```

The tag has to match `VERSION`, or the build fails on purpose: the in-app update
check compares the release tag against the version compiled into the binary.

A pull request can publish a build too, for testing on a real device, but only
when you ask for it:

- put `[device]` anywhere in the commit message, or
- change `VERSION` in that commit.

Either one publishes that push as `vX.Y.Z-prN`, a pre-release. `/releases/latest`
ignores pre-releases, so only a device carrying the
`system/pocketbook-statistics/prerelease` marker file is ever offered one.

`VERSION` also has to stand above the last release, or the reader would never be
offered the build; when it does not, the run warns and publishes nothing. The
number is counted from the tags already there, and `-pr` builds share that
counter with hand-cut `-rc` candidates, so they never collide.

## Translations

There are 29 catalogs in `qt/qml/i18n/`. Each is a plain `.js` file: a
`strings` map keyed by the ids the QML uses, plus a `plural(n)` function
returning the index into the `"plural.*"` form arrays (two forms for English,
three for Russian, four for Slovenian). `{placeholder}` markers are filled in by
`Tr.t(key, values)`, and a catalog may ignore a placeholder its wording does not
need.

To add a language, copy `en.js` to `<code>.js`, translate it, then:

1. add `<file>i18n/<code>.js</file>` to `qt/qml/pocketbook-statistics.qrc`,
2. add `import "i18n/<code>.js" as <Name>` and one `"<code>": <Name>` row to the
   `catalogs` table in `Tr.qml`,
3. add a row to `kLauncherNames` in `qt/src/installer.cpp`: the launcher tile's
   label is written before any QML engine exists, so it is the one user-facing
   string outside the catalogs.

Nothing else changes, because call sites only ever name keys. Keys a catalog
leaves out fall back to English, so a partial translation is usable.
