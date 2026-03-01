#!/usr/bin/env bash
if [ -z "${BASH_VERSION:-}" ]; then
  exec bash "$0" "$@"
fi

set -euo pipefail

WORKSPACE="${1:-$(pwd)}"
REQUESTED_PROJECT="${2:-auto}"
NPROC="${NPROC:-$(nproc)}"

log() {
  echo "[$(date +'%F %T')] $*"
}

run_cmd() {
  log "Running: $*"
  "$@"
}

has_cmd() {
  command -v "$1" >/dev/null 2>&1
}

run_in_project_dir() {
  local project_dir="$1"
  local run_fn="$2"

  if [ ! -d "$project_dir" ]; then
    log "skip: missing project directory $project_dir"
    return 1
  fi

  (cd "$project_dir" && "$run_fn")
}

run_in_dir() {
  local dir="$1"
  shift
  if [ ! -d "$dir" ]; then
    log "skip: missing directory $dir"
    return 1
  fi
  log "entering $dir"
  (cd "$dir" && "$@")
}

maybe_ctest() {
  local build_dir="$1"
  if [ -d "$build_dir" ] && [ -f "$build_dir/CTestTestfile.cmake" ]; then
    run_cmd ctest --test-dir "$build_dir" --output-on-failure
  fi
}

run_scratchbird() {
  log "==> Building ScratchBird"
  run_cmd cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  run_cmd cmake --build build -j "$NPROC"
  maybe_ctest build
}

run_scratchbird_ai() {
  log "==> Building ScratchBird-ai"
  run_cmd python3 -m venv .venv
  source .venv/bin/activate
  run_cmd python3 -m pip install -U pip setuptools wheel
  if [ -f pyproject.toml ] || [ -f setup.cfg ] || [ -f setup.py ]; then
    run_cmd python3 -m pip install -e ".[mcp]" || run_cmd python3 -m pip install -e ".[test]" || run_cmd python3 -m pip install -e .
  fi
  run_cmd python3 -m unittest discover -s tests -p 'test_*.py'
  if [ -x tools/smoke_http_contract.py ]; then
    run_cmd PYTHONPATH=src python3 tools/smoke_http_contract.py --mode selftest
  fi
  if [ -f tools/validate_evidence_gates.py ]; then
    run_cmd python3 tools/validate_evidence_gates.py --repo-root .
  fi
}

run_scratchbird_driver() {
  log "==> Building ScratchBird-driver (best-effort full matrix)"
  run_cmd cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  if has_cmd go; then
    run_in_dir tracks/alpha/drivers/go bash -lc 'go test ./...'
  else
    log "skip Go driver: go not installed"
  fi

  if has_cmd npm; then
    if [ -f tracks/alpha/drivers/node/package.json ]; then
      run_in_dir tracks/alpha/drivers/node bash -lc '
        npm install
        npm run build --if-present
        npm test --if-present
      '
    else
      log "skip Node driver: package manifest missing"
    fi
  else
    log "skip Node driver: npm missing"
  fi

  if has_cmd python3; then
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
  else
    log "skip Python driver: python3 missing"
  fi

  if has_cmd cargo; then
    run_in_dir tracks/alpha/drivers/rust bash -lc 'cargo test'
  else
    log "skip Rust driver: cargo missing"
  fi

  if has_cmd composer; then
    if [ -f tracks/alpha/drivers/php/composer.json ]; then
      run_in_dir tracks/alpha/drivers/php bash -lc '
        composer install
        if [ -x vendor/bin/phpunit ]; then
          vendor/bin/phpunit tests
        fi
      '
    else
      log "skip PHP driver: composer.json missing"
    fi
  else
    log "skip PHP driver: composer missing"
  fi

  if has_cmd fpc; then
    if [ -f tracks/alpha/drivers/pascal/tests/TlsCryptoAndPolicyTests.pas ]; then
      run_in_dir tracks/alpha/drivers/pascal bash -lc 'fpc -Mdelphi -Fu./src -FE./tests ./tests/TlsCryptoAndPolicyTests.pas'
    else
      log "skip Pascal driver: test file missing"
    fi
  else
    log "skip Pascal driver: fpc missing"
  fi

  if has_cmd dotnet; then
    if [ -f tracks/alpha/drivers/dotnet/src/ScratchBird.Data/ScratchBird.Data.csproj ]; then
      run_in_dir tracks/alpha/drivers/dotnet bash -lc '
        dotnet build src/ScratchBird.Data/ScratchBird.Data.csproj
        dotnet test
      '
    else
      log "skip .NET driver: project file missing"
    fi
  else
    log "skip .NET driver: dotnet missing"
  fi

  if [ -x tracks/alpha/drivers/jdbc/gradlew ]; then
    run_in_dir tracks/alpha/drivers/jdbc bash -lc './gradlew build'
  elif has_cmd gradle; then
    run_in_dir tracks/alpha/drivers/jdbc bash -lc 'gradle build'
  else
    log "skip JDBC driver: gradle missing"
  fi

  if [ -d tracks/alpha/drivers/odbc ]; then
    run_cmd cmake -S tracks/alpha/drivers/odbc -B build-odbc -DCMAKE_BUILD_TYPE=Release
    run_cmd cmake --build build-odbc -j "$NPROC"
  fi
  if [ -d tracks/beta/drivers/cpp ]; then
    run_cmd cmake -S tracks/beta/drivers/cpp -B build-cpp -DCMAKE_BUILD_TYPE=Release
    run_cmd cmake --build build-cpp -j "$NPROC"
  fi
  if [ -d tracks/alpha/drivers/cli ]; then
    run_cmd cmake -S tracks/alpha/drivers/cli -B build-cli -DCMAKE_BUILD_TYPE=Release -DSB_BUILD_CLI=ON -DSB_BUILD_CPP=ON -DSB_BUILD_ODBC=OFF
    run_cmd cmake --build build-cli -j "$NPROC"
  fi
}

run_scratchrobin() {
  log "==> Building ScratchRobin"
  run_cmd cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  run_cmd cmake --build build -j "$NPROC"
  maybe_ctest build
  if [ -x build/scratchrobin_tool ]; then
    run_cmd ./build/scratchrobin_tool --runtime-startup
    run_cmd ./build/scratchrobin_tool --release-gate-check
    run_cmd ./build/scratchrobin_tool --validate-package-manifest=resources/templates/package_profile_manifest.example.json
    run_cmd ./build/scratchrobin_tool --check-package-artifacts=resources/templates/package_profile_manifest.example.json
  fi
  if [ -x tools/run_conformance_gate.sh ]; then
    run_cmd ./tools/run_conformance_gate.sh ./build
  fi
}

run_duckdb() {
  log "==> Building DuckDB"
  if has_cmd make; then
    run_cmd make
    run_cmd make unit
    run_cmd make allunit
  else
    run_cmd cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    run_cmd cmake --build build -j "$NPROC"
    maybe_ctest build
  fi
}

run_influxdb() {
  log "> Building InfluxDB"
  if ! has_cmd cargo; then
    log "ERROR: cargo missing"
    return 1
  fi
  run_cmd cargo build
  if has_cmd cargo-nextest; then
    run_cmd cargo nextest run --workspace
  else
    run_cmd cargo install cargo-nextest
    run_cmd cargo nextest run --workspace
  fi
}

run_mongo() {
  log "==> Building MongoDB"
  run_cmd python buildscripts/install_bazel.py
  export PATH="$HOME/.local/bin:$PATH"
  run_cmd bazel build install-dist-test
}

run_redis() {
  log "==> Building Redis"
  run_cmd make -j "$NPROC"
  run_cmd make test
}

run_milvus() {
  log "==> Building Milvus"
  run_cmd bash scripts/install_deps.sh
  run_cmd make -j "$NPROC"
}

run_opensearch() {
  log "==> Building OpenSearch"
  run_cmd ./gradlew localDistro
  run_cmd ./gradlew check
}

run_neo4j() {
  log "==> Building Neo4j"
  if [ -z "${MAVEN_OPTS:-}" ]; then
    export MAVEN_OPTS="-Xmx2048m"
  fi
  run_cmd mvn clean install -T1C
}

run_dbeaver() {
  log "==> Building DBeaver"
  if [ ! -x tools/build.sh ]; then
    log "ERROR: tools/build.sh missing"
    return 1
  fi
  run_cmd sh tools/build.sh
}

run_postgresql() {
  log "==> Building PostgreSQL"
  if [ -x configure ]; then
    run_cmd ./configure
    run_cmd make -j "$NPROC"
    if [ -f Makefile ]; then
      run_cmd make check
    fi
  else
    log "configure script missing; skipping PostgreSQL build"
  fi
}

run_mysql_server() {
  log "==> Building mysql-server"
  if [ -x configure ]; then
    run_cmd ./configure
    run_cmd make -j "$NPROC"
  elif [ -f CMakeLists.txt ]; then
    run_cmd cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    run_cmd cmake --build build -j "$NPROC"
  else
    log "No local build entrypoint for mysql-server found. Build guidance in mysql-server/INSTALL (external MySQL build docs)."
  fi
}

run_mariadb() {
  log "==> Building MariaDB"
  if [ -x configure ]; then
    run_cmd ./configure
    run_cmd make -j "$NPROC"
  elif [ -f CMakeLists.txt ]; then
    run_cmd cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    run_cmd cmake --build build -j "$NPROC"
  else
    log "No local build entrypoint for MariaDB found; run official MariaDB build documentation."
  fi
}

run_firebird() {
  log "==> Building Firebird"
  if [ -x autogen.sh ]; then
    run_cmd ./autogen.sh
  fi
  if [ -x configure ]; then
    run_cmd ./configure
    run_cmd make -j "$NPROC"
  elif [ -f CMakeLists.txt ]; then
    run_cmd cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    run_cmd cmake --build build -j "$NPROC"
  else
    log "No local build script found. See https://www.firebirdsql.org/en/building-the-code/"
  fi
}

run_clickhouse() {
  log "==> Building ClickHouse"
  run_cmd cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  run_cmd cmake --build build -j "$NPROC"
  maybe_ctest build
}

run_cassandra() {
  log "==> Building Cassandra"
  if has_cmd ant; then
    run_cmd ant build
  elif [ -f build.xml ]; then
    log "ant not available; cannot run Cassandra build without Apache Ant"
    return 1
  else
    log "No Cassandra build metadata found"
  fi
}

run_cassandra_tests() {
  if has_cmd ant && [ -f build.xml ]; then
    run_cmd ant test
  else
    log "Ant missing; skip Cassandra tests"
  fi
}

run_project() {
  local project="$1"
  local project_lc=""
  local requested_dir=""
  local run_fn=""

  project_lc="$(printf '%s' "$project" | tr '[:upper:]' '[:lower:]')"

  if [ "$project_lc" = "auto" ] || [ "$project_lc" = "all" ]; then
    for requested_dir in \
      ScratchBird \
      ScratchBird-ai \
      ScratchBird-driver \
      ScratchRobin \
      duckdb \
      influxdb \
      mongo \
      redis \
      milvus \
      OpenSearch \
      neo4j \
      mysql-server \
      postgresql \
      firebird \
      ClickHouse \
      cassandra \
      dbeaver \
      server \
      mariadb; do
      case "$requested_dir" in
        ScratchBird) run_in_project_dir "$WORKSPACE/$requested_dir" run_scratchbird || true ;;
        ScratchBird-ai) run_in_project_dir "$WORKSPACE/$requested_dir" run_scratchbird_ai || true ;;
        ScratchBird-driver) run_in_project_dir "$WORKSPACE/$requested_dir" run_scratchbird_driver || true ;;
        ScratchRobin) run_in_project_dir "$WORKSPACE/$requested_dir" run_scratchrobin || true ;;
        OpenSearch) run_in_project_dir "$WORKSPACE/$requested_dir" run_opensearch || true ;;
        ClickHouse) run_in_project_dir "$WORKSPACE/$requested_dir" run_clickhouse || true ;;
        duckdb) run_in_project_dir "$WORKSPACE/$requested_dir" run_duckdb || true ;;
        influxdb) run_in_project_dir "$WORKSPACE/$requested_dir" run_influxdb || true ;;
        mongo) run_in_project_dir "$WORKSPACE/$requested_dir" run_mongo || true ;;
        redis) run_in_project_dir "$WORKSPACE/$requested_dir" run_redis || true ;;
        milvus) run_in_project_dir "$WORKSPACE/$requested_dir" run_milvus || true ;;
        neo4j) run_in_project_dir "$WORKSPACE/$requested_dir" run_neo4j || true ;;
        mysql-server) run_in_project_dir "$WORKSPACE/$requested_dir" run_mysql_server || true ;;
        postgresql) run_in_project_dir "$WORKSPACE/$requested_dir" run_postgresql || true ;;
        firebird) run_in_project_dir "$WORKSPACE/$requested_dir" run_firebird || true ;;
        cassandra)
          run_in_project_dir "$WORKSPACE/$requested_dir" run_cassandra || true
          run_in_project_dir "$WORKSPACE/$requested_dir" run_cassandra_tests || true
          ;;
        mariadb) run_in_project_dir "$WORKSPACE/$requested_dir" run_mariadb || true ;;
        server) run_in_project_dir "$WORKSPACE/$requested_dir" run_mariadb || true ;;
        dbeaver) run_in_project_dir "$WORKSPACE/$requested_dir" run_dbeaver || true ;;
        *)
          log "Unknown auto-build project directory: $requested_dir"
          ;;
      esac
    done
    return 0
  fi

  case "$project_lc" in
    scratchbird*)
      run_fn="run_scratchbird"
      requested_dir="ScratchBird"
      ;;
    scratchbird-ai*)
      run_fn="run_scratchbird_ai"
      requested_dir="ScratchBird-ai"
      ;;
    scratchbird-driver*)
      run_fn="run_scratchbird_driver"
      requested_dir="ScratchBird-driver"
      ;;
    scratchrobin*)
      run_fn="run_scratchrobin"
      requested_dir="ScratchRobin"
      ;;
    duckdb*)
      run_fn="run_duckdb"
      requested_dir="duckdb"
      ;;
    influxdb*)
      run_fn="run_influxdb"
      requested_dir="influxdb"
      ;;
    mongo*|mongodb*)
      run_fn="run_mongo"
      requested_dir="mongo"
      ;;
    redis*)
      run_fn="run_redis"
      requested_dir="redis"
      ;;
    milvus*)
      run_fn="run_milvus"
      requested_dir="milvus"
      ;;
    opensearch*)
      run_fn="run_opensearch"
      requested_dir="OpenSearch"
      ;;
    neo4j*)
      run_fn="run_neo4j"
      requested_dir="neo4j"
      ;;
    dbeaver*)
      run_fn="run_dbeaver"
      requested_dir="dbeaver"
      ;;
    server*)
      run_fn="run_mariadb"
      requested_dir="server"
      ;;
    mariadb*)
      run_fn="run_mariadb"
      requested_dir="mariadb"
      ;;
    cassandra*)
      run_fn="run_cassandra"
      requested_dir="cassandra"
      ;;
    mysql*|mysql-server*)
      run_fn="run_mysql_server"
      requested_dir="mysql-server"
      ;;
    postgresql*)
      run_fn="run_postgresql"
      requested_dir="postgresql"
      ;;
    firebird*)
      run_fn="run_firebird"
      requested_dir="firebird"
      ;;
    clickhouse*)
      run_fn="run_clickhouse"
      requested_dir="ClickHouse"
      ;;
    *)
      log "Unknown project '$project'."
      log "Known projects: ScratchBird, ScratchBird-ai, ScratchBird-driver, ScratchRobin, mongo, redis, opensearch, ClickHouse, duckdb, influxdb, milvus, neo4j, dbeaver, cassandra, mysql-server, postgresql, firebird, mariadb, server."
      return 1
      ;;
  esac

  if [ -n "$requested_dir" ]; then
    run_in_project_dir "$WORKSPACE/$requested_dir" "$run_fn"
    if [ "$project_lc" = "cassandra" ] || [ "${project_lc#cassandra}" != "$project_lc" ]; then
      run_in_project_dir "$WORKSPACE/$requested_dir" run_cassandra_tests || true
    fi
    return 0
  fi

  log "No action selected for '$project'."
  return 1
}

PROJECT="${REQUESTED_PROJECT}"
if [ "$PROJECT" = "auto" ]; then
  if [ -d "$WORKSPACE/ScratchBird" ] || [ -d "$WORKSPACE/ScratchBird-ai" ] || [ -d "$WORKSPACE/ScratchBird-driver" ] || [ -d "$WORKSPACE/ScratchRobin" ] || [ -d "$WORKSPACE/ClickHouse" ] || [ -d "$WORKSPACE/duckdb" ] || [ -d "$WORKSPACE/milvus" ] || [ -d "$WORKSPACE/mongo" ] || [ -d "$WORKSPACE/neo4j" ] || [ -d "$WORKSPACE/mysql-server" ] || [ -d "$WORKSPACE/postgresql" ] || [ -d "$WORKSPACE/mariadb" ] || [ -d "$WORKSPACE/server" ]; then
    PROJECT="all"
  else
    PROJECT="$(basename "$WORKSPACE")"
  fi
fi

if [ ! -d "$WORKSPACE" ]; then
  log "Workspace missing: $WORKSPACE"
  exit 1
fi

cd "$WORKSPACE"
run_project "$PROJECT"
