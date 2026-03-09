<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# System Topology

[Prev](./README.md) | [Next](./02_embedded_engine_model.md) | [Topic README](./README.md) | [Developers Guide README](../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/server/scratchbird_server.cpp:1

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/core/storage_engine.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_storage_engine.cpp:1
- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_ipc_server.cpp:1

## Synopsis

ScratchBird operates as a distributed system of cooperating processes with clear boundaries between client-facing components, protocol translation, and storage execution. This document describes the end-to-end topology from client entry to engine execution.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         SCRATCHBIRD SYSTEM                                   │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │  CLIENT LAYER                                                        │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                  │   │
│  │  │ Native App  │  │  PG Client  │  │ MySQL Client│  ...             │   │
│  │  │ (SBWP)      │  │ (Wire Prot) │  │ (Wire Prot) │                  │   │
│  │  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘                  │   │
│  └─────────┼────────────────┼────────────────┼──────────────────────────┘   │
│            │                │                │                               │
│            │ TCP/Unix       │ TCP 5432       │ TCP 3306                      │
│            │ Port 3092      │                │                               │
│            │                │                │                               │
│  ┌─────────┼────────────────┼────────────────┼──────────────────────────┐   │
│  │  PARSER LAYER (sb_listener_*)                                        │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                  │   │
│  │  │ sb_listener │  │ sb_listener │  │ sb_listener │                  │   │
│  │  │ _native     │  │ _pg         │  │ _mysql      │                  │   │
│  │  │             │  │             │  │             │                  │   │
│  │  │ • SBWP v1.1 │  │ • PG Proto  │  │ • MySQL Prot│                  │   │
│  │  │ • Native SQL│  │ • PG SQL    │  │ • MySQL SQL │                  │   │
│  │  │ • UUID Maps │  │ • PG→SBLR   │  │ • MySQL→SBLR│                  │   │
│  │  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘                  │   │
│  └─────────┼────────────────┼────────────────┼──────────────────────────┘   │
│            │                │                │                               │
│            └────────────────┴────────────────┘                               │
│                         │                                                    │
│                         │ Hardened IPC (Engine→Listener only)                │
│                         │ AES-256-GCM + Mutual Auth                          │
│                         │                                                    │
│  ┌──────────────────────┼───────────────────────────────────────────────┐   │
│  │  ENGINE LAYER         │                                               │   │
│  │  ┌──────────────────┐ │                                               │   │
│  │  │  sb_server       │ │                                               │   │
│  │  │  (Main Process)  │ │                                               │   │
│  │  │                  │ │                                               │   │
│  │  │ ┌──────────────┐ │ │  ┌──────────────┐  ┌──────────────┐          │   │
│  │  │ │ SBLR Parser  │◄┼─┘  │ SBLR Optimizer│  │ SBLR Executor│          │   │
│  │  │ │ (UDR Side)   │ │    │ (Cost-based)  │  │ (MGA Storage)│          │   │
│  │  │ └──────────────┘ │    └──────────────┘  └──────────────┘          │   │
│  │  │         │        │            │                │                    │   │
│  │  │    ┌────┴────┐   │      ┌────┴────┐      ┌────┴────┐               │   │
│  │  │    │  LLVM   │   │      │ Stats   │      │ MGA     │               │   │
│  │  │    │  JIT    │   │      │ Cache   │      │ Engine  │               │   │
│  │  │    └─────────┘   │      └─────────┘      └────┬────┘               │   │
│  │  └──────────────────┘                            │                      │   │
│  │                                                  │                      │   │
│  │  ┌───────────────────────────────────────────────┘                      │   │
│  │  │  STORAGE LAYER                                                          │   │
│  │  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                 │   │
│  │  │  │ Catalog Pages│  │ Table Data   │  │ Index Pages  │                 │   │
│  │  │  │ (System)     │  │ (MGA)        │  │ (11 types)   │                 │   │
│  │  │  └──────────────┘  └──────────────┘  └──────────────┘                 │   │
│  │  └─────────────────────────────────────────────────────────────────────┘   │
│  └─────────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Component Boundaries

### 1. Client Layer

| Component | Protocol | Port | Purpose |
|-----------|----------|------|---------|
| Native SB Clients | SBWP v1.1 | 3092 | Native wire protocol |
| PostgreSQL Clients | PostgreSQL Protocol | 5432 | Emulation compatibility |
| MySQL Clients | MySQL Protocol | 3306 | Emulation compatibility |
| Firebird Clients | Firebird Protocol | 3050 | Emulation compatibility |

**Key Point:** Clients use native database drivers, unchanged. No client-side modification required.

### 2. Parser Layer (Listeners)

The parser layer has two sides:

**User-Facing Side:**
- Wire protocol handler (PostgreSQL, MySQL, Firebird protocols)
- Authentication using environment-scoped policies
- Dialect-specific SQL parsing
- Sandboxing for emulated databases

**UDR Side (Engine-Facing):**
- SQL → SBLR bytecode translation
- Name resolution (client names → UUIDs)
- Type mapping (dialect types → SB native)
- Dynamic SQL compilation for UDRs

**Security Model:**
- Listener NEVER initiates communication to engine
- Only engine → listener via hardened IPC
- Mutual TLS authentication
- Command signing (HMAC-SHA256)

### 3. Engine Layer (sb_server)

**SBLR Parser (UDR):**
- Converts SQL to SBLR bytecode
- Used for dynamic SQL in UDRs
- Shares parser infrastructure with user-facing side

**SBLR Optimizer:**
- Cost-based query optimization
- Statistics-driven plan selection
- Index selection (11 core types)
- Join ordering

**SBLR Executor:**
- SBLR VM execution
- Hot path detection
- LLVM JIT compilation (optional)
- MGA transaction coordination

**LLVM JIT:**
- Compiles hot SBLR to native code
- Supported: x86_64, ARM64, RISC-V
- Platform vectorization (AVX2, NEON, SVE)
- Code cache management

### 4. Storage Layer (MGA)

**Multi-Generational Architecture:**
- Everything is an INSERT
- UPDATE = new version + pointer
- DELETE = tombstone marker
- COMMIT = flag switch on transaction record

**Page Types:**
- Catalog pages (system tables)
- Table data pages (row versions)
- Index pages (11 core types)
- TOAST pages (oversized data)
- BLOB pages (binary large objects)

**Index Types:**
1. BTREE - Balanced tree
2. HASH - Hash index
3. RTREE - Spatial (2D/3D)
4. FULLTEXT - Text search
5. BITMAP - Low cardinality
6. GIN - Generalized Inverted
7. GIST - Generalized Search Tree
8. VECTOR - Approximate nearest neighbor
9. LSM - Log-Structured Merge
10. COLUMNAR - Analytics
11. COMPOUND - Multi-column

## Process Flow

### Query Execution Flow

```
1. Client Connection
   └─> Listener accepts connection on protocol-specific port

2. Authentication
   └─> Environment lookup (!:env.emulated_pg.mydb)
   └─> Master UUID resolution
   └─> Policy-based authentication (password, cert, token, etc.)
   └─> Sandboxing context established

3. Query Reception
   └─> Client sends SQL (dialect-specific)
   └─> Parser validates syntax
   └─> Auto-qualification for sandbox (emulated)

4. SQL → SBLR Translation
   └─> Name resolution (client names → UUIDs)
   └─> Type mapping (dialect → SB native)
   └─> SBLR bytecode generation

5. Engine Execution
   └─> SBLR received by executor
   └─> Optimization (plan selection)
   └─> Optional: LLVM JIT compilation
   └─> MGA storage operations

6. Result Return
   └─> SBLR results → dialect format
   └─> UUIDs → client names
   └─> Wire protocol encoding
   └─> Client receives result
```

### Transaction Flow

```
BEGIN;
  ├─> Transaction ID assigned
  ├─> Snapshot taken (visibility baseline)
  ├─> Operations append new versions
  ├─> Indexes point to all versions
COMMIT;
  ├─> Commit flag set on transaction record
  ├─> Changes visible to new snapshots
  ├─> Old versions remain (MGA history)
ROLLBACK;
  └─> Transaction marked aborted
  └─> Versions ignored by all snapshots
```

## Trust Boundaries

| Boundary | Mechanism | Direction |
|----------|-----------|-----------|
| Client → Listener | TLS 1.3 | Bidirectional |
| Listener → Engine | Hardened IPC + Mutual Auth | Engine → Listener only |
| Engine → Storage | In-process | Internal |
| Cluster Nodes | T-of-N Consensus | Distributed |

## Deployment Topologies

### Embedded Mode
```
Application ──linked──► Engine (no IPC, no network)
```

### Embedded with Parser
```
Application ──linked──► Engine + Parser (UDR only)
```

### IPC Server Mode
```
Multiple Clients ──IPC──► Listener Pool ──IPC──► Engine
         (unix sockets, shared memory)
```

### Network Server Mode
```
Remote Clients ──TCP──► Listener Pool ──IPC──► Engine
```

### Cluster Mode
```
Clients ──TCP──► Listener ──IPC──► Engine Node ──Raft──► Other Nodes
                                          │
                                    T-of-N Security
```

## Failure Domains

| Component | Failure Impact | Recovery |
|-----------|---------------|----------|
| Listener | Client disconnections | Auto-restart by engine |
| Parser (UDR) | Query translation fails | Error to client, retry |
| Optimizer | Suboptimal plans | Query hints, stats update |
| Executor | Transaction abort | Automatic rollback |
| Storage | Data unavailable | Replication failover |

## Startup Sequence

```
1. Engine initializes
   └─> Catalog pages loaded
   └─> UUID registry initialized
   └─> MGA sweep coordinator started

2. Listeners spawned
   └─> sb_listener_native (port 3092)
   └─> sb_listener_pg (port 5432)
   └─> sb_listener_mysql (port 3306)
   └─> sb_listener_fb (port 3050)

3. Listeners register with engine
   └─> Hardened IPC established
   └─> Capability advertisement
   └─> Health check protocol

4. Engine accepts connections
   └─> Client connections routed to listeners
   └─> Query processing begins
```

## Key Design Principles

1. **Listener Security:** Never initiate communication (prevents compromised listener attacks)
2. **UUID Identity:** All objects identified by UUIDs; names resolved at boundaries
3. **Bytecode Equivalence:** All dialects compile to SBLR; identical execution path
4. **MGA Consistency:** Database is its own log; point-in-time recovery built-in
5. **Distributed Security:** No single node holds complete security info (cluster mode)

## See Also

- [Parser Topology](03_parser_topology.md)
- [Listener Topology](04_listener_topology.md)
- [Group and Cluster Trust Models](09_group_and_cluster_trust_models.md)
- [Transactions and MGA](../transactions_and_mga/README.md)
- [Emulation and Protocol](../emulation_and_protocol/README.md)
