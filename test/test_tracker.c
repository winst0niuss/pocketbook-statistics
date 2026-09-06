/* Host test: session derivation from explorer-3-like snapshots. */
#include "../src/tracker.h"
#include "../src/daemon.h"
#include "../src/log.h"
#include "../src/stats_db.h"
#include "../src/version.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

/* Every scenario below that says "today" asks the C library for it, while the
 * aggregates ask SQLite ('now', 'localtime'). The two agree except across the
 * instant the day changes, and a run that starts a moment before midnight
 * finishes in a day the sessions were not written into: the streak then counts
 * from yesterday and comes out one short. The window is a fraction of a second
 * wide and the suite takes about a tenth of one, so simply stand back from it.
 *
 * Waiting is the honest fix here. Freezing the clock would leave the day-level
 * SQL — the part that actually decides what a day is — untested against a real
 * one. */
static void avoid_midnight(void)
{
    for (;;) {
        const time_t now = time(NULL);
        struct tm lt;
        localtime_r(&now, &lt);
        const int left = (23 - lt.tm_hour) * 3600 + (59 - lt.tm_min) * 60
                         + (60 - lt.tm_sec);
        if (left > 5)
            return;
        sleep((unsigned)left + 1);
    }
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

/* Switched off inside a book. The firmware saves the position on the way down
 * and stamps a fresh opentime on the way up, so the save is older than the open
 * that follows it. Taken from a real device, where 14 minutes and 10 turned
 * pages arrived as a day with no reading at all: the new session's own window
 * is zero seconds wide, and ten pages inside it read as a jump. */
static void test_reopen_after_poweroff(sqlite3 *exp)
{
    const char *db_path = "/tmp/bs_test_reopen.db";
    unlink(db_path);
    tracker t;
    assert(tracker_init(&t, db_path, EXP_DB) == 0);
    set_since(t.stats, "0");

    const long t0 = today_noon();
    char sql[256];

    /* The book is opened and the firmware saves the position in the same
     * second: a session with nothing in it yet. */
    set_state(exp, t0, t0, 123);
    pb_state s;
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 1);
    assert(q1(t.stats, "SELECT COUNT(*) FROM sessions") == 1);

    /* 14 minutes of reading, ten pages, and the one save that recorded it
     * lands while nothing is polling — the reader is on its way off. Then it
     * comes back up: a fresh daemon, and an opentime stamped after that save. */
    tracker_close(&t);
    assert(tracker_init(&t, db_path, EXP_DB) == 0);
    set_state(exp, t0 + 890, t0 + 838, 133);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 1);

    /* The stretch belongs to the row that was open when it was read. */
    snprintf(sql, sizeof(sql),
             "SELECT active_seconds FROM sessions WHERE start_time=%ld", t0);
    assert(q1(t.stats, sql) == 838);
    snprintf(sql, sizeof(sql),
             "SELECT pages_read FROM sessions WHERE start_time=%ld", t0);
    assert(q1(t.stats, sql) == 10);
    /* The reopen is still a session of its own, and it starts empty. */
    assert(q1(t.stats, "SELECT COUNT(*) FROM sessions") == 2);
    snprintf(sql, sizeof(sql),
             "SELECT active_seconds FROM sessions WHERE start_time=%ld",
             t0 + 890);
    assert(q1(t.stats, sql) == 0);

    /* Seen twice is paid once: update_session only moves a row's end forward,
     * and the app's catchUp() observes the same state seconds later. */
    assert(tracker_read_state(EXP_DB, &s) == 0);
    tracker_observe(&t, &s);
    snprintf(sql, sizeof(sql),
             "SELECT active_seconds FROM sessions WHERE start_time=%ld", t0);
    assert(q1(t.stats, sql) == 838);
    assert(q1(t.stats, "SELECT COUNT(*) FROM sessions") == 2);

    /* A move backwards before the reopen buys nothing either: the reader went
     * to a bookmark, and what is read from there is credited by the
     * observations that follow. */
    tracker_close(&t);
    unlink(db_path);
    assert(tracker_init(&t, db_path, EXP_DB) == 0);
    set_since(t.stats, "0");
    set_state(exp, t0, t0, 123);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 1);
    tracker_close(&t);
    assert(tracker_init(&t, db_path, EXP_DB) == 0);
    set_state(exp, t0 + 890, t0 + 838, 40);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 1);
    snprintf(sql, sizeof(sql),
             "SELECT active_seconds FROM sessions WHERE start_time=%ld", t0);
    assert(q1(t.stats, sql) == 0);

    /* A jump before the reopen is still navigation: 300 pages in a minute is
     * a link to the notes at the back, not a minute of reading. */
    tracker_close(&t);
    unlink(db_path);
    assert(tracker_init(&t, db_path, EXP_DB) == 0);
    set_since(t.stats, "0");
    set_state(exp, t0, t0, 123);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 1);
    tracker_close(&t);
    assert(tracker_init(&t, db_path, EXP_DB) == 0);
    set_state(exp, t0 + 120, t0 + 60, 423);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 1);
    snprintf(sql, sizeof(sql),
             "SELECT active_seconds FROM sessions WHERE start_time=%ld", t0);
    assert(q1(t.stats, sql) == 0);

    tracker_close(&t);
    unlink(db_path);
}

/* The same power-off, across a local midnight. The stretch belongs to two days
 * and has to be written as two rows: every day-level figure groups by
 * date(end_time), so one row holding both would move the whole evening onto
 * the new day. */
static void test_reopen_across_midnight(sqlite3 *exp)
{
    const char *db_path = "/tmp/bs_test_reopen_mid.db";
    unlink(db_path);
    tracker t;
    assert(tracker_init(&t, db_path, EXP_DB) == 0);
    set_since(t.stats, "0");

    const long midnight = today_midnight();
    const long opened = midnight - 1200; /* 23:40, and reading from there */
    char sql[256];
    pb_state s;

    set_state(exp, opened, opened, 100);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 1);

    /* Switched off at 00:10 and back on at 00:15: the save predates the open,
     * 20 pages across half an hour that straddles the day boundary. */
    tracker_close(&t);
    assert(tracker_init(&t, db_path, EXP_DB) == 0);
    set_state(exp, midnight + 900, midnight + 600, 120);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&t, &s) == 1);

    /* 1800 seconds, split where the day is: 1200 before, 600 after. */
    snprintf(sql, sizeof(sql),
             "SELECT active_seconds FROM sessions WHERE start_time=%ld", opened);
    assert(q1(t.stats, sql) == 1200);
    snprintf(sql, sizeof(sql),
             "SELECT active_seconds FROM sessions WHERE start_time=%ld", midnight);
    assert(q1(t.stats, sql) == 600);
    /* Neither row carries seconds across the boundary. */
    snprintf(sql, sizeof(sql),
             "SELECT COUNT(*) FROM sessions WHERE date(end_time,'unixepoch','localtime')"
             " <> date(end_time - active_seconds + 1,'unixepoch','localtime')");
    assert(q1(t.stats, sql) == 0);
    /* And the pages went with the seconds, in the same proportion: two thirds
     * of the window fell before midnight, so two thirds of the pages did.
     * Checking only the sum would let a split that gives one side everything
     * pass, and then a day's pace would be built on pages nobody read in it. */
    snprintf(sql, sizeof(sql),
             "SELECT pages_read FROM sessions WHERE start_time=%ld", opened);
    const long pages_before = q1(t.stats, sql);
    snprintf(sql, sizeof(sql),
             "SELECT pages_read FROM sessions WHERE start_time=%ld", midnight);
    const long pages_after = q1(t.stats, sql);
    assert(pages_before == 13); /* 20 * 1200 / 1800, rounded down */
    assert(pages_after == 7);
    assert(pages_before + pages_after == 20);

    tracker_close(&t);
    unlink(db_path);
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

    /* The same rule on the Overview's own pace: an estimate is wall clock
     * reconstructed after the fact, so it may not divide into pages. */
    overall_stats est;
    ex(t.stats, "INSERT INTO sessions (book_id,start_time,end_time,"
                " active_seconds,pages_start,pages_end,recovered,pages_read)"
                " VALUES (7,20000,23600,3600,1,900,1,899)");
    assert(stats_overall(t.stats, &est) == 0);
    assert(est.pages_per_min == before.pages_per_min);
    assert(est.total_hours > before.total_hours); /* but the hours are real */
    ex(t.stats, "DELETE FROM sessions WHERE start_time=20000");

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

/* The year the streak screen draws. The day of the year is what indexes the
 * grid, so the ends of it and a leap year are where an off-by-one would live,
 * and a session from another year must not appear at all. */
static void test_year_days(void)
{
    const char *db_path = "/tmp/bs_test_year.db";
    unlink(db_path);
    tracker t;
    assert(tracker_init(&t, db_path, EXP_DB) == 0);
    set_since(t.stats, "0");

    /* 2024 was a leap year: 366 days, and 29 February is one of them. */
    struct tm d;
    memset(&d, 0, sizeof(d));
    d.tm_year = 124; /* 2024 */
    d.tm_mday = 1;
    d.tm_hour = 12;
    d.tm_isdst = -1;
    const long jan1 = (long)mktime(&d);
    d.tm_mon = 1;
    d.tm_mday = 29;
    d.tm_isdst = -1;
    const long feb29 = (long)mktime(&d);
    d.tm_mon = 11;
    d.tm_mday = 31;
    d.tm_isdst = -1;
    const long dec31 = (long)mktime(&d);
    /* And one the year does not own. */
    d.tm_year = 125;
    d.tm_mon = 0;
    d.tm_mday = 3;
    d.tm_isdst = -1;
    const long next_year = (long)mktime(&d);

    char sql[256];
    const long days[] = {jan1, feb29, dec31, next_year};
    for (unsigned i = 0; i < sizeof(days) / sizeof(days[0]); i++) {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO sessions (book_id,start_time,end_time,"
                 " active_seconds,pages_start,pages_end,recovered,pages_read)"
                 " VALUES (7,%ld,%ld,900,1,10,0,9)", days[i] - 900, days[i]);
        ex(t.stats, sql);
    }

    unsigned char grid[366];
    memset(grid, 0, sizeof(grid));
    assert(stats_year_days(t.stats, 2024, grid, 366) == 3);
    assert(grid[0] == 1);   /* 1 January is index 0 */
    assert(grid[59] == 1);  /* 29 February, which only a leap year has */
    assert(grid[365] == 1); /* 31 December of a 366-day year */
    /* Nothing from the year after it. */
    memset(grid, 0, sizeof(grid));
    assert(stats_year_days(t.stats, 2025, grid, 365) == 1);
    assert(grid[2] == 1);

    /* A day under the minute does not count, here or in the streak. */
    ex(t.stats, "UPDATE sessions SET active_seconds = 30");
    memset(grid, 0, sizeof(grid));
    assert(stats_year_days(t.stats, 2024, grid, 366) == 0);

    tracker_close(&t);
    unlink(db_path);
}

/* The log is the only debugger this app has, and the rotation is the one part
 * of it that can damage a log rather than add to it: at 64 KB the file is
 * halved, keeping the newest 32 KB. Nothing checked that until now, because on
 * a host the writer aimed at a device path that does not exist. */
static void test_log_rotation(void)
{
    const char *log_path = "/tmp/bs_test_app.log";
    unlink(log_path);
    setenv("POCKETBOOK_STATISTICS_LOG", log_path, 1);
    assert(strcmp(pb_log_path(), log_path) == 0);

    /* Each line is stamped and about 60 bytes, so this comfortably passes the
     * 64 KB mark and rotates on the way. */
    for (int i = 0; i < 1600; i++)
        pb_log("line %06d 0123456789012345678901234567890123456789", i);

    struct stat st;
    assert(stat(log_path, &st) == 0);
    /* Halved, not emptied, and not left to grow. */
    assert(st.st_size > 16 * 1024);
    assert(st.st_size < 64 * 1024);

    FILE *f = fopen(log_path, "r");
    assert(f != NULL);
    char first[512] = {0}, last[512] = {0};
    assert(fgets(first, sizeof(first), f) != NULL);
    while (fgets(last, sizeof(last), f))
        ;
    fclose(f);

    /* The newest line survived: it is the one worth keeping. */
    assert(strstr(last, "line 001599") != NULL);
    /* And the oldest kept line is a whole line, not the tail of one the seek
     * landed in the middle of. */
    assert(strstr(first, " line 0") != NULL);
    assert(first[0] >= '0' && first[0] <= '9'); /* the timestamp is intact */

    /* Nothing is left behind: the temporary copy is renamed, never dropped. */
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s.%d", log_path, (int)getpid());
    assert(stat(tmp, &st) != 0);

    unlink(log_path);
    unsetenv("POCKETBOOK_STATISTICS_LOG");
    assert(strcmp(pb_log_path(), PB_LOG_PATH) == 0);
}

/* An explorer-3 old enough to have no books_fast_hashes. The cover key then
 * comes from the file's own hash instead, which is the whole point of keeping
 * a second statement around: losing tracking over a cover key would be a poor
 * trade. Firmware this old has not been seen, but the fallback is written and
 * so it is worth knowing that it works. */
static void test_legacy_schema(void)
{
    const char *exp_path = "/tmp/bs_test_legacy_explorer.db";
    const char *db_path = "/tmp/bs_test_legacy.db";
    unlink(exp_path);
    unlink(db_path);

    sqlite3 *old_exp;
    assert(sqlite3_open(exp_path, &old_exp) == SQLITE_OK);
    ex(old_exp, "CREATE TABLE books_impl (id INTEGER PRIMARY KEY, title TEXT, author TEXT);"
                "CREATE TABLE files (book_id INTEGER, storageid INTEGER, fast_hash BLOB);"
                "CREATE TABLE books_settings (bookid INTEGER, profileid INTEGER,"
                " position TEXT, position_ts INTEGER, cpage INTEGER, npage INTEGER,"
                " opentime INTEGER, completed INTEGER);");
    ex(old_exp, "INSERT INTO books_impl VALUES (3,'Altbuch','Autorin');"
                "INSERT INTO files VALUES (3,1,x'ccdd');"
                "INSERT INTO books_settings VALUES (3,1,'p',2000,5,200,1900,0);");

    pb_state s;
    assert(tracker_read_state(exp_path, &s) == 0);
    assert(s.bookid == 3);
    assert(strcmp(s.title, "Altbuch") == 0);
    /* The key is there, taken from files because the hash table is not. */
    assert(strcmp(s.cover, "ccdd") == 0);

    /* And a session derived from it is an ordinary session. */
    tracker t;
    assert(tracker_init(&t, db_path, exp_path) == 0);
    assert(tracker_observe(&t, &s) == 1);
    assert(q1(t.stats, "SELECT COUNT(*) FROM sessions WHERE book_id=3") == 1);

    tracker_close(&t);
    sqlite3_close(old_exp);
    unlink(exp_path);
    unlink(db_path);
}

/* Per-book totals, which the Overview turns into "about 5h 22m left". The
 * pace is the half worth testing: it divides pages by the minutes they were
 * read in, and both sides have to exclude what was never measured. */
static void test_stats_book(void)
{
    const char *db_path = "/tmp/bs_test_book.db";
    unlink(db_path);
    tracker t;
    assert(tracker_init(&t, db_path, EXP_DB) == 0);
    set_since(t.stats, "0");

    int64_t secs = -1;
    double ppm = -1;
    /* A book with nothing recorded answers zero, not a stale value. */
    stats_book(t.stats, 7, &secs, &ppm);
    assert(secs == 0 && ppm == 0);

    /* Sixty pages over an hour: one page a minute. */
    ex(t.stats, "INSERT INTO sessions (book_id,start_time,end_time,"
                " active_seconds,pages_start,pages_end,recovered,pages_read)"
                " VALUES (7,1000,4600,3600,1,61,0,60)");
    stats_book(t.stats, 7, &secs, &ppm);
    assert(secs == 3600);
    assert(ppm > 0.99 && ppm < 1.01);

    /* An estimate counts in the total, never in the pace: it is wall clock
     * reconstructed after the fact and carries no pages. */
    ex(t.stats, "INSERT INTO sessions (book_id,start_time,end_time,"
                " active_seconds,pages_start,pages_end,recovered,pages_read)"
                " VALUES (7,5000,8600,3600,NULL,NULL,1,0)");
    stats_book(t.stats, 7, &secs, &ppm);
    assert(secs == 7200);
    assert(ppm > 0.99 && ppm < 1.01);

    /* The filter that keeps estimates out of the pace has to hold even when a
     * recovered row carries pages, which it never should: the migration that
     * seeded pages_read skips them and tracker_recover() writes none. The
     * point of the filter is that a row like this one — a future bug, or a
     * database edited by hand — cannot bend the pace. */
    ex(t.stats, "INSERT INTO sessions (book_id,start_time,end_time,"
                " active_seconds,pages_start,pages_end,recovered,pages_read)"
                " VALUES (7,9000,12600,3600,1,500,1,499)");
    stats_book(t.stats, 7, &secs, &ppm);
    assert(secs == 10800);            /* it counts toward the total */
    assert(ppm > 0.99 && ppm < 1.01); /* and not toward the pace */
    ex(t.stats, "DELETE FROM sessions WHERE start_time=9000");

    /* Another book's reading is not this book's. */
    ex(t.stats, "INSERT INTO sessions (book_id,start_time,end_time,"
                " active_seconds,pages_start,pages_end,recovered,pages_read)"
                " VALUES (8,1000,4600,3600,1,300,0,299)");
    stats_book(t.stats, 7, &secs, &ppm);
    assert(secs == 7200);
    assert(ppm > 0.99 && ppm < 1.01);

    /* And nothing before tracking started is history at all. */
    set_since(t.stats, "6000");
    stats_book(t.stats, 7, &secs, &ppm);
    assert(secs == 3600); /* only the estimate, which ends at 8600 */
    assert(ppm == 0);     /* and it has no pages to make a pace from */

    tracker_close(&t);
    unlink(db_path);
}

/* Where the app reads and writes. Every one of these is fixed on the device and
 * overridable by environment variable, which is what lets the tests point the
 * code at a temporary file — including the two the liveness check uses, whose
 * override is the only reason test_daemon_alive() can exist. */
static void test_paths(void)
{
    assert(strcmp(stats_db_path(), STATS_DB) == 0);
    assert(strcmp(explorer_db_path(), EXPLORER_DB) == 0);
    assert(strcmp(pidfile_path(), PIDFILE) == 0);
    assert(strcmp(proc_dir_path(), "/proc") == 0);

    setenv("POCKETBOOK_STATISTICS_DB", "/tmp/x.db", 1);
    setenv("POCKETBOOK_STATISTICS_EXPLORER_DB", "/tmp/y.db", 1);
    setenv("POCKETBOOK_STATISTICS_PIDFILE", "/tmp/z.pid", 1);
    setenv("POCKETBOOK_STATISTICS_PROC", "/tmp/proc", 1);
    assert(strcmp(stats_db_path(), "/tmp/x.db") == 0);
    assert(strcmp(explorer_db_path(), "/tmp/y.db") == 0);
    assert(strcmp(pidfile_path(), "/tmp/z.pid") == 0);
    assert(strcmp(proc_dir_path(), "/tmp/proc") == 0);

    unsetenv("POCKETBOOK_STATISTICS_DB");
    unsetenv("POCKETBOOK_STATISTICS_EXPLORER_DB");
    unsetenv("POCKETBOOK_STATISTICS_PIDFILE");
    unsetenv("POCKETBOOK_STATISTICS_PROC");
    assert(strcmp(stats_db_path(), STATS_DB) == 0);
    assert(strcmp(pidfile_path(), PIDFILE) == 0);
}

/* --- The daemon ------------------------------------------------------------
 *
 * Only two things about it have a host build: whether it thinks one of its own
 * is already running, and the marks it leaves behind for the next start. Both
 * are worth it. The first decides whether the day is measured at all, and it
 * was wrong on a real device for months; the second is the only evidence there
 * will ever be about a daemon that is killed without a chance to say so. */

#define TEST_PIDFILE "/tmp/bs_test_daemon.pid"
#define TEST_PROC "/tmp/bs_test_proc"

static void write_pidfile_text(const char *text)
{
    FILE *f = fopen(TEST_PIDFILE, "w");
    assert(f != NULL);
    fputs(text, f);
    fclose(f);
}

/* /proc/<pid>/cmdline as the kernel writes it: arguments separated by NULs,
 * which is the part the check has to get right. */
static void write_cmdline(int pid, const char *args, size_t len)
{
    char dir[128], path[160];
    snprintf(dir, sizeof(dir), "%s/%d", TEST_PROC, pid);
    mkdir(TEST_PROC, 0755);
    mkdir(dir, 0755);
    snprintf(path, sizeof(path), "%s/cmdline", dir);
    FILE *f = fopen(path, "wb");
    assert(f != NULL);
    assert(fwrite(args, 1, len, f) == len);
    fclose(f);
}

static void remove_cmdline(int pid)
{
    char path[160], dir[128];
    snprintf(path, sizeof(path), "%s/%d/cmdline", TEST_PROC, pid);
    snprintf(dir, sizeof(dir), "%s/%d", TEST_PROC, pid);
    unlink(path);
    rmdir(dir);
}

static void test_daemon_alive(void)
{
    setenv("POCKETBOOK_STATISTICS_PIDFILE", TEST_PIDFILE, 1);
    setenv("POCKETBOOK_STATISTICS_PROC", TEST_PROC, 1);
    const int self = (int)getpid();
    char pid_text[32];
    snprintf(pid_text, sizeof(pid_text), "%d\n", self);

    /* No pidfile: nothing has ever started one. */
    unlink(TEST_PIDFILE);
    assert(daemon_alive() == 0);

    /* A live pid whose process is one of ours. This process stands in for the
     * daemon, so the pid is genuinely alive and kill(pid, 0) succeeds. */
    write_pidfile_text(pid_text);
    static const char ours[] =
        "/mnt/ext1/applications/PocketBookStatistics.app\0--daemon\0";
    write_cmdline(self, ours, sizeof(ours) - 1);
    assert(daemon_alive() == 1);

    /* The same live pid, but the process is the firmware's reader. This is the
     * case that cost a real device its measurements: the pidfile survived a
     * reboot and the number had been handed to somebody else. */
    static const char reader[] = "/ebrmain/bin/eink-reader.app\0/mnt/ext1/b.epub\0";
    write_cmdline(self, reader, sizeof(reader) - 1);
    assert(daemon_alive() == 0);

    /* Our own app, but not the daemon: the flag is what tells them apart. */
    static const char app[] = "/mnt/ext1/applications/PocketBookStatistics.app\0";
    write_cmdline(self, app, sizeof(app) - 1);
    assert(daemon_alive() == 0);

    /* An empty cmdline is a zombie, which polls nothing. */
    write_cmdline(self, "", 0);
    assert(daemon_alive() == 0);

    /* A cmdline that cannot be read counts as gone. Not starting a daemon
     * costs a day of measurement; a second one costs a poll loop and cannot
     * double-count. */
    remove_cmdline(self);
    assert(daemon_alive() == 0);

    /* A pid nothing owns, and a pidfile with nothing in it. */
    write_cmdline(self, ours, sizeof(ours) - 1);
    write_pidfile_text("2147483640\n");
    assert(daemon_alive() == 0);
    write_pidfile_text("not a pid\n");
    assert(daemon_alive() == 0);
    write_pidfile_text("");
    assert(daemon_alive() == 0);

    unlink(TEST_PIDFILE);
    remove_cmdline(self);
    rmdir(TEST_PROC);
    unsetenv("POCKETBOOK_STATISTICS_PIDFILE");
    unsetenv("POCKETBOOK_STATISTICS_PROC");
}

static void test_daemon_marks(void)
{
    const char *db_path = "/tmp/bs_test_marks.db";
    unlink(db_path);
    tracker t;
    assert(tracker_init(&t, db_path, EXP_DB) == 0);

    /* A first start on a fresh database: the marks appear, and there is no
     * previous run to report. */
    assert(stats_meta_int(t.stats, META_DAEMON_STARTED) == 0);
    daemon_note_start(t.stats);
    const int64_t first = stats_meta_int(t.stats, META_DAEMON_STARTED);
    assert(first > 0);
    assert(stats_meta_int(t.stats, META_DAEMON_LAST_POLL) == first);
    assert(stats_meta_int(t.stats, META_DAEMON_POLLS) == 0);

    /* A run that polled for a while and was then killed. What the heartbeat
     * left behind is what the next start reads. */
    stats_meta_set_int(t.stats, META_DAEMON_POLLS, 40);
    stats_meta_set_int(t.stats, META_DAEMON_LAST_POLL, first + 600);
    daemon_note_start(t.stats);
    assert(stats_meta_int(t.stats, META_DAEMON_POLLS) == 0);
    const int64_t second = stats_meta_int(t.stats, META_DAEMON_STARTED);
    assert(second >= first);
    assert(stats_meta_int(t.stats, META_DAEMON_LAST_POLL) == second);

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
    avoid_midnight();

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
    /* How much of the standby the tracker could rule out depends on the clock
     * it read while observing: `asleep` loses everything that lies after
     * position_ts, and that stretch grows by a second every time the test
     * crosses a second boundary. So the window is 900 s plus however long this
     * test itself has been running, which is what `elapsed` measures. Asserting
     * a bare 900 made the test fail whenever the machine was slow enough to
     * tick over, which is exactly what happened on CI. */
    const long elapsed = (long)time(NULL) - now;
    /* Split-invariant: a run started within 75 minutes of local midnight puts
     * the session in two rows, and the split moves seconds without making any. */
    snprintf(sql, sizeof(sql),
             "SELECT SUM(active_seconds) FROM sessions WHERE book_id=7");
    long got = q1(sleepy.stats, sql);
    /* 3300 window - 2400 asleep */
    assert(got >= 897 && got <= 900 + elapsed);

    /* The same standby is never deducted twice: the row carries the total it
     * was billed against, and this window is measured from there. */
    set_state(exp, base, now, 118);
    assert(tracker_read_state(EXP_DB, &s) == 0);
    assert(tracker_observe(&sleepy, &s) == 2);
    got = q1(sleepy.stats, sql);
    /* + a clean 1200 s window, and the same slack as above */
    assert(got >= 2097 && got <= 2100 + elapsed);
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

    test_reopen_after_poweroff(exp);
    test_reopen_across_midnight(exp);
    test_year_days();
    test_stats_book();
    test_legacy_schema();
    test_log_rotation();
    test_paths();
    test_daemon_alive();
    test_daemon_marks();
    test_streak();
    test_manual_totals();
    test_version_compare();

    printf("all tracker tests ok\n");
    return 0;
}
