# Database Lifecycle and Upgrade Plan (Embedded → Server → Cluster Roles)

Purpose: Define a staged design and implementation plan for creating databases and upgrading them through operating modes and cluster roles. This serves both as an internal implementation guide and end-user documentation for DB creation and promotion.

## 1) Operating Modes and Role Taxonomy

### 1.1 Modes
- **Embedded**: Library-linked, single-writer, minimal security guarantees.  
- **Server**: Networked engine, authenticated clients, engine-enforced security.  
- **Cluster Member**: Node participates in cluster governance and policy distribution.

### 1.2 Cluster Roles
- **OLTP**: Low-latency transactional workload, MGA primary.  
- **OLAP**: Read-optimized, analytical workloads, stronger caching and batch operations.  
- **SHADOW**: Hot standby for promotion.  
- **SECURITY**: Cluster authority for identities/policies.  
- **REPLICATION AUTHORITATIVE MASTER**: Primary replication source, quorum-aware.

Each role is a policy bundle, not a separate codebase. Role selection gates features, defaults, and enforcement.

## 2) Upgrade Path: Embedded → Server → Cluster

### 2.1 Embedded → Server Upgrade
**Goal**: Enable networked access and engine-side security enforcement.

Required components:
- Listener layer enabled (TLS/mTLS configurable).  
- Authentication enabled (local or external).  
- Transaction security context bound to session/AuthKey.  
- Audit logging enabled (configurable sinks).  
- UUID resolver view enabled (object UUID ↔ path/name/type).  

Operational steps:
1) Set server mode flag in DB metadata/config.  
2) Initialize auth subsystem and session catalog.  
3) Enable audit policy (local or external sink).  
4) Validate schema/object UUID resolver indexes.  
5) Restart database in server mode and validate connection/login flow.  

### 2.2 Server → Cluster Member Upgrade
**Goal**: Join cluster governance and policy distribution.

Required components:
- Cluster identity + membership registry.  
- Quorum verification (configurable).  
- Policy epoch tracking (global + per-table).  
- AuthKey cache policy with quorum gating.  
- Audit routing and policy distribution.  

Operational steps:
1) Register database UUID with cluster authority.  
2) Set cluster role (OLTP/OLAP/SHADOW/SECURITY/REPLICATION MASTER).  
3) Configure quorum threshold (N-of-M) and failure mode.  
4) Enable policy epoch synchronization.  
5) Validate AuthKey cache behavior against quorum policy.  

## 3) Role-Specific Implementation Requirements

### 3.1 OLTP Role
- MGA transaction path optimized; row-level locks and visibility.  
- Strict per-transaction security context.  
- RLS enforcement and constraint checks.  
- Short-latency audit logging (async allowed by policy).

### 3.2 OLAP Role
- Read-optimized caching and batch execution.  
- Strong plan caching with policy epoch invalidation.  
- RLS enforcement required; configurable metadata redaction.  
- Optional higher query result caps.

### 3.3 SHADOW Role
- WAL optional; MGA-based recovery still primary.  
- Must support promotion workflow with audit and quorum requirements.  
- Reject writes unless in pass-through or promotion mode.

### 3.4 SECURITY Role
- Authoritative storage of identities, roles, groups, AuthKeys.  
- Policy distribution and signing.  
- Quorum participation required for security operations.  
- Local cache for other members is subordinate to SECURITY nodes.

### 3.5 REPLICATION AUTHORITATIVE MASTER Role
- Supports replication stream output (protocol choice).  
- Quorum-aware commit/authorization decisions (policy-driven).  
- Audit and replication logs must be retained per policy.

## 4) Security-Level Alignment per Upgrade Phase

Each upgrade step must verify security level prerequisites (see security matrix in the findings report). Example:
- Embedded: Level 0 or Level 1.  
- Server: Level 1+ (Auth required), Level 3+ if RLS enabled.  
- Cluster: Level 4+ (audit), Level 5+ (TLS/mTLS), Level 6 (quorum + centralized authority).

## 5) Object Identity and Namespace Management

All objects are UUID-based; name/path are mutable metadata.

Required infrastructure before promotion:
- Unified UUID resolver view with indexes.  
- Object rename/move support (schema relocation).  
- Dependency tracking and approval token enforcement.

## 6) End-User Creation Workflow (High-Level)

### 6.1 Create Embedded DB
1) `CREATE DATABASE` (embedded profile).  
2) Initialize catalogs and UUID resolver indexes.  
3) Set default namespace and ownership rules.  

### 6.2 Promote to Server
1) Enable listener + auth + audit.  
2) Create initial users/roles/groups.  
3) Verify security context binding.  

### 6.3 Join Cluster (Role Assignment)
1) Register with cluster authority.  
2) Select role and security policy level.  
3) Configure quorum and cache policies.  
4) Validate audit/replication posture.  

## 7) Implementation Milestones (Phased)

Phase A: Embedded baseline
- Catalog + UUID resolver view.  
- Basic auth placeholders (local).  
- MGA-only transaction semantics.

Phase B: Server enablement
- Listener + auth integration.  
- Session/AuthKey binding.  
- Audit sinks and tamper-evidence.  

Phase C: Cluster membership
- Quorum checks + policy epochs.  
- Security authority role.  
- Role-based configuration bundles.

Phase D: Role hardening
- OLAP/OLTP optimizations.  
- SHADOW promotion flows.  
- Replication master streams.

## 8) Open Decisions (Architect-Configurable)

- Quorum thresholds and failure modes.  
- Audit sink selection and encryption key custody.  
- Default namespace precedence and role-switch behavior.  
- Metadata visibility and redaction levels.  
- Pass-through migration scope.

---

This plan should be updated as features land and as the security matrix is refined.
