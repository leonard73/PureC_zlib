#include "pczlib_compute.h"

#define ADLER_BASE 65521u
#define ADLER_NMAX 5552u

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define PCZ_HAS_NEON 1
#else
#define PCZ_HAS_NEON 0
#endif

static uint32_t neon_adler32(const uint8_t *data, size_t n, uint32_t seed) {
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

static size_t neon_match_len(const uint8_t *a, const uint8_t *b, size_t max_len) {
    size_t i = 0;
#if PCZ_HAS_NEON
    while (i + 16 <= max_len) {
        uint8x16_t va = vld1q_u8(a + i);
        uint8x16_t vb = vld1q_u8(b + i);
        uint8x16_t cmp = vceqq_u8(va, vb);
#if defined(__aarch64__)
        uint64_t lo = vgetq_lane_u64(vreinterpretq_u64_u8(cmp), 0);
        uint64_t hi = vgetq_lane_u64(vreinterpretq_u64_u8(cmp), 1);
        if (lo != 0xFFFFFFFFFFFFFFFFULL || hi != 0xFFFFFFFFFFFFFFFFULL) {
            break;
        }
#else
        uint8_t lanes[16];
        vst1q_u8(lanes, cmp);
        for (int j = 0; j < 16; ++j) {
            if (lanes[j] != 0xFF) {
                return i + (size_t)j;
            }
        }
#endif
        i += 16;
    }
#endif
    while (i < max_len && a[i] == b[i]) {
        ++i;
    }
    return i;
}

static const pcz_compute_ops_t g_neon_ops = {
    .name = "neon",
    .adler32 = neon_adler32,
    .match_len = neon_match_len,
};

const pcz_compute_ops_t *pcz_compute_arm_neon(void) {
    return &g_neon_ops;
}
