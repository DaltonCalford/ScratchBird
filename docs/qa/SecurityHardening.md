### Static Analysis
- `clang-tidy` in CI; `cppcheck` target (`cmake --build build --target cppcheck`).

### Sanitizers
- ASan/UBSan CI matrix via `sanitizers.yml`.

### Dependencies and SBOM
- SBOM with Syft; vulnerabilities scan with Grype; fail on high/critical.

### Input Validation Tests
- Add unit tests covering path parsing, encodings, lengths, and formats.

