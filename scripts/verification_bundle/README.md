# ScratchBird Verification Bundle (`SB_Dev_Bootstrap`)

This bundle is the external beta bootstrap object.

Primary entrypoint:

```bash
sudo ./SB_Dev_Bootstrap.sh --yes
```

The script is designed for a fresh Linux machine and produces a support-ready zip artifact containing logs, reports, results, and system diagnostics.

## Exact Commands

Run from the bundle directory:

```bash
cd scripts/verification_bundle
sudo ./SB_Dev_Bootstrap.sh --yes
```

Run with package refresh and safe ScratchBird repo refresh:

```bash
sudo ./SB_Dev_Bootstrap.sh --yes --refresh-packages --refresh-sb-repo --refresh-all-repos
```

Run in a custom workspace location:

```bash
sudo ./SB_Dev_Bootstrap.sh --yes --workspace-root /srv --target-dir sb_beta_verify
```

Keep runtime stack online after verification:

```bash
sudo ./SB_Dev_Bootstrap.sh --yes --keep-runtime-up
```

## What The Script Does

Execution is emitted as numbered stages in logs:

1. `S01_INSTALL_DEPS`: installs build/test/verification tools (APT), including Docker and zip tooling.
2. `S02_SYSTEM_USER_GROUP`: creates or validates `scratchbird` user/group and Docker group membership.
3. `S03_WORKSPACE_PREP`: prepares/chowns workspace for non-root build/test execution.
4. `S04_WORKSPACE_BOOTSTRAP`: creates verification workspace, Python venv, and clones/updates repositories.
5. `S05_REFRESH_SB_REPO` (optional): safe `git fetch` + `git pull --ff-only` for ScratchBird.
6. `S06_BUILD_AND_CTEST`: full ScratchBird build plus `ctest`.
7. `S07_RUNTIME_UP`: starts ScratchBird static runtime and reference DB containers.
8. `S08_VERIFY`: executes footprint + differential + perf + wire-capture (when available).
9. `S09_RUNTIME_DOWN`: stops runtime unless `--keep-runtime-up` is set.
10. `S10_PACKAGE_ARTIFACTS`: packages diagnostics into a zip artifact.

## Output And Support Artifact

Default workspace:

- `/opt/sb_verification/repos`
- `/opt/sb_verification/results`
- `/opt/sb_verification/reports`

Generated support package:

- `/opt/sb_verification/reports/SB_Dev_Bootstrap_<run_id>.zip`
- `/opt/sb_verification/reports/SB_Dev_Bootstrap_<run_id>.zip.sha256`

The zip contains:

- per-step logs
- verification reports and raw results
- repo commit/state snapshot
- tool versions and OS/runtime diagnostics
- `RUN_SUMMARY.md` with pass/fail status for each stage

Send that zip + checksum for remote troubleshooting.

## Script Options

```bash
./SB_Dev_Bootstrap.sh --help
```

Key options:

- `--refresh-packages`: upgrades host packages before dependency install.
- `--refresh-sb-repo`: safe pull of ScratchBird repo (no forced reset).
- `--refresh-all-repos`: updates all existing repos during bootstrap clone stage.
- `--skip-install-deps`: skip apt install when machine is already provisioned.
- `--skip-runtime-start`: skip runtime startup (manual endpoints).
- `--skip-verify`: skip verification lanes (build/test-only pass).
- `--artifact-zip <path>`: write zip to an explicit destination.

## Local Re-Verification (Already Provisioned Host)

If host setup is already complete and you only want a fresh verification pass:

```bash
cd /path/to/ScratchBird
./scripts/run_fresh_local_verification.sh
```

That path archives previous artifacts, resets runtime, reruns verification, and writes a summary markdown report.
