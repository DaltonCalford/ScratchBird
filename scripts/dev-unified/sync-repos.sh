#!/usr/bin/env bash
if [ -z "${BASH_VERSION:-}" ]; then
  exec bash "$0" "$@"
fi

set -euo pipefail

BASE_DIR="$(pwd)"
BRANCH=""
FORCE=0
NO_PULL=0
VERBOSE=0
SHALLOW=""

declare -A REPO_URLS
REPO_URLS=(
  [scratchbird]="https://github.com/DaltonCalford/ScratchBird.git"
  [scratchbird-ai]="https://github.com/DaltonCalford/ScratchBird-ai.git"
  [scratchbird-driver]="https://github.com/DaltonCalford/ScratchBird-driver.git"
  [scratchrobin]="https://github.com/DaltonCalford/ScratchRobin.git"
  [clickhouse]="https://github.com/ClickHouse/ClickHouse.git"
  [duckdb]="https://github.com/duckdb/duckdb.git"
  [influxdb]="https://github.com/influxdata/influxdb.git"
  [milvus]="https://github.com/milvus-io/milvus.git"
  [neo4j]="https://github.com/neo4j/neo4j.git"
  [redis]="https://github.com/redis/redis.git"
  [mongo]="https://github.com/mongodb/mongo.git"
  [opensearch]="https://github.com/opensearch-project/OpenSearch.git"
  [cassandra]="https://github.com/apache/cassandra.git"
  [mysql-server]="https://github.com/mysql/mysql-server.git"
  [postgresql]="https://git.postgresql.org/git/postgresql.git"
  [firebird]="https://github.com/FirebirdSQL/firebird.git"
  [dbeaver]="https://github.com/dbeaver/dbeaver.git"
  [server]="https://github.com/MariaDB/server.git"
)

declare -A REPO_DIRS
REPO_DIRS=(
  [scratchbird]="ScratchBird"
  [scratchbird-ai]="ScratchBird-ai"
  [scratchbird-driver]="ScratchBird-driver"
  [scratchrobin]="ScratchRobin"
  [clickhouse]="ClickHouse"
  [duckdb]="duckdb"
  [influxdb]="influxdb"
  [milvus]="milvus"
  [neo4j]="neo4j"
  [redis]="redis"
  [mongo]="mongo"
  [opensearch]="OpenSearch"
  [cassandra]="cassandra"
  [mysql-server]="mysql-server"
  [postgresql]="postgresql"
  [firebird]="firebird"
  [dbeaver]="dbeaver"
  [server]="server"
)

KNOWN_REPOS=(
  scratchbird
  scratchbird-ai
  scratchbird-driver
  scratchrobin
  clickhouse
  duckdb
  influxdb
  milvus
  neo4j
  mongo
  opensearch
  redis
  cassandra
  mysql-server
  postgresql
  firebird
  dbeaver
  server
)

usage() {
  cat <<'EOF'
Usage: sync-repos.sh [options] [repo ...]

Options:
  --base-dir <path>     Base path containing repos (default: current directory)
  --all                 Sync all known repos
  --branch <name>       Checkout/sync this branch
  --force               Hard reset local branch to remote before pulling
  --no-pull             Clone only; do not pull/update existing repos
  --depth <n>           Shallow clone depth for missing repos (default: full clone)
  --verbose             Print remote branch info and command progress
  --help                Show this help

Repo names (case-insensitive, dashes/underscores accepted):
  all, scratchbird, scratchbird-ai, scratchbird-driver, scratchrobin,
  clickhouse, duckdb, influxdb, milvus, neo4j, mongo, redis, opensearch,
  cassandra, mysql-server, postgresql, firebird, dbeaver, server

Examples:
  ./sync-repos.sh --all
  ./sync-repos.sh scratchbird redis
  ./sync-repos.sh --branch main --force server
EOF
}

normalize_repo_name() {
  local repo="$1"
  repo="${repo,,}"
  repo="${repo//_/-}"
  case "$repo" in
    mongodb)
      repo="mongo"
      ;;
    scratchbirdai|scratchbird_ai)
      repo="scratchbird-ai"
      ;;
    scratchbirddriver|scratchbird_driver)
      repo="scratchbird-driver"
      ;;
    scratchrobin_ai|scratchrobin-ai)
      repo="scratchrobin"
      ;;
    clickhouse*)
      repo="clickhouse"
      ;;
    mariadb|maria-db)
      repo="server"
      ;;
    *)
      ;;
  esac
  echo "$repo"
}

has_repo() {
  local repo="$1"
  for known in "${KNOWN_REPOS[@]}"; do
    if [ "$repo" = "$known" ]; then
      return 0
    fi
  done
  return 1
}

log() {
  echo "[$(date +'%F %T')] $*"
}

sync_repo() {
  local repo="$1"
  local repo_dir="$BASE_DIR/${REPO_DIRS[$repo]}"
  local repo_url="${REPO_URLS[$repo]}"
  local remote_ref=""
  local -a clone_cmd

  log "syncing $repo ($repo_url)"

  if [ ! -d "$repo_dir/.git" ]; then
    if [ ! -d "$repo_dir" ]; then
      clone_cmd=(git clone)
      if [ -n "$SHALLOW" ]; then
        clone_cmd+=(--depth "$SHALLOW")
      fi
      if [ -n "$BRANCH" ]; then
        clone_cmd+=(--branch "$BRANCH")
      fi
      clone_cmd+=("$repo_url" "$repo_dir")
      "${clone_cmd[@]}"
      log "cloned: $repo_dir"
      return 0
    fi
    log "WARNING: $repo_dir exists but is not a git repository; skipping."
    return 1
  fi

  if [ "$NO_PULL" -eq 1 ]; then
    log "skipping refresh for existing repo: $repo_dir"
    return 0
  fi

  if [ "$VERBOSE" -eq 1 ]; then
    git -C "$repo_dir" remote -v
  fi

  git -C "$repo_dir" fetch --all --prune

  if [ -n "$BRANCH" ]; then
    git -C "$repo_dir" checkout "$BRANCH" || git -C "$repo_dir" checkout -b "$BRANCH" "origin/$BRANCH"
    remote_ref="origin/$BRANCH"
  else
    remote_ref="@{u}"
  fi

  if [ "$FORCE" -eq 1 ]; then
    if [ -n "$remote_ref" ] && git -C "$repo_dir" rev-parse --verify --quiet "$remote_ref" >/dev/null 2>&1; then
      git -C "$repo_dir" reset --hard "$remote_ref"
    else
      git -C "$repo_dir" pull --ff-only
    fi
  else
    if ! git -C "$repo_dir" pull --ff-only "$remote_ref"; then
      # shell fallback for repos without upstream configured
      if [ -n "$BRANCH" ]; then
        git -C "$repo_dir" pull --ff-only origin "$BRANCH"
      else
        log "SKIP: cannot fast-forward $repo_dir (local changes or no upstream). Use --force after review."
      fi
    fi
  fi

  log "done: $repo_dir"
}

SELECT_ALL=0
REQUESTED=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --base-dir)
      BASE_DIR="$2"
      shift 2
      ;;
    --all)
      SELECT_ALL=1
      shift
      ;;
    --branch)
      BRANCH="$2"
      shift 2
      ;;
    --force)
      FORCE=1
      shift
      ;;
    --no-pull)
      NO_PULL=1
      shift
      ;;
    --depth)
      SHALLOW="$2"
      shift 2
      ;;
    --verbose)
      VERBOSE=1
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      REQUESTED+=("$1")
      shift
      ;;
  esac
done

if [ "${#REQUESTED[@]}" -eq 0 ] && [ "$SELECT_ALL" -eq 0 ]; then
  usage
  exit 1
fi

if [ "$SELECT_ALL" -eq 1 ]; then
  REQUESTED=("${KNOWN_REPOS[@]}")
fi

declare -a sync_list=()
declare -a deduped=()
declare -A seen_repos=()

for repo_raw in "${REQUESTED[@]}"; do
  repo="$(normalize_repo_name "$repo_raw")"
  if [ "$repo" = "all" ]; then
    for known in "${KNOWN_REPOS[@]}"; do
      sync_list+=("$known")
    done
    continue
  fi
  if ! has_repo "$repo"; then
    echo "Unknown repo: $repo_raw"
    echo "Use --help for available repositories."
    exit 1
  fi
  sync_list+=("$repo")
done

for repo in "${sync_list[@]}"; do
  if [ -z "${seen_repos[$repo]:-}" ]; then
    deduped+=("$repo")
    seen_repos[$repo]=1
  fi
done

log "base dir: $BASE_DIR"
for repo in "${deduped[@]}"; do
  sync_repo "$repo"
done
