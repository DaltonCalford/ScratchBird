# ScratchBird Beta Cluster Replication Specification Suite
## Complete Multi-Node Replication Architecture

**Status:** BETA SPECIFICATION (COMPREHENSIVE SCOPE)
**Version:** 1.0
**Date:** 2026-01-08
**Authority:** Chief Architect
**Project Type:** Educational (No Hard Deadlines)

---

## Executive Summary

This specification suite defines ScratchBird's **complete Beta cluster replication architecture** - the PRIMARY focus of the Beta release. Building on Alpha's mature transaction system (fully implemented MGA/TIP), Beta adds distributed replication capabilities for production-grade multi-node deployments.

**Beta Mandate:** ALL cluster features MUST be complete before Beta release. No deferrals to GA.

**Key Innovation:** RFC 9562 UUIDv8 with embedded Hybrid Logical Clocks (HLC) for deterministic conflict resolution in leaderless multi-master replication, combined with time-partitioned Merkle forests for efficient anti-entropy.

---

## Architecture Philosophy: Building on Alpha's Foundation

### What Alpha Provides (Fully Implemented)

✅ **Transaction System** - 2,069 lines of production code
- TIP (Transaction Inventory Pages) - disk-based transaction state
- MGA visibility (`isVersionVisible()`) - TIP lookups, not snapshots
- Group commit optimization - batched TIP writes
- 2PC support - PREPARED state for distributed transactions
- 64-bit XIDs - no wraparound issues

✅ **UUIDv7 Identifiers** - RFC 9562 compliant
- Time-ordered by default (`using ID = UuidV7Bytes;`)
- System-wide: table_id, database_id, node_id, etc.
- Sequential B-tree inserts (no random I/O)

✅ **Per-Shard Architecture** (SBCLUSTER-05)
- Hash-based shard assignment
- Independent shard transactions
- No cross-shard 2PC (per SBCLUSTER-00 guiding principles)

### What Beta Adds (This Specification)

🎯 **Leaderless Multi-Master Replication**
- Any node accepts writes (no single bottleneck)
- Quorum coordination (W + R > N)
- UUIDv8-HLC conflict resolution
- Global write capability for multi-region deployments

🎯 **Time-Partitioned Anti-Entropy**
- Hourly sealed Merkle trees (immutable roots)
- O(1) divergence detection per time window
- Bandwidth proportional to differences (not dataset size)
- <2% CPU overhead (vs 10-20% for hash-based)

🎯 **Schema-Driven Colocation**
- Multi-table atomicity via shared partition keys
- Local transactions (no 2PC needed)
- Foreign key-aware shard assignment

🎯 **UUIDv8 with Embedded HLC**
- RFC 9562 version 8 (custom format)
- 48-bit timestamp + 12-bit logical counter + 16-bit node ID
- Monotonic ordering even with clock skew
- Backward compatible (UUIDv7 remains standard)

---

## Document Structure

| Document | Title | Focus | Lines | Status |
|----------|-------|-------|-------|--------|
| **00** | Index (this doc) | Navigation, overview, scope | ~300 | ✅ Complete |
| **01** | UUIDv8 HLC Architecture | Custom ID format with causal ordering | ~1,800 | 🟡 Draft |
| **02** | Leaderless Quorum Replication | Multi-master write model | ~2,000 | 🟡 Draft |
| **03** | Time-Partitioned Merkle Forest | Efficient anti-entropy | ~1,800 | 🟡 Draft |
| **04** | Schema-Driven Colocation | Multi-table atomicity | ~1,400 | 🟡 Draft |
| **05** | MGA Integration | TIP-based distributed visibility | ~1,500 | 🟡 Draft |
| **06** | Implementation Phases | Beta roadmap, 20-24 weeks | ~1,200 | 🟡 Draft |
| **07** | Testing Strategy | Chaos tests, performance benchmarks | ~1,000 | 🟡 Draft |
| **08** | Migration & Operations | Upgrade path, runbooks | ~1,000 | 🟡 Draft |
| **Total** | | | **~12,000** | |

---

## Reading Guide

### For Architects
**Recommended order:**
1. 00_BETA_REPLICATION_INDEX (this document)
2. 01_UUIDV8_HLC_ARCHITECTURE (understand conflict resolution foundation)
3. 02_LEADERLESS_QUORUM_REPLICATION (core multi-master model)
4. 03_TIME_PARTITIONED_MERKLE_FOREST (anti-entropy efficiency)
5. 05_MGA_INTEGRATION (how it integrates with existing TIP/MGA)

### For Implementers
**Recommended order:**
1. Read all documents sequentially (00→08)
2. Reference `/include/scratchbird/core/transaction_manager.h` (actual implementation)
3. Reference `SBCLUSTER-07-REPLICATION.md` (existing primary-replica model)
4. Reference `/MGA_RULES.md` (transaction visibility rules)

### For Security Reviewers
**Focus on:**
- 01_UUIDV8_HLC_ARCHITECTURE (clock skew attacks, Byzantine nodes)
- 02_LEADERLESS_QUORUM_REPLICATION (quorum safety, split-brain prevention)
- 05_MGA_INTEGRATION (cross-transaction visibility isolation)

### For Operations Teams
**Focus on:**
- 06_IMPLEMENTATION_PHASES (deployment strategy, rollout)
- 07_TESTING_STRATEGY (failure scenarios, recovery procedures)
- 08_MIGRATION_OPERATIONS (upgrade from Alpha, monitoring)

---

## Key Concepts

### 1. UUIDv8 with Hybrid Logical Clock (HLC)

**Problem:** Random UUIDs (v4) cause B-tree fragmentation. Time-ordered UUIDs (v7) solve that, but need logical counters for conflict resolution in multi-master replication.

**Solution:** RFC 9562 UUIDv8 (custom format) with embedded HLC.

```
UUIDv8-HLC Structure (128 bits):
┌─────────────────────────────────────────────────────────┐
│ Bits 0-47:   unix_ts_ms (48 bits)  ← Physical timestamp │
│ Bits 48-51:  version=8 (4 bits)    ← RFC 9562 v8        │
│ Bits 52-63:  hlc_counter (12 bits) ← Logical counter    │
│ Bits 64-65:  variant=10 (2 bits)   ← RFC 9562 variant   │
│ Bits 66-81:  node_id (16 bits)     ← Cluster node ID    │
│ Bits 82-127: random (46 bits)      ← Entropy            │
└─────────────────────────────────────────────────────────┘

Key Properties:
  - Time-ordered (B-tree friendly)
  - Causal ordering (HLC counter)
  - Node-unique (node_id prevents collisions)
  - Conflict resolution (compare timestamp, then counter)
```

**Conflict Resolution Example:**
```
Node A: Write @T=1000ms, counter=5
Node B: Write @T=1000ms, counter=7 (received A's write first)

Winner: Node B (higher counter at same timestamp)
Result: Deterministic, no clock skew issues
```

### 2. Leaderless Quorum Replication (Mode 3)

**Extends SBCLUSTER-07 with third replication mode:**

```
Mode 1: Logical Replication (Primary-Replica, commit log streaming)
Mode 2: Physical Replication (Shadow databases, block-level)
Mode 3: Leaderless Quorum (Multi-master, any node writes) ← NEW
```

**Quorum Model:**
```
Configuration: N=3 replicas, W=2 (write quorum), R=2 (read quorum)

Write Path:
  1. Client → Coordinator Node
  2. Coordinator → 3 replicas (parallel RPC)
  3. Wait for W=2 ACKs
  4. Return success to client
  5. Third replica applies asynchronously

Read Path:
  1. Client → Coordinator Node
  2. Coordinator → R=2 replicas (parallel RPC)
  3. If results differ, use UUIDv8-HLC to resolve conflict
  4. Read repair: send winner to stale replica
  5. Return consistent result to client

Quorum Rule: W + R > N (ensures read sees latest write)
  Example: 2 + 2 > 3 ✓ (at least 1 overlap)
```

**Use Cases:**
- Multi-region deployments (write locally, replicate globally)
- High availability (no single primary bottleneck)
- Global applications (low write latency from any region)

### 3. Time-Partitioned Merkle Forest

**Problem:** Traditional Merkle trees (hash-based) cause "repair storms" - every write invalidates hashes up to root, requiring perpetual re-computation.

**Solution:** Time-partitioned trees that seal when time window closes (immutable).

```
Timeline: ──────────────────────────────────────────────▶
         10:00    11:00    12:00    13:00    14:00

Merkle   ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐
Forest   │Tree  │ │Tree  │ │Tree  │ │Tree  │ │Tree  │
         │10-11 │ │11-12 │ │12-13 │ │13-14 │ │14-15 │ ← Active
         └──────┘ └──────┘ └──────┘ └──────┘ └──────┘
            ↓        ↓        ↓        ↓
         SEALED   SEALED   SEALED   SEALED
        (root hash cached, immutable)

Anti-Entropy Protocol:
  1. Exchange sealed tree roots (last 24 hours)
  2. Compare: root_local == root_remote?
  3. If match: No divergence in that hour (skip)
  4. If mismatch: Drill down into that specific hour
  5. Repair: Stream missing records for divergent hours only

Result: Bandwidth proportional to divergence, not dataset size
```

**Performance:**
- Traditional Merkle: 10-20% CPU overhead (constant re-hashing)
- Time-Partitioned: <2% CPU overhead (sealed trees never recomputed)

### 4. Schema-Driven Colocation

**Problem:** ScratchBird has no cross-shard transactions (per SBCLUSTER-00). Multi-table updates require application-level sagas or eventual consistency.

**Solution:** Colocate related tables on same shard via shared partition key.

```sql
-- Define partition key matching foreign key relationship
CREATE TABLE users (
    user_id UUID PRIMARY KEY,  -- UUIDv8
    name VARCHAR(100),
    email VARCHAR(200)
) PARTITION BY user_id;

CREATE TABLE orders (
    order_id UUID PRIMARY KEY,  -- UUIDv8
    user_id UUID NOT NULL REFERENCES users(user_id),
    amount DECIMAL(10,2)
) PARTITION BY user_id;  -- ← Same key as users!

CREATE TABLE order_items (
    item_id UUID PRIMARY KEY,
    order_id UUID NOT NULL REFERENCES orders(order_id),
    product_id UUID,
    quantity INT
) PARTITION BY order_id.user_id;  -- ← Transitively same shard

-- Result: All data for user_id=X lives on SAME shard
-- Multi-table transaction = LOCAL transaction (no 2PC needed)

BEGIN;
  UPDATE users SET email = 'new@example.com' WHERE user_id = 'X';
  UPDATE orders SET status = 'shipped' WHERE user_id = 'X';
  INSERT INTO order_items (order_id, ...) VALUES (...);
COMMIT;  -- Single shard commit, ACID guaranteed
```

**Shard Assignment:**
```
hash(user_id 'X') % 16 = Shard 7

All tables partitioned by user_id:
  users.user_id = 'X'        → Shard 7
  orders.user_id = 'X'       → Shard 7
  order_items.user_id = 'X'  → Shard 7

Transaction touches only Shard 7 → Local TIP commit
```

---

## Integration with Existing Architecture

### SBCLUSTER-07 (Existing Replication)

**Current:** Primary-Replica model (Mode 1: Logical, Mode 2: Physical)
- One primary per shard accepts writes
- N replicas receive commit log stream
- Raft-coordinated failover

**Beta Addition:** Mode 3 (Leaderless Quorum)
- Any node accepts writes
- W + R > N coordination
- UUIDv8-HLC conflict resolution
- No failover needed (no single primary)

**Compatibility:** All three modes coexist. Users choose per deployment:
- **Single-region HA:** Mode 1 (simple, proven)
- **DR with fast recovery:** Mode 2 (shadow databases)
- **Multi-region global writes:** Mode 3 (leaderless)

### MGA_RULES.md (Firebird Transaction Model)

**Critical:** ScratchBird uses MGA, NOT PostgreSQL MVCC

**MGA Principles:**
- TIP-based visibility (`isVersionVisible()` checks TIP state)
- In-place updates with back versions (not append-only)
- No snapshots (per-transaction state in TIP, not snapshot arrays)
- No write-after log (WAL) for crash recovery (TIP handles crash recovery)

**Beta Replication Integration:**
```cpp
// On replica receiving replicated write
void apply_replicated_write(ReplicatedWrite write) {
    // 1. Determine winner (if conflict exists)
    if (has_local_write(write.key)) {
        UuidV8 local_uuid = get_local_write_uuid(write.key);
        UuidV8 remote_uuid = write.uuid;

        if (!should_apply_remote(local_uuid, remote_uuid)) {
            return;  // Local write wins
        }
    }

    // 2. Apply write to local storage (in-place update + back version)
    storage_engine.apply_write(write.key, write.value);

    // 3. Update replica's TIP with remote transaction state
    transaction_manager.updateTipFromRemote(
        write.transaction_id,
        TransactionState::COMMITTED,
        write.commit_timestamp
    );

    // 4. Visibility now handled by local TIP (MGA semantics preserved)
}
```

### SBCLUSTER-05 (Sharding)

**Current:** Hash-based shard assignment
```cpp
shard_id = hash(partition_key) % num_shards;
```

**Beta Enhancement:** Colocation-aware assignment
```cpp
shard_id = hash_with_colocation(table_id, partition_key, catalog);
// If table has FK to colocated table, use same hash
// Otherwise, normal hash
```

### UUIDv7 (Existing) vs UUIDv8 (Beta)

**UUIDv7:** Remains standard RFC 9562 implementation
- Use for: Single-node, primary-replica (Mode 1/2), non-replicated tables
- Properties: Time-ordered, 48-bit timestamp + random entropy

**UUIDv8:** New Beta implementation with HLC
- Use for: Leaderless quorum (Mode 3), multi-master tables
- Properties: Time-ordered + causal ordering (HLC counter)

**API:**
```cpp
// Existing (Alpha, remains unchanged)
UuidV7Bytes generateUuidV7();  // RFC 9562 v7

// New (Beta)
UuidV8Bytes generateUuidV8WithHLC();  // RFC 9562 v8 (custom format)

// Both are time-ordered, both work in B-trees
// UUIDv8 adds HLC for conflict resolution
```

---

## Beta Implementation Scope (Comprehensive)

### Phase 1: Foundation (Weeks 1-4)
**Deliverables:**
- [ ] UUIDv8 data structure (`struct UuidV8Bytes`)
- [ ] HLC generator (`generateUuidV8WithHLC()`)
- [ ] HLC conflict resolution logic
- [ ] Unit tests (monotonicity, clock skew scenarios)

**Acceptance Criteria:**
- UUIDv8 generation is monotonic (timestamp + counter)
- Clock skew doesn't break ordering (counter increments)
- Conflict resolution is deterministic (same inputs → same winner)

### Phase 2: Leaderless Quorum (Weeks 5-10)
**Deliverables:**
- [ ] Quorum coordinator (W + R > N enforcement)
- [ ] Replica-side write application (TIP updates)
- [ ] Read repair (convergence on reads)
- [ ] Split-brain prevention (generation numbers)
- [ ] Integration tests (W+R>N guarantees, network partitions)

**Acceptance Criteria:**
- W + R > N quorum enforced (reads see latest writes)
- Network partition: majority continues, minority blocks
- Conflict resolution: UUIDv8-HLC winner applied consistently
- Read repair: stale replicas converge within 1 second

### Phase 3: Schema Colocation (Weeks 11-14)
**Deliverables:**
- [ ] Parser extensions (`PARTITION BY` clause)
- [ ] Catalog metadata (colocation relationships)
- [ ] Shard assignment with FK awareness
- [ ] Multi-table transaction tests (atomicity verification)

**Acceptance Criteria:**
- Colocated tables assigned to same shard (verify shard_id)
- Multi-table transaction: single TIP commit (no 2PC)
- Cross-shard query: multiple shard coordinators (no atomicity)

### Phase 4: Time-Partitioned Merkle (Weeks 15-20)
**Deliverables:**
- [ ] Merkle tree data structure (nodes, leaves, roots)
- [ ] Hourly sealing logic (immutable roots)
- [ ] Anti-entropy protocol (root comparison, drill-down)
- [ ] Divergence repair (stream missing records)
- [ ] Performance tests (CPU overhead <2%)

**Acceptance Criteria:**
- Sealed trees never recomputed (immutable roots)
- Divergence detection: O(1) per sealed hour
- Repair bandwidth proportional to differences (not dataset size)
- CPU overhead <2% (vs 10-20% for hash-based Merkle)

### Phase 5: Integration & Hardening (Weeks 21-24)
**Deliverables:**
- [ ] End-to-end integration tests (all features together)
- [ ] Chaos testing (random failures, network partitions)
- [ ] Performance benchmarking (latency, throughput, overhead)
- [ ] Operations runbooks (failure recovery, monitoring)
- [ ] User documentation (configuration, best practices)

**Acceptance Criteria:**
- All functional tests pass (100% pass rate)
- Chaos tests: cluster survives cascading failures
- Performance targets met (see Success Criteria below)
- Documentation complete (installation to operations)

**Total:** 24 weeks (~6 months) to Beta-complete cluster replication

---

## Success Criteria (Beta Release)

### Functional Requirements (ALL MANDATORY)

**Core Features:**
- [ ] Leaderless quorum replication operational (Mode 3)
- [ ] UUIDv8-HLC conflict resolution deterministic
- [ ] Time-partitioned Merkle forests reduce anti-entropy overhead
- [ ] Schema colocation enables multi-table local transactions
- [ ] All three replication modes coexist (Mode 1, 2, 3)

**Distributed Guarantees:**
- [ ] W + R > N quorum enforced (reads see latest writes)
- [ ] Network partition resilience (majority continues)
- [ ] Split-brain prevention (generation numbers + fencing)
- [ ] No cross-transaction data leakage (MGA isolation preserved)
- [ ] Crash recovery via TIP (no write-after log (WAL) replay)

**Operational:**
- [ ] Monitoring: replication lag, quorum health, anti-entropy progress
- [ ] Alerting: quorum failures, high conflict rate, excessive lag
- [ ] Runbooks: node failure, network partition, conflict tuning
- [ ] Documentation: installation, configuration, troubleshooting

### Performance Targets (Beta)

| Metric | Target | Configuration |
|--------|--------|---------------|
| **Write Latency (Same DC)** | < 15ms (P95) | W=2, R=1, N=3, leaderless mode |
| **Write Latency (Cross-Region)** | < 50ms (P95) | US-East → US-West, W=2 |
| **Write Throughput** | 50K TPS/shard | W=2, R=1, N=3, async apply |
| **Read Latency (No Conflicts)** | < 5ms (P95) | R=1, single replica query |
| **Read Latency (With Conflicts)** | < 10ms (P95) | R=2, HLC conflict resolution |
| **Replication Lag** | < 100ms (P99) | Same DC, async mode |
| **Anti-Entropy CPU Overhead** | < 2% | Time-partitioned Merkle |
| **Conflict Rate** | < 0.1% | Multi-region, concurrent writes |

### Quality Gates (ALL REQUIRED)

**Testing:**
- [ ] Unit tests: 100% coverage for UUIDv8, HLC, quorum coordinator
- [ ] Integration tests: 100% coverage for replication modes (1, 2, 3)
- [ ] Chaos tests: 100 random failure scenarios, cluster survives all
- [ ] Performance tests: All targets met (see table above)
- [ ] Security audit: No cross-shard data leakage, TIP isolation verified

**Documentation:**
- [ ] User guide: Installation, configuration, best practices
- [ ] Operations guide: Monitoring, alerting, failure recovery
- [ ] API documentation: UUIDv8 generation, replication APIs
- [ ] Architecture docs: All 8 specifications reviewed and approved

**Code Quality:**
- [ ] No critical bugs (severity 1)
- [ ] No medium bugs (severity 2)
- [ ] All code reviewed (peer + architect)
- [ ] Thread safety verified (lock hierarchy, race conditions)
- [ ] Memory safety verified (valgrind clean)

---

## Dependencies

### Prerequisites (Must Exist Before Beta Implementation)

**From Alpha (Fully Implemented):**
- [x] Transaction Manager with TIP (`transaction_manager.h/cpp` - 2,069 lines)
- [x] MGA visibility (`isVersionVisible()` - TIP-based, not snapshots)
- [x] UUIDv7 generation (`generateUuidV7()` - RFC 9562 compliant)
- [x] Group commit optimization (batched TIP writes)
- [x] 2PC support (PREPARED state, GID tracking)

**From SBCLUSTER Specs (Some Implemented):**
- [x] SBCLUSTER-00: Guiding Principles (no cross-shard transactions)
- [x] SBCLUSTER-01: Raft CCE (configuration management)
- [x] SBCLUSTER-02: Node identity (certificates, membership)
- [ ] SBCLUSTER-05: Sharding (hash-based assignment) - needs colocation extension
- [ ] SBCLUSTER-07: Replication (Mode 1/2 specified, need Mode 3)

### Concurrent Development (Can Proceed in Parallel)

**Related SBCLUSTER Specs:**
- SBCLUSTER-06: Distributed Query (scatter-gather across shards)
- SBCLUSTER-08: Backup/Restore (coordinated snapshots)
- SBCLUSTER-09: Scheduler (distributed job execution)
- SBCLUSTER-10: Observability (metrics, tracing, audit)

---

## Risk Assessment

### High Risk (Mitigation Required)

**R1: Complexity of Leaderless Coordination**
- Risk: Quorum coordination bugs (W + R > N violations)
- Mitigation: Exhaustive testing (all quorum combinations), formal verification
- Contingency: Fall back to Mode 1 (primary-replica) if critical bugs found

**R2: HLC Clock Skew Edge Cases**
- Risk: Large clock skew (>1 second) causes counter overflow
- Mitigation: NTP required (operational requirement), alert on skew >100ms
- Contingency: Manual conflict resolution via admin tool

**R3: Merkle Tree Memory Overhead**
- Risk: Storing 24 hours of sealed trees consumes excessive RAM
- Mitigation: Cache only roots (32 bytes × 24 = 768 bytes), serialize trees to disk
- Contingency: Reduce retention (12 hours instead of 24)

### Medium Risk (Monitor Closely)

**R4: Network Partition Duration**
- Risk: Extended partition (>1 hour) causes large divergence
- Mitigation: Time-partitioned Merkle limits repair bandwidth
- Acceptance: Large divergences require time to repair (expected behavior)

**R5: Conflict Rate in Multi-Region**
- Risk: High conflict rate (>1%) degrades performance
- Mitigation: Schema colocation reduces conflicts, read repair converges quickly
- Tuning: Adjust quorum (W=3, R=1) for write-heavy workloads

### Low Risk (Acceptable)

**R6: UUIDv8 Adoption**
- Risk: Users confused by two UUID versions (v7 vs v8)
- Mitigation: Clear documentation, default to v7 (v8 opt-in)
- Acceptance: Power users understand trade-offs

---

## References

### Internal Documents (Actual Implementation)
- **`/include/scratchbird/core/transaction_manager.h`** - Transaction system (FULLY IMPLEMENTED)
- **`/src/core/transaction_manager.cpp`** - Transaction implementation (2,069 lines)
- **`/MGA_RULES.md`** - Firebird MGA architecture rules (**CRITICAL**)
- **`SBCLUSTER-07-REPLICATION.md`** - Existing primary-replica replication
- **`SBCLUSTER-05-SHARDING.md`** - Hash-based shard assignment
- **`SBCLUSTER-00-GUIDING-PRINCIPLES.md`** - No cross-shard transactions

### External Standards
- **RFC 9562:** Universally Unique Identifiers (UUIDs) - UUIDv7 and UUIDv8
- **Hybrid Logical Clocks:** Kulkarni et al. (2014) - [Paper](https://cse.buffalo.edu/tech-reports/2014-04.pdf)
- **Dynamo:** Amazon's leaderless replication - [Paper](https://www.allthingsdistributed.com/files/amazon-dynamo-sosp2007.pdf)
- **Merkle Trees:** Original concept by Ralph Merkle (1979)
- **Firebird MGA:** Firebird 5.0 documentation (transaction model)

### Research Foundation
- **`/docs/specifications/reference/UUIDv7 Replication System Design.md`** - Consensus research
- **Web verification:**
  - [UUIDv7 Database Performance - Bindbee](https://www.bindbee.dev/blog/why-bindbee-chose-uuidv7)
  - [PostgreSQL 18 UUIDv7 Support - Neon](https://neon.com/postgresql/postgresql-18/uuidv7-support)
  - [HLC in YugabyteDB](https://docs.yugabyte.com/preview/architecture/transactions/transactions-overview/)
  - [Anti-Entropy Merkle Trees - System Design School](https://systemdesignschool.io/blog/anti-entropy)

---

## Navigation

### Detailed Specifications
- [01_UUIDV8_HLC_ARCHITECTURE.md](./01_UUIDV8_HLC_ARCHITECTURE.md) - Custom ID format with HLC
- [02_LEADERLESS_QUORUM_REPLICATION.md](./02_LEADERLESS_QUORUM_REPLICATION.md) - Multi-master replication
- [03_TIME_PARTITIONED_MERKLE_FOREST.md](./03_TIME_PARTITIONED_MERKLE_FOREST.md) - Anti-entropy engine
- [04_SCHEMA_DRIVEN_COLOCATION.md](./04_SCHEMA_DRIVEN_COLOCATION.md) - Multi-table atomicity
- [05_MGA_INTEGRATION.md](./05_MGA_INTEGRATION.md) - TIP-based distributed visibility
- [06_IMPLEMENTATION_PHASES.md](./06_IMPLEMENTATION_PHASES.md) - Beta roadmap (24 weeks)
- [07_TESTING_STRATEGY.md](./07_TESTING_STRATEGY.md) - Chaos tests, benchmarks
- [08_MIGRATION_OPERATIONS.md](./08_MIGRATION_OPERATIONS.md) - Upgrade, monitoring

### Related Specifications
- [SBCLUSTER Specification Suite](../../Cluster%20Specification%20Work/)
- [BETA_REPLICATION_ARCHITECTURE_FINDINGS.md](../BETA_REPLICATION_ARCHITECTURE_FINDINGS.md) - Analysis
- [Security Design Specification](../../Security%20Design%20Specification/)
- [MGA Transaction Rules](/MGA_RULES.md) (**CRITICAL**)

---

## Project Context: Educational & No Hard Deadlines

**Important:** This is an **educational project** with no commercial deadlines. Implementation timeline (24 weeks) is a planning estimate, not a requirement.

**Principles:**
- **Correctness over speed** - Take time to understand and implement properly
- **Learning-focused** - Document decisions, trade-offs, and alternatives
- **Quality gates mandatory** - All tests must pass before moving to next phase
- **No shortcuts** - Complete implementation of all Beta features required

**Timeline Flexibility:**
- Estimates provided for planning (24 weeks)
- Actual implementation may take longer (acceptable)
- Prefer thorough understanding over rushing
- Iterate on design if issues discovered

---

**Document Status:** ✅ COMPLETE
**Last Updated:** 2026-01-08
**Next Action:** Create detailed specifications (01-08)
**Approval Required:** Chief Architect

---

**End of Index Document**
