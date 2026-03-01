#!/usr/bin/env bash
set -euo pipefail

WORKDIR_PATH="${WORKDIR_PATH:-/workspace}"
SSH_USER="${SSH_USER:-builder}"
SSH_GROUP="${SSH_GROUP:-$SSH_USER}"
SSH_PUBLIC_KEY="${SSH_PUBLIC_KEY:-}"
SSH_PUBLIC_KEY_FILE="${SSH_PUBLIC_KEY_FILE:-}"
SSH_PASSWORD="${SSH_PASSWORD:-}"
LISTEN_IP="${LISTEN_IP:-0.0.0.0}"
SSH_PORT="${SSH_PORT:-22}"
AUTO_BUILD="${AUTO_BUILD:-}"
AUTO_BUILD_PROJECT="${AUTO_BUILD_PROJECT:-}"

SSHD_CONF_DIR="/etc/ssh/sshd_config.d"
SSHD_CONF_FILE="$SSHD_CONF_DIR/dev-container.conf"

if ! id -u "$SSH_USER" >/dev/null 2>&1; then
  groupadd -r "$SSH_GROUP" 2>/dev/null || true
  useradd --create-home --shell /bin/bash --gid "$SSH_GROUP" "$SSH_USER" 2>/dev/null || useradd --create-home --shell /bin/bash "$SSH_USER"
fi

mkdir -p "/home/$SSH_USER/.ssh"
chown -R "$SSH_USER:$SSH_USER" "/home/$SSH_USER/.ssh"
chmod 700 "/home/$SSH_USER/.ssh"

if [ -n "$SSH_PUBLIC_KEY" ]; then
  echo "$SSH_PUBLIC_KEY" > "/home/$SSH_USER/.ssh/authorized_keys"
fi

if [ -n "$SSH_PUBLIC_KEY_FILE" ] && [ -f "$SSH_PUBLIC_KEY_FILE" ]; then
  cat "$SSH_PUBLIC_KEY_FILE" >> "/home/$SSH_USER/.ssh/authorized_keys"
fi

chmod 600 "/home/$SSH_USER/.ssh/authorized_keys" 2>/dev/null || true

if [ ! -s "/home/$SSH_USER/.ssh/authorized_keys" ]; then
  if [ -z "$SSH_PASSWORD" ]; then
    SSH_PASSWORD="$(openssl rand -base64 24)"
    echo "warning: no SSH key provided; generated temporary password for ${SSH_USER}: ${SSH_PASSWORD}"
  fi
  echo "$SSH_USER:$SSH_PASSWORD" | chpasswd
else
  passwd -d "$SSH_USER" >/dev/null 2>&1 || true
fi

usermod -aG sudo "$SSH_USER"
echo "${SSH_USER} ALL=(ALL) NOPASSWD:ALL" > "/etc/sudoers.d/$SSH_USER"
chmod 0440 "/etc/sudoers.d/$SSH_USER"

mkdir -p "$SSHD_CONF_DIR"
cat > "$SSHD_CONF_FILE" <<EOF
Port ${SSH_PORT}
ListenAddress ${LISTEN_IP}
PasswordAuthentication yes
PermitRootLogin no
ChallengeResponseAuthentication no
UsePAM no
AllowUsers ${SSH_USER}
EOF
chmod 600 "$SSHD_CONF_FILE"

ssh-keygen -A

mkdir -p "$WORKDIR_PATH"
cd "$WORKDIR_PATH"

if [ -n "$AUTO_BUILD" ]; then
  /usr/local/bin/build-matrix.sh "$WORKDIR_PATH" "${AUTO_BUILD_PROJECT:-auto}" || true
fi

exec /usr/sbin/sshd -D -e -f /etc/ssh/sshd_config -o PidFile=/run/sshd/sshd.pid
