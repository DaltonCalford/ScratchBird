# ScratchBird Cluster Security Specification

**Document ID**: SBSEC-06  
**Version**: 1.0  
**Status**: Draft for Review  
**Date**: January 2026  
**Scope**: Cluster deployment mode only  

---

## 1. Introduction

### 1.1 Purpose

This document defines the security architecture for ScratchBird cluster deployments. It specifies how multiple nodes coordinate securely, how security databases maintain authoritative identity information, and how quorum-based decisions protect critical operations.

### 1.2 Scope

This specification covers:
- Cluster topology and node roles
- Security database architecture
- Node authentication and inter-node TLS
- Quorum-based security decisions
- Fencing and split-brain prevention
- Credential replication
- Cluster membership changes

### 1.3 Related Documents

| Document ID | Title |
|-------------|-------|
| SBSEC-01 | Security Architecture |
| SBSEC-02 | Identity and Authentication |
| SBSEC-04 | Encryption and Key Management |
| SBSEC-07 | Network Presence Binding |

### 1.4 Security Level Applicability

| Feature | L0 | L1 | L2 | L3 | L4 | L5 | L6 |
|---------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Cluster mode | ● | ● | ● | ● | ● | ● | ● |
| Inter-node TLS | | | ● | ● | ● | ● | ● |
| mTLS (client certs) | | | | ○ | ○ | ● | ● |
| Security databases | | | | | | ● | ● |
| Quorum enforcement | | | | | | ○ | ● |
| Fencing | | | | | | ● | ● |
| Network Presence Binding | | | | | | | ● |

● = Required | ○ = Optional | (blank) = Not applicable

---

## 2. Cluster Topology

### 2.1 Node Roles

```
┌─────────────────────────────────────────────────────────────────┐
│                    CLUSTER NODE ROLES                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  DATA NODE                                                       │
│  ──────────                                                      │
│  • Stores user databases                                        │
│  • Executes queries                                             │
│  • Caches credentials from Security DB                          │
│  • Participates in quorum (if voting member)                    │
│                                                                  │
│  SECURITY NODE                                                   │
│  ─────────────                                                   │
│  • Hosts Security Database                                      │
│  • Authoritative source for identity/auth                       │
│  • Replicates to other Security Nodes                           │
│  • May also be a Data Node                                      │
│                                                                  │
│  ARBITER NODE                                                    │
│  ────────────                                                    │
│  • Voting member only (no data)                                 │
│  • Provides quorum in even-numbered clusters                    │
│  • Lightweight resource requirements                            │
│                                                                  │
│  WITNESS NODE                                                    │
│  ────────────                                                    │
│  • External to cluster (different failure domain)               │
│  • Tie-breaker for split-brain                                  │
│  • Does not store data or credentials                           │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Typical Cluster Configurations

| Configuration | Data Nodes | Security Nodes | Arbiters | Quorum |
|---------------|------------|----------------|----------|--------|
| Minimum HA | 2 | 1 (shared) | 1 | 2 of 3 |
| Standard | 3 | 2 | 0 | 2 of 3 |
| Production | 5 | 3 | 0 | 3 of 5 |
| Multi-DC | 5+ | 3+ | 0-2 | Varies |

### 2.3 Cluster Topology Example

```
┌─────────────────────────────────────────────────────────────────┐
│                  PRODUCTION CLUSTER TOPOLOGY                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Data Center A                     Data Center B                 │
│  ─────────────                     ─────────────                 │
│                                                                  │
│  ┌─────────────┐                  ┌─────────────┐               │
│  │   Node 1    │                  │   Node 3    │               │
│  │  Data +     │◄────────────────►│  Data +     │               │
│  │  Security   │                  │  Security   │               │
│  │  (Primary)  │                  │  (Standby)  │               │
│  └──────┬──────┘                  └──────┬──────┘               │
│         │                                │                       │
│         │                                │                       │
│  ┌──────┴──────┐                  ┌──────┴──────┐               │
│  │   Node 2    │                  │   Node 4    │               │
│  │  Data       │◄────────────────►│  Data       │               │
│  │             │                  │             │               │
│  └─────────────┘                  └─────────────┘               │
│                                                                  │
│                       ┌─────────────┐                           │
│                       │   Node 5    │                           │
│                       │  Security   │ (Third DC or Cloud)       │
│                       │  (Standby)  │                           │
│                       └─────────────┘                           │
│                                                                  │
│  Quorum: 3 of 5 nodes                                           │
│  Security DB: 2 of 3 Security Nodes for writes                  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Security Database

### 3.1 Overview

The Security Database is the authoritative source for all authentication and authorization information in a cluster.

### 3.2 Security Database Contents

```sql
-- Security Database Schema (simplified)

-- Users and credentials
sys.sec_users
sys.sec_credentials
sys.sec_authkeys

-- Roles and groups
sys.sec_roles
sys.sec_groups
sys.sec_role_memberships
sys.sec_group_memberships

-- Privileges
sys.sec_privileges
sys.sec_default_privileges

-- Policies
sys.sec_rls_policies
sys.sec_password_policies
sys.sec_session_policies

-- External auth
sys.sec_auth_providers
sys.sec_identity_mappings
sys.sec_group_mappings

-- Audit configuration
sys.sec_audit_config

-- Cluster-specific
sys.sec_nodes
sys.sec_node_certificates
sys.sec_cluster_config
```

### 3.3 Security Database Replication

```
┌─────────────────────────────────────────────────────────────────┐
│              SECURITY DATABASE REPLICATION                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Write Path:                                                     │
│                                                                  │
│  1. Client sends security modification                           │
│     (CREATE USER, GRANT, etc.)                                  │
│                                                                  │
│  2. Request routed to Primary Security Node                     │
│                                                                  │
│  3. Primary acquires quorum (for critical operations)           │
│                                                                  │
│  4. Primary applies change                                       │
│                                                                  │
│  5. Change replicated to Standby Security Nodes                 │
│     (synchronous for L6, async for L5)                          │
│                                                                  │
│  6. Acknowledgment returned to client                           │
│                                                                  │
│  Read Path:                                                      │
│                                                                  │
│  • Reads may go to any Security Node (with replication lag)     │
│  • Critical reads (during auth) prefer Primary                  │
│  • Data nodes cache credentials locally                         │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 3.4 Credential Caching

Data nodes cache credentials from the Security Database:

```sql
-- Local credential cache on data nodes
CREATE TABLE sys.local_credential_cache (
    user_uuid           UUID PRIMARY KEY,
    username            VARCHAR(128),
    password_hash       BYTEA,
    hash_algorithm      VARCHAR(32),
    user_status         VARCHAR(32),
    roles_json          JSONB,          -- Cached role memberships
    groups_json         JSONB,          -- Cached group memberships
    
    -- Cache metadata
    cached_at           TIMESTAMP NOT NULL,
    expires_at          TIMESTAMP NOT NULL,
    source_node_uuid    UUID,           -- Security node that provided this
    source_version      BIGINT,         -- Version for invalidation
    
    -- Validation tracking
    last_validated_at   TIMESTAMP,
    validation_failures INTEGER DEFAULT 0
);
```

### 3.5 Cache Invalidation

```
┌─────────────────────────────────────────────────────────────────┐
│                  CACHE INVALIDATION                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Push Invalidation (immediate):                                  │
│  ──────────────────────────────                                  │
│  1. Security change occurs (password change, revoke, etc.)      │
│  2. Security Node broadcasts INVALIDATE message                  │
│  3. Message includes: user_uuid, change_type, version           │
│  4. Data nodes receive and invalidate cached entry              │
│  5. Next auth for this user fetches fresh data                  │
│                                                                  │
│  TTL Invalidation (background):                                  │
│  ─────────────────────────────                                   │
│  • Cache entries have configurable TTL                          │
│  • Expired entries re-fetched on next use                       │
│  • Background job pre-fetches about-to-expire entries           │
│                                                                  │
│  Version Invalidation (consistency):                             │
│  ───────────────────────────────────                             │
│  • Each security change increments global version               │
│  • Data nodes track last-known version                          │
│  • If version gap detected, full cache refresh triggered        │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 4. Node Authentication

### 4.1 Node Identity

Each node has a unique identity established at cluster join:

```sql
CREATE TABLE sys.sec_nodes (
    node_uuid           UUID PRIMARY KEY,
    node_name           VARCHAR(128) NOT NULL UNIQUE,
    
    -- Node type
    node_role           ENUM('DATA', 'SECURITY', 'ARBITER', 'WITNESS'),
    is_voting           BOOLEAN DEFAULT TRUE,
    
    -- Network
    addresses           INET[],         -- All listen addresses
    cluster_port        INTEGER,        -- Inter-node communication
    client_port         INTEGER,        -- Client connections
    
    -- Certificates
    certificate_fingerprint BYTEA,      -- SHA-256 of node cert
    certificate_expires TIMESTAMP,
    
    -- Status
    status              ENUM('JOINING', 'ACTIVE', 'LEAVING', 'FENCED', 'REMOVED'),
    last_heartbeat      TIMESTAMP,
    
    -- Metadata
    joined_at           TIMESTAMP,
    joined_by           UUID,           -- Admin who approved join
    
    UNIQUE (certificate_fingerprint)
);
```

### 4.2 Node Certificate

Each node has a TLS certificate for inter-node communication:

```
┌─────────────────────────────────────────────────────────────────┐
│                  NODE CERTIFICATE                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Subject:                                                        │
│    CN = node-1.cluster.scratchbird.local                        │
│    O  = ScratchBird Cluster                                     │
│    OU = [cluster-uuid]                                          │
│                                                                  │
│  Subject Alternative Names:                                      │
│    DNS: node-1.cluster.scratchbird.local                        │
│    DNS: node-1                                                  │
│    IP:  192.168.1.10                                            │
│    IP:  10.0.0.10                                               │
│                                                                  │
│  Extended Key Usage:                                             │
│    TLS Server Authentication                                    │
│    TLS Client Authentication                                    │
│                                                                  │
│  Custom Extension (OID 1.3.6.1.4.1.xxxxx.1):                   │
│    node_uuid: 0198f0b2-1111-...                                 │
│    cluster_uuid: 0198f0b2-0000-...                              │
│    node_role: DATA                                              │
│                                                                  │
│  Validity:                                                       │
│    Not Before: 2026-01-01                                       │
│    Not After:  2027-01-01 (1 year default)                      │
│                                                                  │
│  Issuer:                                                         │
│    CN = ScratchBird Cluster CA                                  │
│    Cluster-specific CA or external PKI                          │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 4.3 Mutual TLS

All inter-node communication uses mutual TLS:

```
┌─────────────────────────────────────────────────────────────────┐
│                  INTER-NODE mTLS                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Node A (client)                    Node B (server)              │
│       │                                  │                       │
│       │  1. TCP connect                  │                       │
│       │─────────────────────────────────►│                       │
│       │                                  │                       │
│       │  2. TLS ClientHello              │                       │
│       │─────────────────────────────────►│                       │
│       │                                  │                       │
│       │  3. ServerHello + Certificate    │                       │
│       │     + CertificateRequest         │                       │
│       │◄─────────────────────────────────│                       │
│       │                                  │                       │
│       │  4. Verify server cert:          │                       │
│       │     • Signed by Cluster CA       │                       │
│       │     • Same cluster_uuid          │                       │
│       │     • Not revoked                │                       │
│       │     • Fingerprint in sys.sec_nodes                      │
│       │                                  │                       │
│       │  5. Client Certificate           │                       │
│       │─────────────────────────────────►│                       │
│       │                                  │                       │
│       │                    6. Verify client cert (same checks)  │
│       │                                  │                       │
│       │  7. Finished                     │                       │
│       │◄────────────────────────────────►│                       │
│       │                                  │                       │
│       │  Encrypted channel established   │                       │
│       │                                  │                       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 4.4 Certificate Validation

```python
def validate_node_certificate(cert, connection_context):
    """Validate a node's certificate during TLS handshake."""
    
    # 1. Basic certificate validation
    if not verify_certificate_chain(cert, cluster_ca):
        return CERT_INVALID_CHAIN
    
    if cert.not_after < current_time():
        return CERT_EXPIRED
    
    # 2. Check revocation
    if is_certificate_revoked(cert):
        return CERT_REVOKED
    
    # 3. Extract node identity
    node_uuid = extract_node_uuid(cert)
    cluster_uuid = extract_cluster_uuid(cert)
    
    # 4. Verify cluster membership
    if cluster_uuid != our_cluster_uuid:
        return CERT_WRONG_CLUSTER
    
    # 5. Verify node is known and active
    node = lookup_node(node_uuid)
    if not node:
        return CERT_UNKNOWN_NODE
    
    if node.status not in ('ACTIVE', 'JOINING'):
        return CERT_NODE_NOT_ACTIVE
    
    # 6. Verify fingerprint matches
    if sha256(cert.der) != node.certificate_fingerprint:
        audit_log(CERT_FINGERPRINT_MISMATCH, node_uuid, cert)
        return CERT_FINGERPRINT_MISMATCH
    
    return CERT_VALID, node
```

---

## 5. Quorum

### 5.1 Quorum Model

Quorum ensures that cluster-wide decisions have majority agreement:

```
┌─────────────────────────────────────────────────────────────────┐
│                      QUORUM MODEL                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Quorum Types:                                                   │
│                                                                  │
│  MAJORITY (default)                                              │
│  ─────────────────                                               │
│  • More than half of voting members                             │
│  • 3-node cluster: 2 required                                   │
│  • 5-node cluster: 3 required                                   │
│  • Used for: Most operations                                    │
│                                                                  │
│  SUPERMAJORITY                                                   │
│  ─────────────                                                   │
│  • Two-thirds of voting members                                 │
│  • 3-node cluster: 2 required                                   │
│  • 5-node cluster: 4 required                                   │
│  • Used for: Membership changes, key operations                 │
│                                                                  │
│  UNANIMOUS                                                       │
│  ─────────                                                       │
│  • All voting members                                           │
│  • Used for: CMK rotation, cluster dissolution                  │
│                                                                  │
│  TWO_PERSON (Level 6)                                           │
│  ───────────────────                                             │
│  • Two different administrators must approve                    │
│  • Used for: Superuser operations, security policy changes      │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 Quorum Rule Matrix

| Operation | Quorum Type | Security Level |
|-----------|-------------|----------------|
| Read data | None | All |
| Write data | None (local) | All |
| User authentication | None (cached) | All |
| Create user | MAJORITY | 5+ |
| Grant privilege | MAJORITY | 5+ |
| Revoke privilege | MAJORITY | 5+ |
| Block user | MAJORITY | 5+ |
| Change password policy | SUPERMAJORITY | 6 |
| Rotate DBK | MAJORITY | 5+ |
| Rotate CMK | UNANIMOUS | 6 |
| Add node | SUPERMAJORITY | 5+ |
| Remove node | SUPERMAJORITY | 5+ |
| Fence node | MAJORITY | 5+ |
| Superuser operation | TWO_PERSON | 6 |

### 5.3 Quorum Acquisition

```python
def acquire_quorum(operation, required_type):
    """Acquire quorum for a security operation."""
    
    # 1. Determine required votes
    voting_nodes = get_voting_nodes()
    total_votes = len(voting_nodes)
    
    if required_type == 'MAJORITY':
        required_votes = (total_votes // 2) + 1
    elif required_type == 'SUPERMAJORITY':
        required_votes = (total_votes * 2 // 3) + 1
    elif required_type == 'UNANIMOUS':
        required_votes = total_votes
    
    # 2. Prepare proposal
    proposal = QuorumProposal(
        proposal_id=generate_uuid_v7(),
        operation=operation,
        required_votes=required_votes,
        expires_at=current_time() + QUORUM_TIMEOUT
    )
    
    # 3. Send to all voting nodes
    responses = []
    for node in voting_nodes:
        try:
            response = send_proposal(node, proposal)
            responses.append(response)
        except NodeUnreachable:
            continue
    
    # 4. Count votes
    votes_for = sum(1 for r in responses if r.vote == 'FOR')
    votes_against = sum(1 for r in responses if r.vote == 'AGAINST')
    
    # 5. Determine outcome
    if votes_for >= required_votes:
        return QuorumResult(
            status='ACHIEVED',
            votes_for=votes_for,
            votes_against=votes_against,
            proposal_id=proposal.proposal_id
        )
    else:
        return QuorumResult(
            status='NOT_ACHIEVED',
            votes_for=votes_for,
            votes_against=votes_against,
            reason=f"Need {required_votes}, got {votes_for}"
        )
```

### 5.4 Two-Person Rule (Level 6)

```
┌─────────────────────────────────────────────────────────────────┐
│                   TWO-PERSON RULE                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  For critical operations in Level 6:                            │
│                                                                  │
│  1. Administrator A initiates operation                          │
│     (e.g., GRANT SUPERUSER TO bob)                              │
│                                                                  │
│  2. System creates pending approval request                      │
│     • Unique request ID                                         │
│     • Operation details                                         │
│     • Requester identity (A)                                    │
│     • Expiry time (e.g., 24 hours)                              │
│                                                                  │
│  3. Notification sent to eligible approvers                     │
│     • Must be different from requester                          │
│     • Must have CLUSTER_ADMIN privilege                         │
│                                                                  │
│  4. Administrator B reviews and approves                        │
│     APPROVE PENDING REQUEST 'request-uuid';                     │
│                                                                  │
│  5. System verifies:                                             │
│     • B ≠ A (different administrator)                           │
│     • B has required privilege                                  │
│     • Request not expired                                       │
│                                                                  │
│  6. Operation executed with dual authorization logged           │
│                                                                  │
│  Operations requiring two-person rule:                          │
│  • GRANT/REVOKE SUPERUSER                                       │
│  • Modify password policy                                       │
│  • Rotate CMK                                                   │
│  • Add/remove Security Node                                     │
│  • Emergency fencing override                                   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 6. Fencing

### 6.1 Overview

Fencing prevents split-brain scenarios by ensuring only one partition can operate authoritatively.

### 6.2 Fencing Triggers

| Trigger | Action |
|---------|--------|
| Network partition detected | Minority partition fenced |
| Node health check failures | Node fenced |
| Storage unreachable | Node fenced |
| Security violation detected | Node fenced |
| Administrative action | Node fenced |

### 6.3 Fencing Mechanism

```
┌─────────────────────────────────────────────────────────────────┐
│                  FENCING MECHANISM                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Split-Brain Scenario:                                          │
│                                                                  │
│  Partition A               │              Partition B            │
│  (Nodes 1, 2, 3)           │              (Nodes 4, 5)          │
│                            │                                     │
│  ┌─────────────────────┐   │   ┌─────────────────────┐          │
│  │ Can reach 3 nodes   │   │   │ Can reach 2 nodes   │          │
│  │ Has majority (3/5)  │   │   │ No majority (2/5)   │          │
│  │                     │   │   │                     │          │
│  │ CONTINUES OPERATING │   │   │ SELF-FENCES         │          │
│  └─────────────────────┘   │   └─────────────────────┘          │
│                            │                                     │
│  ───────────────────────────────────────────────────────────────│
│                                                                  │
│  Self-Fencing Actions (Partition B):                            │
│                                                                  │
│  1. Detect loss of quorum                                       │
│  2. Stop accepting write operations                             │
│  3. Continue read operations (stale data warning)               │
│  4. Enter FENCED state                                          │
│  5. Attempt to rejoin majority                                  │
│                                                                  │
│  Optional Hard Fencing (STONITH):                               │
│                                                                  │
│  • Majority can forcibly power off minority nodes               │
│  • Via IPMI, ILO, cloud API, etc.                              │
│  • Prevents any operation by fenced nodes                       │
│  • Required for shared storage configurations                   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 6.4 Fencing State Machine

```
┌─────────────────────────────────────────────────────────────────┐
│                 NODE FENCING STATES                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐                                               │
│  │    ACTIVE    │ Normal operation                              │
│  └──────┬───────┘                                               │
│         │                                                        │
│         ├─── quorum lost ──────────► SELF_FENCED                │
│         │                                                        │
│         ├─── health check fail ───► SUSPECTED                   │
│         │                                                        │
│         ├─── admin fence ─────────► ADMIN_FENCED                │
│         │                                                        │
│         │                                                        │
│  ┌──────────────┐                                               │
│  │  SUSPECTED   │ Other nodes can't reach this node            │
│  └──────┬───────┘                                               │
│         │                                                        │
│         ├─── timeout ──────────────► FENCED                     │
│         │                                                        │
│         └─── recovers ─────────────► ACTIVE                     │
│                                                                  │
│  ┌──────────────┐                                               │
│  │ SELF_FENCED  │ Node fenced itself due to quorum loss        │
│  └──────┬───────┘                                               │
│         │                                                        │
│         └─── quorum restored ──────► REJOINING                  │
│                                                                  │
│  ┌──────────────┐                                               │
│  │    FENCED    │ Node fenced by cluster                        │
│  └──────┬───────┘                                               │
│         │                                                        │
│         └─── admin unfence ────────► REJOINING                  │
│                                                                  │
│  ┌──────────────┐                                               │
│  │  REJOINING   │ Synchronizing state                          │
│  └──────┬───────┘                                               │
│         │                                                        │
│         └─── sync complete ────────► ACTIVE                     │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 6.5 Fenced Node Behavior

When a node is fenced:

| Operation | Behavior |
|-----------|----------|
| Reads | Allowed with stale data warning |
| Writes | Rejected |
| Authentication | From cache only, no new users |
| Admin operations | Rejected |
| Inter-node communication | Continues (for rejoin) |

---

## 7. Cluster Membership

### 7.1 Adding a Node

```
┌─────────────────────────────────────────────────────────────────┐
│                   NODE JOIN PROCESS                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. Generate node identity                                       │
│     • Create node UUID                                          │
│     • Generate node certificate (or request from CA)            │
│     • Configure cluster connection                              │
│                                                                  │
│  2. Submit join request to existing node                        │
│     • Node certificate                                          │
│     • Requested role (DATA, SECURITY, etc.)                     │
│     • Network addresses                                         │
│                                                                  │
│  3. Cluster validates request                                    │
│     • Certificate signed by trusted CA                          │
│     • Cluster has capacity                                      │
│     • No UUID/address conflicts                                 │
│                                                                  │
│  4. Acquire quorum (SUPERMAJORITY)                              │
│     • All voting nodes approve membership change                │
│                                                                  │
│  5. Add node to membership                                       │
│     • Insert into sys.sec_nodes                                 │
│     • Replicate to all nodes                                    │
│     • New node now trusted                                      │
│                                                                  │
│  6. Synchronize data                                             │
│     • Security database (if Security Node)                      │
│     • User databases (if Data Node)                             │
│     • Local credential cache                                    │
│                                                                  │
│  7. Node becomes ACTIVE                                          │
│     • Can accept client connections                             │
│     • Participates in quorum (if voting)                        │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 7.2 Removing a Node

```sql
-- Graceful removal (node cooperates)
ALTER CLUSTER REMOVE NODE 'node-5' GRACEFUL;

-- Forced removal (node unresponsive)
ALTER CLUSTER REMOVE NODE 'node-5' FORCE;

-- Process:
-- 1. Acquire SUPERMAJORITY quorum
-- 2. Mark node as LEAVING
-- 3. Migrate data off node (if graceful)
-- 4. Revoke node certificate
-- 5. Remove from sys.sec_nodes
-- 6. Recalculate quorum requirements
```

### 7.3 Node Replacement

When replacing a failed node:

```
1. Remove failed node (FORCE if necessary)
2. Add replacement node with new UUID
3. New node syncs from remaining nodes
4. Certificate of failed node remains revoked
```

---

## 8. Degraded Mode

### 8.1 Security Database Unavailable

When all Security Nodes are unreachable:

```
┌─────────────────────────────────────────────────────────────────┐
│              DEGRADED MODE OPERATION                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Available Operations:                                           │
│  ─────────────────────                                           │
│  • Authentication using cached credentials                       │
│  • Read operations on data                                      │
│  • Existing sessions continue                                   │
│                                                                  │
│  Unavailable Operations:                                         │
│  ───────────────────────                                         │
│  • New user creation                                            │
│  • Password changes                                             │
│  • Privilege grants/revokes                                     │
│  • New role/group assignments                                   │
│  • External auth (if provider unreachable)                      │
│                                                                  │
│  Restrictions:                                                   │
│  ─────────────                                                   │
│  • Cache entries have limited TTL                               │
│  • Unknown users cannot authenticate                            │
│  • Audit logs queued locally                                    │
│                                                                  │
│  Recovery:                                                       │
│  ─────────                                                       │
│  • Automatic when Security Node reachable                       │
│  • Cache refresh triggered                                      │
│  • Queued audit logs synchronized                               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 8.2 Degraded Mode Policies

```sql
-- Configuration
ALTER CLUSTER SET degraded_mode.allow_cached_auth = TRUE;
ALTER CLUSTER SET degraded_mode.cache_ttl_extension = '30 minutes';
ALTER CLUSTER SET degraded_mode.max_duration = '4 hours';
ALTER CLUSTER SET degraded_mode.alert_threshold = '5 minutes';
```

---

## 9. Audit Events

### 9.1 Cluster Events

| Event | Logged Data |
|-------|-------------|
| NODE_JOINED | node_uuid, node_role, approved_by |
| NODE_LEFT | node_uuid, reason (graceful/forced) |
| NODE_FENCED | node_uuid, fence_reason, fenced_by |
| NODE_UNFENCED | node_uuid, unfenced_by |
| QUORUM_ACQUIRED | operation, votes_for, votes_against |
| QUORUM_FAILED | operation, votes_for, votes_needed |
| PARTITION_DETECTED | partition_nodes, has_quorum |
| DEGRADED_MODE_ENTERED | reason, security_nodes_status |
| DEGRADED_MODE_EXITED | duration, operations_queued |

### 9.2 Security Database Events

| Event | Logged Data |
|-------|-------------|
| SECDB_WRITE | operation_type, affected_objects |
| SECDB_REPLICATED | source_node, target_nodes, latency |
| CACHE_INVALIDATED | user_uuid, invalidation_source |
| CACHE_MISS | user_uuid, cache_state |
| TWO_PERSON_REQUESTED | operation, requester |
| TWO_PERSON_APPROVED | operation, requester, approver |
| TWO_PERSON_DENIED | operation, requester, denier, reason |

---

## 10. Configuration Reference

### 10.1 Cluster Policies

| Policy Key | Type | Default | Description |
|------------|------|---------|-------------|
| `cluster.name` | string | (required) | Cluster identifier |
| `cluster.quorum_model` | enum | MAJORITY | Default quorum type |
| `cluster.heartbeat_interval` | interval | 1s | Node heartbeat frequency |
| `cluster.heartbeat_timeout` | interval | 5s | Heartbeat failure threshold |
| `cluster.join_timeout` | interval | 60s | Max time for node join |

### 10.2 Security Database Policies

| Policy Key | Type | Default | Description |
|------------|------|---------|-------------|
| `secdb.replication_mode` | enum | SYNC | SYNC or ASYNC |
| `secdb.read_preference` | enum | PRIMARY | PRIMARY, NEAREST, ANY |
| `secdb.cache_ttl` | interval | 5m | Credential cache TTL |
| `secdb.cache_ttl_degraded` | interval | 30m | Cache TTL in degraded mode |

### 10.3 Fencing Policies

| Policy Key | Type | Default | Description |
|------------|------|---------|-------------|
| `fence.auto_fence` | boolean | true | Auto-fence on quorum loss |
| `fence.stonith_enabled` | boolean | false | Enable hard fencing |
| `fence.stonith_timeout` | interval | 30s | STONITH timeout |
| `fence.rejoin_delay` | interval | 10s | Delay before rejoin |

---

*End of Document*


## Additional Normative Sub-Documents

- **SBSEC-06.01 Quorum Proposal Canonicalization & Hashing**
- **SBSEC-06.02 Quorum Evidence, Signatures, and Audit Coupling**
