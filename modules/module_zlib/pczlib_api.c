#include "pczlib.h"
#include "pczlib_internal.h"

#include <string.h>

static pcz_status_t pcz_write_zlib_header(pcz_buffer_t *out, int level) {
    uint8_t cmf = 0x78;
    uint8_t flg;
    uint16_t chk;
    int flevel;

    if (level <= 1) {
        flevel = 0;
    } else if (level <= 5) {
        flevel = 1;
    } else if (level <= 7) {
        flevel = 2;
    } else {
        flevel = 3;
    }

    flg = (uint8_t)(flevel << 6);
    chk = (uint16_t)((cmf << 8) | flg);
    flg = (uint8_t)(flg + ((31 - (chk % 31)) % 31));

    if (pcz_buffer_append_byte(out, cmf) != PCZ_OK || pcz_buffer_append_byte(out, flg) != PCZ_OK) {
        return PCZ_ERR_OOM;
    }
    return PCZ_OK;
}

static uint32_t pcz_read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static pcz_status_t pcz_append_be32(pcz_buffer_t *out, uint32_t v) {
    uint8_t bytes[4];
    bytes[0] = (uint8_t)(v >> 24);
    bytes[1] = (uint8_t)(v >> 16);
    bytes[2] = (uint8_t)(v >> 8);
    bytes[3] = (uint8_t)(v);
    return pcz_buffer_append_data(out, bytes, 4);
}

pcz_status_t pcz_compress_buffer(
    const uint8_t *input,
    size_t input_size,
    pcz_buffer_t *output,
    pcz_backend_t backend,
    int level) {
    pcz_buffer_t deflate;
    pcz_status_t st;
    const pcz_compute_ops_t *ops;
    uint32_t adler;

    if ((!input && input_size > 0) || !output || level < 0 || level > 9) {
        return PCZ_ERR_ARG;
    }

    ops = pcz_compute_select(backend);
    pcz_buffer_init(output);
    pcz_buffer_init(&deflate);

    st = pcz_write_zlib_header(output, level);
    if (st != PCZ_OK) {
        pcz_buffer_free(output);
        return st;
    }

    st = pcz_deflate_compress_fixed(input, input_size, &deflate, ops, level);
    if (st != PCZ_OK) {
        pcz_buffer_free(output);
        return st;
    }

    st = pcz_buffer_append_data(output, deflate.data, deflate.size);
    if (st != PCZ_OK) {
        pcz_buffer_free(&deflate);
        pcz_buffer_free(output);
        return st;
    }

    adler = ops->adler32(input, input_size, 1u);
    st = pcz_append_be32(output, adler);
    pcz_buffer_free(&deflate);
    if (st != PCZ_OK) {
        pcz_buffer_free(output);
        return st;
    }

    return PCZ_OK;
}

pcz_status_t pcz_decompress_buffer(
    const uint8_t *input,
    size_t input_size,
    pcz_buffer_t *output,
    pcz_backend_t backend) {
    const pcz_compute_ops_t *ops;
    uint8_t cmf, flg;
    uint16_t chk;
    uint32_t expected_adler;
    uint32_t got_adler;
    pcz_status_t st;

    if ((!input && input_size > 0) || !output || input_size < 6) {
        return PCZ_ERR_ARG;
    }

    cmf = input[0];
    flg = input[1];
    chk = (uint16_t)((cmf << 8) | flg);

    if ((cmf & 0x0Fu) != 8 || (cmf >> 4) > 7 || (chk % 31) != 0) {
        return PCZ_ERR_FORMAT;
    }
    if ((flg & 0x20u) != 0u) {
        return PCZ_ERR_UNSUPPORTED;
    }

    expected_adler = pcz_read_be32(input + input_size - 4);
    ops = pcz_compute_select(backend);

    st = pcz_deflate_decompress(input + 2, input_size - 6, output, ops);
    if (st != PCZ_OK) {
        return st;
    }

    got_adler = ops->adler32(output->data, output->size, 1u);
    if (got_adler != expected_adler) {
        pcz_buffer_free(output);
        return PCZ_ERR_FORMAT;
    }

    return PCZ_OK;
}

pcz_status_t pcz_compress_file(
    const char *input_path,
    const char *output_path,
    pcz_backend_t backend,
    int level) {
    pcz_buffer_t in;
    pcz_buffer_t out;
    pcz_status_t st;

    if (!input_path || !output_path) {
        return PCZ_ERR_ARG;
    }

    st = pcz_file_read_all(input_path, &in);
    if (st != PCZ_OK) {
        return st;
    }

    st = pcz_compress_buffer(in.data, in.size, &out, backend, level);
    pcz_buffer_free(&in);
    if (st != PCZ_OK) {
        return st;
    }

    st = pcz_file_write_all(output_path, out.data, out.size);
    pcz_buffer_free(&out);
    return st;
}

pcz_status_t pcz_decompress_file(
    const char *input_path,
    const char *output_path,
    pcz_backend_t backend) {
    pcz_buffer_t in;
    pcz_buffer_t out;
    pcz_status_t st;

    if (!input_path || !output_path) {
        return PCZ_ERR_ARG;
    }

    st = pcz_file_read_all(input_path, &in);
    if (st != PCZ_OK) {
        return st;
    }

    st = pcz_decompress_buffer(in.data, in.size, &out, backend);
    pcz_buffer_free(&in);
    if (st != PCZ_OK) {
        return st;
    }

    st = pcz_file_write_all(output_path, out.data, out.size);
    pcz_buffer_free(&out);
    return st;
}
