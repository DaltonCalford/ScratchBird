# Section 29 Test Contract

## Required certification lanes

### Startup and config

- listener profile construction from `network` defaults
- listener profile replacement by explicit `listener.<name>` sections
- refusal when engine endpoint is missing
- refusal when pool bounds are invalid
- refusal when managed mode uses non-loopback bind
- refusal when direct mode also requires proxy binding

### Control-plane and management

- `HELLO` / `HELLO_ACK` worker registration
- `MANAGEMENT_COMMAND` / `MANAGEMENT_RESPONSE` framing
- command coverage:
  - `PING`
  - `STATUS`
  - `STOP graceful`
  - `STOP force`
  - `RELOAD`
  - `POOL SET`
  - `KILL`
  - `DBBT_VALIDATE`
  - `LPREFACE_VALIDATE`

### Pool behavior

- warm-pool startup gating to `pool_min`
- on-demand versus prefork or hybrid startup behavior
- rejection during drain
- rejection under queue saturation
- runtime pool resize behavior
- worker recycle after admin kill

### Managed-mode validation

- `DBBT` listener-id validation
- `DBBT` expiry and skew validation
- replay-cache enforcement
- `LPREFACE` field validation
- deterministic nack-code selection

### Parser/engine session edge

- engine attach requires user
- disabled or missing user fails attach
- current schema and search path are seeded from the catalog session
- autocommit-enabled attach still obeys the always-in-transaction rules from
  section `08`
- native READY returns session id and feature flags

### Observability

- metric registration for all section `29` listener and pool metrics
- status payload fields
- manager-consumed structured `STATUS` fields for parser-pool readiness and
  bounded remote-management posture defaults
- managed audit event emission for preface decisions

## Unsupported lanes

No section `29` certification is required for:
- live migration
- dual execution mirror
- replication runtime
- cluster or UDR fabric runtime
