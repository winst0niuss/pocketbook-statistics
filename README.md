# PocketBook Statistics

Reading statistics for stock PocketBook e-readers.

Your reader already knows what you read and when. This app shows it: how long
you read today, how fast you read, what you read each day of the month, and how
many days in a row you have been reading. It is drawn with the reader's own
interface parts, so it looks like a built-in screen.

<p align="center">
  <img src="docs/screenshots/overview.png" alt="Overview: current book, today, all books" width="300">
</p>

<p align="center"><sub>The main screen on a PB629. The app follows the language
of your device.</sub></p>

The full guide to every screen and setting is in
**[instruction.md](instruction.md)**.

## Main screen

- **Current book.** Cover, how far you are, and how much time is left at your
  own reading speed. Tap the cover or the title to open the book.
- **Today.** Minutes read and pages per hour.
- **All time.** Books finished and hours read.

## Calendar

<p align="center">
  <img src="docs/screenshots/calendar.png" alt="Calendar: a month of covers" width="245">
  &nbsp;&nbsp;&nbsp;
  <img src="docs/screenshots/day.png" alt="One day: what was read and for how long" width="245">
</p>

One month per screen, each day showing the cover of the book you read most; a
badge means more than one book that day. Tap a day to see the books and the time
for each one.

## Reading streak

<p align="center">
  <img src="docs/screenshots/info.png" alt="The info screen, opening with the streak" width="245">
  &nbsp;&nbsp;&nbsp;
  <img src="docs/screenshots/streak.png" alt="The year as a grid of days, with the current and best streak" width="245">
</p>

The ⓘ screen starts with how many days in a row you have read. Tap that card to
see the whole year as a grid of days, with your current and longest streak. Days
before you installed the app stay empty: there was nothing to measure them with.

## Editing the totals

The app counts only what it has seen on this reader. To add reading done before
you installed it, or on another device, press and hold **books finished** or
**total hours**. New reading is still added on top, and speed, calendar and
streak stay as measured.

## What is counted as reading

- Only plausible time counts: a gap between two saved positions is worth no more
  than the turned pages justify, and the time the device slept is not counted.
- Jumps to a footnote or a bookmark give no time and no pages.
- Every day keeps its own time: a session never crosses midnight.
- **Finished** comes from the reader's own *mark as read* flag, so it matches
  your Library.

Details are in the [guide](instruction.md#what-is-counted-as-reading).

## Install

1. Download the `.zip` from the [latest release](../../releases/latest) and
   unpack it.
2. Copy `PocketBookStatistics.app` to the `applications/` folder on the reader
   over USB.
3. Eject the reader, open the app once, then reboot it so the icon appears in
   the menu.

Updates come over Wi-Fi: ⓘ → *Check for update*. The firmware does not let apps
start at boot, so keep **Start statistics when a book opens** on (ⓘ screen).
Uninstalling is described in the [guide](instruction.md#install-update-uninstall).

## License

[MIT](LICENSE).
