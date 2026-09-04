/* Host test: session derivation from explorer-3-like snapshots. */
#include "../src/tracker.h"
#include "../src/stats_db.h"
#include "../src/version.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define EXP_DB "/tmp/bs_test_explorer.db"
#define ST_DB "/tmp/bs_test_stats.db"

static void ex(sqlite3 *db, const char *sql)
{
    char *err = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "FAIL sql: %s\n%s\n", err ? err : "?", sql);
        exit(1);
    }
}

static sqlite3 *make_explorer(void)
{
    unlink(EXP_DB);
    sqlite3 *db;
    assert(sqlite3_open(EXP_DB, &db) == SQLITE_OK);
    ex(db, "CREATE TABLE books_impl (id INTEGER PRIMARY KEY, title TEXT, author TEXT);"
           "CREATE TABLE files (book_id INTEGER, storageid INTEGER, fast_hash BLOB);"
           "CREATE TABLE books_fast_hashes (fast_hash BLOB PRIMARY KEY, book_id INTEGER);"
           "CREATE TABLE books_settings (bookid INTEGER, profileid INTEGER,"
           " position TEXT, position_ts INTEGER, cpage INTEGER, npage INTEGER,"
           " opentime INTEGER, completed INTEGER);");
    ex(db, "INSERT INTO books_impl VALUES (7,'Testbuch','Autorin');"
           "INSERT INTO files VALUES (7,1,x'aabb');"
           "INSERT INTO books_fast_hashes VALUES (x'aabb',7);"
           "INSERT INTO books_settings VALUES (7,1,'p',1000,10,300,1000,0);");
    /* A second book, parked out of every query (`opentime > 0` selects state)
     * until a test needs something other than book 7 to be the open one. */
    ex(db, "INSERT INTO books_impl VALUES (8,'Zweitbuch','Autorin');"
           "INSERT INTO books_settings VALUES (8,1,'p',0,0,100,0,0);");
    return db;
}

/* Makes book 8 the most recently opened one, which is how a real device leaves
 * an older session behind: books_settings keeps only the latest open per book,
 * so only the newest of them can still be running. 0 parks it again. */
static void set_other_open(sqlite3 *db, long open)
{
    char sql[256];
    snprintf(sql, sizeof(sql),
             "UPDATE books_settings SET opentime=%ld, position_ts=%ld, cpage=1"
             " WHERE bookid=8", open, open ? open + 1 : 0);
    ex(db, sql);
}

static void set_state(sqlite3 *db, long open, long pos, int cpage)
{
    char sql[256];
    snprintf(sql, sizeof(sql),
             "UPDATE books_settings SET opentime=%ld, position_ts=%ld, cpage=%d"
             " WHERE bookid=7", open, pos, cpage);
    ex(db, sql);
}

/* Moves the "measured from here on" marker the stats filter by. */
static void set_since(sqlite3 *db, const char *value_sql)
{
    char sql[256];
    snprintf(sql, sizeof(sql),
             "INSERT INTO meta (key,value) VALUES ('tracking_since', %s)"
             " ON CONFLICT(key) DO UPDATE SET value = %s", value_sql, value_sql);
    ex(db, sql);
}

/* Today at noon: scenarios that need real timestamps must not straddle a local
 * midnight, and a session opened in 1970 would be split at every one since. */
static long today_noon(void)
{
    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    lt.tm_hour = 12;
    lt.tm_min = 0;
    lt.tm_sec = 0;
    return (long)mktime(&lt);
}

/* Local midnight of the day the test runs in, so a scenario that has to
 * straddle one does not depend on the machine's time zone. */
static long today_midnight(void)
{
    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    lt.tm_hour = 0;
    lt.tm_min = 0;
    lt.tm_sec = 0;
    lt.tm_isdst = -1;
    return (long)mktime(&lt);
}

static long q1(sqlite3 *db, const char *sql)
{
    sqlite3_stmt *st;
    long v = -999;
    assert(sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK);
    if (sqlite3_step(st) == SQLITE_ROW)
        v = (long)sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return v;
}

/* The streak on the About screen: days in a row with reading, counted back
 * from today. Sessions are written straight into the stats DB — what is being
 * tested is the walk over the days, not how they got there. */
static void test_streak(void)
{
    const char *db_path = "/tmp/bs_test_streak.db";
    unlink(db_path);
    tracker t;
    assert(tracker_init(&t, db_path, EXP_DB) == 0);
    /* Everything the walk sees has to be inside the tracked window. */
    set_since(t.stats, "0");

    const long noon = today_noon();
    const long day = 86400;
    char sql[256];
    overall_stats o;

    /* Nothing at all is no streak, not a crash. */
    assert(stats_overall(t.stats, &o) == 0 && o.streak_days == 0);

    /* Today, yesterday, the day before: three. */
    for (int i = 0; i < 3; i++) {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO sessions (book_id,start_time,end_time,"
                 " active_seconds,pages_start,pages_end,recovered,pages_read)"
                 " VALUES (7,%ld,%ld,600,1,5,0,4)",
                 noon - i * day - 600, noon - i * day);
        ex(t.stats, sql);
    }
    assert(stats_overall(t.stats, &o) == 0 && o.streak_days == 3);

    /* A fourth day, but with the fifth missing: the gap ends the run. */
    snprintf(sql, sizeof(sql),
             "INSERT INTO sessions (book_id,start_time,end_time,"
             " active_seconds,pages_start,pages_end,recovered,pages_read)"
             " VALUES (7,%ld,%ld,600,1,5,0,4)",
             noon - 4 * day - 600, noon - 4 * day);
    ex(t.stats, sql);
    assert(stats_overall(t.stats, &o) == 0 && o.streak_days == 3);

    /* Nothing read yet today: the run still stands, counted from yesterday.
     * Otherwise every morning would report a broken streak. */
    snprintf(sql, sizeof(sql), "DELETE FROM sessions WHERE end_time = %ld", noon);
    ex(t.stats, sql);
    assert(stats_overall(t.stats, &o) == 0 && o.streak_days == 2);

    /* A minute is the floor: seconds spent opening a book to look something up
     * are not a day of reading, and they must not carry a streak either. */
    ex(t.stats, "UPDATE sessions SET active_seconds = 30");
    assert(stats_overall(t.stats, &o) == 0 && o.streak_days == 0);

    /* Two days back is already too far to be a run that reaches today. */
    ex(t.stats, "UPDATE sessions SET active_seconds = 600");
    snprintf(sql, sizeof(sql), "DELETE FROM sessions WHERE end_time = %ld",
             noon - day);
    ex(t.stats, sql);
    assert(stats_overall(t.stats, &o) == 0 && o.streak_days == 0);

    tracker_close(&t);
    unlink(db_path);
}

/* The hand-set totals: an offset in `meta`, and nothing else moved.
 *
 * What the Overview does with the offset is C++ and has no host build, so what
 * is checked here is the half that can be: the value survives a round trip,
 * an absent key reads as no adjustment, and a stored offset leaves every
 * measured figure exactly where it was. */
static void test_manual_totals(void)
{
    const char *db_path = "/tmp/bs_test_totals.db";
    unlink(db_path);
    tracker t;
    assert(tracker_init(&t, db_path, EXP_DB) == 0);
    set_since(t.stats, "0");

    const long noon = today_noon();
    char sql[256];
    snprintf(sql, sizeof(sql),
             "INSERT INTO sessions (book_id,start_time,end_time,"
             " active_seconds,pages_start,pages_end,recovered,pages_read)"
             " VALUES (7,%ld,%ld,3600,1,30,0,29)", noon - 3600, noon);
    ex(t.stats, sql);

    overall_stats before;
    assert(stats_overall(t.stats, &before) == 0);
    assert(before.total_hours > 0.99 && before.total_hours < 1.01);

    /* A key nobody wrote is no adjustment. */
    assert(stats_meta_int(t.stats, META_OFFSET_SECONDS) == 0);
    assert(stats_meta_int(t.stats, META_OFFSET_BOOKS) == 0);

    /* Ten hours read on another device, and three books with them. */
    assert(stats_meta_set_int(t.stats, META_OFFSET_SECONDS, 10 * 3600) == 0);
    assert(stats_meta_set_int(t.stats, META_OFFSET_BOOKS, 3) == 0);
    assert(stats_meta_int(t.stats, META_OFFSET_SECONDS) == 10 * 3600);
    assert(stats_meta_int(t.stats, META_OFFSET_BOOKS) == 3);

    /* The measurement is untouched: the offset is added where the card is
     * built, and the pace, today and the streak never see it. */
    overall_stats after;
    assert(stats_overall(t.stats, &after) == 0);
    assert(after.total_hours == before.total_hours);
    assert(after.today_secs == before.today_secs);
    assert(after.pages_per_min == before.pages_per_min);
    assert(after.streak_days == before.streak_days);

    /* Written twice is set, not summed — the dialog stores a target, not a
     * stream of steps. */
    assert(stats_meta_set_int(t.stats, META_OFFSET_SECONDS, 20 * 3600) == 0);
    assert(stats_meta_int(t.stats, META_OFFSET_SECONDS) == 20 * 3600);

    /* Reset is a zero, and it survives the reopen every catchUp() does. */
    assert(stats_meta_set_int(t.stats, META_OFFSET_SECONDS, 0) == 0);
    tracker_close(&t);
    assert(tracker_init(&t, db_path, EXP_DB) == 0);
    assert(stats_meta_int(t.stats, META_OFFSET_SECONDS) == 0);
    assert(stats_meta_int(t.stats, META_OFFSET_BOOKS) == 3);

    tracker_close(&t);
    unlink(db_path);
}

/* Update check: tag names from GitHub releases against our own VERSION. */
static void test_version_compare(void)
{
    assert(version_compare("0.5.1", "0.5.1") == 0);
    assert(version_compare("v0.5.1", "0.5.1") == 0);   /* the tag keeps its v */
    assert(version_compare("0.5.1", "v0.5.2") == -1);
    assert(version_compare("v0.6.0", "0.5.9") == 1);
    assert(version_compare("1.2", "1.2.0") == 0);      /* missing part is zero */
    assert(version_compare("0.10.0", "0.9.0") == 1);   /* numeric, not textual */
    /* Release candidates order below their own release and among themselves,
     * so a device on the pre-release channel is offered rc2 after rc1 and the
     * final release after that. */
    assert(version_compare("v1.4.0-rc1", "1.4.0") == -1);
    assert(version_compare("1.4.0", "v1.4.0-rc9") == 1);
    assert(version_compare("1.4.0-rc1", "1.4.0-rc2") == -1);
    assert(version_compare("1.4.0-rc2", "1.4.0-rc2") == 0);
    assert(version_compare("1.4.0-rc2", "1.3.9") == 1);
    assert(version_compare("1.4.0-rc", "1.4.0-rc1") == 0); /* bare rc is the first */
    assert(version_compare("", "0.0.1") == -1);        /* garbage reads as 0.0.0 */
}

int main(void)
{
    /* Fixed zone: the day-boundary logic is local-time based. */
    setenv("TZ", "Europe/Berlin", 1);
    tzset();

    sqlite3 *exp = make_explorer();
    unlink(ST_DB);

    tracker t;
    assert(tracker_init(&t, ST_DB, EXP_DB) == 0);

    /* read_state returns the book incl. metadata + cover hash */
    pb_state s;
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(s.bookid == 7 && s.opentime == 1000 && s.cpage == 10);
    assert(strcmp(s.title, "Testbuch") == 0);
    assert(strcmp(s.cover, "aabb") == 0);

    /* The cover key must outlive the file: a finished book is usually deleted,
     * and the key is all that can still find a thumbnail for it. */
    ex(exp, "DELETE FROM files WHERE book_id = 7");
    pb_state deleted;
    assert(tracker_read_state(EXP_DB, &deleted) == 0);
    assert(strcmp(deleted.cover, "aabb") == 0);
    ex(exp, "INSERT INTO files VALUES (7,1,x'aabb')");

    /* New session observed: active = pos-open (0 here), pages_start = cpage */
    assert(tracker_observe(&t, &s) == 1);
    assert(q1(t.stats, "SELECT COUNT(*) FROM sessions") == 1);
    assert(q1(t.stats, "SELECT active_seconds FROM sessions") == 0);
    assert(q1(t.stats, "SELECT pages_start FROM sessions") == 10);

    /* Page turn after 60s: active += 60 */
    set_state(exp, 1000, 1060, 12);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 2);
    assert(q1(t.stats, "SELECT active_seconds FROM sessions") == 60);
    assert(q1(t.stats, "SELECT pages_end FROM sessions") == 12);

    /* 1h standby, then a single page turn: one page is all the evidence there
     * is, so the gap is worth one page and no more. */
    set_state(exp, 1000, 1060 + 3600, 13);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    tracker_observe(&t, &s);
    assert(q1(t.stats, "SELECT active_seconds FROM sessions") == 60 + SECONDS_PER_PAGE_CAP);
    assert(q1(t.stats, "SELECT end_time FROM sessions") == 4660);

    /* No new position_ts -> nothing happens */
    assert(tracker_observe(&t, &s) == 0);
    assert(q1(t.stats, "SELECT active_seconds FROM sessions") == 60 + SECONDS_PER_PAGE_CAP);

    /* New open = new session; pages_start = pages_end of the old one */
    set_state(exp, 9000, 9005, 14);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 1);
    assert(q1(t.stats, "SELECT COUNT(*) FROM sessions") == 2);
    assert(q1(t.stats, "SELECT pages_start FROM sessions WHERE start_time=9000") == 13);
    assert(q1(t.stats, "SELECT active_seconds FROM sessions WHERE start_time=9000") == 5);

    /* Reopened where it was left: the firmware stamps opentime now but keeps
     * the older position_ts, so the open — not that stale timestamp — starts
     * the clock. Otherwise the row ends before it begins and the next page
     * turn is measured from hours ago, landing as a capped ten minutes. */
    tracker_close(&t);
    assert(tracker_init(&t, ST_DB, EXP_DB) == 0);
    set_state(exp, 30000, 25000, 50);       /* position_ts 5000 s before open */
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 1);
    assert(q1(t.stats, "SELECT end_time FROM sessions WHERE start_time=30000") == 30000);
    assert(q1(t.stats, "SELECT active_seconds FROM sessions WHERE start_time=30000") == 0);
    set_state(exp, 30000, 30090, 52);       /* first page turn, 90 s in */
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 2);
    assert(q1(t.stats, "SELECT active_seconds FROM sessions WHERE start_time=30000") == 90);

    /* Recovery: session created without a daemon, backfilled + dedupe. Book 8
     * is opened afterwards, so book 7's session is over and is a candidate —
     * the one still open is the daemon's to measure, not to reconstruct. */
    tracker_close(&t);
    set_state(exp, 20000, 20000 + 1200, 40);
    set_other_open(exp, 60000);
    assert(tracker_init(&t, ST_DB, EXP_DB) == 0);
    assert(tracker_recover(&t) >= 1);
    assert(q1(t.stats, "SELECT COUNT(*) FROM sessions") == 4);
    assert(q1(t.stats, "SELECT active_seconds FROM sessions WHERE start_time=20000") == 1200);
    assert(q1(t.stats, "SELECT recovered FROM sessions WHERE start_time=20000") == 1);
    /* Recovered = estimate: no pages_start, so it cannot skew pages/minute */
    assert(q1(t.stats, "SELECT pages_start IS NULL FROM sessions WHERE start_time=20000") == 1);
    tracker_recover(&t); /* idempotent */
    assert(q1(t.stats, "SELECT COUNT(*) FROM sessions") == 4);

    /* Recovery cap against huge spans. Ten hours from 50000 crosses local
     * midnight, so the capped estimate is split across the two days it covers:
     * the cap bounds the reconstruction, not each row it lands in. */
    set_state(exp, 50000, 50000 + 10 * 3600, 60);
    tracker_recover(&t);
    assert(q1(t.stats, "SELECT SUM(active_seconds) FROM sessions"
                       " WHERE recovered=1 AND start_time>=50000") ==
           RECOVERED_CAP_SECONDS);
    assert(q1(t.stats, "SELECT COUNT(DISTINCT date(end_time,'unixepoch','localtime'))"
                       " FROM sessions WHERE recovered=1 AND start_time>=50000") == 2);
    /* The tail row starts at that midnight, which the live-session scenario
     * further down reuses; drop it so the two do not share a primary key. */
    ex(t.stats, "DELETE FROM sessions WHERE recovered=1 AND start_time>50000");

    /* Rows written by older versions get retrofitted on open */
    ex(t.stats, "UPDATE sessions SET active_seconds=6*3600, pages_start=5"
                " WHERE start_time=50000");
    tracker_close(&t);
    assert(tracker_init(&t, ST_DB, EXP_DB) == 0);
    assert(q1(t.stats, "SELECT active_seconds FROM sessions WHERE start_time=50000") ==
           RECOVERED_CAP_SECONDS);
    assert(q1(t.stats, "SELECT pages_start IS NULL FROM sessions WHERE start_time=50000") == 1);

    /* The session that is open right now is the daemon's to measure: the shim
     * starts it seconds after a book is opened, and stamping that session as a
     * reconstruction dropped the whole evening from every average. */
    set_other_open(exp, 0);
    set_state(exp, 70000, 70000 + 600, 70);
    const int before_live = (int)q1(t.stats, "SELECT COUNT(*) FROM sessions");
    tracker_recover(&t);
    assert(q1(t.stats, "SELECT COUNT(*) FROM sessions") == before_live);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 1);
    assert(q1(t.stats, "SELECT recovered FROM sessions WHERE start_time=70000") == 0);
    assert(q1(t.stats, "SELECT active_seconds FROM sessions WHERE start_time=70000") == 600);

    /* Session across local midnight is split so both days get their time */
    set_state(exp, 82500, 82500, 20); /* 1970-01-01 23:55 CET */
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 1);
    set_state(exp, 82500, 82900, 22); /* 1970-01-02 00:01:40 CET */
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 2);
    assert(q1(t.stats, "SELECT active_seconds FROM sessions WHERE start_time=82500") == 300);
    assert(q1(t.stats, "SELECT active_seconds FROM sessions WHERE start_time=82800") == 100);
    assert(q1(t.stats, "SELECT date(end_time,'unixepoch','localtime')='1970-01-01'"
                       " FROM sessions WHERE start_time=82500") == 1);
    assert(q1(t.stats, "SELECT date(end_time,'unixepoch','localtime')='1970-01-02'"
                       " FROM sessions WHERE start_time=82800") == 1);
    /* the new row continues the page count instead of restarting it */
    assert(q1(t.stats, "SELECT pages_start FROM sessions WHERE start_time=82800") == 22);

    /* stats_overall computes without crashing and plausibly. Marker at 0 is
     * the pre-marker database: nothing gets filtered out. */
    overall_stats o;
    set_since(t.stats, "0");
    assert(stats_overall(t.stats, &o) == 0);
    assert(o.total_hours > 0);

    /* Everything before tracking started is reconstructed from the firmware's
     * last-open timestamps, not measured, so no view may count it. */
    ex(t.stats, "DELETE FROM sessions");
    set_since(t.stats, "strftime('%s','now','-1 days')");
    ex(t.stats, "INSERT INTO sessions (book_id,start_time,end_time,active_seconds,recovered)"
                " VALUES (7, strftime('%s','now','-5 days'),"
                "            strftime('%s','now','-5 days'), 3600, 1),"
                "        (7, strftime('%s','now'), strftime('%s','now'), 1800, 0)");
    assert(stats_overall(t.stats, &o) == 0);
    assert(o.total_hours > 0.49 && o.total_hours < 0.51); /* only the 1800 s row */
    assert(stats_tracking_since(t.stats) > 0);

    /* A tracker that has just been created must resume the session it finds
     * rather than start beside it. Every caller builds one sooner or later —
     * the daemon after a restart, and catchUp on every refresh — and until it
     * did, the stretch since the last observation was dropped rather than
     * capped. Real timestamps, so today's noon anchors it clear of midnight. */
    tracker_close(&t);
    assert(tracker_init(&t, ST_DB, EXP_DB) == 0);
    ex(t.stats, "DELETE FROM sessions");
    char sql[256];
    const long t0 = today_noon();
    set_state(exp, t0, t0 + 30, 177);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 1);
    snprintf(sql, sizeof(sql),
             "SELECT active_seconds FROM sessions WHERE start_time=%ld", t0);
    assert(q1(t.stats, sql) == 30);

    /* 18 minutes and 30 pages with nothing watching, then the app is opened:
     * catchUp observes once, from a tracker that knows nothing yet. */
    tracker_close(&t);
    set_state(exp, t0, t0 + 30 + 1080, 207);
    assert(tracker_init(&t, ST_DB, EXP_DB) == 0);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 2);
    assert(q1(t.stats, sql) == 30 + 1080);
    assert(q1(t.stats, "SELECT COUNT(*) FROM sessions") == 1);
    assert(q1(t.stats, "SELECT pages_end FROM sessions") == 207);

    /* The same eighteen minutes with one page turned is not reading: the
     * pages are the evidence, and one page is worth one page. */
    tracker_close(&t);
    assert(tracker_init(&t, ST_DB, EXP_DB) == 0);
    ex(t.stats, "DELETE FROM sessions");
    set_state(exp, t0, t0 + 30, 177);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 1);
    tracker_close(&t);
    set_state(exp, t0, t0 + 30 + 1080, 178);
    assert(tracker_init(&t, ST_DB, EXP_DB) == 0);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 2);
    assert(q1(t.stats, sql) == 30 + SECONDS_PER_PAGE_CAP);
    ex(t.stats, "DELETE FROM sessions");
    tracker_close(&t);

    /* A session first seen after it has already crossed midnight must be split
     * as well. Reading from 23:30 to 00:03 with nothing watching (the daemon is
     * not scheduled while a book is on screen), then the app is opened: the
     * whole 33 minutes used to land on the new day, which showed 33 minutes
     * read "today" at three minutes past midnight. */
    const long mid = today_midnight();
    assert(tracker_init(&t, ST_DB, EXP_DB) == 0);
    ex(t.stats, "DELETE FROM sessions");
    /* An earlier session of the same book, so the 30 pages turned since then
     * are the evidence the credit is measured against. */
    snprintf(sql, sizeof(sql),
             "INSERT INTO sessions (book_id,start_time,end_time,active_seconds,"
             " pages_start,pages_end,recovered) VALUES (7,%ld,%ld,600,15,20,0)",
             mid - 90000, mid - 89400);
    ex(t.stats, sql);
    set_state(exp, mid - 1800, mid + 180, 50); /* 23:30 -> 00:03, 30 pages */
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 1);
    snprintf(sql, sizeof(sql),
             "SELECT active_seconds FROM sessions WHERE start_time=%ld", mid - 1800);
    assert(q1(t.stats, sql) == 1800);
    snprintf(sql, sizeof(sql),
             "SELECT active_seconds FROM sessions WHERE start_time=%ld", mid);
    assert(q1(t.stats, sql) == 180);
    assert(q1(t.stats, "SELECT COUNT(*) FROM sessions") == 3);
    /* the two new rows land on two different days */
    snprintf(sql, sizeof(sql),
             "SELECT COUNT(DISTINCT date(end_time,'unixepoch','localtime'))"
             " FROM sessions WHERE start_time>=%ld", mid - 1800);
    assert(q1(t.stats, sql) == 2);
    /* and the row left open is the one the next observation extends */
    set_state(exp, mid - 1800, mid + 300, 52);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 2);
    snprintf(sql, sizeof(sql),
             "SELECT active_seconds FROM sessions WHERE start_time=%ld", mid);
    assert(q1(t.stats, sql) == 300);

    /* Rows already written that way are cut once, on open: the evening they
     * belong to is not corrected by any later observation. The marker is
     * cleared here because an earlier tracker_init in this test already
     * stamped it. */
    ex(t.stats, "DELETE FROM sessions");
    snprintf(sql, sizeof(sql),
             "INSERT INTO sessions (book_id,start_time,end_time,active_seconds,"
             " pages_start,pages_end,recovered) VALUES (7,%ld,%ld,1980,20,50,0)",
             mid - 1800, mid + 180);
    ex(t.stats, sql);
    ex(t.stats, "DELETE FROM meta WHERE key='midnight_split_1_6_3'");
    tracker_close(&t);
    assert(tracker_init(&t, ST_DB, EXP_DB) == 0);
    snprintf(sql, sizeof(sql),
             "SELECT active_seconds FROM sessions WHERE start_time=%ld", mid - 1800);
    assert(q1(t.stats, sql) == 1800);
    snprintf(sql, sizeof(sql),
             "SELECT end_time FROM sessions WHERE start_time=%ld", mid - 1800);
    assert(q1(t.stats, sql) == mid - 1);
    snprintf(sql, sizeof(sql),
             "SELECT active_seconds FROM sessions WHERE start_time=%ld", mid);
    assert(q1(t.stats, sql) == 180);
    snprintf(sql, sizeof(sql),
             "SELECT pages_start FROM sessions WHERE start_time=%ld", mid);
    assert(q1(t.stats, sql) == 50); /* the pages all belong to the first row */
    assert(q1(t.stats, "SELECT SUM(active_seconds) FROM sessions") == 1980);
    /* and it is a one-shot: a second open leaves the two rows alone */
    tracker_close(&t);
    assert(tracker_init(&t, ST_DB, EXP_DB) == 0);
    assert(q1(t.stats, "SELECT COUNT(*) FROM sessions") == 2);
    assert(q1(t.stats, "SELECT SUM(active_seconds) FROM sessions") == 1980);

    ex(t.stats, "DELETE FROM sessions");
    tracker_close(&t);
    snprintf(sql, sizeof(sql),
             "SELECT active_seconds FROM sessions WHERE start_time=%ld", t0);

    /* Two trackers on one session, which is the normal case on the device: the
     * daemon's lives across polls, the app builds a fresh one on every refresh,
     * and a second daemon gets spawned whenever the pidfile check misses one.
     * Each must credit a stretch only as far as the row has not been paid for
     * already — measuring from its own last-seen position billed the same
     * minutes twice, and three sessions in one evening came out at exactly
     * twice the wall clock they spanned. */
    tracker daemon, refresh;
    assert(tracker_init(&daemon, ST_DB, EXP_DB) == 0);
    assert(tracker_init(&refresh, ST_DB, EXP_DB) == 0);
    set_state(exp, t0, t0 + 30, 177);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&daemon, &s) == 1);

    set_state(exp, t0, t0 + 630, 187);   /* 10 pages over the next 10 minutes */
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&refresh, &s) == 2);
    assert(q1(daemon.stats, sql) == 630);
    assert(tracker_observe(&daemon, &s) == 0); /* same stretch, already paid */
    assert(q1(daemon.stats, sql) == 630);

    /* And the overtaken tracker keeps measuring from where the row now stands,
     * not from the position it still remembered. */
    set_state(exp, t0, t0 + 1230, 197);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&daemon, &s) == 2);
    assert(q1(daemon.stats, sql) == 1230);
    snprintf(sql, sizeof(sql),
             "SELECT active_seconds <= end_time - start_time FROM sessions"
             " WHERE start_time=%ld", t0);
    assert(q1(daemon.stats, sql) == 1);
    tracker_close(&refresh);
    tracker_close(&daemon);

    /* Sleep is not read. A window between two firmware position saves is wall
     * clock and holds both the reading and the hours the reader lay closed;
     * whatever the daemon measured as suspend has to come off it before the
     * pages get to vouch for the rest. Drawn from a real evening: 75 minutes
     * between two saves, thirteen pages, the cover shut for an hour of it —
     * billed as 65 minutes of reading before this.
     *
     * The marks are relative to now on purpose. The guard being tested is that
     * suspend recorded *after* a save is not deducted from the window ending at
     * it, and that only means anything while the save is recent — which on the
     * device it always is, because the daemon polls within a poll interval of
     * one. A couple of seconds pass while the test runs, so the assertions
     * carry that much slack. */
    unlink(ST_DB);
    tracker sleepy;
    assert(tracker_init(&sleepy, ST_DB, EXP_DB) == 0);
    const long now = (long)time(NULL);
    const long base = now - 4500;
    set_state(exp, base, base, 100);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&sleepy, &s) == 1);

    /* What the poll loop would have accumulated across a closed cover. The
     * daemon derives this from the two clocks; the test states it outright,
     * because a host has no device to suspend. */
    ex(sleepy.stats, "INSERT OR REPLACE INTO meta (key,value)"
                     " VALUES ('sleep_total', 3600)");
    /* Saved 20 minutes ago, 13 pages on: of the hour of standby, only the
     * 40 minutes that can have fallen inside the window are deducted. */
    set_state(exp, base, now - 1200, 113);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&sleepy, &s) == 2);
    /* Split-invariant: a run started within 75 minutes of local midnight puts
     * the session in two rows, and the split moves seconds without making any. */
    snprintf(sql, sizeof(sql),
             "SELECT SUM(active_seconds) FROM sessions WHERE book_id=7");
    long got = q1(sleepy.stats, sql);
    assert(got >= 897 && got <= 900); /* 3300 window - 2400 asleep */

    /* The same standby is never deducted twice: the row carries the total it
     * was billed against, and this window is measured from there. */
    set_state(exp, base, now, 118);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&sleepy, &s) == 2);
    got = q1(sleepy.stats, sql);
    assert(got >= 2097 && got <= 2100); /* + a clean 1200 s window */
    tracker_close(&sleepy);

    /* A footnote link is not reading. Books that gather their notes at the
     * back are read by tapping into them and tapping back; the firmware then
     * saves a position hundreds of pages from the last one, and that distance
     * used to vouch for the whole window and go into the pace as pages read —
     * over 7000 of them in one day on a real device (issue #2). */
    unlink(ST_DB);
    tracker jumper;
    assert(tracker_init(&jumper, ST_DB, EXP_DB) == 0);
    set_state(exp, t0, t0 + 30, 100);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&jumper, &s) == 1);
    snprintf(sql, sizeof(sql),
             "SELECT active_seconds FROM sessions WHERE start_time=%ld", t0);

    /* Twenty minutes on, 25 pages: ordinary reading, credited whole. */
    set_state(exp, t0, t0 + 1230, 125);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&jumper, &s) == 2);
    assert(q1(jumper.stats, sql) == 1230);
    assert(q1(jumper.stats, "SELECT pages_read FROM sessions") == 25);

    /* Then a note at the back of the book, 800 pages away, saved twenty
     * minutes later: neither the pages nor the window are reading. */
    set_state(exp, t0, t0 + 2430, 925);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&jumper, &s) == 2);
    assert(q1(jumper.stats, sql) == 1230);
    assert(q1(jumper.stats, "SELECT pages_read FROM sessions") == 25);
    /* The position itself still follows the reader — the way back would
     * otherwise be measured from a page nobody is on. */
    assert(q1(jumper.stats, "SELECT pages_end FROM sessions") == 925);

    /* Back to where the reading was (a move backwards, worth nothing) and on
     * from there, which is credited as it always was. */
    set_state(exp, t0, t0 + 2460, 126);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&jumper, &s) == 2);
    set_state(exp, t0, t0 + 3060, 136);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&jumper, &s) == 2);
    assert(q1(jumper.stats, sql) == 1830);
    assert(q1(jumper.stats, "SELECT pages_read FROM sessions") == 35);

    /* A handful of pages between two saves seconds apart is not a jump: the
     * rate rule only applies to a move big enough to be one. */
    set_state(exp, t0, t0 + 3065, 139);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&jumper, &s) == 2);
    assert(q1(jumper.stats, "SELECT pages_read FROM sessions") == 38);

    /* And the pace is built on that column, so it stays a reading speed. */
    set_since(jumper.stats, "0");
    assert(stats_overall(jumper.stats, &o) == 0);
    assert(o.pages_per_min > 1.0 && o.pages_per_min < 1.5);

    /* Pages are credited only where seconds are. A session first seen with no
     * elapsed span — reopened where it was left, so the firmware's position_ts
     * is still the previous session's — buys no time, and pages with no
     * minutes beside them are what makes a pace lie. */
    set_state(exp, t0 + 4000, t0 + 3900, 141);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&jumper, &s) == 1);
    snprintf(sql, sizeof(sql),
             "SELECT active_seconds FROM sessions WHERE start_time=%ld",
             t0 + 4000);
    assert(q1(jumper.stats, sql) == 0);
    snprintf(sql, sizeof(sql),
             "SELECT pages_read FROM sessions WHERE start_time=%ld", t0 + 4000);
    assert(q1(jumper.stats, sql) == 0);

    /* Databases written before pages_read existed keep their pace: the column
     * is seeded once from the two positions. Only once — a window that was all
     * jump must not be handed its distance back on the next open. */
    ex(jumper.stats, "DELETE FROM sessions");
    ex(jumper.stats, "INSERT INTO sessions (book_id,start_time,end_time,"
                     " active_seconds,pages_start,pages_end,recovered,pages_read)"
                     " VALUES (7,100,700,600,10,40,0,0)");
    ex(jumper.stats, "DELETE FROM meta WHERE key='pages_read_1_6_5'");
    tracker_close(&jumper);
    assert(tracker_init(&jumper, ST_DB, EXP_DB) == 0);
    assert(q1(jumper.stats, "SELECT pages_read FROM sessions") == 30);
    ex(jumper.stats, "UPDATE sessions SET pages_read = 0");
    tracker_close(&jumper);
    assert(tracker_init(&jumper, ST_DB, EXP_DB) == 0);
    assert(q1(jumper.stats, "SELECT pages_read FROM sessions") == 0);
    tracker_close(&jumper);

    test_streak();
    test_manual_totals();
    test_version_compare();

    printf("all tracker tests ok\n");
    return 0;
}
