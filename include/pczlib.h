#ifndef PCZLIB_H
#define PCZLIB_H

#include "pczlib_common.h"
#include "pczlib_compute.h"

#ifdef __cplusplus
extern "C" {
#endif

pcz_status_t pcz_compress_buffer(
    const uint8_t *input,
    size_t input_size,
    pcz_buffer_t *output,
    pcz_backend_t backend,
    int level);

pcz_status_t pcz_decompress_buffer(
    const uint8_t *input,
    size_t input_size,
    pcz_buffer_t *output,
    pcz_backend_t backend);

pcz_status_t pcz_compress_file(
    const char *input_path,
    const char *output_path,
    pcz_backend_t backend,
    int level);

pcz_status_t pcz_decompress_file(
    const char *input_path,
    const char *output_path,
    pcz_backend_t backend);

pcz_status_t pcz_file_read_all(const char *path, pcz_buffer_t *out);
pcz_status_t pcz_file_write_all(const char *path, const uint8_t *data, size_t n);

#ifdef __cplusplus
}
#endif

#endif
