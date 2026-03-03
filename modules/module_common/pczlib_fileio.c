#include "pczlib.h"

#include <stdio.h>
#include <stdlib.h>

pcz_status_t pcz_file_read_all(const char *path, pcz_buffer_t *out) {
    FILE *fp;
    long size;

    if (!path || !out) {
        return PCZ_ERR_ARG;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        return PCZ_ERR_IO;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return PCZ_ERR_IO;
    }
    size = ftell(fp);
    if (size < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return PCZ_ERR_IO;
    }

    pcz_buffer_init(out);
    if (size > 0) {
        pcz_status_t st = pcz_buffer_reserve(out, (size_t)size);
        if (st != PCZ_OK) {
            fclose(fp);
            return st;
        }
        out->size = (size_t)size;
        if (fread(out->data, 1, out->size, fp) != out->size) {
            pcz_buffer_free(out);
            fclose(fp);
            return PCZ_ERR_IO;
        }
    }

    fclose(fp);
    return PCZ_OK;
}

pcz_status_t pcz_file_write_all(const char *path, const uint8_t *data, size_t n) {
    FILE *fp;

    if (!path || (!data && n > 0)) {
        return PCZ_ERR_ARG;
    }

    fp = fopen(path, "wb");
    if (!fp) {
        return PCZ_ERR_IO;
    }

    if (n > 0 && fwrite(data, 1, n, fp) != n) {
        fclose(fp);
        return PCZ_ERR_IO;
    }

    fclose(fp);
    return PCZ_OK;
}
