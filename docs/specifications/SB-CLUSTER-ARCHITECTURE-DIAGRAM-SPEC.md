# SB-CLUSTER-ARCHITECTURE-DIAGRAM-SPEC

## ScratchBird Cluster Architecture Visualization Specification

### Model: Single-Writer-Per-Shard (MGA)

---

# 1. Purpose

This document defines the canonical visual architecture for ScratchBird’s:

- Single-writer-per-shard cluster
  
- MGA (Firebird-style) transaction model
  
- Control-plane + data-plane separation
  
- Replication and GC safety
  
- Fencing and leader enforcement
  
- Snapshot-vector reads
  

This specification defines **logical layers, required diagram components, visual grouping rules, and flow paths**.

---

# 2. Diagram Layers

All cluster diagrams MUST follow a layered structure:

┌──────────────────────────────────────────────┐  
│ Client Layer │  
├──────────────────────────────────────────────┤  
│ Routing / Access Layer │  
├──────────────────────────────────────────────┤  
│ Control Plane Layer │  
├──────────────────────────────────────────────┤  
│ Data Plane Layer │  
├──────────────────────────────────────────────┤  
│ Storage Layer (per shard) │  
└──────────────────────────────────────────────┘

---

# 3. Logical Component Map

---

## 3.1 Client Layer

**Entities:**

- Legacy Client
  
- Native v3 Client
  
- Emulated Engine Client (Postgres/Firebird/etc.)
  
- BI Tool (ODBC/JDBC)
  

**Connections:**

- Clients connect only to:
  
  - Manager (optional mode)
    
  - Shard-aware Listener
    
  - Direct Shard Listener (if configured)
    

Clients NEVER directly interact with:

- Followers
  
- Control Plane
  
- Other shards
  

---

## 3.2 Routing / Access Layer

**Components:**

- Native Listener (Managed or Direct mode)
  
- Router Engine
  
- Session Manager
  
- Epoch Validator
  

**Required visual relationships:**

- Listener → Router
  
- Router → Control Plane (read-only metadata access)
  
- Router → Shard Leader
  

**Label Requirements:**

- Routing decision includes:
  
  - shard_id
    
  - leader_endpoint
    
  - routing_epoch
    

---

## 3.3 Control Plane Layer

This MUST be visually separate from data plane.

**Components:**

- Control Plane Log (replicated)
  
- Leader Election Module
  
- Membership Registry
  
- Shard Map
  
- Cluster Epoch Manager
  
- Security Epoch Manager
  

**Required Arrows:**

- Control Plane Log ↔ All Nodes
  
- Leader Election → Shard Leader Assignment
  
- Control Plane → Router (read-only)
  

**Explicit Visual Markers:**

- cluster_config_epoch
  
- leader_term
  
- fencing_token
  

Control Plane MUST NOT directly handle data writes.

---

## 3.4 Data Plane Layer

Each shard must be visually represented as a grouped unit:

Shard N  
 ├── Leader (Writer)  
 ├── Follower 1  
 └── Follower 2

Leader responsibilities:

- Accept writes
  
- Assign local_txn_id
  
- Emit Shard Commit Log (SCL)
  
- Publish CWM_shard
  
- Compute OST_shard
  

Followers:

- Apply SCL in order
  
- Publish RWM_shard
  
- Serve reads (if allowed)
  

---

## 3.5 Storage Layer

Within each shard group:

- Database Engine (MGA)
  
- TIP (Transaction Inventory Pages)
  
- Record Version Chains
  
- Sweep/GC Process
  
- Shard Commit Log (SCL)
  
- Snapshot Registry
  

Each shard storage must be visually independent.

---

# 4. Required Diagram Types

The architecture spec requires 6 canonical diagrams.

---

## Diagram 1 — High-Level Cluster Overview

Purpose:  
Show full layered architecture.

Must include:

- Clients
  
- Router
  
- Control Plane
  
- Multiple Shards
  
- Leader/Follower
  
- Replication arrows
  
- Epoch labeling
  

---

## Diagram 2 — Write Flow (Single Shard)

Sequence must show:

1. Client
  
2. Listener
  
3. Router
  
4. Epoch validation
  
5. Fencing token check
  
6. Shard Leader
  
7. LocalTxnId allocation
  
8. TIP update
  
9. SCL append
  
10. Follower replication
  
11. Commit acknowledgement
  

Explicitly label:

- GTXID = (shard_id, local_txn_id)
  
- leader_term validation
  
- fencing_token enforcement
  

---

## Diagram 3 — Cross-Shard Read Flow

Sequence must show:

1. Router pins cluster_config_epoch
  
2. Router reads CWM_shard from each shard
  
3. Builds snapshot_vector
  
4. Sends parallel read requests
  
5. Each shard executes under its snapshot boundary
  
6. Router merges results
  

Label:

- snapshot_vector[shard]
  
- CWM_shard
  
- No global WAL ordering
  

---

## Diagram 4 — Replication Pipeline

Must show:

Leader:

- Local commit
  
- SCL append
  

Follower:

- SCL receive
  
- Apply in local_txn_id order
  
- Update RWM_shard
  

Label:

- Strict ordering guarantee
  
- Idempotent apply
  

---

## Diagram 5 — GC / Sweep Safety Model

Must show:

- Snapshot Registry
  
- Active Snapshots
  
- OST_shard
  
- RWM_shard
  
- GC_safe_shard = min(OST_shard, RWM_shard)
  
- Sweep process blocked until safe
  

Clearly show:

- Record versions
  
- Visibility boundaries
  

---

## Diagram 6 — Leader Failover

Sequence:

1. Leader crash
  
2. Lease expiration
  
3. Control Plane election
  
4. leader_term increment
  
5. Fencing token update
  
6. New leader begins writes
  
7. Old leader rejected on write attempt
  

Label:

- fencing_token mismatch rejection

---

# 5. Visual Notation Requirements

## 5.1 Colors (recommended)

- Client Layer: Blue
  
- Router Layer: Teal
  
- Control Plane: Purple
  
- Leader Nodes: Green
  
- Followers: Light Green
  
- Storage: Gray
  
- Fencing / Epoch markers: Red highlights
  

---

## 5.2 Symbol Conventions

- Cylinders: Persistent Storage
  
- Double-border: Authoritative control-plane components
  
- Dotted lines: Read-only metadata access
  
- Solid lines: Write/data flow
  
- Dashed arrows: Replication flow
  
- Lightning bolt icon: Leader failure event
  
- Shield icon: Fencing enforcement point
  

---

# 6. Mandatory Labels

Every architecture diagram MUST label:

- cluster_config_epoch
  
- leader_term
  
- fencing_token
  
- CWM_shard
  
- OST_shard
  
- RWM_shard
  
- GC_safe_shard
  
- GTXID
  

These are core correctness anchors.

---

# 7. Non-Goals to Avoid in Diagrams

Do NOT show:

- Global WAL
  
- Global XID counter
  
- Shared storage between shards
  
- Multi-writer per shard
  
- Implicit 2PC (unless future diagram)
  

---

# 8. Diagram Generation Format Recommendations

To make diagrams version-controlled:

Recommended formats:

- Mermaid (for Markdown)
  
- PlantUML
  
- Draw.io XML
  
- SVG committed in repo
  

Optional:

- PNG export for release notes

---

# 9. Example Mermaid Skeleton (High-Level)


---

# 10. Deliverables

Before Beta, the following diagrams must exist:

- `/docs/diagrams/cluster_overview.svg`
  
- `/docs/diagrams/write_flow.svg`
  
- `/docs/diagrams/read_flow.svg`
  
- `/docs/diagrams/replication_flow.svg`
  
- `/docs/diagrams/gc_model.svg`
  
- `/docs/diagrams/failover_flow.svg`
  

---

# 11. Acceptance Criteria

Architecture diagrams are considered complete when:

- All invariants are visually represented.
  
- Epoch and fencing markers are explicit.
  
- Replication ordering is clear.
  
- GC safety logic is visible.
  
- Leader failover behavior is explicit.
  
- No WAL-based assumptions appear.
  

---

# 12. Summary

The cluster visual architecture must communicate:

- Deterministic single-writer enforcement
  
- MGA-consistent snapshot model
  
- Epoch-based routing
  
- Safe replication
  
- Safe garbage collection
  
- Controlled failover
  

The diagrams must reinforce that:

ScratchBird Cluster is correctness-first, deterministic, and security-enforced.
