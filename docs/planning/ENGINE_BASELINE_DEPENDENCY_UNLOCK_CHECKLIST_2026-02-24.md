# Engine Baseline Dependency Unlock Checklist (2026-02-24)

Last updated: 2026-02-24
Scope: Unblock `ENGINE_BASELINE_COMPARISON_TRACKER_2026-02-22.csv` blocked root gates and rerun.

## 1) Root blocker classes observed
1. Missing toolchain executables:
   - `mvn` (Neo4j lane)
   - `bison` (MariaDB configure gate)
   - `conan` (Milvus third-party build; must be Conan v1, not v2)
   - `bazel` (MongoDB build gate)
   - `protoc` (InfluxDB Rust/prost build)
   - Docker daemon/CLI (OpenSearch docker distribution tasks)
2. Compiler version mismatch:
   - ClickHouse requires `clang >= 21`; host had `clang 15` selected.
3. Long-running build/test gates timing out:
   - ScratchBird build matrix compile
   - DuckDB build
   - OpenSearch assemble
   - InfluxDB workspace build (toolchain bootstrap + git fetch)
   - Cassandra `ant artifacts`
   - Redis native tests
   - MySQL `mysql-test-run.pl` smoke
4. Firebird lane command mismatch:
   - CMake route failed against this clone; switched to Autotools (`autogen/configure/make`) per lane workplan.
5. Additional engine-specific hard requirements:
   - Cassandra `ant artifacts` requires Go `>= 1.23.1` for docs generation unless doc generation is skipped.
   - Firebird configure requires `tommath` dev headers unless built with builtin tommath option.
   - MongoDB Bazel lane requires a resolvable C/C++ toolchain (`@@bazel_tools//tools/cpp:toolchain_type`), otherwise `bazel build install-mongod` fails at analysis phase.
   - Firebird build lane also requires `libtool` in path when building bundled `libtommath` (`make[4]: libtool: No such file or directory`).
   - Compatibility compare lanes require converted test trees under:
     - `tests/compatibility/firebird/converted`
     - `tests/compatibility/mysql/converted`
     - `tests/compatibility/postgresql/converted`
   - `ScratchBird-driver` FDW wrapper clients (`sb_pg_isql`, `sb_my_isql`, `sb_fb_isql`) are gated by missing adapter/link dependencies in this tree:
     - `sb_my_isql` unresolved `scratchbird::fdw::MySQLAdapter::*`
     - `sb_fb_isql` unresolved engine symbols (`Database`, `Executor`, `FirebirdQueryCompiler`, `CatalogManager`)
   - PostgreSQL compare lane using generic `sb_isql` fallback currently fails with transport policy mismatch on port `5432` (`TLS is required for inet_listener/managed transport`).

## 2) Minimal Linux package checklist (Debian/Ubuntu)
Run as privileged user:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  libtool \
  maven \
  bison \
  libsnappy-dev \
  liblzo2-dev \
  protobuf-compiler \
  libtommath-dev \
  docker.io \
  python3-venv \
  openjdk-21-jdk \
  ant
```

Notes:
1. `build-essential` ensures local GCC/G++ toolchain discovery for Bazel C++ builds (MongoDB lane).
2. `maven` is required for Neo4j gates.
3. `bison` is hard-required by MariaDB configure.
4. `libsnappy-dev` and `liblzo2-dev` remove non-fatal but important compression dependency gaps in MariaDB.
5. `python3-venv` is used for isolated Python tool installs (Conan/Bazel helper dependencies).
6. `protobuf-compiler` is required for InfluxDB codegen (`protoc`).
7. `docker.io` is required by OpenSearch distribution docker tasks.
8. `libtommath-dev` satisfies Firebird configure dependency.
9. `libtool` is required for Firebird bundled library build stage.

## 3) User-local tools (no root required)
```bash
# Conan v1 (Milvus requires Conan 1.x argument surface)
python3 -m venv ~/.venvs/conan
~/.venvs/conan/bin/pip install -U pip
~/.venvs/conan/bin/pip install 'conan<2'
mkdir -p ~/.local/bin
ln -sf ~/.venvs/conan/bin/conan ~/.local/bin/conan

# Bazel (MongoDB helper script)
python3 ~/CliWork/mongo/buildscripts/install_bazel.py
~/.local/bin/bazel --version
~/.local/bin/bazel build //:buildifier --nobuild
```

## 4) ClickHouse compiler gate
Required: `clang-21` or newer.

Checklist:
1. Install LLVM clang 21 from your approved package source.
2. Verify:
```bash
clang-21 --version
```
3. Export for baseline run:
```bash
export CC=clang-21
export CXX=clang++-21
```

## 5) Firebird lane normalization
Use Autotools flow for this clone:
```bash
cd ~/CliWork/firebird
./autogen.sh --with-builtin-tommath
./configure
make -j2
```

## 6) Cassandra doc-generation dependency
Option A (preferred): install Go >= 1.23.1.

Option B (faster baseline lane path): skip doc generation:
```bash
cd ~/CliWork/cassandra
ant artifacts -Drelease=true -Dant.gen-doc.skip=true
```

## 7) OpenSearch task selection
Preferred no-docker baseline command:
```bash
cd ~/CliWork/OpenSearch
./gradlew :server:assemble -x test
```

If full `assemble` is required and docker daemon is not available, skip docker image tasks explicitly:
```bash
cd ~/CliWork/OpenSearch
./gradlew assemble -x test \
  -x :distribution:docker:buildArm64DockerImage \
  -x :distribution:docker:buildDockerImage \
  -x :distribution:docker:buildPpc64leDockerImage \
  -x :distribution:docker:buildRiscv64DockerImage \
  -x :distribution:docker:buildS390xDockerImage
```

## 8) Rerun sequence
1. Reset blocked rows to pending (leave done rows intact).
2. Rerun tracker harness:

```bash
python3 -u ~/CliWork/ScratchBird/scripts/baseline/run_engine_baseline_tracker.py \
  --tracker ~/CliWork/local_work/docs/planning/ENGINE_BASELINE_COMPARISON_TRACKER_2026-02-22.csv
```

3. Validate no pending rows:
```bash
python3 - <<'PY'
import csv
from collections import Counter
p='~/CliWork/local_work/docs/planning/ENGINE_BASELINE_COMPARISON_TRACKER_2026-02-22.csv'
with open(p.replace('~','/home/dcalford'), newline='', encoding='utf-8') as f:
    rows=list(csv.DictReader(f))
print(Counter(r['status'] for r in rows))
PY
```

## 9) Expected outcomes after checklist
1. Neo4j/MariaDB/Milvus/Mongo root blockers should move from missing-tool failures to real build/test results.
2. Timeout-only gates should reduce after extended command budgets.
3. Remaining blockers should be true build/test or dependency chain blockers, not harness/tooling artifacts.
4. Compare gates are now past missing-asset setup and blocked on runtime parity/transport constraints:
   - Firebird/MySQL: curated SQL failures in converted test subset.
   - PostgreSQL: TLS transport requirement mismatch for current endpoint and no working FDW wrapper binary in this tree.
