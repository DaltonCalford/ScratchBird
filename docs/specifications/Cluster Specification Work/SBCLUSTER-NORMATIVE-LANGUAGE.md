# ScratchBird Cluster Specifications - Normative Language Guide

**RFC 2119 Compliance Document**

---

## 1. Purpose

This document defines the normative language used throughout the ScratchBird cluster specification suite (SBCLUSTER-00 through SBCLUSTER-10). All specifications SHALL be interpreted according to the definitions in RFC 2119: "Key words for use in RFCs to Indicate Requirement Levels."

---

## 2. Normative Keywords (RFC 2119)

The key words **MUST**, **MUST NOT**, **REQUIRED**, **SHALL**, **SHALL NOT**, **SHOULD**, **SHOULD NOT**, **RECOMMENDED**, **MAY**, and **OPTIONAL** in the cluster specifications are to be interpreted as described in RFC 2119.

### 2.1 Absolute Requirements

#### MUST / SHALL / REQUIRED

**Definition**: This word, or the terms "SHALL" or "REQUIRED", mean that the definition is an absolute requirement of the specification.

**Implication**: Implementations that do not conform to MUST/SHALL/REQUIRED clauses are **non-compliant** and **MUST NOT** claim conformance to the specification.

**Usage**: Reserved for:
- Security-critical constraints (e.g., "Certificates MUST be validated against the cluster CA")
- Data integrity requirements (e.g., "Audit events MUST include prev_event_hash")
- Safety invariants (e.g., "Epoch numbers MUST be monotonically increasing")

**Examples**:
```
✓ CORRECT: "Nodes MUST validate CCE signatures before applying configuration."
✗ INCORRECT: "Nodes should validate CCE signatures." (too weak for security)
```

#### MUST NOT / SHALL NOT

**Definition**: This phrase, or the phrase "SHALL NOT", mean that the definition is an absolute prohibition of the specification.

**Implication**: Implementations that violate MUST NOT/SHALL NOT clauses are **non-compliant** and introduce **critical defects**.

**Usage**: Reserved for:
- Forbidden operations (e.g., "Nodes MUST NOT downgrade to older CCE epochs")
- Security violations (e.g., "Private keys MUST NOT be included in backups")
- Invariant violations (e.g., "Transactions MUST NOT span multiple shards")

**Examples**:
```
✓ CORRECT: "Backup procedures MUST NOT include private keys or certificates."
✗ INCORRECT: "Backup procedures should not include private keys." (too weak)
```

---

### 2.2 Strong Recommendations

#### SHOULD / RECOMMENDED

**Definition**: This word, or the adjective "RECOMMENDED", mean that there may exist valid reasons in particular circumstances to ignore a particular item, but the full implications must be understood and carefully weighed before choosing a different course.

**Implication**: Implementations MAY deviate from SHOULD/RECOMMENDED clauses, but:
1. Deviation MUST be explicitly documented
2. Implications MUST be analyzed and understood
3. Deviation SHOULD be justified in implementation notes

**Usage**: Reserved for:
- Performance optimizations (e.g., "Query coordinators SHOULD cache shard routing tables")
- Operational best practices (e.g., "Clusters SHOULD use at least 3 nodes for HA")
- Non-critical features (e.g., "Metrics SHOULD be exported in Prometheus format")

**Examples**:
```
✓ CORRECT: "Replication lag SHOULD be monitored via scratchbird_replication_lag_seconds metric."
✓ CORRECT: "Nodes SHOULD run health checks every 15 seconds." (can be tuned)
```

#### SHOULD NOT / NOT RECOMMENDED

**Definition**: This phrase, or the phrase "NOT RECOMMENDED", mean that there may exist valid reasons in particular circumstances when the particular behavior is acceptable or even useful, but the full implications should be understood and the case carefully weighed before implementing any behavior described with this label.

**Implication**: Same as SHOULD (strong recommendation against, but not forbidden).

**Usage**: Reserved for:
- Discouraged patterns (e.g., "Administrators SHOULD NOT manually edit epoch records")
- Performance anti-patterns (e.g., "Queries SHOULD NOT use SELECT * on sharded tables")

**Examples**:
```
✓ CORRECT: "Applications SHOULD NOT issue queries that require full table scans across all shards."
```

---

### 2.3 Optional Features

#### MAY / OPTIONAL

**Definition**: This word, or the adjective "OPTIONAL", mean that an item is truly optional. One vendor may choose to include the item because a particular marketplace requires it or because the vendor feels that it enhances the product while another vendor may omit the same item.

**Implication**:
- Implementations MAY choose to implement or omit
- Omission does NOT affect conformance
- Presence MUST be detectable (feature discovery)

**Usage**: Reserved for:
- Optional extensions (e.g., "Nodes MAY implement query result caching")
- Optional optimizations (e.g., "Coordinators MAY use adaptive query routing")
- Optional integrations (e.g., "Clusters MAY export traces to Jaeger")

**Examples**:
```
✓ CORRECT: "Scheduler agents MAY execute jobs concurrently (default: 10 concurrent)."
✓ CORRECT: "Implementations MAY provide a web-based cluster administration UI."
```

---

## 3. Normative vs. Informative Sections

### 3.1 Normative Sections

Sections containing MUST/SHALL/REQUIRED/SHOULD/MAY are **normative** and define implementation requirements.

**Normative sections include**:
- Data structures (field requirements, constraints)
- Algorithms (required steps, ordering)
- Protocols (message formats, state machines)
- Security requirements (authentication, authorization, encryption)
- Error handling (required error codes, recovery procedures)

**Markers**:
- Sections with RFC 2119 keywords are implicitly normative
- May be explicitly marked: `**Normative Section**`

### 3.2 Informative Sections

Sections without RFC 2119 keywords are **informative** and provide context, examples, or guidance.

**Informative sections include**:
- Examples
- Performance characteristics (unless stated as requirements)
- Operational procedures (unless stated as requirements)
- Diagrams and illustrations
- Historical context

**Markers**:
- Examples sections: `### Examples` or `## 14. Examples`
- Performance sections: `## Performance Characteristics` (informative unless stated otherwise)
- May be explicitly marked: `**Informative Section**`

---

## 4. Normative Language by Specification Area

### 4.1 Security Requirements (MUST / MUST NOT)

**All security constraints are absolute requirements (MUST/MUST NOT):**

```
✓ "Nodes MUST authenticate using mTLS with X.509 certificates."
✓ "Certificate chains MUST be validated against the cluster CA."
✓ "Private keys MUST NOT be transmitted over the network."
✓ "Audit events MUST be signed with Ed25519 node keys."
✓ "TLS connections MUST use TLS 1.3 or later."
✓ "Cipher suites MUST provide forward secrecy."
```

**Rationale**: Security violations compromise the entire cluster. No flexibility.

---

### 4.2 Data Integrity Requirements (MUST / MUST NOT)

**All data integrity constraints are absolute requirements:**

```
✓ "Epoch numbers MUST be monotonically increasing."
✓ "Audit events MUST include prev_event_hash for chain verification."
✓ "Transactions MUST NOT span multiple shards."
✓ "Shard assignments MUST be consistent across all cluster members."
✓ "Write-after log (WAL) entries MUST be applied in order."
```

**Rationale**: Data corruption is unacceptable. No flexibility.

---

### 4.3 Operational Recommendations (SHOULD / SHOULD NOT)

**Operational best practices use SHOULD/SHOULD NOT:**

```
✓ "Clusters SHOULD run at least 3 nodes for high availability."
✓ "Replication lag SHOULD be monitored and alerted above 10 seconds."
✓ "Administrators SHOULD NOT manually modify epoch records."
✓ "Backups SHOULD be encrypted at rest."
✓ "Metrics SHOULD be exported every 15 seconds."
```

**Rationale**: Valid reasons to deviate exist (e.g., dev/test clusters may run 1 node).

---

### 4.4 Optional Features (MAY)

**Truly optional features use MAY:**

```
✓ "Implementations MAY provide query result caching."
✓ "Nodes MAY export metrics in OpenTelemetry format."
✓ "Clusters MAY implement dynamic shard rebalancing."
✓ "Administrators MAY configure custom alert thresholds."
```

**Rationale**: Feature presence does not affect conformance.

---

## 5. Normative Clause Patterns

### 5.1 Required Behavior

**Pattern**: `<Subject> MUST <verb> <constraint>`

**Examples**:
```
- "Nodes MUST reject CCE epochs with epoch_number <= current_epoch."
- "Raft leaders MUST propose CCE updates via consensus."
- "Query coordinators MUST route queries based on shard key hash."
```

### 5.2 Prohibited Behavior

**Pattern**: `<Subject> MUST NOT <verb> <constraint>`

**Examples**:
```
- "Nodes MUST NOT apply epochs without verifying signature."
- "Transactions MUST NOT execute across shard boundaries."
- "Backup procedures MUST NOT include cryptographic private keys."
```

### 5.3 Conditional Requirements

**Pattern**: `IF <condition> THEN <subject> MUST <verb>`

**Examples**:
```
- "IF replication_lag > 60 seconds, THEN replicas MUST fire alert."
- "IF node_failure_count >= 3, THEN Raft MUST propose node removal."
- "IF certificate expiry < 30 days, THEN node MUST renew certificate."
```

### 5.4 Recommended Practices

**Pattern**: `<Subject> SHOULD <verb> <guidance>`

**Examples**:
```
- "Administrators SHOULD schedule sweep/GC jobs during low-traffic periods."
- "Clusters SHOULD use replication factor >= 2 for production."
- "Nodes SHOULD monitor peer health every 15 seconds."
```

### 5.5 Optional Extensions

**Pattern**: `<Subject> MAY <verb> <feature>`

**Examples**:
```
- "Implementations MAY provide a REST API for cluster management."
- "Nodes MAY cache parsed query plans for performance."
- "Clusters MAY integrate with external secret management systems."
```

---

## 6. Conformance Levels

### 6.1 Core Conformance (Alpha)

**Definition**: Implementation satisfies all MUST/MUST NOT/SHALL/SHALL NOT clauses in:
- SBCLUSTER-00 (Guiding Principles)
- SBCLUSTER-01 (CCE)
- SBCLUSTER-02 (Membership)
- SBCLUSTER-03 (CA Policy)
- SBCLUSTER-04 (Security Bundle)

**Excludes**: SHOULD/MAY clauses (recommendations/options)

**Status**: **REQUIRED** for Alpha release

---

### 6.2 Full Conformance (Beta)

**Definition**: Core Conformance + all MUST/MUST NOT clauses in:
- SBCLUSTER-05 (Sharding)
- SBCLUSTER-06 (Distributed Query)
- SBCLUSTER-07 (Replication)
- SBCLUSTER-08 (Backup and Restore)
- SBCLUSTER-09 (Scheduler)
- SBCLUSTER-10 (Observability)

**Status**: **REQUIRED** for Beta release

---

### 6.3 Extended Conformance (GA)

**Definition**: Full Conformance + adherence to SHOULD/SHOULD NOT clauses

**Status**: **RECOMMENDED** for GA (production) release

---

## 7. Verification Checklist

### 7.1 Implementation Checklist

For each specification (SBCLUSTER-00 through SBCLUSTER-10):

| Check | Description | Status |
|-------|-------------|--------|
| ☐ | All MUST clauses implemented | Required |
| ☐ | All MUST NOT clauses enforced | Required |
| ☐ | All SHALL clauses implemented | Required |
| ☐ | All SHALL NOT clauses enforced | Required |
| ☐ | All SHOULD clauses implemented (or deviations documented) | Recommended |
| ☐ | All MAY clauses evaluated (implemented or explicitly omitted) | Optional |

### 7.2 Testing Checklist

| Check | Description | Status |
|-------|-------------|--------|
| ☐ | MUST clauses have positive tests (verify correct behavior) | Required |
| ☐ | MUST NOT clauses have negative tests (verify rejection) | Required |
| ☐ | Edge cases for MUST/MUST NOT clauses tested | Required |
| ☐ | SHOULD clauses have tests (where implemented) | Recommended |

---

## 8. Common Misuses and Corrections

### 8.1 Too Weak (Should → Must)

❌ **INCORRECT**: "Nodes should validate certificates."
✅ **CORRECT**: "Nodes MUST validate certificates against the cluster CA."

**Rationale**: Certificate validation is security-critical.

---

### 8.2 Too Strong (Must → Should)

❌ **INCORRECT**: "Metrics must be exported every 15 seconds."
✅ **CORRECT**: "Metrics SHOULD be exported every 15 seconds."

**Rationale**: Scrape interval is configurable, not a hard requirement.

---

### 8.3 Ambiguous (Descriptive → Normative)

❌ **INCORRECT**: "Epochs are immutable."
✅ **CORRECT**: "Epochs MUST NOT be modified after publication."

**Rationale**: Normative language clarifies requirement.

---

### 8.4 Missing Conditionality

❌ **INCORRECT**: "Replicas must fire alerts."
✅ **CORRECT**: "IF replication_lag > 10 seconds, THEN replicas MUST fire alert."

**Rationale**: Condition clarifies when requirement applies.

---

## 9. Interpretation Rules

### 9.1 Conflicting Clauses

**Rule**: If MUST and MAY conflict, MUST takes precedence.

**Example**:
- "Nodes MUST use TLS 1.3." (absolute)
- "Nodes MAY use TLS 1.2 for legacy compatibility." (optional)

**Resolution**: TLS 1.3 is required; TLS 1.2 extension is invalid.

---

### 9.2 Implicit Requirements

**Rule**: If a feature is REQUIRED, all dependencies are implicitly REQUIRED.

**Example**:
- "Nodes MUST implement Raft consensus." (explicit)
- Implies: "Nodes MUST implement Raft leader election." (implicit)

---

### 9.3 Temporal Scope

**Rule**: Requirements apply to the specified release (Alpha/Beta/GA).

**Example**:
- "Alpha implementations MUST support single-shard queries."
- "Beta implementations MUST support multi-shard queries."

**Interpretation**: Multi-shard NOT required for Alpha conformance.

---

## 10. Normative References

This document and all cluster specifications reference:

- **RFC 2119**: "Key words for use in RFCs to Indicate Requirement Levels" (March 1997)
  https://www.ietf.org/rfc/rfc2119.txt

- **RFC 8174**: "Ambiguity of Uppercase vs Lowercase in RFC 2119 Key Words" (May 2017)
  https://www.ietf.org/rfc/rfc8174.txt

---

## 11. Document Maintenance

**Ownership**: Chief Architect

**Review**: All normative changes MUST be reviewed by:
- Chief Architect
- Security Lead
- At least one implementation lead

**Approval**: Normative changes REQUIRE consensus approval.

---

**Document Status**: NORMATIVE

**Version**: 1.0

**Last Updated**: 2026-01-02

**Applies To**: SBCLUSTER-00 through SBCLUSTER-10

---

## Appendix A: Quick Reference Card

### Absolute Requirements (No Flexibility)
- **MUST** / **SHALL** / **REQUIRED**: Do this (mandatory)
- **MUST NOT** / **SHALL NOT**: Never do this (forbidden)

### Strong Recommendations (Deviation Requires Justification)
- **SHOULD** / **RECOMMENDED**: Do this (unless good reason not to)
- **SHOULD NOT** / **NOT RECOMMENDED**: Avoid this (unless good reason)

### Optional (No Conformance Impact)
- **MAY** / **OPTIONAL**: Your choice (truly optional)

### Rule of Thumb
- **Security, data integrity, safety**: MUST / MUST NOT
- **Performance, operations, best practices**: SHOULD / SHOULD NOT
- **Extensions, optimizations, integrations**: MAY

---

## Appendix B: Normative Language Audit Template

Use this template when reviewing specifications:

```markdown
## Normative Language Audit: [Specification Name]

### MUST Clauses
- [ ] Line X: "[Quote clause]" - Status: Implemented / Not Implemented
- [ ] Line Y: "[Quote clause]" - Status: Implemented / Not Implemented

### MUST NOT Clauses
- [ ] Line X: "[Quote clause]" - Status: Enforced / Not Enforced

### SHOULD Clauses (with deviations)
- [ ] Line X: "[Quote clause]" - Status: Implemented / Deviated (Reason: ...)

### MAY Clauses (implementation decisions)
- [ ] Line X: "[Quote clause]" - Decision: Implemented / Omitted
```

---

**End of Normative Language Guide**
