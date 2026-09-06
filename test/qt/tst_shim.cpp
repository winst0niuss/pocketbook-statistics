/* The open-book shim: the only way anything of ours runs after a reboot, and
 * the only change this app makes that can stop a book from opening. Every
 * assertion here is about the two writes it makes to the user partition —
 * the script, and our name at the front of the application list. */
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QTest>

#include "fixtures.h"
#include "shim.h"

namespace {

const QString kScript = QStringLiteral("/mnt/ext1/system/bin/pbstatistics-open.app");
const QString kUserExt = QStringLiteral("/mnt/ext1/system/config/extensions.cfg");
const QString kBackup =
    QStringLiteral("/mnt/ext1/system/config/extensions.cfg.pbstatistics-backup");
const QString kSysExt = QStringLiteral("/ebrmain/config/extensions.cfg");

/* Entries look like  epub:@EPUB_file:1:reader.app,other.app:ICON_EPUB */
QString entryFor(const QByteArray &cfg, const QString &ext)
{
    for (const QString &line : QString::fromUtf8(cfg).split(QLatin1Char('\n')))
        if (line.startsWith(ext + QLatin1Char(':')))
            return line;
    return QString();
}

QStringList appsFor(const QByteArray &cfg, const QString &ext)
{
    const QStringList fields = entryFor(cfg, ext).split(QLatin1Char(':'));
    if (fields.size() < 5)
        return {};
    return fields.at(3).split(QLatin1Char(','), Qt::SkipEmptyParts);
}

} // namespace

class TestShim : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        device_ = new Device;
        /* What the firmware ships, and what the user partition usually holds:
         * one entry for one of the three formats. */
        device_->write(kSysExt,
                       "epub:@EPUB_file:1:reader.app:ICON_EPUB\n"
                       "fb2:@FB2_file:1:reader.app:ICON_FB2\n"
                       "pdf:@PDF_file:1:pdfviewer.app,reader.app:ICON_PDF\n"
                       "djvu:@DJVU_file:1:pdfviewer.app:ICON_DJVU\n");
        device_->write(kUserExt, "epub:@EPUB_file:1:reader.app:ICON_EPUB\n");
        shim_ = new Shim;
    }
    void cleanup()
    {
        delete shim_;
        delete device_;
        shim_ = nullptr;
        device_ = nullptr;
    }

    void installsForEveryReadingFormat();
    void keepsTheReadersThatWereThere();
    void installingTwiceNamesUsOnce();
    void installedNeedsEveryFormat();
    void removeGivesTheDeviceBack();
    void removeDropsAnEntryThatWasOnlyOurs();
    void refreshReplacesAStaleScript();
    void refreshLeavesAnUpToDateScriptAlone();
    void refreshDoesNothingWhenTheShimIsNotInstalled();
    void aFormatNobodyClaimsGetsAnEntryOfItsOwn();
    void aFailedWriteLeavesTheDeviceAsItWas();

private:
    Device *device_ = nullptr;
    Shim *shim_ = nullptr;
};

void TestShim::installsForEveryReadingFormat()
{
    QVERIFY(!shim_->installed());
    QVERIFY(shim_->install());
    QVERIFY(shim_->installed());

    /* The script is the resource, verbatim, and executable — the firmware runs
     * it in place of the reader. */
    QFile shipped(QStringLiteral(":/shim/open-book.sh"));
    QVERIFY(shipped.open(QIODevice::ReadOnly));
    QCOMPARE(device_->read(kScript), shipped.readAll());
    QVERIFY(QFileInfo(device_->at(kScript)).permission(QFile::ExeOwner));

    const QByteArray cfg = device_->read(kUserExt);
    for (const QString &ext : {QStringLiteral("epub"), QStringLiteral("fb2"),
                               QStringLiteral("pdf")}) {
        const QStringList apps = appsFor(cfg, ext);
        QVERIFY2(!apps.isEmpty(), qPrintable(ext));
        /* First, or the firmware hands the book to the reader directly and
         * nothing starts the daemon. */
        QCOMPARE(apps.first(), QStringLiteral("pbstatistics-open.app"));
    }
    /* Formats we do not read stay untouched — every entry here is a format
     * that stops opening if the shim is broken. */
    QVERIFY(entryFor(cfg, QStringLiteral("djvu")).isEmpty());
    /* Backed up before the first write. */
    QCOMPARE(device_->read(kBackup),
             QByteArrayLiteral("epub:@EPUB_file:1:reader.app:ICON_EPUB\n"));
}

/* Overwriting the application list is how KOReader once made the firmware's own
 * viewers disappear. "Open with" has to keep offering them. */
void TestShim::keepsTheReadersThatWereThere()
{
    QVERIFY(shim_->install());
    const QByteArray cfg = device_->read(kUserExt);

    QCOMPARE(appsFor(cfg, QStringLiteral("epub")),
             QStringList({QStringLiteral("pbstatistics-open.app"),
                          QStringLiteral("reader.app")}));
    /* pdf had no user entry: the firmware's own is where the readers come
     * from, and its icon and internal name come with them. */
    QCOMPARE(appsFor(cfg, QStringLiteral("pdf")),
             QStringList({QStringLiteral("pbstatistics-open.app"),
                          QStringLiteral("pdfviewer.app"),
                          QStringLiteral("reader.app")}));
    QVERIFY(entryFor(cfg, QStringLiteral("pdf")).endsWith(QStringLiteral(":ICON_PDF")));
}

void TestShim::installingTwiceNamesUsOnce()
{
    QVERIFY(shim_->install());
    QVERIFY(shim_->install());

    const QStringList apps = appsFor(device_->read(kUserExt), QStringLiteral("epub"));
    QCOMPARE(apps.count(QStringLiteral("pbstatistics-open.app")), 1);
    QCOMPARE(apps.size(), 2);
}

/* A device set up by hand — or by a build that intercepted fewer formats —
 * must read as "off", so pressing the button finishes the job instead of
 * looking done already. */
void TestShim::installedNeedsEveryFormat()
{
    QVERIFY(shim_->install());
    QVERIFY(shim_->installed());

    QByteArray cfg = device_->read(kUserExt);
    cfg.replace("pbstatistics-open.app,", ""); /* strip us from every entry */
    device_->write(kUserExt, cfg);
    QVERIFY(!shim_->installed());

    /* And the script alone is not enough either way round. */
    device_->write(kUserExt,
                   "epub:@EPUB_file:1:pbstatistics-open.app,reader.app:ICON_EPUB\n");
    QVERIFY(!shim_->installed());

    QVERIFY(shim_->install());
    QVERIFY(QFile::remove(device_->at(kScript)));
    QVERIFY(!shim_->installed());
}

void TestShim::removeGivesTheDeviceBack()
{
    QVERIFY(shim_->install());
    QVERIFY(shim_->remove());

    QVERIFY(!QFile::exists(device_->at(kScript)));
    const QByteArray cfg = device_->read(kUserExt);
    QVERIFY(!cfg.contains("pbstatistics-open.app"));
    /* The reader that was there before is still there... */
    QCOMPARE(appsFor(cfg, QStringLiteral("epub")),
             QStringList({QStringLiteral("reader.app")}));
    /* ...including in the entries that were copied out of the firmware's own
     * table when the user partition had none. */
    QCOMPARE(appsFor(cfg, QStringLiteral("fb2")),
             QStringList({QStringLiteral("reader.app")}));
}

/* An entry that names nothing but us is one we fabricated for a format the
 * firmware has no reader for. Left behind, it would point at a script that is
 * no longer there — so it goes with us. */
void TestShim::removeDropsAnEntryThatWasOnlyOurs()
{
    device_->write(kUserExt,
                   "epub:@EPUB_file:1:pbstatistics-open.app:ICON_EPUB\n"
                   "fb2:@FB2_file:1:pbstatistics-open.app,reader.app:ICON_FB2\n");

    QVERIFY(shim_->remove());

    const QByteArray cfg = device_->read(kUserExt);
    QVERIFY2(entryFor(cfg, QStringLiteral("epub")).isEmpty(),
             cfg.constData());
    QCOMPARE(appsFor(cfg, QStringLiteral("fb2")),
             QStringList({QStringLiteral("reader.app")}));
}

/* An app update ships a new script and nothing else ever rewrites it: two
 * releases of fixes to this file never left the repository that way. */
void TestShim::refreshReplacesAStaleScript()
{
    QVERIFY(shim_->install());
    device_->write(kScript, QByteArrayLiteral("#!/bin/sh\n# an older release\n"));

    shim_->refresh();

    QFile shipped(QStringLiteral(":/shim/open-book.sh"));
    QVERIFY(shipped.open(QIODevice::ReadOnly));
    QCOMPARE(device_->read(kScript), shipped.readAll());
    QVERIFY(QFileInfo(device_->at(kScript)).permission(QFile::ExeOwner));
}

void TestShim::refreshLeavesAnUpToDateScriptAlone()
{
    QVERIFY(shim_->install());
    QFile f(device_->at(kScript));
    QVERIFY(f.open(QIODevice::ReadWrite));
    const QDateTime old = QDateTime::currentDateTime().addSecs(-3600);
    QVERIFY(f.setFileTime(old, QFileDevice::FileModificationTime));
    f.close();

    shim_->refresh();

    QCOMPARE(QFileInfo(device_->at(kScript)).lastModified().toSecsSinceEpoch(),
             old.toSecsSinceEpoch());
}

/* refresh() runs at every startup, including on the many devices that never
 * turned the shim on. It must not install one. */
void TestShim::refreshDoesNothingWhenTheShimIsNotInstalled()
{
    const QByteArray before = device_->read(kUserExt);

    shim_->refresh();

    QVERIFY(!QFile::exists(device_->at(kScript)));
    QCOMPARE(device_->read(kUserExt), before);
}

/* A format neither the user partition nor the firmware names: the entry is
 * fabricated, because without one the shim would never be asked to open it. */
void TestShim::aFormatNobodyClaimsGetsAnEntryOfItsOwn()
{
    device_->write(kSysExt, "epub:@EPUB_file:1:reader.app:ICON_EPUB\n");
    device_->write(kUserExt, QByteArray());

    QVERIFY(shim_->install());

    const QByteArray cfg = device_->read(kUserExt);
    QCOMPARE(entryFor(cfg, QStringLiteral("pdf")),
             QStringLiteral("pdf:@PDF_file:1:pbstatistics-open.app:ICON_PDF"));
    QVERIFY(shim_->installed());
}

/* Every path above the handover is failure-tolerant, because a shim that does
 * not reach its last line is a book that will not open. A script written but
 * an extensions.cfg that could not be is the worst of both, so it rolls back. */
void TestShim::aFailedWriteLeavesTheDeviceAsItWas()
{
    const QByteArray before = device_->read(kUserExt);
    QFile cfg(device_->at(kUserExt));
    QVERIFY(cfg.setPermissions(QFile::ReadOwner));

    const bool ok = shim_->install();

    QVERIFY(cfg.setPermissions(QFile::ReadOwner | QFile::WriteOwner));
    if (!ok) {
        QVERIFY2(!QFile::exists(device_->at(kScript)),
                 "the script must not outlive the entries that name it");
        QCOMPARE(device_->read(kUserExt), before);
    } else {
        /* Running as root, where a read-only file is still writable. */
        QVERIFY(shim_->installed());
    }
}

QTEST_GUILESS_MAIN(TestShim)
#include "tst_shim.moc"
