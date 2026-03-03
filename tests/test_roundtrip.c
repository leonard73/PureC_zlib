#include "pczlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run_one_case(const uint8_t *input, size_t input_size, pcz_backend_t backend, int level) {
    pcz_buffer_t c;
    pcz_buffer_t d;
    pcz_status_t st;

    st = pcz_compress_buffer(input, input_size, &c, backend, level);
    if (st != PCZ_OK) {
        fprintf(stderr, "compress failed backend=%d level=%d: %s\n", backend, level, pcz_status_string(st));
        return 1;
    }

    st = pcz_decompress_buffer(c.data, c.size, &d, backend);
    if (st != PCZ_OK) {
        fprintf(stderr, "decompress failed backend=%d level=%d: %s\n", backend, level, pcz_status_string(st));
        pcz_buffer_free(&c);
        return 1;
    }

    if (d.size != input_size || (input_size > 0 && memcmp(d.data, input, input_size) != 0)) {
        fprintf(stderr, "mismatch backend=%d level=%d\n", backend, level);
        pcz_buffer_free(&c);
        pcz_buffer_free(&d);
        return 1;
    }

    pcz_buffer_free(&c);
    pcz_buffer_free(&d);
    return 0;
}

int main(void) {
    uint8_t case1[] = "hello hello hello hello hello";
    uint8_t case2[4096];
    uint8_t case3[8192];
    pcz_backend_t backends[] = {
        PCZ_BACKEND_AUTO,
        PCZ_BACKEND_SCALAR,
        PCZ_BACKEND_AVX2,
        PCZ_BACKEND_AVX512,
        PCZ_BACKEND_NEON,
        PCZ_BACKEND_OPENCL
    };
    size_t nb = sizeof(backends) / sizeof(backends[0]);

    const uint8_t known_zlib_hello[] = {
        0x78, 0x9C, 0xCB, 0x48, 0xCD, 0xC9, 0xC9, 0x07, 0x00, 0x06, 0x2C, 0x02, 0x15
    };

    for (size_t i = 0; i < sizeof(case2); ++i) {
        case2[i] = (uint8_t)(i % 251);
    }

    for (size_t i = 0; i < sizeof(case3); ++i) {
        case3[i] = (uint8_t)(((i / 64) % 4 == 0) ? 'A' : (((i / 64) % 4 == 1) ? 'B' : (((i / 64) % 4 == 2) ? 'C' : 'D')));
    }

    for (size_t b = 0; b < nb; ++b) {
        if (run_one_case(case1, sizeof(case1) - 1, backends[b], 1) != 0) {
            return 1;
        }
        if (run_one_case(case2, sizeof(case2), backends[b], 6) != 0) {
            return 1;
        }
        if (run_one_case(case3, sizeof(case3), backends[b], 9) != 0) {
            return 1;
        }
    }

    {
        pcz_buffer_t out;
        pcz_status_t st = pcz_decompress_buffer(known_zlib_hello, sizeof(known_zlib_hello), &out, PCZ_BACKEND_AUTO);
        if (st != PCZ_OK) {
            fprintf(stderr, "known vector decode failed: %s\n", pcz_status_string(st));
            return 1;
        }
        if (out.size != 5 || memcmp(out.data, "hello", 5) != 0) {
            fprintf(stderr, "known vector mismatch\n");
            pcz_buffer_free(&out);
            return 1;
        }
        pcz_buffer_free(&out);
    }

    printf("test_roundtrip passed\n");
    return 0;
}
