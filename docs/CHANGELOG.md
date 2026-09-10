# Changelog

What each release changed, and why. Newest first. Pre-release builds (`-rc`,
`-pr`) are not listed on their own: what they fixed is listed under the release
they led to, together with what was tried there and dropped. The reasoning
behind the rules the code keeps is in [DECISIONS.md](DECISIONS.md).

## Unreleased

- **Docs.** The README is now a short overview; the full guide moved to
  `instruction.md`. Why: the README had grown into a manual, and most readers
  only need to know what the app does and how to install it.
- **Docs.** `CLAUDE.md` reduced to rules; the stories behind them moved to
  `docs/DECISIONS.md`, and this changelog was started.

## 2.1.1 — 2026-09-09

**The app starts on a PocketBook Verse Pro (PB634)** (issue #10, PR #11).

- **Fix: the platform plugin is named on every launch.** That reader starts the
  app with `QT_QPA_PLATFORM=pocketbook` already set, a plugin its Qt 6 cannot
  load. The app set its own name only when the variable was empty, so it died
  inside the `QGuiApplication` constructor: the icon was there, tapping it went
  back to the home screen, and nothing was written anywhere. Now `pocketbook2`
  is set unconditionally, after `InitInkview`, and a `-platform` argument is
  removed from the command line.
- **Fix: `iv_get_default_font` and `currentLang` are resolved at runtime.** Why:
  a firmware missing either would have killed the app on the first call with no
  trace; now it loses a font name, not the app.
- **Diagnostics for readers nobody here owns.** The first log lines — version,
  runtime Qt, model, firmware, screen — are written before any Qt call. Qt's own
  messages go into `app.log` during startup. A death by signal writes
  `killed by signal N during <stage>`, for the app and the daemon. A launch that
  had to override an inherited platform name says so in every build. Why: stderr
  goes nowhere on the device, which is why the failure above stayed silent for
  three builds.
- The locale warning Qt prints on every launch no longer reaches the log.

Tried and dropped: choosing the plugin from what the plugin directory lists
(2.1.1-pr4). That listing is not an inventory of what Qt can load, and it would
have broken the PB629 too.

## 2.1.0 — 2026-09-07

- **Fix: the session the reader was switched off in is counted** (PR #6). Why:
  twenty minutes of reading, then power off and on inside the book, showed as a
  day with no reading. The firmware saves the position before the next open, and
  the stretch was dropped.
- **Fix: a stale pid no longer passes for a running daemon.** Why: the pidfile
  survives a reboot, the pid got reused by a firmware process, and the daemon was
  never started — the day was measured only while the app was open.
- **Fix: a reopened book keeps its last saved page as the starting point.** Why:
  a repaginated EPUB reported page 4 instead of 177, the next save read as a
  207-page jump, and the evening showed 0 minutes.
- **Fix: screens no longer show zeros when the stats database cannot be
  opened.** Why: zeros read as "you have not read anything".
- **Tracking starts for every format the reader opens** (PR #9), not only EPUB,
  FB2 and PDF. Why: a DJVU, MOBI or CBZ opened without the daemon behind it, and
  the day went unmeasured.
- **Autostart is on by default, and stays on after updates.** Why: without it
  the day was measured only while the app was open, and nobody could guess there
  was a switch; widening the format list also made the switch read "off" after
  an update.
- The daemon records its runs in the database, and the next start logs how the
  previous one ended. Why: it is killed with SIGKILL often enough that a line on
  the way out cannot be relied on.
- The streak legend is centred under the grid; three Russian strings reworded.
- **Build.** A pull request publishes a device build only when asked for
  (`[device]` in the commit message, or a `VERSION` change), as `-prN` (PR #7).
  Why: every push used to become a "release candidate" nobody meant to try.
  Nothing is published over a failing test.
- **Tests.** The 29 catalogs are checked (`check_i18n.mjs`), and the Qt half and
  the screens are tested on the host (`make qt-test`, PR #8). A flaky sleep test
  fixed (PR #5). The README asks for issue reports (PR #4).

## 2.0.1 — 2026-09-04

- **The all-time totals can be corrected by hand** (issue #1, PR #2): press and
  hold *books finished* or *total hours*. Why: reading done before the app was
  installed, or on another device, had nowhere to go. The correction is stored as
  a difference from the measurement, so new reading still adds to it, and speed,
  calendar and streak stay measured.
- The panel saves on OK; X and the backdrop leave the figure as it was. Why: a
  panel opened out of curiosity must not change the number.
- A clearer info screen: version next to the name, the update button on its own
  line, a heading for the autostart switch.
- The README rewritten in plain language, with fresh screenshots (PR #3).

## 2.0.0 — 2026-09-04

First release of this repository; the history before it was not kept.

- Overview: the current book, today's minutes and pace, all-time books and
  hours. Calendar of covers by day. Reading streak and the year as a grid.
- Measurement rules: time is credited only as far as turned pages make it
  plausible; footnote and bookmark jumps are navigation; device sleep is
  subtracted; sessions never cross midnight; reconstructed time is an estimate
  kept out of averages; nothing before the first run is shown as history.
- Covers extracted from EPUB, FB2 and CBZ and kept after the book is deleted.
- 29 interface languages. Network only for the update check, only to
  `api.github.com`.

## Before 2.0.0

Reconstructed from notes that survived the squashed history; incomplete and
undated.

- **1.6.5** — pace is built on a new `pages_read` column instead of the distance
  between positions. Why: footnote jumps were counted as pages read — a real
  device reported over 7000 pages in a day.
- **1.6.3** — a session written in one piece is also split at midnight, and old
  rows were split once. Why: reading 23:30–00:03 put all 33 minutes on the new
  day.
- **1.6.2** — a fresh tracker resumes the open session instead of dropping the
  stretch since the last observation (18 minutes showed as 1). Two trackers no
  longer bill the same stretch twice (fixed in rc4). Tried and dropped in rc2:
  counting poll intervals as reading — a 45-minute evening came out as 9.
- **1.6.0** — the "share of the shelf read" ring removed from the Overview. Why:
  it mixed an all-time count with the current shelf and once reported 100 %.
- **1.4.0** — release candidates ship as `-rc` pre-releases to one reader
  carrying a `prerelease` marker. That release took eleven candidates.
- **0.11.1** — the ReadTrack migration removed.
- **0.11.0** — renamed from ReadTrack to PocketBook Statistics, with a one-shot
  migration of the binary, data directory and launcher entry.
- **0.7** — covers keyed by the bare file hash from `books_fast_hashes`, so a
  deleted book keeps its cover.
