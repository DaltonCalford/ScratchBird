# XOS-007 Linux/Windows Package Contract
Last-Modified: 2026-02-22

## Objective
Define deterministic artifact layout for runtime-only and QA packages across Linux and Windows.

## Package Families
1. `runtime`: production deployment payload only.
2. `qa`: runtime payload plus tests, diagnostics tools, and verification scripts.

## Directory Layout Contract
Base output root:
1. `ScratchBird/release/beta/runtime/`
2. `ScratchBird/release/beta/qa/`

Per-target subdirectories:
1. `linux-x64-gcc`
2. `linux-x64-clang`
3. `windows-x64-msvc`
4. `windows-x64-mingw`

Example:
1. `release/beta/runtime/linux-x64-gcc/`
2. `release/beta/runtime/windows-x64-msvc/`
3. `release/beta/qa/linux-x64-gcc/`
4. `release/beta/qa/windows-x64-msvc/`

## Artifact Naming Contract
1. Runtime archive:
   - Linux: `scratchbird-beta-0.1.0-runtime-<target>.tar.gz`
   - Windows: `scratchbird-beta-0.1.0-runtime-<target>.zip`
2. QA archive:
   - Linux: `scratchbird-beta-0.1.0-qa-<target>.tar.gz`
   - Windows: `scratchbird-beta-0.1.0-qa-<target>.zip`
3. Every archive must include `manifest.json` and `checksums.sha256`.

## Inclusion Rules
### Runtime package
1. Server/listener/manager binaries.
2. Required runtime configuration templates.
3. Required shared libraries and licenses.

### QA package
1. Runtime package content.
2. Test binaries and test runner scripts.
3. Compatibility harness scripts and gate reports.
4. Portability audit evidence for this cycle.

## Git Rules
1. Generated archives are never committed to git.
2. Only manifests, checksums, and textual gate evidence are committed.

## Gate Binding
- Gate: `XOS-GATE-01`
- Tracker row: `XOS-007`
