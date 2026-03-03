#ifndef PCZLIB_INTERNAL_H
#define PCZLIB_INTERNAL_H

#include "pczlib_common.h"
#include "pczlib_compute.h"

pcz_status_t pcz_deflate_compress_fixed(
    const uint8_t *input,
    size_t input_size,
    pcz_buffer_t *out_deflate,
    const pcz_compute_ops_t *ops,
    int level);

pcz_status_t pcz_deflate_decompress(
    const uint8_t *deflate_data,
    size_t deflate_size,
    pcz_buffer_t *output,
    const pcz_compute_ops_t *ops);

#endif
