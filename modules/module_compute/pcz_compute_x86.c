#include "pczlib_compute.h"

#define ADLER_BASE 65521u
#define ADLER_NMAX 5552u

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#include <immintrin.h>
#define PCZ_HAS_X86 1
#else
#define PCZ_HAS_X86 0
#endif

static uint32_t x86_adler32(const uint8_t *data, size_t n, uint32_t seed) {
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

#if PCZ_HAS_X86
__attribute__((target("avx2")))
static size_t avx2_match_len_impl(const uint8_t *a, const uint8_t *b, size_t max_len) {
    size_t i = 0;
    while (i + 32 <= max_len) {
        __m256i va = _mm256_loadu_si256((const __m256i *)(const void *)(a + i));
        __m256i vb = _mm256_loadu_si256((const __m256i *)(const void *)(b + i));
        __m256i cmp = _mm256_cmpeq_epi8(va, vb);
        uint32_t mask = (uint32_t)_mm256_movemask_epi8(cmp);
        if (mask != 0xFFFFFFFFu) {
            return i + (size_t)__builtin_ctz(~mask);
        }
        i += 32;
    }
    while (i < max_len && a[i] == b[i]) {
        ++i;
    }
    return i;
}

__attribute__((target("avx512bw,avx512vl")))
static size_t avx512_match_len_impl(const uint8_t *a, const uint8_t *b, size_t max_len) {
    size_t i = 0;
    while (i + 64 <= max_len) {
        __m512i va = _mm512_loadu_si512((const void *)(a + i));
        __m512i vb = _mm512_loadu_si512((const void *)(b + i));
        __mmask64 mask = _mm512_cmpeq_epi8_mask(va, vb);
        if (mask != 0xFFFFFFFFFFFFFFFFULL) {
            return i + (size_t)__builtin_ctzll(~(unsigned long long)mask);
        }
        i += 64;
    }
    while (i < max_len && a[i] == b[i]) {
        ++i;
    }
    return i;
}

static size_t avx2_match_len(const uint8_t *a, const uint8_t *b, size_t max_len) {
    if (__builtin_cpu_supports("avx2")) {
        return avx2_match_len_impl(a, b, max_len);
    }
    return scalar_match_len(a, b, max_len);
}

static size_t avx512_match_len(const uint8_t *a, const uint8_t *b, size_t max_len) {
    if (__builtin_cpu_supports("avx512bw") && __builtin_cpu_supports("avx512vl")) {
        return avx512_match_len_impl(a, b, max_len);
    }
    if (__builtin_cpu_supports("avx2")) {
        return avx2_match_len_impl(a, b, max_len);
    }
    return scalar_match_len(a, b, max_len);
}
#else
static size_t avx2_match_len(const uint8_t *a, const uint8_t *b, size_t max_len) {
    return scalar_match_len(a, b, max_len);
}

static size_t avx512_match_len(const uint8_t *a, const uint8_t *b, size_t max_len) {
    return scalar_match_len(a, b, max_len);
}
#endif

static const pcz_compute_ops_t g_avx2_ops = {
    .name = "avx2",
    .adler32 = x86_adler32,
    .match_len = avx2_match_len,
};

static const pcz_compute_ops_t g_avx512_ops = {
    .name = "avx512",
    .adler32 = x86_adler32,
    .match_len = avx512_match_len,
};

const pcz_compute_ops_t *pcz_compute_x86_avx2(void) {
    return &g_avx2_ops;
}

const pcz_compute_ops_t *pcz_compute_x86_avx512(void) {
    return &g_avx512_ops;
}
