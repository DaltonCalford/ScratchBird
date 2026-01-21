# ScratchBird Component Model and Responsibilities

**Document Version:** 0.1  
**Date:** 2026-01-06  
**Status:** Draft (Alpha finalization)

## Purpose

This document consolidates the current component model (Engine, Parser, Network Listener) and the responsibilities/boundaries used for component diagrams, responsibility charts, and alpha/beta planning. It merges the original component description, clarification updates, and gap analysis notes.

## High-Level Topology

```
+---------------------------------------------------------------+
|                          CLIENTS                              |
|  PG/MySQL/Firebird/Native drivers and tools (untrusted input)  |
+---------------------------------------------------------------+
                               |
                               v
+---------------------------------------------------------------+
|                       NETWORK LISTENER                         |
|  - Accepts connections, TLS as configured                       |
|  - Assigns idle parser from pool                                |
|  - Hands off socket + metadata                                  |
+---------------------------------------------------------------+
                               |
                               v
+---------------------------------------------------------------+
|                        PARSER (1 per client)                    |
|  - Wire protocol handling                                       |
|  - SQL dialect -> SBLR                                           |
|  - Per-connection cache                                          |
+---------------------------------------------------------------+
                               | IPC / embedded
                               v
+---------------------------------------------------------------+
|                         ENGINE CORE                             |
|  - SBLR validation + execution (trusted authority)              |
|  - Shared cache, transactions, storage, catalog                 |
+---------------------------------------------------------------+
```

Reuse artifacts: [Component Model Diagrams](../../diagrams/component_model_diagrams.md) and
[Component Responsibility Matrix](../../diagrams/component_responsibility_matrix.md).

## Component Responsibilities

### Engine Core

- Multi-user, multi-tasking, multi-threading; supports concurrent client processes.
- Owns storage, transactions, concurrency, buffer/cache, optimization, and security.
- Executes and validates SBLR only (no SQL); verifies SBLR from all clients, including local IPC.
- Maintains a shared SBLR cache across all connections; reuse is gated by security checks.
- Manages catalog/metadata and dialect virtualization surfaces for emulation.
- Provides remote database connectors and local file connectors (UDR-backed).
- Monitors stale connections/transactions; runs GC, job scheduler tasks, and maintenance jobs (index rebuilds, sweeps).
- Durability model follows Firebird-style MGA; no write-after log (WAL, optional post-gold) in the engine core.
  - Optional post-gold write-after log (WAL) may exist for replication/PITR.
  - Beta temporal database is expected to supersede write-after log (WAL, optional post-gold) needs for recovery.

### Parser (Dialect Adapter)

- One dialect per parser (ScratchBird, PostgreSQL, MySQL, Firebird, others).
- Runs embedded (library) or as a separate process via IPC (inet fallback if needed).
- Handles wire protocol parsing/encoding, authentication handshake extraction, error mapping, and type mapping.
- Translates dialect SQL into SBLR; maintains per-connection session state.
- Caches SBLR/engine API artifacts per connection to avoid regeneration; no cross-connection state.
- Emulates catalog/schema behavior so clients believe the emulated database is real.
- MUST NOT expose ScratchBird-only features not supported by the emulated dialect.

### Network Listener

- Accepts incoming connections on protocol-specific ports (replacement for the original Y-Valve concept).
- Maintains a pool of pre-started, idle parsers per protocol; assigns a parser on connect.
- Spawns/replaces parser workers when pool is exhausted or unhealthy.
- Hands off socket and connection metadata to the parser; not on the hot path after handoff.
- Coordinates with other listeners in cluster mode for routing/admission control.
- Uses its own configuration file for pool sizing, limits, and protocol bindings.

## Responsibility Matrix

This section mirrors the reusable matrix in
`docs/diagrams/component_responsibility_matrix.md`.

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
| Storage, transactions, GC, maintenance | Primary | None | None | Firebird-style MGA; no write-after log (WAL, optional post-gold) in core. |
| Scheduler (task planning) | Primary | None | None | Engine schedules GC and maintenance work. |
| Job system (execution workers) | Primary | None | None | Engine owns background job execution. |
| UDR connectors (local/remote) | Primary | None | None | Engine executes UDR connectors and enforces security. |
| Parser pool management | None | None | Primary | Spawn/assign/recycle parser workers. |
| Cluster manager (listener coordination) | Support | None | Primary | Listener-to-listener routing and admission control in cluster mode. |
| Stale connection/txn detection | Primary | Support | Support | Engine authoritative, parser/listener report. |

## Diagram (Mermaid)

```mermaid
flowchart LR
  Clients[Clients] --> Listener[Network Listener]
  Listener -->|socket handoff| Parser[Parser (per connection)]
  Parser -->|IPC or embedded| Engine[Engine Core]
  Engine --> Catalog[(Catalog/Metadata)]
  Engine --> Storage[(Storage/Transactions)]
  Engine --> Cache[(Shared SBLR Cache)]
  Parser --> SessionCache[(Per-session Cache)]
```

## Diagram (ASCII)

```
Clients
  |
  v
Network Listener --(socket handoff)--> Parser (per connection)
                                          |
                                          v
                                    Engine Core
                                      |   |   |
                                      |   |   +--> Shared SBLR Cache
                                      |   +------> Catalog/Metadata
                                      +----------> Storage/Transactions
```

## Trust Boundaries

- All clients are untrusted, including local IPC clients.
- Parser and listener are constrained; they never authorize or execute database state changes.
- Engine is the sole authority for authentication, authorization, catalog integrity, and SBLR validation.

## Caching and Reuse Model

- Engine cache: shared SBLR cache keyed by normalized SBLR/query fingerprint; reuse requires security/context validation.
- Parser cache: per-session compile artifacts (SQL -> SBLR, statement handles); cleared on session end.
- Cache invalidation: schema changes and privilege changes must invalidate both parser and engine caches.

## Outstanding Decisions and Work Items

- Finalize the engine API surface (SBLR submission, metadata RPCs, error/notice formats).
- Define the SBLR cache keying and invalidation rules, including security gating.
- Specify metadata virtualization interfaces for emulated dialects (catalog views, system tables, error codes).
- Complete transaction/concurrency spec (stale txn detection, savepoints, GC scheduling).
- Define durability/replication/PITR approach given no write-after log (WAL, optional post-gold) in core and future temporal DB.
- Formalize listener <-> parser handoff protocol and pool management for cluster mode.
- Decide whether to split "protocol adapter" from "SQL parser" for shared transport logic.
- Observability: unified logging/metrics across engine, parser, and listener.
