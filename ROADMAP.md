# Roadmap

## Near-term

- [ ] Real OpenCL runtime implementation (device query, buffers, kernel dispatch)
- [ ] More unit tests for invalid/corrupted streams
- [ ] Add benchmark target and backend comparison table
- [ ] Improve compression ratio with optional dynamic Huffman encoder

## Mid-term

- [ ] Streaming API (`init/update/final`) for large files
- [ ] Optional preset dictionary support
- [ ] Add fuzzing harness for decompressor robustness

## Long-term

- [ ] SIMD-optimized checksum path per backend
- [ ] Pluggable parallel chunk compression mode
- [ ] Multi-format wrappers (gzip/raw-deflate compatibility modes)
