#pragma once

#include <QString>

struct ScreenSize {
    int width = 0;
    int height = 0;
    int panelHeight = 0;
};

/* Initializes InkView (TASK_MAKEACTIVE) and returns the panel metrics.
 * inkview.h stays in this one TU; its macros collide with Qt. */
ScreenSize openInkViewScreen();
QString inkViewFontFamily();
QString inkViewLang(); /* e.g. "de", "en" */

/* Hands `path` to whichever reader the firmware would open it with, as the
 * library screen does. False means the firmware exports no OpenBook at all —
 * anything past that point is the firmware's business, not ours. */
bool inkViewOpenBook(const QString &path);

/* --- Network, for the update check only ---------------------------------
 * The firmware owns the network stack (Wi-Fi association, TLS, certificates),
 * so the updater never opens a socket itself. Both calls block for as long as
 * the firmware takes; callers must repaint before entering them. */

/* Returned when the firmware exports no usable download function at all. */
constexpr int kInkViewNetUnsupported = -1000;

/* Brings Wi-Fi up if it isn't already. False means no connection. */
bool inkViewNetworkUp();

/* Downloads `url` to `dest`, following redirects. Returns 0 on success or a
 * negative InkView NET_E* code; `inkViewNetErrorText` renders one for the UI. */
/* Seconds the firmware may spend on one transfer. The check is small and must
 * fail fast, because the UI thread is stopped for the whole of it; the release
 * is 600 KB over whatever Wi-Fi the reader has. */
constexpr int kCheckTimeoutSeconds = 20;
constexpr int kDownloadTimeoutSeconds = 60;

/* Downloads to a file, synchronously — the firmware offers nothing else that
 * works under Qt. Both knobs matter because the call blocks the UI thread:
 * a version check is a few hundred bytes and should give up quickly, while the
 * release itself is worth waiting for and worth one retry after a reconnect. */
int inkViewDownload(const QString &url, const QString &dest, int timeoutSeconds,
                    bool retry);
QString inkViewNetErrorText(int code);
