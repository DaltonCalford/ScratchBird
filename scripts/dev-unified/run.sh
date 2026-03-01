#!/usr/bin/env bash
if [ -z "${BASH_VERSION:-}" ]; then
  exec bash "$0" "$@"
fi

set -euo pipefail

IMAGE_NAME="scratchbird-unified-builder"
IMAGE_TAG="latest"
HOST_DIR=""
WORKSPACE_REL=""
HOST_IP="127.0.0.1"
HOST_PORT="2222"
CONTAINER_NAME="cliwork-builder"
SSH_USER="builder"
AUTO_BUILD=""
AUTO_BUILD_PROJECT="auto"
SSH_KEY_FILE=""
RUN_FOREGROUND=""

usage() {
  cat <<'EOF'
Usage: run.sh [options]

Options:
  --host-dir <path>         Host directory that contains project repos (default: current dir)
  --workspace <path>        Relative path under --host-dir, or absolute project/workspace path
  --path <path>             Alias for --workspace
  --ip <ip>                 Local bind IP for SSH (default: 127.0.0.1)
  --port <port>             Host SSH port (default: 2222)
  --name <container name>    Container name (default: cliwork-builder)
  --image <name>            Image name (default: scratchbird-unified-builder)
  --user <linux username>    SSH user inside container (default: builder)
  --ssh-key <pubkey file>   Public key file to inject into container
  --build [project]         Run build-matrix at startup (project optional, default: auto)
  --foreground              Keep container attached to terminal
  --help                    Show this help

Examples:
  ./run.sh --workspace /home/me/CliWork/ScratchBird --ip 192.168.1.20
  ./run.sh --workspace ScratchBird-driver --build ScratchBird-driver
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --host-dir)
      HOST_DIR="$2"
      shift 2
      ;;
    --workspace|--path)
      WORKSPACE_REL="$2"
      shift 2
      ;;
    --ip)
      HOST_IP="$2"
      shift 2
      ;;
    --port)
      HOST_PORT="$2"
      shift 2
      ;;
    --name)
      CONTAINER_NAME="$2"
      shift 2
      ;;
    --image)
      IMAGE_NAME="$2"
      shift 2
      ;;
    --user)
      SSH_USER="$2"
      shift 2
      ;;
    --ssh-key)
      SSH_KEY_FILE="$2"
      shift 2
      ;;
    --build)
      AUTO_BUILD="1"
      if [ "${2:-}" ] && [[ "${2:-}" != --* ]]; then
        AUTO_BUILD_PROJECT="$2"
        shift 2
      else
        shift
      fi
      ;;
    --foreground)
      RUN_FOREGROUND="1"
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      usage
      exit 1
      ;;
  esac
done

IMAGE="${IMAGE_NAME}:${IMAGE_TAG}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCKERFILE_PATH="${SCRIPT_DIR}/Dockerfile"

if [ -z "$HOST_DIR" ]; then
  HOST_DIR="$(pwd)"
fi
HOST_DIR="$(realpath "$HOST_DIR")"

if [ -n "$WORKSPACE_REL" ] && [[ "$WORKSPACE_REL" == /* ]]; then
  WORKSPACE_ABS="$(realpath "$WORKSPACE_REL")"
  if [ ! -d "$WORKSPACE_ABS" ]; then
    echo "Workspace path missing: $WORKSPACE_ABS"
    exit 1
  fi
  if [[ "$WORKSPACE_ABS" != "$HOST_DIR"/* ]]; then
    HOST_DIR="$(dirname "$WORKSPACE_ABS")"
    echo "INFO: Host directory set from absolute workspace path: $HOST_DIR"
  fi
  WORKSPACE_IN_CONTAINER="/workspace${WORKSPACE_ABS#"$HOST_DIR"}"
else
  if [ -n "$WORKSPACE_REL" ]; then
    WORKSPACE_IN_CONTAINER="/workspace/${WORKSPACE_REL#/}"
  else
    WORKSPACE_IN_CONTAINER="/workspace"
  fi
fi

if [ ! -d "$HOST_DIR" ]; then
  echo "Host directory missing: $HOST_DIR"
  exit 1
fi

RUN_MODE_ARGS=("--detach")
if [ -n "$RUN_FOREGROUND" ]; then
  RUN_MODE_ARGS=("-it")
fi

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
  echo "Building image $IMAGE from $DOCKERFILE_PATH"
  docker build -t "$IMAGE" -f "$DOCKERFILE_PATH" "$SCRIPT_DIR"
fi

DOCKER_ARGS=(
  --name "$CONTAINER_NAME"
  "${RUN_MODE_ARGS[@]}"
  --publish "${HOST_IP}:${HOST_PORT}:22"
  -v "${HOST_DIR}:/workspace"
  -e WORKDIR_PATH="$WORKSPACE_IN_CONTAINER"
  -e LISTEN_IP="0.0.0.0"
  -e SSH_PORT="22"
  -e SSH_USER="$SSH_USER"
  "$IMAGE"
)

if [ -n "$AUTO_BUILD" ]; then
  DOCKER_ARGS+=(
    -e AUTO_BUILD="1"
    -e AUTO_BUILD_PROJECT="$AUTO_BUILD_PROJECT"
  )
fi

if [ -n "$SSH_KEY_FILE" ]; then
  if [ ! -f "$SSH_KEY_FILE" ]; then
    echo "SSH key file missing: $SSH_KEY_FILE"
    exit 1
  fi
  DOCKER_ARGS+=(
    -v "${SSH_KEY_FILE}:/tmp/authorized_key:ro"
    -e SSH_PUBLIC_KEY_FILE="/tmp/authorized_key"
  )
fi

echo "Starting container:"
echo "  image:      $IMAGE"
echo "  host dir:   $HOST_DIR"
echo "  workdir:    $WORKSPACE_IN_CONTAINER"
echo "  ssh:        ssh ${SSH_USER}@${HOST_IP} -p ${HOST_PORT}"

if [ -n "$RUN_FOREGROUND" ]; then
  docker run "${DOCKER_ARGS[@]}"
else
  CONTAINER_ID="$(docker run "${DOCKER_ARGS[@]}" | tr -d '\n')"
  if [ -z "$CONTAINER_ID" ]; then
    echo "Failed to start container"
    exit 1
  fi
  echo "Container running: $CONTAINER_ID"
fi
