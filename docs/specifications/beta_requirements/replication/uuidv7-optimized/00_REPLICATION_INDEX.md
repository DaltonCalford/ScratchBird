# ScratchBird Beta Replication Specification Index
## UUIDv7-Optimized Multi-Multi Table Replication

**Status:** BETA SPECIFICATION
**Version:** 1.0
**Date:** 2026-01-08
**Authority:** Chief Architect

---

## Executive Summary

This specification suite defines ScratchBird's Beta-phase replication architecture, leveraging UUIDv7's temporal ordering properties for high-performance, low-overhead distributed replication. The design implements consensus best practices from modern distributed systems (2026) while maintaining compatibility with ScratchBird's Firebird MGA transaction model and per-shard architecture.

**Key Innovation:** Time-Partitioned Merkle Forests that align verification windows with LSM compaction cycles, reducing anti-entropy overhead from stochastic "repair storms" to deterministic, lightweight operations.

---

## Architecture Philosophy

### The "No Technical Debt" Paradigm

ScratchBird is a greenfield database built in 2026 with no legacy constraints. This specification rejects accumulated technical debt from prior generations:

**What We Reject:**
- ❌ Random-distribution models (Cassandra-style consistent hashing)
- ❌ Hash-based Merkle trees (perpetual re-computation storms)
- ❌ Physical-clock-only conflict resolution (clock skew vulnerabilities)
- ❌ Global 2PC for multi-table transactions (availability killer)
- ❌ Snapshot-based visibility (PostgreSQL MVCC overhead)

**What We Embrace:**
- ✅ Temporal locality (UUIDv7 time-ordered identifiers)
- ✅ Time-partitioned verification structures (sealed immutable trees)
- ✅ Hybrid Logical Clocks (HLC) embedded in UUIDv7 for causal ordering
- ✅ Schema-driven colocation (related data on same shards)
- ✅ Firebird MGA visibility (TIP-based, no snapshots)
- ✅ Split-plane consensus (Raft for control, Leaderless for data)

---

## Document Structure

This specification is organized into focused documents:

| Document | Title | Focus | Lines | Status |
|----------|-------|-------|-------|--------|
| **00** | Index (this doc) | Navigation & overview | ~200 | ✅ Complete |
| **01** | Core Architecture | Split-plane, hybrid consensus | ~1,500 | 🟡 Draft |
| **02** | UUIDv7 Integration | Identifier structure, time ordering | ~1,200 | 🟡 Draft |
| **03** | Time-Partitioned Merkle Forest | Anti-entropy verification | ~1,800 | 🟡 Draft |
| **04** | Hybrid Logical Clocks | Conflict resolution, causality | ~1,500 | 🟡 Draft |
| **05** | Multi-Table Replication | Schema colocation, local transactions | ~1,400 | 🟡 Draft |
| **06** | MGA Integration | Firebird TIP visibility, GC coordination | ~1,300 | 🟡 Draft |
| **07** | Implementation Phases | Beta roadmap, dependencies | ~1,000 | 🟡 Draft |
| **08** | Testing Strategy | Chaos tests, verification | ~800 | 🟡 Draft |
| **Total** | | | **~10,700** | |

---

## Reading Guide

### For Architects
**Recommended order:**
1. 00_REPLICATION_INDEX (this document)
2. 01_CORE_ARCHITECTURE (understand split-plane model)
3. 03_TIME_PARTITIONED_MERKLE_FOREST (key innovation)
4. 04_HYBRID_LOGICAL_CLOCKS (conflict resolution)
5. 05_MULTI_TABLE_REPLICATION (consistency model)

### For Implementers
**Recommended order:**
1. Read all documents sequentially (00→08)
2. Reference `/docs/specifications/Cluster Specification Work/SBCLUSTER-07-REPLICATION.md` for per-shard write-after log (WAL) streaming
3. Reference `/docs/specifications/beta_requirements/replication/REPLICATION_AND_SHADOW_PROTOCOLS.md` for shadow databases
4. Reference `/MGA_RULES.md` for transaction visibility integration

### For Security Reviewers
**Focus on:**
- 01_CORE_ARCHITECTURE (trust model, fencing)
- 04_HYBRID_LOGICAL_CLOCKS (clock skew attacks, Byzantine failures)
- 06_MGA_INTEGRATION (visibility isolation, cross-transaction leakage)

### For Operations Teams
**Focus on:**
- 07_IMPLEMENTATION_PHASES (deployment strategy)
- 08_TESTING_STRATEGY (failure scenarios, recovery procedures)
- Integration with SBCLUSTER-10 (observability)

---

## Key Concepts

### 1. Split-Plane Architecture

```
┌─────────────────────────────────────────────────────┐
│               CONTROL PLANE (Raft)                  │
│  - Catalog & Schema (strong consistency)            │
│  - Cluster topology & membership                    │
│  - Security bundle distribution                     │
│  - Shard map & replication config                   │
└─────────────────────────────────────────────────────┘
                         │
         ┌───────────────┼───────────────┐
         │               │               │
┌────────▼────────┐ ┌────▼───────┐ ┌────▼───────┐
│   DATA PLANE    │ │ DATA PLANE │ │ DATA PLANE │
│  (Leaderless)   │ │(Leaderless)│ │(Leaderless)│
│                 │ │            │ │            │
│  Shard 001-005  │ │ Shard 6-10 │ │ Shard 11-16│
│  Row data (UUIDv7)│ │   (UUIDv7) │ │   (UUIDv7) │
│  Gossip protocol│ │   Gossip   │ │   Gossip   │
└─────────────────┘ └────────────┘ └────────────┘
```

### 2. UUIDv7 Structure (RFC 9562)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
├─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┤
│         unix_ts_ms (48 bits)          │ ver │ hlc_counter(12) │
├─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┤
│v│      node_id (16)      │          entropy (46)              │
├─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┤

Key Properties:
  - Monotonically increasing (sortable by time)
  - HLC counter embedded (causality tracking)
  - Node ID prevents collisions
  - Entropy ensures uniqueness
```

### 3. Time-Partitioned Merkle Forest

```
Timeline: ───────────────────────────────────────▶
         10:00   10:01   10:02   10:03   10:04

Merkle   ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐
Forest   │Tree │ │Tree │ │Tree │ │Tree │ │Tree │
         │ 00  │ │ 01  │ │ 02  │ │ 03  │ │ 04  │ ← Active (mutable)
         └─────┘ └─────┘ └─────┘ └─────┘ └─────┘
           ↓       ↓       ↓       ↓
         SEALED SEALED SEALED SEALED
        (immutable roots cached)

Anti-Entropy:
  1. Compare root hashes for sealed trees (O(1) per tree)
  2. If mismatch, drill down into that specific tree
  3. Active tree verified via read repair (frequent, approximate)
```

### 4. Hybrid Logical Clock Conflict Resolution

```
Event Timeline:
  Node A: Write @10:05:120 (wall clock)
  Node B: Write @10:05:115 (wall clock, but B's clock is slow)

Without HLC:
  Node A UUID: unix_ts=10:05:120, counter=0
  Node B UUID: unix_ts=10:05:115, counter=0
  Result: A wins (higher timestamp) ← WRONG if B happened after A

With HLC:
  Node A receives B's write:
    - B's timestamp (115) < A's clock (120)
    - Increment A's logical counter
    - A UUID: unix_ts=120, counter=1
  Node B receives A's write:
    - A's timestamp (120) > B's clock (115)
    - Update B's clock to 120
    - Increment B's logical counter
    - B UUID: unix_ts=120, counter=1

  Result: Causal ordering preserved, clock skew doesn't matter
```

### 5. Schema-Driven Colocation

```sql
-- Define partition key for multi-table atomicity
CREATE TABLE users (
    user_id UUID PRIMARY KEY,  -- UUIDv7
    name VARCHAR(100),
    email VARCHAR(200)
) PARTITION BY user_id;

CREATE TABLE orders (
    order_id UUID PRIMARY KEY,  -- UUIDv7
    user_id UUID NOT NULL,      -- Foreign key
    amount DECIMAL(10,2)
) PARTITION BY user_id;  -- Same partition key as users!

-- Result: All data for user_id=X lives on the same shard
-- Transaction updating both tables = local transaction (no 2PC)
```

---

## Integration Points

### With Existing ScratchBird Architecture

**SBCLUSTER-07 (Replication):**
- Extends per-shard write-after log (WAL) streaming with UUIDv7-aware verification
- Adds time-partitioned Merkle forest for efficient anti-entropy
- Maintains primary-replica model, enhances with leaderless option

**MGA_RULES.md (Firebird Transactions):**
- TIP-based visibility integrates with HLC timestamps
- No snapshot overhead (MGA principle preserved)
- GC coordination with time-partitioned trees

**SBCLUSTER-05 (Sharding):**
- Shard assignment uses UUIDv7 time component for locality
- Colocation rules for multi-table transactions
- Shard migration preserves time-ordering

**Security Design Specification:**
- Replication streams encrypted (TLS 1.3)
- Certificate-based node authentication
- Audit chain integration (replication events logged)

---

## Success Criteria

### Functional Requirements (Beta)
- [ ] Time-partitioned Merkle forest operational for 1M+ rows per shard
- [ ] HLC embedded in UUIDv7, conflict resolution deterministic
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

---

## Implementation Timeline (Beta)

**Phase 1: Foundation (Weeks 1-4)**
- UUIDv7 generator with HLC embedding
- Time-partitioned Merkle tree implementation
- Integration with existing catalog (table_id, row_id as UUIDv7)

**Phase 2: Anti-Entropy (Weeks 5-8)**
- Gossip protocol for sealed tree root exchange
- Drill-down verification for divergent trees
- Read repair for active (mutable) trees

**Phase 3: Multi-Table (Weeks 9-12)**
- Schema-driven colocation parser extensions
- Local transaction coordinator for colocated tables
- Saga pattern for cross-partition transactions (optional)

**Phase 4: Testing & Hardening (Weeks 13-16)**
- Chaos testing (node failures, network partitions)
- Performance benchmarking
- Security audit
- Documentation

**Total:** 16 weeks (4 months) to Beta-ready replication

---

## Dependencies

### Prerequisites (Must Complete Before Beta Replication)
- [ ] Alpha Parser Remediation (P-001 to P-010) ← **BLOCKING**
- [ ] UUIDv7 library integration (RFC 9562 compliant)
- [ ] SBCLUSTER-01 through SBCLUSTER-04 (cluster foundation)
- [ ] Per-shard write-after log (WAL) streaming (SBCLUSTER-07 base implementation)

### Concurrent Development (Can Proceed in Parallel)
- SBCLUSTER-08 (Backup/Restore) - shares time-partitioned Merkle trees
- SBCLUSTER-09 (Scheduler) - uses replication topology for job distribution
- SBCLUSTER-10 (Observability) - monitors replication lag, anti-entropy metrics

---

## Risk Assessment

### High Risk
1. **Complexity of HLC Integration**
   - Mitigation: Start with simple Last-Write-Wins (LWW), add HLC incrementally
   - Fallback: Use physical timestamps only for Beta, HLC in GA

2. **Time-Partitioned Tree Memory Overhead**
   - Mitigation: Sealed trees serialized to disk, only roots cached
   - Fallback: Increase sealing interval (trade freshness for memory)

### Medium Risk
3. **Schema Colocation User Experience**
   - Mitigation: Auto-detect FK relationships, suggest colocation
   - Fallback: Document as best practice, don't enforce

4. **Cross-Shard Query Performance**
   - Mitigation: Parallel execution, push-down optimization
   - Acceptance: Multi-shard queries inherently slower (expected)

### Low Risk
5. **Clock Skew in HLC**
   - Mitigation: NTP synchronization required (document as operational requirement)
   - Detection: Alert if HLC counter grows beyond threshold (indicates skew)

---

## References

### Research Foundation
- `/docs/specifications/reference/UUIDv7 Replication System Design.md` (consensus research)
- [RFC 9562: UUIDv7 Specification](https://www.rfc-editor.org/rfc/rfc9562.html)
- [Hybrid Logical Clocks Paper](https://cse.buffalo.edu/tech-reports/2014-04.pdf) (Kulkarni et al.)

### Web Verification
- [Ed25519 + Merkle Tree + UUIDv7 - DEV Community](https://dev.to/veritaschain/ed25519-merkle-tree-uuidv7-building-tamper-proof-decision-logs-o1e)
- [UUIDv7 Database Performance - Bindbee](https://www.bindbee.dev/blog/why-bindbee-chose-uuidv7)
- [PostgreSQL 18 UUIDv7 Support - Neon](https://neon.com/postgresql/postgresql-18/uuidv7-support)
- [Hybrid Logical Clocks - YugabyteDB](https://docs.yugabyte.com/preview/architecture/transactions/transactions-overview/)
- [Anti-Entropy Merkle Trees - System Design School](https://systemdesignschool.io/blog/anti-entropy)
- [HLC in Distributed Systems - Medium](https://medium.com/geekculture/all-things-clock-time-and-order-in-distributed-systems-hybrid-logical-clock-in-depth-7c645eb03682)

### Internal Documents
- `/docs/specifications/Cluster Specification Work/SBCLUSTER-07-REPLICATION.md`
- `/docs/specifications/beta_requirements/replication/REPLICATION_AND_SHADOW_PROTOCOLS.md`
- `/MGA_RULES.md` (Firebird transaction visibility)
- `/docs/specifications/TRANSACTION_MGA_CORE.md`

### Industry Examples
- **CockroachDB**: Hybrid logical clocks for distributed transactions
- **YugabyteDB**: HLC + DocDB for multi-region consistency
- **Cassandra**: Merkle trees for anti-entropy (but hash-based, not time-partitioned)
- **Amazon Dynamo**: Leaderless replication, vector clocks (precursor to HLC)

---

## Navigation

### Detailed Specifications
- [01_CORE_ARCHITECTURE.md](./01_CORE_ARCHITECTURE.md) - Split-plane, consensus model
- [02_UUIDV7_INTEGRATION.md](./02_UUIDV7_INTEGRATION.md) - Identifier structure, generation
- [03_TIME_PARTITIONED_MERKLE_FOREST.md](./03_TIME_PARTITIONED_MERKLE_FOREST.md) - Anti-entropy engine
- [04_HYBRID_LOGICAL_CLOCKS.md](./04_HYBRID_LOGICAL_CLOCKS.md) - Conflict resolution
- [05_MULTI_TABLE_REPLICATION.md](./05_MULTI_TABLE_REPLICATION.md) - Schema colocation
- [06_MGA_INTEGRATION.md](./06_MGA_INTEGRATION.md) - Firebird MGA visibility
- [07_IMPLEMENTATION_PHASES.md](./07_IMPLEMENTATION_PHASES.md) - Beta roadmap
- [08_TESTING_STRATEGY.md](./08_TESTING_STRATEGY.md) - Verification, chaos tests

### Related Specifications
- [SBCLUSTER Specification Suite](../../Cluster%20Specification%20Work/)
- [Security Design Specification](../../Security%20Design%20Specification/)
- [MGA Transaction Rules](/MGA_RULES.md)

---

**Document Status:** DRAFT (Beta Specification Phase)
**Last Updated:** 2026-01-08
**Next Review:** Weekly during Beta development
**Approval Required:** Chief Architect, Distributed Systems Lead, Storage Engineering Lead

---

**End of Index Document**
