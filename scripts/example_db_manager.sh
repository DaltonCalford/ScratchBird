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
POST_BOOTSTRAP_LOG=""
SEED_MARKER=""
EXAMPLE_BUNDLE_MARKER=""
EXAMPLE_BUNDLE_LOG=""
RUNTIME_ENV_FILE=""
CONNECTIONS_JSON_FILE=""
GENERATED_AUTH_ENV_FILE=""
GENERATED_BOOTSTRAP_SQL_FILE=""
GENERATED_POST_BOOTSTRAP_SQL_FILE=""

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

ADMIN_USER=""
ADMIN_PASSWORD=""

PG_USER=""
PG_PASSWORD=""
PG_DB="${SCRATCHBIRD_EXAMPLE_PG_DB:-regression}"

MYSQL_USER=""
MYSQL_PASSWORD=""
MYSQL_DB="${SCRATCHBIRD_EXAMPLE_MY_DB:-compat_mysql}"

FB_USER=""
FB_PASSWORD=""
FB_DB="${SCRATCHBIRD_EXAMPLE_FB_DB:-compat_firebird}"

COMPAT_CANONICAL_ADMIN_USER="${SCRATCHBIRD_EXAMPLE_COMPAT_ADMIN_USER:-sys_admin}"
COMPAT_CANONICAL_ADMIN_USERID="${SCRATCHBIRD_EXAMPLE_COMPAT_ADMIN_USERID:-u_sys_admin}"
COMPAT_CANONICAL_USER="${SCRATCHBIRD_EXAMPLE_COMPAT_CANONICAL_USER:-public_user}"
COMPAT_CANONICAL_USERID="${SCRATCHBIRD_EXAMPLE_COMPAT_CANONICAL_USERID:-u_public_user}"
COMPAT_NATIVE_USER=""
COMPAT_NATIVE_PASSWORD=""
COMPAT_PG_USER=""
COMPAT_PG_PASSWORD=""
COMPAT_MY_USER=""
COMPAT_MY_PASSWORD=""
COMPAT_FB_USER=""
COMPAT_FB_PASSWORD=""
COMPAT_FB_EXTERNAL_ALIAS="${SCRATCHBIRD_EXAMPLE_COMPAT_FB_EXTERNAL_ALIAS:-public.user}"

MAIN_DB="${SCRATCHBIRD_EXAMPLE_MAIN_DB:-main}"
NATIVE_SSLMODE="${SCRATCHBIRD_EXAMPLE_NATIVE_SSLMODE:-disable}"
AUTH_METHODS="${SCRATCHBIRD_EXAMPLE_AUTH_METHODS:-password}"
AUTH_PASSWORD_HASH="${SCRATCHBIRD_EXAMPLE_AUTH_PASSWORD_HASH:-argon2id}"
AUTH_ALLOW_SUPERUSER_REMOTE="${SCRATCHBIRD_EXAMPLE_ALLOW_SUPERUSER_REMOTE:-true}"

RUN_AS_USER="${SCRATCHBIRD_EXAMPLE_RUN_AS_USER:-$(id -un)}"
RUN_AS_GROUP="${SCRATCHBIRD_EXAMPLE_RUN_AS_GROUP:-$(id -gn)}"

AUTH_MANIFEST="${SCRATCHBIRD_EXAMPLE_AUTH_MANIFEST:-${REPO_ROOT}/resources/bootstrap/default_auth_manifest.json}"
SEED_RENDERER="${SCRATCHBIRD_EXAMPLE_SEED_RENDERER:-${REPO_ROOT}/scripts/emulation/render_example_seed_sql.py}"
BOOTSTRAP_SQL="${SCRATCHBIRD_EXAMPLE_BOOTSTRAP_SQL:-}"
POST_BOOTSTRAP_SQL="${SCRATCHBIRD_EXAMPLE_POST_BOOTSTRAP_SQL:-}"
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
  SCRATCHBIRD_EXAMPLE_AUTH_MANIFEST  Override auth manifest path (default resources/bootstrap/default_auth_manifest.json)
  SCRATCHBIRD_EXAMPLE_SEED_RENDERER  Override manifest->SQL renderer script
  SCRATCHBIRD_EXAMPLE_BOOTSTRAP_SQL  Override generated bootstrap SQL path
  SCRATCHBIRD_EXAMPLE_POST_BOOTSTRAP_SQL  Override generated post-bootstrap SQL path
  SCRATCHBIRD_EXAMPLE_AUTH_METHODS   Auth methods list (default password)
  SCRATCHBIRD_EXAMPLE_AUTH_PASSWORD_HASH Password hash algorithm (default argon2id)
  SCRATCHBIRD_EXAMPLE_ALLOW_SUPERUSER_REMOTE Allow remote superuser auth (default true)
  SCRATCHBIRD_EXAMPLE_COMPAT_*       Override default cross-engine identity profile aliases/passwords
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

    PG_ISQL_BIN="$(resolve_binary SCRATCHBIRD_PG_PSQL_BIN \
        "${WORKSPACE_ROOT}/postgresql/build_codex/src/bin/psql/psql" \
        "${WORKSPACE_ROOT}/postgresql/build_codex2/src/bin/psql/psql" \
        "${WORKSPACE_ROOT}/postgresql/build/src/bin/psql/psql" \
        "${SCRATCHBIRD_PG_ISQL:-}" || true)"

    MY_ISQL_BIN="$(resolve_binary SCRATCHBIRD_MYSQL_CLI_BIN \
        "${WORKSPACE_ROOT}/mysql-server/build_codex2/runtime_output_directory/mysql" \
        "${WORKSPACE_ROOT}/mysql-server/build_codex/runtime_output_directory/mysql" \
        "${WORKSPACE_ROOT}/mysql-server/build/runtime_output_directory/mysql" \
        "${SCRATCHBIRD_MY_ISQL:-}" || true)"

    FB_ISQL_BIN="$(resolve_binary SCRATCHBIRD_FB_NATIVE_ISQL \
        "${WORKSPACE_ROOT}/firebird/gen/Release/firebird/bin/isql" \
        "${WORKSPACE_ROOT}/firebird/gen/Debug/firebird/bin/isql" \
        "${WORKSPACE_ROOT}/firebird/build/bin/isql" \
        "${WORKSPACE_ROOT}/firebird/build/isql" \
        "${SCRATCHBIRD_FB_ISQL:-}" || true)"
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
    POST_BOOTSTRAP_LOG="${LOG_DIR}/post_bootstrap_seed.out"
    SEED_MARKER="${EXAMPLE_ROOT}/.seeded"
    EXAMPLE_BUNDLE_MARKER="${EXAMPLE_ROOT}/.example_bundle_seeded"
    EXAMPLE_BUNDLE_LOG="${LOG_DIR}/example_bundle_import.out"
    RUNTIME_ENV_FILE="${PROFILE_DIR}/runtime.env"
    CONNECTIONS_JSON_FILE="${PROFILE_DIR}/connections.json"
    GENERATED_AUTH_ENV_FILE="${PROFILE_DIR}/auth_defaults.env"
    GENERATED_BOOTSTRAP_SQL_FILE="${PROFILE_DIR}/bootstrap_seed.generated.sql"
    GENERATED_POST_BOOTSTRAP_SQL_FILE="${PROFILE_DIR}/post_bootstrap_seed.generated.sql"
}

prepare_layout() {
    mkdir -p "${EXAMPLE_ROOT}" "${CONTROL_DIR}" "${LOG_DIR}" "${PROFILE_DIR}"
}

load_auth_defaults() {
    [[ -f "${AUTH_MANIFEST}" ]] || die "auth manifest not found: ${AUTH_MANIFEST}"
    [[ -f "${SEED_RENDERER}" ]] || die "seed renderer not found: ${SEED_RENDERER}"

    if [[ -n "${PROFILE_DIR}" ]]; then
        mkdir -p "${PROFILE_DIR}"
    fi

    local env_out=""
    local cleanup_env="0"
    local -a cmd=(
        python3 "${SEED_RENDERER}"
        --manifest "${AUTH_MANIFEST}"
        --compat-admin-userid "${COMPAT_CANONICAL_ADMIN_USERID}"
        --compat-admin-user "${COMPAT_CANONICAL_ADMIN_USER}"
        --compat-canonical-userid "${COMPAT_CANONICAL_USERID}"
        --compat-canonical-user "${COMPAT_CANONICAL_USER}"
        --compat-fb-external-alias "${COMPAT_FB_EXTERNAL_ALIAS}"
    )

    if [[ -n "${PROFILE_DIR}" && -d "${PROFILE_DIR}" ]]; then
        env_out="${GENERATED_AUTH_ENV_FILE}"
    else
        env_out="$(mktemp)"
        cleanup_env="1"
    fi
    cmd+=(--env-out "${env_out}")

    if [[ -n "${PROFILE_DIR}" && -d "${PROFILE_DIR}" ]]; then
        cmd+=(
            --bootstrap-sql-out "${GENERATED_BOOTSTRAP_SQL_FILE}"
            --post-bootstrap-sql-out "${GENERATED_POST_BOOTSTRAP_SQL_FILE}"
        )
    fi

    "${cmd[@]}" || die "failed to materialize auth defaults from ${AUTH_MANIFEST}"
    [[ -f "${env_out}" ]] || die "auth defaults renderer did not create ${env_out}"
    # shellcheck disable=SC1090
    source "${env_out}"
    if [[ "${cleanup_env}" == "1" ]]; then
        rm -f "${env_out}"
    fi

    if [[ -z "${BOOTSTRAP_SQL}" && -n "${PROFILE_DIR}" && -d "${PROFILE_DIR}" ]]; then
        BOOTSTRAP_SQL="${GENERATED_BOOTSTRAP_SQL_FILE}"
    fi
    if [[ -z "${POST_BOOTSTRAP_SQL}" && -n "${PROFILE_DIR}" && -d "${PROFILE_DIR}" ]]; then
        POST_BOOTSTRAP_SQL="${GENERATED_POST_BOOTSTRAP_SQL_FILE}"
    fi
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
methods = ${AUTH_METHODS}
password_hash = ${AUTH_PASSWORD_HASH}
allow_superuser_remote = ${AUTH_ALLOW_SUPERUSER_REMOTE}

[storage]
tablespace_recovery_mode = allow_missing

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

wait_for_socket_path() {
    local path="$1"
    local attempts="${2:-120}"
    local sleep_sec="${3:-0.25}"
    local i
    for ((i = 0; i < attempts; i++)); do
        if [[ -S "${path}" ]]; then
            return 0
        fi
        sleep "${sleep_sec}"
    done
    return 1
}

run_native_probe() {
    local user="$1"
    local password="$2"

    "${ISQL_BIN}" "${MAIN_DB}" \
        --mode=local-ipc \
        --ipc-method=tcp \
        --sslmode="${NATIVE_SSLMODE}" \
        -H "${BIND_HOST}" \
        -p "${NATIVE_PORT}" \
        -U "${user}" \
        -P "${password}" \
        -c "SELECT 1;" \
        -q > /dev/null 2>&1
}

wait_for_native_probe() {
    local user="$1"
    local password="$2"
    local attempts="${3:-120}"
    local sleep_sec="${4:-0.25}"
    local i

    for ((i = 0; i < attempts; i++)); do
        if run_native_probe "${user}" "${password}"; then
            return 0
        fi
        sleep "${sleep_sec}"
    done

    return 1
}

wait_for_postgresql_probe() {
    [[ -x "${PG_ISQL_BIN}" ]] || return 0

    local attempts="${1:-120}"
    local sleep_sec="${2:-0.25}"
    local i

    for ((i = 0; i < attempts; i++)); do
        if PGPASSWORD="${PG_PASSWORD}" \
            "${PG_ISQL_BIN}" \
                -h "${BIND_HOST}" \
                -p "${PG_PORT}" \
                -U "${PG_USER}" \
                -d "${MAIN_DB}" \
                -c "SELECT 1;" \
                -q > /dev/null 2>&1; then
            return 0
        fi
        sleep "${sleep_sec}"
    done

    return 1
}

wait_for_mysql_probe() {
    [[ -x "${MY_ISQL_BIN}" ]] || return 0

    local attempts="${1:-120}"
    local sleep_sec="${2:-0.25}"
    local i

    for ((i = 0; i < attempts; i++)); do
        if [[ -n "${MYSQL_PASSWORD}" ]]; then
            if "${MY_ISQL_BIN}" \
                -h "${BIND_HOST}" \
                -P "${MYSQL_PORT}" \
                -u "${MYSQL_USER}" \
                "-p${MYSQL_PASSWORD}" \
                -e "SHOW VARIABLES LIKE 'version_comment';" \
                -q > /dev/null 2>&1; then
                return 0
            fi
        else
            if "${MY_ISQL_BIN}" \
                -h "${BIND_HOST}" \
                -P "${MYSQL_PORT}" \
                -u "${MYSQL_USER}" \
                -e "SHOW VARIABLES LIKE 'version_comment';" \
                -q > /dev/null 2>&1; then
                return 0
            fi
        fi
        sleep "${sleep_sec}"
    done

    return 1
}

wait_for_runtime_surfaces() {
    if ! wait_for_native_probe "${ADMIN_USER}" "${ADMIN_PASSWORD}" 120 0.25; then
        tail -n 100 "${SERVER_LOG}" >&2 || true
        die "native runtime surface did not become query-ready on ${BIND_HOST}:${NATIVE_PORT}"
    fi

    if ! wait_for_postgresql_probe 120 0.25; then
        tail -n 100 "${SERVER_LOG}" >&2 || true
        die "postgres runtime surface did not become query-ready on ${BIND_HOST}:${PG_PORT}"
    fi

    if ! wait_for_mysql_probe 120 0.25; then
        tail -n 100 "${SERVER_LOG}" >&2 || true
        die "mysql runtime surface did not become query-ready on ${BIND_HOST}:${MYSQL_PORT}"
    fi
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
        elif ! wait_for_socket_path "${CONTROL_DIR}/sb_engine.main.sock" 120 0.25; then
            startup_error="engine control socket did not come up at ${CONTROL_DIR}/sb_engine.main.sock"
        elif ! wait_for_socket_path "${CONTROL_DIR}/sb_listener.scratchbird.${NATIVE_PORT}.sock" 120 0.25; then
            startup_error="native listener control socket did not come up at ${CONTROL_DIR}/sb_listener.scratchbird.${NATIVE_PORT}.sock"
        elif ! wait_for_socket_path "${CONTROL_DIR}/sb_listener.postgresql.${PG_PORT}.sock" 120 0.25; then
            startup_error="postgres listener control socket did not come up at ${CONTROL_DIR}/sb_listener.postgresql.${PG_PORT}.sock"
        elif ! wait_for_socket_path "${CONTROL_DIR}/sb_listener.mysql.${MYSQL_PORT}.sock" 120 0.25; then
            startup_error="mysql listener control socket did not come up at ${CONTROL_DIR}/sb_listener.mysql.${MYSQL_PORT}.sock"
        elif ! wait_for_socket_path "${CONTROL_DIR}/sb_listener.firebird.${FB_PORT}.sock" 120 0.25; then
            startup_error="firebird listener control socket did not come up at ${CONTROL_DIR}/sb_listener.firebird.${FB_PORT}.sock"
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
        --sslmode="${NATIVE_SSLMODE}" \
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
}

run_post_bootstrap_seed() {
    [[ -x "${ISQL_BIN}" ]] || die "sb_isql binary missing: ${ISQL_BIN}"
    [[ -f "${POST_BOOTSTRAP_SQL}" ]] || die "post-bootstrap SQL not found: ${POST_BOOTSTRAP_SQL}"

    : > "${POST_BOOTSTRAP_LOG}"
    if ! "${ISQL_BIN}" "${MAIN_DB}" \
        --mode=local-ipc \
        --ipc-method=tcp \
        --sslmode="${NATIVE_SSLMODE}" \
        -H "${BIND_HOST}" \
        -p "${NATIVE_PORT}" \
        -U "${ADMIN_USER}" \
        -P "${ADMIN_PASSWORD}" \
        -b \
        -f "${POST_BOOTSTRAP_SQL}" \
        -o "${POST_BOOTSTRAP_LOG}" \
        -q; then
        tail -n 200 "${POST_BOOTSTRAP_LOG}" >&2 || true
        die "post-bootstrap SQL failed"
    fi
}

run_admin_sql() {
    local sql_text="$1"
    local suppress_error_output="${2:-0}"
    local err_file
    err_file="$(mktemp)"
    if ! "${ISQL_BIN}" "${MAIN_DB}" \
        --mode=local-ipc \
        --ipc-method=tcp \
        --sslmode="${NATIVE_SSLMODE}" \
        -H "${BIND_HOST}" \
        -p "${NATIVE_PORT}" \
        -U "${ADMIN_USER}" \
        -P "${ADMIN_PASSWORD}" \
        -c "${sql_text}" \
        -q > /dev/null 2> "${err_file}"; then
        if [[ "${suppress_error_output}" != "1" ]]; then
            cat "${err_file}" >&2 || true
        fi
        rm -f "${err_file}"
        return 1
    fi
    rm -f "${err_file}"
    return 0
}

ensure_seed_user() {
    local username="$1"
    local password="$2"
    local superuser="$3"
    local escaped_password="${password//\'/\'\'}"
    local create_sql="CREATE USER ${username} WITH PASSWORD '${escaped_password}'"
    local alter_sql="ALTER USER ${username} WITH PASSWORD '${escaped_password}'"
    if [[ "${superuser}" == "1" ]]; then
        create_sql="${create_sql} SUPERUSER"
        alter_sql="${alter_sql} SUPERUSER"
    fi
    create_sql="${create_sql};"
    alter_sql="${alter_sql};"

    if run_admin_sql "${create_sql}" 1; then
        return 0
    fi

    if run_admin_sql "${alter_sql}"; then
        return 0
    fi

    die "failed to provision user ${username}"
}

ensure_default_engine_users() {
    # Driver CLI file-mode parsing can silently skip later CREATE USER entries.
    # Execute idempotent per-user statements to make auth fixtures deterministic.
    ensure_seed_user "${ADMIN_USER}" "${ADMIN_PASSWORD}" 1
    ensure_seed_user "${PG_USER}" "${PG_PASSWORD}" 1
    ensure_seed_user "${MYSQL_USER}" "${MYSQL_PASSWORD}" 1
    ensure_seed_user "${FB_USER}" "${FB_PASSWORD}" 1
    ensure_seed_user "${COMPAT_NATIVE_USER}" "${COMPAT_NATIVE_PASSWORD}" 0
    ensure_seed_user "${COMPAT_PG_USER}" "${COMPAT_PG_PASSWORD}" 0
    ensure_seed_user "${COMPAT_MY_USER}" "${COMPAT_MY_PASSWORD}" 0
    ensure_seed_user "${COMPAT_FB_USER}" "${COMPAT_FB_PASSWORD}" 0
}

run_seed_pipeline() {
    run_bootstrap_seed
    run_post_bootstrap_seed
    ensure_default_engine_users
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
export SCRATCHBIRD_PG_PSQL_BIN='${PG_ISQL_BIN}'
export SCRATCHBIRD_MY_ISQL='${MY_ISQL_BIN}'
export SCRATCHBIRD_MYSQL_CLI_BIN='${MY_ISQL_BIN}'
export SCRATCHBIRD_FB_ISQL='${FB_ISQL_BIN}'
export SCRATCHBIRD_FB_NATIVE_ISQL='${FB_ISQL_BIN}'

export SCRATCHBIRD_EXAMPLE_COMPAT_CANONICAL_USER='${COMPAT_CANONICAL_USER}'
export SCRATCHBIRD_EXAMPLE_COMPAT_CANONICAL_USERID='${COMPAT_CANONICAL_USERID}'
export SCRATCHBIRD_EXAMPLE_COMPAT_NATIVE_USER='${COMPAT_NATIVE_USER}'
export SCRATCHBIRD_EXAMPLE_COMPAT_NATIVE_PASSWORD='${COMPAT_NATIVE_PASSWORD}'
export SCRATCHBIRD_EXAMPLE_COMPAT_NATIVE_AUTH_METHOD='password'
export SCRATCHBIRD_EXAMPLE_COMPAT_PG_USER='${COMPAT_PG_USER}'
export SCRATCHBIRD_EXAMPLE_COMPAT_PG_PASSWORD='${COMPAT_PG_PASSWORD}'
export SCRATCHBIRD_EXAMPLE_COMPAT_MY_USER='${COMPAT_MY_USER}'
export SCRATCHBIRD_EXAMPLE_COMPAT_MY_PASSWORD='${COMPAT_MY_PASSWORD}'
export SCRATCHBIRD_EXAMPLE_COMPAT_FB_USER='${COMPAT_FB_USER}'
export SCRATCHBIRD_EXAMPLE_COMPAT_FB_PASSWORD='${COMPAT_FB_PASSWORD}'
export SCRATCHBIRD_EXAMPLE_COMPAT_FB_EXTERNAL_ALIAS='${COMPAT_FB_EXTERNAL_ALIAS}'
export SCRATCHBIRD_EXAMPLE_COMPAT_PG_AUTH_METHOD='scram_sha_256'
export SCRATCHBIRD_EXAMPLE_COMPAT_MY_AUTH_METHOD='password'
export SCRATCHBIRD_EXAMPLE_COMPAT_FB_AUTH_METHOD='password'
export SCRATCHBIRD_EXAMPLE_COMPAT_NATIVE_PASSWORD_POLICY='native_v3_strict'
export SCRATCHBIRD_EXAMPLE_COMPAT_PG_PASSWORD_POLICY='pg_emulated_default'
export SCRATCHBIRD_EXAMPLE_COMPAT_MY_PASSWORD_POLICY='mysql_emulated_default'
export SCRATCHBIRD_EXAMPLE_COMPAT_FB_PASSWORD_POLICY='firebird_emulated_default'

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
  "compat_identity_profile": {
    "canonical_userid": "${COMPAT_CANONICAL_USERID}",
    "canonical_user": "${COMPAT_CANONICAL_USER}",
    "native": {
      "user": "${COMPAT_NATIVE_USER}",
      "password": "${COMPAT_NATIVE_PASSWORD}",
      "auth_method": "password",
      "password_policy": "native_v3_strict"
    },
    "postgresql": {
      "user": "${COMPAT_PG_USER}",
      "password": "${COMPAT_PG_PASSWORD}",
      "auth_method": "scram_sha_256",
      "password_policy": "pg_emulated_default"
    },
    "mysql": {
      "user": "${COMPAT_MY_USER}",
      "password": "${COMPAT_MY_PASSWORD}",
      "auth_method": "password",
      "password_policy": "mysql_emulated_default"
    },
    "firebird": {
      "user": "${COMPAT_FB_USER}",
      "external_alias": "${COMPAT_FB_EXTERNAL_ALIAS}",
      "password": "${COMPAT_FB_PASSWORD}",
      "auth_method": "password",
      "password_policy": "firebird_emulated_default"
    }
  },
  "clients": {
    "sb_isql": "${ISQL_BIN}",
    "postgresql_psql": "${PG_ISQL_BIN}",
    "mysql_cli": "${MY_ISQL_BIN}",
    "firebird_isql": "${FB_ISQL_BIN}"
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
    log "compat_identity=${COMPAT_CANONICAL_USERID}:${COMPAT_CANONICAL_USER} pg=${COMPAT_PG_USER} my=${COMPAT_MY_USER} fb=${COMPAT_FB_USER} (ext=${COMPAT_FB_EXTERNAL_ALIAS})"
    log "profiles=${PROFILE_DIR}"
}

dynamic_setup() {
    set_mode_paths dynamic
    resolve_binaries

    log "preparing dynamic example database at ${EXAMPLE_ROOT}"
    stop_server
    rm -rf "${EXAMPLE_ROOT}"
    prepare_layout
    load_auth_defaults
    write_token
    write_config
    start_server
    run_seed_pipeline
    write_connection_profiles
    wait_for_runtime_surfaces
    run_example_bundle_import
    if [[ "${DID_IMPORT_BUNDLE}" == "1" ]]; then
        log "restarting listeners after example bundle import"
        stop_server
        start_server
        write_connection_profiles
        wait_for_runtime_surfaces
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
    prepare_layout
    load_auth_defaults
    print_status
}

static_up() {
    set_mode_paths static
    resolve_binaries
    prepare_layout
    load_auth_defaults
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
        run_seed_pipeline
    fi
    write_connection_profiles
    wait_for_runtime_surfaces
    if [[ ! -f "${EXAMPLE_BUNDLE_MARKER}" ]]; then
        run_example_bundle_import
        if [[ "${DID_IMPORT_BUNDLE}" == "1" ]]; then
            log "restarting listeners after example bundle import"
            stop_server
            start_server
            write_connection_profiles
            wait_for_runtime_surfaces
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
    load_auth_defaults
    write_token
    write_config
    start_server
    run_seed_pipeline
    write_connection_profiles
    wait_for_runtime_surfaces
    run_example_bundle_import
    if [[ "${DID_IMPORT_BUNDLE}" == "1" ]]; then
        log "restarting listeners after example bundle import"
        stop_server
        start_server
        write_connection_profiles
        wait_for_runtime_surfaces
    fi
    print_status
}

static_status() {
    set_mode_paths static
    prepare_layout
    load_auth_defaults
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
