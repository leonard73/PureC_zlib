/*
 * OpenCL kernel prototypes for future acceleration work.
 * Current project ships an opencl-stub backend in native C.
 */

__kernel void match_prefix_len(
    __global const uchar *a,
    __global const uchar *b,
    __global uint *out_len,
    uint max_len) {
    uint i = 0;
    while (i < max_len && a[i] == b[i]) {
        i++;
    }
    out_len[0] = i;
}

__kernel void adler32_chunk(
    __global const uchar *data,
    __global uint *out_s1,
    __global uint *out_s2,
    uint n) {
    uint s1 = 1;
    uint s2 = 0;
    for (uint i = 0; i < n; ++i) {
        s1 += data[i];
        s2 += s1;
    }
    out_s1[0] = s1;
    out_s2[0] = s2;
}
