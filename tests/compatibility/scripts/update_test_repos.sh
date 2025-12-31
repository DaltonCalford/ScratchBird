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

cleanup() {
  rm -rf "$WORK_DIR"
}
trap cleanup EXIT

echo "Updating vendored compatibility test repositories..."

echo "  - Firebird: fbt-repository"
git clone --depth 1 https://github.com/FirebirdSQL/fbt-repository.git \
  "$WORK_DIR/fbt-repository"
mkdir -p "$COMPAT_DIR/firebird/repos/fbt-repository"
rsync -a --delete --exclude='.git' \
  "$WORK_DIR/fbt-repository/" \
  "$COMPAT_DIR/firebird/repos/fbt-repository/"

echo "  - MySQL: mysql-test (sparse checkout)"
git clone --depth 1 --filter=blob:none --no-checkout \
  https://github.com/mysql/mysql-server.git \
  "$WORK_DIR/mysql-server"
git -C "$WORK_DIR/mysql-server" sparse-checkout init --cone
git -C "$WORK_DIR/mysql-server" sparse-checkout set mysql-test
git -C "$WORK_DIR/mysql-server" checkout
mkdir -p "$COMPAT_DIR/mysql/repos/mysql-server/mysql-test"
rsync -a --delete --exclude='.git' \
  "$WORK_DIR/mysql-server/mysql-test/" \
  "$COMPAT_DIR/mysql/repos/mysql-server/mysql-test/"

echo "  - PostgreSQL: regress tests (sparse checkout)"
git clone --depth 1 --filter=blob:none --no-checkout \
  https://github.com/postgres/postgres.git \
  "$WORK_DIR/postgres"
git -C "$WORK_DIR/postgres" sparse-checkout init --cone
git -C "$WORK_DIR/postgres" sparse-checkout set src/test/regress
git -C "$WORK_DIR/postgres" checkout
mkdir -p "$COMPAT_DIR/postgresql/repos/postgres/src/test/regress"
rsync -a --delete --exclude='.git' \
  "$WORK_DIR/postgres/src/test/regress/" \
  "$COMPAT_DIR/postgresql/repos/postgres/src/test/regress/"

echo "Done. Commit the updates to ScratchBird."
