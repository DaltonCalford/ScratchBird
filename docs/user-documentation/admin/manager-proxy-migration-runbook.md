# Direct To Manager Proxy Migration Runbook

Last updated: 2026-02-21

[Back to Administration Index](index.md) | [Back to Documentation Index](../index.md)

---

## Scope

This runbook migrates a deployment from:

- `front_door_mode = direct`

to:

- `front_door_mode = manager_proxy`

with a rollback path.

---

## Preconditions

1. `sb_manager` binary is installed and executable.
2. Internal native listener owner database exists (`manager.owner_database`).
3. Manager and internal native ports are different.
4. `manager.mcp_auth_secret` is set to a deployment secret.
5. DBBT keyring is configured for production environments.

---

## Step 1: Capture Current State

```bash
cp /etc/scratchbird/sb_server.conf /etc/scratchbird/sb_server.conf.pre-manager-proxy
```

```bash
sb_server --config /etc/scratchbird/sb_server.conf --check
```

---

## Step 2: Apply Manager Proxy Settings

Update `sb_server.conf`:

```ini
[server]
front_door_mode = manager_proxy

[manager]
bind_address = 0.0.0.0
port = 3090
internal_native_bind = 127.0.0.1
internal_native_port = 3392
owner_database = main
binary = sb_manager
mcp_auth_secret = <high-entropy-secret>
listener_id = 1
dbbt_ttl_ms = 30000
dbbt_clock_skew_ms = 2000
dbbt_replay_cache_size = 4096
require_proxy_binding = true
```

Validate:

```bash
sb_server --config /etc/scratchbird/sb_server.conf --check
```

---

## Step 3: Restart And Verify

```bash
sudo systemctl restart scratchbird
sudo systemctl status scratchbird
```

Verify expected process and sockets:

1. `sb_manager` is running.
2. public ingress is exposed on `manager.port`.
3. internal native listener is loopback/UDS-only.

---

## Step 4: Functional Verification

1. MCP auth succeeds.
2. `MCP_DB_CONNECT` succeeds for intended databases.
3. Native auth/query flow succeeds after proxy transition.

Recommended test command:

```bash
cd build
ctest --output-on-failure -R "ManagerProxyMcpTest\\.DbConnectTransitionsToByteProxyAndRelaysNativeAuthAndQueryFrames"
```

---

## Rollback

If migration fails, restore the previous config and restart:

```bash
cp /etc/scratchbird/sb_server.conf.pre-manager-proxy /etc/scratchbird/sb_server.conf
sb_server --config /etc/scratchbird/sb_server.conf --check
sudo systemctl restart scratchbird
```

Rollback validation:

1. `front_door_mode = direct` is active.
2. direct listeners accept connections.
3. manager process is not required for ingress.
