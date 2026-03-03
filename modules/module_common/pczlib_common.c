#include "pczlib_common.h"

#include <stdlib.h>
#include <string.h>

const char *pcz_status_string(pcz_status_t st) {
    switch (st) {
        case PCZ_OK:
            return "ok";
        case PCZ_ERR_ARG:
            return "invalid argument";
        case PCZ_ERR_OOM:
            return "out of memory";
        case PCZ_ERR_IO:
            return "io error";
        case PCZ_ERR_FORMAT:
            return "format error";
        case PCZ_ERR_UNSUPPORTED:
            return "unsupported";
        case PCZ_ERR_INTERNAL:
            return "internal error";
        default:
            return "unknown";
    }
}

void pcz_buffer_init(pcz_buffer_t *buf) {
    if (!buf) {
        return;
    }
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

void pcz_buffer_free(pcz_buffer_t *buf) {
    if (!buf) {
        return;
    }
    free(buf->data);
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

pcz_status_t pcz_buffer_reserve(pcz_buffer_t *buf, size_t new_cap) {
    uint8_t *new_data;
    if (!buf) {
        return PCZ_ERR_ARG;
    }
    if (new_cap <= buf->capacity) {
        return PCZ_OK;
    }
    new_data = (uint8_t *)realloc(buf->data, new_cap);
    if (!new_data) {
        return PCZ_ERR_OOM;
    }
    buf->data = new_data;
    buf->capacity = new_cap;
    return PCZ_OK;
}

pcz_status_t pcz_buffer_append_byte(pcz_buffer_t *buf, uint8_t value) {
    pcz_status_t st;
    if (!buf) {
        return PCZ_ERR_ARG;
    }
    if (buf->size == buf->capacity) {
        size_t new_cap = buf->capacity == 0 ? 256 : buf->capacity * 2;
        st = pcz_buffer_reserve(buf, new_cap);
        if (st != PCZ_OK) {
            return st;
        }
    }
    buf->data[buf->size++] = value;
    return PCZ_OK;
}

pcz_status_t pcz_buffer_append_data(pcz_buffer_t *buf, const uint8_t *data, size_t n) {
    pcz_status_t st;
    size_t new_size;

    if (!buf || (!data && n > 0)) {
        return PCZ_ERR_ARG;
    }
    if (n == 0) {
        return PCZ_OK;
    }

    new_size = buf->size + n;
    if (new_size > buf->capacity) {
        size_t new_cap = buf->capacity == 0 ? 256 : buf->capacity;
        while (new_cap < new_size) {
            if (new_cap > (size_t)-1 / 2) {
                return PCZ_ERR_OOM;
            }
            new_cap *= 2;
        }
        st = pcz_buffer_reserve(buf, new_cap);
        if (st != PCZ_OK) {
            return st;
        }
    }

    memcpy(buf->data + buf->size, data, n);
    buf->size += n;
    return PCZ_OK;
}
