# Statistics, redesigned

A plan for what this app should show and why. Written against what a PocketBook
actually records (`DEVICE-DATA.md`) and what an e-ink screen can actually do —
every metric below names its source, and the ones that cannot be built are
listed too, so nobody re-proposes them.

## What the references teach

| Source | Worth stealing | Worth leaving |
|---|---|---|
| **KOReader statistics** | Per-hour histogram under each calendar day — "when did this book keep me up". Time per page as the honest speed metric. Per-book totals. | The seven-screen menu tree. Numbers to two decimals. |
| **Kobo Reading Life** | Time-of-day reading pattern. Sessions as a first-class unit. | Awards and badges: gamification ages badly and needs a social loop we don't have. |
| **Goodreads Year in Books** | The annual retrospective as a *narrative*: longest book, fastest read, genre mix, one number that matters. | Ratings, shelves, challenges — we have no ratings and no network. |
| **What none of them do well** | — | Covers as the primary visual. We already have them, and on a 6" mono screen a wall of covers beats any chart. |

## Principles

1. **One question per screen.** A tab answers one thing. If a number needs a
   paragraph, it belongs in a dialog behind a tap, not on the tab.
2. **No scrolling.** e-ink scrolling is unpleasant and slow. Everything fits
   1404×1872 (or 758×1024 at 1x) or it doesn't ship.
3. **Measurement and estimate never share a number.** Reconstructed spans
   (`recovered = 1`) and firmware-dated finishes are not the same thing as
   tracked reading; see the invariants in `CLAUDE.md`.
4. **Unknown is a state, not a zero.** A day before install is blank-with-reason,
   never "read nothing".
5. **Covers first, numbers second.** The picture is what the reader recognises.
6. **Every screen readable at arm's length.** One dominant number per block,
   caption beneath, no legend the reader has to decode.

## Screens

**Superseded, 2026-08-21.** The five-tab version below was built and then cut
back to two screens plus info, modelled on Kobo's reading-stats page: everything
that matters at a glance, and nothing that needs explaining. What shipped is
*Overview* (current book, pace, library donut) and *Calendar* (a month of
covers). The tabs described here — Habit, Books, Year — were removed; their code
is in the history if any of it is wanted back.

Five tabs plus the info glyph. Current tabs in brackets.

### 1 · Now  [Overview]

*Question: what am I reading and how is it going?*

```
┌──────────────────────────────────────────────┐
│ ┌────────┐  ПЫЛЬ                             │
│ │        │  Хью Хауи                         │
│ │ cover  │  ▓▓▓▓▓▓▓▓░░░░░░░░  47 %           │
│ │        │  осталось ≈ 6 ч 40 мин            │
│ └────────┘  начата 12 дней назад             │
├──────────────────────────────────────────────┤
│    47 мин        3          22 стр/ч         │
│   сегодня     сессии       ваш темп          │
├──────────────────────────────────────────────┤
│  ▁▂▅▇▃▁▂  последние 7 дней · всего 4 ч 10 м  │
└──────────────────────────────────────────────┘
```

- **Time left** — `(npage − cpage) ÷ personal pages-per-hour`, from tracked
  sessions on this book, falling back to the all-books average. Never shown for
  a percentage-paginated book (`npage = 100`); those get "≈ 53 % осталось".
- **Started N days ago** — first session for the book.
- **Sessions today** — count of `recovered = 0` sessions.
- **Pages per hour**, not per minute: 0.1 pages/min reads as broken; 6 pages/h
  reads as a fact. Excludes percentage-paginated books.
- **Week sparkline** — seven bars, tracked minutes per day.

### 2 · Habit  [Streak]

*Question: am I reading regularly, and when?*

```
┌──────────────────────────────────────────────┐
│  12        34        86 %                    │
│  дней      рекорд    дней с чтением          │
│  подряд    2026      за месяц                │
├──────────────────────────────────────────────┤
│  ▪▪▫▪▪▪▪ ▪▫▫▪▪▪▪ ▪▪▪▪▫▫▪ …   год             │
│  (heatmap, finished books marked)            │
├──────────────────────────────────────────────┤
│  когда вы читаете                            │
│  ▁▁▁▂▃▂▁▁▁▁▂▃▅▇▇▅▃▂▂▃▅▇▆▂  0–24 ч            │
│  чаще всего: 22:00–23:00                     │
└──────────────────────────────────────────────┘
```

- **Hour histogram** is new and cheap: bucket `sessions` by
  `strftime('%H', end_time, 'localtime')`. This is the Kobo/KOReader idea worth
  copying, and the one thing our data supports perfectly.
- **Days-with-reading share** counts only tracked days, so it starts at 100 %
  and settles — better than a streak that resets to zero on one busy day.

### 3 · Calendar  [Calendar]

*Question: what did I read on a given day?* Largely as it is today: month grid,
cover of the day's most-read book, finished books marked even before tracking
began. Add:

- a per-day minute bar under the cover, so a glance separates 10 minutes from
  two hours (KOReader's histogram idea, at day resolution);
- month total in the header: `Июль · 14 дней · 9 ч 20 мин`.

### 4 · Library  [new]

*Question: what do I have, and what have I done with it?*

```
┌──────────────────────────────────────────────┐
│  ◕ 25 %      5 / 20        3                 │
│  прочитано   книг на       брошено           │
│  из них      устройстве    (нет прогресса    │
│                             > 60 дней)       │
├──────────────────────────────────────────────┤
│  [cover] [cover] [cover] [cover] [cover]     │
│   47 %    12 %   готово   3 %     не начата  │
└──────────────────────────────────────────────┘
```

- Answers the question the donut is *for*: how much of what I own have I read.
  Both halves from the current library (`files`), never mixed with the all-time
  finished count — the mistake that once produced "100 % finished".
- **Abandoned** — opened, `position_ts` older than 60 days, not finished. A real
  category no reference shows, and the most actionable number here.

### 5 · Year  [Year]

*Question: what kind of year was it?* Goodreads' retrospective, built from data
we already hold.

```
┌──────────────────────────────────────────────┐
│  18 книг · 5 240 страниц · 92 часа           │
├──────────────────────────────────────────────┤
│  янв [▪][▪]      май [▪][▪][▪]               │
│  фев [▪]         июн                         │
│  …               (covers per month)          │
├──────────────────────────────────────────────┤
│  самая длинная   самая быстрая   жанры       │
│  Задача трёх тел  Лето, прощай   фант. 9     │
│  410 стр          2 дня           детект. 4  │
└──────────────────────────────────────────────┘
```

- **Genres / languages / decades** come from `books.db` tags — thin coverage
  (10 of 85 books have a genre here), so render only what is known and never
  show a zero bucket.
- **Longest / fastest** are per-book records over finished books; fastest needs
  tracked sessions, so it only appears once a year of tracking exists.

## Ranked backlog

| # | Item | Source | Effort |
|---|---|---|---|
| 1 | Pages-per-hour instead of per-minute | existing | XS |
| 2 | Hour-of-day histogram | `sessions.end_time` | S |
| 3 | Week sparkline on Now | `sessions` | S |
| 4 | Library tab with abandoned count | `files` + `books_settings` | M |
| 5 | Per-day minute bar in calendar | `sessions` | S |
| 6 | Year: pages + hours totals | `npage`, `sessions` | S |
| 7 | Year: longest / fastest records | `npage` + `sessions` | M |
| 8 | Genres / languages / decades | `books.db` tags | M |
| 9 | Bookmarks & highlights count | `books.db` `obj.book_mark` | M |

## Deliberately not building

- **Awards and badges.** Kobo's work because a store sits behind them.
- **Word counts and "reading age".** The firmware has no word counts; deriving
  them from file size is a guess dressed as a statistic.
- **Charts with two axes, pies beyond the one donut, anything with a legend.**
  Mono e-ink has no colour to separate series.
- **Comparisons to other readers.** No network, by design.
- **Rebuilding history before install.** Reconstructions stay out of history;
  only firmware-dated finishes and bookmark dates predate tracking.

## Data notes that constrain the design

- **Pages are unreliable for ~25 % of books** (`npage = 100` means percent, not
  pages). Any page-based number must exclude them and say "≈".
- **Reading time exists only after install.** Every time-based screen needs an
  empty state that explains itself rather than showing zeros.
- **Finished books are usually deleted**, so library-based and history-based
  counts legitimately disagree; each screen must say which set it means.
- **Covers survive deletion** via our own cache — the reason a cover-first
  design works at all here.
