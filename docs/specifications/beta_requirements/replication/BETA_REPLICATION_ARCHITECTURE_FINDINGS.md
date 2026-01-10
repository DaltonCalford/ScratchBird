# Beta Replication Architecture - Implementation Findings & Recommendations

**Date:** 2026-01-08
**Phase:** Alpha Polish → Beta Preparation
**Status:** Architecture Analysis Complete

---

## Executive Summary

Analysis of ScratchBird's **actual implementation** (not outdated specifications) reveals a mature, production-ready transaction system with full MGA/TIP support. This document provides findings and recommendations for Beta replication features that extend the existing architecture.

**Key Finding:** Specifications are OUT OF DATE. The codebase has a fully implemented transaction system (~2,069 lines) with TIP, MGA visibility, group commit, and 2PC support. Beta replication specs should build on this reality, not outdated design docs.

---

## 1. Actual Implementation Status (Code Review)

### 1.1 Transaction Manager (`transaction_manager.h/cpp`)

**Status:** ✅ **FULLY IMPLEMENTED** (2,069 lines of production code)

**Confirmed Features:**
```cpp
class TransactionManager {
    // ✅ TIP (Transaction Inventory Pages) - fully working
    struct TIPPageHeader { ... };  // min_xid, max_xid, num_transactions
    struct TIPEntry { uint64_t xid; uint8_t state; uint64_t commit_time; };

    // ✅ Transaction states (MGA standard)
    enum class TransactionState { ACTIVE, COMMITTED, ABORTED, PREPARED };

    // ✅ Transaction markers (Firebird MGA)
    uint64_t oldest_xid_;          // OIT (Oldest Interesting Transaction)
    uint64_t oldest_active_xid_;   // OAT (Oldest Active Transaction)
    uint64_t oldest_snapshot_;     // OST (Oldest Snapshot Transaction)

    // ✅ MGA visibility (NOT PostgreSQL MVCC)
    auto isVersionVisible(uint64_t version_xid, uint64_t reader_xid) -> bool;

    // ✅ Group commit optimization
    auto performGroupCommit(CommitWaiter* leader, ErrorContext* ctx) -> Status;

    // ✅ 2PC support
    enum TransactionState { PREPARED = 3 };  // Limbo state
    auto prepareTransaction(...) -> Status;
    auto commitPreparedTransaction(const std::string& gid, ...) -> Status;

    // ✅ Thread-safe with documented lock ordering
    // Mutex hierarchy: mutex_ → ProcArray::array_lock → group_commit_mutex_

    // ✅ LRU cache for TIP lookups
    std::unordered_map<uint64_t, TransactionState> transaction_cache_;

    // ✅ XID wraparound protection (64-bit XIDs, no wraparound issues)
    static constexpr uint64_t MAX_SAFE_XID = UINT64_MAX - 1000000;
};
```

**Statistics:**
- Transactions started/committed/aborted
- Read-only transaction optimizations
- Group commit metrics (batches, total XIDs)

### 1.2 UUIDv7 Implementation (`uuidv7.h`)

**Status:** ✅ **SYSTEM DEFAULT** (RFC 9562 compliant)

```cpp
namespace scratchbird::core {
    struct UuidV7Bytes {
        std::array<uint8_t, 16> bytes{};
        auto operator<(const UuidV7Bytes& other) const -> bool;  // Time-ordered!
        auto toString() const -> std::string;
    };

    auto generateUuidV7() -> UuidV7Bytes;  // Time-ordered generation
    using ID = UuidV7Bytes;  // ← System-wide type alias
}
```

**Usage:** All table_id, database_id, node_id = UUIDv7 (time-ordered by default)

### 1.3 Existing Replication (SBCLUSTER-07)

**Status:** ✅ **SPECIFIED** (not yet implemented, but spec is solid)

**Current Design:**
- Per-shard primary-replica topology
- Logical replication (commit log streaming)
- Physical replication (block-level for shadows)
- Raft-coordinated failover with generation numbers
- Async/Sync/Quorum modes

**What's Missing for Multi-Multi:**
- Leaderless write mode (any node accepts writes)
- HLC conflict resolution
- Time-partitioned anti-entropy
- Schema-driven colocation for multi-table atomicity

---

## 2. Gap Analysis: Current vs Research Recommendations

### 2.1 What Exists (Alpha)

| Feature | Status | Implementation |
|---------|--------|----------------|
| **UUIDv7 IDs** | ✅ Working | RFC 9562 compliant, time-ordered |
| **TIP** | ✅ Working | TIPPageHeader, TIPEntry, disk-based |
| **MGA Visibility** | ✅ Working | `isVersionVisible()` using TIP lookups |
| **Group Commit** | ✅ Working | Batching with leader election |
| **2PC** | ✅ Working | PREPARED state, GID tracking |
| **64-bit XIDs** | ✅ Working | No wraparound (UINT64_MAX safe) |
| **Transaction Markers** | ✅ Working | OIT, OAT, OST tracking |
| **Thread Safety** | ✅ Working | Documented lock hierarchy |
| **Primary-Replica Spec** | ✅ Specified | SBCLUSTER-07 (not impl yet) |

### 2.2 What Research Recommends (Beta)

| Feature | Research | Effort | Priority |
|---------|----------|--------|----------|
| **HLC Embedding** | Add 12-bit logical counter to UUIDv7 | Medium | High |
| **Time-Partitioned Merkle** | Hourly sealed trees for anti-entropy | High | Medium |
| **Leaderless Quorum** | W + R > N, any node writes | High | High |
| **Schema Colocation** | Multi-table atomicity via partition key | Medium | High |
| **Split-Plane Control** | Raft for metadata, leaderless for data | Medium | High |

### 2.3 Compatibility with Existing Architecture

**✅ Good Fit:**
1. **HLC Embedding** - Extends existing UUIDv7 generation, no breaking changes
2. **Schema Colocation** - Adds to existing shard assignment (SBCLUSTER-05)
3. **Split-Plane Control** - Aligns with existing Raft CCE (SBCLUSTER-01)

**⚠️ Requires Careful Integration:**
4. **Leaderless Quorum** - Extends SBCLUSTER-07 replication modes (add Mode 3)
5. **Time-Partitioned Merkle** - New anti-entropy mechanism (doesn't replace LSN catch-up)

**❌ Not Recommended for Beta:**
- **Global snapshots** - ScratchBird uses per-shard MGA, no cross-shard snapshots
- **Distributed transactions** - Explicitly avoided per SBCLUSTER-00 (no 2PC across shards)

---

## 3. Beta Replication Architecture Recommendations

### 3.1 Extend SBCLUSTER-07 with Leaderless Mode

**Proposed:** Add **Mode 3: Leaderless Quorum Replication** alongside existing Logical/Physical

```
Current SBCLUSTER-07 Modes:
  Mode 1: Logical Replication (commit log streaming)
  Mode 2: Physical Replication (block-level mirroring)

Beta Addition:
  Mode 3: Leaderless Quorum (any node accepts writes)
          - W + R > N quorum (e.g., W=2, R=2, N=3)
          - HLC-based conflict resolution
          - Use when: Multi-region, no single bottleneck
```

**Implementation Path:**
1. Add `ReplicationMode::LEADERLESS_QUORUM` enum value
2. Extend `ReplicationConfiguration` with quorum settings (W, R, N)
3. Implement quorum coordinator (batches writes to N replicas, waits for W ACKs)
4. Add HLC conflict resolution (embed in commit log records)
5. Integrate with existing TIP (replicas update local TIP on apply)

**Effort:** 4-6 weeks (extends existing code paths)

### 3.2 Add HLC to UUIDv7 Generation

**Proposed:** Extend `generateUuidV7()` to embed 12-bit HLC logical counter

**Current UUIDv7 (RFC 9562):**
```
Bits:
  0-47:  unix_ts_ms (48 bits timestamp)
  48-51: version = 7 (4 bits)
  52-63: random_a (12 bits)
  64-65: variant = 10 (2 bits)
  66-127: random_b (62 bits)
```

**Proposed Enhancement:**
```
Bits:
  0-47:  unix_ts_ms (48 bits timestamp) ← unchanged
  48-51: version = 7 (4 bits) ← unchanged
  52-63: hlc_counter (12 bits) ← NEW (replaces random_a)
  64-65: variant = 10 (2 bits) ← unchanged
  66-81: node_id (16 bits) ← NEW (from cluster membership)
  82-127: random_b (46 bits) ← reduced entropy (still 46 bits)
```

**HLC Generation Logic:**
```cpp
static uint64_t last_ts_ms = 0;
static uint16_t hlc_counter = 0;

UuidV7Bytes generateUuidV7WithHLC() {
    uint64_t now_ms = current_time_milliseconds();

    if (now_ms == last_ts_ms) {
        hlc_counter++;  // Same millisecond → increment logical counter
        if (hlc_counter >= 4096) {
            // Counter overflow → sleep until next millisecond
            sleep_until_next_millisecond();
            now_ms = current_time_milliseconds();
            hlc_counter = 0;
        }
    } else if (now_ms > last_ts_ms) {
        hlc_counter = 0;  // New millisecond → reset counter
        last_ts_ms = now_ms;
    } else {
        // Clock went backwards → use last_ts_ms and increment counter
        now_ms = last_ts_ms;
        hlc_counter++;
    }

    // Embed in UUIDv7 format
    return format_uuidv7(now_ms, hlc_counter, node_id, random_bits);
}
```

**Conflict Resolution:**
```cpp
// When applying replicated write with conflict
UuidV7Bytes local_uuid = ...;   // Local write
UuidV7Bytes remote_uuid = ...;  // Remote write (same key)

// Extract HLC components
auto [local_ts, local_counter] = extract_hlc(local_uuid);
auto [remote_ts, remote_counter] = extract_hlc(remote_uuid);

// Compare: timestamp first, then logical counter
if (remote_ts > local_ts ||
    (remote_ts == local_ts && remote_counter > local_counter)) {
    // Remote write wins
    apply_remote_write();
} else {
    // Local write wins
    keep_local_write();
}
```

**Effort:** 2-3 weeks (modification to existing UUIDv7 generator)

### 3.3 Time-Partitioned Merkle Forests for Anti-Entropy

**Proposed:** Add optional time-partitioned Merkle verification to existing LSN-based catch-up

**Architecture:**
```
Replication Catch-Up (Current):
  - Replica behind by N LSNs
  - Primary streams write-after log (WAL) records from last_lsn to current_lsn
  - Works well for small gaps (<1000 records)

Time-Partitioned Merkle (Beta Addition):
  - For large gaps (>10,000 records or >1 hour behind):
    1. Compare sealed hourly Merkle tree roots
    2. Identify divergent hours (root hash mismatch)
    3. Drill down into divergent hours only
    4. Apply missing records for those hours
  - Reduces bandwidth: O(divergence) not O(dataset)
```

**Merkle Tree Sealing:**
```cpp
struct HourlyMerkleTree {
    timestamp_t hour_start;      // 2026-01-08 14:00:00
    timestamp_t hour_end;        // 2026-01-08 15:00:00
    bytes32 root_hash;           // Merkle root
    bool sealed;                 // True when hour ends
    uint64_t lsn_range_start;    // First LSN in this hour
    uint64_t lsn_range_end;      // Last LSN in this hour
};

// Every hour, seal the previous hour's tree
void seal_hourly_merkle_tree() {
    HourlyMerkleTree* prev_hour = get_previous_hour_tree();
    prev_hour->sealed = true;
    prev_hour->root_hash = compute_merkle_root(prev_hour);

    // Cache root hash (immutable now)
    merkle_root_cache_[prev_hour->hour_start] = prev_hour->root_hash;
}

// Anti-entropy protocol
void anti_entropy_check(UUID replica_uuid) {
    // 1. Get replica's sealed tree roots
    auto replica_roots = rpc_client.get_merkle_roots(replica_uuid);

    // 2. Compare with local sealed tree roots
    for (auto& [hour, local_root] : merkle_root_cache_) {
        bytes32 replica_root = replica_roots[hour];

        if (local_root != replica_root) {
            // Divergence detected in this hour
            log_info("Divergence in hour {}: replicating missing records", hour);

            // 3. Stream write-after log (WAL) records for this hour only
            stream_wal_range(replica_uuid, hour_start_lsn, hour_end_lsn);
        }
    }
}
```

**Effort:** 6-8 weeks (new subsystem, integrates with existing replication)

### 3.4 Schema-Driven Colocation (Parser Extension)

**Proposed:** Allow `PARTITION BY` on multiple tables with same key for local multi-table transactions

**SQL Syntax:**
```sql
-- Define partition key for table
CREATE TABLE users (
    user_id UUID PRIMARY KEY,
    name VARCHAR(100),
    email VARCHAR(200)
) PARTITION BY user_id;  -- ← Partition key

CREATE TABLE orders (
    order_id UUID PRIMARY KEY,
    user_id UUID NOT NULL REFERENCES users(user_id),
    amount DECIMAL(10,2)
) PARTITION BY user_id;  -- ← Same partition key as users!

-- Result: All data for user_id=X lives on same shard
-- Transaction updating both tables = LOCAL transaction (no 2PC needed)
```

**Shard Assignment:**
```cpp
// Current (SBCLUSTER-05): Hash-based
shard_id = hash(partition_key_value) % num_shards;

// Beta Enhancement: Respect colocation
// If `orders.user_id = 123` → assign to same shard as `users.user_id = 123`

uint16_t assign_shard_colocated(UUID table_id, const TypedValue& partition_key_value) {
    // 1. Look up partition key for this table
    ColumnInfo pk_col = catalog.get_partition_key_column(table_id);

    // 2. If foreign key to another table with same partition key:
    if (pk_col.is_foreign_key) {
        UUID referenced_table = pk_col.foreign_key_table;
        if (catalog.has_same_partition_key(table_id, referenced_table)) {
            // Use same hash as referenced table
            return hash(partition_key_value) % num_shards;
        }
    }

    // 3. Otherwise, normal hash-based assignment
    return hash(partition_key_value) % num_shards;
}
```

**Effort:** 3-4 weeks (parser + catalog extensions)

---

## 4. Recommended Beta Replication Specification Structure

### 4.1 Directory Layout

```
/docs/specifications/beta_requirements/replication/
├── 00_BETA_REPLICATION_INDEX.md (navigation, overview)
├── 01_LEADERLESS_QUORUM_REPLICATION.md (Mode 3 addition)
├── 02_HLC_UUIDV7_INTEGRATION.md (conflict resolution)
├── 03_TIME_PARTITIONED_ANTI_ENTROPY.md (Merkle forests)
├── 04_SCHEMA_DRIVEN_COLOCATION.md (multi-table atomicity)
└── 05_MIGRATION_FROM_SBCLUSTER07.md (upgrade path)
```

### 4.2 Specification Priorities

**P0 (Must-Have for Beta):**
1. ✅ **Leaderless Quorum Replication** - Core multi-multi feature
2. ✅ **HLC Embedding in UUIDv7** - Conflict resolution foundation

**P1 (Should-Have for Beta):**
3. ⚠️ **Schema-Driven Colocation** - Enables local multi-table transactions
4. ⚠️ **Time-Partitioned Anti-Entropy** - Performance optimization

**P2 (Nice-to-Have, Can Defer to GA):**
5. ⏳ **Advanced quorum tuning** (per-table W/R/N settings)
6. ⏳ **Cross-region latency optimization** (regional coordinators)

### 4.3 Integration Points with Existing Specs

**Must Reference:**
- ✅ **SBCLUSTER-07** (extend, not replace)
- ✅ **SBCLUSTER-05** (shard assignment for colocation)
- ✅ **SBCLUSTER-01** (Raft CCE for replication config changes)
- ✅ **MGA_RULES.md** (TIP-based visibility, no MVCC contamination)
- ✅ **transaction_manager.h** (actual implementation, not outdated specs)

**Must NOT Reference:**
- ❌ **TRANSACTION_MAIN.md** status claim ("MOSTLY NOT IMPLEMENTED") - OUT OF DATE
- ❌ **PostgreSQL MVCC patterns** (snapshots, CLOG, write-after log (WAL) for crash recovery)
- ❌ **Legacy WAL terminology** - use "write-after log (WAL)" or "Commit Log Streaming" instead

---

## 5. Implementation Roadmap (Beta Phase)

### Phase 1: Foundation (Weeks 1-4)
- [ ] **Week 1-2:** HLC embedding in UUIDv7 (`generateUuidV7WithHLC()`)
- [ ] **Week 3-4:** Conflict resolution logic (HLC comparison, winner selection)
- [ ] **Testing:** HLC monotonicity, clock skew scenarios, conflict resolution correctness

### Phase 2: Leaderless Quorum (Weeks 5-10)
- [ ] **Week 5-6:** Quorum coordinator (write to N replicas, wait for W ACKs)
- [ ] **Week 7-8:** Replica-side application (TIP updates, HLC conflict resolution)
- [ ] **Week 9-10:** Failover logic (no promotion needed, any node accepts writes)
- [ ] **Testing:** W+R>N guarantees, network partition scenarios, quorum failures

### Phase 3: Schema Colocation (Weeks 11-14)
- [ ] **Week 11-12:** Parser extensions (`PARTITION BY` clause, foreign key detection)
- [ ] **Week 13:** Catalog extensions (colocation metadata, shard assignment)
- [ ] **Week 14:** Testing (multi-table transactions, atomicity guarantees)

### Phase 4: Anti-Entropy (Weeks 15-20)
- [ ] **Week 15-17:** Time-partitioned Merkle tree implementation (hourly sealing)
- [ ] **Week 18-19:** Anti-entropy protocol (root comparison, drill-down)
- [ ] **Week 20:** Testing (large divergences, incremental repair)

### Phase 5: Integration & Polish (Weeks 21-24)
- [ ] **Week 21:** Integration tests (all features together)
- [ ] **Week 22:** Performance benchmarking (quorum latency, anti-entropy overhead)
- [ ] **Week 23:** Chaos testing (network partitions, node failures)
- [ ] **Week 24:** Documentation (user guides, operations runbooks)

**Total:** 24 weeks (~6 months) to Beta-ready multi-multi replication

---

## 6. Key Architectural Constraints

### 6.1 What ScratchBird DOES (MGA Principles)

✅ **Per-Shard Transactions** - Each shard has independent TIP
✅ **TIP-Based Visibility** - `isVersionVisible()` uses TIP lookups, not snapshots
✅ **In-Place Updates** - Back versions, not append-only
✅ **No Cross-Shard 2PC** - Per SBCLUSTER-00, no distributed transactions across shards
✅ **64-bit XIDs** - No wraparound (UINT64_MAX safe)
✅ **Group Commit** - Batching for performance

### 6.2 What ScratchBird DOES NOT DO (Anti-Patterns)

❌ **PostgreSQL MVCC** - No snapshots, no CLOG, no xmin/xmax arrays
❌ **Write-after log (WAL) for Crash Recovery** - TIP handles crash recovery, not log replay
❌ **Cross-Shard Transactions** - Each shard is independent (no global 2PC)
❌ **Global Snapshots** - Per-shard MGA, not cluster-wide MVCC
❌ **Append-Only Storage** - In-place updates with back versions

### 6.3 Terminology Corrections

| ❌ Incorrect (PostgreSQL) | ✅ Correct (ScratchBird) |
|--------------------------|-------------------------|
| Write-after log (WAL) | Commit Log Streaming (after commit) |
| MVCC | MGA (Multi-Generational Architecture) |
| CLOG | TIP (Transaction Inventory Pages) |
| Snapshot isolation | Statement-level read consistency + TIP |
| xmin/xmax visibility | TIP state lookup |
| Tuple versions | Back versions |

---

## 7. Success Criteria (Beta Replication)

### 7.1 Functional Requirements

- [ ] **Leaderless quorum mode** operational (any node accepts writes, W+R>N enforced)
- [ ] **HLC conflict resolution** deterministic (no clock skew issues)
- [ ] **Schema colocation** enables local multi-table transactions
- [ ] **Time-partitioned anti-entropy** reduces bandwidth vs full LSN catch-up
- [ ] **Cross-region writes** < 50ms latency (P99) with W=2, R=2, N=3
- [ ] **Network partition resilience** (majority continues, minority blocks)

### 7.2 Performance Targets

| Metric | Target | Measurement |
|--------|--------|-------------|
| **Write Latency (W=2)** | < 15ms (P95) | Same datacenter, 3-node cluster |
| **Write Latency (W=2)** | < 50ms (P95) | Cross-region (US-East → US-West) |
| **Write Throughput** | 50K TPS/shard | Leaderless mode, W=2, R=1, N=3 |
| **Anti-Entropy Overhead** | < 2% CPU | Time-partitioned Merkle (vs 10-20% for hash-based) |
| **Replication Lag** | < 100ms (P99) | Same DC, async mode |
| **Conflict Rate** | < 0.1% | Multi-region, concurrent writes to same key |

### 7.3 Operational Requirements

- [ ] **Monitoring:** Replication lag, quorum health, anti-entropy progress
- [ ] **Alerting:** Quorum failures, high conflict rate, excessive lag
- [ ] **Runbooks:** Node failure, network partition, conflict resolution tuning
- [ ] **Documentation:** Migration guide from SBCLUSTER-07, configuration best practices

---

## 8. Open Questions & Decisions Needed

### Q1: Beta Scope - Minimal vs Comprehensive?

**Option A (Minimal):** P0 only (Leaderless + HLC) → 10-12 weeks
**Option B (Comprehensive):** P0 + P1 (add Colocation + Anti-Entropy) → 20-24 weeks

**Recommendation:** Start with Option A, add P1 based on Beta feedback.

### Q2: Compatibility with Existing SBCLUSTER-07?

**Approach:** Mode 3 addition (not replacement)
- Mode 1 (Logical) + Mode 2 (Physical) remain unchanged
- Mode 3 (Leaderless Quorum) added as opt-in feature
- Users choose based on use case (HA → Mode 1, Multi-region → Mode 3)

**Migration Path:**
- Alpha → Beta: No breaking changes (Mode 1/2 work as before)
- Users upgrade replication to Mode 3 when ready

### Q3: HLC Embedding - Breaking Change to UUIDv7?

**Concern:** Changing UUIDv7 format may break compatibility

**Solution:** Add `generateUuidV7WithHLC()` alongside existing `generateUuidV7()`
- Old code continues using standard RFC 9562 UUIDv7
- New replication code uses HLC-enhanced version
- Both are valid UUIDv7 (different entropy distributions, but both sortable)

---

## 9. Conclusion & Next Steps

### Summary

ScratchBird has a **mature, production-ready transaction system** with:
- ✅ Full TIP implementation (2,069 lines of working code)
- ✅ MGA visibility semantics (`isVersionVisible()`)
- ✅ UUIDv7 as system default (time-ordered IDs)
- ✅ Group commit optimization
- ✅ 2PC support for prepared transactions

Beta replication should **extend this foundation**, not reinvent it.

### Immediate Actions (Week 1)

1. ✅ **Update outdated specifications:**
   - Mark TRANSACTION_MAIN.md status as "FULLY IMPLEMENTED"
   - Add note: "See `transaction_manager.h` for actual implementation"

2. ✅ **Create Beta replication index:**
   - `/docs/specifications/beta_requirements/replication/00_BETA_REPLICATION_INDEX.md`
   - Overview, navigation, scope, priorities

3. ⏭️ **Draft P0 specifications:**
   - `01_LEADERLESS_QUORUM_REPLICATION.md` (extends SBCLUSTER-07)
   - `02_HLC_UUIDV7_INTEGRATION.md` (conflict resolution)

### Questions for Alignment

Before creating detailed specifications, please confirm:

1. **Scope:** Option A (Minimal - 10-12 weeks) or Option B (Comprehensive - 20-24 weeks)?
2. **Priority:** Is multi-region global writes needed for Beta, or focus on HA/DR first?
3. **Timeline:** What's the target Beta release date?
4. **Breaking Changes:** Acceptable to modify UUIDv7 generation (add HLC counter)?

---

**Status:** ✅ ANALYSIS COMPLETE
**Next:** Draft Beta replication specifications based on decisions above
**Blocking:** Need scope/priority/timeline confirmation from project leadership

---

**End of Findings Document**
