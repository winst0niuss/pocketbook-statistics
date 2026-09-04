#ifndef TRACKER_H
#define TRACKER_H

#include <stdint.h>
#include <sqlite3.h>

/* Active reading time: the gap between two position updates counts only up to
 * this cap. It applies where nothing else bounds the gap — the first look at a
 * session that started between two observations. */
#define IDLE_CAP_SECONDS 600
/* What one page may be worth. A gap between two page turns is credited only as
 * far as the pages actually turned make plausible, which is what lets a long
 * stretch nobody watched come back whole: 30 pages over 18 minutes is reading,
 * and the flat cap used to pay 10 minutes for it. The measured pace on a real
 * device runs 28-137 s a page, so five minutes is generous; the same bound
 * stops one page turn after an hour away from buying an hour of "reading". */
#define SECONDS_PER_PAGE_CAP 300
/* The mirror of the cap above: what a span of time may be worth in pages.
 * Books that gather their footnotes at the end are read by tapping a link,
 * reading the note and tapping back, and the firmware then stamps a position
 * hundreds of pages away from the one before it. That delta vouches for the
 * whole window (at 300 s a page it buys more than any window holds) and lands
 * in the pace as thousands of pages an hour — over 7000 in one day on a real
 * device (issue #2). Reading measured here runs 28-137 s a page, so fifteen
 * seconds is already twice as fast as the fastest page ever seen on this
 * device; above that rate the position moved by navigation, not by reading. */
#define SECONDS_PER_PAGE_MIN 15
/* ...but only a move at least this large is weighed against the clock at all.
 * The window between two firmware saves is normally long enough for the rule
 * above to be generous, yet it can also be seconds — two saves in a row as a
 * book is closed — and there a handful of genuinely turned pages would fail
 * it. A jump to the notes at the back of a book is not a handful. */
#define JUMP_MIN_PAGES 10
/* Sanity cap for sessions reconstructed without a running daemon. The span
 * opentime..position_ts is pure wall clock and includes standby, so keep this
 * tight: recovered rows are an estimate, not measured reading time. */
#define RECOVERED_CAP_SECONDS (90 * 60)
#define POLL_SECONDS 30
/* A poll interval across which the wall clock ran far ahead of CLOCK_MONOTONIC
 * is the device having been suspended: the CPU was off, so nobody turned a
 * page. The e-ink reader lets the CPU go down between renders while a book is
 * being read too, so a short suspend is ordinary reading; only a stretch this
 * long is the book having been put down, which on a PB629 is a closed cover.
 * The threshold sits well above the poll intervals measured while reading
 * (2-7 minutes of wall clock at a 7-22 % duty cycle) and well below the
 * shortest sleep seen between two firmware position saves (15 minutes). */
#define DEEP_SLEEP_SECONDS 900
/* Suspends at least this long are logged whether or not they are counted, so
 * one evening on a device shows where the two populations actually sit. */
#define SLEEP_LOG_SECONDS 120

/* Latest reading state from the firmware DB (explorer-3.db). */
typedef struct {
    int64_t bookid;
    int64_t opentime;    /* session start (last open) */
    int64_t position_ts; /* last position update */
    int cpage, npage, completed;
    char title[256];
    char author[256];
    char cover[128];     /* "<storageid><hex-fast_hash>" or "" */
} pb_state;

typedef struct {
    sqlite3 *stats;
    const char *explorer_path;
    int64_t cur_book, cur_open, cur_pos_ts;
    int64_t cur_row_start; /* start_time of the row we currently write to */
    int cur_pages;         /* cpage as of cur_pos_ts, for bounding the gap */
    int64_t cur_sleep;     /* sleep_total as of the row's end_time */
    /* Marks for the two clocks, kept in memory rather than in the DB: only a
     * process that observes on its own cadence can classify a suspend, and
     * that is the daemon. StatsBridge::catchUp() builds a fresh tracker every
     * refresh, so its marks stay empty and it never classifies — which is the
     * intent, since a difference measured across hours says nothing about
     * where inside those hours the device was asleep. */
    int64_t mark_wall, mark_mono;
} tracker;

/* Opens/creates our own stats DB. 0 = ok. */
int tracker_init(tracker *t, const char *stats_path, const char *explorer_path);
/* Reads the most recently opened book state. 0 = ok, 1 = no book, <0 = error. */
int tracker_read_state(const char *explorer_path, pb_state *out);
/* Backfills sessions created while no daemon was running. */
int tracker_recover(tracker *t);
/* One poll tick: derives session progress from the state. */
int tracker_observe(tracker *t, const pb_state *s);
void tracker_close(tracker *t);
/* Seconds on CLOCK_MONOTONIC, which stops while the device is suspended. 0
 * where the platform has no such clock. */
int64_t pb_monotonic_seconds(void);

#endif
