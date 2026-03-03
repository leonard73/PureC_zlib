#ifndef PCZLIB_COMPUTE_H
#define PCZLIB_COMPUTE_H

#include "pczlib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PCZ_BACKEND_AUTO = 0,
    PCZ_BACKEND_SCALAR = 1,
    PCZ_BACKEND_AVX2 = 2,
    PCZ_BACKEND_AVX512 = 3,
    PCZ_BACKEND_NEON = 4,
    PCZ_BACKEND_OPENCL = 5
} pcz_backend_t;

typedef struct {
    const char *name;
    uint32_t (*adler32)(const uint8_t *data, size_t n, uint32_t seed);
    size_t (*match_len)(const uint8_t *a, const uint8_t *b, size_t max_len);
} pcz_compute_ops_t;

const pcz_compute_ops_t *pcz_compute_select(pcz_backend_t backend);

const pcz_compute_ops_t *pcz_compute_scalar(void);
const pcz_compute_ops_t *pcz_compute_x86_avx2(void);
const pcz_compute_ops_t *pcz_compute_x86_avx512(void);
const pcz_compute_ops_t *pcz_compute_arm_neon(void);
const pcz_compute_ops_t *pcz_compute_opencl(void);

#ifdef __cplusplus
}
#endif

#endif
