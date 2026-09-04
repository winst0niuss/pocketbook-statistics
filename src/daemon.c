#include "daemon.h"
#include "log.h"
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

/* 1 if a daemon is already running. */
static int daemon_alive(void)
{
    FILE *f = fopen(PIDFILE, "r");
    if (!f)
        return 0;
    int pid = 0;
    if (fscanf(f, "%d", &pid) != 1)
        pid = 0;
    fclose(f);
    return pid > 0 && kill(pid, 0) == 0;
}

static void write_pidfile(void)
{
    FILE *f = fopen(PIDFILE, "w");
    if (f) {
        fprintf(f, "%d\n", (int)getpid());
        fclose(f);
    }
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
    tracker_recover(&t);
    /* One line every twenty polls, and it reports wall time beside them,
     * because the two disagree: this loop only runs when the device gives it a
     * processor, and a reader between page turns mostly does not. Ten minutes
     * of polling spread over an hour of wall clock is the measurement that
     * killed counting time by the poll interval; keep it on record. */
    const int heartbeat_polls = 20;
    int polls = 0;
    int counted = 0;
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
            polls = 0;
            counted = 0;
            heartbeat_start = now;
            heartbeat_mono = mono;
        }
        for (int i = 0; i < POLL_SECONDS && running; i++)
            sleep(1);
    }
    tracker_close(&t);
    unlink(PIDFILE);
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
