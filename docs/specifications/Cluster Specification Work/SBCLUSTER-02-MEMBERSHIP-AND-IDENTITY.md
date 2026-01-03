# SBCLUSTER-02: Membership and Identity

**Document ID**: SBCLUSTER-02
**Version**: 1.0
**Status**: Beta Specification
**Date**: January 2026
**Dependencies**: SBCLUSTER-00, SBCLUSTER-01

---

## 1. Introduction

### 1.1 Purpose

This document defines the **membership model and identity system** for ScratchBird clusters, specifying how nodes join, authenticate, and are authorized to participate in cluster operations. This includes cryptographic identity binding, mTLS authentication, role-based node authorization, and peer observation mechanisms.

### 1.2 Scope

This specification covers:
- Node identity (node_uuid, cluster_uuid, certificates)
- Cryptographic identity binding (X.509, URI SANs)
- Authentication (mandatory mTLS, TLS 1.3)
- Authorization (node roles, enabled/disabled state)
- Membership lifecycle (join, enable, disable, remove)
- Peer observation protocol
- Node health and liveness tracking

### 1.3 Related Documents

| Document ID | Title |
|-------------|-------|
| SBCLUSTER-00 | Guiding Principles |
| SBCLUSTER-01 | Cluster Configuration Epoch |
| SBCLUSTER-03 | CA Policy |
| SBSEC-06 | Cluster Security (from security specs) |

### 1.4 Terminology

| Term | Definition |
|------|------------|
| **Node** | A ScratchBird server instance participating in a cluster |
| **node_uuid** | Unique stable identifier for a node (UUID v7) |
| **cluster_uuid** | Unique identifier for the cluster (UUID v7) |
| **Enabled Member** | Node authorized to participate in quorum and operations |
| **Disabled Member** | Node present but not participating (maintenance, etc.) |
| **Voter** | Node participating in Raft consensus |
| **Learner** | Node receiving Raft log but not voting |
| **Peer Observation** | Process where nodes verify each other's identity |

---

## 2. Node Identity

### 2.1 Identity Components

Each cluster node has a **compound identity**:

```cpp
struct NodeIdentity {
    // Stable identifiers
    UUID node_uuid;              // Unique, immutable, never reused
    UUID cluster_uuid;           // Which cluster this node belongs to

    // Display information
    string node_name;            // Human-readable name (e.g., "prod-db-01")

    // Cryptographic identity
    X509Certificate certificate;  // Node's X.509 certificate
    bytes32 cert_fingerprint;    // SHA-256 of DER-encoded cert

    // Network
    string[] listen_addresses;   // Addresses this node listens on
    uint16_t cluster_port;       // Port for cluster communication

    // Versioning
    uint32_t node_version;       // ScratchBird version (e.g., 1.0.0 → 100000)

    // Metadata
    timestamp_t joined_at;       // When node joined cluster
    UUID joined_by;              // Administrator who added node
};
```

### 2.2 UUID Requirements

**node_uuid**:
- **UUID v7** (time-ordered with random component)
- Generated at node initialization
- Stored in node's local configuration
- **Immutable** - never changes for the lifetime of the node
- **Never reused** - even if node is removed from cluster

**cluster_uuid**:
- Generated when cluster is created (bootstrap)
- All nodes in the cluster share the same cluster_uuid
- Used to prevent cross-cluster confusion
- Embedded in certificates via URI SAN

### 2.3 Cryptographic Identity Binding

Each node's identity is cryptographically bound using **X.509 certificates** with specific requirements:

```
Certificate Structure:
  Subject: CN=<node_uuid>
  Subject Alternative Name (SAN):
    - URI: sb://cluster/<cluster_uuid>/node/<node_uuid>
    - DNS: <node_name>.cluster.<cluster_uuid>.scratchbird.local
  Key Usage: Digital Signature, Key Encipherment
  Extended Key Usage: Server Authentication, Client Authentication
  Signature Algorithm: Ed25519 or ECDSA-P256
  Validity: Max 90 days (recommended: 30 days)
```

**Example Certificate SAN**:
```
URI: sb://cluster/01935f4a-8b2e-7890-abcd-0123456789ab/node/01935f4a-8b2e-7890-1111-0123456789ab
DNS: prod-db-01.cluster.01935f4a-8b2e-7890-abcd-0123456789ab.scratchbird.local
```

**Why URI SANs?**
- Provides cryptographic proof of cluster and node identity
- Prevents certificate reuse across clusters
- Enables automated validation without relying on CN alone
- Follows modern PKI best practices

---

## 3. Authentication

### 3.1 Mandatory mTLS

**All cluster RPC communication MUST use mutual TLS (mTLS)**:

```
Client (Node A)          Server (Node B)
      │                        │
      │──── TLS ClientHello ───►
      │                        │
      │◄─── TLS ServerHello ───│
      │◄─── Certificate ───────│  (Node B's cert)
      │◄─── CertificateRequest─│  (Requests client cert)
      │                        │
      │──── Certificate ───────►  (Node A's cert)
      │──── ClientKeyExchange ─►
      │──── CertificateVerify ─►  (Proves private key possession)
      │                        │
      │◄─── Finished ──────────│
      │──── Finished ──────────►
      │                        │
      │  Encrypted channel     │
      └────────────────────────┘
```

**Requirements**:
- TLS 1.3 (REQUIRED for new deployments)
- TLS 1.2 (ALLOWED for compatibility, deprecated)
- Cipher suites:
  - `TLS_AES_256_GCM_SHA384` (preferred)
  - `TLS_AES_128_GCM_SHA256` (allowed)
  - `TLS_CHACHA20_POLY1305_SHA256` (allowed)

### 3.2 Certificate Validation

When a node receives an mTLS connection, it MUST validate:

```cpp
bool validate_peer_certificate(const X509Certificate& cert,
                                const UUID& expected_cluster_uuid) {
    // 1. Basic X.509 validation
    if (!verify_certificate_chain(cert)) {
        return false;  // Chain of trust broken
    }

    // 2. Check validity period
    timestamp_t now = current_timestamp();
    if (now < cert.not_before || now > cert.not_after) {
        return false;  // Certificate expired or not yet valid
    }

    // 3. Extract URI SAN
    string uri_san = cert.get_uri_san();
    if (uri_san.empty()) {
        return false;  // No URI SAN present
    }

    // 4. Parse URI: sb://cluster/<cluster_uuid>/node/<node_uuid>
    auto parsed = parse_scratchbird_uri(uri_san);
    if (!parsed.valid) {
        return false;  // Malformed URI
    }

    // 5. Verify cluster UUID matches
    if (parsed.cluster_uuid != expected_cluster_uuid) {
        return false;  // Certificate from wrong cluster
    }

    // 6. Extract node UUID
    UUID peer_node_uuid = parsed.node_uuid;

    // 7. Verify this node is in membership list
    if (!is_member(peer_node_uuid)) {
        return false;  // Node not in cluster
    }

    // 8. Verify certificate fingerprint matches membership record
    bytes32 expected_fingerprint = get_member_cert_fingerprint(peer_node_uuid);
    bytes32 actual_fingerprint = sha256(cert.der_encoded());
    if (expected_fingerprint != actual_fingerprint) {
        return false;  // Certificate changed (potential attack)
    }

    return true;  // All checks passed
}
```

### 3.3 Certificate Rotation

Nodes rotate certificates as part of CA policy changes (see SBCLUSTER-03):

```
1. CA overlap epoch published (CA_A + CA_B)
2. Node generates new keypair
3. Node requests certificate from CA_B
4. Node updates local certificate
5. Node updates membership record with new cert fingerprint
6. Peer observation confirms new certificate
7. CA cutover epoch published (CA_B only)
8. Old certificate no longer accepted
```

---

## 4. Authorization

### 4.1 Node Roles

Each node has one or more **roles** that determine its capabilities:

```cpp
enum NodeRole : uint32_t {
    VOTER = 0x01,           // Participates in Raft voting
    LEARNER = 0x02,         // Receives Raft log but doesn't vote
    SHARD_OWNER = 0x04,     // Owns one or more shards
    COORDINATOR = 0x08,     // Can coordinate distributed queries
    SCHEDULER_AGENT = 0x10, // Runs scheduled jobs
    BACKUP_TARGET = 0x20,   // Can receive backups
    MONITOR = 0x40,         // Read-only monitoring access
};

// Nodes can have multiple roles (bitfield)
using NodeRoles = uint32_t;
```

**Role Descriptions**:

| Role | Description | Quorum | Data Storage |
|------|-------------|:------:|:------------:|
| **VOTER** | Participates in Raft consensus | Yes | No (unless also SHARD_OWNER) |
| **LEARNER** | Receives Raft log for catch-up | No | No |
| **SHARD_OWNER** | Owns and serves shard data | No | Yes |
| **COORDINATOR** | Routes distributed queries | No | No |
| **SCHEDULER_AGENT** | Executes scheduled jobs | No | Depends on job |
| **BACKUP_TARGET** | Stores backup data | No | Yes |
| **MONITOR** | Read-only status queries | No | No |

**Typical Configurations**:

```
Standard Data Node:
  VOTER | SHARD_OWNER | COORDINATOR | SCHEDULER_AGENT

Read Replica (Non-Voting):
  LEARNER | SHARD_OWNER | COORDINATOR

Dedicated Coordinator:
  VOTER | COORDINATOR | SCHEDULER_AGENT

Monitoring Node:
  MONITOR
```

### 4.2 Enabled vs. Disabled State

Nodes exist in one of several membership states:

```cpp
enum MembershipState : uint8_t {
    JOINING = 0,        // Node joining cluster (bootstrap phase)
    ENABLED = 1,        // Active, participating in operations
    DISABLED = 2,       // Temporarily disabled (maintenance)
    REMOVED = 3,        // Permanently removed from cluster
    FAILED = 4,         // Detected as failed, pending removal
};
```

**State Semantics**:

| State | Quorum | Operations | Receives Raft Log | Can Vote |
|-------|:------:|:----------:|:-----------------:|:--------:|
| **JOINING** | No | None | No | No |
| **ENABLED** | Yes | All | Yes | Yes (if VOTER) |
| **DISABLED** | No | None | Yes | No |
| **REMOVED** | No | None | No | No |
| **FAILED** | No | None | No | No |

**State Transitions**:

```
JOINING ──enable──► ENABLED
                      │
                      │ disable
                      ▼
                   DISABLED
                      │
                      │ enable
                      ▲────────┘
                      │
                      │ remove
                      ▼
                   REMOVED

ENABLED ──detect failure──► FAILED ──confirm removal──► REMOVED
```

### 4.3 Membership Record

The cluster maintains a **membership table** in the Raft log:

```cpp
struct MembershipRecord {
    // Identity
    UUID node_uuid;
    UUID cluster_uuid;
    string node_name;

    // Cryptographic identity
    bytes32 cert_fingerprint;    // Current certificate fingerprint
    Ed25519PublicKey pubkey;     // Node's public key for signing

    // Network
    string[] cluster_addresses;  // Host:port for cluster communication

    // Authorization
    NodeRoles roles;             // Bitmask of roles
    MembershipState state;       // Current state

    // Metadata
    uint32_t node_version;
    timestamp_t joined_at;
    timestamp_t last_seen_at;    // Updated by heartbeats
    UUID added_by;               // Administrator who added node

    // State transitions
    timestamp_t state_changed_at;
    UUID state_changed_by;
    string state_change_reason;
};
```

---

## 5. Membership Lifecycle

### 5.1 Joining a Cluster

**Process**:

```
1. New node initialized with:
   - Generated node_uuid
   - Target cluster_uuid
   - Initial certificate signed by cluster CA

2. Administrator executes on existing cluster leader:
   ADD NODE <node_uuid>
   WITH NAME '<node_name>'
   ADDRESSES '<host:port>', '<host2:port2>'
   ROLES VOTER, SHARD_OWNER
   CERTIFICATE '<base64_cert>';

3. Leader validates:
   - node_uuid not already in cluster
   - cluster_uuid in certificate matches
   - Certificate signed by current CA
   - Administrator has CLUSTER_MEMBERSHIP_ADMIN privilege

4. Leader writes MembershipRecord to Raft log:
   - State = JOINING
   - Roles as specified
   - Certificate fingerprint

5. Raft commits the entry

6. New node connects to cluster:
   - Authenticates via mTLS
   - Downloads current Raft log
   - Activates current CCE (epoch)

7. New node writes ReadyAttestation to Raft log

8. Leader transitions node state to ENABLED

9. If node has VOTER role:
   - Raft membership updated (add voter)
   - Quorum size recalculated
```

**ADD NODE SQL Syntax**:

```sql
ADD NODE <node_uuid>
WITH NAME <string>
ADDRESSES <address_list>
ROLES <role_list>
CERTIFICATE <pem_or_base64>;

-- Example
ADD NODE '01935f4a-8b2e-7890-1111-0123456789ab'
WITH NAME 'prod-db-02'
ADDRESSES 'prod-db-02.example.com:7947', '10.0.1.42:7947'
ROLES VOTER, SHARD_OWNER, COORDINATOR
CERTIFICATE 'LS0tLS1CRUdJTi...';
```

### 5.2 Enabling a Node

Transitions a DISABLED node to ENABLED:

```sql
ENABLE NODE <node_uuid>;
```

**Process**:
```
1. Administrator executes ENABLE NODE
2. Leader validates node is DISABLED
3. Leader writes EnableNode event to Raft log
4. Node state transitions to ENABLED
5. Node resumes operations
6. If VOTER: Added back to Raft quorum
```

### 5.3 Disabling a Node

Temporarily disables a node (maintenance, troubleshooting):

```sql
DISABLE NODE <node_uuid>
REASON 'Planned maintenance';
```

**Process**:
```
1. Administrator executes DISABLE NODE
2. Leader validates node is ENABLED
3. Leader writes DisableNode event to Raft log
4. Node state transitions to DISABLED
5. Node stops participating in operations:
   - If VOTER: Removed from Raft quorum (quorum size decreased)
   - Stops serving queries (except self-status)
   - Continues receiving Raft log
6. Audit event logged
```

**Important**: Disabling nodes affects quorum:
- 5-node cluster (quorum = 3)
- Disable 1 node → 4 nodes, quorum = 3 (still functional)
- Disable 2 nodes → 3 nodes, quorum = 2 (degraded)
- Disable 3 nodes → 2 nodes, quorum = 2 (NO QUORUM - cluster halts)

### 5.4 Removing a Node

Permanently removes a node from the cluster:

```sql
REMOVE NODE <node_uuid>
REASON 'Hardware decommissioned';
```

**Process**:
```
1. Administrator executes REMOVE NODE
2. Leader validates:
   - Removal doesn't break quorum
   - Shards owned by node are replicated elsewhere (if SHARD_OWNER)
3. Leader writes RemoveNode event to Raft log
4. Node state transitions to REMOVED
5. Node's certificate fingerprint invalidated
6. Future mTLS connections from this node rejected
7. Audit event logged
8. Node's historical data retained for audit
```

---

## 6. Peer Observation

### 6.1 Purpose

**Peer observation** is the mechanism where nodes verify each other's identity and configuration state. This provides:
- Defense against self-certification attacks
- Independent verification of certificate validity
- Detection of configuration drift
- Proof of convergence for CCE epochs

### 6.2 Observation Protocol

**Frequency**: Periodic (default: every 60 seconds) + on-demand (epoch changes)

**Process**:

```
Node A observes Node B:

1. Node A establishes mTLS connection to Node B
2. Node A validates Node B's certificate (section 3.2)
3. Node A sends ObservationRequest:
   {
     observer_node_uuid: A's UUID
     subject_node_uuid: B's UUID
     request_timestamp: now
   }
4. Node B responds with ObservationResponse:
   {
     node_uuid: B's UUID
     cluster_uuid: cluster UUID
     current_epoch: B's current epoch number
     node_version: B's version
     node_state: B's membership state
     roles: B's roles
     cert_fingerprint: SHA-256 of B's cert
     uptime_seconds: B's uptime
     signature: Ed25519 signature of this response
   }
5. Node A validates:
   - Signature matches B's public key
   - cluster_uuid matches
   - cert_fingerprint matches mTLS cert
   - current_epoch matches expected or is newer
6. Node A records observation:
   {
     observer: A's UUID
     subject: B's UUID
     observed_at: timestamp
     epoch_observed: B's current epoch
     cert_fingerprint: B's cert fingerprint
     valid: true/false
     signature: A signs this observation
   }
7. Observation stored locally and optionally written to Raft log
```

### 6.3 Observation Evidence

Observations are used as **evidence** for:

**CCE Convergence** (SBCLUSTER-01):
```
For epoch N to become EFFECTIVE:
- All enabled nodes must observe all other enabled nodes at epoch N
- Observations written to Raft log as ConvergenceAttestation
```

**CA Rotation** (SBCLUSTER-03):
```
For CA cutover:
- All enabled nodes must observe all other enabled nodes with new CA certs
- Ensures no node still using old CA before cutover
```

**Failure Detection**:
```
If node A cannot observe node B after N attempts:
- Node A logs ObservationFailure
- If majority of nodes report ObservationFailure for B:
  - B's state transitions to FAILED
  - Administrator investigation triggered
```

### 6.4 Gossip-Based Observation (Optional Enhancement)

For large clusters (>10 nodes), full peer observation (N×N) becomes expensive. An optional **gossip protocol** can reduce overhead:

```
1. Each node randomly selects K peers (K = 3-5)
2. Observes those K peers
3. Exchanges observation results with peers
4. Builds transitive observation graph
5. Detects failures via gossip propagation

This reduces per-node observation load from O(N) to O(K).
```

---

## 7. Health and Liveness

### 7.1 Heartbeat Protocol

Nodes send periodic heartbeats to update their `last_seen_at` timestamp:

```cpp
struct Heartbeat {
    UUID node_uuid;
    timestamp_t timestamp;
    uint64_t current_epoch;
    NodeRoles active_roles;
    Signature signature;
};
```

**Heartbeat Interval**: 10 seconds (default)

**Processing**:
```
1. Node sends Heartbeat to cluster leader
2. Leader validates signature
3. Leader updates member's last_seen_at
4. If member was FAILED and now responsive:
   - Log recovery event
   - Administrator decides whether to re-enable
```

### 7.2 Failure Detection

A node is considered **potentially failed** if:
```
current_time - last_seen_at > failure_timeout

failure_timeout = 3 × heartbeat_interval = 30 seconds (default)
```

**Detection Process**:
```
1. Leader monitors last_seen_at for all ENABLED members
2. If timeout exceeded:
   - Leader logs POTENTIAL_FAILURE event
   - Leader attempts direct ping to node
   - Leader checks peer observations from other nodes
3. If multiple nodes report observation failure:
   - State transitions to FAILED
   - Administrator notification
4. If node recovers and sends heartbeat:
   - POTENTIAL_FAILURE cleared
   - No automatic state change (admin must re-enable if disabled)
```

### 7.3 Liveness vs. Safety

**Trade-off**:
- **Short timeout** (30s): Fast failure detection, risk of false positives
- **Long timeout** (5m): Slower detection, fewer false positives

**ScratchBird approach**:
- Use **short timeout for detection** (30s)
- Require **confirmation period before action** (3 consecutive failures)
- Require **manual administrator intervention** for state changes

This prioritizes **safety** (no automatic node removal) over **liveness** (fast recovery).

---

## 8. Bootstrap Process

### 8.1 Single-Node Bootstrap

Creating a new cluster:

```bash
# Initialize first node
scratchbird cluster init \
  --cluster-name "production-cluster" \
  --node-name "prod-db-01" \
  --listen-address "prod-db-01.example.com:7947" \
  --data-dir "/var/lib/scratchbird" \
  --admin-user "alice" \
  --admin-password-file "/secure/admin_password.txt"

# This creates:
# - cluster_uuid (new UUID v7)
# - node_uuid (new UUID v7)
# - Self-signed CA for initial bootstrap
# - Node certificate
# - Epoch 1 (genesis epoch)
# - Initial administrator account
```

**Bootstrap Epoch**:
```
Epoch 1 (Genesis):
  - cluster_uuid: <generated>
  - CA Policy: Self-signed bootstrap CA
  - Security Bundle: Empty (no domains, RLS, etc.)
  - Shard Map: Single shard, owned by bootstrap node
  - Membership: Single node (VOTER, SHARD_OWNER, COORDINATOR)
```

### 8.2 Multi-Node Bootstrap

For production clusters, use **external CA**:

```bash
# Node 1 (leader)
scratchbird cluster init \
  --cluster-name "production-cluster" \
  --node-name "prod-db-01" \
  --ca-certificate "/path/to/ca.crt" \
  --node-certificate "/path/to/node1.crt" \
  --node-key "/path/to/node1.key" \
  ...

# Nodes 2-N
scratchbird cluster join \
  --cluster-uuid "<from_node1>" \
  --node-name "prod-db-02" \
  --join-address "prod-db-01.example.com:7947" \
  --node-certificate "/path/to/node2.crt" \
  --node-key "/path/to/node2.key" \
  ...
```

---

## 9. Security Considerations

### 9.1 Certificate Private Key Protection

**Requirements**:
- Private keys MUST be stored with restricted permissions (chmod 600)
- Private keys SHOULD be encrypted at rest (filesystem encryption)
- Private keys MAY be stored in HSM for high-security deployments
- Private keys MUST NEVER be transmitted over network

### 9.2 Node UUID Uniqueness

**Threat**: Attacker generates node_uuid collision to impersonate a node.

**Mitigation**:
- UUID v7 has 122 bits of randomness → collision probability negligible
- Certificate binding (can't use same node_uuid without private key)
- Membership table explicitly tracks node_uuid → certificate fingerprint

### 9.3 Certificate Revocation

**Scenario**: Node compromised, certificate must be revoked.

**Process**:
```
1. Administrator disables node immediately
2. New epoch published with updated CA policy (CRL or OCSP)
3. Node's certificate fingerprint removed from membership
4. All nodes reject connections from this certificate
5. Node must be re-provisioned with new certificate to rejoin
```

---

## 10. Example Membership Table

```
| node_uuid                              | node_name     | roles                    | state   | epoch | last_seen |
|----------------------------------------|---------------|--------------------------|---------|-------|-----------|
| 01935f4a-8b2e-7890-1111-0123456789ab   | prod-db-01    | VOTER, SHARD_OWNER, COORD| ENABLED | 42    | 2s ago    |
| 01935f4a-8b2e-7890-2222-0123456789ab   | prod-db-02    | VOTER, SHARD_OWNER, COORD| ENABLED | 42    | 3s ago    |
| 01935f4a-8b2e-7890-3333-0123456789ab   | prod-db-03    | VOTER, SHARD_OWNER, COORD| ENABLED | 42    | 2s ago    |
| 01935f4a-8b2e-7890-4444-0123456789ab   | prod-db-04    | LEARNER, SHARD_OWNER     | ENABLED | 42    | 5s ago    |
| 01935f4a-8b2e-7890-5555-0123456789ab   | prod-db-05    | SHARD_OWNER              | DISABLED| 42    | 2m ago    |

Quorum size: 2 (3 VOTER nodes enabled, simple majority = 2)
```

---

## 11. References

- [X.509 Certificate Format](https://datatracker.ietf.org/doc/html/rfc5280)
- [TLS 1.3 Specification](https://datatracker.ietf.org/doc/html/rfc8446)
- [Ed25519 Signatures](https://ed25519.cr.yp.to/)
- SBCLUSTER-01: Cluster Configuration Epoch
- SBCLUSTER-03: CA Policy
- SBSEC-06: Cluster Security

---

**End of SBCLUSTER-02**
