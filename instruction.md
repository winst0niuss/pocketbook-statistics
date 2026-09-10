# PocketBook Statistics — user guide

The short version is in the [README](README.md). This page explains each screen
and setting in detail.

## Main screen

Three blocks, top to bottom:

- **Current book.** Cover, how far you are, and how much time is left at your
  own reading speed.
- **Today.** Minutes read and pages per hour.
- **All time.** Books finished and hours read.

Tap the cover or the title to open the book. Close the book and you are back
here.

## Calendar

One month per screen. Each day shows the cover of the book you read most that
day. A badge means you read more than one book that day. At the top: how many
days you read this month and how much time it took.

Tap a day to see the books and the time for each one. Tap a book to see its
figures.

## Reading streak

The ⓘ screen starts with your streak: how many days in a row you have read. Tap
that card to see the whole year:

- your current streak and the longest one this year,
- how many days you read this year,
- every day of the year as a square, like a contribution graph. Days go down,
  weeks go to the right. January to June on top, July to December below.

Days before you installed the app are empty squares. There was nothing to
measure them with yet.

## Editing the totals

<p align="center">
  <img src="docs/screenshots/edit-books.png" alt="Editing the number of books finished" width="245">
  &nbsp;&nbsp;&nbsp;
  <img src="docs/screenshots/edit-hours.png" alt="Editing the total hours read" width="245">
</p>

The totals count only what this app has seen on this reader. If you read for
years before you installed it, or on another device, you can add that time
yourself.

Press and hold **books finished** or **total hours**, set the number with the
buttons, and press OK. X closes the panel and changes nothing.

Your correction is saved separately from the measured data. New reading is still
added on top of it, and nothing else changes: reading speed, calendar and streak
stay as they were measured.

## Start tracking when a book opens

The app cannot start by itself when the reader boots, because the firmware does
not allow that. There is a switch for it on the ⓘ screen instead: **Start
statistics when a book opens**.

With it on, opening an EPUB, FB2 or PDF starts the tracking and then hands the
book to your usual reader app. With it off, just open the app once after you
switch the reader on.

## What is counted as reading

- The app checks your reading position every 30 seconds and turns the changes
  into reading sessions.
- Only plausible time counts. Your reader saves the position rarely, so one gap
  can hold both reading and hours with a closed cover. The app pays for such a
  gap no more than the turned pages justify, at most five minutes per page, and
  the time the device slept is not counted at all.
- Jumps are not reading. Following a footnote to the end of the book or going
  back to a bookmark gives no time and no pages. What you read after the jump is
  counted normally.
- A session never crosses midnight, so every day keeps its own time.
- If the app was not running for a while, it restores that time from the
  reader's own timestamps. Such time is an estimate: it counts in the totals,
  but never in the averages.
- **Finished** always comes from the reader's own *mark as read* flag, so the
  number matches your Library.
- Covers are taken from the book file (EPUB, FB2, CBZ) and kept in the app's
  cache, so a book keeps its cover even after you delete the file.

## Privacy

Nothing is uploaded. No account, no telemetry. The app reads the reader's
database and writes its own files in `system/pocketbook-statistics/`. It goes
online only when you press *Check for update*, and only to `api.github.com`.

## Supported devices

- **PocketBook Verse (PB629)**, firmware U629.6.10.1461 — the reader this is
  developed on.
- **PocketBook Verse Pro (PB634)**, firmware 634.10.3425 — working since 2.1.1.

Other Allwinner **B288/B300** readers with Qt 6.8 firmware should work, but
nobody has tried yet. The app uses the Qt already on the reader, so a firmware
with another Qt version will not run it.

## Install, update, uninstall

1. Download the `.zip` from the [latest release](../../releases/latest) and
   unpack it.
2. Copy `PocketBookStatistics.app` to the `applications/` folder on the reader
   over USB.
3. Eject the reader, open the app once, then reboot it so the icon appears in
   the menu.

After that, updates come over Wi-Fi: ⓘ → *Check for update*.

To uninstall: delete the `.app`, restore `view.json` from the backup next to it,
and delete the `system/pocketbook-statistics/` folder.

## Reporting a problem

I build this app on the only reader I own, a PB629, so your report is the only
way I learn how it behaves elsewhere. Please [open an issue](../../issues) and
attach:

- the log from `system/pocketbook-statistics/app.log`, where the app and the
  service write one line per event,
- a screenshot or a photo of what you see,
- your reader model and firmware version.

After a failed update the ⓘ screen shows the last lines of the log. Otherwise
copy the file over USB.

I read every issue and I am grateful for each one.

## Build

One ARM binary that is both the app and its background service, linked against
the Qt 6.8.2 already on the reader. Run `make sdk` once, then `make qt`;
`make test` runs the tests on your computer. More in [BUILDING.md](BUILDING.md)
and [docs/DEVICE-DATA.md](docs/DEVICE-DATA.md).

Bundles [SQLite](https://www.sqlite.org/) and
[miniz](https://github.com/richgel999/miniz); cross-compiled with
[fstanis/pocketbook-sdk-qt6](https://github.com/fstanis/pocketbook-sdk-qt6).

## Disclaimer

Not affiliated with PocketBook. Use at your own risk. The app only reads the
firmware database. Its optional configuration edits are backed up, but you are
installing third-party software on your device.
