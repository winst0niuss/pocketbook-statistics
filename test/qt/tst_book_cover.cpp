/* Cover extraction: the formats this app reads itself, and the cache that is
 * the only thing a finished-and-deleted book has left. */
#include <QImage>
#include <QTest>

#include "book_cover.h"
#include "device_paths.h"
#include "fixtures.h"

extern "C" {
#include "daemon.h"
}

namespace {

/* The dominant colour of the cached cover, which is how a test says "the OPF's
 * image came back, not the chapter illustration next to it". */
QColor colourOf(const QString &path)
{
    QImage img(path);
    if (img.isNull())
        return {};
    return img.pixelColor(img.width() / 2, img.height() / 2);
}

const char *kCoverColour = "#204080";

} // namespace

class TestBookCover : public QObject {
    Q_OBJECT

private slots:
    void init() { device_ = new Device; }
    void cleanup() { delete device_; device_ = nullptr; }

    void extractsTheOpfCover_data();
    void extractsTheOpfCover();
    void comicTakesItsFirstPageInNaturalOrder();
    void comicIgnoresResourceForkJunk();
    void unreadableFormatsGiveNothing();
    void plainPdfIsNeverOpened();
    void cachedCoverIsReturnedWithoutTheBook();
    void aTallCoverIsScaledDown();
    void withoutAKeyThereIsNoCache();
    void anEpubWithoutADeclaredCoverFallsBackToAnItemNamedLikeOne();
    void aComicTriesAFewPagesBeforeGivingUp();
    void zeroPaddedPageNumbersStillSortAsNumbers();

private:
    QString bookAt(const QString &name, const QByteArray &bytes) const
    {
        device_->write(QStringLiteral("/books/") + name, bytes);
        return device_->at(QStringLiteral("/books/") + name);
    }

    Device *device_ = nullptr;
};

void TestBookCover::extractsTheOpfCover_data()
{
    QTest::addColumn<QByteArray>("bytes");
    QTest::addColumn<QString>("name");

    const QByteArray cover = pngBytes(120, 200, kCoverColour);
    QTest::newRow("epub2 <meta name=cover>") << epub2(cover) << "two.epub";
    QTest::newRow("epub3 properties") << epub3(cover) << "three.epub";
    QTest::newRow("bare fb2") << fb2(cover) << "book.fb2";
    QTest::newRow("fb2.zip") << fb2Zip(cover) << "book.fb2.zip";
}

/* Each fixture also carries a second image the extraction must not pick: an
 * EPUB's chapter illustration, an FB2's other <binary>. Guessing at an image
 * inside a book finds those instead of the cover. */
void TestBookCover::extractsTheOpfCover()
{
    QFETCH(QByteArray, bytes);
    QFETCH(QString, name);

    const QString path = bookAt(name, bytes);
    const QString cached = bookCover(path, QStringLiteral("key-") + name);
    QVERIFY2(!cached.isEmpty(), "no cover extracted");
    QCOMPARE(cached, devicePath(OWN_COVER_DIR "/key-") + name + QStringLiteral(".png"));
    QCOMPARE(colourOf(cached).name(), QColor(QLatin1String(kCoverColour)).name());
}

/* "page2" before "page10": plain string order puts page10 on the front cover. */
void TestBookCover::comicTakesItsFirstPageInNaturalOrder()
{
    const QString path = bookAt(QStringLiteral("comic.cbz"),
        cbz({{QStringLiteral("page10.png"), pngBytes(120, 200, "#ff0000")},
             {QStringLiteral("page2.png"), pngBytes(120, 200, kCoverColour)},
             {QStringLiteral("page1.png"), pngBytes(120, 200, "#00ff00")}}));

    /* page1 wins over page2 over page10 — the archive order is deliberately
     * the reverse of the answer. */
    const QString cached = bookCover(path, QStringLiteral("comic"));
    QVERIFY(!cached.isEmpty());
    QCOMPARE(colourOf(cached).name(), QColor(QStringLiteral("#00ff00")).name());
}

void TestBookCover::comicIgnoresResourceForkJunk()
{
    const QString path = bookAt(QStringLiteral("mac.cbz"),
        cbz({{QStringLiteral("__MACOSX/._001.png"), pngBytes(10, 10, "#ff0000")},
             {QStringLiteral(".hidden.png"), pngBytes(10, 10, "#ff0000")},
             {QStringLiteral("001.png"), pngBytes(120, 200, kCoverColour)}}));

    const QString cached = bookCover(path, QStringLiteral("mac"));
    QVERIFY(!cached.isEmpty());
    QCOMPARE(colourOf(cached).name(), QColor(QLatin1String(kCoverColour)).name());
}

/* PDF, DJVU, MOBI and CBR have no decoder here; they fall back to the
 * firmware's cache, which is the caller's business, not this one's. */
void TestBookCover::unreadableFormatsGiveNothing()
{
    const QString path = bookAt(QStringLiteral("book.djvu"),
                                QByteArrayLiteral("AT&TFORM....DJVU"));
    QVERIFY(bookCover(path, QStringLiteral("djvu")).isEmpty());
}

/* A 20 MB PDF read into RAM on every refresh, to look for XML that is not
 * there, costs as much as an extraction and never finds one — so only the .fb2
 * suffix opens a file that is not an archive. */
void TestBookCover::plainPdfIsNeverOpened()
{
    QByteArray pdf = QByteArrayLiteral("%PDF-1.4\n");
    /* An FB2 cover would be found in these bytes if anything looked. */
    pdf += "<binary id=\"x\" content-type=\"image/png\">";
    pdf += pngBytes(120, 200, kCoverColour).toBase64();
    pdf += "</binary>";
    const QString path = bookAt(QStringLiteral("book.pdf"), pdf);
    QVERIFY(bookCover(path, QStringLiteral("pdf")).isEmpty());
}

/* The whole point of the cache: the book is gone and the cover is still there.
 * An empty bookPath asks the cache alone. */
void TestBookCover::cachedCoverIsReturnedWithoutTheBook()
{
    const QString path = bookAt(QStringLiteral("gone.epub"),
                                epub2(pngBytes(120, 200, kCoverColour)));
    const QString cached = bookCover(path, QStringLiteral("gone"));
    QVERIFY(!cached.isEmpty());
    QVERIFY(QFile::remove(path));

    QCOMPARE(bookCover(QString(), QStringLiteral("gone")), cached);
    QVERIFY(bookCover(QString(), QStringLiteral("never-seen")).isEmpty());
}

/* Thumbnails on an e-ink screen; a 2000 px cover is flash nobody reads. */
void TestBookCover::aTallCoverIsScaledDown()
{
    const QString path = bookAt(QStringLiteral("big.epub"),
                                epub2(pngBytes(600, 1000, kCoverColour)));
    const QString cached = bookCover(path, QStringLiteral("big"));
    QVERIFY(!cached.isEmpty());
    QCOMPARE(QImage(cached).height(), 400);
}

void TestBookCover::withoutAKeyThereIsNoCache()
{
    const QString path = bookAt(QStringLiteral("nokey.epub"),
                                epub2(pngBytes(120, 200, kCoverColour)));
    QVERIFY(bookCover(path, QString()).isEmpty());
}

/* Last resort, and only inside the manifest: an item that is an image and whose
 * id says "cover". Still the OPF deciding — guessing at the archive would find
 * chapter illustrations. */
void TestBookCover::anEpubWithoutADeclaredCoverFallsBackToAnItemNamedLikeOne()
{
    const QByteArray container =
        "<?xml version=\"1.0\"?><container><rootfiles>"
        "<rootfile full-path=\"content.opf\"/></rootfiles></container>";
    const QByteArray opf =
        "<?xml version=\"1.0\"?><package><manifest>"
        "<item id=\"ch1\" href=\"ch1.png\" media-type=\"image/png\"/>"
        "<item id=\"the-cover\" href=\"front.png\" media-type=\"image/png\"/>"
        "</manifest></package>";
    const QString path = device_->at(QStringLiteral("/books/loose.epub"));
    QVERIFY(writeZip(path, {{QStringLiteral("META-INF/container.xml"), container},
                            {QStringLiteral("content.opf"), opf},
                            {QStringLiteral("ch1.png"), pngBytes(60, 60, "#ff0000")},
                            {QStringLiteral("front.png"), pngBytes(120, 200, kCoverColour)}}));

    const QString cached = bookCover(path, QStringLiteral("loose"));
    QVERIFY(!cached.isEmpty());
    QCOMPARE(colourOf(cached).name(), QColor(QLatin1String(kCoverColour)).name());
}

/* A comic's first entries are sometimes a scan-group banner or an image Qt has
 * no decoder for. */
void TestBookCover::aComicTriesAFewPagesBeforeGivingUp()
{
    const QString path = bookAt(QStringLiteral("scan.cbz"),
        cbz({{QStringLiteral("000-banner.webp"), QByteArrayLiteral("not an image at all")},
             {QStringLiteral("001.png"), pngBytes(120, 200, kCoverColour)}}));

    const QString cached = bookCover(path, QStringLiteral("scan"));
    QVERIFY(!cached.isEmpty());
    QCOMPARE(colourOf(cached).name(), QColor(QLatin1String(kCoverColour)).name());
}

void TestBookCover::zeroPaddedPageNumbersStillSortAsNumbers()
{
    const QString path = bookAt(QStringLiteral("padded.cbz"),
        cbz({{QStringLiteral("p007.png"), pngBytes(120, 200, "#ff0000")},
             {QStringLiteral("p0002.png"), pngBytes(120, 200, kCoverColour)}}));

    const QString cached = bookCover(path, QStringLiteral("padded"));
    QVERIFY(!cached.isEmpty());
    QCOMPARE(colourOf(cached).name(), QColor(QLatin1String(kCoverColour)).name());
}

QTEST_GUILESS_MAIN(TestBookCover)
#include "tst_book_cover.moc"
