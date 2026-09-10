# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

This file holds rules only. Where to look for the rest:

- **`docs/DECISIONS.md`** — why each rule exists: the incident, the measurement, what was tried and failed. Same headings as here. Read the matching section before changing or relaxing a rule.
- **`docs/CHANGELOG.md`** — what each release changed and why.
- **`docs/DEVICE-DATA.md`** — what the firmware stores (both databases, field coverage on a real device, the hash that joins them) and what it does not. Check it before designing a metric.
- **`docs/STATS-PLAN.md`** — which metrics the app should show, each with its data source, and the ones that cannot be built. Read it before proposing a new metric.
- **`BUILDING.md`** — the build for a human: prerequisites, project layout, adding a translation.

## Environment quirks

- The working directory still has its old name, `pocketbook-readtrack `, **with a trailing space**. Always quote paths. The project and its GitHub repo are `pocketbook-statistics`.
- Git repo on `main`, remote `origin` → `winst0niuss/pocketbook-statistics`. History starts at 2.0.0; nothing earlier was kept.
- No Docker, cmake or Qt on this machine: `make qt` and `make qt-test` run only in CI.
- `PLAN.md` and `todo.md` are the owner's working notes, git-ignored — not project documentation or a spec. Stage explicit paths; `git add -A` has swept one in before.

## Commands

```bash
make sdk      # one-time: sparse-clones fstanis/pocketbook-sdk-qt6 into third_party/ (~2 MB)
make qt       # cross-compiles in Docker -> build-qt/PocketBookStatistics.app (ARM32 softfp ELF)
make test     # host build+run of test/test_tracker.c, + check_qml.py + check_i18n.mjs
make qt-test  # cmake project under test/: the Qt half and the screens (needs a host Qt 6)
make deploy   # cp to $(DEVICE)/applications/ (DEVICE defaults to /Volumes/NO NAME)
make icons    # regenerate qt/qml/*.bmp from tools/make_icon.py (needs Pillow)
make qmlcheck # tools/check_qml.py alone, without the C tests
make i18ncheck # the 29 catalogs alone (needs node)
```

- **`make test`** is one assert-based binary, no test selection: to run one scenario, comment out the others in `main()` of `test/test_tracker.c`. It compiles `src/tracker.c`, `stats_db.c`, `version.c`, `log.c` and `daemon.c` against the host `libsqlite3`, so **the C core must stay free of Qt and device headers**. Tests build their DBs under `/tmp` and pass the paths to `tracker_init`. C paths are overridable: `POCKETBOOK_STATISTICS_DB`, `_EXPLORER_DB`, `_PIDFILE`, `_PROC`, `_LOG` — which is also how a host build is pointed at a copy of a device's databases.
- **`make qt-test`** covers everything in `qt/src/` except `main.cpp` and `inkview_bridge.cpp` (replaced by `test/qt/inkview_stub.cpp`), plus every screen (`test/qml/stubs/` stands in for `com.pocketbook.controls` at PB629 metrics). Any host Qt 6. Build dir `build-host/`; ctest suites `book_cover`, `installer`, `shim`, `stats_bridge`, `updater`, `qml`. One suite: `ctest --test-dir build-host -R shim --output-on-failure`. One function: `QT_QPA_PLATFORM=offscreen ./build-host/tst_shim installsForEveryReadingFormat` (the env var and `tst_qml`'s `-import test/qml/stubs` are ctest properties, not compiled in). Add a test here when touching the bridge, covers, installer, shim, updater or QML.
- **Every `/mnt/ext1` path in `qt/src` goes through `devicePath()`** (`qt/src/device_paths.h`); `POCKETBOOK_STATISTICS_ROOT` relocates them all. A new absolute path must too, or its test writes to the real filesystem. `POCKETBOOK_STATISTICS_APP` names the installed binary, `_NO_HANDOVER` stops the updater spawning the swap script.
- A new C file, or one that gained a call: `cc -c -O2 -Wall -Wextra -std=gnu99 src/<file>.c -o /dev/null` before pushing.
- CI (`.github/workflows/build.yml`): `make test`, `make qt-test` (plus a coverage summary that gates nothing), then `qmllint qt/qml/*.qml` and `make qt` in the SDK image; nothing is published over a failing test. Only qmllint's exit code matters — its import/unqualified-access warnings are noise. CI is the only thing that compiles the C++ for ARM: `gh run watch <id> --exit-status` is the wait.
- **A property on an Item may not be called `x`, `y`, `z`, `width`, `height`, `state`, `data`, `parent`, `opacity`, `visible`, `enabled`, `anchors`, `clip`, `focus` or their neighbours**, and no function or id may be duplicated: the component fails to instantiate and the app silently stops opening. `tools/check_qml.py` holds the list.
- **Appearance can be verified only on a reader**, from a screenshot it writes to `screens/scrNNNN.bmp` on its storage. Measure sizes and margins, don't eyeball: `sips -s format png`, decode the PNG in plain Python (zlib + the five filters; Pillow is not installed), take bounding boxes of dark pixels. `Global.dp(1)` ≈ 1.32 px on a PB629 (758x1024).
- **SQL over the firmware library**: run it first against a copy of a real `explorer-3.db` with the `sqlite3` CLI. Copy `-wal` and `-shm` along with the `.db`, all *before* opening anything — the CLI checkpoints on open and truncates the WAL.
- **Startup is ~3.2 s on a PB629 and has been measured — do not re-investigate.** It is `engine.load()` building three tabs; the lever is building a tab when first shown.

## Architecture

One ARM ELF that is both the app and its background daemon; it links the device's own Qt 6.8.2 from `/ebrmain` (nothing Qt is bundled). SQLite and miniz *are* bundled from `qt/third_party` (`SQLITE_OMIT_LOAD_EXTENSION`, `SQLITE_THREADSAFE=1`); host tests use the host's `-lsqlite3`, so a passing test does not prove a version-specific SQLite feature exists on the reader.

**Data flow:** firmware library DB (`explorer-3.db`, opened **read-only**, never written) → `tracker.c` derives sessions → own stats DB (`/mnt/ext1/system/pocketbook-statistics/statistics.db`) → `stats_db.c` aggregates → `stats_bridge.cpp` shapes `QVariantMap`s → QML tabs.

**Layers:**
- `src/*.c` — plain C, no Qt. `tracker.c` (session derivation + schema + idempotent migration), `stats_db.c` (aggregation SQL), `daemon.c` (pidfile, 30 s poll loop, `spawn_daemon`).
- `qt/src/main.cpp` — `--daemon` is handled **before any Qt call** and runs the pure C loop. Otherwise: `InitInkview`, then **`QT_QPA_PLATFORM=pocketbook2` is set unconditionally** and `dropPlatformArgs()` removes any `-platform` argument (a PB634 inherits a name its Qt cannot load, issue #10). Never choose the plugin from the plugin directory listing — tried, wrong. Then software rendering, launcher icon registration, daemon fork, `qrc:/main.qml`. Qt diagnostics go into `app.log` until the scene stands. A build with a version suffix (`-pr`/`-rc`) also logs each startup step, the environment and the command line; `anomaly()` lines (an overridden platform, a dropped `-platform`) are written in every build.
- `qt/src/stats_bridge.cpp` — the only QML-visible object (`stats`); each `Q_INVOKABLE` opens/closes explorer-3 itself. The whole QML↔C++ surface is seven calls: `overall()`, `currentBook()`, `month(year, mon)`, `year(y)`, `openBook(path)`, `setTotalHours(h)`, `setBooksFinished(n)`; private `catchUp()` runs inside each of the four aggregates. The setters store the difference from the measurement as an offset in `meta`; a negative argument clears it. `year()` is the streak screen's whole feed: one entry per day from 1 January (0 not read, 1 read), current and best runs, where measurement begins. `openBook()` hands `currentBook().filePath` (filled only while the file is on the device) to the firmware's `OpenBook`. `month()` returns the whole month in one map: `days[]` with `secs` and `books[]` with cover URLs, plus `readDays`.
- `qt/src/inkview_bridge.cpp` — the **only** TU allowed to include `inkview.h` (its macros collide with Qt). Also holds the two network calls.
- `qt/src/updater.cpp` — the `updater` context property: version check, unpack, stage, hand over.
- `qt/qml/` — two screens, no scrolling (e-ink): `OverviewTab` and `CalendarTab`, chosen from a bottom bar in `main.qml`. `AboutTab` is toggled from a header glyph opposite the firmware's home button. Its streak card opens `StreakTab` (the year as a grid, January-June over July-December), built by a `Loader` on first open. The card is hidden below two days; a day counts at a minute or more; today may be empty without breaking the streak; untracked days never count. The streak sentence is split on a marker passed through `Tr.t`. On the calendar, a day opens `PanelDialog`, a book in it opens `BookDialog`. The Overview's `Card`/`StatCard` are inline components whose width is derived (`(content - gap) / 2`), never placed.

### The release loop

- A device-side change is proved on the device, in a PR. A PR push publishes a build only if its commit message contains `[device]` or it changes `VERSION`; it goes out as `vX.Y.Z-prN`, a pre-release.
- Only a reader carrying `system/pocketbook-statistics/prerelease` (placed by hand over USB; nothing in the app creates it) is offered pre-releases; `/releases/latest` skips them.
- `VERSION` must stand above the last release, or the run warns and publishes nothing. `-pr` and `-rc` share one counter numbered from existing tags; `version_compare()` orders by that number (`2.1.0-pr7` > `2.1.0-rc3`, both < `2.1.0`). The workflow stamps the full string into `VERSION` before building. A candidate by hand: set `VERSION` to `X.Y.Z-rcN` and push the tag.
- To release: bump `VERSION` in the commit that earns the release, move the `Unreleased` entries of `docs/CHANGELOG.md` under the new version, merge, tag `vX.Y.Z`. The tag must match `VERSION` — CI fails the build otherwise. Then delete every `-rc`/`-pr` release and tag of that version.

### Invariants that are easy to break

- **`recovered = 1` rows are estimates.** They count toward totals but are excluded (`AND recovered = 0`) from every derived metric — averages, pages/min, session counts. Capped at `RECOVERED_CAP_SECONDS`, no `pages_start`.
- **Sessions never span local midnight** — including sessions written whole (the first observation of a session, `tracker_recover()`), through `insert_session_split()`. The counted window is `[position_ts - active, position_ts]`, never the row's full span; `active` is capped at `RECOVERED_CAP_SECONDS`. `split_legacy_midnight()` is one-shot via a marker in `meta`; a later fix to it needs a new key.
- **Reading time is credited by `credited()`**: at most `pages x SECONDS_PER_PAGE_CAP` (300 s), `IDLE_CAP_SECONDS` (600) with no page evidence, never over `RECOVERED_CAP_SECONDS` (90 min) per unobserved stretch. A page backwards buys nothing.
- **A big forward move is navigation** (`page_jump()`): at least `JUMP_MIN_PAGES` (10) needing more than `SECONDS_PER_PAGE_MIN` (15 s) a page costs the window its seconds *and* its pages. `pages_end` still tracks the real position.
- **A page the firmware has not stamped is not a position.** When `position_ts` stands at or before both the open and the previous row's end, `tracker_observe()` keeps the last stamped page as the baseline.
- **The pace is built on `pages_read`, never on `pages_end - pages_start`.** It stays 0 on recovered rows and is split at midnight with the seconds; aggregates dividing pages by minutes select `pages_read > 0`. The `pages_read_1_6_5` backfill must stay one-shot.
- **A position stamped before the open belongs to the previous session** (reader switched off inside a book: `position_ts < opentime`). `close_previous_session()` credits it there under the ordinary rules, via `credit_window()`.
- **A live pid is not our pid.** `daemon_alive()` and the shim require `/proc/<pid>/cmdline` to hold the app's name *and* `--daemon`; the pid alone decides only where there is no procfs.
- **A new tracker resumes the open row** for `(book_id, opentime)` via `resume_session()`, never starts one beside it.
- **What has been credited is what the row says, never what a process remembers.** The daemon and `catchUp()` write the same session: `tracker_observe()` re-reads the open row on *every* observation, `update_session` carries `AND end_time<?1`, and the migration clamps any non-recovered row to its own span.
- **`tracker_recover()` skips the session open right now** — the book `tracker_read_state()` returns.
- **Reading time comes from the firmware's `position_ts`, never from counting our polls** — the daemon is not scheduled while a book is on screen. `position_ts` is a save, not a page turn: on a PB629 it comes every 15 to 75 minutes.
- **Device sleep is not reading.** `note_sleep()` compares `CLOCK_MONOTONIC` with `time()` and adds only stretches over `DEEP_SLEEP_SECONDS` (15 min) to `meta.sleep_total`; rows carry `sleep_end`, and the next observation subtracts the difference before `credited()`. Only marks one poll apart are classified; the total is written only when it grows.
- **Manual totals live in `meta`** (`manual_offset_books` / `manual_offset_seconds`) and are added only where those two cards are built, in `overall()`. Never in a session row.
- **The Overview shows measurement, not inference.** No ratios against the current shelf; the finished count is all-time.
- **Deduplicate by normalized title, not `book_id`** (`sameBook()` / `titleWords()`).
- **Nothing before `meta.tracking_since` is shown as history.** Every session aggregate appends `AND_TRACKED` (`src/stats_db.h`); the calendar shows those days as unknown (`trackedFromDay`), dims them and prints `trackingSince` beneath the grid. Firmware finish dates are exempt, with `secs = 0`.
- **"Finished" always comes from the firmware** (`books_settings.completed` / `completed_ts`).
- **`VERSION` is the single source of the version** (CMake reads it into `APP_VERSION`).
- **The updater compares every non-draft tag and keeps the highest** — GitHub sorts `/releases` by tag text, not date.
- **A running binary cannot overwrite itself.** Stage `PocketBookStatistics.app.new` beside the installed file (same mount — not `/tmp`); a generated `/bin/sh` script waits for the PID, kills the daemon, `mv`s and restarts.
- **The update path repaints before blocking.** `check()`/`install()` run on the GUI thread and call `publish()` before each blocking firmware call; user input stays excluded.
- **Covers must outlive the book file:**
  - Key: the bare hash from `books_fast_hashes`, never `files.fast_hash`. 33-character keys are legacy `<storageid><hash>`, recognised by `coverUrlForKey()`.
  - `adopt_cover()` copies the firmware's PNG to `OWN_COVER_DIR` as `fw_<hash>.png` on first sight — in the poll path, so no allocation, a no-op once copied.
  - `resolveCoverUrl()`: our cache → extraction (`book_cover.cpp`) → firmware cache, adopted on the way out. Format by content: `META-INF/container.xml` means EPUB (the OPF alone decides); an `.fb2` member means fb2.zip; any other image archive is a comic (first page in natural order). A bare `.fb2` is scanned as bytes. Only the `.fb2` suffix opens a non-archive. PDF, DJVU, MOBI, CBR stay on the firmware cache.

### Device-side constraints

- Cross-compilation constraints (softfp ABI, exact Qt version match, `rcc --no-zstd`) come from fstanis/pocketbook-sdk-qt6 — read its notes before touching the toolchain or CMake.
- **Launcher icons are 106x64 8-bit BMPs**; 106 is the only known-good width. `--canvas`/`--glyph` on `tools/make_icon.py` exist for re-probing. Glyph on a 48-unit grid, stroke width unscaled.
- **The shim is the only way to run at boot-ish time.** `shim.cpp` installs `pbstatistics-open.app` into `/mnt/ext1/system/bin` and puts it *first* in the `system/config/extensions.cfg` app list for every format, keeping the names already there. Formats: `baseFormats()` (epub, fb2, pdf, always) plus every extension in `otherFormats()` one of the two tables already routes somewhere. The script starts the daemon and `exec`s the next reader, the user's table first. It must read the pidfile in the shell, delay the daemon start, and tolerate failure everywhere above the handover.
- **`installed()` is all-or-nothing** (our name in every format's entry). `refresh()` updates script and entries at startup, but only where an entry already names us. `enableByDefault()` installs the shim once ever on first run; its marker `system/pocketbook-statistics/shim-default` is written *before* the attempt.
- `installer.cpp` adds `U_pocketbook_statistics` to `system/config/desktop/view.json` after backing it up. Idempotent and failure-tolerant: the app must start if the file is missing or read-only.
- The app was ReadTrack until v0.10.6; no current build migrates `system/readtrack` or a `U_readtrack` tile.
- **The network is used only for the update check**, only when the user presses the button: `api.github.com` and the release's `PocketBookStatistics.zip`. No background check; reading data never leaves the device.
- **Firmware network API:** use the synchronous `QuickDownloadExt3`, never `NewSession`/`DownloadTo`; never read `iv_sessioninfo`; redirects are followed for us. Timeouts: version check `kCheckTimeoutSeconds` (20) with no retry, download `kDownloadTimeoutSeconds` (60) with one retry. `QueryNetwork()` is not a connection test — always call `NetConnectSilent(NULL)`.
- **Every InkView function is resolved with `dlsym` via `resolve<Fn>()`**, which logs and falls back. Only `InitInkview`, `ScreenWidth`, `ScreenHeight` and `PanelHeight` are linked.
- **`app.log` is the only debugger.** `src/log.c` is the one writer (`update_log.cpp` bridges to it); each line is opened, written, closed; the file is halved at 64 KB. `pb_log_install_crash_handler()` writes `killed by signal N during <stage>` with `open`/`write` only; the stage is the last `mark()`. The start block names model, hardware, firmware and screen. The About tab shows the tail only after an update `error`. Log what would matter a week later — starts, stops, refusals, failures — not normal work. The heartbeat's `M with reading` counts only credited windows (`tracker_observe()` returning 2), so `0 with reading` does not mean nothing was recorded.

### Before pushing

Read the diff back as a reviewer would — the lines, not the intent. Each of these shipped broken once (details in `docs/DECISIONS.md`):

- Check the change against the invariants above.
- Ask whether it can reach the device at all: installed files (shim script, `extensions.cfg` entries) change only if `refresh()` rewrites them.
- Do the layout arithmetic instead of eyeballing it (`Global.dp(1)` ≈ 1.32 px).
- Verify assumptions about the outside world (GitHub's sort order, firmware behaviour).
- Run `make test`, and `cc -c` any C file the host tests do not compile.
- A change that reaches users gets a line in `docs/CHANGELOG.md` under `Unreleased`: what changed and why, with the issue/PR number. A new or changed rule goes here, its story into `docs/DECISIONS.md`.

### Conventions

- A new file in `qt/src/` goes into **both** `CMakeLists.txt` and `test/CMakeLists.txt`. New QML files go into **both** `qt/qml/pocketbook-statistics.qrc` and the parent tab.
- User-facing strings go through `Tr.t("<key>")` against the 29 catalogs in `qt/qml/i18n/`; `Tr.plural("plural.<noun>", n)` for counts. `tools/check_i18n.mjs` fails on a missing key, an unpassed placeholder, a short plural array, or a language missing from one of the four lists. Untranslated keys are recorded in `qt/qml/i18n/untranslated.json` — regenerate it with `node tools/check_i18n.mjs --update-baseline`, never edit by hand. A missing key falls back to English. To add a language: the catalog, `pocketbook-statistics.qrc`, an import and a row in the `catalogs` table in `Tr.qml`, a row in `kLauncherNames` in `installer.cpp`. Each catalog's `plural(n)` decides the form count (Slovenian needs four).
- The launcher tile label is the **one** user-facing string outside the catalogs: `launcherTitle()` in `installer.cpp`, written before a QML engine exists. It names what the app does ("Statistics"), not the app.
- **`GlobalValues.defaultBorderColor` is black on this firmware** — a line colour only. For an empty track or a wash, use `defaultTextColor` at low opacity.
- Sizing and colour come from the firmware theme: `Global.dp(n)`, `GlobalValues.*`, `FontStyles.*`, `StyledText` instead of `Text`. No hardcoded pixels or colours.
- Tabs refresh in `Component.onCompleted`, `onVisibleChanged`, on application activation and once a minute in front (`main.qml`). There is no live binding to the DB. Every `StatsBridge` aggregate calls `catchUp()` first; anything new that reads the stats DB for the UI must too.
- C code is `-Wall -Wextra -std=gnu99`, no allocation in the poll path; comments in English.
- User docs: `README.md` is a short overview linking to `instruction.md`, the full guide. A change users can see goes into `instruction.md`; the README only when the feature list changes.
