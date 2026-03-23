#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="${SB_VERIFY_REPO_ROOT:-${WORKSPACE_ROOT}/repos}"
SCRATCHBIRD_REPO="${REPO_ROOT}/ScratchBird"
ACTION="${1:-status}"

PG_CONTAINER="sb-ref-postgresql"
MY_CONTAINER="sb-ref-mysql"
FB_CONTAINER="sb-ref-firebird"

REF_PG_PORT="${SB_VERIFY_REF_PG_PORT:-5432}"
REF_MY_PORT="${SB_VERIFY_REF_MY_PORT:-3306}"
REF_FB_PORT="${SB_VERIFY_REF_FB_PORT:-3050}"

REF_PG_USER="${SB_VERIFY_REF_PG_USER:-postgres}"
REF_PG_PASSWORD="${SB_VERIFY_REF_PG_PASSWORD:-postgres}"
REF_PG_DB="${SB_VERIFY_REF_PG_DB:-postgres}"

REF_MY_USER="${SB_VERIFY_REF_MY_USER:-root}"
REF_MY_PASSWORD="${SB_VERIFY_REF_MY_PASSWORD:-root}"
REF_MY_DB="${SB_VERIFY_REF_MY_DB:-mysql}"

REF_FB_USER="${SB_VERIFY_REF_FB_USER:-SYSDBA}"
REF_FB_PASSWORD="${SB_VERIFY_REF_FB_PASSWORD:-masterkey}"
REF_FB_DB="${SB_VERIFY_REF_FB_DB:-/firebird/data/ref_verify_firebird.fdb}"

DOCKER_BIN=""
if command -v docker >/dev/null 2>&1; then
  if docker info >/dev/null 2>&1; then
    DOCKER_BIN="docker"
  elif command -v sudo >/dev/null 2>&1 && sudo docker info >/dev/null 2>&1; then
    DOCKER_BIN="sudo docker"
  fi
fi

wait_port() {
  local host="$1"
  local port="$2"
  local retries="${3:-60}"
  local i
  for ((i=0; i<retries; i++)); do
    if (echo >"/dev/tcp/${host}/${port}") >/dev/null 2>&1; then
      return 0
    fi
    sleep 1
  done
  return 1
}

ensure_firebird_reference_db() {
  if ! command -v isql-fb >/dev/null 2>&1; then
    return 0
  fi

  local fb_dsn="127.0.0.1/${REF_FB_PORT}:${REF_FB_DB}"
  local attempt
  for attempt in $(seq 1 30); do
    if cat <<SQL | isql-fb "${fb_dsn}" -user "${REF_FB_USER}" -password "${REF_FB_PASSWORD}" >/dev/null 2>&1
SELECT 1 FROM RDB\$DATABASE;
QUIT;
SQL
    then
      return 0
    fi

    cat <<SQL | isql-fb -user "${REF_FB_USER}" -password "${REF_FB_PASSWORD}" >/dev/null 2>&1 || true
CREATE DATABASE '${fb_dsn}' USER '${REF_FB_USER}' PASSWORD '${REF_FB_PASSWORD}';
QUIT;
SQL
    sleep 1
  done

  echo "Failed to create or attach reference Firebird database: ${fb_dsn}" >&2
  return 1
}

require_docker() {
  if [[ -z "${DOCKER_BIN}" ]]; then
    echo "Docker is required to start reference containers but is not available." >&2
    echo "Install docker and ensure daemon is running, or skip runtime bootstrap." >&2
    exit 1
  fi
}

start_scratchbird_stack() {
  if [[ ! -x "${SCRATCHBIRD_REPO}/scripts/example_db_manager.sh" ]]; then
    echo "ScratchBird example_db_manager.sh not found: ${SCRATCHBIRD_REPO}" >&2
    exit 1
  fi
  echo "[runtime] starting ScratchBird static stack"
  (
    cd "${SCRATCHBIRD_REPO}"
    SCRATCHBIRD_EXAMPLE_IMPORT_BUNDLE=0 ./scripts/example_db_manager.sh static-up
  )
}

stop_scratchbird_stack() {
  if [[ -x "${SCRATCHBIRD_REPO}/scripts/example_db_manager.sh" ]]; then
    echo "[runtime] stopping ScratchBird static stack"
    (
      cd "${SCRATCHBIRD_REPO}"
      ./scripts/example_db_manager.sh static-down || true
    )
  fi
}

container_exists() {
  local name="$1"
  ${DOCKER_BIN} ps -a --format '{{.Names}}' | grep -Fxq "${name}"
}

container_running() {
  local name="$1"
  ${DOCKER_BIN} ps --format '{{.Names}}' | grep -Fxq "${name}"
}

start_or_run_container() {
  local name="$1"
  shift
  if container_running "${name}"; then
    echo "[runtime] container already running: ${name}"
    return 0
  fi
  if container_exists "${name}"; then
    echo "[runtime] starting existing container: ${name}"
    ${DOCKER_BIN} start "${name}" >/dev/null
  else
    echo "[runtime] creating container: ${name}"
    ${DOCKER_BIN} run -d --name "${name}" "$@" >/dev/null
  fi
}

start_reference_containers() {
  require_docker

  start_or_run_container "${PG_CONTAINER}" \
    -e POSTGRES_USER="${REF_PG_USER}" \
    -e POSTGRES_PASSWORD="${REF_PG_PASSWORD}" \
    -e POSTGRES_DB="${REF_PG_DB}" \
    -p "${REF_PG_PORT}:5432" \
    postgres:16

  start_or_run_container "${MY_CONTAINER}" \
    -e MYSQL_ROOT_PASSWORD="${REF_MY_PASSWORD}" \
    -e MYSQL_DATABASE="${REF_MY_DB}" \
    -p "${REF_MY_PORT}:3306" \
    mysql:8.4

  start_or_run_container "${FB_CONTAINER}" \
    -e ISC_PASSWORD="${REF_FB_PASSWORD}" \
    -p "${REF_FB_PORT}:3050" \
    jacobalberty/firebird:v3.0

  echo "[runtime] waiting for reference endpoints"
  wait_port 127.0.0.1 "${REF_PG_PORT}" 90
  wait_port 127.0.0.1 "${REF_MY_PORT}" 90
  wait_port 127.0.0.1 "${REF_FB_PORT}" 90

  ensure_firebird_reference_db
}

stop_reference_containers() {
  if [[ -z "${DOCKER_BIN}" ]]; then
    return 0
  fi
  for c in "${PG_CONTAINER}" "${MY_CONTAINER}" "${FB_CONTAINER}"; do
    if container_exists "${c}"; then
      echo "[runtime] removing container: ${c}"
      ${DOCKER_BIN} rm -f "${c}" >/dev/null || true
    fi
  done
}

status_stack() {
  echo "[runtime] ScratchBird repo: ${SCRATCHBIRD_REPO}"
  if [[ -x "${SCRATCHBIRD_REPO}/scripts/example_db_manager.sh" ]]; then
    (
      cd "${SCRATCHBIRD_REPO}"
      ./scripts/example_db_manager.sh static-status || true
    )
  else
    echo "[runtime] ScratchBird static stack: unavailable (repo/script missing)"
  fi

  if [[ -n "${DOCKER_BIN}" ]]; then
    echo "[runtime] Docker containers:"
    ${DOCKER_BIN} ps --format 'table {{.Names}}\t{{.Status}}\t{{.Ports}}' | grep -E "NAMES|${PG_CONTAINER}|${MY_CONTAINER}|${FB_CONTAINER}" || true
  else
    echo "[runtime] Docker unavailable"
  fi
}

case "${ACTION}" in
  up)
    start_scratchbird_stack
    start_reference_containers
    status_stack
    ;;
  down)
    stop_reference_containers
    stop_scratchbird_stack
    ;;
  status)
    status_stack
    ;;
  *)
    echo "Usage: $(basename "$0") [up|down|status]" >&2
    exit 1
    ;;
esac
