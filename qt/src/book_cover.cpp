#include "book_cover.h"

#include <algorithm>
#include <cstring>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSet>
#include <QStringList>
#include <QXmlStreamReader>

#include "device_paths.h"
#include "miniz.h"

extern "C" {
#include "daemon.h"
}

namespace {

QString cacheDir() { return devicePath(OWN_COVER_DIR); }
/* A comic's first entries are sometimes a scan-group banner or an image Qt
 * cannot decode; try a few before giving up. */
constexpr int kComicTries = 4;
/* An FB2 carries its images base64-inline, so the whole file has to be read.
 * Anything past this is not a book any more and is not worth the RAM. */
constexpr qint64 kMaxPlainBytes = 24 * 1024 * 1024;

QByteArray readZipEntry(mz_zip_archive *zip, const QString &name)
{
    size_t size = 0;
    void *p = mz_zip_reader_extract_file_to_heap(
        zip, name.toUtf8().constData(), &size, 0);
    if (!p)
        return {};
    QByteArray data(static_cast<const char *>(p), int(size));
    mz_free(p);
    return data;
}

QByteArray readZipEntry(mz_zip_archive *zip, mz_uint index)
{
    size_t size = 0;
    void *p = mz_zip_reader_extract_to_heap(zip, index, &size, 0);
    if (!p)
        return {};
    QByteArray data(static_cast<const char *>(p), int(size));
    mz_free(p);
    return data;
}

/* Zip filenames are UTF-8 unless the archive came off Windows; decoding those
 * bytes as UTF-8 turns a name into a row of replacement characters that no
 * longer sorts. Latin-1 keeps every byte distinct, which is all the page
 * order needs. */
QString entryName(const char *raw)
{
    const QString utf8 = QString::fromUtf8(raw);
    return utf8.contains(QChar(u'\ufffd')) ? QString::fromLatin1(raw) : utf8;
}

/* One archive member. Kept by index as well as by name: a comic zipped on
 * Windows carries cp1251 or cp866 filenames, which do not survive being read
 * as UTF-8 and then looked up again by name. */
struct Entry {
    mz_uint index = 0;
    QString name;
};

/* Every file entry in the archive, directories and macOS resource-fork junk
 * dropped. */
QList<Entry> zipEntries(mz_zip_archive *zip)
{
    QList<Entry> entries;
    const mz_uint count = mz_zip_reader_get_num_files(zip);
    for (mz_uint i = 0; i < count; ++i) {
        if (mz_zip_reader_is_file_a_directory(zip, i))
            continue;
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(zip, i, &st))
            continue;
        const QString name = entryName(st.m_filename);
        if (name.startsWith(QLatin1String("__MACOSX/")))
            continue;
        const qsizetype slash = name.lastIndexOf(QLatin1Char('/'));
        if (name.mid(slash + 1).startsWith(QLatin1Char('.')))
            continue;
        entries.append(Entry{i, name});
    }
    return entries;
}

/* Path of the OPF from META-INF/container.xml. */
QString opfPath(mz_zip_archive *zip)
{
    const QByteArray xml = readZipEntry(zip, QStringLiteral("META-INF/container.xml"));
    if (xml.isEmpty())
        return {};
    QXmlStreamReader r(xml);
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement() && r.name() == QLatin1String("rootfile"))
            return r.attributes().value(QLatin1String("full-path")).toString();
    }
    return {};
}

/* Cover href from the OPF: EPUB3 properties="cover-image", else EPUB2
 * <meta name="cover" content="id"> -> manifest item with that id. */
QString coverHref(const QByteArray &opf)
{
    QXmlStreamReader r(opf);
    QString coverId;
    QString byProperty;
    struct Item { QString id, href, mediaType; };
    QList<Item> items;

    while (!r.atEnd()) {
        r.readNext();
        if (!r.isStartElement())
            continue;
        if (r.name() == QLatin1String("meta")) {
            const auto attrs = r.attributes();
            if (attrs.value(QLatin1String("name")) == QLatin1String("cover"))
                coverId = attrs.value(QLatin1String("content")).toString();
        } else if (r.name() == QLatin1String("item")) {
            const auto attrs = r.attributes();
            Item it;
            it.id = attrs.value(QLatin1String("id")).toString();
            it.href = attrs.value(QLatin1String("href")).toString();
            it.mediaType = attrs.value(QLatin1String("media-type")).toString();
            const QString props =
                attrs.value(QLatin1String("properties")).toString();
            if (props.split(QLatin1Char(' '))
                    .contains(QLatin1String("cover-image")))
                byProperty = it.href;
            items.append(it);
        }
    }
    if (!byProperty.isEmpty())
        return byProperty;
    if (!coverId.isEmpty()) {
        for (const Item &it : items)
            if (it.id == coverId)
                return it.href;
    }
    /* Last resort: a manifest item whose id contains "cover" and that is an
     * image. */
    for (const Item &it : items)
        if (it.mediaType.startsWith(QLatin1String("image/"))
                && it.id.contains(QLatin1String("cover"), Qt::CaseInsensitive))
            return it.href;
    return {};
}

/* The EPUB's declared cover. The OPF decides on its own: guessing at an image
 * inside an EPUB picks up chapter illustrations and publisher logos. */
QImage epubImage(mz_zip_archive *zip, const QString &opf)
{
    const QByteArray opfData = readZipEntry(zip, opf);
    const QString href = coverHref(opfData);
    if (href.isEmpty())
        return {};
    /* href is relative to the OPF directory */
    const int slash = opf.lastIndexOf(QLatin1Char('/'));
    const QString base = slash >= 0 ? opf.left(slash + 1) : QString();
    QByteArray img = readZipEntry(zip, QDir::cleanPath(base + href));
    if (img.isEmpty())
        img = readZipEntry(zip, href); /* some EPUBs: from root */
    return QImage::fromData(img);
}

bool isImageEntry(const QString &name)
{
    static const QStringList ext{
        QStringLiteral(".jpg"), QStringLiteral(".jpeg"),
        QStringLiteral(".png"), QStringLiteral(".gif"),
        QStringLiteral(".bmp"), QStringLiteral(".webp")};
    for (const QString &e : ext)
        if (name.endsWith(e, Qt::CaseInsensitive))
            return true;
    return false;
}

/* "page2" before "page10": digit runs compare as numbers, the rest
 * case-insensitively. A comic's cover is its first page, and plain string
 * order puts page10 there instead. */
bool naturalLess(const QString &a, const QString &b)
{
    qsizetype i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i].isDigit() && b[j].isDigit()) {
            const qsizetype si = i, sj = j;
            while (i < a.size() && a[i].isDigit())
                ++i;
            while (j < b.size() && b[j].isDigit())
                ++j;
            QStringView na = QStringView(a).mid(si, i - si);
            QStringView nb = QStringView(b).mid(sj, j - sj);
            while (na.size() > 1 && na.startsWith(QLatin1Char('0')))
                na = na.mid(1);
            while (nb.size() > 1 && nb.startsWith(QLatin1Char('0')))
                nb = nb.mid(1);
            if (na.size() != nb.size())
                return na.size() < nb.size();
            const int cmp = na.compare(nb);
            if (cmp != 0)
                return cmp < 0;
        } else {
            const QChar ca = a[i].toLower(), cb = b[j].toLower();
            if (ca != cb)
                return ca < cb;
            ++i;
            ++j;
        }
    }
    return (a.size() - i) < (b.size() - j);
}

/* A comic archive (CBZ) has no manifest at all — it is pages in page order,
 * so the cover is simply the first image. */
QImage comicImage(mz_zip_archive *zip, const QList<Entry> &entries)
{
    QList<Entry> images;
    for (const Entry &e : entries)
        if (isImageEntry(e.name))
            images.append(e);
    if (images.isEmpty())
        return {};
    std::sort(images.begin(), images.end(),
              [](const Entry &a, const Entry &b) {
                  return naturalLess(a.name, b.name);
              });

    const qsizetype tries = std::min<qsizetype>(kComicTries, images.size());
    for (qsizetype i = 0; i < tries; ++i) {
        const QImage img = QImage::fromData(readZipEntry(zip, images.at(i).index));
        if (!img.isNull())
            return img;
    }
    return {};
}

/* Value of attr="…" (or attr='…') inside one tag. Matches a namespaced
 * attribute too — an FB2 image href is l:href or xlink:href depending on who
 * wrote the file. */
QByteArray attrValue(const QByteArray &tag, const char *name)
{
    for (const char quote : {'"', '\''}) {
        const QByteArray key = QByteArray(name) + '=' + quote;
        const qsizetype at = tag.indexOf(key);
        if (at < 0)
            continue;
        const qsizetype from = at + key.size();
        const qsizetype end = tag.indexOf(quote, from);
        if (end < 0)
            continue;
        return tag.mid(from, end - from);
    }
    return {};
}

/* FB2 keeps its images base64 inside <binary> elements and points at one of
 * them from <coverpage>. Scanned as bytes rather than parsed as XML on
 * purpose: half of these files are windows-1251, and Qt 6 has no codec for
 * that — base64 is ASCII whatever the declaration says. */
QByteArray fb2CoverData(const QByteArray &fb2)
{
    QByteArray wanted;
    const qsizetype cover = fb2.indexOf("<coverpage");
    if (cover >= 0) {
        const qsizetype image = fb2.indexOf("<image", cover);
        const qsizetype close = fb2.indexOf("</coverpage", cover);
        if (image >= 0 && (close < 0 || image < close)) {
            const qsizetype tagEnd = fb2.indexOf('>', image);
            if (tagEnd > 0) {
                wanted = attrValue(fb2.mid(image, tagEnd - image), "href");
                if (wanted.startsWith('#'))
                    wanted.remove(0, 1);
            }
        }
    }

    qsizetype pos = 0;
    while ((pos = fb2.indexOf("<binary", pos)) >= 0) {
        const qsizetype tagEnd = fb2.indexOf('>', pos);
        if (tagEnd < 0)
            break;
        const qsizetype stop = fb2.indexOf("</binary", tagEnd);
        if (stop < 0)
            break;
        const QByteArray tag = fb2.mid(pos, tagEnd - pos);
        const bool match = wanted.isEmpty()
            ? attrValue(tag, "content-type").startsWith("image/")
            : attrValue(tag, "id") == wanted;
        if (match)
            return QByteArray::fromBase64(
                fb2.mid(tagEnd + 1, stop - tagEnd - 1));
        pos = stop;
    }
    return {};
}

QImage fb2Image(const QByteArray &fb2)
{
    const QByteArray data = fb2CoverData(fb2);
    return data.isEmpty() ? QImage() : QImage::fromData(data);
}

/* An .fb2.zip — the way FB2 is usually shipped — is one FB2 in an archive. */
QImage zippedFb2Image(mz_zip_archive *zip, const QList<Entry> &entries)
{
    for (const Entry &e : entries) {
        if (!e.name.endsWith(QLatin1String(".fb2"), Qt::CaseInsensitive))
            continue;
        const QImage img = fb2Image(readZipEntry(zip, e.index));
        if (!img.isNull())
            return img;
    }
    return {};
}

/* Every format we can read ourselves, told apart by what is inside the file
 * rather than by its extension: EPUB, FB2 and .fb2.zip, CBZ. PDF, DJVU, MOBI
 * and CBR need a decoder we do not have — those keep falling back to the
 * firmware's own cover cache. */
QImage extractCover(const QString &bookPath)
{
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (mz_zip_reader_init_file(&zip, bookPath.toUtf8().constData(), 0)) {
        QImage img;
        const QString opf = opfPath(&zip);
        const QList<Entry> entries = zipEntries(&zip);
        if (!opf.isEmpty()) {
            img = epubImage(&zip, opf);
        } else {
            img = zippedFb2Image(&zip, entries);
            if (img.isNull())
                img = comicImage(&zip, entries);
        }
        mz_zip_reader_end(&zip);
        return img;
    }

    /* Not an archive. Only FB2 is worth opening blind: reading a 20 MB PDF
     * into memory on every refresh, to scan it for XML that is not there,
     * costs the same as extracting a cover and never finds one. */
    if (!bookPath.endsWith(QLatin1String(".fb2"), Qt::CaseInsensitive))
        return {};
    if (QFileInfo(bookPath).size() > kMaxPlainBytes)
        return {};
    QFile f(bookPath);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QByteArray data = f.readAll();
    f.close();
    return fb2Image(data);
}

} // namespace

QString bookCover(const QString &bookPath, const QString &cacheKey)
{
    if (cacheKey.isEmpty())
        return {};
    const QString cachePath =
        QStringLiteral("%1/%2.png").arg(cacheDir(), cacheKey);
    if (QFile::exists(cachePath))
        return cachePath;
    if (bookPath.isEmpty() || !QFile::exists(bookPath))
        return {};
    /* A success is cached on disk; a failure would otherwise be paid again on
     * every refresh — re-reading a whole FB2, or decoding the first pages of a
     * comic Qt has no format for. Remembered per file, not per key: the same
     * book is often several copies, and each of them deserves its own try.
     * Kept in memory only, so a replaced file is a restart away. Callers are
     * all on the GUI thread. */
    static QSet<QString> failed;
    if (failed.contains(bookPath))
        return {};

    const QImage image = extractCover(bookPath);
    if (image.isNull()) {
        failed.insert(bookPath);
        return {};
    }
    QDir().mkpath(cacheDir());
    const QImage scaled = image.height() > 400
        ? image.scaledToHeight(400, Qt::SmoothTransformation)
        : image;
    return scaled.save(cachePath, "PNG") ? cachePath : QString();
}
