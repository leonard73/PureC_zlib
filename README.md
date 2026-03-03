# PureC_zlib

Pure C (no third-party library) zlib compressor/decompressor project with a Makefile-first workflow.

## Highlights

- Native C implementation of zlib container + DEFLATE codec
- Compression: LZ77 + fixed Huffman encoding
- Decompression: stored / fixed / dynamic Huffman blocks
- Backend abstraction with multiple compute variants:
  - `scalar`
  - `avx2`
  - `avx512`
  - `neon`
  - `opencl` (current stub path + kernel assets)
- CLI + tests + CI ready for GitHub

## Getting Started

### 1) Build

```bash
cd /home/pan/work/github_projects/PureC_zlib
make build
```

### 2) Run quick demo

```bash
make quickstart
```

This generates:
- `bin/quickstart.z`
- `bin/quickstart.out`

### 3) Verify tests

```bash
make test
```

## Main Commands

```bash
make compress INPUT=examples/sample.txt OUTPUT=bin/sample.z BACKEND=auto LEVEL=6
make decompress ZINPUT=bin/sample.z ZOUTPUT=bin/sample.out BACKEND=auto
make roundtrip ROUNDTRIP_INPUT=examples/sample.txt BACKEND=auto LEVEL=6
make info ZINPUT=bin/sample.z
```

## Backend-Specific Commands

Compression:

```bash
make compress_scalar INPUT=... OUTPUT=...
make compress_avx2   INPUT=... OUTPUT=...
make compress_avx512 INPUT=... OUTPUT=...
make compress_neon   INPUT=... OUTPUT=...
make compress_opencl INPUT=... OUTPUT=...
```

Decompression:

```bash
make decompress_scalar ZINPUT=... ZOUTPUT=...
make decompress_avx2   ZINPUT=... ZOUTPUT=...
make decompress_avx512 ZINPUT=... ZOUTPUT=...
make decompress_neon   ZINPUT=... ZOUTPUT=...
make decompress_opencl ZINPUT=... ZOUTPUT=...
```

## CLI Usage

```bash
bin/purec_zlib compress --input in.bin --output out.z --backend auto --level 6
bin/purec_zlib decompress --input out.z --output decoded.bin --backend auto
bin/purec_zlib roundtrip --input in.bin --backend auto --level 6
bin/purec_zlib info --input out.z
```

## Project Layout

- `app/main.c`: CLI
- `include/`: public headers
- `modules/module_zlib/`: zlib wrapper logic
- `modules/module_deflate/`: DEFLATE implementation
- `modules/module_compute/`: scalar/avx2/avx512/neon/opencl compute paths
- `modules/module_common/`: shared utilities
- `tests/`: roundtrip tests
- `kernels/`: OpenCL kernel assets

More details: `docs/ARCHITECTURE.md`

## Quality

Local CI-style check:

```bash
make ci
```

GitHub Actions workflow: `.github/workflows/ci.yml`.

## Notes on OpenCL

- `opencl` backend is integrated as a native C stub for now.
- Kernel source is included in `kernels/opencl_kernels.cl`.
- Roadmap includes full OpenCL runtime execution.

## Contributing

- `CONTRIBUTING.md`
- `ROADMAP.md`
- `.github/ISSUE_TEMPLATE/`

## License

See `LICENSE`.
