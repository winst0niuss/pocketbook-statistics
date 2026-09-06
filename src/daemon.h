#ifndef DAEMON_H
#define DAEMON_H

#define EXPLORER_DB "/mnt/ext1/system/explorer-3/explorer-3.db"
#define STATS_DIR "/mnt/ext1/system/pocketbook-statistics"
#define STATS_DB STATS_DIR "/statistics.db"
#define PIDFILE STATS_DIR "/statistics.pid"
/* What the daemon's own /proc/<pid>/cmdline has to contain for a pid to count
 * as ours. A number alone proves nothing: after a reboot the pid in the file is
 * regularly a live process of the firmware's. */
#define DAEMON_BINARY "PocketBookStatistics"
#define DAEMON_FLAG "--daemon"
#define COVER_DIR "/mnt/ext1/system/cover_chache/hashed"
/* Our own cover cache: EPUB extractions plus copies of firmware covers, which
 * is what keeps a thumbnail after the book file is gone. */
#define OWN_COVER_DIR STATS_DIR "/covers"

#ifdef __cplusplus
extern "C" {
#endif

int run_daemon(void);
void spawn_daemon(const char *self);
const char *stats_db_path(void);
const char *explorer_db_path(void);
/* Where the pidfile is, and where to look up what a pid actually is. Both are
 * fixed on the device and overridable by environment variable, the same way the
 * two database paths are: it is what lets the host tests drive the liveness
 * check, and that check decides whether the day gets measured at all.
 * POCKETBOOK_STATISTICS_PIDFILE, POCKETBOOK_STATISTICS_PROC. */
const char *pidfile_path(void);
const char *proc_dir_path(void);
/* 1 if the pidfile names a process that is one of our daemons. The app and the
 * daemon reach it through spawn_daemon/run_daemon; it is declared here for the
 * tests. */
int daemon_alive(void);
/* Records the marks a daemon leaves while it lives, and logs what the previous
 * run reached. Called at the top of run_daemon(), and a function of its own
 * because the poll loop it sits in never returns. */
struct sqlite3;
void daemon_note_start(struct sqlite3 *stats);

#ifdef __cplusplus
}
#endif

#endif
