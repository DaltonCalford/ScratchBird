# ScratchBird Cluster Implementation Boundary

**Alpha / Beta / GA Feature Requirements**

---

## 1. Purpose and Scope

This document defines the **implementation boundary** for ScratchBird cluster features across three release milestones:
- **Alpha**: Minimum viable cluster (internal testing, proof of concept)
- **Beta**: Production-ready cluster (customer pilots, early adopters)
- **GA (General Availability)**: Full-featured production cluster

**Scope**:
- Feature categorization by release milestone
- MUST vs SHOULD vs MAY requirements per milestone
- Conformance criteria for each release
- Feature dependency graph
- Testing requirements per milestone

---

## 2. Release Milestone Definitions

### 2.1 Alpha Release

**Purpose**: Internal validation, proof of concept, architectural validation

**Audience**: Internal development team, limited alpha testers

**Characteristics**:
- Single-datacenter deployment
- Manual configuration and operations
- Limited production features
- Reduced security hardening (development/testing)
- No SLA guarantees

**Duration**: 3-6 months

**Exit Criteria**: Core cluster features functional, basic security in place

---

### 2.2 Beta Release

**Purpose**: Customer pilots, early adopter production deployments

**Audience**: Friendly customers, early adopters, internal production

**Characteristics**:
- Production-ready security and reliability
- Operational tooling and monitoring
- SLA targets (but not guaranteed)
- Limited scalability (3-10 nodes, 16 shards)

**Duration**: 6-12 months

**Exit Criteria**: Production deployments stable, customer feedback incorporated

---

### 2.3 GA (General Availability) Release

**Purpose**: General production use, enterprise deployments

**Audience**: All customers, enterprise production

**Characteristics**:
- Full feature set
- SLA guarantees
- Enterprise-grade security and compliance
- Full scalability (100+ nodes, 256+ shards)
- Comprehensive operational tooling

**Duration**: Ongoing (maintenance and enhancement)

**Entry Criteria**: Beta production-proven, compliance certifications complete

---

## 3. Feature Matrix by Release

### 3.1 Notation

| Symbol | Meaning | Conformance |
|--------|---------|-------------|
| ✓ **REQUIRED** | MUST be implemented | Non-conformant if missing |
| ○ **RECOMMENDED** | SHOULD be implemented | Conformant but degraded |
| △ **OPTIONAL** | MAY be implemented | No impact on conformance |
| — **NOT REQUIRED** | Not expected in this release | No implementation needed |

---

## 4. SBCLUSTER-00: Guiding Principles

| Principle | Alpha | Beta | GA | Notes |
|-----------|-------|------|-----|-------|
| **Engine Authority** | ✓ | ✓ | ✓ | Fundamental (all releases) |
| **Shard-Local MVCC** | ✓ | ✓ | ✓ | Fundamental (all releases) |
| **No Cross-Shard Txns** | ✓ | ✓ | ✓ | Architectural constraint (all) |
| **Push Compute to Data** | ○ | ✓ | ✓ | Alpha may do naive routing |
| **Identical Security Config** | ○ | ✓ | ✓ | Alpha may use simplified security |
| **One-Way Upgrades** | — | ✓ | ✓ | Not needed in Alpha (no upgrades) |
| **Trust Boundary Enforcement** | ○ | ✓ | ✓ | Alpha may backup keys (dev/test) |
| **Immutable Audit Chain** | — | ✓ | ✓ | Alpha may use simple logging |
| **Consensus Over Configuration** | ✓ | ✓ | ✓ | Raft required (all releases) |
| **Observability by Design** | ○ | ✓ | ✓ | Alpha: basic metrics only |

---

## 5. SBCLUSTER-01: Cluster Configuration Epoch (CCE)

| Feature | Alpha | Beta | GA | Notes |
|---------|-------|------|-----|-------|
| **Epoch record structure** | ✓ | ✓ | ✓ | Core data structure |
| **Monotonic epoch numbers** | ✓ | ✓ | ✓ | MUST (all releases) |
| **Raft distribution** | ✓ | ✓ | ✓ | MUST (all releases) |
| **Hash chaining (prev_epoch_hash)** | — | ✓ | ✓ | Beta+ security requirement |
| **Cryptographic signatures** | — | ✓ | ✓ | Beta+ security requirement |
| **Epoch validation** | ○ | ✓ | ✓ | Alpha: basic validation |
| **CCE version history** | — | ✓ | ✓ | Beta+ for audit/rollback |
| **Automated epoch upgrades** | — | ○ | ✓ | GA: zero-downtime upgrades |

**Alpha Simplifications**:
- No hash chaining or signatures (trust assumed)
- Manual epoch creation and distribution
- No version history retention

**Beta Requirements**:
- Full cryptographic sealing (hash chain + signatures)
- Automated validation and rejection of invalid epochs

**GA Enhancements**:
- Automated rolling upgrades with epoch propagation
- Epoch rollback (emergency recovery)

---

## 6. SBCLUSTER-02: Membership and Identity

| Feature | Alpha | Beta | GA | Notes |
|---------|-------|------|-----|-------|
| **Node UUIDv7 identity** | ✓ | ✓ | ✓ | MUST (all releases) |
| **Ed25519 keypairs** | ○ | ✓ | ✓ | Alpha may use simpler auth |
| **X.509 certificates** | — | ✓ | ✓ | Beta+ mTLS requirement |
| **mTLS authentication** | — | ✓ | ✓ | Beta+ MUST |
| **Node roles (ENGINE, COORDINATOR)** | ✓ | ✓ | ✓ | MUST (all releases) |
| **Raft-based membership** | ✓ | ✓ | ✓ | MUST (all releases) |
| **Graceful node join/leave** | ○ | ✓ | ✓ | Alpha may require restart |
| **Certificate renewal** | — | ○ | ✓ | GA: automated renewal |
| **Certificate revocation (CRL)** | — | ✓ | ✓ | Beta+ security requirement |

**Alpha Simplifications**:
- Shared secret or simple token auth (not mTLS)
- No certificate infrastructure
- Manual node addition/removal

**Beta Requirements**:
- Full PKI infrastructure (CA, certificates, CRL)
- mTLS for all inter-node communication
- Graceful join/leave via Raft

**GA Enhancements**:
- Automated certificate renewal (before expiry)
- Online certificate rotation (no downtime)

---

## 7. SBCLUSTER-03: CA Policy

| Feature | Alpha | Beta | GA | Notes |
|---------|-------|------|-----|-------|
| **Cluster CA** | — | ✓ | ✓ | Beta+ MUST |
| **Certificate issuance** | — | ✓ | ✓ | Beta+ MUST |
| **Certificate lifecycle** | — | ✓ | ✓ | Beta+ (issue, renew, revoke) |
| **CRL publication** | — | ✓ | ✓ | Beta+ MUST |
| **CA private key protection** | — | ○ | ✓ | GA: HSM required |
| **Intermediate CAs** | — | — | △ | GA: optional delegation |
| **Certificate transparency logs** | — | — | △ | GA: optional (compliance) |

**Alpha Simplifications**:
- No CA infrastructure (not needed without mTLS)

**Beta Requirements**:
- Root CA with private key protection (software-based)
- Certificate issuance and revocation
- CRL published and distributed via CCE

**GA Enhancements**:
- CA private key in HSM (Hardware Security Module)
- Automated certificate lifecycle management
- Optional intermediate CAs for delegation

---

## 8. SBCLUSTER-04: Security Bundle

| Feature | Alpha | Beta | GA | Notes |
|---------|-------|------|-----|-------|
| **Security bundle structure** | — | ✓ | ✓ | Beta+ MUST |
| **EncryptionPolicy** | ○ | ✓ | ✓ | Alpha: basic encryption |
| **TLSPolicy** | — | ✓ | ✓ | Beta+ MUST |
| **AuthenticationPolicy** | ○ | ✓ | ✓ | Alpha: simplified auth |
| **RLS (Row-Level Security)** | — | ○ | ✓ | GA: MUST, Beta: partial |
| **CLS (Column-Level Security)** | — | — | ○ | GA: RECOMMENDED |
| **AuditPolicy** | — | ✓ | ✓ | Beta+ MUST |
| **Bundle versioning** | — | ✓ | ✓ | Beta+ MUST |
| **Bundle distribution via CCE** | — | ✓ | ✓ | Beta+ MUST |

**Alpha Simplifications**:
- Minimal security: basic encryption, simple auth
- No formalized security bundle
- No RLS/CLS

**Beta Requirements**:
- Complete security bundle with all policies
- Versioned and distributed via CCE
- RLS partially implemented (basic predicates)

**GA Enhancements**:
- Full RLS with complex predicates
- CLS for sensitive column masking
- Policy templates for common scenarios

---

## 9. SBCLUSTER-05: Sharding

| Feature | Alpha | Beta | GA | Notes |
|---------|-------|------|-----|-------|
| **Shard concept** | ✓ | ✓ | ✓ | MUST (all releases) |
| **Fixed shard count** | ✓ | ✓ | ✓ | MUST (all releases) |
| **Consistent hashing** | ✓ | ✓ | ✓ | MUST (all releases) |
| **Shard map** | ✓ | ✓ | ✓ | MUST (all releases) |
| **Shard assignment** | ✓ | ✓ | ✓ | MUST (all releases) |
| **Manual shard migration** | — | ○ | ✓ | GA: MUST, Beta: manual only |
| **Automated shard rebalancing** | — | — | △ | GA: OPTIONAL |
| **Dynamic resharding** | — | — | — | Post-GA (future work) |

**Alpha Simplifications**:
- Fixed shard count (e.g., 16 shards)
- No shard migration (cluster rebuild if needed)

**Beta Requirements**:
- Manual shard migration (operational tool)
- Shard map distributed via CCE

**GA Enhancements**:
- Automated shard rebalancing on node add/remove
- Shard migration with zero downtime

---

## 10. SBCLUSTER-06: Distributed Query

| Feature | Alpha | Beta | GA | Notes |
|---------|-------|------|-----|-------|
| **Query coordinator** | ✓ | ✓ | ✓ | MUST (all releases) |
| **Single-shard routing** | ✓ | ✓ | ✓ | MUST (all releases) |
| **Multi-shard scatter-gather** | ○ | ✓ | ✓ | Alpha: may be limited |
| **Push-down optimization** | ○ | ✓ | ✓ | Alpha: naive execution OK |
| **Parallel shard execution** | ○ | ✓ | ✓ | Alpha: serial OK |
| **Result merging** | ○ | ✓ | ✓ | Alpha: basic merging |
| **Distributed aggregations** | — | ✓ | ✓ | Beta+ MUST |
| **Distributed JOINs** | — | ○ | ✓ | GA: full support |
| **Query plan caching** | — | — | △ | GA: OPTIONAL |
| **Adaptive query routing** | — | — | △ | GA: OPTIONAL |

**Alpha Simplifications**:
- Basic single-shard and broadcast queries
- No advanced push-down optimizations
- Serial execution acceptable

**Beta Requirements**:
- Full multi-shard support
- Parallel execution
- Distributed aggregations (SUM, COUNT, AVG, etc.)

**GA Enhancements**:
- Complex distributed JOINs
- Query plan caching
- Adaptive routing based on load

---

## 11. SBCLUSTER-07: Replication

| Feature | Alpha | Beta | GA | Notes |
|---------|-------|------|-----|-------|
| **Replication factor = 2** | ○ | ✓ | ✓ | Alpha: RF=1 acceptable |
| **Primary/replica model** | ○ | ✓ | ✓ | Alpha: no replication OK |
| **Asynchronous replication** | — | ✓ | ✓ | Beta+ MUST |
| **Write-after log (WAL) streaming** | — | ✓ | ✓ | Beta+ MUST |
| **Replica failover** | — | ✓ | ✓ | Beta+ MUST |
| **Replication lag monitoring** | — | ✓ | ✓ | Beta+ MUST |
| **Synchronous replication** | — | — | △ | GA: OPTIONAL |
| **Quorum reads** | — | — | △ | GA: OPTIONAL |

**Alpha Simplifications**:
- No replication (RF=1, single copy per shard)
- Acceptable for development/testing

**Beta Requirements**:
- RF=2 (primary + 1 replica)
- Asynchronous write-after log (WAL) streaming
- Automated failover on primary failure

**GA Enhancements**:
- Optional synchronous replication (strong consistency)
- Quorum reads for critical applications

---

## 12. SBCLUSTER-08: Backup and Restore

| Feature | Alpha | Beta | GA | Notes |
|---------|-------|------|-----|-------|
| **Per-shard backups** | ○ | ✓ | ✓ | Alpha: manual OK |
| **Full backup** | ○ | ✓ | ✓ | Alpha: manual OK |
| **Incremental backup (write-after log (WAL))** | — | ○ | ✓ | GA: MUST |
| **Cluster-consistent snapshots** | — | ○ | ✓ | GA: MUST |
| **Trust boundary enforcement** | — | ✓ | ✓ | Beta+ MUST (no key backup) |
| **Backup encryption** | — | ✓ | ✓ | Beta+ MUST |
| **Automated backup scheduling** | — | ○ | ✓ | GA: MUST via scheduler |
| **Point-in-time recovery (PITR)** | — | — | ○ | GA: RECOMMENDED |
| **Cross-region backup** | — | — | △ | GA: OPTIONAL |

**Alpha Simplifications**:
- Manual backups (dump/restore scripts)
- No encryption requirement (dev/test)

**Beta Requirements**:
- Automated per-shard backups
- Backup encryption (AES-256-GCM)
- Trust boundary: NO certificate/key backup

**GA Enhancements**:
- Incremental backups (write-after log (WAL)-based)
- Point-in-time recovery
- Automated scheduling via SBCLUSTER-09

---

## 13. SBCLUSTER-09: Scheduler

| Feature | Alpha | Beta | GA | Notes |
|---------|-------|------|-----|-------|
| **Scheduler control plane** | — | ○ | ✓ | GA: MUST for automation |
| **Scheduler agents** | — | ○ | ✓ | GA: MUST |
| **Job classes (LOCAL_SAFE, etc.)** | — | ○ | ✓ | GA: MUST |
| **Cron scheduling** | — | ○ | ✓ | GA: MUST |
| **Partition rules** | — | ○ | ✓ | GA: MUST |
| **Job dependencies** | — | — | △ | GA: OPTIONAL |
| **Job retry and failure handling** | — | ○ | ✓ | GA: MUST |

**Alpha Simplifications**:
- No scheduler (manual operations via scripts)

**Beta Requirements**:
- Basic scheduler for maintenance jobs (sweep/GC, etc.)
- Cron-like scheduling

**GA Enhancements**:
- Full scheduler with job dependencies
- Advanced partition rules
- Failure handling and alerting

---

## 14. SBCLUSTER-10: Observability and Audit

| Feature | Alpha | Beta | GA | Notes |
|---------|-------|------|-----|-------|
| **Metrics collection** | ○ | ✓ | ✓ | Alpha: basic metrics |
| **Prometheus exposition** | ○ | ✓ | ✓ | Alpha: optional |
| **OpenTelemetry support** | — | ○ | ✓ | GA: RECOMMENDED |
| **Distributed tracing** | — | ○ | ✓ | GA: RECOMMENDED |
| **Audit logging** | — | ✓ | ✓ | Beta+ MUST |
| **Cryptographic audit chain** | — | ✓ | ✓ | Beta+ MUST |
| **Audit chain verification** | — | ✓ | ✓ | Beta+ MUST |
| **Churn metrics** | — | ✓ | ✓ | Beta+ MUST |
| **Peer observation** | — | ✓ | ✓ | Beta+ MUST |
| **Alert rules** | — | ○ | ✓ | GA: MUST |
| **Grafana dashboards** | — | — | ○ | GA: RECOMMENDED |

**Alpha Simplifications**:
- Basic metrics (query count, latency)
- Simple logging (no audit chain)

**Beta Requirements**:
- Prometheus-compatible metrics
- Cryptographic audit chain (hash-linked, signed)
- Churn metrics and peer observation

**GA Enhancements**:
- Full OpenTelemetry support (traces, metrics, logs)
- Comprehensive alert rules
- Pre-built Grafana dashboards

---

## 15. Conformance Summary

### 15.1 Alpha Conformance

**MUST Implement**:
- SBCLUSTER-00: Core principles (engine authority, shard-local MVCC)
- SBCLUSTER-01: Basic CCE (no crypto)
- SBCLUSTER-02: Node identity and Raft membership (no mTLS)
- SBCLUSTER-05: Sharding (fixed shard count)
- SBCLUSTER-06: Basic distributed query (single-shard + broadcast)

**MAY Omit**:
- SBCLUSTER-03: CA Policy (no PKI needed)
- SBCLUSTER-04: Full security bundle (simplified security OK)
- SBCLUSTER-07: Replication (RF=1 acceptable)
- SBCLUSTER-08: Automated backups (manual OK)
- SBCLUSTER-09: Scheduler (not needed)
- SBCLUSTER-10: Audit chain (simple logging OK)

**Acceptable for**: Internal testing, proof of concept

---

### 15.2 Beta Conformance

**MUST Implement**:
- All Alpha requirements (MUST)
- SBCLUSTER-02: mTLS authentication
- SBCLUSTER-03: Full CA infrastructure
- SBCLUSTER-04: Complete security bundle
- SBCLUSTER-06: Multi-shard queries with push-down
- SBCLUSTER-07: Replication (RF=2, write-after log (WAL) streaming, failover)
- SBCLUSTER-08: Automated encrypted backups
- SBCLUSTER-10: Cryptographic audit chain

**SHOULD Implement**:
- SBCLUSTER-09: Basic scheduler

**Acceptable for**: Customer pilots, production (with SLA caveats)

---

### 15.3 GA Conformance

**MUST Implement**:
- All Beta requirements (MUST + SHOULD)
- SBCLUSTER-04: Full RLS, CLS
- SBCLUSTER-06: Distributed JOINs
- SBCLUSTER-07: Replication lag < 10s
- SBCLUSTER-08: Incremental backups (PITR)
- SBCLUSTER-09: Full scheduler with dependencies
- SBCLUSTER-10: OpenTelemetry, alert rules

**RECOMMENDED**:
- SBCLUSTER-03: CA in HSM
- SBCLUSTER-07: Synchronous replication option
- SBCLUSTER-10: Grafana dashboards

**Acceptable for**: General production, enterprise deployments

---

## 16. Feature Dependencies

### 16.1 Dependency Graph

```
SBCLUSTER-01 (CCE)
    ├─► SBCLUSTER-02 (Membership) [Raft required]
    ├─► SBCLUSTER-03 (CA) [CCE distributes certs]
    └─► SBCLUSTER-04 (Security Bundle) [CCE distributes bundle]
        └─► SBCLUSTER-10 (Audit) [Audit policy in bundle]

SBCLUSTER-05 (Sharding)
    └─► SBCLUSTER-06 (Distributed Query) [Queries route to shards]
        └─► SBCLUSTER-07 (Replication) [Replicas serve queries]

SBCLUSTER-08 (Backup)
    ├─► SBCLUSTER-05 (Sharding) [Per-shard backups]
    └─► SBCLUSTER-09 (Scheduler) [Automated backup jobs]

SBCLUSTER-10 (Observability)
    └─► All modules (metrics, audit)
```

**Critical Path**: SBCLUSTER-01 → SBCLUSTER-02 → SBCLUSTER-05 → SBCLUSTER-06

**Optional Path**: SBCLUSTER-09 (Scheduler) can be omitted in Alpha/Beta

---

## 17. Testing Requirements by Release

### 17.1 Alpha Testing

**Required Tests**:
- [ ] Raft consensus (leader election, log replication)
- [ ] CCE distribution
- [ ] Shard routing (single-shard queries)
- [ ] Basic query execution

**Optional Tests**:
- Multi-shard queries
- Failover scenarios

**Quality Bar**: Functional correctness (no performance/scale requirements)

---

### 17.2 Beta Testing

**Required Tests**:
- [ ] All Alpha tests (MUST pass)
- [ ] mTLS authentication
- [ ] Certificate validation and CRL
- [ ] Multi-shard query execution
- [ ] Replication and failover
- [ ] Backup and restore (including trust boundary)
- [ ] Audit chain verification

**Performance Tests**:
- [ ] 1000 QPS single-shard queries
- [ ] 100 QPS multi-shard queries
- [ ] 3-node cluster stability (24 hours)

**Quality Bar**: Production-ready functionality, basic performance

---

### 17.3 GA Testing

**Required Tests**:
- [ ] All Beta tests (MUST pass)
- [ ] RLS/CLS enforcement
- [ ] Distributed JOINs
- [ ] Incremental backup and PITR
- [ ] Scheduler job execution
- [ ] Alert firing and resolution

**Performance Tests**:
- [ ] 100K QPS single-shard queries
- [ ] 10K QPS multi-shard queries
- [ ] 10-node cluster stability (7 days)
- [ ] Scalability to 100 nodes

**Chaos Tests**:
- [ ] Random node failures
- [ ] Network partitions
- [ ] Certificate expiry
- [ ] Disk failures

**Quality Bar**: Enterprise production-grade

---

## 18. Migration Path

### 18.1 Alpha → Beta Migration

**Breaking Changes**:
- Authentication: Shared secret → mTLS (requires certificate provisioning)
- CCE: No crypto → Hash chain + signatures (incompatible epoch format)
- Replication: RF=1 → RF=2 (data migration required)

**Migration Steps**:
1. Backup all data
2. Provision CA and certificates
3. Upgrade CCE to include hash chain/signatures
4. Add replica nodes
5. Enable replication
6. Verify audit chain

**Downtime**: Required (no in-place upgrade Alpha → Beta)

---

### 18.2 Beta → GA Migration

**Non-Breaking Changes**:
- Feature additions (RLS, CLS, PITR, scheduler)
- No protocol changes

**Migration Steps**:
1. Rolling upgrade (node-by-node)
2. CCE update with new GA features
3. Enable RLS/CLS policies
4. Configure scheduler jobs

**Downtime**: Not required (rolling upgrade supported)

---

## 19. Quick Reference Card

| Specification | Alpha | Beta | GA |
|---------------|-------|------|-----|
| **SBCLUSTER-00** | Partial | Full | Full |
| **SBCLUSTER-01** | Basic CCE | Crypto CCE | Crypto CCE |
| **SBCLUSTER-02** | No mTLS | mTLS | mTLS |
| **SBCLUSTER-03** | — | CA | CA + HSM |
| **SBCLUSTER-04** | Minimal | Full | Full + RLS/CLS |
| **SBCLUSTER-05** | Fixed shards | Fixed shards | Rebalancing |
| **SBCLUSTER-06** | Basic | Full | Full + JOINs |
| **SBCLUSTER-07** | No replication | RF=2 | RF=2 + sync option |
| **SBCLUSTER-08** | Manual | Automated | Automated + PITR |
| **SBCLUSTER-09** | — | Basic | Full |
| **SBCLUSTER-10** | Metrics | Metrics + Audit | Full OTEL + Audit |

---

## 20. Document Maintenance

**Ownership**: Chief Architect

**Review**: Before each release milestone

**Approval Required**: Chief Architect, Engineering Leads

**Last Updated**: 2026-01-02

---

**Document Status**: NORMATIVE (Implementation Requirements)

**Version**: 1.0

---

**End of Implementation Boundary Document**
