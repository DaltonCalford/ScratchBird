# Front-Door Modes

Last updated: 2026-02-21

Mode selection controls how clients enter ScratchBird.

[Back to Configuration Index](index.md) | [Back to Documentation Index](../index.md)

---

## Mode Summary

| Mode | Topology | When to use |
|------|----------|-------------|
| `direct` | `client -> listener -> parser -> engine` | Lowest overhead, existing listener behavior |
| `manager_proxy` | `client -> sb_manager (MCP) -> internal native listener -> parser -> engine` | Centralized ingress policy and DB binding controls |

---

## Configuration Keys

### `[server]`

```ini
front_door_mode = direct
```

Allowed values:

- `direct`
- `manager_proxy`

### `[manager]` (used when `front_door_mode = manager_proxy`)

```ini
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

Optional:

```ini
dbbt_keyring = /etc/scratchbird/dbbt.keys
```

---

## Security Behavior

`manager_proxy` mode adds three ingress controls before byte proxy activation:

1. MCP control-plane authentication and database selection on `sb_manager`.
2. MCP authentication uses `auth_method=TOKEN` and validates against `manager.mcp_auth_secret`.
3. Listener preface validation (`LPREFACE`) with DB Binding Token (DBBT) checks:
   - token integrity
   - expiry window
   - replay rejection
   - listener binding match

In manager mode, internal native listeners are bound to loopback/UDS-only endpoints and are not exposed as public ingress listeners.

---

## Operational Notes

- Keep `front_door_mode=direct` as the default until manager-mode gates are validated in your environment.
- Use distinct ports for `manager.port` and `manager.internal_native_port`.
- Set `manager.mcp_auth_secret` to a unique deployment secret and rotate it with your normal secret lifecycle.
- Keep `require_proxy_binding=true` in manager mode.

---

## Validation

```bash
sb_server --config /etc/scratchbird/sb_server.conf --check
```

Run manager-focused tests:

```bash
cd build
ctest --output-on-failure -R "ManagerProxyMcpTest|ControlPlaneDbbtTest|AuthPolicyProtocolParityTest"
```
