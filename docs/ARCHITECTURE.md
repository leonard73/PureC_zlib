# Architecture

## Modules

- `module_zlib`
  - zlib container handling (header + Adler-32 trailer)
- `module_deflate`
  - DEFLATE compression and decompression
  - compressor: LZ77 + fixed Huffman block generation
  - decompressor: stored / fixed / dynamic block decoding
- `module_compute`
  - backend-specific compute helpers (`match_len`, `adler32`)
  - backends: scalar, avx2, avx512, neon, opencl-stub
- `module_common`
  - status, dynamic buffer, file I/O
- `app/main.c`
  - CLI entrypoint and commands

## Data Flow

Compression:
1. Read input bytes
2. LZ77 tokenization (`literal` / `length-distance`)
3. Fixed Huffman DEFLATE bitstream generation
4. Wrap zlib header + Adler-32

Decompression:
1. Validate zlib header
2. DEFLATE decode (stored/fixed/dynamic)
3. Verify Adler-32 trailer
4. Write output bytes

## Compute Backend Strategy

`match_len` and checksum helpers are dispatched through `pcz_compute_select`.

- `auto` selects the best available backend at runtime.
- Explicit backend selection is available via CLI/Make targets.
- OpenCL path is currently a native C stub with kernel assets prepared in `kernels/`.
