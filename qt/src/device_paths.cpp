#include "device_paths.h"

#include <QCoreApplication>
#include <QDir>

namespace {

QString root()
{
    return QString::fromLocal8Bit(qgetenv("POCKETBOOK_STATISTICS_ROOT"));
}

} // namespace

QString devicePath(const char *absolute)
{
    return devicePath(QString::fromLatin1(absolute));
}

QString devicePath(const QString &absolute)
{
    const QString prefix = root();
    return prefix.isEmpty() ? absolute : prefix + absolute;
}

QString appFilePath()
{
    const QByteArray override = qgetenv("POCKETBOOK_STATISTICS_APP");
    if (!override.isEmpty())
        return QString::fromLocal8Bit(override);
    return QCoreApplication::applicationFilePath();
}
