#pragma once

#include <QString>

/* Extracts the cover from the book file — EPUB (from the OPF), FB2 and
 * .fb2.zip (the <coverpage> binary), CBZ (the first page) — and caches it as
 * a scaled PNG. Returns the cache path, or empty on failure; an empty
 * bookPath asks the cache alone. */
QString bookCover(const QString &bookPath, const QString &cacheKey);
