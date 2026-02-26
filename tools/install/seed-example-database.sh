#!/usr/bin/env bash
set -euo pipefail

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    echo "error: seed-example-database.sh must run as root" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

SERVICE_USER="${1:-scratchbird}"
SERVICE_GROUP="${2:-scratchbird}"
STATE_DIR="${SCRATCHBIRD_STATE_DIR:-/var/lib/scratchbird}"
EXAMPLE_ROOT="${SCRATCHBIRD_EXAMPLE_INSTALL_ROOT:-${STATE_DIR}/example}"

if ! id -u "${SERVICE_USER}" >/dev/null 2>&1; then
    echo "error: service user does not exist: ${SERVICE_USER}" >&2
    exit 1
fi
if ! getent group "${SERVICE_GROUP}" >/dev/null 2>&1; then
    echo "error: service group does not exist: ${SERVICE_GROUP}" >&2
    exit 1
fi

install -d -m 0750 -o "${SERVICE_USER}" -g "${SERVICE_GROUP}" "${EXAMPLE_ROOT}"

export SCRATCHBIRD_EXAMPLE_STATIC_ROOT="${EXAMPLE_ROOT}"
export SCRATCHBIRD_EXAMPLE_RUN_AS_USER="${SERVICE_USER}"
export SCRATCHBIRD_EXAMPLE_RUN_AS_GROUP="${SERVICE_GROUP}"

"${REPO_ROOT}/scripts/example_db_manager.sh" static-refresh

chown -R "${SERVICE_USER}:${SERVICE_GROUP}" "${EXAMPLE_ROOT}"

echo "Seeded static example database at ${EXAMPLE_ROOT}"
echo "Connection profiles: ${EXAMPLE_ROOT}/profiles"
echo "Refresh command: ${REPO_ROOT}/scripts/example_db_manager.sh static-refresh"
