# ScratchBird Test Suite

This directory contains automated unit, integration, conformance, and compatibility tests.

Run all tests (after building):

```bash
ctest --test-dir build --output-on-failure
```

For release gating workflows, see:

- `docs/TEST.md`
- `tests/conformance/public_beta/run_required_public_beta_gate.sh`
