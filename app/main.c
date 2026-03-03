#include "pczlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *prog) {
    printf("Usage:\n");
    printf("  %s compress --input in.bin --output out.z [--backend auto|scalar|avx2|avx512|neon|opencl] [--level 0-9]\n", prog);
    printf("  %s decompress --input in.z --output out.bin [--backend auto|scalar|avx2|avx512|neon|opencl]\n", prog);
    printf("  %s roundtrip --input in.bin [--tmp tmp.z] [--output out.bin] [--backend ...] [--level 0-9]\n", prog);
    printf("  %s info --input in.z\n", prog);
}

static const char *arg_value(int argc, char **argv, const char *key) {
    for (int i = 2; i + 1 < argc; ++i) {
        if (strcmp(argv[i], key) == 0) {
            return argv[i + 1];
        }
    }
    return NULL;
}

static pcz_backend_t parse_backend(const char *s) {
    if (!s || strcmp(s, "auto") == 0) {
        return PCZ_BACKEND_AUTO;
    }
    if (strcmp(s, "scalar") == 0) {
        return PCZ_BACKEND_SCALAR;
    }
    if (strcmp(s, "avx2") == 0) {
        return PCZ_BACKEND_AVX2;
    }
    if (strcmp(s, "avx512") == 0) {
        return PCZ_BACKEND_AVX512;
    }
    if (strcmp(s, "neon") == 0) {
        return PCZ_BACKEND_NEON;
    }
    if (strcmp(s, "opencl") == 0) {
        return PCZ_BACKEND_OPENCL;
    }
    return PCZ_BACKEND_AUTO;
}

static int parse_level(const char *s) {
    long v;
    if (!s) {
        return 6;
    }
    v = strtol(s, NULL, 10);
    if (v < 0) {
        v = 0;
    }
    if (v > 9) {
        v = 9;
    }
    return (int)v;
}

static int cmd_compress(int argc, char **argv) {
    const char *in = arg_value(argc, argv, "--input");
    const char *out = arg_value(argc, argv, "--output");
    const char *backend_s = arg_value(argc, argv, "--backend");
    const char *level_s = arg_value(argc, argv, "--level");
    pcz_status_t st;

    if (!in || !out) {
        fprintf(stderr, "compress requires --input and --output\n");
        return 1;
    }

    st = pcz_compress_file(in, out, parse_backend(backend_s), parse_level(level_s));
    if (st != PCZ_OK) {
        fprintf(stderr, "compress failed: %s\n", pcz_status_string(st));
        return 1;
    }

    printf("compressed: %s -> %s\n", in, out);
    return 0;
}

static int cmd_decompress(int argc, char **argv) {
    const char *in = arg_value(argc, argv, "--input");
    const char *out = arg_value(argc, argv, "--output");
    const char *backend_s = arg_value(argc, argv, "--backend");
    pcz_status_t st;

    if (!in || !out) {
        fprintf(stderr, "decompress requires --input and --output\n");
        return 1;
    }

    st = pcz_decompress_file(in, out, parse_backend(backend_s));
    if (st != PCZ_OK) {
        fprintf(stderr, "decompress failed: %s\n", pcz_status_string(st));
        return 1;
    }

    printf("decompressed: %s -> %s\n", in, out);
    return 0;
}

static int cmd_roundtrip(int argc, char **argv) {
    const char *in_path = arg_value(argc, argv, "--input");
    const char *tmp_path = arg_value(argc, argv, "--tmp");
    const char *out_path = arg_value(argc, argv, "--output");
    const char *backend_s = arg_value(argc, argv, "--backend");
    const char *level_s = arg_value(argc, argv, "--level");
    pcz_backend_t backend = parse_backend(backend_s);
    int level = parse_level(level_s);
    pcz_buffer_t in;
    pcz_buffer_t c;
    pcz_buffer_t d;
    pcz_status_t st;

    if (!in_path) {
        fprintf(stderr, "roundtrip requires --input\n");
        return 1;
    }
    if (!tmp_path) {
        tmp_path = "bin/roundtrip.z";
    }
    if (!out_path) {
        out_path = "bin/roundtrip.out";
    }

    st = pcz_file_read_all(in_path, &in);
    if (st != PCZ_OK) {
        fprintf(stderr, "read input failed: %s\n", pcz_status_string(st));
        return 1;
    }

    st = pcz_compress_buffer(in.data, in.size, &c, backend, level);
    if (st != PCZ_OK) {
        fprintf(stderr, "compress failed: %s\n", pcz_status_string(st));
        pcz_buffer_free(&in);
        return 1;
    }

    st = pcz_decompress_buffer(c.data, c.size, &d, backend);
    if (st != PCZ_OK) {
        fprintf(stderr, "decompress failed: %s\n", pcz_status_string(st));
        pcz_buffer_free(&in);
        pcz_buffer_free(&c);
        return 1;
    }

    if (d.size != in.size || (d.size > 0 && memcmp(d.data, in.data, in.size) != 0)) {
        fprintf(stderr, "roundtrip mismatch\n");
        pcz_buffer_free(&in);
        pcz_buffer_free(&c);
        pcz_buffer_free(&d);
        return 1;
    }

    st = pcz_file_write_all(tmp_path, c.data, c.size);
    if (st == PCZ_OK) {
        st = pcz_file_write_all(out_path, d.data, d.size);
    }

    if (st != PCZ_OK) {
        fprintf(stderr, "write output failed: %s\n", pcz_status_string(st));
        pcz_buffer_free(&in);
        pcz_buffer_free(&c);
        pcz_buffer_free(&d);
        return 1;
    }

    printf("roundtrip ok: input=%zu compressed=%zu ratio=%.2f%%\n",
           in.size,
           c.size,
           in.size == 0 ? 0.0 : ((double)c.size * 100.0 / (double)in.size));
    printf("wrote: %s and %s\n", tmp_path, out_path);

    pcz_buffer_free(&in);
    pcz_buffer_free(&c);
    pcz_buffer_free(&d);
    return 0;
}

static int cmd_info(int argc, char **argv) {
    const char *in = arg_value(argc, argv, "--input");
    pcz_buffer_t data;
    pcz_buffer_t out;
    pcz_status_t st;

    if (!in) {
        fprintf(stderr, "info requires --input\n");
        return 1;
    }

    st = pcz_file_read_all(in, &data);
    if (st != PCZ_OK) {
        fprintf(stderr, "read failed: %s\n", pcz_status_string(st));
        return 1;
    }

    if (data.size < 6) {
        fprintf(stderr, "not a valid zlib stream (too small)\n");
        pcz_buffer_free(&data);
        return 1;
    }

    printf("input: %s\n", in);
    printf("size: %zu bytes\n", data.size);
    printf("cmf: 0x%02X, flg: 0x%02X\n", data.data[0], data.data[1]);

    st = pcz_decompress_buffer(data.data, data.size, &out, PCZ_BACKEND_AUTO);
    if (st != PCZ_OK) {
        printf("decompress validation: failed (%s)\n", pcz_status_string(st));
        pcz_buffer_free(&data);
        return 1;
    }

    printf("decompress validation: ok\n");
    printf("uncompressed size: %zu bytes\n", out.size);

    pcz_buffer_free(&data);
    pcz_buffer_free(&out);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "compress") == 0) {
        return cmd_compress(argc, argv);
    }
    if (strcmp(argv[1], "decompress") == 0) {
        return cmd_decompress(argc, argv);
    }
    if (strcmp(argv[1], "roundtrip") == 0) {
        return cmd_roundtrip(argc, argv);
    }
    if (strcmp(argv[1], "info") == 0) {
        return cmd_info(argc, argv);
    }
    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage(argv[0]);
        return 0;
    }

    usage(argv[0]);
    return 1;
}
