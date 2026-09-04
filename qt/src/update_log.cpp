#include "update_log.h"

#include <QFile>
#include <QStringList>

extern "C" {
#include "log.h"
}

/* A thin bridge onto the C logger, so the daemon — which has no Qt — and the
 * app write the same file, with one rotation policy between them. */
void updateLog(const QString &line)
{
    pb_log("%s", line.toUtf8().constData());
}

QString updateLogTail(int lines)
{
    QFile f(QString::fromLatin1(PB_LOG_PATH));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QStringList all = QString::fromUtf8(f.readAll())
                                .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    f.close();
    return all.mid(qMax(0, all.size() - lines)).join(QLatin1Char('\n'));
}
