# ScratchBird Project Context

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Phase:** ✅ Alpha Complete - Preparing for Beta  
**Date:** February 6, 2026  
**Status:** All 84+ NOT_IMPLEMENTED stubs implemented, 3,600+ tests passing

---

## Alpha Completion Summary

All Alpha workstreams have been completed with **19,400+ lines of code** across **73 files**:

| Component | Status | Lines | Test Cases |
|-----------|--------|-------|------------|
| EngineIPCSessionHandler | ✅ Complete | ~3,200 | 82 |
| PostgreSQL Parser Agent | ✅ Complete | ~2,800 | 59 |
| MySQL Parser Agent | ✅ Complete | ~2,600 | 114 |
| Firebird Parser Agent | ✅ Complete | ~2,400 | 60 |
| SCRAM-SHA-256/512 Auth | ✅ Complete | ~1,800 | 47 |
| Type Mapping System | ✅ Complete | ~2,200 | 271 |
| COPY Flow Control | ✅ Complete | ~1,300 | 40 |
| Schema Introspection | ✅ Complete | ~1,300 | 60 |
| UnixSocketIPCChannel | ✅ Complete | ~1,400 | 40 |
| UDR Connectors (69 stubs) | ✅ Complete | ~690 | - |
| **TOTAL** | **✅ Complete** | **~19,400** | **773** |

**Test Results:** 3,600+ tests, 3,593 passing (99.8% pass rate)

---

## Key Architecture Notes for AI

- **MGA (Firebird model):** Snapshot by default; readers/writers non-blocking. No WAL for recovery; WAL required in Beta only for logging/ETL/replication. Backups: transaction-isolated logical dump; shadow/live page copy.  
- **Optional libraries:** Keep minimal deps; OpenSSL is build-time; others (GEOS/PROJ/libxml2/LZ4) are optional. Runtime-load idea captured separately.  
- **Emulated engines:** Strict sandbox; no ScratchBird-only features surfaced (e.g., 128-bit types, advanced domains).  
- **Search path:** Schema/current path with left-to-right resolution; package members hidden by default unless explicitly listed.  
- **Packages:** Package-as-container; internal visibility; external-callable flags; SHOW … IN PACKAGE for listings; resolve name collisions vs schema.proc.  
- **Locks:** Default MGA non-blocking; explicit "WITH LOCK" enables row/table locks; add deadlock/timeout handling.  
- **Triggers:** Before/after for DB/table (SELECT support TBD); ordered by smallint; ensure runtime hooks.  

---

## Three-Tier Architecture (Validated)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      CLIENT APPLICATIONS                                 │
├─────────────────────────────────────────────────────────────────────────┤
│  PostgreSQL Client   │   MySQL Client   │   Firebird Client   │  ...   │
└──────────────────────┴──────────────────┴─────────────────────┴─────────┘
                            │           │           │
                            ▼           ▼           ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    PARSER AGENTS (Wire Protocols)                        │
├─────────────────────────────────────────────────────────────────────────┤
│  PostgreSQL Parser   │   MySQL Parser   │   Firebird Parser   │  ...   │
│  (Wire Protocol 3.0) │  (Protocol 4.1+) │   (XDR Protocol)    │        │
└──────────────────────┴──────────────────┴─────────────────────┴─────────┘
                            │           │           │
                            ▼           ▼           ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    SBWP (ScratchBird Wire Protocol)                      │
│                         Standardized Message Format                      │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         IPC CHANNEL (Unix Socket)                        │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    ENGINE IPC SESSION HANDLER                            │
│  • LRU Statement Cache  • Transaction Management  • COPY Flow Control   │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         SCRATCHBIRD ENGINE                               │
│  • SBLR Execution  • MVCC  • Storage  • Indexing  • Security            │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## What Was Completed (Alpha)

### Parser Agents
- Full PostgreSQL Wire Protocol 3.0 (SSL, auth, simple/extended query, COPY)
- Full MySQL Protocol 4.1+ (handshake, prepared statements, binary protocol)
- Full Firebird XDR Protocol (SRP auth, BLOBs, cursors, transactions)

### Authentication
- SCRAM-SHA-256/512 (RFC 5802/7677 compliant)
- PBKDF2 key derivation, HMAC-SHA-256/512
- Constant-time comparison (timing attack prevention)

### IPC Infrastructure
- EngineIPCSessionHandler with LRU statement cache (31 methods)
- UnixSocketIPCChannel with message framing
- COPY flow control with credit-based backpressure

### Type System
- Complete bidirectional mapping (140+ conversions)
- PostgreSQL 80+ OIDs, MySQL 35+ types, Firebird 25+ types
- Array type support

### Schema Introspection
- pg_catalog views (pg_class, pg_attribute, pg_type, pg_index, pg_constraint)
- information_schema (tables, columns, constraints)
- Firebird RDB$ compatibility

### UDR Connectors
- All 69 remaining stubs implemented
- PostgreSQL, MySQL, Firebird, ScratchBird connectors

---

## What's Next (Pre-Beta)

### Current Focus
1. **Integration Testing**
   - End-to-end testing with real PostgreSQL/MySQL/Firebird clients
   - Wire protocol compatibility verification
   - Performance benchmarking

2. **Performance Optimization**
   - Profile parser agent throughput
   - Optimize IPC message passing
   - Statement cache tuning

3. **Infrastructure Preparation**
   - Beta cluster implementation planning
   - CI/CD pipeline improvements
   - Docker container preparation

---

## Beta Scope (Required)

### Cluster + Replication
- UUIDv8-HLC conflict resolution
- Leaderless quorum replication
- Merkle anti-entropy

### Sharding + Migration
- Online migration workflows
- Shard-aware metadata
- Cross-node shard movement

### Performance + Observability
- JIT/AOT execution
- Vectorized execution
- Query store and plan history

### Ecosystem
- Complete driver ecosystem (15+ languages)
- BI tool integrations
- Cloud/container packaging

---

## Pointers

- **Alpha Completion Report:** `ALPHA_COMPLETION_REPORT.md`
- **Alpha Completion Summary:** `ALPHA_COMPLETION_SUMMARY_2026-02-06.md`
- **Roadmap:** `/docs/specifications/parser/v3/OFFICIAL_ROADMAP.md`
- **Status Dashboard:** `docs/IMPLEMENTATION_STATUS_DASHBOARD.md`
- **Where We're Going (Beta):** `docs/WHERE_WE_ARE_GOING_BETA.md`
- **Beta Specifications:** `/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-SUMMARY.md`

---

## Files Created/Updated for Alpha

### Headers (25 files)
- `include/scratchbird/ipc/engine_ipc_session_handler.h`
- `include/scratchbird/ipc/copy_flow_control.h`
- `include/scratchbird/ipc/unix_socket_ipc_channel.h`
- `include/scratchbird/client/postgresql_parser_agent.h`
- `include/scratchbird/client/mysql_parser_agent.h`
- `include/scratchbird/client/firebird_parser_agent.h`
- `include/scratchbird/auth/scram_auth.h`
- `include/scratchbird/types/type_mapping.h`
- `include/scratchbird/catalog/schema_introspection.h`
- 16 UDR connector headers

### Implementations (22 files)
- `src/engine/engine_ipc_session_handler.cpp`
- `src/ipc/copy_flow_control.cpp`
- `src/ipc/unix_socket_ipc_channel.cpp`
- `src/client/postgresql_parser_agent.cpp`
- `src/client/mysql_parser_agent.cpp`
- `src/client/firebird_parser_agent.cpp`
- `src/auth/scram_auth.cpp`
- `src/types/type_mapping.cpp`
- `src/catalog/schema_introspection.cpp`
- 13 UDR connector implementations

### Tests (9 files, 773 test cases)
- `tests/unit/test_engine_ipc_session_handler.cpp`
- `tests/unit/test_postgresql_parser_agent.cpp`
- `tests/unit/test_mysql_parser_agent.cpp`
- `tests/unit/test_firebird_parser_agent.cpp`
- `tests/unit/test_scram_auth.cpp`
- `tests/unit/test_type_mapping.cpp`
- `tests/unit/test_schema_introspection.cpp`
- `tests/unit/test_copy_flow_control.cpp`
- `tests/unit/test_unix_socket_channel.cpp`
