#include "pczlib_compute.h"

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#define PCZ_IS_X86 1
#else
#define PCZ_IS_X86 0
#endif

#if defined(__aarch64__) || defined(__arm__)
#define PCZ_IS_ARM 1
#else
#define PCZ_IS_ARM 0
#endif

const pcz_compute_ops_t *pcz_compute_select(pcz_backend_t backend) {
    switch (backend) {
        case PCZ_BACKEND_SCALAR:
            return pcz_compute_scalar();
        case PCZ_BACKEND_AVX2:
            return pcz_compute_x86_avx2();
        case PCZ_BACKEND_AVX512:
            return pcz_compute_x86_avx512();
        case PCZ_BACKEND_NEON:
            return pcz_compute_arm_neon();
        case PCZ_BACKEND_OPENCL:
            return pcz_compute_opencl();
        case PCZ_BACKEND_AUTO:
        default:
#if PCZ_IS_X86
            __builtin_cpu_init();
            if (__builtin_cpu_supports("avx512bw") && __builtin_cpu_supports("avx512vl")) {
                return pcz_compute_x86_avx512();
            }
            if (__builtin_cpu_supports("avx2")) {
                return pcz_compute_x86_avx2();
            }
            return pcz_compute_scalar();
#elif PCZ_IS_ARM
            return pcz_compute_arm_neon();
#else
            return pcz_compute_scalar();
#endif
    }
}
