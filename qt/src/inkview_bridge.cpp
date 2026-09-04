#include "inkview_bridge.h"

#include <QByteArray>
#include <QFile>

#include <dlfcn.h>

#include <cstdlib>

#include <inkview.h>

#include "update_log.h"

namespace {

/* Long enough for a cold Wi-Fi association plus the transfer of a ~600 KB
 * asset over a reader's radio; short enough that a dead AP still gives up. */

/* Every network entry point is resolved at runtime instead of being linked.
 * inkview.h comes from the SDK, the library comes from the firmware, and the
 * two do not always agree: a function declared here but missing there is a
 * lazily-bound symbol that kills the process on the first call rather than
 * failing the load. Looking it up ourselves turns that crash into a message. */
template <typename Fn>
Fn resolve(const char *name)
{
    Fn fn = reinterpret_cast<Fn>(dlsym(RTLD_DEFAULT, name));
    if (fn == nullptr)
        updateLog(QStringLiteral("inkview: %1 missing")
                      .arg(QString::fromLatin1(name)));
    return fn;
}

} // namespace

ScreenSize openInkViewScreen()
{
    InitInkview(TASK_MAKEACTIVE);
    return {ScreenWidth(), ScreenHeight(), PanelHeight()};
}

QString inkViewFontFamily()
{
    const char *family = iv_get_default_font(FONT_FAMILY);
    return family != nullptr ? QString::fromUtf8(family) : QString();
}

QString inkViewLang()
{
    const char *lang = currentLang();
    return lang != nullptr ? QString::fromUtf8(lang) : QString();
}

/* Opening a book is the firmware's own job: OpenBook looks up the handler for
 * the format, starts it as a task and switches the screen to it — the same
 * path the library screen takes, so the reader remembers the position and the
 * shim still starts our daemon.
 *
 * OB_WITHRETURN is what brings the reader back here when the book is closed,
 * rather than to the library; OB_ADDTOLAST keeps the firmware's "recent books"
 * list honest. The return code is logged rather than believed: the header
 * documents none, and what matters to the caller is only that the entry point
 * existed. */
bool inkViewOpenBook(const QString &path)
{
    const auto openBook =
        resolve<int (*)(const char *, const char *, int)>("OpenBook");
    if (openBook == nullptr)
        return false;

    const QByteArray utf8 = path.toUtf8();
    const int rc = openBook(utf8.constData(), nullptr,
                            OB_ADDTOLAST | OB_WITHRETURN);
    updateLog(QStringLiteral("open: OpenBook(%1) = %2").arg(path).arg(rc));
    return true;
}

bool inkViewNetworkUp()
{
    if (const auto queryNetwork = resolve<int (*)()>("QueryNetwork"))
        updateLog(QStringLiteral("net: QueryNetwork = 0x%1")
                      .arg(queryNetwork(), 0, 16));

    /* The state word is not a connection test: 0x202 (NET_WIFI|NET_WIFIREADY)
     * means the radio is up and answered a scan, and a download over it still
     * comes back empty. So always ask to connect — the call returns NET_OK
     * immediately when a connection already exists — and never let QueryNetwork
     * talk us out of it.
     *
     * Silent first: the firmware's own connect dialog draws straight onto the
     * framebuffer under our Qt scene and leaves its remains there. The other
     * two are fallbacks for firmwares that don't export it. */
    if (const auto silent = resolve<int (*)(const char *)>("NetConnectSilent")) {
        const int rc = silent(nullptr);
        updateLog(QStringLiteral("net: NetConnectSilent = %1").arg(rc));
        if (rc == NET_OK)
            return true;
    }
    if (const auto connect2 = resolve<int (*)(const char *, int)>("NetConnect2")) {
        const int rc = connect2(nullptr, 0);
        updateLog(QStringLiteral("net: NetConnect2 = %1").arg(rc));
        if (rc == NET_OK)
            return true;
    }
    if (const auto connect = resolve<int (*)(const char *)>("NetConnect")) {
        const int rc = connect(nullptr);
        updateLog(QStringLiteral("net: NetConnect = %1").arg(rc));
        if (rc == NET_OK)
            return true;
    }

    return false;
}

namespace {

/* Writes a downloaded buffer out and notes what it was: on this firmware a
 * "successful" transfer can still be empty, and an error page is a perfectly
 * ordinary 200, so the first bytes are worth having in the log. */
int writeResult(const void *data, int size, const QByteArray &dest)
{
    if (data == nullptr || size <= 0) {
        updateLog(QStringLiteral("net: empty response"));
        return NET_EFILE;
    }

    /* Readable on a reader screen: a JSON body should be recognisable at a
     * glance, and a zip's binary header only has to look like one. */
    QString head;
    const char *bytes = static_cast<const char *>(data);
    for (int i = 0; i < qMin(size, 40); i++) {
        const char c = bytes[i];
        head += (c >= 0x20 && c < 0x7f) ? QLatin1Char(c) : QLatin1Char('.');
    }
    updateLog(QStringLiteral("net: %1 bytes, starts %2").arg(size).arg(head));

    QFile out(QString::fromUtf8(dest));
    const bool ok = out.open(QIODevice::WriteOnly | QIODevice::Truncate)
                    && out.write(static_cast<const char *>(data), size) == size
                    && out.flush();
    out.close();
    return ok ? NET_OK : NET_EFILE;
}

/* The Quick* family is synchronous: it returns the whole body in one heap
 * buffer. That is what we want here — the session API hands the transfer to a
 * loop InkView only runs for its own applications, which under Qt means the
 * download never actually starts. */
int quickDownload(const QByteArray &url, const QByteArray &dest, int timeout)
{
    int size = 0;

    if (const auto ext3 = resolve<void *(*)(const char *, int *, int, char *,
                                            char *, int *)>("QuickDownloadExt3")) {
        int error = 0;
        void *data = ext3(url.constData(), &size, timeout, nullptr,
                          nullptr, &error);
        updateLog(QStringLiteral("net: QuickDownloadExt3 err = %1").arg(error));
        const int rc = writeResult(data, size, dest);
        if (data != nullptr)
            free(data);
        if (rc == NET_OK)
            return NET_OK;
        return error != 0 ? error : rc;
    }

    if (const auto quick =
            resolve<void *(*)(const char *, int *, int)>("QuickDownload")) {
        void *data = quick(url.constData(), &size, timeout);
        const int rc = writeResult(data, size, dest);
        if (data != nullptr)
            free(data);
        return rc;
    }

    return kInkViewNetUnsupported;
}

} // namespace

int inkViewDownload(const QString &url, const QString &dest, int timeoutSeconds,
                    bool retry)
{
    const QByteArray target = url.toUtf8();
    const QByteArray destPath = dest.toUtf8();

    /* A redirect the firmware did not follow arrives as a short body with a
     * Location we cannot read (this firmware's iv_sessioninfo does not match
     * the SDK header, so its fields are not safe to touch). GitHub's asset URL
     * redirects to a CDN, so try the download and let the caller judge the
     * result by what landed in the file. */
    int rc = quickDownload(target, destPath, timeoutSeconds);
    if (rc != NET_OK && retry) {
        /* An empty body is what a dropped association looks like from here, so
         * reconnect and give it one more go before calling it a failure. The
         * version check does not do this: associated-but-offline is common
         * enough (a captive portal, a router with no uplink), and two timeouts
         * in a row would freeze the screen for two minutes to say the same
         * thing one says in one. */
        updateLog(QStringLiteral("net: retrying after %1").arg(rc));
        if (inkViewNetworkUp())
            rc = quickDownload(target, destPath, timeoutSeconds);
    }
    updateLog(QStringLiteral("net: download rc = %1").arg(rc));
    return rc;
}

QString inkViewNetErrorText(int code)
{
    if (code == kInkViewNetUnsupported)
        return QStringLiteral("no download function in this firmware");
    const auto netError = resolve<const char *(*)(int)>("NetError");
    const char *text = netError != nullptr ? netError(code) : nullptr;
    return text != nullptr ? QString::fromUtf8(text) : QString::number(code);
}
