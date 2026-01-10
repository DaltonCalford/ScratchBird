# Beta Replication Implementation Phases

**Document:** 06_IMPLEMENTATION_PHASES.md
**Status:** BETA SPECIFICATION
**Version:** 1.0
**Date:** 2026-01-09
**Authority:** Chief Architect

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Phase Overview](#phase-overview)
3. [Phase 1: Foundation](#phase-1-foundation)
4. [Phase 2: Leaderless Quorum](#phase-2-leaderless-quorum)
5. [Phase 3: Schema Colocation](#phase-3-schema-colocation)
6. [Phase 4: Time-Partitioned Merkle](#phase-4-time-partitioned-merkle)
7. [Phase 5: MGA Integration](#phase-5-mga-integration)
8. [Phase 6: Hardening & Testing](#phase-6-hardening--testing)
9. [Dependencies and Prerequisites](#dependencies-and-prerequisites)
10. [Risk Mitigation](#risk-mitigation)
11. [Success Criteria](#success-criteria)

---

## Executive Summary

### Timeline

**Total Duration**: 24 weeks (6 months)

**Educational Project**: No hard deadlines, timeline estimates for planning only.

### Phased Approach

| Phase | Focus | Duration | Key Deliverables |
|-------|-------|----------|------------------|
| **1** | Foundation | 4 weeks | UUIDv8-HLC, catalog extensions, SBLR opcodes |
| **2** | Leaderless Quorum | 6 weeks | Quorum coordinator, hinted handoff, gossip protocol |
| **3** | Schema Colocation | 4 weeks | Partition keys, local transactions, foreign key optimization |
| **4** | Time-Partitioned Merkle | 6 weeks | Sealed trees, verification, anti-entropy engine |
| **5** | MGA Integration | 2 weeks | Distributed TIP, global GC, commit log streaming |
| **6** | Hardening & Testing | 2 weeks | Chaos tests, benchmarks, documentation |

**Total**: 24 weeks

### Critical Path

```
Phase 1 (Foundation) is BLOCKING for all subsequent phases
Phase 2, 3, 4 can proceed partially in parallel after Phase 1
Phase 5 depends on Phases 2, 3, 4 completion
Phase 6 depends on Phase 5 completion
```

---

## Phase Overview

### Phase Dependencies

```
┌─────────────┐
│  Phase 1    │
│ Foundation  │ ← BLOCKING (Weeks 1-4)
└──────┬──────┘
       │
   ┌───┴───┬─────────┬──────────┐
   │       │         │          │
┌──▼───┐ ┌─▼──────┐ ┌▼────────┐ │
│Phase2│ │ Phase3 │ │ Phase4  │ │
│Quorum│ │Coloc.  │ │ Merkle  │ │
└──┬───┘ └───┬────┘ └───┬─────┘ │
   │         │          │       │
   └────┬────┴────┬─────┘       │
        │         │             │
     ┌──▼─────────▼───┐         │
     │   Phase 5      │◄────────┘
     │ MGA Integration│ (Weeks 21-22)
     └────────┬───────┘
              │
         ┌────▼────────┐
         │   Phase 6   │
         │  Hardening  │ (Weeks 23-24)
         └─────────────┘
```

### Parallel Work Opportunities

**After Phase 1 Completes**:
- Phase 2 (Quorum) and Phase 3 (Colocation) can proceed in parallel (different subsystems)
- Phase 4 (Merkle) depends on UUIDv8 time-ordering (Phase 1), can start Week 5

**Phase 5** (MGA Integration) requires:
- Quorum writes working (Phase 2)
- Partition keys working (Phase 3)
- Anti-entropy working (Phase 4)

---

## Phase 1: Foundation

### Duration

**Weeks 1-4** (4 weeks)

### Goals

Implement core infrastructure required by all subsequent phases:
1. UUIDv8-HLC generation and comparison
2. Catalog extensions for replication configuration
3. SBLR opcodes for UUIDv8 operations
4. Basic gossip protocol framework

### Tasks

#### Week 1: UUIDv8-HLC Implementation

**Files to Create**:
- `include/scratchbird/core/uuidv8_hlc.h` (UUIDv8HLC struct, generator)
- `src/core/uuidv8_hlc.cpp` (generation algorithm, HLC update)
- `tests/unit/test_uuidv8_hlc.cpp` (monotonicity, clock skew, overflow)

**Implementation Checklist**:
- [ ] `UuidV8HLC` struct with 128-bit byte array
- [ ] `UuidV8Generator` class with HLC state (timestamp, counter, node_id)
- [ ] `generate()` function with monotonicity guarantee
- [ ] `updateHlcFromRemote()` function for causal ordering
- [ ] `operator<()` for conflict resolution (LWW semantics)
- [ ] Unit tests: 100% coverage (monotonicity, clock skew, counter overflow)

**Acceptance Criteria**:
- [ ] Generate 1M UUIDs/sec per thread (performance test)
- [ ] Monotonicity test passes (10K sequential UUIDs)
- [ ] Clock skew test passes (NTP adjustment simulation)
- [ ] Counter overflow handled correctly (stall 1ms)

#### Week 2: Catalog Extensions

**Files to Modify**:
- `src/core/catalog_manager.cpp` (add replication config columns)
- `include/scratchbird/core/catalog_manager.h` (add getters for replication config)
- `src/parser/parser_v2.cpp` (parse `WITH (replication_mode=...)` syntax)
- `tests/unit/test_catalog_replication_config.cpp` (new test file)

**Schema Changes**:

```sql
-- Add columns to sys_tables
ALTER TABLE sys_tables ADD COLUMN replication_mode VARCHAR(20) DEFAULT 'primary_replica';
ALTER TABLE sys_tables ADD COLUMN replication_factor INTEGER DEFAULT 1;
ALTER TABLE sys_tables ADD COLUMN write_quorum INTEGER DEFAULT 1;
ALTER TABLE sys_tables ADD COLUMN read_quorum INTEGER DEFAULT 1;
ALTER TABLE sys_tables ADD COLUMN uuid_type VARCHAR(10) DEFAULT 'v7';
ALTER TABLE sys_tables ADD COLUMN partition_key_columns TEXT DEFAULT NULL;
```

**Implementation Checklist**:
- [ ] Add catalog columns (bootstrap migration)
- [ ] Parser support for `WITH (replication_mode='leaderless_quorum', ...)`
- [ ] Catalog validation (W + R > N for strong consistency)
- [ ] Unit tests: Parse replication config, store/retrieve from catalog

**Acceptance Criteria**:
- [ ] Can create table with `replication_mode='leaderless_quorum'`
- [ ] Catalog query returns replication config correctly
- [ ] Invalid config rejected (e.g., W > N)

#### Week 3: SBLR Opcodes

**Files to Modify**:
- `include/scratchbird/sblr/opcodes.h` (add UUIDv8 opcodes)
- `src/sblr/bytecode_generator_v2.cpp` (generate UUIDv8 opcodes)
- `src/sblr/executor.cpp` (execute UUIDv8 opcodes)
- `tests/unit/test_uuidv8_opcodes.cpp` (new test file)

**New Opcodes**:

```cpp
enum class Opcode : uint8_t {
    // Existing
    GEN_UUID_V7 = 0x8A,

    // New (Beta)
    GEN_UUID_V8_HLC = 0x8B,          // Generate UUIDv8 with HLC
    UUID_V8_TIMESTAMP = 0x8C,        // Extract timestamp (48 bits)
    UUID_V8_COUNTER = 0x8D,          // Extract HLC counter (12 bits)
    UUID_V8_NODE_ID = 0x8E,          // Extract node_id (16 bits)
    UUID_COMPARE = 0x8F,             // Compare two UUIDs (any version)
};
```

**Implementation Checklist**:
- [ ] Bytecode generation for `gen_uuidv8_hlc()` SQL function
- [ ] Executor implementation for GEN_UUID_V8_HLC opcode
- [ ] Extraction opcodes (UUID_V8_TIMESTAMP, UUID_V8_COUNTER, UUID_V8_NODE_ID)
- [ ] UUID comparison opcode (handles UUIDv7 and UUIDv8)
- [ ] Unit tests: Generate UUIDv8, extract fields, compare

**Acceptance Criteria**:
- [ ] SQL: `SELECT gen_uuidv8_hlc()` returns valid UUIDv8
- [ ] SQL: `SELECT uuidv8_timestamp(gen_uuidv8_hlc())` returns current timestamp
- [ ] SBLR bytecode round-trip (generate → execute → verify)

#### Week 4: Gossip Protocol Framework

**Files to Create**:
- `include/scratchbird/cluster/gossip_protocol.h` (GossipManager)
- `src/cluster/gossip_protocol.cpp` (peer discovery, message exchange)
- `tests/unit/test_gossip_protocol.cpp` (new test file)

**Implementation Checklist**:
- [ ] `GossipManager` class (peer list, message queue)
- [ ] Peer discovery (bootstrap from config, dynamic discovery via gossip)
- [ ] Message exchange (send, receive, ack)
- [ ] Heartbeat mechanism (detect node failures)
- [ ] Unit tests: Peer discovery, message delivery, failure detection

**Acceptance Criteria**:
- [ ] 3-node cluster: All nodes discover each other via gossip
- [ ] Heartbeat detects node failure within 10 seconds
- [ ] Message delivery reliable (ACK protocol)

---

## Phase 2: Leaderless Quorum

### Duration

**Weeks 5-10** (6 weeks)

### Goals

Implement Mode 3 (Leaderless Quorum Replication) with quorum coordination, hinted handoff, and read repair.

### Tasks

#### Week 5-6: Quorum Coordinator

**Files to Create**:
- `include/scratchbird/replication/quorum_coordinator.h`
- `src/replication/quorum_coordinator.cpp`
- `tests/unit/test_quorum_coordinator.cpp`

**Implementation Checklist**:
- [ ] `coordinateWrite()` function (send to N replicas, wait for W ACKs)
- [ ] `coordinateRead()` function (query R replicas, reconcile versions)
- [ ] Timeout handling (configurable: `write_timeout_ms`, `read_timeout_ms`)
- [ ] Conflict resolution (UUIDv8-HLC comparison, LWW)
- [ ] Unit tests: W=2/N=3 write succeeds, W=3/N=3 write with 1 failure

**Acceptance Criteria**:
- [ ] Write with W=2, N=3: Succeeds if 2 replicas ACK
- [ ] Write with W=3, N=3: Fails if 1 replica offline
- [ ] Read with R=2, N=3: Returns latest version (HLC-based)

#### Week 7-8: Hinted Handoff

**Files to Create**:
- `include/scratchbird/replication/hinted_handoff.h`
- `src/replication/hinted_handoff.cpp`
- `tests/unit/test_hinted_handoff.cpp`

**Implementation Checklist**:
- [ ] `HintedHandoffManager` class (hint storage, delivery)
- [ ] `storeHint()` function (durable hint log)
- [ ] `deliverHints()` background loop (every 10 seconds)
- [ ] Hint expiration (7 days default, configurable)
- [ ] Unit tests: Store hint, deliver on recovery, expire old hints

**Acceptance Criteria**:
- [ ] Write succeeds (W=2), 3rd replica offline → Hint stored
- [ ] 3rd replica recovers → Hint delivered within 10 seconds
- [ ] Hints older than 7 days automatically expired

#### Week 9-10: Read Repair & Integration

**Files to Modify**:
- `src/replication/quorum_coordinator.cpp` (add read repair logic)
- `src/sblr/executor.cpp` (integrate quorum writes/reads)
- `tests/integration/test_leaderless_quorum.cpp` (new test file)

**Implementation Checklist**:
- [ ] Read repair: Detect version divergence during read, async repair
- [ ] Executor integration: Route writes/reads to quorum coordinator
- [ ] Table-level configuration: Check `replication_mode` from catalog
- [ ] Integration tests: Multi-node quorum writes, reads, conflicts

**Acceptance Criteria**:
- [ ] Read detects divergence → Read repair triggered (async)
- [ ] 3-node cluster: Concurrent writes converge (UUIDv8-HLC winner)
- [ ] Table with `replication_mode='leaderless_quorum'` uses quorum path

---

## Phase 3: Schema Colocation

### Duration

**Weeks 11-14** (4 weeks)

### Goals

Implement partition keys, schema-driven colocation, and local multi-table transactions.

### Tasks

#### Week 11-12: Partition Keys

**Files to Modify**:
- `src/parser/parser_v2.cpp` (parse `PARTITION BY` clause)
- `src/core/catalog_manager.cpp` (store partition key columns)
- `src/cluster/shard_router.cpp` (compute shard_id from partition key)
- `tests/unit/test_partition_keys.cpp` (new test file)

**Implementation Checklist**:
- [ ] Parser: `CREATE TABLE ... PARTITION BY (user_id)`
- [ ] Catalog: Store partition_key_columns (comma-separated)
- [ ] Shard router: `computeShardId(partition_key_value)` using xxhash64
- [ ] Immutability enforcement: Reject UPDATE on partition key column
- [ ] Unit tests: Parse partition key, compute shard_id deterministically

**Acceptance Criteria**:
- [ ] Table created with `PARTITION BY user_id`
- [ ] Rows with same user_id land on same shard (verified)
- [ ] UPDATE partition key → Error

#### Week 13: Foreign Key Optimization

**Files to Modify**:
- `src/core/catalog_manager.cpp` (analyze foreign keys, suggest colocation)
- `src/parser/parser_v2.cpp` (warn on FK without colocation)
- `tests/unit/test_fk_colocation.cpp` (new test file)

**Implementation Checklist**:
- [ ] FK analysis: Detect `orders.user_id REFERENCES users(user_id)`
- [ ] Colocation suggestion: Log warning if orders not partitioned by user_id
- [ ] Catalog query: `suggestColocation(table_name)` API
- [ ] Unit tests: FK detection, colocation suggestion

**Acceptance Criteria**:
- [ ] FK detected: `orders.user_id → users.user_id`
- [ ] Warning logged: "Consider PARTITION BY user_id for colocation"

#### Week 14: Local Transactions

**Files to Modify**:
- `src/sblr/executor.cpp` (detect single-shard transactions, route to shard)
- `src/replication/transaction_coordinator.cpp` (2PC for cross-shard)
- `tests/integration/test_colocated_transactions.cpp` (new test file)

**Implementation Checklist**:
- [ ] Transaction plan analysis: Detect single-shard vs multi-shard
- [ ] Single-shard: Execute locally (existing TIP logic)
- [ ] Multi-shard: Use 2PC (existing transaction_manager.cpp prepareTransaction)
- [ ] Integration tests: Colocated multi-table transaction (ACID)

**Acceptance Criteria**:
- [ ] Transaction updating users + orders (both partition by user_id) → Local
- [ ] Transaction updating accounts A and B (different shards) → 2PC
- [ ] Single-shard transaction latency < 5ms (benchmark)

---

## Phase 4: Time-Partitioned Merkle

### Duration

**Weeks 15-20** (6 weeks)

### Goals

Implement time-partitioned Merkle forests for efficient anti-entropy verification.

### Tasks

#### Week 15-16: Merkle Tree Structure

**Files to Create**:
- `include/scratchbird/replication/merkle_tree.h`
- `src/replication/merkle_tree.cpp`
- `tests/unit/test_merkle_tree.cpp`

**Implementation Checklist**:
- [ ] `MerkleTree` class (leaf map, internal nodes, root hash)
- [ ] `insertLeaf(row_id, row_hash)` function
- [ ] `computeRootHash()` function (binary tree, bottom-up)
- [ ] Serialization: `serializeToDisk()`, `deserializeFromDisk()`
- [ ] Unit tests: Insert leaves, compute root, deterministic hash

**Acceptance Criteria**:
- [ ] Insert 10K leaves, compute root hash < 100ms
- [ ] Root hash deterministic (same leaves, different order → same root)

#### Week 17-18: Sealed Trees & Forest Management

**Files to Create**:
- `include/scratchbird/replication/merkle_forest.h`
- `src/replication/merkle_forest.cpp`
- `tests/unit/test_merkle_forest.cpp`

**Implementation Checklist**:
- [ ] `MerkleForest` class (sealed trees + active tree)
- [ ] `insertRow(row)` function (route to window based on timestamp)
- [ ] `sealTree(window_id)` function (compute root, serialize, mark sealed)
- [ ] Tree sealing trigger (10-minute grace period after window ends)
- [ ] Unit tests: Insert rows, seal tree, verify immutability

**Acceptance Criteria**:
- [ ] Rows inserted in Window 0 → Tree_0
- [ ] Time advances → Tree_0 sealed, root hash cached
- [ ] New rows → Tree_1 (active), Tree_0 unchanged

#### Week 19: Verification Algorithm

**Files to Create**:
- `include/scratchbird/replication/anti_entropy_coordinator.h`
- `src/replication/anti_entropy_coordinator.cpp`
- `tests/unit/test_anti_entropy_verification.cpp`

**Implementation Checklist**:
- [ ] `verifySealedTrees()` function (compare root hashes, O(1) per tree)
- [ ] `repairWindow()` function (drill down, find divergent rows)
- [ ] Gossip integration: Exchange sealed tree metadata
- [ ] Unit tests: Detect divergence, drill down to leaf, repair

**Acceptance Criteria**:
- [ ] 720 sealed trees (30 days): Verification < 100ms
- [ ] Divergent window detected: Drill down finds exact row
- [ ] Repair: Sync divergent row, verify root hashes match

#### Week 20: LSM Compaction Integration

**Files to Modify**:
- `src/storage/lsm_compactor.cpp` (build Merkle tree during compaction)
- `tests/integration/test_merkle_lsm.cpp` (new test file)

**Implementation Checklist**:
- [ ] Compaction hook: Call `merkle_tree.insertLeaf()` for each row
- [ ] Seal tree after compaction completes (if window passed)
- [ ] Integration tests: Compact L0→L1, verify Merkle tree sealed

**Acceptance Criteria**:
- [ ] Compaction builds Merkle tree with 0% additional I/O
- [ ] Sealed tree root hash matches manual tree construction

---

## Phase 5: MGA Integration

### Duration

**Weeks 21-22** (2 weeks)

### Goals

Integrate distributed TIP, global GC, and commit log streaming with existing MGA implementation.

### Tasks

#### Week 21: Distributed TIP & Visibility

**Files to Modify**:
- `include/scratchbird/core/transaction_manager.h` (add distributed methods)
- `src/core/transaction_manager.cpp` (global XID, distributed visibility)
- `tests/unit/test_distributed_tip.cpp` (new test file)

**Implementation Checklist**:
- [ ] `allocateGlobalXid(node_id)` function (node_id in upper 16 bits)
- [ ] `isVersionVisibleDistributed()` function (TIP + HLC check)
- [ ] `replicateTIPEntry()` function (receive TIP from remote node)
- [ ] Unit tests: Global XID allocation, distributed visibility

**Acceptance Criteria**:
- [ ] Node 1 allocates xid=0x0001_..., Node 2 allocates xid=0x0002_... (no collision)
- [ ] Node A writes, Node B reads → Visibility correct (TIP + HLC)

#### Week 22: Global GC & Commit Log

**Files to Modify**:
- `src/core/transaction_manager.cpp` (global OIT/OAT computation)
- `src/replication/commit_log_streamer.cpp` (new file, stream TIP entries)
- `tests/integration/test_distributed_gc.cpp` (new test file)

**Implementation Checklist**:
- [ ] `computeGlobalOIT()` function (gossip aggregation)
- [ ] `performDistributedSweep()` function (sweep using global OIT)
- [ ] `CommitLogStreamer` class (stream commit log to replicas)
- [ ] Integration tests: Distributed GC, commit log replication

**Acceptance Criteria**:
- [ ] 3-node cluster: Global OIT = min(all nodes' local OIT)
- [ ] GC sweeps back versions conservatively (global OIT threshold)
- [ ] Commit log streamed to replicas, TIP replicated

---

## Phase 6: Hardening & Testing

### Duration

**Weeks 23-24** (2 weeks)

### Goals

Chaos testing, performance benchmarks, documentation, and production readiness.

### Tasks

#### Week 23: Chaos Testing

**Files to Create**:
- `tests/chaos/test_network_partition.cpp`
- `tests/chaos/test_node_failure.cpp`
- `tests/chaos/test_clock_skew.cpp`

**Implementation Checklist**:
- [ ] Network partition test (split cluster, verify quorum availability)
- [ ] Node failure test (kill node, verify hinted handoff + anti-entropy)
- [ ] Clock skew test (adjust wall clock, verify HLC correction)
- [ ] Chaos scenario: Random node failures + partitions (verify convergence)

**Acceptance Criteria**:
- [ ] Network partition: Majority partition writable, minority read-only
- [ ] Node failure: Recovery via hints + anti-entropy < 1 minute
- [ ] Clock skew: HLC prevents causality violations

#### Week 24: Performance Benchmarks & Documentation

**Benchmarks**:
- [ ] Write throughput: 50K TPS per shard (Mode 3, W=2, R=2, N=3)
- [ ] Read latency: p99 < 20ms (same DC)
- [ ] Anti-entropy overhead: < 2% CPU
- [ ] Conflict resolution: < 1ms per conflict

**Documentation**:
- [ ] User guide: How to enable Mode 3, configure quorum parameters
- [ ] Operations guide: Monitoring, troubleshooting, tuning
- [ ] API reference: SQL functions (gen_uuidv8_hlc, etc.)
- [ ] Architecture diagrams: Update with Beta features

**Acceptance Criteria**:
- [ ] All performance targets met (or documented deviations)
- [ ] Documentation reviewed by 2 engineers, published to wiki

---

## Dependencies and Prerequisites

### Prerequisites (Must Complete Before Beta)

**Alpha Parser Remediation** (P-001 to P-010):
- [ ] Temporary tables (P-001)
- [ ] ENUM domains (P-002)
- [ ] SET domains (P-003)
- [ ] VARIANT domains (P-004)
- [ ] Advanced domain features (P-005)
- [ ] Domain DDL (P-006)
- [ ] SHOW/SET commands (P-007)
- [ ] Subquery expressions (P-008)
- [ ] View expansion (P-009)
- [ ] Parser V1 deprecation (P-010)

**Status**: Alpha remediation MUST be complete before starting Beta Phase 1.

### External Dependencies

**SBCLUSTER Specifications**:
- [ ] SBCLUSTER-01: Cluster Config Epoch (cluster join protocol)
- [ ] SBCLUSTER-02: Membership & Identity (node IDs)
- [ ] SBCLUSTER-05: Sharding (shard map, consistent hashing)
- [ ] SBCLUSTER-07: Replication (base commit log streaming)

**Status**: SBCLUSTER-01, 02, 05 should be implemented before Phase 2 (Quorum).

### Concurrent Development

**Can Proceed in Parallel**:
- SBCLUSTER-08: Backup/Restore (shares time-partitioned Merkle trees)
- SBCLUSTER-09: Scheduler (uses replication topology)
- SBCLUSTER-10: Observability (monitors replication lag, anti-entropy metrics)

---

## Risk Mitigation

### High Risk

**Risk 1: HLC Counter Overflow Under Load**
- **Mitigation**: Stall 1ms when counter reaches 4095 (advance timestamp)
- **Fallback**: Reduce sub-millisecond precision (accept collisions)
- **Detection**: Log warning if counter exceeds 3000 (indicates high load or clock issues)

**Risk 2: Time-Partitioned Tree Memory Overhead**
- **Mitigation**: Serialize sealed trees to disk, keep only roots in memory
- **Fallback**: Increase sealing interval (1 hour → 4 hours, reduce tree count)
- **Monitoring**: Track tree count, memory usage per shard

**Risk 3: Cross-Shard Transaction Performance**
- **Mitigation**: Document 2PC latency (2 RTTs), recommend colocation
- **Acceptance**: Multi-shard transactions inherently slower (expected trade-off)
- **Alternative**: Saga pattern for eventual consistency (no 2PC)

### Medium Risk

**Risk 4: Gossip Protocol Scalability (100+ nodes)**
- **Mitigation**: Limit gossip fanout (max 10 peers per round)
- **Fallback**: Hierarchical gossip (clusters of 10 nodes, cross-cluster gossip)
- **Monitoring**: Track gossip message volume, latency

**Risk 5: Anti-Entropy Verification Load**
- **Mitigation**: Stagger verification (not all shards at once)
- **Fallback**: Reduce verification frequency (60s → 300s)
- **Monitoring**: Track CPU usage during anti-entropy

### Low Risk

**Risk 6: Clock Skew in HLC**
- **Mitigation**: Require NTP synchronization (operational requirement)
- **Detection**: Alert if HLC counter grows beyond threshold (indicates skew)
- **Acceptance**: Clock skew < 100ms (NTP standard)

---

## Success Criteria

### Functional Requirements (Beta)

- [ ] Time-partitioned Merkle forest operational for 1M+ rows per shard
- [ ] HLC embedded in UUIDv8, conflict resolution deterministic
- [ ] Schema-driven colocation enables local multi-table transactions
- [ ] Anti-entropy completes in O(log N) comparisons for sealed trees
- [ ] Cross-shard queries maintain per-shard snapshot isolation (MGA)
- [ ] Replication lag < 100ms (same DC), < 1s (cross-DC)

### Performance Targets (Beta)

- [ ] Write throughput: 50K TPS per shard (RF=2, async replication)
- [ ] Anti-entropy overhead: < 2% CPU (vs 10-20% for hash-based Merkle trees)
- [ ] Time to detect divergence: < 10 seconds
- [ ] Repair bandwidth: Proportional to differences (not dataset size)

### Integration Requirements (Beta)

- [ ] Works with Firebird MGA visibility (TIP-based)
- [ ] Integrates with SBCLUSTER-07 per-shard write-after log (WAL) streaming
- [ ] Compatible with existing shadow database protocol
- [ ] Audit chain records all replication topology changes

### Beta Completion Definition

**"Beta is complete when ALL cluster features are implemented and tested."**
- All 6 phases delivered with passing tests
- Performance targets met (or documented deviations with mitigation plan)
- Documentation complete (user guide, operations guide, API reference)
- Chaos testing passes (network partitions, node failures, clock skew)

---

## Appendix: Week-by-Week Schedule

```
Week  Phase  Focus                          Key Deliverable
----  -----  -----------------------------  ----------------------------------
1     1      UUIDv8-HLC                     UuidV8Generator class, unit tests
2     1      Catalog Extensions             replication_mode, partition_key_columns
3     1      SBLR Opcodes                   GEN_UUID_V8_HLC, UUID_COMPARE
4     1      Gossip Framework               Peer discovery, heartbeat

5     2      Quorum Coordinator (Part 1)    coordinateWrite() function
6     2      Quorum Coordinator (Part 2)    coordinateRead(), conflict resolution
7     2      Hinted Handoff (Part 1)        HintedHandoffManager, hint storage
8     2      Hinted Handoff (Part 2)        Hint delivery loop, expiration
9     2      Read Repair                    Async read repair during reads
10    2      Quorum Integration             Executor routing, integration tests

11    3      Partition Keys (Part 1)        Parser: PARTITION BY clause
12    3      Partition Keys (Part 2)        Shard router, immutability enforcement
13    3      Foreign Key Optimization       FK analysis, colocation suggestions
14    3      Local Transactions             Single-shard detection, 2PC fallback

15    4      Merkle Tree Structure          MerkleTree class, computeRootHash()
16    4      Tree Serialization             serializeToDisk(), deserializeFromDisk()
17    4      Merkle Forest (Part 1)         MerkleForest class, sealed trees
18    4      Merkle Forest (Part 2)         Tree sealing trigger, grace period
19    4      Verification Algorithm         verifySealedTrees(), repairWindow()
20    4      LSM Compaction Integration     Build tree during compaction

21    5      Distributed TIP                allocateGlobalXid(), distributed visibility
22    5      Global GC & Commit Log         computeGlobalOIT(), CommitLogStreamer

23    6      Chaos Testing                  Network partition, node failure tests
24    6      Performance & Docs             Benchmarks, user guide, operations guide
```

---

**Document Status:** DRAFT (Beta Specification Phase)
**Last Updated:** 2026-01-09
**Next Review:** Weekly during implementation
**Approval Required:** Chief Architect, Distributed Systems Lead, Storage Engineering Lead

---

**End of Document 06**
