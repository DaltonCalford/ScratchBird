# Verification Design Review

Date: 2026-02-27
Scope reviewed: source suite imported from `local_work/verification`

## Key Issues Found

1. Provisioning was Ubuntu-only and root-only.
2. Default cloning behavior pulled all upstream engine repos (heavy, slow, high bandwidth).
3. No single orchestrator for bootstrap -> clone -> build -> verify.
4. Differential comparisons could mask setup/teardown failures by only evaluating exec assertions.
5. No standardized run manifest for reproducibility metadata.
6. Wire/byte parity path existed elsewhere in ScratchBird scripts but was not integrated into this suite flow.

## Hardening Applied In This Bundle

1. Added bundle-level `bootstrap.sh` orchestrator.
2. Added Linux dependency script wrapper (`bootstrap_install_linux.sh`) with explicit behavior.
3. Added clone presets (`core`, `full`, `scratchbird`) to reduce default clone footprint.
4. Added ScratchBird build/test bootstrap script.
5. Added one-command verification runner script for footprint/diff/perf plus optional wire capture.
6. Patched differential runner to fail explicitly on setup/teardown errors.
7. Added run manifest generation and consolidated report index output.

## Remaining Constraints

1. Dependency auto-install path is implemented for APT-based systems.
2. Runner still depends on real endpoints and credentials for true live parity.
3. Performance thresholds remain workload/config driven and should be tuned per host class.

## Usage Intent

This object is intended for reproducible external beta validation from source, not for speculative claim generation.
