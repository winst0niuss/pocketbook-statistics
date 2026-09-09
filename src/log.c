#include "log.h"

#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

const char *pb_log_path(void)
{
    const char *p = getenv("POCKETBOOK_STATISTICS_LOG");
    return p ? p : PB_LOG_PATH;
}
/* Big enough for a week of ordinary use, small enough to read on the device. */
#define LOG_MAX_BYTES (64 * 1024)
#define LOG_KEEP_BYTES (32 * 1024)

/* Halve the file rather than delete it: the oldest lines are the ones worth
 * losing, and a log that empties itself has nothing to say about the week it
 * was kept for. */
static void rotate_if_needed(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0 || st.st_size <= LOG_MAX_BYTES)
        return;

    FILE *in = fopen(path, "r");
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
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s.%d", path, (int)getpid());
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
    rename(tmp, path);
}

void pb_log(const char *fmt, ...)
{
    const char *path = pb_log_path();
    mkdir(STATS_DIR, 0755);
    rotate_if_needed(path);

    FILE *f = fopen(path, "a");
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

/* --- The crash trail -----------------------------------------------------
 * Everything below runs from a signal handler, so it may call only what is
 * async-signal-safe: no stdio, no allocation, no localtime. That is why the
 * line carries no timestamp — it stands under the last one written, which
 * places it closely enough — and why the path is copied at install time
 * instead of being asked for again here. */

static char g_stage[64] = "start";
static char g_crash_path[512];

void pb_log_stage(const char *stage)
{
    if (!stage)
        return;
    size_t i = 0;
    for (; stage[i] != '\0' && i + 1 < sizeof(g_stage); i++)
        g_stage[i] = stage[i];
    g_stage[i] = '\0';
}

static void write_str(int fd, const char *s)
{
    size_t n = 0;
    while (s[n] != '\0')
        n++;
    ssize_t written = write(fd, s, n);
    (void)written; /* a log that cannot be written is not worth dying twice for */
}

static void crash_handler(int sig)
{
    int fd = open(g_crash_path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd >= 0) {
        char num[4];
        int n = 0;
        if (sig >= 100)
            num[n++] = (char)('0' + (sig / 100) % 10);
        if (sig >= 10)
            num[n++] = (char)('0' + (sig / 10) % 10);
        num[n++] = (char)('0' + sig % 10);
        num[n] = '\0';

        write_str(fd, "app: killed by signal ");
        write_str(fd, num);
        write_str(fd, " during ");
        write_str(fd, g_stage);
        write_str(fd, "\n");
        close(fd);
    }

    /* Hand the signal back to the default action, so the process still dies
     * the way it would have: this is a witness, not a recovery. */
    signal(sig, SIG_DFL);
    raise(sig);
}

void pb_log_install_crash_handler(void)
{
    const char *path = pb_log_path();
    size_t i = 0;
    for (; path[i] != '\0' && i + 1 < sizeof(g_crash_path); i++)
        g_crash_path[i] = path[i];
    g_crash_path[i] = '\0';

    static const int caught[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
    for (size_t s = 0; s < sizeof(caught) / sizeof(caught[0]); s++)
        signal(caught[s], crash_handler);
}
