# ScratchBird Network Presence Binding Specification

**Document ID**: SBSEC-07  
**Version**: 1.0  
**Status**: Draft for Review  
**Date**: January 2026  
**Scope**: Cluster deployment mode, Security Level 6 only  

---

## 1. Introduction

### 1.1 Purpose

This document defines Network Presence Binding, a security mechanism that protects the Memory Encryption Key (MEK) using Shamir's Secret Sharing across cluster nodes. This creates a "Dead Man's Switch" where the MEK can only be reconstructed when sufficient cluster nodes are online and communicating.

### 1.2 Security Objective

Network Presence Binding provides:
- **Key availability tied to cluster health**: MEK accessible only when k-of-n nodes are present
- **Protection against offline attacks**: Compromised nodes/disks cannot decrypt data without live cluster
- **Automated key destruction**: Key becomes unrecoverable if cluster majority is lost
- **Defense in depth**: Additional layer beyond CMK protection

### 1.3 Scope

This specification covers:
- Shamir's Secret Sharing implementation
- Share distribution and storage
- MEK reconstruction protocol
- Heartbeat and presence verification
- Failure scenarios and recovery

### 1.4 Related Documents

| Document ID | Title |
|-------------|-------|
| SBSEC-01 | Security Architecture |
| SBSEC-04 | Encryption and Key Management |
| SBSEC-06 | Cluster Security |

### 1.5 Security Level Applicability

Network Presence Binding is:
- **OPTIONAL** at Security Level 5
- **REQUIRED** at Security Level 6

---

## 2. Cryptographic Foundation

### 2.1 Shamir's Secret Sharing

Network Presence Binding uses Shamir's Secret Sharing Scheme (SSSS) over GF(2^8):

```
┌─────────────────────────────────────────────────────────────────┐
│               SHAMIR'S SECRET SHARING                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Secret: S (the MEK, 32 bytes)                                  │
│  Threshold: k (minimum shares to reconstruct)                   │
│  Total: n (total shares distributed)                            │
│                                                                  │
│  For each byte of S:                                             │
│                                                                  │
│  1. Create random polynomial of degree (k-1):                   │
│     f(x) = S + a₁x + a₂x² + ... + aₖ₋₁xᵏ⁻¹                     │
│                                                                  │
│     where S is the secret byte and a₁...aₖ₋₁ are random        │
│                                                                  │
│  2. Evaluate polynomial at n points:                            │
│     Share₁ = f(1)                                               │
│     Share₂ = f(2)                                               │
│     ...                                                          │
│     Shareₙ = f(n)                                               │
│                                                                  │
│  3. Each share is (x, f(x)) pair                                │
│                                                                  │
│  Reconstruction:                                                 │
│  • Any k shares can reconstruct S using Lagrange interpolation  │
│  • Fewer than k shares reveal no information about S            │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Galois Field Arithmetic

All arithmetic is performed in GF(2^8) using the AES irreducible polynomial:

```c
// Irreducible polynomial: x^8 + x^4 + x^3 + x + 1 (0x11B)
#define GF256_POLYNOMIAL 0x11B

// GF(256) addition (XOR)
static inline uint8_t gf256_add(uint8_t a, uint8_t b) {
    return a ^ b;
}

// GF(256) multiplication (constant-time)
static uint8_t gf256_mul(uint8_t a, uint8_t b) {
    uint16_t result = 0;
    uint16_t temp_a = a;
    
    for (int i = 0; i < 8; i++) {
        // If bit i of b is set, add a << i
        uint16_t mask = -((b >> i) & 1);  // All 1s or all 0s
        result ^= (temp_a & mask);
        temp_a <<= 1;
    }
    
    // Reduce modulo polynomial
    for (int i = 14; i >= 8; i--) {
        uint16_t mask = -((result >> i) & 1);
        result ^= ((GF256_POLYNOMIAL << (i - 8)) & mask);
    }
    
    return (uint8_t)result;
}

// GF(256) multiplicative inverse (constant-time via extended Euclidean)
static uint8_t gf256_inv(uint8_t a) {
    // Uses Fermat's little theorem: a^254 = a^(-1) in GF(256)
    uint8_t result = a;
    for (int i = 0; i < 6; i++) {
        result = gf256_mul(result, result);
        result = gf256_mul(result, a);
    }
    result = gf256_mul(result, result);
    return result;
}
```

### 2.3 Constant-Time Operations

All cryptographic operations MUST be constant-time to prevent timing attacks:

```c
// Constant-time conditional select
static inline uint8_t ct_select(uint8_t mask, uint8_t a, uint8_t b) {
    return (mask & a) | (~mask & b);
}

// Constant-time equality comparison
static inline uint8_t ct_eq(uint8_t a, uint8_t b) {
    uint8_t diff = a ^ b;
    // If diff is 0, return 0xFF; otherwise return 0x00
    diff |= diff >> 4;
    diff |= diff >> 2;
    diff |= diff >> 1;
    return ~(diff & 1) + 1;  // 0 -> 0xFF, non-zero -> 0x00
}
```

---

## 3. Share Generation

### 3.1 Share Structure

```c
typedef struct ShareData {
    // Share identification
    uint8_t     share_index;            // x-coordinate (1 to n)
    uint8_t     threshold;              // k value
    uint8_t     total_shares;           // n value
    
    // Share data
    uint8_t     share_bytes[32];        // y-coordinates for 32-byte MEK
    
    // Integrity
    uint8_t     checksum[32];           // HMAC-SHA256 of above fields
    
    // Metadata
    uint8_t     version;                // Share format version
    uuid_t      generation_id;          // Unique per share generation
    uint64_t    generated_at;           // Timestamp
    
} ShareData;
```

### 3.2 Share Generation Algorithm

```c
typedef struct ShareGeneration {
    uuid_t      generation_id;
    uint8_t     threshold;
    uint8_t     total_shares;
    ShareData   shares[MAX_SHARES];
} ShareGeneration;

ShareGeneration* generate_shares(
    const uint8_t* mek,          // 32-byte MEK
    uint8_t k,                   // threshold
    uint8_t n,                   // total shares
    const uint8_t* hmac_key      // for checksum
) {
    ShareGeneration* gen = secure_alloc(sizeof(ShareGeneration));
    gen->generation_id = generate_uuid_v7();
    gen->threshold = k;
    gen->total_shares = n;
    
    // For each byte of the MEK
    for (int byte_idx = 0; byte_idx < 32; byte_idx++) {
        uint8_t secret_byte = mek[byte_idx];
        
        // Generate random polynomial coefficients
        uint8_t coeffs[k];
        coeffs[0] = secret_byte;  // Constant term is secret
        secure_random_bytes(&coeffs[1], k - 1);
        
        // Evaluate polynomial at each share point
        for (int share_idx = 0; share_idx < n; share_idx++) {
            uint8_t x = share_idx + 1;  // x values are 1 to n
            uint8_t y = evaluate_polynomial(coeffs, k, x);
            gen->shares[share_idx].share_bytes[byte_idx] = y;
        }
        
        // Clear coefficients
        secure_memzero(coeffs, sizeof(coeffs));
    }
    
    // Fill in share metadata and compute checksums
    for (int i = 0; i < n; i++) {
        gen->shares[i].share_index = i + 1;
        gen->shares[i].threshold = k;
        gen->shares[i].total_shares = n;
        gen->shares[i].version = SHARE_VERSION;
        gen->shares[i].generation_id = gen->generation_id;
        gen->shares[i].generated_at = current_timestamp();
        
        compute_share_checksum(&gen->shares[i], hmac_key);
    }
    
    return gen;
}

static uint8_t evaluate_polynomial(
    const uint8_t* coeffs,
    uint8_t degree,
    uint8_t x
) {
    // Horner's method for polynomial evaluation
    uint8_t result = coeffs[degree - 1];
    for (int i = degree - 2; i >= 0; i--) {
        result = gf256_mul(result, x);
        result = gf256_add(result, coeffs[i]);
    }
    return result;
}
```

### 3.3 Share Distribution

Shares are distributed to nodes immediately after generation:

```
┌─────────────────────────────────────────────────────────────────┐
│                 SHARE DISTRIBUTION                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  5-node cluster, k=3 (3-of-5 threshold)                         │
│                                                                  │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌────────┐│
│  │ Node 1  │  │ Node 2  │  │ Node 3  │  │ Node 4  │  │ Node 5 ││
│  │         │  │         │  │         │  │         │  │        ││
│  │ Share 1 │  │ Share 2 │  │ Share 3 │  │ Share 4 │  │ Share 5││
│  │ (x=1)   │  │ (x=2)   │  │ (x=3)   │  │ (x=4)   │  │ (x=5)  ││
│  └─────────┘  └─────────┘  └─────────┘  └─────────┘  └────────┘│
│                                                                  │
│  Each share:                                                     │
│  • Encrypted with node's local key (derived from node cert)     │
│  • Stored in volatile memory only (never persisted)             │
│  • Verified against checksum on use                             │
│                                                                  │
│  Distribution via mTLS:                                          │
│  • Primary node generates all shares                            │
│  • Sends each share to corresponding node                       │
│  • Nodes acknowledge receipt                                    │
│  • Primary clears local copy of other nodes' shares             │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 4. MEK Reconstruction

### 4.1 Lagrange Interpolation

```c
bool reconstruct_mek(
    ShareData* shares,
    int num_shares,
    uint8_t* mek_out,           // 32-byte output buffer
    const uint8_t* hmac_key     // for checksum verification
) {
    // Verify we have enough shares
    if (num_shares < shares[0].threshold) {
        return false;
    }
    
    uint8_t k = shares[0].threshold;
    
    // Verify all shares have same generation
    for (int i = 1; i < num_shares; i++) {
        if (!uuid_equal(shares[i].generation_id, shares[0].generation_id)) {
            return false;
        }
    }
    
    // Verify share checksums
    for (int i = 0; i < num_shares; i++) {
        if (!verify_share_checksum(&shares[i], hmac_key)) {
            audit_log(SHARE_CHECKSUM_INVALID, shares[i].share_index);
            return false;
        }
    }
    
    // Reconstruct each byte using Lagrange interpolation
    for (int byte_idx = 0; byte_idx < 32; byte_idx++) {
        uint8_t secret_byte = 0;
        
        for (int i = 0; i < k; i++) {
            uint8_t xi = shares[i].share_index;
            uint8_t yi = shares[i].share_bytes[byte_idx];
            
            // Compute Lagrange basis polynomial at x=0
            uint8_t numerator = 1;
            uint8_t denominator = 1;
            
            for (int j = 0; j < k; j++) {
                if (i == j) continue;
                
                uint8_t xj = shares[j].share_index;
                
                // numerator *= (0 - xj) = xj (in GF(256), -x = x)
                numerator = gf256_mul(numerator, xj);
                
                // denominator *= (xi - xj)
                denominator = gf256_mul(denominator, gf256_add(xi, xj));
            }
            
            // Lagrange coefficient: yi * numerator / denominator
            uint8_t coeff = gf256_mul(yi, numerator);
            coeff = gf256_mul(coeff, gf256_inv(denominator));
            
            secret_byte = gf256_add(secret_byte, coeff);
        }
        
        mek_out[byte_idx] = secret_byte;
    }
    
    return true;
}
```

### 4.2 Reconstruction Protocol

```
┌─────────────────────────────────────────────────────────────────┐
│              MEK RECONSTRUCTION PROTOCOL                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Node starting up (needs MEK to decrypt data):                  │
│                                                                  │
│  1. Check local share                                            │
│     • Verify share exists in memory                             │
│     • Verify checksum valid                                     │
│     • Record local share as "available"                         │
│                                                                  │
│  2. Contact peer nodes                                           │
│     • Establish mTLS connections                                │
│     • Request share presence status (not the share itself)      │
│                                                                  │
│  3. Verify threshold met                                         │
│     • Count available shares                                    │
│     • If < k available: wait or fail                            │
│                                                                  │
│  4. Request shares from peers                                    │
│     • SHARE_REQUEST message with generation_id                  │
│     • Peers verify requesting node is cluster member            │
│     • Shares transmitted over mTLS                              │
│                                                                  │
│  5. Reconstruct MEK                                              │
│     • Lagrange interpolation                                    │
│     • Verify result (if verification data available)            │
│     • Store in secure memory                                    │
│                                                                  │
│  6. Clear received shares                                        │
│     • secure_memzero all share data except local                │
│     • Only local share retained                                 │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 4.3 Message Protocol

```c
// Share presence check (doesn't reveal share data)
typedef struct SharePresenceRequest {
    uint8_t     message_type;       // MSG_SHARE_PRESENCE_REQUEST
    uuid_t      generation_id;      // Which generation
    uuid_t      requesting_node;
    uint64_t    timestamp;
    uint8_t     signature[64];      // Ed25519 signature
} SharePresenceRequest;

typedef struct SharePresenceResponse {
    uint8_t     message_type;       // MSG_SHARE_PRESENCE_RESPONSE
    uuid_t      generation_id;
    uuid_t      responding_node;
    uint8_t     share_index;        // Which share this node has
    uint8_t     status;             // PRESENT, NOT_PRESENT, INVALID
    uint64_t    timestamp;
    uint8_t     signature[64];
} SharePresenceResponse;

// Share transfer (over mTLS only)
typedef struct ShareTransferRequest {
    uint8_t     message_type;       // MSG_SHARE_TRANSFER_REQUEST
    uuid_t      generation_id;
    uuid_t      requesting_node;
    uint8_t     purpose;            // RECONSTRUCTION, RESHARING
    uint64_t    timestamp;
    uint8_t     signature[64];
} ShareTransferRequest;

typedef struct ShareTransferResponse {
    uint8_t     message_type;       // MSG_SHARE_TRANSFER_RESPONSE
    uuid_t      generation_id;
    uuid_t      responding_node;
    ShareData   share;              // The actual share
    uint64_t    timestamp;
    uint8_t     signature[64];
} ShareTransferResponse;
```

---

## 5. Heartbeat and Presence

### 5.1 Heartbeat Protocol

Nodes continuously verify each other's presence:

```
┌─────────────────────────────────────────────────────────────────┐
│                  HEARTBEAT PROTOCOL                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Every HEARTBEAT_INTERVAL (default: 1 second):                  │
│                                                                  │
│  1. Each node sends heartbeat to all peers:                      │
│     {                                                            │
│       type: HEARTBEAT,                                          │
│       node_id: uuid,                                            │
│       share_status: PRESENT | ABSENT | INVALID,                 │
│       generation_id: uuid,                                      │
│       sequence: uint64,                                         │
│       timestamp: uint64,                                        │
│       signature: [64 bytes]                                     │
│     }                                                            │
│                                                                  │
│  2. Peer responds with heartbeat acknowledgment                  │
│                                                                  │
│  3. Track peer status:                                           │
│     • Last heartbeat received                                   │
│     • Consecutive missed heartbeats                             │
│     • Share status (has valid share)                            │
│                                                                  │
│  4. If HEARTBEAT_TIMEOUT exceeded:                              │
│     • Mark peer as UNREACHABLE                                  │
│     • Recalculate available shares                              │
│     • If < k shares available: trigger MEK_UNAVAILABLE          │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 Presence Verification

```c
typedef struct PeerStatus {
    uuid_t      node_id;
    uint64_t    last_heartbeat;
    uint64_t    last_sequence;
    uint8_t     share_status;
    uint8_t     share_index;
    uuid_t      generation_id;
    int         missed_heartbeats;
    NodeState   state;              // ACTIVE, SUSPECTED, UNREACHABLE
} PeerStatus;

void process_heartbeat(HeartbeatMessage* hb, PeerStatus* peers, int* peer_count) {
    // Find or create peer entry
    PeerStatus* peer = find_peer(peers, peer_count, hb->node_id);
    if (!peer) {
        peer = add_peer(peers, peer_count, hb->node_id);
    }
    
    // Verify signature
    if (!verify_heartbeat_signature(hb)) {
        audit_log(HEARTBEAT_SIGNATURE_INVALID, hb->node_id);
        return;
    }
    
    // Verify sequence (prevent replay)
    if (hb->sequence <= peer->last_sequence) {
        audit_log(HEARTBEAT_REPLAY, hb->node_id, hb->sequence);
        return;
    }
    
    // Update peer status
    peer->last_heartbeat = current_timestamp();
    peer->last_sequence = hb->sequence;
    peer->share_status = hb->share_status;
    peer->share_index = hb->share_index;
    peer->generation_id = hb->generation_id;
    peer->missed_heartbeats = 0;
    peer->state = NODE_ACTIVE;
}

int count_available_shares(PeerStatus* peers, int peer_count, uuid_t generation_id) {
    int available = 0;
    
    // Count local share if valid
    if (local_share_valid(generation_id)) {
        available++;
    }
    
    // Count peer shares
    for (int i = 0; i < peer_count; i++) {
        if (peers[i].state == NODE_ACTIVE &&
            peers[i].share_status == SHARE_PRESENT &&
            uuid_equal(peers[i].generation_id, generation_id)) {
            available++;
        }
    }
    
    return available;
}
```

### 5.3 MEK Availability State Machine

```
┌─────────────────────────────────────────────────────────────────┐
│             MEK AVAILABILITY STATES                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────────┐                                           │
│  │  MEK_UNAVAILABLE │ Initial state / insufficient shares       │
│  └────────┬─────────┘                                           │
│           │ k shares become available                            │
│           ▼                                                      │
│  ┌──────────────────┐                                           │
│  │ MEK_RECONSTRUCTING│ Gathering shares from peers              │
│  └────────┬─────────┘                                           │
│           │ reconstruction successful                            │
│           ▼                                                      │
│  ┌──────────────────┐                                           │
│  │  MEK_AVAILABLE   │ MEK in memory, can encrypt/decrypt        │
│  └────────┬─────────┘                                           │
│           │                                                      │
│           ├─── available shares drop below k ───► MEK_DEGRADED  │
│           │                                                      │
│           ├─── share refresh needed ───► MEK_REFRESHING         │
│           │                                                      │
│           └─── shutdown ───► MEK_DESTROYED                      │
│                                                                  │
│  ┌──────────────────┐                                           │
│  │   MEK_DEGRADED   │ MEK valid but insufficient shares for    │
│  │                  │ re-reconstruction if node restarts        │
│  └────────┬─────────┘                                           │
│           │                                                      │
│           ├─── shares restored to k ───► MEK_AVAILABLE          │
│           │                                                      │
│           └─── grace period expires ───► MEK_DESTROYED          │
│                                                                  │
│  ┌──────────────────┐                                           │
│  │  MEK_DESTROYED   │ MEK cleared from memory                   │
│  └──────────────────┘                                           │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 6. Share Management

### 6.1 Share Refresh

Shares should be periodically refreshed without changing the secret:

```c
// Resharing: generate new shares for same MEK
ShareGeneration* refresh_shares(
    const uint8_t* mek,
    uint8_t new_k,
    uint8_t new_n,
    const uint8_t* hmac_key
) {
    // Generate completely new shares
    ShareGeneration* new_gen = generate_shares(mek, new_k, new_n, hmac_key);
    
    // New generation_id invalidates old shares
    
    return new_gen;
}
```

Refresh is triggered by:
- Time-based policy (e.g., every 24 hours)
- Membership change (node added/removed)
- Security event (compromised node suspected)
- Administrative action

### 6.2 Membership Changes

When cluster membership changes, shares must be redistributed:

```
┌─────────────────────────────────────────────────────────────────┐
│              MEMBERSHIP CHANGE HANDLING                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Node Added:                                                     │
│  ───────────                                                     │
│  1. New node joins cluster (no share yet)                       │
│  2. Primary node reconstructs MEK                               │
│  3. Generate new share set with n+1 shares                      │
│  4. Distribute shares to all nodes including new node           │
│  5. Old shares invalidated by new generation_id                 │
│                                                                  │
│  Node Removed (Graceful):                                        │
│  ─────────────────────────                                       │
│  1. Node signals departure                                      │
│  2. Primary node reconstructs MEK                               │
│  3. Generate new share set with n-1 shares                      │
│  4. Distribute shares to remaining nodes                        │
│  5. Departing node destroys its share                           │
│                                                                  │
│  Node Removed (Forced/Failed):                                   │
│  ─────────────────────────────                                   │
│  1. Node fenced or unresponsive                                 │
│  2. Remaining nodes verify k shares still available             │
│  3. Primary reconstructs MEK                                    │
│  4. Generate new shares (failed node's share now useless)       │
│  5. Distribute to remaining nodes                               │
│                                                                  │
│  Threshold Change:                                               │
│  ─────────────────                                               │
│  1. Admin requests new threshold k                              │
│  2. Validate: 1 < k ≤ n                                         │
│  3. Acquire TWO_PERSON approval (Level 6)                       │
│  4. Reconstruct MEK, generate new shares with new k             │
│  5. Distribute new shares                                       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 6.3 Share Destruction

Shares are destroyed when:
- Node shuts down (share in volatile memory)
- Share refresh completes (old generation invalidated)
- Node removed from cluster
- Security incident detected

```c
void destroy_share(ShareData* share) {
    // Overwrite share data multiple times
    for (int pass = 0; pass < 3; pass++) {
        // Fill with random data
        secure_random_bytes(share->share_bytes, sizeof(share->share_bytes));
        memory_barrier();  // Ensure writes complete
    }
    
    // Final zero fill
    secure_memzero(share, sizeof(ShareData));
    
    audit_log(SHARE_DESTROYED, share->share_index, share->generation_id);
}
```

---

## 7. Failure Scenarios

### 7.1 Node Failure (Single)

```
Scenario: 5-node cluster (k=3), Node 3 fails

Before:
  Nodes: [1, 2, 3, 4, 5]
  Available shares: 5
  Threshold: 3

After Node 3 failure:
  Nodes: [1, 2, -, 4, 5]
  Available shares: 4
  Threshold: 3 (still met)

Action:
  • Cluster continues operating
  • Admin should replace failed node
  • Optional: refresh shares to exclude Node 3's share
```

### 7.2 Multiple Node Failure (Below Threshold)

```
Scenario: 5-node cluster (k=3), Nodes 2, 4, 5 fail

Before:
  Nodes: [1, 2, 3, 4, 5]
  Available shares: 5

After failures:
  Nodes: [1, -, 3, -, -]
  Available shares: 2
  Threshold: 3 (NOT MET)

Action:
  • MEK becomes unavailable
  • All encrypted operations fail
  • Cluster enters DEGRADED state
  • Options:
    a. Wait for nodes to recover
    b. Administrative recovery procedure
```

### 7.3 Network Partition

```
Scenario: 5-node cluster (k=3) partitions into [1,2] and [3,4,5]

Partition A (2 nodes):
  Available shares: 2
  Threshold: 3 (NOT MET)
  → MEK unavailable, self-fence

Partition B (3 nodes):
  Available shares: 3
  Threshold: 3 (MET)
  → MEK available, continues operating

Resolution:
  • Partition A detects quorum loss
  • Partition A self-fences
  • Partition B has quorum and MEK
  • When network heals, Partition A rejoins
```

### 7.4 Total Cluster Loss

```
Scenario: All nodes fail simultaneously

Available shares: 0
MEK: Lost (only existed in volatile memory)

Recovery:
  • MEK cannot be reconstructed
  • Data encrypted with MEK is unrecoverable
  • Must restore from backup
  • New MEK generated on cluster restart
  • This is by design ("Dead Man's Switch")
```

### 7.5 Recovery Procedures

```
┌─────────────────────────────────────────────────────────────────┐
│               RECOVERY PROCEDURES                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Scenario 1: Single node recovery                               │
│  ─────────────────────────────────                               │
│  1. Restart failed node                                         │
│  2. Node contacts cluster                                       │
│  3. If cluster has MEK: node receives new share                 │
│  4. Normal operation resumes                                    │
│                                                                  │
│  Scenario 2: Below-threshold recovery                           │
│  ─────────────────────────────────────                           │
│  1. Bring enough nodes online to reach k                        │
│  2. Nodes coordinate share presence                             │
│  3. MEK reconstructed from k shares                             │
│  4. New shares distributed to all online nodes                  │
│                                                                  │
│  Scenario 3: Total loss with backup                             │
│  ───────────────────────────────────                             │
│  1. Restore cluster from backup                                 │
│  2. Backup includes CMK-encrypted data                          │
│  3. New MEK generated                                           │
│  4. Data re-encrypted under new MEK                             │
│  5. New shares distributed                                      │
│                                                                  │
│  Scenario 4: Emergency CMK recovery                             │
│  ───────────────────────────────────                             │
│  1. If CMK is recoverable (HSM, split knowledge)               │
│  2. Bootstrap cluster with CMK                                  │
│  3. Generate new MEK                                            │
│  4. Re-encrypt all data                                         │
│  5. Generate and distribute shares                              │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 8. Security Considerations

### 8.1 Threat Model

| Threat | Mitigation |
|--------|------------|
| Single node compromise | k-1 shares reveal nothing |
| Offline disk theft | MEK in memory only, share insufficient alone |
| Network eavesdropping | mTLS for all share transfers |
| Replay attacks | Sequence numbers, timestamps |
| Malicious node | Certificate validation, signature verification |
| Timing attacks | Constant-time GF(256) arithmetic |
| Memory attacks | Secure memory allocation, immediate clearing |

### 8.2 Share Security

| Requirement | Implementation |
|-------------|----------------|
| Share confidentiality | mTLS transfer, memory-only storage |
| Share integrity | HMAC-SHA256 checksum |
| Share authenticity | Ed25519 signatures on messages |
| No share persistence | Volatile memory only |
| Secure clearing | Multi-pass overwrite with barrier |

### 8.3 Protocol Security

```
Message Authentication:
  • All messages signed with node's Ed25519 key
  • Key derived from node certificate
  • Signature covers entire message including timestamp

Replay Prevention:
  • Strictly increasing sequence numbers
  • Timestamp within acceptable window (±30 seconds)
  • Generation ID binds shares to specific generation

Channel Security:
  • mTLS required for all share transfers
  • Certificate validation per SBSEC-06
  • Perfect forward secrecy via ephemeral keys
```

---

## 9. Audit Events

| Event | Logged Data |
|-------|-------------|
| SHARES_GENERATED | generation_id, k, n, generated_by |
| SHARE_DISTRIBUTED | node_id, share_index, generation_id |
| SHARE_RECEIVED | from_node, share_index, generation_id |
| MEK_RECONSTRUCTED | shares_used[], generation_id |
| MEK_DESTROYED | reason, generation_id |
| SHARE_REFRESH_STARTED | old_gen, new_gen, reason |
| SHARE_REFRESH_COMPLETED | new_gen, nodes_updated |
| THRESHOLD_CHANGED | old_k, new_k, approved_by[] |
| INSUFFICIENT_SHARES | available, required, missing_nodes[] |
| SHARE_CHECKSUM_INVALID | node_id, share_index |
| HEARTBEAT_TIMEOUT | node_id, last_seen |

---

## 10. Configuration Reference

| Policy Key | Type | Default | Description |
|------------|------|---------|-------------|
| `npb.enabled` | boolean | false | Enable Network Presence Binding |
| `npb.threshold` | integer | (n/2)+1 | Share threshold (k) |
| `npb.heartbeat_interval` | interval | 1s | Heartbeat frequency |
| `npb.heartbeat_timeout` | interval | 5s | Heartbeat failure threshold |
| `npb.refresh_interval` | interval | 24h | Share refresh frequency |
| `npb.degraded_grace_period` | interval | 5m | Grace period in degraded state |
| `npb.reconstruction_timeout` | interval | 30s | Max time for MEK reconstruction |

---

## Appendix A: Mathematical Reference

### A.1 GF(2^8) Properties

```
Field: GF(2^8) with irreducible polynomial p(x) = x^8 + x^4 + x^3 + x + 1

Properties:
  • 256 elements (0x00 to 0xFF)
  • Addition: XOR (a ⊕ b)
  • Subtraction: Same as addition (a ⊕ b)
  • Multiplication: Polynomial multiplication mod p(x)
  • Division: a / b = a × b^(-1)
  • Every non-zero element has a multiplicative inverse

Generator: g = 0x03 generates all non-zero elements
```

### A.2 Lagrange Interpolation

```
Given k points (x₁, y₁), (x₂, y₂), ..., (xₖ, yₖ):

f(x) = Σᵢ yᵢ × Lᵢ(x)

where Lᵢ(x) = Πⱼ≠ᵢ (x - xⱼ) / (xᵢ - xⱼ)

To find f(0) (the secret):

f(0) = Σᵢ yᵢ × Πⱼ≠ᵢ (xⱼ) / (xᵢ - xⱼ)

In GF(256), subtraction is XOR, so:

f(0) = Σᵢ yᵢ × Πⱼ≠ᵢ (xⱼ) / (xᵢ ⊕ xⱼ)
```

---

*End of Document*
