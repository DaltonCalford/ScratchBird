# ScratchWork Unified Linux Builder

This directory provides a portable SSH-ready Linux container for building and testing the ScratchBird family repositories and selected database engine sources.

## Scripts in this folder

- `bootstrap-workspace.sh` - one-command workspace bootstrap (sync repos, generate scripts, optional docker start)
- `run.sh` - builds (if needed) and starts the SSH container
- `sync-repos.sh` - clones or refreshes one or many repositories
- `install-workspace-build-scripts.sh` - installs mutable build scripts into the root of a workspace
- `start-scratchbird-environment.sh` is generated into your workspace root
- `Dockerfile` - container image definition

## Recommended workflow for users

1. Set up the workspace and install/update repos.
2. Install local build scripts into the workspace root.
3. Start the container pointing to that workspace.
4. SSH in and run local `build-*.sh`, `test-*.sh`, and environment startup script.

This keeps build scripts outside the image so updates to scripts do not require rebuilding the container.

## 1) Download or refresh source repositories
### Option A (recommended): use the bootstrap script

Copy `bootstrap-workspace.sh` into your planned working directory (for example `~/CliWork`) and run it there.
Also copy `sync-repos.sh`, `install-workspace-build-scripts.sh`, and `run.sh` alongside it, or run it from this dev-unified folder and use `--tools-dir`.

```bash
cd ~/CliWork
bash ./bootstrap-workspace.sh --workspace /home/<user>/CliWork --all --start --ip 127.0.0.5 --port 2222
```

Examples:

```bash
bash ./bootstrap-workspace.sh --workspace /home/<user>/CliWork scratchbird scratchrobin duckdb server
bash ./bootstrap-workspace.sh --workspace /home/<user>/CliWork --all --branch main --force
bash ./bootstrap-workspace.sh --workspace /home/<user>/CliWork --all --start
```

What it does:
- runs `sync-repos.sh` for selected repos
- runs `install-workspace-build-scripts.sh` into `~/CliWork` (build/test/start helpers)
- optionally starts Docker when `--start` is used

### Option B: keep manual control with existing scripts

Copy `sync-repos.sh` into your working directory and run it there.

```bash
cd ~/CliWork
bash ./sync-repos.sh --all
```

### Supported repos

Supported repo aliases include:
`scratchbird`, `scratchbird-ai`, `scratchbird-driver`, `scratchrobin`, `clickhouse`, `duckdb`, `influxdb`, `milvus`, `neo4j`, `mongo`, `redis`, `opensearch`, `cassandra`, `mysql-server`, `postgresql`, `firebird`, `dbeaver`, `server` (alias: `mariadb`).

## 2) Install local build scripts into workspace root

Copy `install-workspace-build-scripts.sh` into the same workspace.

```bash
cd ~/CliWork
bash ./install-workspace-build-scripts.sh --overwrite
```

This writes executable scripts like:
`build-scratchbird.sh`, `build-scratchbird-ai.sh`, `build-scratchrobin.sh`, `build-clickhouse.sh`, `build-duckdb.sh`, `build-server.sh`, `start-scratchbird-environment.sh`, etc., directly in `~/CliWork`.

A `build-all.sh` and `test-all.sh` are also written (or updated). `build-all.sh` runs the available individual build scripts. `test-all.sh` runs the available individual test scripts.

To update these scripts as development evolves, just rerun `install-workspace-build-scripts.sh`; no container rebuild is required.

`bootstrap-workspace.sh` also handles this step automatically for the selected repos.

## 3) Start the SSH container pointing to your workspace

Run `run.sh` and point it at the workspace path.

```bash
cd ~/CliWork/ScratchBird/scripts/dev-unified
./run.sh --workspace /home/<user>/CliWork --ip 127.0.0.5 --port 2222
```

Run from `~/CliWork/ScratchBird/scripts/dev-unified` so the image build context is correct.
Output will include the SSH command, for example:

```text
ssh builder@127.0.0.5 -p 2222
```

## 4) Start local ScratchBird environment

Inside SSH at `/workspace`, run:

```bash
./start-scratchbird-environment.sh \
  --bind-address 127.0.0.5 \
  --native-port 13092 \
  --emulate postgres,mysql,firebird \
  --postgres-port 15432 \
  --mysql-port 13306 \
  --firebird-port 13050
```

Example login output includes:
- native endpoint user/password
- MySQL/Postgres/Firebird emulation endpoints and matching login commands when client tools are installed
- kill command for stopping the server

Add `--foreground` to keep the server log streaming in the current terminal.

## Build inside the container

From inside SSH in `/workspace`:

```bash
./build-scratchbird.sh
./build-scratchbird-ai.sh
./build-server.sh  # MariaDB source in server/
./build-all.sh
./test-all.sh
```

## Optional: one-shot startup build

`run.sh` still supports:

```bash
./run.sh --workspace /home/<user>/CliWork --build ScratchBird
```

This passes through to `/usr/local/bin/build-matrix.sh` inside the image and can be useful for quick smoke starts.

## Notes

- Build tools and compilers are installed in the container image.
- MariaDB is expected in the `server/` directory.
- This setup is intentionally source-mounted (`-v <host-dir>:/workspace`), so any local script updates in your working directory are immediately visible in SSH sessions.
