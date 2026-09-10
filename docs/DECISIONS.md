# Why things are the way they are

`CLAUDE.md` holds the rules. This file holds what is behind them: the incident
that made a rule necessary, the measurement it rests on, and what was tried
first and did not work. The headings follow `CLAUDE.md`, so the reason for a
rule is under the same heading as the rule.

Read the matching section before changing or relaxing a rule. When a rule is
added or changed, write the rule into `CLAUDE.md` and the story here. What
each release changed is in [CHANGELOG.md](CHANGELOG.md).

## Testing and tooling

### `daemon.c` in the host tests

`daemon.c` is compiled into `make test` for `daemon_alive()`, which decides
whether the day gets measured at all: `pidfile_path()` and `proc_dir_path()`
are overridable by environment variable (`POCKETBOOK_STATISTICS_PIDFILE`,
`POCKETBOOK_STATISTICS_PROC`) exactly so a test can hand it a pidfile and a fake
`/proc` and check what it makes of them.

### Why `make qt-test` behaves differently by hand

`QT_QPA_PLATFORM=offscreen` is set as a property of every test under ctest, and
`tst_qml` is additionally handed the `-import test/qml/stubs` that stands in for
the firmware's QML module. Neither is compiled into the binaries, which is why a
QTest binary run by hand behaves differently from the same test under ctest.

### What the QML suites can and cannot see

There is no host-runnable UI in the sense that matters — nothing here renders
what the reader draws — but the QML does run: `test/qml/stubs/` stands in for
`com.pocketbook.controls` (which exists only in the firmware at `/ebrmain/qml`)
at the metrics measured on a PB629, so `make qt-test` builds every screen,
refreshes it against a real `StatsBridge` over a seeded database, and checks
its arithmetic. That catches a component that fails to instantiate and a row
wider than the screen. What it cannot judge is how any of it *looks*.

Measuring a screenshot is how the firmware's own header button was matched —
38 px glyph, 2 px stroke, 27 px from the edge on a 758x1024 PB629, where
`Global.dp(1)` measures ~1.32 px.

### Startup time: ~3.2 s on a PB629, measured

From `/proc/self/stat` against `/proc/uptime` and marks through `main()`: loader
0.13–0.44 s, InkView screen 0.1 s, launcher registration 0.01 s,
`QGuiApplication` 0.07 s, `engine.load()` returning at ~3.0 s, first frame
~0.2 s later. Every bridge call the tabs make on load is under 100 ms, so the
data is not it. `engine.load()` covers both compiling the QML and building the
object tree, and the disk cache in `main.cpp` (`QML_FORCE_DISK_CACHE`, needed
because QML from a resource is not cached by default) moved nothing — which
leaves the tree: three tabs are constructed at startup, calendar included, with
31 cells and their covers. The lever, if it is ever worth pulling, is building a
tab when it is first shown rather than at launch. `StreakTab` is already built
that way.

### Querying a copy of `explorer-3.db`

The schema is documented in `docs/DEVICE-DATA.md`, but only real data shows
that a query returns 5 books and not 223, or that a ring reads 0 % on a shelf
that has clearly been read. The reader leaves data in `-wal` and `-shm`, and the
`sqlite3` CLI checkpoints on open, which folds the WAL into your copy and
truncates the file — so the second question is asked of a different database
than the first. The WAL holds less than it looks like it might, either way: on a
real device every frame in it was a `sync_accounts` page, so earlier `cpage` /
`position_ts` values are not recoverable from there.

### Why `check_qml.py` exists

It catches what qmllint accepts and the engine then rejects — a duplicated
function or id, or a property named after one the base type already has, makes
the component fail to instantiate, `main.cpp` exits on an empty scene, and the
app simply stops opening with no message anywhere. That last one shipped:
`property var y` on an `Item` is "Cannot override FINAL property", which took
down every file importing that type, and qmllint says it as a warning while only
its exit code gates the build. `rcc` embeds QML verbatim, so nothing but these
checks stands between a QML mistake and a device that will not open the app.

## Architecture

### Bundled SQLite

The device runs the `qt/third_party` amalgamation, the host tests the host's
`-lsqlite3` — different SQLite builds, so a passing test does not prove a
version-specific feature exists on the reader.

### The platform plugin (issue #10, PB634)

Qt's own diagnostics are routed into `app.log` from the first line to the moment
the scene stands, because a plugin that will not load is a `qFatal` in the
`QGuiApplication` constructor and stderr on this device goes nowhere. A build
whose `APP_VERSION` carries a suffix — every `-pr`/`-rc` — also marks each
startup step, so a reader that dies before drawing says where; releases stay
quiet about the steps. What a release does *not* stay quiet about is an anomaly
in how it was launched — an inherited `QT_QPA_PLATFORM` that is not ours, a
`-platform` taken off the command line: `anomaly()` writes those in every build,
because they are printed only when something is wrong and the reader they happen
on belongs to somebody else, whom the alternative asks for a test build to learn
what one line already knows.

A PB634 (issue #10) starts this process with `QT_QPA_PLATFORM=pocketbook`
already in its environment, and Qt 6 there cannot load that one — `pocketbook2`
is the only PocketBook plugin it lists. Setting ours *only when the variable was
empty* left the inherited name standing, and the app died in the
`QGuiApplication` constructor across three builds without a word. It is now set
unconditionally, and after `InitInkview` rather than before, so a firmware that
sets it from inside cannot overwrite ours either. Confirmed fixed on that PB634
(firmware 634.10.3425) in 2.1.1-rc7.

A `-platform` argument outranks the variable, so `dropPlatformArgs()` takes one
out of the array handed to `QGuiApplication` — the name was proved to come from
outside the binary (there is no bare `pocketbook` string in it), and the
environment is only the likeliest of the ways in.

Choosing the name from what `<QT_PLUGIN_PATH>/platforms` lists is *not* the
answer and was tried (2.1.1-pr4): that directory is no inventory of what Qt can
load — it held a name Qt 6 would not load, while the `pocketbook2` it does load
comes from a search path of Qt's own — and on this reader the code never even
ran, because the variable was not empty. It would have done the same to a PB629.

### The bridge

`year()` returns a day as read or not read. The grid carried the firmware's
finish marks as a third symbol until it turned out to say nothing the other two
do not.

`currentBook().filePath` is filled only while the file is still on the device —
`files` is an inventory of what exists right now, and a PocketBook Cloud row
carries no folder at all, so the lookup checks the folder and then the disk.

### The screens

`AboutTab` is not one of the bottom-bar screens because an update check is a
detour rather than a screen. The year grid is split January-June over
July-December because 53 weeks across 758 px leaves an 11 px square.

The streak card opens with the one thing on the About screen that is about the
reading rather than the app — the figure set in the Overview's serif at figure
size while the words around it stay body text. The sentence is split on a marker
passed through `Tr.t`, because word order differs per language and Turkish opens
with the number. Under it sits the Overview's own section heading, a caption over
a rule, so the two screens read as one app. The card is hidden below two days,
because "1 day in a row" is not a run and that screen has no room to spare (it
has painted over the navigation bar once). Today may be empty without breaking
the streak because otherwise a streak would read as broken every morning.

`NavIcon.qml` draws line art on a Canvas because the firmware fonts have no
dingbats to rely on. A card's width is derived, never placed, so a row cannot
come out lopsided.

## The release loop

Every push to a branch used to go out as `vX.Y.Z-rcN`, which was wrong twice
over: most pushes are not worth installing, so the release list filled with
builds nobody meant to try, and calling them candidates said something untrue.
The suffix is `-pr`, not `-rc`: a candidate is aimed at a release, a PR build is
whatever the branch was at when it was asked for (changed in 2.1.0, PR #7).

The workflow stamps the version string into `VERSION` before the build, so the
binary and the tag agree — a build stamped `2.1.0` would outrank every
`2.1.0-prN` after it and refuse the next one.

Builds pile up fast (v1.4.0 took eleven candidates, and that was before the
asking was optional), which is why the `-rc`/`-pr` run is deleted when the final
release goes out.

GitHub returns `/releases` sorted by tag name as text, not by date —
`rc9, rc8 … rc2, rc11, rc10, rc1` — so the updater's old "newest is first" put a
device on rc9 in front of rc9.

A tag that disagrees with `VERSION` makes every installed build offer an update
to the version it already runs — CI fails the tag build for exactly that reason.

## Invariants

### Recovered rows

`recovered = 1` rows are wall-clock spans reconstructed when the daemon wasn't
running, capped at `RECOVERED_CAP_SECONDS`, and carry no `pages_start`. Rows of
estimated reading time were once written at 1–5 hours each; `tracker_init`
rewrites any recovered row over 90 minutes on every open, so 265 h became
172.8 h at the next launch.

### Midnight

All day-level stats `GROUP BY date(end_time,'unixepoch','localtime')`, so one row
is worth one day. The incremental path splits its own gap;
`insert_session_split()` is the same rule for the two paths that place a
session that had already run before anything saw it — the first observation of
a session (the daemon is not scheduled while a book is on screen, so
`catchUp()` is regularly the first to see one at all) and `tracker_recover()`.
Until 1.6.3 neither split: reading 23:30 to 00:03 and then opening the app put
all 33 minutes on the new day and left the previous one empty.

The counted window is `[position_ts - active, position_ts]`, never the row's
full span — a book reopened where it was left starts days before the seconds it
credits — and `active` is capped at `RECOVERED_CAP_SECONDS`, so it can cross at
most one midnight. The first row ends at midnight - 1 while holding the seconds
up to midnight itself, which is the one second of slack in the migration's
clamp. `split_legacy_midnight()` runs once ever (a marker in `meta`, because
`tracker_init` runs on every `catchUp()`) and moves seconds rather than
inventing them.

### `credited()`

A flat cap cannot tell twenty unwatched minutes of reading from one page turned
after an hour away, and paid ten minutes for both. The 300 s per page bound is
generous against the measured pace on a real device, 28-137 s a page.

### `page_jump()`

Books that gather their footnotes at the back are read by tapping a link and
tapping back, so `cpage` lands hundreds of pages away and returns; under
`credited()` that distance vouches for the entire window, and as
`pages_end - pages_start` it went into the pace as pages read — a real device
reported over 7000 pages in a day. The `JUMP_MIN_PAGES` floor is what keeps two
saves seconds apart from disqualifying three genuinely turned pages.
`pages_end` still tracks the real position because the way back would otherwise
be measured from a page nobody is on.

### An unstamped page

On a PB629 a book left at page 177 read back as `books_settings.cpage` = 4 on
the reopen, with the *previous* session's `position_ts` still beside it — most
likely the reflowable EPUB being repaginated, which moves `cpage` without the
firmware saving anything. The zero-second window credited nothing either way,
but the 4 became the baseline, and the real save at page 211 seventeen minutes
later then read as a 207-page jump — the evening was dropped on both sides of
midnight and the app showed 0 minutes. A page the reader genuinely turned back
to before closing came with a save, so its `position_ts` is newer and it counts
as before. Both halves of the condition are needed: a reconstructed row that
ends after the next open must not take over the baseline either.

### `pages_read`

`pages_end - pages_start` are positions and move by navigation. A database from
before 1.6.5 has the column seeded once from the two positions so an existing
pace survives the upgrade; a backfill that ran again would hand every window
that was all jump its distance back on the next open.

### Switched off inside a book

The firmware saves the position on the way down, then stamps a fresh `opentime`
on the way up: `position_ts < opentime`. `tracker_observe()` clamps that away (a
row must not end before it begins), which used to drop the whole stretch since
the previous row's last observation — measured on a real device as 14 minutes
and 10 turned pages arriving as a day with no reading at all, the pages inside
the new zero-second window reading as a jump on top (fixed in 2.1.0).

### A live pid is not our pid

The pidfile survives a reboot and pids come from a small range, so
`kill(pid, 0)` alone called the daemon alive when the number belonged to some
firmware process: nothing started a daemon, and the day was measured only while
the app happened to be open. An unreadable cmdline counts as gone: not starting
a daemon costs a day, a second one costs a poll loop and cannot double-count.

### Resuming a session

Every caller builds a fresh tracker sooner or later — the daemon after a
restart, and `StatsBridge::catchUp()` on *every* refresh — and until 1.6.2 the
stretch since the last observation was not capped but dropped: 18 minutes and
30 turned pages reached the screen as "1 minute". Resuming is also what makes a
dead daemon survivable: opening the app credits the whole stretch, split at
midnight like any other.

### Two trackers, one row

The daemon's tracker lives across polls; `catchUp()` builds a fresh one on every
refresh, and the app's minute timer keeps firing behind the reader because
`Qt.application.state` never leaves `Active` on this firmware. Until 1.6.2-rc4
each measured the gap from its own last-seen `position_ts` and both billed it:
three sessions in one evening carried exactly twice the wall clock they spanned,
and a 92-minute afternoon reported 106.

### `tracker_recover()` and the open session

It runs at daemon start, and the shim starts the daemon seconds after a book is
opened; whether a page had been turned by then decided whether the evening was
stamped `recovered = 1` and dropped from every average. `books_settings` keeps
only the latest open per book, which makes the newest of them the only one that
can still be running.

### Polls are not a clock

The CPU suspends between page turns: a 20-minute stretch got 9 of the 41 polls
it was owed, an 18-minute one got 3. 1.6.2-rc2 credited the poll interval itself
whenever the firmware's reader had a process, on the theory that a book on
screen is a book being read, and recorded a 45-minute evening as 9 minutes. The
premise was wrong in the other direction too: `eink-reader` stays resident after
the book is closed, so a tick landed on a ten-hour-old session while the user
was in this app.

`position_ts` is stamped whether or not our process has a processor, but it is
**not** a page turn. The firmware writes it when the reader saves state, which
on a PB629 measured out at once every 15 to 75 minutes: 64 pages across an
evening arrived as five observations. So a single window between two saves
routinely holds both the reading and the hours the reader lay closed, and no
threshold on `position_ts` alone can separate them. The daemon's heartbeat
prints polls against wall seconds *and* awake seconds, so the duty cycle stays
on record.

### Sleep

The e-ink reader also drops the CPU between renders while a book is genuinely
being read: the poll intervals measured mid-reading run 2-7 minutes of wall
clock at a 7-22 % duty cycle, and a closed cover runs far longer — hence the
15-minute threshold. `StatsBridge::catchUp()` looks hours after the last mark,
and a difference measured across an afternoon says nothing about where inside
it the device slept, which is why only marks one poll apart are classified.
Without this, an evening of 18:00 to 21:25 with 64 pages billed 3 h 15 m: five
windows, each under its own page budget, each mostly a shut cover.

### Manual totals (issue #1)

Reading done before the app was installed, or on another reader, is invisible
to it. What is stored is the difference from the measured figure, which is why
later reading still adds to the total. Writing it into `sessions` would move the
pace, today's minutes, the calendar, the day panel and the streak. A long press
rather than a tap, because these cards are read far more often than they are
edited; steps rather than a keyboard, because the firmware's numeric input is
not something a Qt scene can raise. The panel writes on OK only: a panel opened
out of curiosity must not change the number, which is what writing on every
step did.

### Measurement, not inference

The Overview once carried a ring for "how much of the shelf on this device have
I read", derived from `files` joined to `books_settings`; it was removed in
1.6.0 along with the query behind it — the only thing that screen asked the
firmware library. The finished count is all-time, from `completed_ts`, and
includes books long deleted, while anything about the current shelf would drop
as soon as new books are copied on (26 finished ever ÷ 5 books present is how a
ratio once reported 100 %).

### Deduplication

`books_impl` keeps stale rows and the same book exists as several file copies.

### `tracking_since`

Sessions before it only exist because `tracker_recover()` reconstructed them
from the firmware's last-open timestamps, and they are guesses, not
measurement. The firmware dates a finish without saying how long that day's
reading was, which is why finish marks on untracked days carry `secs = 0`. A
blank day must say why it is blank, or an empty month reads as a bug rather than
as "not recorded yet".

### The updater

`mv` is only atomic within one filesystem, and `/mnt/ext1` is not the same one
as `/tmp`. The swap script kills the daemon because it runs from the same ELF.
InkView must not be driven from a second thread, which is why the update path
runs on the GUI thread and repaints before each blocking call.

### Covers

Most finished books get deleted, and both the file and the firmware's cache
entry go with them — so a thumbnail only exists later if it was copied while
the book was still there. `files` lists only what is on disk (5 of 223 books on
the reference device), so keying off `files.fast_hash` loses the cover for every
deleted book. Rows written before v0.7 hold `<storageid><hash>` — 33 characters
instead of 32.

Extraction wins over the firmware cache because the cache holds ad pages instead
of the cover for some Calibre books; our cache wins over both because it is the
only one that survives. Inside an EPUB the OPF alone decides, because guessing
at an image finds chapter illustrations. A bare `.fb2` is scanned as bytes
because half of them are windows-1251 and Qt 6 has no codec for that, while the
base64 in `<binary>` is ASCII either way. Only the `.fb2` suffix opens a
non-archive: reading every 20 MB PDF into RAM on each refresh, to look for XML
that is not there, costs as much as an extraction and never finds one.

## Device-side constraints

### Launcher icons

The launcher draws a tile as image-then-label at the bitmap's native size, so
the canvas height decides where the label sits — a 128 px canvas drops the label
below the row of labels beside it. Width is stranger: 106 renders, 48 makes the
icon vanish entirely (the tile shows no image at all and the label slides up
into the empty space), and why has never been established. Both sizes were
confirmed on a PB629. The stroke is left unscaled so the line art stays as thin
as the firmware's own icons (~50 px glyphs) instead of reading as heavier than
its neighbours.

### The shim

The firmware's autostart lives in `/ebrmain`, which is the firmware partition —
out of reach and not worth touching. The `extensions.cfg` door is the one
KOReader uses too. Intersecting a generous format list with the device's own
table is what bounds the damage: an extension no firmware here knows about costs
nothing, an entry is never fabricated for a format nothing could open anyway,
and formats routed to something that is not a reader (music, images, fonts,
firmware images) are not in the list at all. The user's table is read first
because it is the one `install()` edited, and the only one that can answer for a
format the firmware's own does not name.

The pidfile is read in the shell because starting the app to ask costs a full Qt
load and delayed the book by five seconds. The daemon start is delayed so it
does not fight the reader for the flash. A shim that does not reach its last
line is a book that will not open.

`installed()` demands our name in the entry of *every* format, so a build that
intercepts more formats than the one which installed the shim made the switch go
"off" for a change the user never made (2.1.0). The shim goes in on the first run
because without it the day is measured only while the app happens to be open,
and someone who installs a reading tracker and sees an empty evening has no way
of guessing there was a switch. The `shim-default` marker is written before the
attempt so a failure is not retried at every launch and a later switch-off is not
overruled at the next start.

### The ReadTrack rename

The rename (v0.11.0) moved the binary, the data directory, the release asset and
the launcher entry; v0.11.0 shipped a one-shot migration for them, and v0.11.1
deleted it — the app had one user, who had already moved across.

### The firmware's network API

All of these were paid for on a PB629:

- The session API (`NewSession`/`DownloadTo`) returns `NET_OK` and writes a
  zero-byte file: the transfer is handed to the event loop InkView runs for its
  own applications, and ours is Qt's.
- Reading `iv_sessioninfo->response` yielded 1 and 0 where an HTTP status
  belonged and killed the process.
- A release asset URL bounces to a CDN host, and `QuickDownloadExt3` lands the
  final body — confirmed with the 599 KB zip.
- Associated-but-offline — a captive portal, a router with no uplink — passes
  the connection test and then runs into the timeout, and with a retry the
  version check used to freeze the screen for two minutes to say what one
  attempt says.
- `QueryNetwork()` returning `0x202` (`NET_WIFI|NET_WIFIREADY`) means the radio
  is up and answered a scan — a download in that state comes back empty.

### `dlsym`

`inkview.h` is the SDK's, `libinkview.so` is the device's, and a function present
in the header but missing from the library is a lazily-bound symbol that kills
the process on first call — with no console to say why. `iv_get_default_font` and
`currentLang` were linked until a PB634 (issue #10) died in startup with a log
that could not say why. Neither turned out to be that device's fault (its log
shows the app past `ensureRegistered()`, so `currentLang` is there), but a linked
call cannot report itself, and these two are made in the stretch where nothing
has drawn yet.

### `app.log`

Every line is opened, written and closed on its own so a crash cannot swallow
the last one. The crash handler uses nothing but `open`/`write` because a signal
handler may call neither stdio nor anything that takes a timestamp. The daemon
sets its own stage, which is what finally told its long-suspected SIGKILL apart
from a crash. The About tab shows the tail only after an error because the
screen does not scroll, so anything printed under a successful check falls off
the bottom unseen.

## Before pushing — what shipped broken

- The shim script is copied to `system/bin` at install and nothing rewrote it
  afterwards, so two releases of fixes to it never left the repository. Then the
  refresh that fixed *that* keyed off `installed()`, which demands an entry per
  format — skipping exactly the hand-set-up device it was written for. And a
  third turn of the same screw: widening the format list made `installed()`
  false on a device that had the shim for three, so the switch read "off" after
  an update.
- A spacer was added beside a Row that already applies `spacing`, and the
  figures row came out 21 px wider than the screen; the label lost its last
  letter.
- The updater assumed `/releases` was sorted by date (see *The release loop*).
- Recovered rows written at 1–5 hours (see *Recovered rows*).

## Conventions

- `GlobalValues.defaultBorderColor` is black on this firmware: filling a shape
  with it and then drawing on top in `defaultTextColor` produces a black
  rectangle, which is how the progress bar and the "ALL BOOKS" heading both
  disappeared.
- The launcher label is "Statistics"/"Статистика"/"Statistik", not the app
  name, because it sits among firmware tiles that are named for what they do.
- Returning from the reader does not trigger `Component.onCompleted` or
  `onVisibleChanged` — the window never stopped being visible — hence the extra
  refresh on application activation. `catchUp()` exists because the daemon's
  30 s poll is too coarse for "close the book, open the app", and a daemon killed
  while the reader had the foreground would otherwise leave that session
  unwritten until the next launch.
- A new file left out of `test/CMakeLists.txt` is in nothing `make qt-test`
  builds: every test that touches it fails to link, and any test that does not
  simply never covers it.
