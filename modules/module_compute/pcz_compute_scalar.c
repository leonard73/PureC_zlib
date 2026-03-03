#include "pczlib_compute.h"

#define ADLER_BASE 65521u
#define ADLER_NMAX 5552u

static uint32_t scalar_adler32(const uint8_t *data, size_t n, uint32_t seed) {
    uint32_t s1 = seed & 0xFFFFu;
    uint32_t s2 = (seed >> 16) & 0xFFFFu;

    if (seed == 0) {
        s1 = 1;
        s2 = 0;
    }

    while (n > 0) {
        size_t block = n > ADLER_NMAX ? ADLER_NMAX : n;
        n -= block;
        while (block--) {
            s1 += *data++;
            s2 += s1;
        }
        s1 %= ADLER_BASE;
        s2 %= ADLER_BASE;
    }

    return (s2 << 16) | s1;
}

static size_t scalar_match_len(const uint8_t *a, const uint8_t *b, size_t max_len) {
    size_t i = 0;
    while (i < max_len && a[i] == b[i]) {
        ++i;
    }
    return i;
}

static const pcz_compute_ops_t g_scalar_ops = {
    .name = "scalar",
    .adler32 = scalar_adler32,
    .match_len = scalar_match_len,
};

const pcz_compute_ops_t *pcz_compute_scalar(void) {
    return &g_scalar_ops;
}
