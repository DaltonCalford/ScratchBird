#!/usr/bin/env bash
set -euo pipefail

ASSUME_YES=0
REFRESH_PACKAGES=0
INSTALL_DOCKER=1

usage() {
  cat <<USAGE
Usage: $(basename "$0") [options]

Options:
  --yes               Non-interactive mode
  --refresh           Run apt-get upgrade before install
  --no-docker         Skip docker package installation
  --help              Show help
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --yes)
      ASSUME_YES=1
      shift
      ;;
    --refresh)
      REFRESH_PACKAGES=1
      shift
      ;;
    --no-docker)
      INSTALL_DOCKER=0
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if ! command -v apt-get >/dev/null 2>&1; then
  echo "Unsupported distro for automatic dependency install (apt-get not found)." >&2
  echo "Install dependencies manually, then rerun with --no-install-deps at bundle level." >&2
  exit 1
fi

SUDO=""
if [[ "${EUID}" -ne 0 ]]; then
  if ! command -v sudo >/dev/null 2>&1; then
    echo "This script requires root privileges (sudo not found)." >&2
    exit 1
  fi
  SUDO="sudo"
fi

if [[ "${ASSUME_YES}" -eq 0 ]]; then
  echo "This will install verification dependencies via apt-get."
  read -r -p "Continue? [y/N] " ans
  case "${ans}" in
    y|Y|yes|YES) ;;
    *)
      echo "Cancelled."
      exit 1
      ;;
  esac
fi

export DEBIAN_FRONTEND=noninteractive
${SUDO} apt-get update
if [[ "${REFRESH_PACKAGES}" -eq 1 ]]; then
  ${SUDO} apt-get upgrade -y
fi
${SUDO} apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  git \
  curl \
  wget \
  jq \
  rsync \
  ca-certificates \
  python3 \
  python3-pip \
  python3-venv \
  python3-yaml \
  postgresql-client \
  default-mysql-client \
  firebird3.0-utils \
  liblz4-dev \
  libzstd-dev \
  libgeos-dev \
  libproj-dev \
  libcrypt-dev \
  zip \
  unzip \
  xz-utils \
  netcat-openbsd \
  libssl-dev \
  zlib1g-dev \
  libreadline-dev \
  libncurses5-dev \
  libncursesw5-dev \
  libicu-dev \
  libxml2-dev \
  libxslt1-dev \
  bison \
  flex

if [[ "${INSTALL_DOCKER}" -eq 1 ]]; then
  ${SUDO} apt-get install -y docker.io
  ${SUDO} apt-get install -y docker-compose-plugin || ${SUDO} apt-get install -y docker-compose-v2 || true
  if command -v systemctl >/dev/null 2>&1; then
    ${SUDO} systemctl enable --now docker || true
  fi
fi

echo "Linux dependency bootstrap complete (APT)."
