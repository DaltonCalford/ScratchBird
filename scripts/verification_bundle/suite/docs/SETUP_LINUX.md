# Verification Environment Setup (Linux)

This guide sets up a clean machine to run the ScratchBird verification suite.

For full host provisioning + build/test/verify + packaged diagnostics, use:

```bash
cd /path/to/ScratchBird/scripts/verification_bundle
sudo ./SB_Dev_Bootstrap.sh --yes
```

## 1. Install baseline tooling

```bash
cd /path/to/verification/workspace
./scripts/bootstrap_install_linux.sh --yes
```

Baseline installs:

- build tools: `build-essential`, `cmake`, `ninja-build`, `pkg-config`
- version control/tools: `git`, `curl`, `wget`, `jq`
- Python: `python3`, `python3-pip`, `python3-venv`, `python3-yaml`
- test clients: `postgresql-client`, `default-mysql-client`, `firebird3.0-utils`
- archive/network helpers: `zip`, `unzip`, `xz-utils`, `netcat-openbsd`
- optional/native feature libraries: `liblz4-dev`, `libzstd-dev`, `libgeos-dev`, `libproj-dev`, `libcrypt-dev`
- common libraries for native builds: `libssl-dev`, `zlib1g-dev`, `libreadline-dev`, `libncurses5-dev`, `libncursesw5-dev`, `libicu-dev`, `libxml2-dev`, `libxslt1-dev`, `bison`, `flex`

## 2. Prepare workspace directories

```bash
./scripts/bootstrap_prepare_workspace.sh
./scripts/bootstrap_python_env.sh
```

This creates:

- `repos/`
- `results/`
- `reports/`

## 3. Clone repositories

```bash
./.venv/bin/python scripts/bootstrap_clone_repos.py --config configs/repositories.yaml --preset core --depth 1
```

Clone only ScratchBird family:

```bash
./.venv/bin/python scripts/bootstrap_clone_repos.py --config configs/repositories.yaml --preset scratchbird
```

Clone full engine catalog:

```bash
./.venv/bin/python scripts/bootstrap_clone_repos.py --config configs/repositories.yaml --preset full
```

## 4. Build required binaries

Minimum binaries used by this suite:

- `sb_isql` (for ScratchBird native lane)
- `psql` (PostgreSQL lane)
- `mysql` (MySQL lane)
- `isql-fb` (Firebird lane)

The runner resolves binaries from:

- paths listed in `configs/engines.yaml` `client_candidates`
- system `PATH`

## 5. Configure endpoints

Set environment variables if your ports/users/passwords differ from defaults in `configs/engines.yaml`:

- ScratchBird native: `SB_VERIFY_SB_NATIVE_*`
- ScratchBird Firebird emulation: `SB_VERIFY_SB_FB_*`
- ScratchBird PostgreSQL emulation: `SB_VERIFY_SB_PG_*`
- ScratchBird MySQL emulation: `SB_VERIFY_SB_MY_*`
- Reference Firebird: `SB_VERIFY_REF_FB_*`
- Reference PostgreSQL: `SB_VERIFY_REF_PG_*`
- Reference MySQL: `SB_VERIFY_REF_MY_*`

## 6. Build ScratchBird and run verification

```bash
./scripts/bootstrap_build_scratchbird.sh
./scripts/bootstrap_runtime_stack.sh up
./scripts/run_full_verification.sh
```

To tear down runtime stack after verification:

```bash
./scripts/bootstrap_runtime_stack.sh down
```
