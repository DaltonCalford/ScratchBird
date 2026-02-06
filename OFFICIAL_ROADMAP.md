# Official Roadmap

**Last Updated:** February 6, 2026  
**Status:** ✅ Alpha Complete, Preparing for Beta

---

## Alpha (COMPLETE) ✅

### Alpha Completion Summary

**Completed:** February 6, 2026  
**Total Implementation:** 19,400+ lines across 73 files  
**NOT_IMPLEMENTED Stubs:** 84+ → 0 (100% complete)  
**Test Pass Rate:** 99.8% (3,593/3,600 tests passing)

### Core Engine ✅
- **Completed:** storage engine (heap), MGA transactions, base catalog, scheduler/jobs,
  constraint enforcement, RLS/definer security wiring, cache/buffer plan.
- **Completed:** tablespace routing defaults + root page allocation
- **Completed:** index migration safety for SPGIST/BITMAP/COLUMNSTORE/LSM
- **Completed:** monitoring parity (MON$ placeholders → sys.* tables)
- **Completed:** backup/restore parity across tablespaces/catalogs
- **Completed:** timezone/charset/collation resource loading + catalog persistence

### Parser + PSQL ✅
- **Completed:** V2 parser base, dialect parsers, semantic analyzer v2, baseline bytecode
- **Completed:** V2 parser completeness (DDL/DML/utility/PSQL surface)
- **Completed:** PSQL bytecode emission + executor parity (FOR/CASE/SUSPEND/etc)
- **Completed:** MERGE statements, statement-level CASE
- **Completed:** COPY FROM/TO with flow control

### Network & Service ✅
- **Completed:** listener/pool/parser/server process operational, wire adapters (FB/MySQL/PG/native)
- **Completed:** full wire protocol implementations:
  - PostgreSQL: Protocol 3.0 (SSL, auth, simple/extended query, COPY)
  - MySQL: Protocol 4.1+ (handshake, prepared statements, binary protocol)
  - Firebird: XDR Protocol (SRP auth, BLOBs, cursors)
- **Completed:** SCRAM-SHA-256/512 authentication (RFC 5802/7677)
- **Completed:** dialect parity test suites
- **Completed:** auth/config wiring

### Alpha Components Delivered

| Component | Description | Lines Added |
|-----------|-------------|-------------|
| EngineIPCSessionHandler | Full engine integration with LRU statement cache | ~3,200 |
| PostgreSQL Parser Agent | Wire protocol 3.0 full implementation | ~2,800 |
| MySQL Parser Agent | Protocol 4.1+ full implementation | ~2,600 |
| Firebird Parser Agent | XDR protocol full implementation | ~2,400 |
| SCRAM-SHA-256/512 Auth | RFC 5802/7677 compliant | ~1,800 |
| Type Mapping | 140+ type conversions | ~2,200 |
| COPY Flow Control | Credit-based backpressure | ~1,300 |
| Schema Introspection | pg_catalog, information_schema, RDB$ | ~1,300 |
| UnixSocketIPCChannel | Full IPC channel with message framing | ~1,400 |
| UDR Connectors | All 69 remaining stubs implemented | ~690 |

### Alpha Test Suite

- **Total Test Cases:** 3,600+
- **Pass Rate:** 99.8%
- **New Alpha Tests:** 670+ test cases across 9 test files
- **Test Files:** test_engine_ipc_session_handler, test_postgresql_parser_agent, test_mysql_parser_agent, test_firebird_parser_agent, test_scram_auth, test_type_mapping, test_schema_introspection, test_copy_flow_control, test_unix_socket_channel

---

## Pre-Beta (Current) 🚧

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

4. **Documentation**
   - Protocol compatibility matrices
   - Migration guides from other databases
   - Administration documentation

---

## Beta (Planned) 📋

### Specifications Complete ✅

All Beta specifications are complete and ready for implementation:

### Beta Scope

1. **Cluster + Replication**
   - Cluster membership, security, and policy (SBCLUSTER suite)
   - Distributed MGA with UUIDv8-HLC conflict resolution
   - Leaderless quorum replication and time-partitioned Merkle forests
   - Schema-aware colocation and shard-aware routing
   - Autoscaling and elastic node lifecycle (SBCLUSTER-12)

2. **Sharding + Cross-Server Migration**
   - Online migration workflows
   - Shard-aware metadata
   - Consistent hashing (16-256 shards)

3. **JIT/AOT Execution**
   - Vectorized execution
   - Native code generation

4. **Drivers/ORMs/Tools**
   - ODBC/JDBC drivers
   - BI tool integrations
   - Framework support

5. **Big Data/Streaming**
   - Kafka/Flink integrations
   - Cloud/container packaging

6. **Optional Engine Enhancements**
   - Storage encoding improvements
   - File shrink/compaction

### Beta Differentiators

- **Against MySQL/PostgreSQL:** Built-in replication and migration tooling without external layers
- **Against FirebirdSQL:** Cluster/distributed features while retaining MGA heritage
- **Against Enterprise Engines:** HA/DR, observability, and integration breadth without lock-in

---

## GA Phase (Future)

- Production hardening and performance tuning
- Extended SQL features
- Cloud-native deployment options
- Extended protocol support (TDS/MSSQL)

---

## References

- **Alpha Completion Report:** `ALPHA_COMPLETION_REPORT.md`
- **Alpha Completion Summary:** `ALPHA_COMPLETION_SUMMARY_2026-02-06.md`
- **Status Dashboard:** `docs/IMPLEMENTATION_STATUS_DASHBOARD.md`
- **Alpha/Beta Scope:** `docs/findings/ALPHA_BETA_SCOPE_STATUS.md`
- **Beta Planning:** `docs/WHERE_WE_ARE_GOING_BETA.md`
- **Planning Index:** `docs/planning/`
- **Cluster Specifications:** `docs/specifications/Cluster Specification Work/`
