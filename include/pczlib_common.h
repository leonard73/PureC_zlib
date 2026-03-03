#ifndef PCZLIB_COMMON_H
#define PCZLIB_COMMON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PCZ_OK = 0,
    PCZ_ERR_ARG = 1,
    PCZ_ERR_OOM = 2,
    PCZ_ERR_IO = 3,
    PCZ_ERR_FORMAT = 4,
    PCZ_ERR_UNSUPPORTED = 5,
    PCZ_ERR_INTERNAL = 6
} pcz_status_t;

const char *pcz_status_string(pcz_status_t st);

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} pcz_buffer_t;

void pcz_buffer_init(pcz_buffer_t *buf);
void pcz_buffer_free(pcz_buffer_t *buf);
pcz_status_t pcz_buffer_reserve(pcz_buffer_t *buf, size_t new_cap);
pcz_status_t pcz_buffer_append_byte(pcz_buffer_t *buf, uint8_t value);
pcz_status_t pcz_buffer_append_data(pcz_buffer_t *buf, const uint8_t *data, size_t n);

#ifdef __cplusplus
}
#endif

#endif
