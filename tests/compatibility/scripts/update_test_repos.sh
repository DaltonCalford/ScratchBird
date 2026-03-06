#!/usr/bin/env bash
set -euo pipefail

if ! command -v git >/dev/null 2>&1; then
  echo "Error: git is required." >&2
  exit 1
fi

if ! command -v rsync >/dev/null 2>&1; then
  echo "Error: rsync is required." >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPAT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
WORK_DIR="$(mktemp -d)"

FETCH_MYSQL=1
SOURCE_MODE="${SCRATCHBIRD_TEST_REPO_SOURCE:-auto}" # auto|local|remote
LOCAL_ROOT="${SCRATCHBIRD_REFERENCE_REPO_ROOT:-${HOME}/CliWork}"
SYNC_LOCAL_REFS="${SCRATCHBIRD_SYNC_LOCAL_REFS:-0}"

usage() {
  cat >&2 <<'USAGE'
Usage:
  update_test_repos.sh [--with-mysql|--skip-mysql] [--source auto|local|remote] [--local-root <path>]

Environment:
  SCRATCHBIRD_FETCH_MYSQL_TESTS=0      Skip MySQL refresh.
  SCRATCHBIRD_TEST_REPO_SOURCE=auto    Source mode: auto|local|remote.
  SCRATCHBIRD_REFERENCE_REPO_ROOT=...  Root containing local reference clones.
  SCRATCHBIRD_SYNC_LOCAL_REFS=1        In local mode, fetch/pull local clones before vendoring.
USAGE
}

normalize_source_mode() {
  local mode="$1"
  case "$mode" in
    auto|local|remote)
      printf '%s' "$mode"
      ;;
    *)
      echo "Error: invalid source mode '$mode' (expected auto|local|remote)." >&2
      exit 2
      ;;
  esac
}

sync_tree() {
  local src="$1"
  local dst="$2"
  mkdir -p "$dst"
  rsync -a --delete --exclude='.git' "$src/" "$dst/"
}

repo_remote_url() {
  local src="$1"
  if [[ -d "$src/.git" ]]; then
    git -C "$src" remote get-url origin 2>/dev/null || echo "unknown"
  else
    echo "n/a"
  fi
}

repo_head_commit() {
  local src="$1"
  if [[ -d "$src/.git" ]]; then
    git -C "$src" rev-parse HEAD 2>/dev/null || echo "unknown"
  else
    echo "n/a"
  fi
}

update_local_reference_repo() {
  local repo_path="$1"
  local label="$2"
  if [[ ! -d "$repo_path/.git" ]]; then
    echo "    - ${label}: missing git repository at $repo_path (skip sync)"
    return
  fi

  local dirty
  dirty="$(git -C "$repo_path" status --porcelain | wc -l | tr -d '[:space:]')"
  echo "    - ${label}: fetch --all --tags --prune (dirty_files=${dirty})"
  git -C "$repo_path" fetch --all --tags --prune
  if [[ "$dirty" == "0" ]]; then
    git -C "$repo_path" pull --ff-only --tags
  else
    echo "      skip pull (dirty worktree)"
  fi
}

local_source_ready() {
  local root="$1"
  local mysql_ok=1

  if [[ "$FETCH_MYSQL" -eq 1 && ! -d "$root/mysql-server/mysql-test" ]]; then
    mysql_ok=0
  fi

  [[ -d "$root/fbt-repository" \
    && -d "$root/firebird-qa" \
    && -d "$root/postgresql/src/test" \
    && "$mysql_ok" -eq 1 ]]
}

write_snapshot_manifest() {
  local mode="$1"
  local fb_fbt_src="$2"
  local fb_qa_src="$3"
  local my_src="$4"
  local pg_src="$5"
  local manifest_path="$COMPAT_DIR/SNAPSHOT_MANIFEST.md"

  local mysql_entry_url="skipped"
  local mysql_entry_commit="skipped"
  local mysql_entry_tests="skipped"
  if [[ "$FETCH_MYSQL" -eq 1 ]]; then
    mysql_entry_url="$(repo_remote_url "$my_src")"
    mysql_entry_commit="$(repo_head_commit "$my_src")"
    mysql_entry_tests="$(find "$COMPAT_DIR/mysql/repos/mysql-server/mysql-test" -type f -name '*.test' | wc -l | tr -d '[:space:]')"
  fi

  local fbt_count
  fbt_count="$(find "$COMPAT_DIR/firebird/repos/fbt-repository/tests" -type f -name '*.fbt' | wc -l | tr -d '[:space:]')"
  local fbqa_count
  fbqa_count="$(find "$COMPAT_DIR/firebird/repos/firebird-qa/tests" -type f -name '*_test.py' | wc -l | tr -d '[:space:]')"
  local pg_regress_count
  pg_regress_count="$(find "$COMPAT_DIR/postgresql/repos/postgres/src/test/regress/sql" -type f -name '*.sql' | wc -l | tr -d '[:space:]')"

  cat > "$manifest_path" <<MANIFEST
# Compatibility Snapshot Manifest

Last updated: $(date -u +%Y-%m-%dT%H:%M:%SZ)
Source mode: \`${mode}\`
Local reference root: \`${LOCAL_ROOT}\`

## Firebird

- fbt-repository source: \`$(repo_remote_url "$fb_fbt_src")\`
- fbt-repository commit: \`$(repo_head_commit "$fb_fbt_src")\`
- vendored path: \`tests/compatibility/firebird/repos/fbt-repository\`
- test files (\`.fbt\`): \`${fbt_count}\`

- firebird-qa source: \`$(repo_remote_url "$fb_qa_src")\`
- firebird-qa commit: \`$(repo_head_commit "$fb_qa_src")\`
- vendored path: \`tests/compatibility/firebird/repos/firebird-qa\`
- test files (\`*_test.py\`): \`${fbqa_count}\`

## MySQL

- mysql source: \`${mysql_entry_url}\`
- mysql commit: \`${mysql_entry_commit}\`
- vendored path: \`tests/compatibility/mysql/repos/mysql-server/mysql-test\`
- test files (\`.test\`): \`${mysql_entry_tests}\`

## PostgreSQL

- postgresql source: \`$(repo_remote_url "$pg_src")\`
- postgresql commit: \`$(repo_head_commit "$pg_src")\`
- vendored path: \`tests/compatibility/postgresql/repos/postgres/src/test\`
- regress sql files: \`${pg_regress_count}\`
MANIFEST
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --with-mysql)
      FETCH_MYSQL=1
      shift
      ;;
    --skip-mysql)
      FETCH_MYSQL=0
      shift
      ;;
    --source)
      [[ $# -lt 2 ]] && usage && exit 2
      SOURCE_MODE="$(normalize_source_mode "$2")"
      shift 2
      ;;
    --source=*)
      SOURCE_MODE="$(normalize_source_mode "${1#*=}")"
      shift
      ;;
    --local-root)
      [[ $# -lt 2 ]] && usage && exit 2
      LOCAL_ROOT="$2"
      shift 2
      ;;
    --local-root=*)
      LOCAL_ROOT="${1#*=}"
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage
      exit 2
      ;;
  esac
done

if [[ "${SCRATCHBIRD_FETCH_MYSQL_TESTS:-}" == "0" ]]; then
  FETCH_MYSQL=0
fi

SOURCE_MODE="$(normalize_source_mode "$SOURCE_MODE")"
if [[ "$SOURCE_MODE" == "auto" ]]; then
  if local_source_ready "$LOCAL_ROOT"; then
    SOURCE_MODE="local"
  else
    SOURCE_MODE="remote"
  fi
fi

cleanup() {
  rm -rf "$WORK_DIR"
}
trap cleanup EXIT

echo "Updating vendored compatibility test repositories..."
echo "  - Source mode: ${SOURCE_MODE}"
echo "  - Local reference root: ${LOCAL_ROOT}"

if [[ "$SOURCE_MODE" == "local" ]]; then
  if ! local_source_ready "$LOCAL_ROOT"; then
    echo "Error: local source mode requested, but required reference clones are missing under $LOCAL_ROOT" >&2
    exit 1
  fi

  if [[ "$SYNC_LOCAL_REFS" == "1" ]]; then
    echo "  - Syncing local reference clones from remotes first"
    update_local_reference_repo "$LOCAL_ROOT/fbt-repository" "fbt-repository"
    update_local_reference_repo "$LOCAL_ROOT/firebird-qa" "firebird-qa"
    if [[ "$FETCH_MYSQL" -eq 1 ]]; then
      update_local_reference_repo "$LOCAL_ROOT/mysql-server" "mysql-server"
    fi
    update_local_reference_repo "$LOCAL_ROOT/postgresql" "postgresql"
  fi

  echo "  - Firebird: fbt-repository (local)"
  sync_tree "$LOCAL_ROOT/fbt-repository" "$COMPAT_DIR/firebird/repos/fbt-repository"

  echo "  - Firebird: firebird-qa (local)"
  sync_tree "$LOCAL_ROOT/firebird-qa" "$COMPAT_DIR/firebird/repos/firebird-qa"

  if [[ "$FETCH_MYSQL" -eq 1 ]]; then
    echo "  - MySQL: mysql-test (local)"
    sync_tree "$LOCAL_ROOT/mysql-server/mysql-test" "$COMPAT_DIR/mysql/repos/mysql-server/mysql-test"
  else
    echo "  - MySQL: skipped (--skip-mysql or SCRATCHBIRD_FETCH_MYSQL_TESTS=0)"
  fi

  echo "  - PostgreSQL: src/test (local)"
  sync_tree "$LOCAL_ROOT/postgresql/src/test" "$COMPAT_DIR/postgresql/repos/postgres/src/test"

  write_snapshot_manifest \
    "$SOURCE_MODE" \
    "$LOCAL_ROOT/fbt-repository" \
    "$LOCAL_ROOT/firebird-qa" \
    "$LOCAL_ROOT/mysql-server" \
    "$LOCAL_ROOT/postgresql"

  echo "Done. Commit the updates to ScratchBird."
  exit 0
fi

echo "  - Firebird: fbt-repository"
git clone --depth 1 https://github.com/FirebirdSQL/fbt-repository.git \
  "$WORK_DIR/fbt-repository"
sync_tree "$WORK_DIR/fbt-repository" "$COMPAT_DIR/firebird/repos/fbt-repository"

echo "  - Firebird: firebird-qa"
git clone --depth 1 https://github.com/FirebirdSQL/firebird-qa.git \
  "$WORK_DIR/firebird-qa"
sync_tree "$WORK_DIR/firebird-qa" "$COMPAT_DIR/firebird/repos/firebird-qa"

if [[ "$FETCH_MYSQL" -eq 1 ]]; then
  echo "  - MySQL: mysql-test (sparse checkout)"
  git clone --depth 1 --filter=blob:none --no-checkout \
    https://github.com/mysql/mysql-server.git \
    "$WORK_DIR/mysql-server"
  git -C "$WORK_DIR/mysql-server" sparse-checkout init --cone
  git -C "$WORK_DIR/mysql-server" sparse-checkout set mysql-test
  git -C "$WORK_DIR/mysql-server" checkout
  sync_tree "$WORK_DIR/mysql-server/mysql-test" "$COMPAT_DIR/mysql/repos/mysql-server/mysql-test"
else
  echo "  - MySQL: skipped (--skip-mysql or SCRATCHBIRD_FETCH_MYSQL_TESTS=0)"
fi

echo "  - PostgreSQL: src/test (sparse checkout)"
git clone --depth 1 --filter=blob:none --no-checkout \
  https://github.com/postgres/postgres.git \
  "$WORK_DIR/postgres"
git -C "$WORK_DIR/postgres" sparse-checkout init --cone
git -C "$WORK_DIR/postgres" sparse-checkout set src/test
git -C "$WORK_DIR/postgres" checkout
sync_tree "$WORK_DIR/postgres/src/test" "$COMPAT_DIR/postgresql/repos/postgres/src/test"

write_snapshot_manifest \
  "$SOURCE_MODE" \
  "$WORK_DIR/fbt-repository" \
  "$WORK_DIR/firebird-qa" \
  "$WORK_DIR/mysql-server" \
  "$WORK_DIR/postgres"

echo "Done. Commit the updates to ScratchBird."
