# ScratchBird Cluster: AI-Handoff Minimal Invariant Summary

**Session-Start Required Reading for AI Assistants**

---

## Purpose

This document contains the **absolute minimum invariants** that MUST be preserved when working on ScratchBird cluster code. If you are an AI assistant asked to modify cluster code, **read this first**.

**Target**: AI coding assistants after session compaction or context loss

**Length**: 2 pages (intentionally brief for fast context loading)

---

## Core Invariants (MUST NEVER VIOLATE)

### 1. Engine Authority Principle

**The ScratchBird engine is the authoritative source of truth.**

```
✓ CORRECT: Cluster adapts to engine semantics (Firebird MGA, types, constraints)
✗ WRONG:   Engine adapts to cluster semantics
```

**Implication**: When engine behavior and cluster behavior conflict, **engine wins**. Cluster MUST conform.

**Critical**: Read `/MGA_RULES.md` before touching transaction code.

---

### 2. Shard-Local MVCC (No Cross-Shard Transactions)

**Each shard maintains independent MVCC state. Transactions MUST NOT span shards.**

```cpp
// ✓ CORRECT: Transaction scoped to one shard
Transaction txn = begin_transaction(shard_id: 5);
txn.execute("UPDATE users SET balance = balance - 100 WHERE id = 123");
txn.commit();  // Commits only on shard 5

// ✗ WRONG: Cross-shard transaction (FORBIDDEN)
Transaction txn = begin_transaction();
txn.execute("UPDATE users SET balance = balance - 100 WHERE id = 123");  // Shard 5
txn.execute("UPDATE accounts SET balance = balance + 100 WHERE id = 456");  // Shard 7
txn.commit();  // INVALID: cannot commit across shards
```

**Rationale**: No distributed transactions = no 2PC complexity = better availability.

**Implication**: Applications use **sagas** or **eventual consistency** for multi-shard operations.

---

### 3. Cluster Configuration Epoch (CCE) Monotonicity

**Epoch numbers MUST be strictly monotonically increasing. Nodes MUST NOT downgrade.**

```cpp
// ✓ CORRECT: Epoch progression
current_epoch: 17
proposed_epoch: 18  // VALID (18 > 17)

// ✗ WRONG: Epoch downgrade (FORBIDDEN)
current_epoch: 18
proposed_epoch: 17  // INVALID (17 < 18) → REJECT
```

**Enforcement**:
```cpp
if (proposed_epoch.epoch_number <= current_epoch.epoch_number) {
    throw InvalidEpochException("Epoch numbers must be monotonically increasing");
}
```

**Rationale**: One-way upgrades ensure security policies only strengthen, never weaken.

---

### 4. Trust Boundary: Restore ≠ Trust

**Certificates, private keys, and CA bundles MUST NEVER be backed up.**

```cpp
// ✓ CORRECT: Backup procedure
backup_shard_data(shard_id);
backup_wal_segments(shard_id);
backup_catalog_metadata();
// Note: NO certificate backup

// ✗ WRONG: Backup includes cryptographic material (FORBIDDEN)
backup_node_private_key();       // WRONG
backup_node_certificate();       // WRONG
backup_cluster_ca_private_key(); // WRONG
```

**Restore Procedure**:
```
1. Restore data from backup
2. Generate NEW CA (new root of trust)
3. Issue NEW node certificates
4. Nodes join as NEW cluster (new cluster UUID)
```

**Rationale**: Restored cluster is a **new cluster** with new trust, not a clone.

---

### 5. Immutable Audit Chain

**Audit events MUST be hash-chained and digitally signed. Events MUST NOT be modified or deleted.**

```cpp
// ✓ CORRECT: Append-only audit
AuditEvent event;
event.event_hash = SHA256(event.canonical_json || prev_event_hash);  // Hash chain
event.signature = Ed25519_sign(node_private_key, event.event_hash);  // Signature
append_audit_event(event);  // Append only

// ✗ WRONG: Modify or delete audit events (FORBIDDEN)
modify_audit_event(event_uuid, new_data);  // WRONG
delete_audit_event(event_uuid);            // WRONG
```

**Verification**:
```cpp
bool verify_audit_chain(vector<AuditEvent> events) {
    bytes prev_hash = {};
    for (AuditEvent& event : events) {
        if (event.prev_event_hash != prev_hash) return false;  // Chain broken
        if (SHA256(event.canonical_json || prev_hash) != event.event_hash) return false;
        if (!Ed25519_verify(node_pubkey, event.event_hash, event.signature)) return false;
        prev_hash = event.event_hash;
    }
    return true;
}
```

**Implication**: Tampering with any audit event breaks the chain and is **immediately detectable**.

---

## Critical Security Requirements (MUST)

### 6. mTLS for All Inter-Node Communication

**All node↔node connections MUST use mutual TLS with certificate validation.**

```cpp
// ✓ CORRECT: Establish mTLS connection
TLSConnection conn = connect_to_peer(peer_uuid);
conn.verify_peer_certificate(cluster_ca);  // MUST validate
if (conn.get_peer_uuid() != expected_peer_uuid) {
    throw AuthenticationException("Peer UUID mismatch");
}

// ✗ WRONG: Unencrypted or unauthenticated connection (FORBIDDEN)
TCPConnection conn = connect_to_peer(peer_ip);  // NO TLS
send_data(conn, data);  // WRONG: plaintext
```

**Requirement**: TLS 1.3, forward secrecy cipher suites, certificate revocation list (CRL) checked.

---

### 7. Raft Consensus for Critical Cluster State

**CCE updates, membership changes, and shard map updates MUST use Raft consensus.**

```cpp
// ✓ CORRECT: Propose CCE update via Raft
ClusterConfigEpoch new_cce = create_next_epoch();
RaftProposal proposal = {.type = PROPOSAL_CCE_UPDATE, .data = new_cce};
raft_propose(proposal);  // Raft leader proposes, quorum commits

// ✗ WRONG: Direct CCE update without Raft (FORBIDDEN)
store_cce_directly(new_cce);  // WRONG: bypasses consensus
broadcast_cce_to_peers(new_cce);  // WRONG: no quorum guarantee
```

**Rationale**: Raft ensures all nodes converge to same configuration (no split-brain).

---

## Data Integrity Requirements (MUST)

### 8. Shard Key Hash Consistency

**Shard assignment MUST use consistent hashing. Same key MUST always route to same shard.**

```cpp
// ✓ CORRECT: Deterministic shard routing
uint16_t calculate_shard_id(TypedValue shard_key, uint16_t num_shards) {
    uint64_t hash = CityHash64(shard_key.serialize());  // Deterministic hash
    return hash % num_shards;
}

// ✗ WRONG: Non-deterministic shard routing (FORBIDDEN)
uint16_t calculate_shard_id_random() {
    return rand() % num_shards;  // WRONG: random routing
}
```

**Implication**: Changing hash function or shard count **requires full data migration** (resharding).

---

### 9. Write-after Log (WAL) Ordering

**Write-after log (WAL) entries MUST be applied in order on replicas.**

```cpp
// ✓ CORRECT: Apply write-after log (WAL) in sequence
for (WALEntry entry : wal_stream) {
    if (entry.lsn != expected_lsn) {
        throw WALSequenceException("Write-after log (WAL) entries out of order");
    }
    apply_wal_entry(entry);
    expected_lsn = entry.lsn + 1;
}

// ✗ WRONG: Apply write-after log (WAL) out of order (FORBIDDEN)
apply_wal_entry(entry_5);
apply_wal_entry(entry_3);  // WRONG: out of order
apply_wal_entry(entry_4);
```

**Rationale**: Out-of-order write-after log (WAL) application corrupts replica state.

---

## Forbidden Operations (MUST NOT)

### 10. No Cross-Shard Foreign Keys

**Foreign key constraints MUST NOT reference rows in different shards.**

```sql
-- ✓ CORRECT: Foreign key within same shard
CREATE TABLE orders (
    order_id INT PRIMARY KEY,
    customer_id INT,
    FOREIGN KEY (customer_id) REFERENCES customers(customer_id)
);
-- Assuming orders and customers are co-located (same shard key)

-- ✗ WRONG: Cross-shard foreign key (FORBIDDEN)
CREATE TABLE orders (
    order_id INT PRIMARY KEY,
    product_id INT,
    FOREIGN KEY (product_id) REFERENCES products(product_id)
);
-- If orders and products are on different shards, FK is INVALID
```

**Enforcement**: Parser or catalog MUST reject cross-shard FK definitions.

---

## Testing Invariants (MUST Verify)

### 11. Raft Leader Election Stability

**Raft MUST elect a leader within 5 seconds of cluster start or leader failure.**

```cpp
TEST(Raft, LeaderElection) {
    auto cluster = create_test_cluster(3, 16);
    cluster.kill_leader();
    ASSERT_TRUE(cluster.wait_for_new_leader(timeout: 5s));
}
```

---

### 12. Audit Chain Verification

**Tampering with any audit event MUST be detected by chain verification.**

```cpp
TEST(Audit, TamperDetection) {
    vector<AuditEvent> events = generate_audit_events(100);
    ASSERT_TRUE(verify_audit_chain(events));  // Valid chain

    events[50].canonical_json = "{\"tampered\": true}";  // Tamper
    ASSERT_FALSE(verify_audit_chain(events));  // Tamper detected
}
```

---

## Quick Checklist for Code Changes

Before committing cluster code, verify:

- [ ] **No cross-shard transactions**: Each transaction scoped to one shard
- [ ] **CCE monotonicity**: Epoch numbers only increase
- [ ] **No key backup**: Backup code never includes certs/keys
- [ ] **Audit immutability**: No audit event modification/deletion
- [ ] **mTLS enforcement**: All inter-node connections use mTLS
- [ ] **Raft for critical state**: CCE/membership changes via Raft
- [ ] **Consistent hashing**: Shard routing is deterministic
- [ ] **Write-after log (WAL) ordering**: Replicas apply write-after log (WAL) in order
- [ ] **No cross-shard FKs**: Foreign keys don't span shards
- [ ] **Tests pass**: Raft election, audit verification tests

---

## When in Doubt

**Read the full specification**:
- `/docs/specifications/Cluster Specification Work/SBCLUSTER-00-GUIDING-PRINCIPLES.md`
- `/docs/specifications/Cluster Specification Work/SBCLUSTER-01-CLUSTER-CONFIG-EPOCH.md`
- `/docs/specifications/Cluster Specification Work/SBCLUSTER-SUMMARY.md`

**Ask the user**: If an invariant is unclear or seems in conflict, **ask** before proceeding.

**Reject dangerous changes**: If a code change would violate an invariant, **refuse and explain why**.

---

## Critical Files (Do Not Modify Without Reading Spec)

| File Pattern | Read First | Reason |
|--------------|-----------|---------|
| `**/raft_*` | SBCLUSTER-01 | Raft consensus critical |
| `**/cce_*` | SBCLUSTER-01 | CCE monotonicity |
| `**/transaction_*` | MGA_RULES.md | MGA semantics |
| `**/shard_*` | SBCLUSTER-05 | Sharding invariants |
| `**/audit_*` | SBCLUSTER-10 | Audit chain integrity |
| `**/backup_*` | SBCLUSTER-08 | Trust boundary |
| `**/certificate_*` | SBCLUSTER-03 | CA policy |

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-01-02 | Initial AI-handoff summary |

---

**Document Status**: CRITICAL (Must Read Before Cluster Code Changes)

**Applies To**: All cluster-related code (`src/cluster/*`, `src/raft/*`, `src/shard/*`, etc.)

**Last Updated**: 2026-01-02

---

**End of AI-Handoff Minimal Invariant Summary**
