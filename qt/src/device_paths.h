#pragma once

#include <QString>

/* Every absolute path this app writes to is a device path: /mnt/ext1/... on the
 * reader, and nowhere a host test may go. POCKETBOOK_STATISTICS_ROOT is
 * prepended to all of them, which is what lets the installer, the shim, the
 * updater and the cover cache be driven against a temporary directory — the
 * same seam the C side already has for its two database paths
 * (POCKETBOOK_STATISTICS_DB, POCKETBOOK_STATISTICS_EXPLORER_DB).
 *
 * Unset on the device, where every path stays exactly what it reads as, and
 * read on every call so one test process can work in several roots.
 */
QString devicePath(const char *absolute);

/* The installed binary — QCoreApplication::applicationFilePath(), except where
 * POCKETBOOK_STATISTICS_APP names one instead. The updater stages beside it and
 * the handover script moves the new file onto it, so a test that let it stand
 * would be pointing that mv at the test runner. */
QString appFilePath();
