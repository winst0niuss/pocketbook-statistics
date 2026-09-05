#include "version.h"

#include <ctype.h>

/* A version is three dot-separated numbers, optionally followed by a
 * pre-release suffix: "1.4.0", "v1.4.0", "1.4.0-rc2", "2.1.0-pr7". The suffix
 * number is kept apart from the three because it orders the other way round —
 * 1.4.0-rc2 comes *before* 1.4.0, the way semver has it — and because a build
 * that only knew the three would see every candidate for a release as that
 * release and never offer the next one.
 *
 * Two spellings, one counter. "rc" is a candidate cut by hand; "pr" is a build
 * published from a pull request. CI numbers both from the tags that already
 * exist for that version, so the numbers never collide and the suffix letters
 * carry no order of their own: 2.1.0-pr7 is simply later than 2.1.0-rc3. Any
 * other suffix is ignored, which reads the version as the final release it is
 * built on — the old behaviour, and the safe one, since a final release
 * outranks nothing that follows it. */
static void parse(const char *s, int out[3], int *rc)
{
    out[0] = out[1] = out[2] = 0;
    *rc = 0; /* 0 means a final release, which outranks any candidate */
    if (!s)
        return;
    while (*s == 'v' || *s == 'V' || isspace((unsigned char)*s))
        s++;
    for (int i = 0; i < 3 && isdigit((unsigned char)*s); i++) {
        int n = 0;
        while (isdigit((unsigned char)*s))
            n = n * 10 + (*s++ - '0');
        out[i] = n;
        if (*s != '.')
            break;
        s++;
    }
    if (*s == '-' || *s == '~')
        s++;
    const int is_rc = (s[0] == 'r' || s[0] == 'R') && (s[1] == 'c' || s[1] == 'C');
    const int is_pr = (s[0] == 'p' || s[0] == 'P') && (s[1] == 'r' || s[1] == 'R');
    if (is_rc || is_pr) {
        s += 2;
        if (*s == '.' || *s == '-')
            s++;
        int n = 0;
        while (isdigit((unsigned char)*s))
            n = n * 10 + (*s++ - '0');
        *rc = n > 0 ? n : 1; /* a bare "-rc" or "-pr" is the first one */
    }
}

int version_compare(const char *a, const char *b)
{
    int va[3], vb[3], ra, rb;
    parse(a, va, &ra);
    parse(b, vb, &rb);
    for (int i = 0; i < 3; i++) {
        if (va[i] != vb[i])
            return va[i] < vb[i] ? -1 : 1;
    }
    if (ra == rb)
        return 0;
    if (ra == 0)
        return 1;  /* the release beats its own candidates */
    if (rb == 0)
        return -1;
    return ra < rb ? -1 : 1;
}
