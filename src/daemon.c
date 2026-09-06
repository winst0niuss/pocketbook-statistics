#include "daemon.h"
#include "log.h"
#include "stats_db.h"
#include "tracker.h"
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t stop_signal = 0;

static void on_term(int sig)
{
    stop_signal = sig;
    running = 0;
}

const char *stats_db_path(void)
{
    const char *p = getenv("POCKETBOOK_STATISTICS_DB");
    return p ? p : STATS_DB;
}

const char *explorer_db_path(void)
{
    const char *p = getenv("POCKETBOOK_STATISTICS_EXPLORER_DB");
    return p ? p : EXPLORER_DB;
}

const char *pidfile_path(void)
{
    const char *p = getenv("POCKETBOOK_STATISTICS_PIDFILE");
    return p ? p : PIDFILE;
}

const char *proc_dir_path(void)
{
    const char *p = getenv("POCKETBOOK_STATISTICS_PROC");
    return p ? p : "/proc";
}

/* 1 if a daemon of ours is already running.
 *
 * `kill(pid, 0)` is not enough on this device. The pidfile survives a reboot,
 * pids are handed out from a small range, and the number in it is then
 * regularly a live process of the firmware's — the check passes, nothing starts
 * a daemon, and the day is measured only while the app happens to be open. It
 * has been seen on a real reader. So look at what the process actually is:
 * /proc/<pid>/cmdline holds the binary and its arguments, NUL-separated, and
 * ours carries both the app's name and the --daemon flag.
 *
 * A cmdline that cannot be read counts as "not ours" and a daemon is started.
 * The two mistakes are not equal: a daemon that was not started measures
 * nothing until the app is next opened, while a second one costs a poll loop
 * and cannot double-count — update_session() only ever moves a row's end
 * forward, so the loser of a race adds nothing. It is also the case for a
 * zombie, whose cmdline is empty and which polls nothing. procfs is there on
 * this firmware: the startup measurement reads /proc/self/stat and the shim
 * has always read /proc/<pid>. */
int daemon_alive(void)
{
    FILE *f = fopen(pidfile_path(), "r");
    if (!f)
        return 0;
    int pid = 0;
    if (fscanf(f, "%d", &pid) != 1)
        pid = 0;
    fclose(f);
    if (pid <= 0 || kill(pid, 0) != 0)
        return 0;

    char path[280];
    snprintf(path, sizeof(path), "%s/%d/cmdline", proc_dir_path(), pid);
    FILE *c = fopen(path, "r");
    if (!c) {
        pb_log("daemon: cannot read %s, treating pid %d as gone", path, pid);
        return 0;
    }
    char cmd[512];
    const size_t n = fread(cmd, 1, sizeof(cmd) - 1, c);
    fclose(c);
    cmd[n] = '\0';
    for (size_t i = 0; i + 1 < n; i++) {
        if (cmd[i] == '\0')
            cmd[i] = ' ';
    }
    const int ours = strstr(cmd, DAEMON_BINARY) != NULL &&
                     strstr(cmd, DAEMON_FLAG) != NULL;
    if (!ours)
        pb_log("daemon: pid %d in the pidfile is not ours, starting one", pid);
    return ours;
}

static void write_pidfile(void)
{
    FILE *f = fopen(pidfile_path(), "w");
    if (f) {
        fprintf(f, "%d\n", (int)getpid());
        fclose(f);
    }
}

/* Reads what the run before this one got to, says it, and starts fresh marks
 * for this one. The daemon regularly disappears between app launches without
 * logging a stop, which means SIGKILL, which cannot be caught — so the only way
 * to learn anything about it is to leave marks behind while alive and read them
 * on the way back up. A run that ended cleanly logs its signal as well, and
 * then the two lines agree.
 *
 * A function of its own because the poll loop it sits in never returns, and a
 * test has to be able to drive it. */
void daemon_note_start(sqlite3 *stats)
{
    const int64_t now = (int64_t)time(NULL);
    const int64_t prev_start = stats_meta_int(stats, META_DAEMON_STARTED);
    if (prev_start > 0) {
        const int64_t prev_last = stats_meta_int(stats, META_DAEMON_LAST_POLL);
        /* "At least", because the marks are written once a heartbeat and the
         * run ended somewhere after the last one. */
        pb_log("daemon: previous run lived at least %d s, %d polls, last mark "
               "%d s before this start",
               (int)(prev_last - prev_start),
               (int)stats_meta_int(stats, META_DAEMON_POLLS),
               (int)(now - prev_last));
    }
    stats_meta_set_int(stats, META_DAEMON_STARTED, now);
    stats_meta_set_int(stats, META_DAEMON_LAST_POLL, now);
    stats_meta_set_int(stats, META_DAEMON_POLLS, 0);
}

int run_daemon(void)
{
    if (daemon_alive())
        return 0;
    mkdir(STATS_DIR, 0755);
    write_pidfile();
    signal(SIGTERM, on_term);
    signal(SIGINT, on_term);
    /* The daemon keeps dying between app launches without ever logging a stop,
     * so whatever ends it is either SIGKILL — which cannot be caught, and would
     * be the firmware killing the task — or a signal nobody was listening for.
     * Catch the plausible ones and say which arrived; the answer decides
     * whether the daemon has to become a binary of its own, under a name the
     * task manager does not recognise as this app. */
    signal(SIGHUP, on_term);
    signal(SIGQUIT, on_term);
    signal(SIGPIPE, SIG_IGN);

    tracker t;
    if (tracker_init(&t, stats_db_path(), explorer_db_path()) != 0) {
        pb_log("daemon: cannot open %s", stats_db_path());
        return 1;
    }
    pb_log("daemon: started (pid %d)", (int)getpid());

    daemon_note_start(t.stats);

    tracker_recover(&t);
    /* One line every twenty polls, and it reports wall time beside them,
     * because the two disagree: this loop only runs when the device gives it a
     * processor, and a reader between page turns mostly does not. Ten minutes
     * of polling spread over an hour of wall clock is the measurement that
     * killed counting time by the poll interval; keep it on record. */
    const int heartbeat_polls = 20;
    int polls = 0;
    int counted = 0;
    int64_t total_polls = 0;
    int64_t heartbeat_start = (int64_t)time(NULL);
    /* Beside the wall seconds: how many of them the CPU was actually up for.
     * The gap between the two is what a closed cover costs, and it is the
     * measurement the sleep accounting in tracker_observe() rests on. */
    int64_t heartbeat_mono = pb_monotonic_seconds();
    while (running) {
        pb_state s;
        if (tracker_read_state(t.explorer_path, &s) == 0) {
            if (tracker_observe(&t, &s) == 2)
                counted++;
        }
        if (++polls >= heartbeat_polls) {
            const int64_t now = (int64_t)time(NULL);
            const int64_t mono = pb_monotonic_seconds();
            pb_log("daemon: alive, %d polls over %d s wall (%d s awake), %d "
                   "with reading",
                   polls, (int)(now - heartbeat_start),
                   (int)(mono - heartbeat_mono), counted);
            /* The same marks, in the database rather than the log: written
             * once a heartbeat, never once a poll, because the poll path must
             * not touch flash every thirty seconds. */
            total_polls += polls;
            stats_meta_set_int(t.stats, META_DAEMON_POLLS, total_polls);
            stats_meta_set_int(t.stats, META_DAEMON_LAST_POLL, now);
            polls = 0;
            counted = 0;
            heartbeat_start = now;
            heartbeat_mono = mono;
        }
        for (int i = 0; i < POLL_SECONDS && running; i++)
            sleep(1);
    }
    tracker_close(&t);
    unlink(pidfile_path());
    pb_log("daemon: stopped by signal %d", (int)stop_signal);
    return 0;
}

void spawn_daemon(const char *self)
{
    if (daemon_alive())
        return;
    pb_log("app: no daemon running, spawning one");
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl(self, self, "--daemon", (char *)NULL);
        _exit(1);
    }
}
