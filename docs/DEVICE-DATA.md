# What the device already knows

Everything the PB629 firmware stores that a reading-stats app could use, and —
just as important — what it does not store. Surveyed on a **PocketBook Verse (PB629),
firmware U629.6.10.1461**, from the USB-mounted `/mnt/ext1` with a
library of 223 indexed files and 84 opened books.

Read this before inventing a new metric: half the things that look like they
need our own tracking are already sitting in a firmware database, and the other
half cannot be derived at all, no matter how the query is written.

## The short version

| Question | Answer |
|---|---|
| Which books exist, their titles and authors | `explorer-3.db` |
| Where the reader stopped, how far in, finished or not | `explorer-3.db` |
| Genres, series, language, publisher, publish year | `books.db` |
| Bookmarks, highlights, quotes — with timestamps | `books.db` |
| Covers the firmware already rendered | `cover_chache/hashed/` |
| **How long anyone read, and on which days** | **nowhere — this is why PocketBook Statistics exists** |

## Sources, in order of usefulness

### 1. `system/explorer-3/explorer-3.db` — the library

The Library app's own database, and the one PocketBook Statistics already polls. Opened
**read-only, always** — it is the firmware's working state, and a write would be
noticed.

| Table | Rows here | What it holds |
|---|---|---|
| `books_impl` | 223 | title, author, `firstauthor`, `series`, `numinseries`, `size`, `isbn`, `ts_added`, `sort_title`, `hidden` |
| `books_settings` | 84 | the reading state: `position`, `position_ts`, `cpage`, `npage`, `opentime`, `completed`, `completed_ts`, `favorite`, `favorite_ts` — one row per (book, profile) |
| `files` | 9 | `filename`, `folder_id`, `storageid`, `size`, `modification_time`, `fast_hash`, `ext` — **only files that exist right now** |
| `books_fast_hashes` | 223 | `fast_hash` → `book_id`; the join to everything outside this database |
| `genres` / `booktogenre` | 18 / 25 | genre names (`sf_social`, `sf_detective`…) and their book links |
| `bookshelfs` / `bookshelfs_books` | 1 / 11 | user shelves |
| `folders`, `storages` | 16 / 4 | filesystem layout |
| `timestamps` | 106 | library rescan marks — `time` only, `description` is empty in practice |
| `file_props` | 344 | per-hash key/value pairs, `prop_type` 3/4/9/10/11/14; values are opaque short ints and UUIDs |
| `social`, `tags2`, `sync_data` | 0 | ReadRate/cloud features, unused on this device |

**Field coverage matters more than the schema.** Out of 84 `books_settings`
rows: 78 have a `position`, 75 have `cpage`/`npage`, 79 have an `opentime`, 26
are `completed`. So roughly one book in ten has no page numbers at all — every
page-based metric needs a fallback. In `books_impl`, only 102 of 223 rows carry
a `ts_added` and just 5 a `series`: sideloaded files often arrive with nothing
but a filename.

**`books_impl` is a history, `files` is an inventory, and the gap between them
is enormous.** On this device: 223 books indexed, 7 book files physically
present, 9 rows in `files` (5 books — a cloud-synced copy gets a second row
under `storageid` 2). 218 books have no file at all.

A missing file says nothing about whether the book was read — but the reading
state left behind does, and the three cases are worth keeping apart:

| Deleted book | Here | What survived |
|---|---|---|
| never opened | 140 | metadata only — no `books_settings` row at all |
| opened, not finished | 52 | position, page, `opentime` |
| finished | 26 | the above plus `completed` / `completed_ts` |

So "deleted" splits into a real library of 78 books that were actually read and
140 that were only ever indexed. Any count of "books I own" or "books I read"
has to pick one of these deliberately — and note that a book deleted **before**
PocketBook Statistics was installed carries firmware timestamps only, which is exactly the
history our own sessions cannot vouch for.

Two consequences worth designing around:

- Anything that needs the file itself — EPUB cover extraction, page counts from
  the document — works for a handful of books and must fall back for the rest.
  The firmware's cover cache, by contrast, keeps 180 PNGs and outlives deletion,
  which makes it the only cover source for a deleted book.
- Joining through `files` silently drops almost the whole library. Use
  `books_fast_hashes` (223 rows, one per book) when a hash is needed.

`books_impl` also holds things that are not books — a `MIT-LICENSE` and other
text files that KOReader's own directories dragged into the index — and the same
book appears several times over as different copies. Hence deduplication by
normalized title rather than `book_id`.

### 2. `system/config/books.db` — metadata, bookmarks, per-book settings

A second, *live* database (its newest write matched `explorer-3.db` to the
second during this survey) with a completely different shape: a generic
item/tag store.

```
Items(OID, ParentID, TypeID, State, TimeAlt, HashUUID)   -- 170 rows
Tags(OID, ItemID, TagID, Val, TimeEdt)                   -- 1467 rows
TagNames(OID, TagName)                                   -- the vocabulary
```

`TypeNames` distinguishes `type.book` (85 rows) from `obj.book_mark` (84) — a
bookmark is an item whose `ParentID` is the book. Everything else is a tag on an
item, and `Tags.TimeEdt` dates each one.

Tags that carry usable signal, with how many books have them here:

| Tag | Count | Value format |
|---|---|---|
| `doc.read_progress` | 75 | fraction, `0.0148680787533522` — finer than `cpage`/`npage` |
| `doc.last_read_position` | 81 | `pbr:/webkit?##epubcfi(...)` |
| `doc.time_opened` | 81 | unix seconds |
| `bm.book_mark` | 84 | `{"anchor":"pbr:/word?page=164…","created":1760125056}` — **a dated event** |
| `bm.type` | 84 | `bookmark` (79), `highlight` (4), `draws` (1) |
| `bm.quotation` | 84 | `{"text":"…"}` — the quoted passage |
| `bm.color`, `bm.draws` | 4 / 1 | highlight colour, freehand drawing |
| `doc.authors` | 75 | author string |
| `doc.publish-year` | 64 | year |
| `doc.language` | 62 | `ru`, `en`… |
| `doc.publisher` | 46 | publisher |
| `doc.genres` | 10 | `sf_social` |
| `doc.annotation` | 9 | publisher blurb, HTML |
| `doc.isbn`, `doc.translators`, `doc.series` | 7 / 7 / 4 | `doc.series` is `{"name":"Бункер","ord":1}` |

`doc.read_status`, `book.read_status_changed`, `doc.covers`, `doc.is_favorite`
and `doc.open_stats` exist in `TagNames` but hold **no rows** on this firmware —
their equivalents live in `explorer-3.db`. A tag name is not a promise.

Bookmark timestamps are the one genuinely dated activity the firmware keeps
besides `completed_ts`: 39 of the 84 bookmarks here were created in one month.
That is a real, if partial, record of reading days — and unlike our own
sessions, it predates PocketBook Statistics's installation.

### 3. Joining the two databases

`books.db` and `explorer-3.db` share no ids. They share a **hash**:

```sql
-- explorer-3.db
SELECT hex(fast_hash), book_id FROM books_fast_hashes;   -- 249498F0B1B09D37…
-- books.db
SELECT HashUUID FROM Items WHERE TypeID = 0;             -- 249498F0B1B09D37…
```

81 of 223 hashes matched here — exactly the books that have ever been opened.
`hex()` output is uppercase on both sides, so no case folding is needed, but
watch the storage type: `fast_hash` is a 16-byte BLOB, `HashUUID` a 32-char
string.

**The cover cache is the reason any of this matters.** It holds 81 PNGs on this
device, one per book that was ever opened — including all 26 finished ones,
whose files were deleted long ago. Reaching them needs the hash from
`books_fast_hashes`; going through `files` finds 5 books and draws the other 218
as a single letter.

The same hash addresses the firmware's cover cache:

```
system/cover_chache/hashed/<storageid><lower(hex(fast_hash))>.png
                           └─ "1" for internal storage
```

PocketBook Statistics already builds exactly this key (`tracker.c`), and prefers its own
extraction from the book file (EPUB, FB2, CBZ) because the cache holds the
wrong image for some sideloaded books.

### 4. Everything else, briefly

| Path | Contents | Verdict |
|---|---|---|
| `system/state/lastopen.txt` | 30 recently opened paths, newest first | ordering only, no timestamps |
| `system/config/global.cfg` | device settings, incl. `statistics_enabled=1`, `obreey_stat=0` | flags for *the firmware's* telemetry; no counters stored on `ext1` |
| `system/config/audiobooks/audiobooks.db` | audiobook progress (`book_state.read_percents`, `last_read_ts`) | empty here, but the schema is there if audiobooks ever matter |
| `system/player-2/player-2.db` | music tracks | irrelevant |
| `system/pbdicts/pbdicts.db` | 92 installed dictionaries | a catalogue, **not** a lookup history |
| `system/browser.sqlite` | browser history | out of scope |
| `system/config/desktop/view.json` | launcher entries | PocketBook Statistics writes here (icon registration) |
| `Notes/` | empty on this device | export target, not a store |

### 5. Third-party, if installed

`applications/koreader/settings/statistics.sqlite3` — KOReader keeps precisely
what the stock firmware does not:

```sql
book(id, title, authors, pages, total_read_time, total_read_pages, …)
page_stat_data(id_book, page, start_time, duration, total_pages)
```

Per-page durations, so real reading time. On this device it holds 4 books and
10 rows from two evenings in July 2026 — enough to prove the format, not enough
to import. Worth knowing about as an optional source for people who use both;
it is not something PocketBook Statistics can rely on.

## What the firmware does not have

- **No session history.** There is one `opentime` and one `position_ts` per
  book, overwritten on every read. Yesterday's reading is gone the moment you
  open the book today.
- **No reading duration anywhere.** Not per book, not per day, not in total.
- **No page-turn events**, no per-page timing, nothing with the granularity
  KOReader records.
- **No dated history before the app was installed.** `completed_ts` and
  bookmark `created` stamps are the only exceptions — everything else is a
  single "most recent" value.

This is the whole reason PocketBook Statistics runs a daemon: reading time has to be
*derived*, by polling `position_ts` every 30 seconds and turning the movement
into sessions in our own database. See `src/tracker.c` and the invariants in
`CLAUDE.md`.

## Metrics this survey makes possible

Not yet implemented, ordered by effort. All of them are pure reads of data that
already exists — no new tracking, no schema change on our side.

| Metric | Source | Note |
|---|---|---|
| Books by genre / language / publish decade | `books.db` tags | Coverage is thin: 10 books have a genre, 62 a language. Show only what is known — never render a gap as zero. |
| Series progress ("book 2 of 4") | `doc.series` + `books_impl.series` | 4–5 books here; useful, rarely populated. |
| Bookmarks and highlights per book | `books.db` `obj.book_mark` items | 84 items, dated, with the quoted text. |
| Highlighting activity over time | `bm.book_mark` `created` | The only dated activity that predates our install — a heatmap could show it as a distinct mark from measured reading. |
| Finer progress than pages | `doc.read_progress` | A float where `cpage`/`npage` gives integers, and present for 75 books. |
| Shelf-based stats | `bookshelfs_books` | 1 shelf, 11 books here. |
| Library growth over time | `books_impl.ts_added` | 102 of 223 rows have it. |
| "Owned but never opened" | `books_impl` minus `books_settings` | 140 rows here, file present or not; mixed with indexed non-books, so filter by extension or hash presence. |
| Books read but no longer on the device | `books_settings` where no `files` row | 78 here (26 of them finished) — invisible in the Library UI, and most of PocketBook Statistics's own history points at them. |

## Rules for using any of it

1. **Read-only, every time.** Both databases belong to running firmware apps.
   Open with `SQLITE_OPEN_READONLY`, never `ATTACH` for writing, and never hold
   a transaction open across a poll.
2. **Assume every field is missing.** Coverage numbers above are from one
   device; a fresh library or a shop-bought one will differ wildly. Every metric
   needs a defined look when its input is absent.
3. **A tag name in `TagNames` proves nothing.** Five of them are empty here.
   Count rows before designing a screen around one.
4. **Deduplicate by title, not id.** Both databases keep stale rows, and the
   same book arrives repeatedly as different files.
5. **The hash is the only cross-database key.** Nothing else lines up.
6. **`ext1` is not the whole device.** Firmware-internal state (`/ebrmain`,
   `/mnt/secure`) is invisible over USB; anything not under `/mnt/ext1` was out
   of reach for this survey and may hold more.

## Reproducing this

The device mounts as a plain volume; copy the databases off it and query the
copies, so nothing can be written back by accident:

```bash
cp "/Volumes/NO NAME/system/explorer-3/explorer-3.db" /tmp/
cp "/Volumes/NO NAME/system/config/books.db" /tmp/
sqlite3 /tmp/books.db "SELECT n.TagName, COUNT(*) FROM Tags t
  JOIN TagNames n ON n.OID = t.TagID GROUP BY n.TagName ORDER BY 2 DESC;"
```

The volume name is whatever the reader reports — `NO NAME` on this one.
