# SBCLUSTER-01: Cluster Configuration Epoch (CCE)

**Document ID**: SBCLUSTER-01
**Version**: 1.0
**Status**: Beta Specification
**Date**: January 2026
**Dependencies**: SBCLUSTER-00 (Guiding Principles)

---

## 1. Introduction

### 1.1 Purpose

This document defines the **Cluster Configuration Epoch (CCE)** system, which provides a temporal, append-only, cryptographically sealed configuration stack for ScratchBird clusters. The CCE is the foundational mechanism that ensures all cluster nodes converge to identical configurations and provides an immutable audit trail of all configuration changes.

### 1.2 Scope

This specification covers:
- Epoch record structure and semantics
- Epoch lifecycle (creation, publication, convergence, activation)
- Hash-chaining and cryptographic sealing
- Published vs. Effective epoch states
- Epoch validation and verification
- Error conditions and recovery

### 1.3 Related Documents

| Document ID | Title |
|-------------|-------|
| SBCLUSTER-00 | Guiding Principles |
| SBCLUSTER-02 | Membership and Identity |
| SBCLUSTER-03 | CA Policy |
| SBCLUSTER-04 | Security Bundle |
| SBSEC-08 | Audit and Compliance |

### 1.4 Terminology

| Term | Definition |
|------|------------|
| **CCE** | Cluster Configuration Epoch - immutable configuration snapshot |
| **Epoch Number** | Monotonically increasing identifier (64-bit unsigned integer) |
| **Published Epoch** | Written to control plane log, not yet effective |
| **Effective Epoch** | All enabled members converged, operations permitted |
| **Configuration Bundle** | Complete set of configuration data referenced by epoch |
| **Hash Chain** | Cryptographic linking of epochs via SHA-256 |

---

## 2. Architecture Overview

### 2.1 Conceptual Model

The CCE system implements a **temporal configuration stack** where each epoch represents an immutable point in the cluster's configuration history:

```
┌─────────────────────────────────────────────────────────────────┐
│                    EPOCH TIMELINE                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Epoch 1 ──────► Epoch 2 ──────► Epoch 3 ──────► Epoch 4       │
│  (Genesis)       (CA Change)     (Shard Add)     (Security)     │
│     │                │                │                │         │
│     │                │                │                │         │
│     └────────────────┴────────────────┴────────────────┘         │
│              Hash-chained (SHA-256)                             │
│              Signed by administrators                           │
│              Append-only (no modifications)                     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 State Transitions

Each epoch progresses through defined states:

```
┌──────────────┐
│   DRAFTED    │  Administrator prepares epoch locally
└──────┬───────┘
       │
       ▼
┌──────────────┐
│  PUBLISHED   │  Written to Raft log, visible to all nodes
└──────┬───────┘
       │
       ▼
┌──────────────┐
│  CONVERGING  │  Nodes activate and peer-observe each other
└──────┬───────┘
       │
       ▼
┌──────────────┐
│  EFFECTIVE   │  All enabled members converged, operations allowed
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ SUPERSEDED   │  Newer epoch published, this epoch historical
└──────────────┘
```

---

## 3. Epoch Record Structure

### 3.1 Core Epoch Record

```cpp
struct ClusterConfigEpoch {
    // Identification
    uint64_t epoch_number;               // Monotonic, starts at 1
    UUID cluster_uuid;                   // Cluster identity

    // Hash chain
    bytes32 previous_epoch_hash;         // SHA-256 of previous epoch
                                        // Zero for epoch 1 (genesis)

    // Configuration references (all SHA-256 hashes)
    bytes32 ca_policy_hash;              // SBCLUSTER-03
    bytes32 security_bundle_hash;        // SBCLUSTER-04
    bytes32 shard_map_hash;              // SBCLUSTER-05
    bytes32 replication_policy_hash;     // SBCLUSTER-07
    bytes32 backup_policy_hash;          // SBCLUSTER-08
    bytes32 scheduler_policy_hash;       // SBCLUSTER-09
    bytes32 capability_matrix_hash;      // Optional feature flags

    // Temporal
    timestamp_t created_at;              // UTC timestamp
    timestamp_t published_at;            // When written to Raft log
    timestamp_t effective_at;            // When convergence complete

    // Authority
    UUID[] administrator_uuids;          // Who signed this epoch
    Signature[] administrator_sigs;      // Ed25519 signatures

    // Metadata
    string description;                  // Human-readable change summary
    uint32_t min_node_version;          // Minimum compatible node version

    // State tracking
    EpochState state;                    // PUBLISHED, CONVERGING, EFFECTIVE, SUPERSEDED
};

enum EpochState : uint8_t {
    DRAFTED = 0,        // Local only, not yet published
    PUBLISHED = 1,      // In Raft log, visible
    CONVERGING = 2,     // Nodes activating
    EFFECTIVE = 3,      // All enabled members converged
    SUPERSEDED = 4      // Newer epoch exists
};
```

### 3.2 Configuration Bundle References

Each hash in the epoch record references a **Configuration Bundle**, which is the actual configuration data:

```cpp
struct ConfigurationBundle {
    bytes32 content_hash;        // SHA-256 of canonical_content
    uint64_t epoch_number;       // Which epoch this belongs to
    BundleType type;            // CA_POLICY, SECURITY_BUNDLE, etc.
    bytes canonical_content;     // Canonically serialized content
    timestamp_t created_at;
};

enum BundleType : uint8_t {
    CA_POLICY = 1,
    SECURITY_BUNDLE = 2,
    SHARD_MAP = 3,
    REPLICATION_POLICY = 4,
    BACKUP_POLICY = 5,
    SCHEDULER_POLICY = 6,
    CAPABILITY_MATRIX = 7
};
```

---

## 4. Epoch Lifecycle

### 4.1 Epoch Creation (DRAFTED State)

**Who**: Cluster administrator with `CLUSTER_CONFIG_ADMIN` privilege

**Process**:

```
1. Administrator connects to current cluster leader
2. Fetches current effective epoch (epoch N)
3. Modifies desired configuration components
4. Generates new configuration bundles
5. Computes SHA-256 hashes of each bundle
6. Creates epoch record (epoch N+1) with:
   - epoch_number = current + 1
   - previous_epoch_hash = hash of epoch N
   - Configuration bundle hashes
   - Administrator UUID
7. Signs epoch record with administrator's private key
8. State = DRAFTED (local only, not yet published)
```

**Validation**:
- Epoch number must be exactly `current_epoch_number + 1`
- All referenced configuration bundles must be valid
- Minimum node version check (all nodes must meet minimum)
- Administrator must have `CLUSTER_CONFIG_ADMIN` privilege

### 4.2 Epoch Publication (PUBLISHED State)

**Who**: Cluster administrator (same or different from creator)

**Process**:

```
1. Administrator submits DRAFTED epoch to cluster leader
2. Leader validates:
   a. Epoch number is current + 1
   b. Signatures are valid
   c. previous_epoch_hash matches current epoch
   d. All configuration bundles are available
   e. No other epoch N+1 published
3. Leader writes to Raft log as atomic operation:
   - Epoch record
   - All referenced configuration bundles
4. Raft commits the log entry
5. Epoch state transitions to PUBLISHED
6. published_at timestamp recorded
7. All nodes receive via Raft replication
```

**Raft Log Entry**:

```cpp
struct RaftLogEntry_EpochPublish {
    uint64_t raft_index;
    uint64_t raft_term;

    ClusterConfigEpoch epoch;
    ConfigurationBundle[] bundles;  // All referenced bundles

    // This entire structure is what gets committed
};
```

### 4.3 Epoch Convergence (CONVERGING State)

**Who**: All enabled cluster members

**Process**:

```
For each enabled node:
1. Node receives epoch via Raft replication
2. Node validates epoch structure and signatures
3. Node extracts and validates configuration bundles
4. Node activates configuration components:
   a. CA Policy (SBCLUSTER-03)
   b. Security Bundle (SBCLUSTER-04)
   c. Shard Map (SBCLUSTER-05)
   d. etc.
5. Node performs peer observation (SBCLUSTER-02):
   - Connects to all other enabled nodes via mTLS
   - Verifies they are running same epoch
   - Records observation evidence
6. Node writes "convergence attestation" to Raft log:
   - Own node_uuid
   - epoch_number
   - Timestamp of activation
   - Peer observation results
```

**Convergence Attestation**:

```cpp
struct ConvergenceAttestation {
    UUID node_uuid;
    uint64_t epoch_number;
    timestamp_t activated_at;

    // Peer observations
    struct PeerObservation {
        UUID peer_node_uuid;
        bytes32 peer_cert_hash;
        uint64_t peer_epoch_number;  // Must match
        timestamp_t observed_at;
        bool mTLS_valid;
    }[];

    Signature node_signature;  // Ed25519
};
```

**Convergence Monitoring**:

The cluster leader monitors convergence attestations:
```
If (all enabled members have submitted attestations for epoch N):
    State transitions to EFFECTIVE
    effective_at timestamp recorded
    Write EffectiveEpoch event to Raft log
```

### 4.4 Epoch Effectiveness (EFFECTIVE State)

**Criteria**:

An epoch becomes EFFECTIVE when:
1. All **enabled members** have activated the epoch
2. All enabled members have peer-observed each other at this epoch
3. No activation failures reported
4. Convergence timeout not exceeded

**Operations Requiring Effective Epoch**:

These operations MUST NOT proceed unless epoch is EFFECTIVE:
- Distributed query execution across shards
- Scheduler control commands (QUORUM_REQUIRED jobs)
- CA policy cutover
- Security-critical administrative operations

**Operations Permitted During CONVERGING**:

These operations MAY proceed:
- Single-shard queries (shard-local)
- Read-only operations
- Administrative queries (status checks)
- LOCAL_SAFE scheduler jobs

### 4.5 Epoch Supersession (SUPERSEDED State)

When a new epoch is published:
```
1. Current EFFECTIVE epoch transitions to SUPERSEDED
2. New epoch begins CONVERGING process
3. SUPERSEDED epoch remains in history (never deleted)
4. Audit trail preserved
```

---

## 5. Hash Chain Integrity

### 5.1 Hash Computation

Each epoch's hash is computed over its **canonical representation**:

```cpp
bytes32 compute_epoch_hash(const ClusterConfigEpoch& epoch) {
    // Canonical serialization (deterministic)
    ByteBuffer canonical = serialize_canonical(epoch);

    // SHA-256
    return SHA256(canonical);
}

ByteBuffer serialize_canonical(const ClusterConfigEpoch& epoch) {
    ByteBuffer buf;

    // Fixed-size fields (big-endian)
    buf.append_u64_be(epoch.epoch_number);
    buf.append_bytes(epoch.cluster_uuid.bytes(), 16);
    buf.append_bytes(epoch.previous_epoch_hash, 32);

    // Configuration hashes (deterministic order)
    buf.append_bytes(epoch.ca_policy_hash, 32);
    buf.append_bytes(epoch.security_bundle_hash, 32);
    buf.append_bytes(epoch.shard_map_hash, 32);
    buf.append_bytes(epoch.replication_policy_hash, 32);
    buf.append_bytes(epoch.backup_policy_hash, 32);
    buf.append_bytes(epoch.scheduler_policy_hash, 32);
    buf.append_bytes(epoch.capability_matrix_hash, 32);

    // Timestamps (Unix nanoseconds, big-endian)
    buf.append_i64_be(epoch.created_at.nanos_since_epoch());
    buf.append_i64_be(epoch.published_at.nanos_since_epoch());

    // Administrators (sorted by UUID for determinism)
    auto sorted_admins = epoch.administrator_uuids;
    std::sort(sorted_admins.begin(), sorted_admins.end());
    buf.append_u32_be(sorted_admins.size());
    for (auto& admin_uuid : sorted_admins) {
        buf.append_bytes(admin_uuid.bytes(), 16);
    }

    // Description (UTF-8, length-prefixed)
    buf.append_u32_be(epoch.description.size());
    buf.append_bytes(epoch.description.data(), epoch.description.size());

    // Minimum version
    buf.append_u32_be(epoch.min_node_version);

    return buf;
}
```

### 5.2 Hash Chain Validation

When validating an epoch:

```cpp
bool validate_epoch_hash_chain(const ClusterConfigEpoch& epoch,
                                const ClusterConfigEpoch* previous_epoch) {
    // Epoch 1 (genesis) has no previous
    if (epoch.epoch_number == 1) {
        return epoch.previous_epoch_hash == bytes32::zero();
    }

    // Must have previous epoch
    if (!previous_epoch) {
        return false;
    }

    // Epoch numbers must be sequential
    if (epoch.epoch_number != previous_epoch->epoch_number + 1) {
        return false;
    }

    // Compute hash of previous epoch
    bytes32 computed_hash = compute_epoch_hash(*previous_epoch);

    // Must match what this epoch claims
    return epoch.previous_epoch_hash == computed_hash;
}
```

---

## 6. Signature and Authorization

### 6.1 Signing Authority

Only users with `CLUSTER_CONFIG_ADMIN` privilege may sign epochs.

**Signature Process**:

```cpp
Signature sign_epoch(const ClusterConfigEpoch& epoch,
                     const Ed25519PrivateKey& admin_key) {
    // Compute epoch hash
    bytes32 epoch_hash = compute_epoch_hash(epoch);

    // Sign the hash
    return ed25519_sign(admin_key, epoch_hash);
}
```

### 6.2 Multi-Administrator Signing

For high-security deployments, epochs MAY require multiple administrator signatures:

```cpp
struct EpochSigningPolicy {
    uint32_t min_signatures;        // Minimum required (default: 1)
    UUID[] authorized_signers;      // Allowed administrator UUIDs
    bool require_quorum;            // If true, min_signatures = quorum size
};
```

**Validation**:

```cpp
bool validate_epoch_signatures(const ClusterConfigEpoch& epoch,
                                const EpochSigningPolicy& policy) {
    if (epoch.administrator_uuids.size() < policy.min_signatures) {
        return false;  // Not enough signatures
    }

    // Verify each signature
    bytes32 epoch_hash = compute_epoch_hash(epoch);
    for (size_t i = 0; i < epoch.administrator_uuids.size(); ++i) {
        UUID admin_uuid = epoch.administrator_uuids[i];
        Signature sig = epoch.administrator_sigs[i];

        // Fetch admin's public key from security catalog
        Ed25519PublicKey pubkey = get_admin_public_key(admin_uuid);

        // Verify signature
        if (!ed25519_verify(pubkey, epoch_hash, sig)) {
            return false;
        }

        // Verify admin is authorized
        if (!policy.authorized_signers.contains(admin_uuid)) {
            return false;
        }
    }

    return true;
}
```

---

## 7. Error Conditions and Recovery

### 7.1 Error Types

| Error | Description | Recovery |
|-------|-------------|----------|
| **EPOCH_NUMBER_GAP** | Published epoch number skips value | Reject, administrator must fix |
| **INVALID_HASH_CHAIN** | previous_epoch_hash mismatch | Reject, indicates tampering |
| **SIGNATURE_INVALID** | Administrator signature fails verification | Reject, re-sign required |
| **CONVERGENCE_TIMEOUT** | Nodes fail to converge within deadline | Admin investigation, force cutover |
| **BUNDLE_UNAVAILABLE** | Referenced configuration bundle missing | Retry fetch, escalate if persistent |
| **INCOMPATIBLE_VERSION** | Node version < min_node_version | Node must upgrade before activation |

### 7.2 Convergence Timeout

If nodes fail to converge within the configured timeout (default: 5 minutes):

```
1. Leader logs CONVERGENCE_TIMEOUT event
2. Identifies non-converged nodes
3. Attempts to contact non-converged nodes:
   - If reachable: Query why convergence failed
   - If unreachable: Mark as potentially failed
4. Administrator notified
5. Options:
   a. Wait longer (extend timeout)
   b. Force cutover (mark non-converged nodes as disabled)
   c. Rollback (publish new epoch reverting changes)
```

**Force Cutover**:

```sql
-- Explicit administrative action
ALTER CLUSTER FORCE CUTOVER TO EPOCH <epoch_number>
    DISABLING NODES (<node_uuid>, ...);
```

This is **destructive** and requires explicit acknowledgement.

### 7.3 Split-Brain Prevention

The Raft consensus protocol prevents split-brain at the control plane level. However, epoch enforcement provides an additional safety layer:

**Rule**: If a node detects it's running an epoch different from cluster majority:
```
1. Log EPOCH_MISMATCH event
2. Refuse distributed operations
3. Downgrade to local-only mode
4. Administrator investigation required
```

---

## 8. Implementation Notes

### 8.1 Storage

Epochs and configuration bundles are stored in:
- **Raft Log**: Primary authoritative source
- **Local Cache**: Each node maintains epoch history
- **Audit Archive**: Long-term epoch history for compliance

### 8.2 Performance Considerations

**Bundle Size Limits**:
- CA Policy: ~100 KB
- Security Bundle: ~10 MB (large clusters with many domains/RLS policies)
- Shard Map: ~1 MB
- Total epoch size: ~15 MB max

**Raft Log Impact**:
- Large epochs increase log size
- Implement log compaction after epochs are EFFECTIVE
- Retain minimum: last 10 epochs + current

### 8.3 Audit Trail

Every epoch change generates audit events:
- Epoch created
- Epoch published
- Node activated epoch
- Epoch became effective
- Epoch superseded

These events include full epoch hash for verification.

---

## 9. Example Workflows

### 9.1 CA Policy Rotation

```
Current: Epoch 42 (CA_A)
Goal: Rotate to CA_B

Step 1: Administrator creates overlap epoch
  Epoch 43:
    - ca_policy_hash = hash(CA_A + CA_B overlap policy)
    - All other hashes unchanged from epoch 42

Step 2: Publish epoch 43
  - Raft commits
  - All nodes activate overlap policy
  - Nodes begin accepting CA_A and CA_B certificates

Step 3: Wait for convergence
  - Monitor attestations
  - Epoch 43 becomes EFFECTIVE

Step 4: Stability window (configurable, default: 1 hour)
  - Verify all nodes healthy at epoch 43

Step 5: Administrator creates cutover epoch
  Epoch 44:
    - ca_policy_hash = hash(CA_B only policy)
    - All other hashes unchanged from epoch 43

Step 6: Publish epoch 44
  - All nodes switch to CA_B only
  - CA_A certificates no longer accepted
  - Epoch 44 becomes EFFECTIVE

Done: CA rotation complete with zero downtime
```

---

## 10. References

### 10.1 External Standards

- [Raft Consensus Algorithm](https://raft.github.io/) - Consensus protocol
- [Ed25519 Signatures](https://ed25519.cr.yp.to/) - Digital signatures
- [SHA-256](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf) - Hash algorithm

### 10.2 Related Specifications

- SBCLUSTER-02: Membership and Identity
- SBCLUSTER-03: CA Policy
- SBCLUSTER-04: Security Bundle
- SBSEC-08: Audit and Compliance

---

**End of SBCLUSTER-01**
