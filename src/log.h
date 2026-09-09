#ifndef LOG_H
#define LOG_H

/* One log for the whole app, written by the C core and the Qt side alike, so a
 * week of use reads as one story: the daemon starting, an update, the shim
 * handing a book over. Lines are timestamped and flushed one at a time — the
 * failure worth catching is the process dying, and a buffered log loses exactly
 * the line that says why.
 *
 * Keep it to events that would matter at the end of that week: starts, stops,
 * refusals, anything that went wrong. Not a trace of normal work. */

#include "daemon.h"

/* Spelled once: update_log.cpp reads the same file to show its tail. */
#define PB_LOG_PATH STATS_DIR "/app.log"

/* The file that is actually written. PB_LOG_PATH on the device, and
 * POCKETBOOK_STATISTICS_LOG where it is set — which is how the host tests get
 * to exercise the rotation, the one thing here that can damage a log rather
 * than merely add to it. */
const char *pb_log_path(void);

#ifdef __cplusplus
extern "C" {
#endif

void pb_log(const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 1, 2)))
#endif
    ;

/* Names what the process is busy with, for the crash line below to quote. Kept
 * in memory and never written on its own — the app marks its startup steps
 * here whether or not the build prints them. */
void pb_log_stage(const char *stage);

/* Turns a death by signal into a line. A qFatal writes one and a SIGSEGV
 * writes nothing, so on a reader with no console the two look identical: the
 * app is gone and the log simply stops. Installed by the app and the daemon;
 * SIGKILL is still beyond reach, which is exactly what makes the difference
 * worth recording. */
void pb_log_install_crash_handler(void);

#ifdef __cplusplus
}
#endif

#endif
