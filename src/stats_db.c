#include "stats_db.h"
#include <string.h>

static double q_double(sqlite3 *db, const char *sql)
{
    sqlite3_stmt *st = NULL;
    double v = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW)
        v = sqlite3_column_double(st, 0);
    sqlite3_finalize(st);
    return v;
}

/* Days in a row with reading, counted back from today.
 *
 * A day counts when it holds at least a minute: opening a book to check
 * something leaves seconds behind, and a streak built on those is a streak
 * nobody earned. Estimates count as they do in the totals — the day is what it
 * is — and days before `tracking_since` cannot count at all, since nothing was
 * measuring then.
 *
 * Today is allowed to be empty. A streak that only exists once the reader has
 * read today would read as broken every morning, so the walk may start at
 * yesterday; it starts nowhere else, and one missed day ends it.
 *
 * The days come out as julian day numbers rather than dates because the walk
 * needs to subtract one from another, and doing that on 'YYYY-MM-DD' strings
 * is how a streak gets a month boundary wrong. */
int stats_streak_days(sqlite3 *db)
{
    const char *sql =
        "SELECT CAST(julianday(date(end_time,'unixepoch','localtime')) AS INTEGER) AS d"
        " FROM sessions WHERE 1=1" AND_TRACKED
        " GROUP BY d HAVING SUM(active_seconds) >= 60"
        " ORDER BY d DESC";
    const int64_t today = (int64_t)q_double(
        db, "SELECT CAST(julianday(date('now','localtime')) AS INTEGER)");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
        return 0;
    int streak = 0;
    int64_t expected = today;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const int64_t day = sqlite3_column_int64(st, 0);
        if (streak == 0 && day == today - 1)
            expected = day; /* nothing read yet today, so the run ends there */
        if (day != expected)
            break;
        streak++;
        expected--;
    }
    sqlite3_finalize(st);
    return streak;
}

int stats_overall(sqlite3 *db, overall_stats *o)
{
    memset(o, 0, sizeof(*o));
    o->total_hours = q_double(db, "SELECT SUM(active_seconds)/3600.0 FROM sessions WHERE 1=1" AND_TRACKED);
    /* recovered = 1 is a wall-clock estimate, not measured reading: it stays in
     * the totals but must not skew the derived metrics. The pages come from
     * pages_read rather than from pages_end - pages_start, because those two
     * are positions and also move when the reader follows a footnote link to
     * the back of the book — a distance that used to land in the pace as
     * pages read. */
    double mins = q_double(db,
        "SELECT SUM(active_seconds)/60.0 FROM sessions"
        " WHERE pages_read > 0 AND active_seconds > 0 AND recovered = 0"
        AND_TRACKED);
    double pages = q_double(db,
        "SELECT SUM(pages_read) FROM sessions"
        " WHERE pages_read > 0 AND active_seconds > 0 AND recovered = 0"
        AND_TRACKED);
    if (mins > 0)
        o->pages_per_min = pages / mins;
    /* Today is the one window the Overview shows directly. Estimates count
     * here as they do in the totals: the day is what it is. */
    o->today_secs = (int)q_double(db,
        "SELECT IFNULL(SUM(active_seconds),0) FROM sessions"
        " WHERE date(end_time,'unixepoch','localtime') = date('now','localtime')"
        AND_TRACKED);
    o->streak_days = stats_streak_days(db);
    return 0;
}

/* The year as a row of days, for the streak screen's grid. Same rule as the
 * streak itself — a minute or more, estimates included, nothing before
 * tracking_since — so the two cannot disagree about what a reading day is.
 *
 * strftime('%j') is the day of the year, 001 on 1 January, which is exactly
 * the index the grid wants; days outside the array are ignored rather than
 * trusted, since a corrupt end_time must not write past it. */
int stats_year_days(sqlite3 *db, int year, unsigned char *days, int count)
{
    const char *sql =
        "SELECT CAST(strftime('%j', end_time,'unixepoch','localtime') AS INTEGER)"
        " FROM sessions"
        " WHERE strftime('%Y', end_time,'unixepoch','localtime') = printf('%04d',?1)"
        AND_TRACKED
        " GROUP BY 1 HAVING SUM(active_seconds) >= 60";
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_int(st, 1, year);
    int n = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const int doy = sqlite3_column_int(st, 0);
        if (doy >= 1 && doy <= count) {
            days[doy - 1] = 1;
            n++;
        }
    }
    sqlite3_finalize(st);
    return n;
}

int64_t stats_meta_int(sqlite3 *db, const char *key)
{
    sqlite3_stmt *st = NULL;
    int64_t v = 0;
    if (sqlite3_prepare_v2(db, "SELECT value FROM meta WHERE key = ?1", -1, &st,
                           NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
        if (sqlite3_step(st) == SQLITE_ROW)
            v = sqlite3_column_int64(st, 0);
    }
    sqlite3_finalize(st);
    return v;
}

int stats_meta_set_int(sqlite3 *db, const char *key, int64_t value)
{
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db,
                           "INSERT OR REPLACE INTO meta (key,value) VALUES (?1,?2)",
                           -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_int64(st, 2, value);
    const int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? 0 : -1;
}

int64_t stats_tracking_since(sqlite3 *db)
{
    return (int64_t)q_double(db,
        "SELECT COALESCE((SELECT value FROM meta WHERE key='tracking_since'), 0)");
}

void stats_book(sqlite3 *db, int64_t bookid, int64_t *secs, double *pages_per_min)
{
    *secs = 0;
    *pages_per_min = 0;
    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT IFNULL(SUM(active_seconds),0),"
        " IFNULL(SUM(CASE WHEN pages_read > 0 AND recovered = 0"
        "   THEN pages_read END),0),"
        " IFNULL(SUM(CASE WHEN pages_read > 0 AND recovered = 0"
        "   THEN active_seconds END),0)"
        " FROM sessions WHERE book_id = ?1" AND_TRACKED;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(st, 1, bookid);
        if (sqlite3_step(st) == SQLITE_ROW) {
            *secs = sqlite3_column_int64(st, 0);
            double pages = sqlite3_column_double(st, 1);
            double s = sqlite3_column_double(st, 2);
            if (s > 0)
                *pages_per_min = pages / (s / 60.0);
        }
    }
    sqlite3_finalize(st);
}
