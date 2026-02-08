# Component Responsibility Matrix

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Source:** `/docs/specifications/parser/v3/COMPONENT_MODEL_AND_RESPONSIBILITIES.md`  
**Scope:** Engine, Parser, Network Listener responsibilities

| Responsibility | Engine Core | Parser | Network Listener | Notes |
| --- | --- | --- | --- | --- |
| Accept connections and TLS | None | None | Primary | Listener owns accept/bind and TLS termination if configured. |
| Wire protocol decode/encode | None | Primary | None | Dialect-specific framing and message handling. |
| SQL dialect parsing | None | Primary | None | Produces SBLR from dialect SQL. |
| SBLR validation and execution | Primary | None | None | Engine is sole authority for SBLR. |
| Authentication decisions | Primary | Support | None | Parser extracts credentials; engine decides. |
| Authorization decisions | Primary | None | None | Engine enforces permissions. |
| Session lifecycle | Primary | Support | Support | Parser tracks per-connection state; listener manages parser pool. |
| Catalog virtualization | Primary | Support | None | Engine defines surfaces; parser emulates dialect views. |
| Shared SBLR cache | Primary | None | None | Global cache across sessions with security gating. |
| Per-session compile cache | None | Primary | None | SQL -> SBLR artifacts scoped to connection. |
| Storage, transactions, GC, maintenance | Primary | None | None | Firebird-style MGA; no WAL in core. |
| Scheduler (task planning) | Primary | None | None | Engine schedules GC and maintenance work. |
| Job system (execution workers) | Primary | None | None | Engine owns background job execution. |
| UDR connectors (local/remote) | Primary | None | None | Engine executes UDR connectors and enforces security. |
| Parser pool management | None | None | Primary | Spawn/assign/recycle parser workers. |
| Cluster manager (listener coordination) | Support | None | Primary | Listener-to-listener routing and admission control in cluster mode. |
| Stale connection/txn detection | Primary | Support | Support | Engine authoritative, parser/listener report. |
