# Installation
Last modified: 2026-02-21

- Version: `0.1.0` (initial early beta)
- Baseline date: `2026-02-21`

## Supported in 0.1.0

### Source Build

- `building-from-source.md`

### Release Packages

- `linux-tarball.md`
- `linux-deb.md`
- `linux-rpm.md`
- `docker.md`

## Install Security Prerequisite (Mandatory)

ScratchBird runtime identity is part of install flow in 0.1.0:

- service user: `scratchbird`
- service group: `scratchbird`

Install flow must create/validate:

- `/var/lib/scratchbird`
- `/var/log/scratchbird`
- `/var/run/scratchbird`

and bootstrap token file:

- `/var/lib/scratchbird/bootstrap.token` (mode `0600`)

Reference helper:

- `tools/install/ensure-service-account.sh`

## Planned for 0.2.0

- Native installer bundles (Linux package-flow consolidation, Windows/macOS installer tracks, guided setup workflows).
- Packaging decision gate: installer bundles vs archive-only release strategy.

## Verification

After installation/build:

```bash
ctest --test-dir build -N
```

Expected baseline test count: `3433`.
