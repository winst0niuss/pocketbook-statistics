#ifndef VERSION_H
#define VERSION_H

/* Compares two release versions. "v0.5.1" and "0.5.1" are the same value,
 * a missing component counts as zero ("1.2" == "1.2.0") and anything after
 * the third number (a "-rc1" suffix) is ignored.
 * Returns -1 if a < b, 0 if equal, 1 if a > b. */
int version_compare(const char *a, const char *b);

#endif
