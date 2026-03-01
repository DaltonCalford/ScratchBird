#!/usr/bin/env bash
if [ -z "${BASH_VERSION:-}" ]; then
  exec bash "$0" "$@"
fi

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_DIR=""
TOOLS_DIR="$SCRIPT_DIR"

SYNC_OPTIONS=()
REPO_REQUESTS=()
RUN_ARGS=()

SELECT_ALL=1
START_CONTAINER=0
START_IP="127.0.0.1"
START_PORT="2222"

usage() {
  cat <<'EOF'
Usage: bootstrap-workspace.sh [options] [repos...]

Requires sync-repos.sh, install-workspace-build-scripts.sh, and run.sh to be available in --tools-dir (defaults to this script directory).
Options:
  --workspace <path>      Host workspace path (default: current directory)
  --tools-dir <path>      Path to dev-unified scripts (default: script directory)
  --all                   Sync/install all repos (default)
  --branch <name>         Branch for sync operation
  --force                 Force sync with git reset --hard to remote
  --no-pull               Skip pulling existing repos
  --depth <n>             Shallow clone depth for missing repos
  --verbose               Verbose sync output
  --start                 Start Docker after setup
  --ip <ip>               SSH bind IP for start (default: 127.0.0.1)
  --port <port>           SSH bind port for start (default: 2222)
  --name <container>      Docker container name
  --image <name>          Docker image name
  --user <linux user>     Container SSH user
  --ssh-key <pubkey>      Public key file injected into container
  --help                  Show this help

Positional repos override --all:
  scratchbird, scratchbird-ai, scratchbird-driver, scratchrobin, clickhouse,
  duckdb, influxdb, milvus, neo4j, mongo, redis, opensearch, cassandra,
  mysql-server, postgresql, firebird, dbeaver, server (alias: mariadb)

Examples:
  bash bootstrap-workspace.sh --workspace /home/me/CliWork --all
  bash bootstrap-workspace.sh --workspace /home/me/CliWork --start --ip 127.0.0.5
  bash bootstrap-workspace.sh --workspace /home/me/CliWork scratchbird server
  EOF
}

require_tool() {
  local tool="$1"
  if [ ! -f "$tool" ] || [ ! -r "$tool" ]; then
    echo "Missing required tool: $tool"
    exit 1
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --workspace)
      WORKSPACE_DIR="$2"
      shift 2
      ;;
    --tools-dir)
      TOOLS_DIR="$2"
      shift 2
      ;;
    --all)
      SELECT_ALL=1
      shift
      ;;
    --branch|--depth)
      SYNC_OPTIONS+=("$1" "$2")
      shift 2
      ;;
    --force|--no-pull|--verbose)
      SYNC_OPTIONS+=("$1")
      shift
      ;;
    --start)
      START_CONTAINER=1
      shift
      ;;
    --ip)
      START_IP="$2"
      RUN_ARGS+=(--ip "$2")
      shift 2
      ;;
    --port)
      START_PORT="$2"
      RUN_ARGS+=(--port "$2")
      shift 2
      ;;
    --name|--image|--user|--ssh-key)
      RUN_ARGS+=("$1" "$2")
      shift 2
      ;;
    --build)
      RUN_ARGS+=("--build")
      if [ "${2:-}" ] && [[ "${2:-}" != --* ]]; then
        RUN_ARGS+=("$2")
        shift 2
      else
        shift 1
      fi
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    --*)
      echo "Unknown option: $1"
      usage
      exit 1
      ;;
    *)
      SELECT_ALL=0
      REPO_REQUESTS+=("$1")
      shift
      ;;
  esac
done

if [ -z "${WORKSPACE_DIR}" ]; then
  WORKSPACE_DIR="$(pwd)"
fi

if [ ! -d "$WORKSPACE_DIR" ]; then
  echo "Workspace missing: $WORKSPACE_DIR"
  exit 1
fi

TOOLS_DIR="$(cd "$TOOLS_DIR" && pwd)"

SYNC_SCRIPT="$TOOLS_DIR/sync-repos.sh"
INSTALL_SCRIPT="$TOOLS_DIR/install-workspace-build-scripts.sh"
RUN_SCRIPT="$TOOLS_DIR/run.sh"

require_tool "$SYNC_SCRIPT"
require_tool "$INSTALL_SCRIPT"
require_tool "$RUN_SCRIPT"

SYNC_CMD=(
  bash "$SYNC_SCRIPT"
  --base-dir "$WORKSPACE_DIR"
)
INSTALL_CMD=(
  bash "$INSTALL_SCRIPT"
  --base-dir "$WORKSPACE_DIR"
  --overwrite
)

if [ "$SELECT_ALL" -eq 1 ]; then
  SYNC_CMD+=(--all)
else
  if [ "${#REPO_REQUESTS[@]}" -gt 0 ]; then
    SYNC_CMD+=("${REPO_REQUESTS[@]}")
    INSTALL_CMD+=("${REPO_REQUESTS[@]}")
  fi
fi
SYNC_CMD+=("${SYNC_OPTIONS[@]}")

log() {
  echo "[$(date +'%F %T')] $*"
}

log "Refreshing repository set in: $WORKSPACE_DIR"
"${SYNC_CMD[@]}"

log "Installing build/test/start scripts into: $WORKSPACE_DIR"
"${INSTALL_CMD[@]}"

if [ "$START_CONTAINER" -eq 1 ]; then
  log "Starting Docker container:"
  bash "$RUN_SCRIPT" --workspace "$WORKSPACE_DIR" --ip "$START_IP" --port "$START_PORT" "${RUN_ARGS[@]}"
  log "SSH into container:"
  echo "  ssh builder@${START_IP} -p ${START_PORT}"
  echo "From /workspace inside SSH:"
  echo "  ./start-scratchbird-environment.sh --bind-address ${START_IP} --native-port 13092"
else
  log "Workspace scripts are ready."
  log "To start Docker now, run:"
  echo "  bash \"$RUN_SCRIPT\" --workspace \"$WORKSPACE_DIR\" --ip ${START_IP} --port ${START_PORT}"
fi
