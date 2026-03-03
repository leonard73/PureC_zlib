# Contributing

Thanks for contributing to PureC_zlib.

## Setup

```bash
make clean build
make test
```

Quick manual smoke test:

```bash
make quickstart
```

## Guidelines

- Keep the project pure C (`-std=c11`) and dependency-free.
- Preserve cross-platform behavior (x86/ARM fallback paths).
- Add tests for edge cases and regressions.
- Update README/docs when CLI or format behavior changes.

## Pull Request Checklist

- [ ] `make build` passes
- [ ] `make test` passes
- [ ] `make quickstart` passes (if relevant)
- [ ] Documentation updated
