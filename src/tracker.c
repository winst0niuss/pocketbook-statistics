#include "tracker.h"
#include "daemon.h"
#include "log.h"
#define STR_(x) #x
#define STR(x) STR_(x)
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static const char *SCHEMA =
    "CREATE TABLE IF NOT EXISTS sessions ("
    "  book_id INTEGER NOT NULL,"
    "  start_time INTEGER NOT NULL,"   /* = opentime aus explorer-3 */
    "  end_time INTEGER NOT NULL,"     /* = letztes position_ts */
    "  active_seconds INTEGER NOT NULL DEFAULT 0,"
    "  pages_start INTEGER,"           /* NULL = unbekannt */
    "  pages_end INTEGER,"
    "  recovered INTEGER NOT NULL DEFAULT 0,"
    /* Running total of counted device sleep as of end_time. The next
     * observation subtracts the difference from the window it bills, so a
     * closed cover costs nothing. */
    "  sleep_end INTEGER NOT NULL DEFAULT 0,"
    /* Pages this row is credited with — not pages_end - pages_start. Those two
     * are positions, and a position also moves by navigation: a footnote link
     * jumps hundreds of pages and back. Only what was believed as reading is
     * added here, which makes this the one column a pace may be built on. */
    "  pages_read INTEGER NOT NULL DEFAULT 0,"
    "  PRIMARY KEY (book_id, start_time));"
    "CREATE TABLE IF NOT EXISTS books ("
    "  book_id INTEGER PRIMARY KEY,"
    "  title TEXT, author TEXT, cover TEXT,"
    "  cpage INTEGER, npage INTEGER, completed INTEGER,"
    "  last_seen INTEGER);"
    "CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value INTEGER);";

/* Retrofits rows written by older versions: recovered spans were capped far
 * too generously and carried a pages_start they had no measured time for.
 * Idempotent, runs on every daemon start. */
static const char *MIGRATE =
    "UPDATE sessions SET pages_start = NULL"
    " WHERE recovered = 1 AND pages_start IS NOT NULL;"
    "UPDATE sessions SET active_seconds = " STR(RECOVERED_CAP_SECONDS)
    " WHERE recovered = 1 AND active_seconds > " STR(RECOVERED_CAP_SECONDS) ";"
    /* No session is worth more wall clock than it spans. Rows written before
     * 1.6.2-rc4 could be: the daemon and the app each credited the same
     * stretch from their own last-seen position, so 22 minutes of reading
     * landed as 44 and one evening reported 208 minutes for 92. The slack of
     * one second is the midnight split, whose first row ends at midnight - 1
     * while holding the seconds up to midnight itself. */
    "UPDATE sessions SET active_seconds = end_time - start_time"
    " WHERE recovered = 0 AND active_seconds > end_time - start_time + 1;"
    /* Stamped once, on the first run: before this moment the firmware kept no
     * session history, so anything we reconstruct for earlier days is a guess
     * and must not be presented as measured reading. An upgrade from a version
     * without the marker dates it to the first session that was measured. */
    "INSERT OR IGNORE INTO meta (key, value) VALUES ('tracking_since',"
    " COALESCE((SELECT MIN(start_time) FROM sessions WHERE recovered = 0),"
    "          strftime('%s','now')));";

static int exec1(sqlite3 *db, const char *sql)
{
    char *err = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "sql error: %s\n", err ? err : "?");
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

static void split_legacy_midnight(tracker *t);
static void backfill_pages_read(tracker *t);

int tracker_init(tracker *t, const char *stats_path, const char *explorer_path)
{
    memset(t, 0, sizeof(*t));
    t->explorer_path = explorer_path;
    if (sqlite3_open(stats_path, &t->stats) != SQLITE_OK)
        return -1;
    sqlite3_busy_timeout(t->stats, 2000);
    if (exec1(t->stats, SCHEMA) != 0)
        return -1;
    /* Added in 1.6.4. The statement failing because the column is already
     * there is the check, so the error is the expected case and is ignored;
     * existing rows start at 0, which is also where sleep_total starts. */
    sqlite3_exec(t->stats,
                 "ALTER TABLE sessions ADD COLUMN sleep_end INTEGER NOT NULL"
                 " DEFAULT 0",
                 NULL, NULL, NULL);
    /* Added in 1.6.5, the same way. */
    sqlite3_exec(t->stats,
                 "ALTER TABLE sessions ADD COLUMN pages_read INTEGER NOT NULL"
                 " DEFAULT 0",
                 NULL, NULL, NULL);
    if (exec1(t->stats, MIGRATE) != 0)
        return -1;
    split_legacy_midnight(t);
    backfill_pages_read(t);
    return 0;
}

void tracker_close(tracker *t)
{
    if (t->stats)
        sqlite3_close(t->stats);
    t->stats = NULL;
}

/* Always open the firmware DB briefly and read-only. */
static sqlite3 *open_explorer(const char *path)
{
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db)
            sqlite3_close(db);
        return NULL;
    }
    sqlite3_busy_timeout(db, 1000);
    return db;
}

#define STATE_COLUMNS \
    "SELECT s.bookid, s.opentime, s.position_ts," \
    "  IFNULL(s.cpage,0), IFNULL(s.npage,0), IFNULL(s.completed,0)," \
    "  IFNULL(b.title,''), IFNULL(b.author,''),"
#define STATE_SOURCE \
    " FROM books_settings s JOIN books_impl b ON b.id = s.bookid" \
    " WHERE s.opentime > 0 AND s.position_ts > 0"

/* The cover key is the bare file hash. It comes from books_fast_hashes, which
 * has a row for every book the library ever indexed — files only lists what is
 * still on disk, so a deleted book would lose its key and with it any chance
 * of ever showing a thumbnail again. */
#define COVER_FROM_HASHES \
    "  IFNULL((SELECT lower(hex(h.fast_hash)) FROM books_fast_hashes h" \
    "          WHERE h.book_id = s.bookid LIMIT 1), '')"
/* Older explorer-3 schemas have no such table; losing tracking over a cover
 * key would be a poor trade, so those fall back to the file's own hash. */
#define COVER_FROM_FILES \
    "  IFNULL((SELECT lower(hex(f.fast_hash)) FROM files f" \
    "          WHERE f.book_id = s.bookid ORDER BY f.storageid LIMIT 1), '')"

static const char *STATE_SQL =
    STATE_COLUMNS COVER_FROM_HASHES STATE_SOURCE " ORDER BY s.opentime DESC LIMIT 1";
static const char *STATE_SQL_LEGACY =
    STATE_COLUMNS COVER_FROM_FILES STATE_SOURCE " ORDER BY s.opentime DESC LIMIT 1";
static const char *RECOVER_SQL = STATE_COLUMNS COVER_FROM_HASHES STATE_SOURCE;
static const char *RECOVER_SQL_LEGACY = STATE_COLUMNS COVER_FROM_FILES STATE_SOURCE;

/* Prepares the first statement the schema accepts. */
static int prepare_supported(sqlite3 *db, const char *sql, const char *legacy,
                             sqlite3_stmt **st)
{
    if (sqlite3_prepare_v2(db, sql, -1, st, NULL) == SQLITE_OK)
        return 0;
    return sqlite3_prepare_v2(db, legacy, -1, st, NULL) == SQLITE_OK ? 0 : -1;
}

static void fill_state(sqlite3_stmt *st, pb_state *out)
{
    memset(out, 0, sizeof(*out));
    out->bookid = sqlite3_column_int64(st, 0);
    out->opentime = sqlite3_column_int64(st, 1);
    out->position_ts = sqlite3_column_int64(st, 2);
    out->cpage = sqlite3_column_int(st, 3);
    out->npage = sqlite3_column_int(st, 4);
    out->completed = sqlite3_column_int(st, 5);
    snprintf(out->title, sizeof(out->title), "%s", sqlite3_column_text(st, 6));
    snprintf(out->author, sizeof(out->author), "%s", sqlite3_column_text(st, 7));
    snprintf(out->cover, sizeof(out->cover), "%s", sqlite3_column_text(st, 8));
}

int tracker_read_state(const char *explorer_path, pb_state *out)
{
    sqlite3 *db = open_explorer(explorer_path);
    if (!db)
        return -1;
    sqlite3_stmt *st = NULL;
    int rc = 1;
    if (prepare_supported(db, STATE_SQL, STATE_SQL_LEGACY, &st) == 0) {
        if (sqlite3_step(st) == SQLITE_ROW) {
            fill_state(st, out);
            rc = 0;
        }
    } else {
        rc = -1;
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    return rc;
}

static int file_exists(const char *path)
{
    struct stat sb;
    return stat(path, &sb) == 0;
}

/* Copies the firmware's cover into our own cache the first time a book is
 * seen. Both halves of that matter: the firmware drops its cache entry when
 * the book is deleted, and a finished book is usually deleted soon after — so
 * this copy, taken while the book is still here, is the only thumbnail a
 * finished book will still have next year.
 *
 * Runs in the poll path, so it allocates nothing and does nothing at all once
 * the copy exists. */
static void adopt_cover(const char *hash)
{
    if (!hash || !*hash)
        return;

    char dest[256];
    snprintf(dest, sizeof(dest), OWN_COVER_DIR "/fw_%s.png", hash);
    if (file_exists(dest))
        return;

    /* The firmware prefixes the file with the storage the book sits on. */
    char src[256];
    int found = 0;
    for (int storage = 1; storage <= 4 && !found; storage++) {
        snprintf(src, sizeof(src), COVER_DIR "/%d%s.png", storage, hash);
        found = file_exists(src);
    }
    if (!found)
        return;

    mkdir(OWN_COVER_DIR, 0755);
    FILE *in = fopen(src, "rb");
    if (!in)
        return;
    FILE *out = fopen(dest, "wb");
    if (!out) {
        fclose(in);
        return;
    }

    char buf[8192];
    size_t n;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            ok = 0;
            break;
        }
    }
    fclose(in);
    if (fclose(out) != 0)
        ok = 0;
    if (!ok)
        unlink(dest); /* half a PNG is worse than none */
}

static void upsert_book(tracker *t, const pb_state *s)
{
    sqlite3_stmt *st = NULL;
    const char *sql =
        "INSERT INTO books (book_id, title, author, cover, cpage, npage, completed, last_seen)"
        " VALUES (?1,?2,?3,?4,?5,?6,?7,?8)"
        " ON CONFLICT(book_id) DO UPDATE SET title=?2, author=?3, cover=?4,"
        "  cpage=?5, npage=?6, completed=?7, last_seen=?8";
    if (sqlite3_prepare_v2(t->stats, sql, -1, &st, NULL) != SQLITE_OK)
        return;
    sqlite3_bind_int64(st, 1, s->bookid);
    sqlite3_bind_text(st, 2, s->title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, s->author, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, s->cover, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 5, s->cpage);
    sqlite3_bind_int(st, 6, s->npage);
    sqlite3_bind_int(st, 7, s->completed);
    sqlite3_bind_int64(st, 8, s->position_ts);
    sqlite3_step(st);
    sqlite3_finalize(st);
    adopt_cover(s->cover);
}

/* pages_end of this book's last earlier session, -1 if none. */
static int prev_pages_end(tracker *t, int64_t bookid, int64_t before)
{
    sqlite3_stmt *st = NULL;
    int val = -1;
    const char *sql =
        "SELECT pages_end FROM sessions WHERE book_id=?1 AND start_time<?2"
        " AND pages_end IS NOT NULL ORDER BY start_time DESC LIMIT 1";
    if (sqlite3_prepare_v2(t->stats, sql, -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int64(st, 1, bookid);
    sqlite3_bind_int64(st, 2, before);
    if (sqlite3_step(st) == SQLITE_ROW)
        val = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return val;
}

/* Wall clock and the monotonic clock disagree by exactly the time the device
 * spent suspended: CLOCK_MONOTONIC stops with the CPU, time() does not. That
 * difference is the only signal on this firmware that can tell an hour of
 * reading from an hour of the reader lying closed — position_ts cannot, because
 * the firmware writes it when the reader saves state, every 15 to 75 minutes,
 * not on a page turn. */
int64_t pb_monotonic_seconds(void)
{
#ifdef CLOCK_MONOTONIC
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        return (int64_t)ts.tv_sec;
#endif
    return 0;
}

static int64_t meta_get(tracker *t, const char *key, int64_t missing)
{
    sqlite3_stmt *st = NULL;
    int64_t v = missing;
    if (sqlite3_prepare_v2(t->stats, "SELECT value FROM meta WHERE key=?1", -1,
                           &st, NULL) != SQLITE_OK)
        return v;
    sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
    if (sqlite3_step(st) == SQLITE_ROW)
        v = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return v;
}

static void meta_set(tracker *t, const char *key, int64_t value)
{
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(
            t->stats, "INSERT OR REPLACE INTO meta (key,value) VALUES (?1,?2)",
            -1, &st, NULL) != SQLITE_OK)
        return;
    sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_int64(st, 2, value);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* Runs on every observation, which for the daemon is once a poll. Between two
 * polls the monotonic clock advances by the loop's own sleep and no more, while
 * the wall clock also counts whatever the device spent suspended in between;
 * the difference is that suspend, and a long one is the book put down. Only
 * marks one poll apart are classified, so the app's catchUp() — which looks
 * hours after the last mark — contributes nothing rather than blaming a whole
 * afternoon on sleep. The total is persisted only when it grows, because the
 * poll path must not write to flash every thirty seconds. */
static int64_t note_sleep(tracker *t)
{
    const int64_t wall = (int64_t)time(NULL);
    const int64_t mono = pb_monotonic_seconds();
    int64_t total = meta_get(t, "sleep_total", 0);
    const int64_t dw = wall - t->mark_wall;
    const int64_t dm = mono - t->mark_mono;

    /* dm below zero is a reboot, dw below zero the clock being set; either
     * costs one interval of classification and nothing else. */
    if (t->mark_wall > 0 && dm >= 0 && dm <= 2 * POLL_SECONDS && dw > dm) {
        const int64_t slept = dw - dm;
        if (slept >= DEEP_SLEEP_SECONDS) {
            total += slept;
            meta_set(t, "sleep_total", total);
        }
        if (slept >= SLEEP_LOG_SECONDS)
            pb_log("sleep: suspended %d s, poll advanced %d s%s", (int)slept,
                   (int)dm, slept >= DEEP_SLEEP_SECONDS ? " (not read)" : "");
    }
    t->mark_wall = wall;
    t->mark_mono = mono;
    return total;
}

/* Local midnight (as epoch) of the day ts falls into. */
static int64_t local_day_start(int64_t ts)
{
    time_t tt = (time_t)ts;
    struct tm tm;
    localtime_r(&tt, &tm);
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    return (int64_t)mktime(&tm);
}

static void insert_session(tracker *t, const pb_state *s, int64_t start_time,
                           int64_t active, int pages, int pages_start_known,
                           int recovered)
{
    sqlite3_stmt *st = NULL;
    const char *sql =
        "INSERT OR IGNORE INTO sessions"
        " (book_id, start_time, end_time, active_seconds, pages_start,"
        "  pages_end, recovered, sleep_end, pages_read)"
        " VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9)";
    if (sqlite3_prepare_v2(t->stats, sql, -1, &st, NULL) != SQLITE_OK)
        return;
    sqlite3_bind_int64(st, 1, s->bookid);
    sqlite3_bind_int64(st, 2, start_time);
    sqlite3_bind_int64(st, 3, s->position_ts);
    sqlite3_bind_int64(st, 4, active);
    /* Recovered rows get no pages_start: the pages were read while nothing was
     * tracking, so the delta would not match the (capped) time. */
    int ps = recovered ? -1 : prev_pages_end(t, s->bookid, start_time);
    if (ps < 0 && pages_start_known)
        ps = s->cpage; /* session just started: current page = start page */
    if (ps >= 0)
        sqlite3_bind_int(st, 5, ps);
    else
        sqlite3_bind_null(st, 5);
    sqlite3_bind_int(st, 6, s->cpage);
    sqlite3_bind_int(st, 7, recovered);
    sqlite3_bind_int64(st, 8, meta_get(t, "sleep_total", 0));
    sqlite3_bind_int(st, 9, pages > 0 ? pages : 0);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* Adds active seconds to one session row and moves its end. The row's own
 * end_time guards the write, because two trackers write here and sqlite
 * serializes them: one that computed its gap from a position the other has
 * already moved past adds nothing instead of adding it twice. (Clamping the
 * total to the row's span belongs in the migration rather than here: the row
 * a midnight split leaves behind ends at midnight - 1, one second short of the
 * time it legitimately holds.) */
static void update_session(tracker *t, int64_t bookid, int64_t start_time,
                           int64_t end_time, int64_t add_active, int add_pages,
                           int pages_end)
{
    sqlite3_stmt *st = NULL;
    const char *sql =
        "UPDATE sessions SET end_time=?1, active_seconds=active_seconds+?2,"
        " pages_end=?3, sleep_end=?6, pages_read=pages_read+?7"
        " WHERE book_id=?4 AND start_time=?5 AND end_time<?1";
    if (sqlite3_prepare_v2(t->stats, sql, -1, &st, NULL) != SQLITE_OK)
        return;
    sqlite3_bind_int64(st, 1, end_time);
    sqlite3_bind_int64(st, 2, add_active);
    sqlite3_bind_int(st, 3, pages_end);
    sqlite3_bind_int64(st, 4, bookid);
    sqlite3_bind_int64(st, 5, start_time);
    sqlite3_bind_int64(st, 6, meta_get(t, "sleep_total", 0));
    sqlite3_bind_int(st, 7, add_pages > 0 ? add_pages : 0);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* Places a whole session at once, splitting the row at local midnight when the
 * seconds it credits fall on both sides of one. Every day-level aggregate
 * groups by date(end_time), so a single row holding 23:30 to 00:03 puts all 33
 * of its minutes on the new day and leaves the previous one empty.
 * tracker_observe() splits the gaps it watches; this is the same rule for the
 * two paths that write a session that already ran before anything saw it — a
 * first observation (the daemon is not scheduled while a book is on screen, so
 * StatsBridge::catchUp() is regularly the first to see a session at all) and
 * tracker_recover(). The counted window is [position_ts - active, position_ts]
 * and active never exceeds RECOVERED_CAP_SECONDS, so it can cross at most one
 * midnight. Returns the start_time of the row left open — the one later
 * observations extend. */
static int64_t insert_session_split(tracker *t, const pb_state *s,
                                    int64_t start_time, int64_t active,
                                    int pages, int pages_start_known,
                                    int recovered)
{
    const int64_t midnight = local_day_start(s->position_ts);
    int64_t before = midnight - (s->position_ts - active);
    if (midnight <= start_time || before <= 0) {
        insert_session(t, s, start_time, active, pages, pages_start_known,
                       recovered);
        return start_time;
    }
    if (before > active)
        before = active;
    /* The pages go with the seconds: nothing here knows where inside the
     * window they were turned, and the credited seconds are the only measure
     * of how much of the reading fell on each side. */
    const int pages_before =
        active > 0 ? (int)((int64_t)pages * before / active) : 0;

    /* The first row ends at midnight - 1 so date(end_time) is the day it
     * belongs to; it still holds the seconds up to midnight itself, which is
     * the one second of slack the migration's clamp allows for. */
    pb_state head = *s;
    head.position_ts = midnight - 1;
    insert_session(t, &head, start_time, before, pages_before,
                   pages_start_known, recovered);
    insert_session(t, s, midnight, active - before, pages - pages_before, 0,
                   recovered);
    return midnight;
}

/* The one-shot half of the midnight fix: rows already in the database that
 * were placed as a whole and never cut. Reading from 23:30 to 00:03 had put
 * all 33 of its minutes on the new day and left the evening before it empty,
 * and no later observation goes back to correct that. The seconds are moved,
 * never invented: the row keeps what falls before midnight and a new row takes
 * the rest. Local midnight is not expressible in SQLite without repeating a
 * date-arithmetic expression three times, so the rows are picked here instead.
 *
 * The marker in `meta` keeps this off every later open — tracker_init runs on
 * every catchUp(), and this scans the whole table. A future fix to this code
 * needs a new key, since the old one is already stamped. */
static void split_legacy_midnight(tracker *t)
{
    if (sqlite3_exec(t->stats,
                     "INSERT OR IGNORE INTO meta (key,value)"
                     " VALUES ('midnight_split_1_6_3', 1)",
                     NULL, NULL, NULL) != SQLITE_OK ||
        sqlite3_changes(t->stats) == 0)
        return;

    struct {
        int64_t book, start, end, active;
        int pages_end, recovered;
    } todo[64];

    /* Each pass fixes at most as many rows as the batch holds and none of them
     * matches again, so the loop terminates; the bound is a backstop. */
    for (int pass = 0; pass < 64; pass++) {
        sqlite3_stmt *st = NULL;
        const char *sql =
            "SELECT book_id, start_time, end_time, active_seconds, pages_end,"
            " recovered FROM sessions WHERE active_seconds > 0";
        if (sqlite3_prepare_v2(t->stats, sql, -1, &st, NULL) != SQLITE_OK)
            return;
        int n = 0;
        while (n < (int)(sizeof(todo) / sizeof(todo[0])) &&
               sqlite3_step(st) == SQLITE_ROW) {
            const int64_t start = sqlite3_column_int64(st, 1);
            const int64_t end = sqlite3_column_int64(st, 2);
            const int64_t active = sqlite3_column_int64(st, 3);
            const int64_t midnight = local_day_start(end);
            /* Only a row whose credited window genuinely straddles the
             * midnight before its end: one that starts after it, or whose
             * seconds all fall after it, is already on the right day — the
             * head a live split leaves behind included. */
            if (midnight <= start || end - active >= midnight)
                continue;
            todo[n].book = sqlite3_column_int64(st, 0);
            todo[n].start = start;
            todo[n].end = end;
            todo[n].active = active;
            todo[n].pages_end = sqlite3_column_int(st, 4);
            todo[n].recovered = sqlite3_column_int(st, 5);
            n++;
        }
        sqlite3_finalize(st);
        if (n == 0)
            return;

        for (int i = 0; i < n; i++) {
            const int64_t midnight = local_day_start(todo[i].end);
            const int64_t before = midnight - (todo[i].end - todo[i].active);

            /* The tail row first: if it cannot be written — a session already
             * starts at this midnight — the original row is left alone rather
             * than shortened, so no reading is lost. */
            sqlite3_stmt *st2 = NULL;
            const char *ins =
                "INSERT OR IGNORE INTO sessions (book_id, start_time, end_time,"
                " active_seconds, pages_start, pages_end, recovered)"
                " VALUES (?1,?2,?3,?4,?5,?6,?7)";
            if (sqlite3_prepare_v2(t->stats, ins, -1, &st2, NULL) != SQLITE_OK)
                return;
            sqlite3_bind_int64(st2, 1, todo[i].book);
            sqlite3_bind_int64(st2, 2, midnight);
            sqlite3_bind_int64(st2, 3, todo[i].end);
            sqlite3_bind_int64(st2, 4, todo[i].active - before);
            /* The pages were all turned before the cut as far as anything here
             * knows, so the tail carries none of them; a recovered row has no
             * pages_start at all. */
            if (todo[i].recovered)
                sqlite3_bind_null(st2, 5);
            else
                sqlite3_bind_int(st2, 5, todo[i].pages_end);
            sqlite3_bind_int(st2, 6, todo[i].pages_end);
            sqlite3_bind_int(st2, 7, todo[i].recovered);
            sqlite3_step(st2);
            sqlite3_finalize(st2);
            if (sqlite3_changes(t->stats) == 0)
                continue;

            sqlite3_stmt *st3 = NULL;
            const char *upd =
                "UPDATE sessions SET end_time=?1, active_seconds=?2"
                " WHERE book_id=?3 AND start_time=?4";
            if (sqlite3_prepare_v2(t->stats, upd, -1, &st3, NULL) != SQLITE_OK)
                return;
            sqlite3_bind_int64(st3, 1, midnight - 1);
            sqlite3_bind_int64(st3, 2, before);
            sqlite3_bind_int64(st3, 3, todo[i].book);
            sqlite3_bind_int64(st3, 4, todo[i].start);
            sqlite3_step(st3);
            sqlite3_finalize(st3);
        }
    }
}

/* Rows written before pages_read existed carry their pages only as the two
 * positions, so the column starts as what the aggregates used to compute from
 * them. That keeps the jump distances those rows may hold — nothing here can
 * tell a jump from reading after the fact — but a pace that is no worse than
 * yesterday's beats one wiped to zero until new sessions accumulate. Guarded
 * by a marker rather than by `pages_read = 0`, which a legitimately uncredited
 * new row also matches: without it every window that was all jump would be
 * handed its jump back on the next open. */
static void backfill_pages_read(tracker *t)
{
    if (sqlite3_exec(t->stats,
                     "INSERT OR IGNORE INTO meta (key,value)"
                     " VALUES ('pages_read_1_6_5', 1)",
                     NULL, NULL, NULL) != SQLITE_OK ||
        sqlite3_changes(t->stats) == 0)
        return;
    sqlite3_exec(t->stats,
                 "UPDATE sessions SET pages_read = pages_end - pages_start"
                 " WHERE recovered = 0 AND pages_start IS NOT NULL"
                 " AND pages_end > pages_start",
                 NULL, NULL, NULL);
}

int tracker_recover(tracker *t)
{
    sqlite3 *db = open_explorer(t->explorer_path);
    if (!db)
        return -1;
    sqlite3_stmt *st = NULL;
    if (prepare_supported(db, RECOVER_SQL, RECOVER_SQL_LEGACY, &st) != 0) {
        sqlite3_close(db);
        return -1;
    }
    /* The session that is open right now is the one about to be measured, and
     * it must not be stamped as a reconstruction on the way past. This runs at
     * daemon start, and the shim starts the daemon seconds after a book is
     * opened: whether a page had been turned by then decided whether the whole
     * evening ended up marked `recovered` and dropped from every average. */
    pb_state cur;
    const int have_cur = tracker_read_state(t->explorer_path, &cur) == 0;

    int n = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        pb_state s;
        fill_state(st, &s);
        if (have_cur && s.bookid == cur.bookid && s.opentime == cur.opentime)
            continue;
        int64_t span = s.position_ts - s.opentime;
        if (span <= 0)
            continue; /* reopened where it was left: nothing to reconstruct */
        if (span > RECOVERED_CAP_SECONDS)
            span = RECOVERED_CAP_SECONDS;
        /* No pages either: a recovered row has no pages_start, and the pages
         * it would claim were turned while nothing was measuring. */
        insert_session_split(t, &s, s.opentime, span, 0, 0, 1);
        upsert_book(t, &s);
        n++;
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    return n;
}

/* The one rule the tracker measures by: a span of wall clock is worth reading
 * only as far as the pages turned across it make plausible. A flat cap cannot
 * tell twenty unwatched minutes of reading from a single page turned after an
 * hour away, and paid the same ten minutes for both — which is how a 19-minute
 * evening reached the screen as one minute. `pages` below zero means there is
 * no page evidence at all, and then the flat cap is all that is left.
 * RECOVERED_CAP_SECONDS is the backstop: no single unobserved stretch, however
 * many pages it claims, is worth more than an hour and a half. */
static int64_t credited(int64_t span, int pages)
{
    if (span <= 0)
        return 0;
    int64_t bound = pages < 0
                        ? IDLE_CAP_SECONDS
                        : (int64_t)(pages > 0 ? pages : 1) * SECONDS_PER_PAGE_CAP;
    if (bound > RECOVERED_CAP_SECONDS)
        bound = RECOVERED_CAP_SECONDS;
    return span < bound ? span : bound;
}

/* The same rule read the other way round: a forward move of the position is
 * reading only as far as the span it happened in can account for. Books that
 * keep their footnotes at the back are read by tapping a link and tapping
 * back, and the firmware saves a position hundreds of pages from the last one
 * — which under credited() vouches for the entire window and, worse, is added
 * to the pace as pages that were never read. A jump forward is treated like
 * the jump backwards already was: it buys nothing, and what is read from
 * wherever the reader lands is credited by the observations that follow.
 * The floor keeps the rate rule off the small deltas short windows carry:
 * two firmware saves can be seconds apart, and three pages turned between
 * them would otherwise fail it. */
static int page_jump(int64_t span, int pages)
{
    return pages >= JUMP_MIN_PAGES &&
           (int64_t)pages * SECONDS_PER_PAGE_MIN > span;
}

/* Picks up the row an earlier observation left open for this (book, open),
 * so a tracker that has just been created continues the session instead of
 * starting one. Every caller builds a fresh tracker sooner or later — the
 * daemon after a restart, and StatsBridge::catchUp() on every single refresh —
 * and without this the stretch since the last observation is not capped but
 * dropped: 18 minutes and 30 turned pages used to land as nothing at all.
 * 0 = adopted, and the caller may go straight to the gap logic. */
static int resume_session(tracker *t, const pb_state *s)
{
    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT start_time, end_time, pages_end, sleep_end FROM sessions"
        " WHERE book_id=?1 AND recovered=0 AND start_time>=?2"
        " ORDER BY start_time DESC LIMIT 1";
    if (sqlite3_prepare_v2(t->stats, sql, -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int64(st, 1, s->bookid);
    sqlite3_bind_int64(st, 2, s->opentime);

    int found = -1;
    if (sqlite3_step(st) == SQLITE_ROW) {
        t->cur_book = s->bookid;
        t->cur_open = s->opentime;
        t->cur_row_start = sqlite3_column_int64(st, 0);
        t->cur_pos_ts = sqlite3_column_int64(st, 1);
        t->cur_pages = sqlite3_column_type(st, 2) == SQLITE_NULL
                           ? s->cpage
                           : sqlite3_column_int(st, 2);
        t->cur_sleep = sqlite3_column_int64(st, 3);
        found = 0;
    }
    sqlite3_finalize(st);
    return found;
}

int tracker_observe(tracker *t, const pb_state *s)
{
    /* Before the early return: the marks have to stay one poll apart, and a
     * tick with no book open is still a poll. */
    const int64_t sleep_total = note_sleep(t);

    if (!s || s->bookid <= 0)
        return 0;

    /* position_ts can predate opentime: the firmware stamps the open
     * immediately but only moves the position on a page turn, so a book
     * reopened where it was left carries the timestamp of the *previous*
     * session. Two things then go wrong unless the open is treated as the
     * start of the clock:
     *
     *   - the row would end before it began (start = opentime, end = an older
     *     position_ts), which is how 16 zero-length rows with end_time <
     *     start_time got into a real database;
     *   - the first page turn would be measured from that stale timestamp, so
     *     a gap of hours would land, capped, as a flat ten minutes of reading.
     *     Two of those in one afternoon added 20 minutes to a day with 15
     *     minutes of actual reading. */
    pb_state open = *s;
    if (open.position_ts < open.opentime)
        open.position_ts = open.opentime;

    /* What has already been paid for is what the row says, not what this
     * process remembers — and it is re-read on every observation, not only
     * when the session changes. Two trackers watch the same book: the daemon's,
     * which lives across polls, and the fresh one StatsBridge::catchUp() builds
     * on every refresh; a second daemon can be spawned besides. One that
     * measures from its own last-seen position bills a stretch another has
     * already billed, and on a real device three sessions in one evening came
     * out at exactly twice the wall clock they spanned. */
    if (resume_session(t, &open) != 0) {
        /* Nothing has written this session yet, so it is genuinely new. The
         * span from the open to the last page turn is reading nobody watched,
         * and the pages since this book's previous session say how much of it
         * to believe. */
        const int prev = prev_pages_end(t, open.bookid, open.opentime);
        const int delta = prev < 0 ? -1 : open.cpage - prev;
        const int64_t span = open.position_ts - open.opentime;
        int64_t active = credited(span, delta);
        if (page_jump(span, delta)) {
            pb_log("jump: %d pages in %d s, not read", delta, (int)span);
            active = 0;
        }
        /* Pages are credited only where seconds are. A window that bought no
         * time — a stale position_ts, or one the device slept through — bought
         * no reading either, and pages without minutes beside them are exactly
         * what makes a pace lie. */
        const int pages = active > 0 && delta > 0 ? delta : 0;
        t->cur_row_start =
            insert_session_split(t, &open, open.opentime, active, pages, 1, 0);
        upsert_book(t, &open);
        t->cur_book = open.bookid;
        t->cur_open = open.opentime;
        t->cur_pos_ts = open.position_ts;
        t->cur_pages = open.cpage;
        t->cur_sleep = sleep_total;
        return 1;
    }

    if (s->position_ts > t->cur_pos_ts) {
        /* The window between two position saves is wall clock, and on this
         * device wall clock includes the hours the reader lay closed: the
         * firmware writes position_ts when it saves state, not on a page turn,
         * so one window routinely holds twenty minutes of reading and an hour
         * of sleep. Bill only what the device was awake for. A real evening:
         * 18:39 to 19:54, thirteen pages — 65 minutes of it were credited
         * because thirteen pages buy that much, and the cover had been shut
         * for most of them. */
        int64_t span = s->position_ts - t->cur_pos_ts;
        int64_t asleep = sleep_total - t->cur_sleep;
        /* Only the sleep that fits inside the window may be deducted from it.
         * The firmware saves the position as the reader loses the screen, so
         * the poll that sees a new position_ts is regularly the first one after
         * the cover was shut — and the sleep it just recorded lies *after* the
         * window, not in it. Without this, fifty real minutes ending at a save
         * would be wiped by the ninety minutes of standby that followed. The
         * bound is deliberately the pessimistic one: what could not possibly
         * have been inside is removed, and the rest is allowed to count as
         * sleep, so this never invents reading time. */
        const int64_t after = (int64_t)time(NULL) - s->position_ts;
        if (asleep > 0 && after > 0)
            asleep -= after < asleep ? after : asleep;
        if (asleep > 0) {
            span -= asleep;
            if (span < 0)
                span = 0;
            pb_log("credit: window %d s, asleep %d s, %d pages",
                   (int)(s->position_ts - t->cur_pos_ts), (int)asleep,
                   s->cpage - t->cur_pages);
        }

        /* A page backwards is a jump — to a bookmark, or back to re-read — and
         * buys nothing on its own; the pages read forward from there are
         * credited by the observations that follow. A move far enough forward
         * is the same jump in the other direction, and costs the window the
         * same way. */
        const int delta = s->cpage - t->cur_pages;
        const int delta_fwd = delta > 0 ? delta : 0;
        int64_t gap = delta < 0 ? 0 : credited(span, delta);
        if (page_jump(span, delta)) {
            pb_log("jump: %d pages in %d s awake, not read", delta, (int)span);
            gap = 0;
        }
        /* Pages follow the seconds: a window worth no time is worth no pages
         * either, or a stretch the device slept through would add pages to the
         * pace with no minutes to divide them by. */
        int pages = gap > 0 ? delta_fwd : 0;

        /* All day stats group by date(end_time), so a session must not carry
         * time across a local midnight: split the row there. The counted window
         * is [position_ts - gap, position_ts]. */
        int64_t midnight = local_day_start(s->position_ts);
        if (t->cur_pos_ts < midnight) {
            int64_t before = midnight - (s->position_ts - gap);
            if (before < 0)
                before = 0;
            if (before > gap)
                before = gap;
            const int pages_before =
                gap > 0 ? (int)((int64_t)pages * before / gap) : 0;
            if (before > 0)
                update_session(t, s->bookid, t->cur_row_start, midnight - 1,
                               before, pages_before, s->cpage);
            /* The new row is opened empty and ending at midnight: the
             * update below is what credits it, and it only credits a row whose
             * end it actually moves forward. */
            pb_state split = *s;
            split.position_ts = midnight;
            insert_session(t, &split, midnight, 0, 0, 0, 0);
            t->cur_row_start = midnight;
            gap -= before;
            pages -= pages_before;
        }

        update_session(t, s->bookid, t->cur_row_start, s->position_ts, gap,
                       pages, s->cpage);
        upsert_book(t, s);
        t->cur_pos_ts = s->position_ts;
        t->cur_pages = s->cpage;
        t->cur_sleep = sleep_total;
        return 2;
    }
    return 0;
}
