#!/usr/bin/env bash
set -euo pipefail

REPO_PATH="${1:-}"
if [[ -z "${REPO_PATH}" ]]; then
  echo "Usage: $(basename "$0") <scratchbird_repo_path>" >&2
  exit 1
fi

if [[ ! -d "${REPO_PATH}/.git" ]]; then
  echo "ScratchBird repository not found at ${REPO_PATH}" >&2
  exit 1
fi

cd "${REPO_PATH}"
echo "[refresh] repository: ${REPO_PATH}"
git fetch --all --tags --prune

if [[ -n "$(git status --porcelain)" ]]; then
  echo "[refresh] local modifications detected; skipping pull to avoid overriding local changes."
  git status --short
  exit 0
fi

BRANCH="$(git symbolic-ref --short -q HEAD || true)"
if [[ -z "${BRANCH}" ]]; then
  echo "[refresh] detached HEAD; skipping pull."
  git rev-parse HEAD
  exit 0
fi

git pull --ff-only --tags
echo "[refresh] updated commit: $(git rev-parse HEAD)"
