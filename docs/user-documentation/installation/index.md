# Installation

- Version: `0.1.0`
- Baseline date: `2026-02-19`

## Supported in 0.1.0

### Source Build

- `building-from-source.md`

### Release Packages

- `linux-tarball.md`
- `docker.md`

## Planned for 0.2.0

- Native installer bundles (Linux package-flow consolidation, Windows/macOS
  installer tracks, guided setup workflows).
- Packaging decision gate: installer bundles vs archive-only release strategy.

## Verification

After installation/build:

```bash
ctest --test-dir build -N
```

Expected baseline test count: `3355`.
