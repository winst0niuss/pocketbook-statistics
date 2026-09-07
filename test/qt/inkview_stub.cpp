#include "inkview_stub.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "inkview_bridge.h"

namespace InkViewStub {

QList<Download> downloads;
QList<QString> requestedUrls;
QList<int> requestedTimeouts;
QList<QString> openedBooks;
QString lang = QStringLiteral("en");
bool networkUp = true;
bool openBookSupported = true;

void reset()
{
    downloads.clear();
    requestedUrls.clear();
    requestedTimeouts.clear();
    openedBooks.clear();
    lang = QStringLiteral("en");
    networkUp = true;
    openBookSupported = true;
}

} // namespace InkViewStub

ScreenSize openInkViewScreen()
{
    return ScreenSize{758, 1024, 80};
}

QString inkViewFontFamily()
{
    return QString();
}

QString inkViewLang()
{
    return InkViewStub::lang;
}

bool inkViewOpenBook(const QString &path)
{
    InkViewStub::openedBooks.append(path);
    return InkViewStub::openBookSupported;
}

bool inkViewNetworkUp()
{
    return InkViewStub::networkUp;
}

int inkViewDownload(const QString &url, const QString &dest, int timeoutSeconds,
                    bool retry)
{
    Q_UNUSED(retry);
    InkViewStub::requestedUrls.append(url);
    InkViewStub::requestedTimeouts.append(timeoutSeconds);

    InkViewStub::Download answer;
    if (!InkViewStub::downloads.isEmpty())
        answer = InkViewStub::downloads.takeFirst();
    if (answer.rc != 0)
        return answer.rc;

    QDir().mkpath(QFileInfo(dest).absolutePath());
    QFile f(dest);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return -1;
    f.write(answer.body);
    f.close();
    return 0;
}

QString inkViewNetErrorText(int code)
{
    return QStringLiteral("stub error %1").arg(code);
}
