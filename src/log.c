#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define LOG_PATH PB_LOG_PATH
/* Big enough for a week of ordinary use, small enough to read on the device. */
#define LOG_MAX_BYTES (64 * 1024)
#define LOG_KEEP_BYTES (32 * 1024)

/* Halve the file rather than delete it: the oldest lines are the ones worth
 * losing, and a log that empties itself has nothing to say about the week it
 * was kept for. */
static void rotate_if_needed(void)
{
    struct stat st;
    if (stat(LOG_PATH, &st) != 0 || st.st_size <= LOG_MAX_BYTES)
        return;

    FILE *in = fopen(LOG_PATH, "r");
    if (!in)
        return;
    if (fseek(in, st.st_size - LOG_KEEP_BYTES, SEEK_SET) != 0) {
        fclose(in);
        return;
    }
    char buf[4096];
    /* Drop the partial line the seek landed in the middle of. */
    if (!fgets(buf, sizeof(buf), in)) {
        fclose(in);
        return;
    }

    /* Name the temporary file after the writer: the app and the daemon both log,
     * and if they rotate at the same moment one must not truncate the other's
     * half-written copy. Losing a few lines to the race is fine; a corrupt log
     * is not. */
    char tmp[sizeof(LOG_PATH) + 32];
    snprintf(tmp, sizeof(tmp), LOG_PATH ".%d", (int)getpid());
    FILE *out = fopen(tmp, "w");
    if (!out) {
        fclose(in);
        return;
    }
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n)
            break;
    }
    fclose(in);
    const int failed = ferror(out) != 0;
    fclose(out);
    if (failed) {
        /* A full disk would otherwise swap a good log for a truncated one. */
        unlink(tmp);
        return;
    }
    rename(tmp, LOG_PATH);
}

void pb_log(const char *fmt, ...)
{
    mkdir(STATS_DIR, 0755);
    rotate_if_needed();

    FILE *f = fopen(LOG_PATH, "a");
    if (!f)
        return;

    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    fprintf(f, "%02d-%02d %02d:%02d:%02d ", tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);

    fputc('\n', f);
    fclose(f);
}
