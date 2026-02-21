#!/usr/bin/env bash
set -euo pipefail

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    echo "error: ensure-service-account.sh must run as root" >&2
    exit 1
fi

SERVICE_USER="${1:-scratchbird}"
SERVICE_GROUP="${2:-scratchbird}"
STATE_DIR="${SCRATCHBIRD_STATE_DIR:-/var/lib/scratchbird}"
LOG_DIR="${SCRATCHBIRD_LOG_DIR:-/var/log/scratchbird}"
RUN_DIR="${SCRATCHBIRD_RUN_DIR:-/var/run/scratchbird}"
BOOTSTRAP_TOKEN_FILE="${SCRATCHBIRD_BOOTSTRAP_TOKEN_FILE:-${STATE_DIR}/bootstrap.token}"

if ! getent group "${SERVICE_GROUP}" >/dev/null 2>&1; then
    groupadd --system "${SERVICE_GROUP}"
fi

if ! id -u "${SERVICE_USER}" >/dev/null 2>&1; then
    useradd \
        --system \
        --gid "${SERVICE_GROUP}" \
        --home-dir "${STATE_DIR}" \
        --create-home \
        --shell /usr/sbin/nologin \
        "${SERVICE_USER}"
fi

install -d -m 0750 -o "${SERVICE_USER}" -g "${SERVICE_GROUP}" \
    "${STATE_DIR}" "${LOG_DIR}" "${RUN_DIR}"

if [[ ! -f "${BOOTSTRAP_TOKEN_FILE}" ]]; then
    umask 0077
    if command -v openssl >/dev/null 2>&1; then
        token="$(openssl rand -hex 32)"
    else
        token="$(od -vAn -N32 -tx1 /dev/urandom | tr -d ' \n')"
    fi
    printf '%s\n' "${token}" > "${BOOTSTRAP_TOKEN_FILE}"
    chown "${SERVICE_USER}:${SERVICE_GROUP}" "${BOOTSTRAP_TOKEN_FILE}"
    chmod 0600 "${BOOTSTRAP_TOKEN_FILE}"
fi

echo "Configured service account ${SERVICE_USER}:${SERVICE_GROUP}"
echo "State dir: ${STATE_DIR}"
echo "Log dir: ${LOG_DIR}"
echo "Run dir: ${RUN_DIR}"
echo "Bootstrap token file: ${BOOTSTRAP_TOKEN_FILE}"
