#ifndef STATS_DB_H
#define STATS_DB_H

#include <stdint.h>
#include <sqlite3.h>

typedef struct {
    double total_hours;
    double pages_per_min;      /* nur aus Sessions mit bekannten Seiten */
    int today_secs;            /* measured today, local time */
    int streak_days;           /* days in a row with reading, up to today */
} overall_stats;

/* Sessions before this moment were reconstructed from the firmware's
 * last-open timestamps, not measured, so every history view filters them out.
 * Falls back to 0 (no filtering) on a database that predates the marker. */
#define TRACKED_SINCE_SQL \
    "(SELECT COALESCE((SELECT value FROM meta WHERE key='tracking_since'), 0))"
#define AND_TRACKED " AND end_time >= " TRACKED_SINCE_SQL

int stats_overall(sqlite3 *db, overall_stats *o);
/* Epoch second the marker holds, 0 if there is none. */
int64_t stats_tracking_since(sqlite3 *db);
/* Days in a row with reading, counted back from today (see the definition for
 * what counts as a day). Also carried in overall_stats. */
int stats_streak_days(sqlite3 *db);
/* Marks days[0 .. count-1] — 1 January onwards — with 1 where that local day
 * holds at least a minute of tracked reading. `count` is 365 or 366; anything
 * the year does not have is left alone. Returns the number of days marked. */
int stats_year_days(sqlite3 *db, int year, unsigned char *days, int count);
/* Total time + speed for a book (speed <= 0 if unknown). */
void stats_book(sqlite3 *db, int64_t bookid, int64_t *secs, double *pages_per_min);

/* Epoch second of the first measured session for a book, 0 if none. */

#endif
