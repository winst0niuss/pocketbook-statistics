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

#ifdef __cplusplus
}
#endif

#endif
