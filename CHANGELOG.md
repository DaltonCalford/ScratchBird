# Changelog

All notable changes to this project are documented here.

This project follows Semantic Versioning.

## [Unreleased]

### Planned for 0.2.0

- Full spec/workplan completion for all partial and planned features.
- Catalog refactor/optimization.
- Emulation parser parity completion and conformance harnesses.
- Native parser normalization and style consistency pass.
- Driver regression pass after normalization/refactor.
- Cross-engine performance benchmark harness and go/no-go gates.
- Installer-bundle packaging decisions and implementation.

## [0.1.0] - 2026-02-19

### Added

- Initial early beta baseline release for native parser + listener + core engine.
- Listener bootstrap ownership contracts with database-owner routing semantics.
- Listener startup port-collision checks that skip unsafe bootstrap.
- Language UDR SQL render endpoint and contract coverage tests.
- Beta release packaging split:
  - runtime-only package
  - QA package (runtime + tests)

### Changed

- Documentation baseline reset to beta `0.1.0` active set.
- Active docs now distinguish implemented behavior vs planned work.

### Verified

- Full clean build completed.
- Full test suite passed: `3355/3355`.

### Packaging

- Full archive produced: `release/scratchbird-beta-20260218-full.tar.gz`.
