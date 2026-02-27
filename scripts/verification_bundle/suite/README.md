# ScratchBird Verification Workspace Template

This directory is a portable verification workspace template.

It supports:

1. Project footprint metrics.
2. Differential behavior comparisons (ScratchBird emulation vs reference engines).
3. Performance/stress workloads.
4. Optional wire/byte parity artifact generation via ScratchBird emulation scripts.

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
