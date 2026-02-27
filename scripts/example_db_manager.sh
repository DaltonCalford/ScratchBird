#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DRIVER_ROOT="${REPO_ROOT}-driver"
WORKSPACE_ROOT="$(cd "${REPO_ROOT}/.." && pwd)"

MODE=""
EXAMPLE_ROOT=""
CONTROL_DIR=""
LOG_DIR=""
PROFILE_DIR=""
DB_FILE=""
CONF_FILE=""
PID_FILE=""
TOKEN_FILE=""
SERVER_LOG=""
BOOTSTRAP_LOG=""
SEED_MARKER=""
EXAMPLE_BUNDLE_MARKER=""
EXAMPLE_BUNDLE_LOG=""
RUNTIME_ENV_FILE=""
CONNECTIONS_JSON_FILE=""

BIND_HOST="127.0.0.1"
NATIVE_PORT=""
PG_PORT=""
MYSQL_PORT=""
FB_PORT=""

SERVER_BIN=""
ISQL_BIN=""
PG_ISQL_BIN=""
MY_ISQL_BIN=""
FB_ISQL_BIN=""
DID_IMPORT_BUNDLE="0"

BOOTSTRAP_USER="${SCRATCHBIRD_EXAMPLE_BOOTSTRAP_USER:-bootstrap_admin}"
BOOTSTRAP_TOKEN="${SCRATCHBIRD_EXAMPLE_BOOTSTRAP_TOKEN:-SbExampleBootstrap_2026!}"

ADMIN_USER="${SCRATCHBIRD_EXAMPLE_ADMIN_USER:-sb_admin}"
ADMIN_PASSWORD="${SCRATCHBIRD_EXAMPLE_ADMIN_PASSWORD:-SbAdmin_Compat1!}"

PG_USER="${SCRATCHBIRD_EXAMPLE_PG_USER:-pg_admin}"
PG_PASSWORD="${SCRATCHBIRD_EXAMPLE_PG_PASSWORD:-PgAdmin_Compat1!}"
PG_DB="${SCRATCHBIRD_EXAMPLE_PG_DB:-regression}"

MYSQL_USER="${SCRATCHBIRD_EXAMPLE_MY_USER:-root}"
MYSQL_PASSWORD="${SCRATCHBIRD_EXAMPLE_MY_PASSWORD:-RootCompat_1!}"
MYSQL_DB="${SCRATCHBIRD_EXAMPLE_MY_DB:-compat_mysql}"

FB_USER="${SCRATCHBIRD_EXAMPLE_FB_USER:-SYSDBA}"
FB_PASSWORD="${SCRATCHBIRD_EXAMPLE_FB_PASSWORD:-SysDbaCompat_1!}"
FB_DB="${SCRATCHBIRD_EXAMPLE_FB_DB:-compat_firebird}"

MAIN_DB="${SCRATCHBIRD_EXAMPLE_MAIN_DB:-main}"

RUN_AS_USER="${SCRATCHBIRD_EXAMPLE_RUN_AS_USER:-$(id -un)}"
RUN_AS_GROUP="${SCRATCHBIRD_EXAMPLE_RUN_AS_GROUP:-$(id -gn)}"

BOOTSTRAP_SQL="${SCRATCHBIRD_EXAMPLE_BOOTSTRAP_SQL:-${REPO_ROOT}/tests/compatibility/scratchbird/example_sql/00_bootstrap_seed.sql}"
EXAMPLE_BUNDLE_ROOT="${SCRATCHBIRD_EXAMPLE_BUNDLE_ROOT:-${WORKSPACE_ROOT}/local_work/findings/example_script_bundle_2}"
EXAMPLE_BUNDLE_IMPORTER="${SCRATCHBIRD_EXAMPLE_BUNDLE_IMPORTER:-${REPO_ROOT}/scripts/emulation/import_example_bundle.py}"

log() {
    printf '[example-db] %s\n' "$*"
}

die() {
    printf '[example-db] error: %s\n' "$*" >&2
    exit 1
}

usage() {
    cat <<'EOF'
Usage: scripts/example_db_manager.sh <command>

Commands:
  dynamic-setup     Recreate and start dynamic example DB for CTest fixtures
  dynamic-teardown  Stop and remove dynamic example DB
  dynamic-status    Show dynamic example DB status

  static-up         Start static example DB (seeded on first run)
  static-down       Stop static example DB without deleting data
  static-refresh    Recreate static example DB and reseed all data
  static-status     Show static example DB status

Environment:
  SCRATCHBIRD_EXAMPLE_DYNAMIC_ROOT   Dynamic root path (default /tmp/scratchbird-example-dynamic)
  SCRATCHBIRD_EXAMPLE_STATIC_ROOT    Static root path (default $HOME/.scratchbird/static-example)
  SCRATCHBIRD_SB_SERVER              Override sb_server binary
  SCRATCHBIRD_SB_ISQL                Override sb_isql binary
  SCRATCHBIRD_EXAMPLE_BOOTSTRAP_SQL  Override bootstrap/seed SQL script path
  SCRATCHBIRD_EXAMPLE_BUNDLE_ROOT    Bundle root (default ../local_work/findings/example_script_bundle_2)
  SCRATCHBIRD_EXAMPLE_IMPORT_BUNDLE  Run bundle importer during setup (default 1)
  SCRATCHBIRD_EXAMPLE_IMPORT_TIMEOUT_SEC  Per-script timeout (default 90)
  SCRATCHBIRD_EXAMPLE_IMPORT_STRICT_NATIVE_CORE  Hard-fail if any native-v3 import fails (default 0)
  SCRATCHBIRD_EXAMPLE_IMPORT_STRICT_EMULATION    Hard-fail if any emulation import fails (default 0)
EOF
}

resolve_binary() {
    local env_var="$1"
    shift
    local value="${!env_var:-}"
    if [[ -n "${value}" ]]; then
        [[ -x "${value}" ]] || die "${env_var} is set but not executable: ${value}"
        printf '%s' "${value}"
        return 0
    fi

    local candidate
    for candidate in "$@"; do
        if [[ -x "${candidate}" ]]; then
            printf '%s' "${candidate}"
            return 0
        fi
    done
    return 1
}

resolve_binaries() {
    SERVER_BIN="$(resolve_binary SCRATCHBIRD_SB_SERVER \
        "${REPO_ROOT}/build/src/sb_server" \
        "${REPO_ROOT}/build/src/server/sb_server")" \
        || die "sb_server not found. Build ScratchBird first."

    ISQL_BIN="$(resolve_binary SCRATCHBIRD_SB_ISQL \
        "${REPO_ROOT}/build/src/sb_isql" \
        "${REPO_ROOT}/build/src/cli/sb_isql" \
        "${DRIVER_ROOT}/build/tracks/alpha/drivers/cli/sb_isql")" \
        || die "sb_isql not found. Build ScratchBird-driver CLI first."

    PG_ISQL_BIN="$(resolve_binary SCRATCHBIRD_PG_ISQL \
        "${REPO_ROOT}/build/src/sb_pg_isql" \
        "${REPO_ROOT}/build/src/cli/sb_pg_isql" \
        "${DRIVER_ROOT}/build/tracks/alpha/drivers/cli/sb_pg_isql" || true)"

    MY_ISQL_BIN="$(resolve_binary SCRATCHBIRD_MY_ISQL \
        "${REPO_ROOT}/build/src/sb_my_isql" \
        "${REPO_ROOT}/build/src/cli/sb_my_isql" \
        "${DRIVER_ROOT}/build/tracks/alpha/drivers/cli/sb_my_isql" || true)"

    FB_ISQL_BIN="$(resolve_binary SCRATCHBIRD_FB_ISQL \
        "${REPO_ROOT}/build/src/sb_fb_isql" \
        "${REPO_ROOT}/build/src/cli/sb_fb_isql" \
        "${DRIVER_ROOT}/build/tracks/alpha/drivers/cli/sb_fb_isql" \
        "$(command -v isql-fb 2>/dev/null || true)" || true)"
}

set_mode_paths() {
    MODE="$1"
    case "${MODE}" in
        dynamic)
            EXAMPLE_ROOT="${SCRATCHBIRD_EXAMPLE_DYNAMIC_ROOT:-/tmp/scratchbird-example-dynamic}"
            NATIVE_PORT="${SCRATCHBIRD_EXAMPLE_DYNAMIC_NATIVE_PORT:-16092}"
            PG_PORT="${SCRATCHBIRD_EXAMPLE_DYNAMIC_PG_PORT:-16432}"
            MYSQL_PORT="${SCRATCHBIRD_EXAMPLE_DYNAMIC_MYSQL_PORT:-16306}"
            FB_PORT="${SCRATCHBIRD_EXAMPLE_DYNAMIC_FB_PORT:-16050}"
            ;;
        static)
            EXAMPLE_ROOT="${SCRATCHBIRD_EXAMPLE_STATIC_ROOT:-${HOME}/.scratchbird/static-example}"
            NATIVE_PORT="${SCRATCHBIRD_EXAMPLE_STATIC_NATIVE_PORT:-13092}"
            PG_PORT="${SCRATCHBIRD_EXAMPLE_STATIC_PG_PORT:-15432}"
            MYSQL_PORT="${SCRATCHBIRD_EXAMPLE_STATIC_MYSQL_PORT:-13306}"
            FB_PORT="${SCRATCHBIRD_EXAMPLE_STATIC_FB_PORT:-13050}"
            ;;
        *)
            die "unknown mode: ${MODE}"
            ;;
    esac

    CONTROL_DIR="${EXAMPLE_ROOT}/control"
    LOG_DIR="${EXAMPLE_ROOT}/logs"
    PROFILE_DIR="${EXAMPLE_ROOT}/profiles"
    DB_FILE="${EXAMPLE_ROOT}/example.sbdb"
    CONF_FILE="${EXAMPLE_ROOT}/example.conf"
    PID_FILE="${CONTROL_DIR}/sb_server.pid"
    TOKEN_FILE="${EXAMPLE_ROOT}/bootstrap.token"
    SERVER_LOG="${LOG_DIR}/sb_server.log"
    BOOTSTRAP_LOG="${LOG_DIR}/bootstrap_seed.out"
    SEED_MARKER="${EXAMPLE_ROOT}/.seeded"
    EXAMPLE_BUNDLE_MARKER="${EXAMPLE_ROOT}/.example_bundle_seeded"
    EXAMPLE_BUNDLE_LOG="${LOG_DIR}/example_bundle_import.out"
    RUNTIME_ENV_FILE="${PROFILE_DIR}/runtime.env"
    CONNECTIONS_JSON_FILE="${PROFILE_DIR}/connections.json"
}

prepare_layout() {
    mkdir -p "${EXAMPLE_ROOT}" "${CONTROL_DIR}" "${LOG_DIR}" "${PROFILE_DIR}"
}

write_token() {
    umask 0077
    printf '%s\n' "${BOOTSTRAP_TOKEN}" > "${TOKEN_FILE}"
    chmod 0600 "${TOKEN_FILE}"
}

write_config() {
    cat > "${CONF_FILE}" <<EOF
[server]
mode = single-database
database = ${DB_FILE}
auto_create = true
pid_file = ${PID_FILE}
run_as_user = ${RUN_AS_USER}
run_as_group = ${RUN_AS_GROUP}
front_door_mode = direct

[network]
bind_address = ${BIND_HOST}
control_socket_dir = ${CONTROL_DIR}
unix_socket = ${CONTROL_DIR}/sb.sock
native_port = ${NATIVE_PORT}
pg_port = ${PG_PORT}
mysql_port = ${MYSQL_PORT}
fb_port = ${FB_PORT}

[authentication]
methods = password
password_hash = argon2id
allow_superuser_remote = true

[logging]
level = info
destination = stderr
timestamps = true
log_connections = true
log_disconnections = true
EOF
}

pid_is_running() {
    local pid="$1"
    [[ -n "${pid}" ]] || return 1
    kill -0 "${pid}" 2>/dev/null
}

stop_matching_processes() {
    local label="$1"
    local pattern="$2"

    local pids=()
    mapfile -t pids < <(pgrep -f -- "${pattern}" || true)
    [[ ${#pids[@]} -gt 0 ]] || return 0

    local pid
    for pid in "${pids[@]}"; do
        [[ "${pid}" != "$$" ]] || continue
        if pid_is_running "${pid}"; then
            log "stopping ${label} pid=${pid}"
            kill "${pid}" || true
        fi
    done

    local i
    for ((i = 0; i < 40; i++)); do
        local alive=0
        for pid in "${pids[@]}"; do
            [[ "${pid}" != "$$" ]] || continue
            if pid_is_running "${pid}"; then
                alive=1
                break
            fi
        done
        [[ ${alive} -eq 0 ]] && return 0
        sleep 0.25
    done

    for pid in "${pids[@]}"; do
        [[ "${pid}" != "$$" ]] || continue
        if pid_is_running "${pid}"; then
            log "forcing ${label} pid=${pid} to stop"
            kill -9 "${pid}" || true
        fi
    done
}

load_pid() {
    if [[ -f "${PID_FILE}" ]]; then
        tr -d ' \n\r' < "${PID_FILE}"
    else
        printf ''
    fi
}

wait_for_port() {
    local host="$1"
    local port="$2"
    local attempts="${3:-120}"
    local sleep_sec="${4:-0.25}"
    local i
    for ((i = 0; i < attempts; i++)); do
        if (echo > "/dev/tcp/${host}/${port}") >/dev/null 2>&1; then
            return 0
        fi
        sleep "${sleep_sec}"
    done
    return 1
}

stop_server() {
    local pid
    pid="$(load_pid)"
    if [[ -n "${pid}" ]] && pid_is_running "${pid}"; then
        log "stopping sb_server pid=${pid}"
        kill "${pid}" || true
        local i
        for ((i = 0; i < 40; i++)); do
            if ! pid_is_running "${pid}"; then
                break
            fi
            sleep 0.25
        done
        if pid_is_running "${pid}"; then
            log "forcing sb_server pid=${pid} to stop"
            kill -9 "${pid}" || true
        fi
    fi

    stop_matching_processes "sb_server" "sb_server --config ${CONF_FILE}"
    stop_matching_processes "listener" "--control-socket-dir ${CONTROL_DIR}"
    stop_matching_processes "parser" "--control-socket ${CONTROL_DIR}/"

    rm -f "${PID_FILE}"
    rm -f "${CONTROL_DIR}"/*.sock 2>/dev/null || true
}

start_server() {
    [[ -x "${SERVER_BIN}" ]] || die "sb_server binary missing: ${SERVER_BIN}"
    [[ -f "${CONF_FILE}" ]] || die "config file missing: ${CONF_FILE}"
    local max_attempts=2
    local attempt
    local startup_error=""
    for ((attempt = 1; attempt <= max_attempts; attempt++)); do
        : > "${SERVER_LOG}"
        if ! SCRATCHBIRD_BOOTSTRAP_TOKEN_FILE="${TOKEN_FILE}" \
            SCRATCHBIRD_BOOTSTRAP_REQUIRE_OWNER_UID="${SCRATCHBIRD_EXAMPLE_BOOTSTRAP_REQUIRE_OWNER_UID:-0}" \
            SCRATCHBIRD_EMULATION_RELAXED_PASSWORD_POLICY="${SCRATCHBIRD_EXAMPLE_RELAXED_PASSWORD_POLICY:-1}" \
            SCRATCHBIRD_NATIVE_FORCE_PASSWORD_AUTH="${SCRATCHBIRD_EXAMPLE_NATIVE_FORCE_PASSWORD_AUTH:-1}" \
            "${SERVER_BIN}" --config "${CONF_FILE}" >> "${SERVER_LOG}" 2>&1; then
            startup_error="failed to start sb_server"
        elif ! wait_for_port "${BIND_HOST}" "${NATIVE_PORT}" 240 0.25; then
            startup_error="native listener failed to come up on ${BIND_HOST}:${NATIVE_PORT}"
        elif ! wait_for_port "${BIND_HOST}" "${PG_PORT}" 240 0.25; then
            startup_error="postgres listener failed to come up on ${BIND_HOST}:${PG_PORT}"
        elif ! wait_for_port "${BIND_HOST}" "${MYSQL_PORT}" 320 0.25; then
            startup_error="mysql listener failed to come up on ${BIND_HOST}:${MYSQL_PORT}"
        elif ! wait_for_port "${BIND_HOST}" "${FB_PORT}" 240 0.25; then
            startup_error="firebird listener failed to come up on ${BIND_HOST}:${FB_PORT}"
        else
            return 0
        fi

        tail -n 100 "${SERVER_LOG}" >&2 || true
        if (( attempt < max_attempts )); then
            log "startup attempt ${attempt}/${max_attempts} failed: ${startup_error}; retrying"
            stop_server
            sleep 1
        fi
    done

    die "${startup_error}"
}

run_bootstrap_seed() {
    [[ -f "${BOOTSTRAP_SQL}" ]] || die "bootstrap SQL not found: ${BOOTSTRAP_SQL}"
    [[ -x "${ISQL_BIN}" ]] || die "sb_isql binary missing: ${ISQL_BIN}"

    : > "${BOOTSTRAP_LOG}"
    if ! "${ISQL_BIN}" "${MAIN_DB}" \
        --mode=local-ipc \
        --ipc-method=tcp \
        -H "${BIND_HOST}" \
        -p "${NATIVE_PORT}" \
        -U "${BOOTSTRAP_USER}" \
        -P "${BOOTSTRAP_TOKEN}" \
        -b \
        -f "${BOOTSTRAP_SQL}" \
        -o "${BOOTSTRAP_LOG}" \
        -q; then
        tail -n 200 "${BOOTSTRAP_LOG}" >&2 || true
        die "bootstrap/seed SQL failed"
    fi
    touch "${SEED_MARKER}"
}

run_example_bundle_import() {
    DID_IMPORT_BUNDLE="0"
    local enabled="${SCRATCHBIRD_EXAMPLE_IMPORT_BUNDLE:-1}"
    if [[ "${enabled}" == "0" ]]; then
        log "example bundle import disabled (SCRATCHBIRD_EXAMPLE_IMPORT_BUNDLE=0)"
        return 0
    fi

    if [[ ! -f "${EXAMPLE_BUNDLE_IMPORTER}" ]]; then
        log "example bundle importer missing, skipping: ${EXAMPLE_BUNDLE_IMPORTER}"
        return 0
    fi
    if [[ ! -f "${EXAMPLE_BUNDLE_ROOT}/manifest.csv" ]]; then
        log "example bundle manifest missing, skipping: ${EXAMPLE_BUNDLE_ROOT}/manifest.csv"
        return 0
    fi

    : > "${EXAMPLE_BUNDLE_LOG}"
    local import_timeout="${SCRATCHBIRD_EXAMPLE_IMPORT_TIMEOUT_SEC:-90}"
    local strict_native_core="${SCRATCHBIRD_EXAMPLE_IMPORT_STRICT_NATIVE_CORE:-0}"
    local strict_emulation="${SCRATCHBIRD_EXAMPLE_IMPORT_STRICT_EMULATION:-0}"
    local output_root="${PROFILE_DIR}/example_bundle"
    local -a cmd=(
        python3 "${EXAMPLE_BUNDLE_IMPORTER}"
        --bundle-root "${EXAMPLE_BUNDLE_ROOT}"
        --output-root "${output_root}"
        --timeout-sec "${import_timeout}"
        --native-isql "${ISQL_BIN}"
        --native-host "${BIND_HOST}"
        --native-port "${NATIVE_PORT}"
        --native-db "${MAIN_DB}"
        --native-user "${ADMIN_USER}"
        --native-password "${ADMIN_PASSWORD}"
        --pg-isql "${PG_ISQL_BIN}"
        --pg-host "${BIND_HOST}"
        --pg-port "${PG_PORT}"
        --pg-db "${PG_DB}"
        --pg-user "${PG_USER}"
        --pg-password "${PG_PASSWORD}"
        --my-isql "${MY_ISQL_BIN}"
        --my-host "${BIND_HOST}"
        --my-port "${MYSQL_PORT}"
        --my-db "${MYSQL_DB}"
        --my-user "${MYSQL_USER}"
        --my-password "${MYSQL_PASSWORD}"
        --fb-isql "${FB_ISQL_BIN}"
        --fb-work-db-root "${EXAMPLE_ROOT}/emulated/firebird"
    )
    if [[ "${strict_native_core}" == "1" ]]; then
        cmd+=(--strict-native-core)
    fi
    if [[ "${strict_emulation}" == "1" ]]; then
        cmd+=(--strict-emulation)
    fi

    if ! "${cmd[@]}" > "${EXAMPLE_BUNDLE_LOG}" 2>&1; then
        tail -n 200 "${EXAMPLE_BUNDLE_LOG}" >&2 || true
        die "example bundle import failed"
    fi

    DID_IMPORT_BUNDLE="1"
    touch "${EXAMPLE_BUNDLE_MARKER}"
}

write_connection_profiles() {
    cat > "${RUNTIME_ENV_FILE}" <<EOF
export SCRATCHBIRD_EXAMPLE_ROOT='${EXAMPLE_ROOT}'
export SCRATCHBIRD_SB_SERVER='${SERVER_BIN}'
export SCRATCHBIRD_SB_ISQL='${ISQL_BIN}'
export SCRATCHBIRD_PG_ISQL='${PG_ISQL_BIN}'
export SCRATCHBIRD_MY_ISQL='${MY_ISQL_BIN}'
export SCRATCHBIRD_FB_ISQL='${FB_ISQL_BIN}'

export SCRATCHBIRD_NATIVE_HOST='${BIND_HOST}'
export SCRATCHBIRD_NATIVE_PORT='${NATIVE_PORT}'
export SCRATCHBIRD_NATIVE_DB='${MAIN_DB}'
export SCRATCHBIRD_NATIVE_USER='${ADMIN_USER}'
export SCRATCHBIRD_NATIVE_PASSWORD='${ADMIN_PASSWORD}'

export SCRATCHBIRD_PG_HOST='${BIND_HOST}'
export SCRATCHBIRD_PG_PORT='${PG_PORT}'
export SCRATCHBIRD_PG_DB='${PG_DB}'
export SCRATCHBIRD_PG_USER='${PG_USER}'
export SCRATCHBIRD_PG_PASSWORD='${PG_PASSWORD}'
export SCRATCHBIRD_PG_PROVISION_USER='0'
export SCRATCHBIRD_PG_OWNER_DB='${MAIN_DB}'
export SCRATCHBIRD_PG_COMPAT_RUN='1'

export SCRATCHBIRD_MY_HOST='${BIND_HOST}'
export SCRATCHBIRD_MY_PORT='${MYSQL_PORT}'
export SCRATCHBIRD_MY_DB='${MYSQL_DB}'
export SCRATCHBIRD_MY_USER='${MYSQL_USER}'
export SCRATCHBIRD_MY_PASSWORD='${MYSQL_PASSWORD}'
export SCRATCHBIRD_MY_OWNER_DB='${MAIN_DB}'
export SCRATCHBIRD_MY_DB_PER_TEST='0'
export SCRATCHBIRD_MY_COMPAT_RUN='1'

export SCRATCHBIRD_FB_USER='${FB_USER}'
export SCRATCHBIRD_FB_PASSWORD='${FB_PASSWORD}'
EOF

    cat > "${CONNECTIONS_JSON_FILE}" <<EOF
{
  "generated_by": "scripts/example_db_manager.sh",
  "mode": "${MODE}",
  "root": "${EXAMPLE_ROOT}",
  "example_bundle_summary": "${PROFILE_DIR}/example_bundle/SUMMARY.json",
  "clients": {
    "sb_isql": "${ISQL_BIN}",
    "sb_pg_isql": "${PG_ISQL_BIN}",
    "sb_my_isql": "${MY_ISQL_BIN}",
    "sb_fb_isql": "${FB_ISQL_BIN}"
  },
  "native": {
    "host": "${BIND_HOST}",
    "port": ${NATIVE_PORT},
    "database": "${MAIN_DB}",
    "user": "${ADMIN_USER}",
    "password": "${ADMIN_PASSWORD}"
  },
  "postgresql": {
    "host": "${BIND_HOST}",
    "port": ${PG_PORT},
    "database": "${PG_DB}",
    "user": "${PG_USER}",
    "password": "${PG_PASSWORD}"
  },
  "mysql": {
    "host": "${BIND_HOST}",
    "port": ${MYSQL_PORT},
    "database": "${MYSQL_DB}",
    "user": "${MYSQL_USER}",
    "password": "${MYSQL_PASSWORD}"
  },
  "firebird": {
    "host": "${BIND_HOST}",
    "port": ${FB_PORT},
    "database": "${FB_DB}",
    "user": "${FB_USER}",
    "password": "${FB_PASSWORD}"
  }
}
EOF
}

print_status() {
    local pid
    pid="$(load_pid)"
    log "mode=${MODE}"
    log "root=${EXAMPLE_ROOT}"
    log "db=${DB_FILE}"
    log "config=${CONF_FILE}"
    log "seeded=$([[ -f "${SEED_MARKER}" ]] && echo yes || echo no)"
    log "example_bundle_seeded=$([[ -f "${EXAMPLE_BUNDLE_MARKER}" ]] && echo yes || echo no)"
    if [[ -n "${pid}" ]] && pid_is_running "${pid}"; then
        log "server=running pid=${pid}"
    else
        log "server=stopped"
    fi
    log "native=${BIND_HOST}:${NATIVE_PORT} user=${ADMIN_USER} db=${MAIN_DB}"
    log "postgres=${BIND_HOST}:${PG_PORT} user=${PG_USER} db=${PG_DB}"
    log "mysql=${BIND_HOST}:${MYSQL_PORT} user=${MYSQL_USER} db=${MYSQL_DB}"
    log "firebird=${BIND_HOST}:${FB_PORT} user=${FB_USER} db=${FB_DB}"
    log "profiles=${PROFILE_DIR}"
}

dynamic_setup() {
    set_mode_paths dynamic
    resolve_binaries

    log "preparing dynamic example database at ${EXAMPLE_ROOT}"
    stop_server
    rm -rf "${EXAMPLE_ROOT}"
    prepare_layout
    write_token
    write_config
    start_server
    run_bootstrap_seed
    write_connection_profiles
    run_example_bundle_import
    if [[ "${DID_IMPORT_BUNDLE}" == "1" ]]; then
        log "restarting listeners after example bundle import"
        stop_server
        start_server
        write_connection_profiles
    fi
    print_status
}

dynamic_teardown() {
    set_mode_paths dynamic
    stop_server
    rm -rf "${EXAMPLE_ROOT}"
    log "dynamic example database removed: ${EXAMPLE_ROOT}"
}

dynamic_status() {
    set_mode_paths dynamic
    print_status
}

static_up() {
    set_mode_paths static
    resolve_binaries
    prepare_layout
    write_token
    write_config

    local pid
    pid="$(load_pid)"
    if [[ -n "${pid}" ]] && pid_is_running "${pid}"; then
        log "static example server already running pid=${pid}"
        write_connection_profiles
        print_status
        return 0
    fi

    start_server
    if [[ ! -f "${SEED_MARKER}" ]]; then
        run_bootstrap_seed
    fi
    write_connection_profiles
    if [[ ! -f "${EXAMPLE_BUNDLE_MARKER}" ]]; then
        run_example_bundle_import
        if [[ "${DID_IMPORT_BUNDLE}" == "1" ]]; then
            log "restarting listeners after example bundle import"
            stop_server
            start_server
            write_connection_profiles
        fi
    fi
    print_status
}

static_down() {
    set_mode_paths static
    stop_server
    log "static example server stopped"
}

static_refresh() {
    set_mode_paths static
    resolve_binaries
    stop_server
    rm -rf "${EXAMPLE_ROOT}"
    prepare_layout
    write_token
    write_config
    start_server
    run_bootstrap_seed
    write_connection_profiles
    run_example_bundle_import
    if [[ "${DID_IMPORT_BUNDLE}" == "1" ]]; then
        log "restarting listeners after example bundle import"
        stop_server
        start_server
        write_connection_profiles
    fi
    print_status
}

static_status() {
    set_mode_paths static
    print_status
}

main() {
    local cmd="${1:-}"
    case "${cmd}" in
        dynamic-setup) dynamic_setup ;;
        dynamic-teardown) dynamic_teardown ;;
        dynamic-status) dynamic_status ;;
        static-up) static_up ;;
        static-down) static_down ;;
        static-refresh) static_refresh ;;
        static-status) static_status ;;
        -h|--help|help|"") usage ;;
        *) die "unknown command: ${cmd}" ;;
    esac
}

main "$@"
