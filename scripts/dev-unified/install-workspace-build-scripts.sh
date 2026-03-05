#!/usr/bin/env bash
if [ -z "${BASH_VERSION:-}" ]; then
  exec bash "$0" "$@"
fi

set -euo pipefail

BASE_DIR="$(pwd)"
OVERWRITE=0
TARGETS=()

KNOWN_PROJECTS=(
  scratchbird
  scratchbird-ai
  scratchbird-driver
  scratchrobin
  duckdb
  influxdb
  mongo
  redis
  milvus
  opensearch
  neo4j
  mysql-server
  postgresql
  firebird
  clickhouse
  cassandra
  dbeaver
  server
)

usage() {
  cat <<'EOF'
Usage: install-workspace-build-scripts.sh [options] [project ...]

Options:
  --base-dir <path>   Root directory containing project repos (default: current directory)
  --overwrite         Replace existing scripts
  --help              Show this help

If no projects are provided, scripts are installed for all supported projects.
Project names:
  scratchbird, scratchbird-ai, scratchbird-driver, scratchrobin, duckdb, influxdb,
  mongo, redis, milvus, opensearch, neo4j, mysql-server, postgresql,
  firebird, clickhouse, cassandra, dbeaver, server
  (mariadb is handled as alias for server)
Generated scripts:
  build-*.sh      build scripts for selected projects
  test-*.sh       test/smoke scripts for selected projects
  build-all.sh
  test-all.sh
  start-scratchbird-environment.sh
EOF
}

normalize_project() {
  local project="$1"
  project="${project,,}"
  project="${project//_/-}"
  case "$project" in
    clickhouse*)
      project="clickhouse"
      ;;
    mongodb)
      project="mongo"
      ;;
    mariadb|maria-db)
      project="server"
      ;;
    *)
      ;;
  esac
  echo "$project"
}

is_known_project() {
  local project="$1"
  local known
  for known in "${KNOWN_PROJECTS[@]}"; do
    if [ "$project" = "$known" ]; then
      return 0
    fi
  done
  return 1
}

emit_scratchbird() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

NPROC="${NPROC:-$(nproc)}"

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/ScratchBird"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
log "Building ScratchBird in $PROJECT_DIR"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j "$NPROC"
if [ -f build/CTestTestfile.cmake ]; then
  ctest --test-dir build --output-on-failure
fi
EOF
}

emit_scratchbird_ai() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/ScratchBird-ai"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ ! -d .venv ]; then
  python3 -m venv .venv
fi
source .venv/bin/activate
python3 -m pip install -U pip setuptools wheel
if [ -f pyproject.toml ] || [ -f setup.py ] || [ -f setup.cfg ]; then
  python3 -m pip install -e ".[mcp]" || python3 -m pip install -e ".[test]" || python3 -m pip install -e .
fi
python3 -m unittest discover -s tests -p 'test_*.py'
if [ -x tools/smoke_http_contract.py ]; then
  PYTHONPATH=src python3 tools/smoke_http_contract.py --mode selftest
fi
if [ -f tools/validate_evidence_gates.py ]; then
  python3 tools/validate_evidence_gates.py --repo-root .
fi
EOF
}

emit_scratchbird_driver() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

NPROC="${NPROC:-$(nproc)}"

log() {
  echo "[$(date +'%F %T')] $*"
}

run_in_dir() {
  local dir="$1"
  shift
  if [ -d "$dir" ]; then
    (cd "$dir" && "$@")
  fi
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/ScratchBird-driver"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
log "Building ScratchBird-driver (best-effort)"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

if command -v go >/dev/null 2>&1; then
  run_in_dir tracks/alpha/drivers/go go test ./...
else
  log "skip Go driver: go not installed"
fi

if [ -f tracks/alpha/drivers/node/package.json ]; then
  run_in_dir tracks/alpha/drivers/node npm install
  run_in_dir tracks/alpha/drivers/node npm run build --if-present
  run_in_dir tracks/alpha/drivers/node npm test --if-present
else
  log "skip Node driver: package missing"
fi

if [ -d tracks/alpha/drivers/python ]; then
  run_in_dir tracks/alpha/drivers/python bash -lc '
    python3 -m venv .venv
    source .venv/bin/activate
    python -m pip install -U pip
    python -m pip install -e ".[test]"
    python -m pytest
  '
else
  log "skip Python driver: missing directory"
fi

if command -v cargo >/dev/null 2>&1 && [ -d tracks/alpha/drivers/rust ]; then
  run_in_dir tracks/alpha/drivers/rust cargo test
fi

if command -v composer >/dev/null 2>&1 && [ -f tracks/alpha/drivers/php/composer.json ]; then
  run_in_dir tracks/alpha/drivers/php bash -lc '
    composer install
    if [ -x vendor/bin/phpunit ]; then
      vendor/bin/phpunit tests
    fi
  '
fi

if command -v fpc >/dev/null 2>&1 && [ -f tracks/alpha/drivers/pascal/tests/TlsCryptoAndPolicyTests.pas ]; then
  run_in_dir tracks/alpha/drivers/pascal fpc -Mdelphi -Fu./src -FE./tests ./tests/TlsCryptoAndPolicyTests.pas
fi

if command -v dotnet >/dev/null 2>&1 && [ -f tracks/alpha/drivers/dotnet/src/ScratchBird.Data/ScratchBird.Data.csproj ]; then
  run_in_dir tracks/alpha/drivers/dotnet dotnet build src/ScratchBird.Data/ScratchBird.Data.csproj
  run_in_dir tracks/alpha/drivers/dotnet dotnet test
fi

if [ -x tracks/alpha/drivers/jdbc/gradlew ]; then
  run_in_dir tracks/alpha/drivers/jdbc ./gradlew build
elif command -v gradle >/dev/null 2>&1; then
  run_in_dir tracks/alpha/drivers/jdbc gradle build
fi

if [ -d tracks/alpha/drivers/odbc ]; then
  cmake -S tracks/alpha/drivers/odbc -B build-odbc -DCMAKE_BUILD_TYPE=Release
  cmake --build build-odbc -j "$NPROC"
fi
if [ -d tracks/beta/drivers/cpp ]; then
  cmake -S tracks/beta/drivers/cpp -B build-cpp -DCMAKE_BUILD_TYPE=Release
  cmake --build build-cpp -j "$NPROC"
fi
if [ -d tracks/alpha/drivers/cli ]; then
  cmake -S tracks/alpha/drivers/cli -B build-cli -DCMAKE_BUILD_TYPE=Release -DSB_BUILD_CLI=ON -DSB_BUILD_CPP=ON -DSB_BUILD_ODBC=OFF
  cmake --build build-cli -j "$NPROC"
fi
EOF
}

emit_scratchrobin() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

NPROC="${NPROC:-$(nproc)}"

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/ScratchRobin"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
log "Building ScratchRobin in $PROJECT_DIR"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j "$NPROC"
if [ -f build/CTestTestfile.cmake ]; then
  ctest --test-dir build --output-on-failure
fi
if [ -x build/scratchrobin_tool ]; then
  ./build/scratchrobin_tool --runtime-startup
  ./build/scratchrobin_tool --release-gate-check
  ./build/scratchrobin_tool --validate-package-manifest=resources/templates/package_profile_manifest.example.json
  ./build/scratchrobin_tool --check-package-artifacts=resources/templates/package_profile_manifest.example.json
fi
if [ -x tools/run_conformance_gate.sh ]; then
  ./tools/run_conformance_gate.sh ./build
fi
EOF
}

emit_duckdb() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/duckdb"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if command -v make >/dev/null 2>&1; then
  make
  make unit
  make allunit
else
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j "${NPROC:-$(nproc)}"
  if [ -f build/CTestTestfile.cmake ]; then
    ctest --test-dir build --output-on-failure
  fi
fi
EOF
}

emit_influxdb() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/influxdb"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if ! command -v cargo >/dev/null 2>&1; then
  echo "cargo is required for InfluxDB build"
  exit 1
fi
cargo build
if command -v cargo-nextest >/dev/null 2>&1; then
  cargo nextest run --workspace || cargo test
else
  cargo test
fi
EOF
}

emit_mongo() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/mongo"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
python3 buildscripts/install_bazel.py
export PATH="${HOME}/.local/bin:${PATH}"
bazel build install-dist-test
EOF
}

emit_redis() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

NPROC="${NPROC:-$(nproc)}"
log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/redis"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
make -j "$NPROC"
make test
EOF
}

emit_milvus() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

NPROC="${NPROC:-$(nproc)}"
log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/milvus"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
bash scripts/install_deps.sh
make -j "$NPROC"
EOF
}

emit_opensearch() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/OpenSearch"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
./gradlew localDistro
./gradlew check
EOF
}

emit_neo4j() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/neo4j"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
export MAVEN_OPTS="${MAVEN_OPTS:--Xmx2048m}"
mvn clean install -T1C
EOF
}

emit_mysql_server() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

NPROC="${NPROC:-$(nproc)}"
log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/mysql-server"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ -x configure ]; then
  ./configure
  make -j "$NPROC"
elif [ -f CMakeLists.txt ]; then
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j "$NPROC"
else
  echo "No local MySQL build entrypoint in mysql-server."
  exit 1
fi
EOF
}

emit_postgresql() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

NPROC="${NPROC:-$(nproc)}"
log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/postgresql"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ -x configure ]; then
  ./configure
  make -j "$NPROC"
  [ -f Makefile ] && make check
else
  echo "configure missing in PostgreSQL repository."
  exit 1
fi
EOF
}

emit_firebird() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

NPROC="${NPROC:-$(nproc)}"
log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/firebird"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ -x autogen.sh ]; then
  ./autogen.sh
fi
if [ -x configure ]; then
  ./configure
  make -j "$NPROC"
elif [ -f CMakeLists.txt ]; then
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j "$NPROC"
else
  echo "No local Firebird build entrypoint found."
  exit 1
fi
EOF
}

emit_clickhouse() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

NPROC="${NPROC:-$(nproc)}"
log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/ClickHouse"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j "$NPROC"
if [ -f build/CTestTestfile.cmake ]; then
  ctest --test-dir build --output-on-failure
fi
EOF
}

emit_cassandra() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/cassandra"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if command -v ant >/dev/null 2>&1; then
  ant build
  ant test
else
  echo "ant not available; skipping Cassandra build"
fi
EOF
}

emit_dbeaver() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/dbeaver"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ -x tools/build.sh ]; then
  sh tools/build.sh
else
  echo "tools/build.sh missing in dbeaver."
  exit 1
fi
EOF
}

emit_server() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

NPROC="${NPROC:-$(nproc)}"
log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/server"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ -x configure ]; then
  ./configure
  make -j "$NPROC"
elif [ -f CMakeLists.txt ]; then
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j "$NPROC"
else
  echo "No local MariaDB build entrypoint in server."
  exit 1
fi
EOF
}

emit_all() {
  local script_path="$1"
  cat > "$script_path" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

WORK_DIR="$(cd "$(dirname "$0")" && pwd)"

for script in "$WORK_DIR"/build-*.sh; do
  if [ -x "${script}" ] && [ "$(basename "${script}")" != "build-all.sh" ]; then
    "${script}"
  fi
done
EOF
}

emit_test_all() {
  local script_path="$1"
  cat > "$script_path" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

WORK_DIR="$(cd "$(dirname "$0")" && pwd)"

for script in "$WORK_DIR"/test-*.sh; do
  if [ -x "${script}" ] && [ "$(basename "$script")" != "test-all.sh" ]; then
    "${script}"
  fi
done
EOF
}

emit_test_scratchbird() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/ScratchBird"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ -f build/CTestTestfile.cmake ]; then
  ctest --test-dir build --output-on-failure
else
  log "No ScratchBird CTest test target found."
fi

if [ -x build/tests/scratchbird_tests ]; then
  ./build/tests/scratchbird_tests
fi
EOF
}

emit_test_scratchbird_ai() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/ScratchBird-ai"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ ! -d .venv ]; then
  python3 -m venv .venv
fi
source .venv/bin/activate
python3 -m pip install -U pip setuptools wheel

if [ -f pyproject.toml ] || [ -f setup.py ] || [ -f setup.cfg ]; then
  python3 -m pip install -e ".[test]" || python3 -m pip install -e .
fi

if [ -d tests ]; then
  if command -v pytest >/dev/null 2>&1; then
    pytest -q tests
  else
    python3 -m unittest discover -s tests -p 'test_*.py'
  fi
else
  log "No tests directory found in ScratchBird-ai"
fi
EOF
}

emit_test_scratchbird_driver() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

NPROC="${NPROC:-$(nproc)}"
log() {
  echo "[$(date +'%F %T')] $*"
}

run_in_dir() {
  local dir="$1"
  shift
  if [ -d "$dir" ]; then
    (cd "$dir" && "$@")
  fi
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/ScratchBird-driver"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if command -v go >/dev/null 2>&1; then
  run_in_dir tracks/alpha/drivers/go go test ./...
else
  log "skip Go driver tests: go missing"
fi

if [ -f tracks/alpha/drivers/node/package.json ]; then
  if command -v npm >/dev/null 2>&1; then
    run_in_dir tracks/alpha/drivers/node npm test
  else
    log "skip Node driver tests: npm missing"
  fi
else
  log "skip Node driver tests: package missing"
fi

if [ -d tracks/alpha/drivers/python ]; then
  if command -v pytest >/dev/null 2>&1; then
    run_in_dir tracks/alpha/drivers/python pytest
  else
    log "skip Python driver tests: pytest missing"
  fi
else
  log "skip Python driver tests: directory missing"
fi

if command -v cargo >/dev/null 2>&1 && [ -d tracks/alpha/drivers/rust ]; then
  run_in_dir tracks/alpha/drivers/rust cargo test
else
  log "skip Rust driver tests: cargo missing"
fi

if command -v composer >/dev/null 2>&1 && [ -f tracks/alpha/drivers/php/composer.json ]; then
  run_in_dir tracks/alpha/drivers/php bash -lc '
    composer install
    if [ -x vendor/bin/phpunit ]; then
      vendor/bin/phpunit
    elif [ -x vendor/bin/pest ]; then
      vendor/bin/pest
    fi
  '
else
  log "skip PHP driver tests: composer missing or manifest absent"
fi

if command -v dotnet >/dev/null 2>&1 && [ -f tracks/alpha/drivers/dotnet/src/ScratchBird.Data/ScratchBird.Data.csproj ]; then
  run_in_dir tracks/alpha/drivers/dotnet dotnet test
else
  log "skip .NET driver tests: dotnet missing"
fi

if command -v ant >/dev/null 2>&1 && [ -d tracks/beta/drivers/cpp ]; then
  run_in_dir tracks/beta/drivers/cpp bash -lc 'ctest --output-on-failure || true'
fi
EOF
}

emit_test_scratchrobin() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/ScratchRobin"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ -f build/CTestTestfile.cmake ]; then
  ctest --test-dir build --output-on-failure
else
  log "No ScratchRobin CTest target found"
fi

if [ -x build/scratchrobin_tool ]; then
  ./build/scratchrobin_tool --release-gate-check || true
fi
EOF
}

emit_test_duckdb() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/duckdb"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ -f build/CTestTestfile.cmake ]; then
  ctest --test-dir build --output-on-failure
elif command -v make >/dev/null 2>&1 && [ -f Makefile ]; then
  make test
else
  log "No DuckDB tests found (run build first to generate test target)."
fi
EOF
}

emit_test_influxdb() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/influxdb"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if command -v cargo >/dev/null 2>&1; then
  cargo test
else
  log "skip InfluxDB tests: cargo missing"
fi
EOF
}

emit_test_mongo() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/mongo"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if command -v bazel >/dev/null 2>&1; then
  bazel test //... --test_output=errors || true
else
  log "skip MongoDB tests: bazel missing"
fi
EOF
}

emit_test_redis() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/redis"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ -f Makefile ]; then
  make test
else
  log "No Redis test target available"
fi
EOF
}

emit_test_milvus() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/milvus"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ -f build/CTestTestfile.cmake ]; then
  ctest --test-dir build --output-on-failure
elif [ -f Makefile ]; then
  make test
else
  log "No Milvus tests found"
fi
EOF
}

emit_test_opensearch() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/OpenSearch"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ -x ./gradlew ]; then
  ./gradlew test
elif command -v gradle >/dev/null 2>&1; then
  gradle test
else
  log "skip OpenSearch tests: gradle missing"
fi
EOF
}

emit_test_neo4j() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/neo4j"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if command -v mvn >/dev/null 2>&1; then
  mvn test -DskipTests=false
else
  log "skip Neo4j tests: maven missing"
fi
EOF
}

emit_test_mysql_server() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/mysql-server"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ -f build/Makefile ]; then
  log "MySQL server makefile exists; test target varies by branch. run manual tests as needed."
elif [ -f Makefile ]; then
  if grep -q "^test:" Makefile; then
    make test
  else
    log "No mysql-server test target found"
  fi
else
  log "No mysql-server test metadata found"
fi
EOF
}

emit_test_postgresql() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/postgresql"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ -f Makefile ] && grep -q "^check:" Makefile; then
  make check
else
  log "No PostgreSQL check target discovered"
fi
EOF
}

emit_test_firebird() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/firebird"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ -f GNUmakefile ]; then
  if make -n check >/dev/null 2>&1; then
    make check
  else
    log "No check target found in Firebird"
  fi
elif [ -f build/CTestTestfile.cmake ]; then
  ctest --test-dir build --output-on-failure
else
  log "No Firebird tests found"
fi
EOF
}

emit_test_clickhouse() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/ClickHouse"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ -f build/CTestTestfile.cmake ]; then
  ctest --test-dir build --output-on-failure
else
  log "No ClickHouse CTest target found"
fi
EOF
}

emit_test_cassandra() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/cassandra"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ -f build.xml ] && command -v ant >/dev/null 2>&1; then
  ant test
else
  log "Cassandra tests require Ant and build.xml"
fi
EOF
}

emit_test_dbeaver() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/dbeaver"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ -x ./gradlew ]; then
  ./gradlew test || true
else
  log "No DBeaver test target discovered"
fi
EOF
}

emit_test_server() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/server"

if [ ! -d "$PROJECT_DIR" ]; then
  log "Missing repository directory: $PROJECT_DIR"
  exit 1
fi

cd "$PROJECT_DIR"
if [ -f build/CTestTestfile.cmake ]; then
  ctest --test-dir build --output-on-failure
elif [ -f Makefile ] && grep -q "^test:" Makefile; then
  make test
else
  log "No MariaDB test target discovered"
fi
EOF
}

emit_start_scratchbird_environment() {
  local file="$1"
  cat > "$file" <<'EOF'
#!/usr/bin/env bash
if [ -z "${BASH_VERSION:-}" ]; then
  exec bash "$0" "$@"
fi

set -euo pipefail

log() {
  echo "[$(date +'%F %T')] $*"
}

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKSPACE_DIR="${WORKSPACE_DIR:-$SCRIPT_DIR}"
SCRATCHBIRD_DIR="${WORKSPACE_DIR}/ScratchBird"
RUNTIME_DIR="${RUNTIME_DIR:-$SCRIPT_DIR/.scratchbird-local-runtime}"

BIND_ADDR="${BIND_ADDR:-127.0.0.1}"
NATIVE_PORT="${NATIVE_PORT:-13092}"
PG_PORT="${PG_PORT:-15432}"
MYSQL_PORT="${MYSQL_PORT:-13306}"
FB_PORT="${FB_PORT:-13050}"
DB_NAME="${SB_DB_NAME:-main}"

NATIVE_USER="${NATIVE_USER:-SysArch}"
NATIVE_PASSWORD="${NATIVE_PASSWORD:-replaceme}"
PG_USER="${PG_USER:-postgres}"
PG_PASSWORD="${PG_PASSWORD:-postgres}"
MYSQL_USER="${MYSQL_USER:-root}"
MYSQL_PASSWORD="${MYSQL_PASSWORD:-root}"
FB_USER="${FB_USER:-SYSDBA}"
FB_PASSWORD="${FB_PASSWORD:-masterkey}"

ENABLE_POSTGRES=0
ENABLE_MYSQL=0
ENABLE_FIREBIRD=0
FG=0

usage() {
  cat <<'USAGE'
Usage: start-scratchbird-environment.sh [options]

Options:
  --workspace <path>        Workspace root containing ScratchBird (default: current directory)
  --bind-address <addr>     Bind address for listeners (default: 127.0.0.1)
  --native-port <port>      Native ScratchBird listener port (default: 13092)
  --postgres-port <port>    PostgreSQL emulation listener port (default: 15432)
  --mysql-port <port>       MySQL emulation listener port (default: 13306)
  --firebird-port <port>    Firebird emulation listener port (default: 13050)
  --database <name>         Database name (default: main)
  --emulate <comma list>    Enable emulations (postgres,mysql,firebird)
  --runtime-dir <path>      Runtime directory for pid/log/config (default: <workspace>/.scratchbird-local-runtime)
  --foreground              Keep foreground and tail server log
  --help                    Show this help
USAGE
}

normalize_emulate_list() {
  local item
  local list="$1"
  IFS=',' read -r -a _items <<< "$list"
  for item in "${_items[@]}"; do
    case "${item,,}" in
      pg|postgres|postgresql)
        ENABLE_POSTGRES=1
        ;;
      mysql|maria|mariadb)
        ENABLE_MYSQL=1
        ;;
      fb|firebird)
        ENABLE_FIREBIRD=1
        ;;
      *)
        log "unknown emulate target: $item"
        ;;
    esac
  done
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --workspace)
      WORKSPACE_DIR="$2"
      SCRATCHBIRD_DIR="${WORKSPACE_DIR}/ScratchBird"
      shift 2
      ;;
    --bind-address)
      BIND_ADDR="$2"
      shift 2
      ;;
    --native-port)
      NATIVE_PORT="$2"
      shift 2
      ;;
    --postgres-port)
      PG_PORT="$2"
      ENABLE_POSTGRES=1
      shift 2
      ;;
    --mysql-port)
      MYSQL_PORT="$2"
      ENABLE_MYSQL=1
      shift 2
      ;;
    --firebird-port)
      FB_PORT="$2"
      ENABLE_FIREBIRD=1
      shift 2
      ;;
    --database)
      DB_NAME="$2"
      shift 2
      ;;
    --emulate)
      normalize_emulate_list "$2"
      shift 2
      ;;
    --runtime-dir)
      RUNTIME_DIR="$2"
      shift 2
      ;;
    --foreground)
      FG=1
      shift
      ;;
    --postgres)
      ENABLE_POSTGRES=1
      shift
      ;;
    --mysql)
      ENABLE_MYSQL=1
      shift
      ;;
    --firebird)
      ENABLE_FIREBIRD=1
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      log "Unknown option: $1"
      usage
      exit 1
      ;;
  esac
done

if [ ! -d "$SCRATCHBIRD_DIR" ]; then
  log "Missing ScratchBird checkout at: $SCRATCHBIRD_DIR"
  exit 1
fi

run_dir="${RUNTIME_DIR}"
mkdir -p "$run_dir" "$run_dir/logs" "$run_dir/control"
CONTROL_DIR="$run_dir/control"

DB_FILE="$run_dir/${DB_NAME}.sbdb"
CONF_FILE="$run_dir/scratchbird-server.conf"
PID_FILE="$run_dir/sb_server.pid"
SERVER_LOG="$run_dir/logs/sb_server.log"

resolve_bin() {
  local value
  for value in "$@"; do
    if [ -n "$value" ] && [ -x "$value" ]; then
      echo "$value"
      return 0
    fi
  done
  return 1
}

SB_SERVER_BIN="$(resolve_bin "${SCRATCHBIRD_DIR}/build/src/sb_server" "${SCRATCHBIRD_DIR}/build/bin/sb_server" "${SCRATCHBIRD_DIR}/build/src/server/sb_server")" || {
  log "sb_server binary not found. Build ScratchBird first."
  exit 1
}

SB_ISQL_BIN="$(resolve_bin "${SCRATCHBIRD_DIR}/build/src/sb_isql" "${SCRATCHBIRD_DIR}/build/src/cli/sb_isql" "${SCRATCHBIRD_DIR}/build/bin/sb_isql")" || true
SB_PG_ISQL_BIN="$(resolve_bin "${SCRATCHBIRD_DIR}/build/src/sb_pg_isql" "${SCRATCHBIRD_DIR}/build/src/cli/sb_pg_isql")" || true
SB_MY_ISQL_BIN="$(resolve_bin "${SCRATCHBIRD_DIR}/build/src/sb_my_isql" "${SCRATCHBIRD_DIR}/build/src/cli/sb_my_isql")" || true
SB_FB_ISQL_BIN="$(resolve_bin "${SCRATCHBIRD_DIR}/build/src/sb_fb_isql" "${SCRATCHBIRD_DIR}/build/src/cli/sb_fb_isql")" || true

if [ -f "$PID_FILE" ]; then
  existing_pid="$(cat "$PID_FILE" 2>/dev/null || true)"
  if [ -n "${existing_pid:-}" ] && kill -0 "$existing_pid" 2>/dev/null; then
    log "stopping old server pid=$existing_pid"
    kill "$existing_pid" || true
    sleep 1
  fi
  rm -f "$PID_FILE"
fi

cat > "$CONF_FILE" <<EOF_CONF
[server]
mode = single-database
database = ${DB_FILE}
auto_create = true
pid_file = ${PID_FILE}
run_as_user = $(id -un)
run_as_group = $(id -gn)
front_door_mode = direct

[network]
bind_address = ${BIND_ADDR}
control_socket_dir = ${CONTROL_DIR}
unix_socket = ${CONTROL_DIR}/sb.sock
native_port = ${NATIVE_PORT}
EOF_CONF

if [ "$ENABLE_POSTGRES" -eq 1 ]; then
  echo "pg_port = ${PG_PORT}" >> "$CONF_FILE"
fi
if [ "$ENABLE_MYSQL" -eq 1 ]; then
  echo "mysql_port = ${MYSQL_PORT}" >> "$CONF_FILE"
fi
if [ "$ENABLE_FIREBIRD" -eq 1 ]; then
  echo "fb_port = ${FB_PORT}" >> "$CONF_FILE"
fi

cat >> "$CONF_FILE" <<'EOF_CONF'

[authentication]
methods = password
password_hash = argon2id
allow_superuser_remote = true

[logging]
level = info
destination = stderr
log_connections = true
timestamps = true
EOF_CONF

wait_for_listener() {
  local port="$1"
  local attempts=80
  local i
  for ((i = 0; i < attempts; i++)); do
    if (echo >"/dev/tcp/${BIND_ADDR}/${port}") >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.25
  done
  return 1
}

"$SB_SERVER_BIN" --config "$CONF_FILE" >> "$SERVER_LOG" 2>&1 &
SERVER_PID="$!"
echo "$SERVER_PID" > "$PID_FILE"

if ! wait_for_listener "${NATIVE_PORT}"; then
  tail -n 120 "$SERVER_LOG" || true
  log "native listener failed to come up on ${BIND_ADDR}:${NATIVE_PORT}"
  exit 1
fi

if [ "$ENABLE_POSTGRES" -eq 1 ]; then
  if ! wait_for_listener "${PG_PORT}"; then
    log "postgres listener did not start on ${BIND_ADDR}:${PG_PORT}"
  fi
fi

if [ "$ENABLE_MYSQL" -eq 1 ]; then
  if ! wait_for_listener "${MYSQL_PORT}"; then
    log "mysql listener did not start on ${BIND_ADDR}:${MYSQL_PORT}"
  fi
fi

if [ "$ENABLE_FIREBIRD" -eq 1 ]; then
  if ! wait_for_listener "${FB_PORT}"; then
    log "firebird listener did not start on ${BIND_ADDR}:${FB_PORT}"
  fi
fi

cat <<USERS

ScratchBird server started.
Config: $CONF_FILE
PID: $SERVER_PID
Log: $SERVER_LOG
Database: $DB_FILE

Native endpoint (always on):
  Listen: ${BIND_ADDR}:${NATIVE_PORT}
  User: ${NATIVE_USER}
  Password: ${NATIVE_PASSWORD}
USERS

if [ -n "${SB_ISQL_BIN}" ] && [ -x "${SB_ISQL_BIN}" ]; then
  echo "  Tool: ${SB_ISQL_BIN}"
  echo "  Login:"
  echo "    ${SB_ISQL_BIN} ${DB_NAME} --mode=local-ipc --ipc-method=tcp -H ${BIND_ADDR} -p ${NATIVE_PORT} -U ${NATIVE_USER} -P '${NATIVE_PASSWORD}' -q"
else
  echo "  Install ScratchBird sb_isql for native login examples."
fi

if [ "$ENABLE_POSTGRES" -eq 1 ]; then
  echo
  echo "PostgreSQL emulation:"
  echo "  Listen: ${BIND_ADDR}:${PG_PORT}"
  echo "  User: ${PG_USER}"
  echo "  Password: ${PG_PASSWORD}"
  if [ -n "${SB_PG_ISQL_BIN}" ] && [ -x "${SB_PG_ISQL_BIN}" ]; then
    echo "  Tool: ${SB_PG_ISQL_BIN}"
    echo "  Login: ${SB_PG_ISQL_BIN} ${DB_NAME} --host ${BIND_ADDR} --port ${PG_PORT} --user ${PG_USER}"
  elif command -v psql >/dev/null 2>&1; then
    echo "  Tool: psql"
    echo "  Login: PGPASSWORD='${PG_PASSWORD}' psql -h ${BIND_ADDR} -p ${PG_PORT} -U ${PG_USER} -d ${DB_NAME}"
  else
    echo "  No PostgreSQL client binary found."
  fi
fi

if [ "$ENABLE_MYSQL" -eq 1 ]; then
  echo
  echo "MySQL emulation:"
  echo "  Listen: ${BIND_ADDR}:${MYSQL_PORT}"
  echo "  User: ${MYSQL_USER}"
  echo "  Password: ${MYSQL_PASSWORD}"
  if [ -n "${SB_MY_ISQL_BIN}" ] && [ -x "${SB_MY_ISQL_BIN}" ]; then
    echo "  Tool: ${SB_MY_ISQL_BIN}"
    echo "  Login: ${SB_MY_ISQL_BIN} ${DB_NAME} --host ${BIND_ADDR} --port ${MYSQL_PORT} --user ${MYSQL_USER}"
  elif command -v mysql >/dev/null 2>&1; then
    echo "  Tool: mysql"
    echo "  Login: mysql --host=${BIND_ADDR} --port=${MYSQL_PORT} -u${MYSQL_USER} -p${MYSQL_PASSWORD} ${DB_NAME}"
  else
    echo "  No MySQL client binary found."
  fi
fi

if [ "$ENABLE_FIREBIRD" -eq 1 ]; then
  echo
  echo "Firebird emulation:"
  echo "  Listen: ${BIND_ADDR}:${FB_PORT}"
  echo "  User: ${FB_USER}"
  echo "  Password: ${FB_PASSWORD}"
  if [ -n "${SB_FB_ISQL_BIN}" ] && [ -x "${SB_FB_ISQL_BIN}" ]; then
    echo "  Tool: ${SB_FB_ISQL_BIN}"
    echo "  Login: ${SB_FB_ISQL_BIN} ${DB_NAME} --host ${BIND_ADDR} --port ${FB_PORT} --user ${FB_USER}"
  elif command -v isql-fb >/dev/null 2>&1; then
    echo "  Tool: isql-fb"
  else
    echo "  No Firebird client binary found."
  fi
fi

cat <<USERS

Stop command:
  kill \$(cat ${PID_FILE})

Restart with extra parsers:
  --emulate postgres,mysql,firebird
USERS

if [ "$FG" -eq 1 ]; then
  tail -f "$SERVER_LOG"
fi
EOF
}

write_script() {
  local output="$1"
  local generator="$2"

  mkdir -p "$(dirname "$output")"
  if [ -e "$output" ] && [ "$OVERWRITE" -eq 0 ]; then
    echo "skip (exists): $output"
    return 0
  fi
  "$generator" "$output"
  chmod +x "$output"
  echo "installed: $output"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --base-dir)
      BASE_DIR="$2"
      shift 2
      ;;
    --overwrite)
      OVERWRITE=1
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      TARGETS+=("$1")
      shift
      ;;
  esac
done

if [ "${#TARGETS[@]}" -eq 0 ]; then
  TARGETS=("${KNOWN_PROJECTS[@]}")
fi

for i in "${!TARGETS[@]}"; do
  TARGETS[$i]="$(normalize_project "${TARGETS[$i]}")"
done

TARGETS=($(printf "%s\n" "${TARGETS[@]}" | awk 'NF' | sort -u))

for project in "${TARGETS[@]}"; do
  if ! is_known_project "$project"; then
    echo "Unknown project: $project"
    echo "Use --help for supported project names."
    exit 1
  fi
done

for project in "${TARGETS[@]}"; do
  case "$project" in
    scratchbird)
      write_script "$BASE_DIR/build-scratchbird.sh" emit_scratchbird
      write_script "$BASE_DIR/test-scratchbird.sh" emit_test_scratchbird
      ;;
    scratchbird-ai)
      write_script "$BASE_DIR/build-scratchbird-ai.sh" emit_scratchbird_ai
      write_script "$BASE_DIR/test-scratchbird-ai.sh" emit_test_scratchbird_ai
      ;;
    scratchbird-driver)
      write_script "$BASE_DIR/build-scratchbird-driver.sh" emit_scratchbird_driver
      write_script "$BASE_DIR/test-scratchbird-driver.sh" emit_test_scratchbird_driver
      ;;
    scratchrobin)
      write_script "$BASE_DIR/build-scratchrobin.sh" emit_scratchrobin
      write_script "$BASE_DIR/test-scratchrobin.sh" emit_test_scratchrobin
      ;;
    duckdb)
      write_script "$BASE_DIR/build-duckdb.sh" emit_duckdb
      write_script "$BASE_DIR/test-duckdb.sh" emit_test_duckdb
      ;;
    influxdb)
      write_script "$BASE_DIR/build-influxdb.sh" emit_influxdb
      write_script "$BASE_DIR/test-influxdb.sh" emit_test_influxdb
      ;;
    mongo)
      write_script "$BASE_DIR/build-mongo.sh" emit_mongo
      write_script "$BASE_DIR/test-mongo.sh" emit_test_mongo
      ;;
    redis)
      write_script "$BASE_DIR/build-redis.sh" emit_redis
      write_script "$BASE_DIR/test-redis.sh" emit_test_redis
      ;;
    milvus)
      write_script "$BASE_DIR/build-milvus.sh" emit_milvus
      write_script "$BASE_DIR/test-milvus.sh" emit_test_milvus
      ;;
    opensearch)
      write_script "$BASE_DIR/build-opensearch.sh" emit_opensearch
      write_script "$BASE_DIR/test-opensearch.sh" emit_test_opensearch
      ;;
    neo4j)
      write_script "$BASE_DIR/build-neo4j.sh" emit_neo4j
      write_script "$BASE_DIR/test-neo4j.sh" emit_test_neo4j
      ;;
    mysql-server)
      write_script "$BASE_DIR/build-mysql-server.sh" emit_mysql_server
      write_script "$BASE_DIR/test-mysql-server.sh" emit_test_mysql_server
      ;;
    postgresql)
      write_script "$BASE_DIR/build-postgresql.sh" emit_postgresql
      write_script "$BASE_DIR/test-postgresql.sh" emit_test_postgresql
      ;;
    firebird)
      write_script "$BASE_DIR/build-firebird.sh" emit_firebird
      write_script "$BASE_DIR/test-firebird.sh" emit_test_firebird
      ;;
    clickhouse)
      write_script "$BASE_DIR/build-clickhouse.sh" emit_clickhouse
      write_script "$BASE_DIR/test-clickhouse.sh" emit_test_clickhouse
      ;;
    cassandra)
      write_script "$BASE_DIR/build-cassandra.sh" emit_cassandra
      write_script "$BASE_DIR/test-cassandra.sh" emit_test_cassandra
      ;;
    dbeaver)
      write_script "$BASE_DIR/build-dbeaver.sh" emit_dbeaver
      write_script "$BASE_DIR/test-dbeaver.sh" emit_test_dbeaver
      ;;
    server)
      write_script "$BASE_DIR/build-server.sh" emit_server
      write_script "$BASE_DIR/test-server.sh" emit_test_server
      ;;
    *)
      echo "Unhandled project: $project"
      ;;
  esac
done

write_script "$BASE_DIR/build-all.sh" emit_all
write_script "$BASE_DIR/test-all.sh" emit_test_all
write_script "$BASE_DIR/start-scratchbird-environment.sh" emit_start_scratchbird_environment
