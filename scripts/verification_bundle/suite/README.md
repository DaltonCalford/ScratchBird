# ScratchBird Verification Workspace Template

This directory is a portable verification workspace template.

It supports:

1. Project footprint metrics.
2. Differential behavior comparisons (ScratchBird emulation vs reference engines).
3. Performance/stress workloads.
4. Optional wire/byte parity artifact generation via ScratchBird emulation scripts.
5. Optimizer donor-comparison corpus runs with normalized plan/result scoring.

For full host provisioning (dependencies + user/group + build/test/verify + artifact zip), use the parent bundle entrypoint:

```bash
cd ..
sudo ./SB_Dev_Bootstrap.sh --yes
```

## Layout

- `configs/` engine/project/repository/workload definitions.
- `cases/` differential and workload SQL packs.
- `scripts/` bootstrap, clone, build, and runner scripts.
- `results/` raw run artifacts.
- `reports/` summarized outputs and run manifests.
- `docs/` setup/bootstrapping notes.
- `ctest/` standalone CTest wiring scaffold.

## Standard bootstrap flow

```bash
./scripts/bootstrap_install_linux.sh --yes
./scripts/bootstrap_prepare_workspace.sh
./scripts/bootstrap_python_env.sh

# default clone preset = core (ScratchBird family + MySQL/PostgreSQL/Firebird upstream repos)
./.venv/bin/python scripts/bootstrap_clone_repos.py --config configs/repositories.yaml --preset core --depth 1

./scripts/bootstrap_build_scratchbird.sh
./scripts/bootstrap_runtime_stack.sh up
./scripts/run_full_verification.sh
```

For repeat local verification on an already provisioned machine:

```bash
cd ../../
./run_fresh_local_verification.sh
```

## Presets

- `core`: ScratchBird family + core reference engine repositories (default).
- `full`: ScratchBird family + full engine repository catalog.
- `scratchbird`: ScratchBird family only.

## Runtime stack

`bootstrap_runtime_stack.sh` manages:

- ScratchBird static stack (`example_db_manager.sh static-up/down/status`)
- reference engine containers (PostgreSQL, MySQL, Firebird)

## Optimizer donor-comparison harness

Optimizer comparison lanes use:

- `configs/optimizer_donor_engines.yaml`
- `configs/optimizer_donor_corpus.yaml`
- `scripts/optimizer_compare_runner.py`

The harness is designed to:

- run normalized setup, plan, exec, and teardown stages
- compare ScratchBird emulation listeners against matching PostgreSQL, MySQL, and Firebird donor dialects
- enumerate installed ScratchBird emulated-engine parser bundles through parser `--print-package-scaffold` discovery before running emulated lanes
- fail closed on emulated lanes whose installed parser scaffold does not match the expected parser/compiler/runtime package bundle
- compare ScratchBird native v3 statements against donor engines using explicit intent-equivalent query variants
- keep DuckDB as a native-intent comparator rather than a dialect-emulation comparator
- preserve plan proof lines, result assertion lines, and metric lines
- emit pairwise competitive scoring without requiring identical plan text
