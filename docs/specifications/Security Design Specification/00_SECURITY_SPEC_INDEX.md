# ScratchBird Security Specification Suite

**Revision:** v2 (engine-side RLS/CLS, strict sys.* virtual views, nonce/IV and canonicalization sub-specs)


**Version**: 1.0  
**Status**: Draft for Review  
**Date**: January 2026  

---

## Document Index

This document provides an index and overview of the complete ScratchBird Security Specification Suite.

### Core Documents

| Doc ID | Document | Description | Lines |
|--------|----------|-------------|-------|
| SBSEC-01 | [Security Architecture](01_SECURITY_ARCHITECTURE.md) | Core architecture, trust boundaries, component model | ~650 |
| SBSEC-02 | [Identity and Authentication](02_IDENTITY_AUTHENTICATION.md) | Users, AuthKeys, Sessions, password policies, external auth | ~800 |
| SBSEC-03 | [Authorization Model](03_AUTHORIZATION_MODEL.md) | RBAC, GBAC, privileges, RLS, CLS, namespace security | ~900 |
| SBSEC-04 | [Encryption and Key Management](04_ENCRYPTION_KEY_MANAGEMENT.md) | Key hierarchy, TDE, wire encryption, HSM integration | ~1000 |
- 04.01_KEY_LIFECYCLE_STATE_MACHINES.md
- 04.02_KEY_MATERIAL_HANDLING_REQUIREMENTS.md
- 04.03_NONCE_IV_SPECIFICATION.md
| SBSEC-05 | [IPC Security](05_IPC_SECURITY.md) | Parser-engine communication, channel security | ~600 |
- 05.A_IPC_WIRE_FORMAT_AND_EXAMPLES.md
| SBSEC-06 | [Cluster Security](06_CLUSTER_SECURITY.md) | Node authentication, quorum, fencing, security database | ~700 |
- 06.01_QUORUM_PROPOSAL_CANONICALIZATION_HASHING.md
- 06.02_QUORUM_EVIDENCE_AND_AUDIT_COUPLING.md
| SBSEC-07 | [Network Presence Binding](07_NETWORK_PRESENCE_BINDING.md) | Shamir's Secret Sharing for MEK protection | ~650 |
| SBSEC-08 | [Audit and Compliance](08_AUDIT_COMPLIANCE.md) | Audit events, tamper-evident logging, SIEM, compliance | ~800 |
- 08.01_AUDIT_EVENT_CANONICALIZATION.md
- 08.02_AUDIT_CHAIN_VERIFICATION_CHECKPOINTS.md
| SBSEC-09 | [Security Levels](09_SECURITY_LEVELS.md) | Master feature matrix, level definitions (0-6) | ~700 |

**Total**: ~6,800 lines of specification

---

## Reading Order

### For Implementation Teams

1. **SBSEC-09** (Security Levels) - Understand the target security postures
2. **SBSEC-01** (Security Architecture) - Core architecture and trust model
3. **SBSEC-02** (Identity and Authentication) - User identity system
4. **SBSEC-03** (Authorization Model) - Permission system
5. **SBSEC-04** (Encryption) - Data protection
6. **SBSEC-05** (IPC Security) - Internal communications
7. **SBSEC-06** (Cluster Security) - Multi-node deployments
8. **SBSEC-07** (Network Presence Binding) - Level 6 MEK protection
9. **SBSEC-08** (Audit and Compliance) - Logging and compliance

### For Security Auditors

1. **SBSEC-09** (Security Levels) - Feature matrix and compliance mapping
2. **SBSEC-08** (Audit and Compliance) - Audit capabilities
3. **SBSEC-01** (Security Architecture) - Trust boundaries
4. Review other documents as needed

### For Compliance Officers

1. **SBSEC-09** (Security Levels) - Compliance framework mapping
2. **SBSEC-08** (Audit and Compliance) - PCI-DSS, HIPAA, SOC 2 mapping
3. **SBSEC-02** (Identity and Authentication) - Authentication controls
4. **SBSEC-03** (Authorization Model) - Access control

---

## Architecture Summary

### Deployment Modes

```
┌─────────────────────────────────────────────────────────────────┐
│                    DEPLOYMENT MODES                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  EMBEDDED          Application links engine library directly    │
│  SHARED LOCAL      On-demand IPC server for local apps          │
│  FULL SERVER       Persistent service with network listeners    │
│  CLUSTER           Multi-node with quorum and replication       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Security Levels

```
┌─────────────────────────────────────────────────────────────────┐
│                    SECURITY LEVELS                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  L0  OPEN              No security (dev/test only)              │
│  L1  AUTHENTICATED     Basic auth + RBAC                        │
│  L2  ENCRYPTED         + Encryption at rest                     │
│  L3  POLICY-CONTROLLED + RLS/CLS + password policies            │
│  L4  AUDITED           + Full audit + chain hashing             │
│  L5  NETWORK-HARDENED  + TLS required + HSM                     │
│  L6  CLUSTER-HARDENED  + NPB + quorum + two-person rule         │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Key Hierarchy

```
┌─────────────────────────────────────────────────────────────────┐
│                    KEY HIERARCHY                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│                         CMK                                      │
│                    (Cluster Master Key)                          │
│                          │                                       │
│          ┌───────────────┼───────────────┐                      │
│          ▼               ▼               ▼                      │
│         MEK             SDK             DBK                      │
│      (Memory)       (Security)       (Database)                  │
│          │               │               │                       │
│          │               ▼               ├────────┬─────────┐   │
│          │              CSK              ▼        ▼         ▼   │
│          │          (Credentials)       TSK      LEK       BKK  │
│          │                               │      (Log)    (Backup)│
│          │                               ▼                       │
│          │                              DEK                      │
│          │                           (Data)                      │
│          │                                                       │
│          └──────────► Network Presence Binding (L6)             │
│                       (Shamir's Secret Sharing)                  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Trust Boundaries

```
┌─────────────────────────────────────────────────────────────────┐
│                    TRUST BOUNDARIES                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  UNTRUSTED                                                       │
│  ──────────                                                      │
│  External clients, network traffic                               │
│                                                                  │
│  CONSTRAINED                                                     │
│  ───────────                                                     │
│  Network Listener: Connection management only                   │
│  Parser Layer: Zero privileges, translation only                │
│                                                                  │
│  TRUSTED                                                         │
│  ───────                                                         │
│  Engine Core: Sole security authority                           │
│                                                                  │
│  PROTECTED                                                       │
│  ─────────                                                       │
│  Security Catalog, Keys, Audit Logs                             │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Cross-Reference Matrix

### Document Dependencies

| Document | Depends On | Referenced By |
|----------|------------|---------------|
| SBSEC-01 | — | All |
| SBSEC-02 | 01 | 03, 05, 06, 08 |
| SBSEC-03 | 01, 02 | 08 |
| SBSEC-04 | 01 | 06, 07 |
| SBSEC-05 | 01, 02 | 06 |
| SBSEC-06 | 01, 02, 04, 05 | 07 |
| SBSEC-07 | 01, 04, 06 | — |
| SBSEC-08 | 01, 02, 03 | — |
| SBSEC-09 | All | — |

### Feature to Document Mapping

| Feature | Primary Document | Related Documents |
|---------|------------------|-------------------|
| Users, AuthKeys, Sessions | SBSEC-02 | 01, 03 |
| Password Policies | SBSEC-02 | 09 |
| External Auth (LDAP, Kerberos) | SBSEC-02 | 06 |
| Roles (RBAC) | SBSEC-03 | 02 |
| Groups (GBAC) | SBSEC-03 | 02 |
| Row-Level Security | SBSEC-03 | 09 |
| Column-Level Security | SBSEC-03 | 09 |
| TDE (Encryption at Rest) | SBSEC-04 | 09 |
| Key Hierarchy | SBSEC-04 | 07 |
| HSM Integration | SBSEC-04 | 09 |
| Parser Security | SBSEC-05 | 01 |
| IPC Encryption | SBSEC-05 | 09 |
| Node Authentication | SBSEC-06 | 04 |
| Quorum | SBSEC-06 | 09 |
| Fencing | SBSEC-06 | 09 |
| Shamir's Secret Sharing | SBSEC-07 | 04, 06 |
| Network Presence Binding | SBSEC-07 | 04, 06, 09 |
| Audit Events | SBSEC-08 | All |
| Chain Hashing | SBSEC-08 | 09 |
| SIEM Integration | SBSEC-08 | 09 |
| Compliance Mapping | SBSEC-08, SBSEC-09 | — |

---

## Glossary of Key Terms

| Term | Definition | Document |
|------|------------|----------|
| AuthKey | Time-limited authentication token bound to a user | SBSEC-02 |
| CMK | Cluster Master Key - root of key hierarchy | SBSEC-04 |
| DBK | Database Key - encrypts database-level keys | SBSEC-04 |
| DEK | Data Encryption Key - encrypts actual data pages | SBSEC-04 |
| GBAC | Group-Based Access Control | SBSEC-03 |
| MEK | Memory Encryption Key - runtime key for memory protection | SBSEC-04 |
| NPB | Network Presence Binding - Shamir-based MEK protection | SBSEC-07 |
| RBAC | Role-Based Access Control | SBSEC-03 |
| RLS | Row-Level Security | SBSEC-03 |
| CLS | Column-Level Security | SBSEC-03 |
| SBLR | ScratchBird Binary Language Representation | SBSEC-01 |
| Security Context | Immutable privilege snapshot for a transaction | SBSEC-01, 03 |
| Security Database | Cluster-wide authoritative identity store | SBSEC-06 |
| TDE | Transparent Data Encryption | SBSEC-04 |
| TSK | Tablespace Key - encrypts DEKs within tablespace | SBSEC-04 |
| Two-Person Rule | Requiring two administrators for critical operations | SBSEC-06, 09 |

---

## Open Issues and Future Work

### Tracked in Documents

Each document has an "Open Issues" section in its appendix. Key items include:

| Issue | Document | Priority |
|-------|----------|----------|
| IPC encryption implementation details | SBSEC-05 | High |
| Parser registration protocol spec | SBSEC-05 | High |
| Channel token format specification | SBSEC-05 | Medium |
| Administrative termination timeout policy | SBSEC-01 | Medium |
| Post-quantum cryptography roadmap | SBSEC-04 | Low |

### Future Documents (Not Yet Written)

| Doc ID | Proposed Document | Description |
|--------|-------------------|-------------|
| SBSEC-10 | Secure Development Guide | Guidelines for developers |
| SBSEC-11 | Penetration Testing Guide | Security testing procedures |
| SBSEC-12 | Incident Response | Security incident handling |
| SBSEC-13 | Disaster Recovery | Security aspects of DR |

---

## Document Conventions

### Requirement Markers

Throughout the documents:
- **●** = Required at this security level
- **○** = Optional at this security level
- **(blank)** = Not applicable at this security level
- **✓** = Allowed/supported

### Diagram Conventions

- Box with solid lines: Component/process
- Box with dashed lines: Optional component
- Arrow (→): Data/control flow
- Double arrow (↔): Bidirectional communication

### Code Conventions

- SQL examples use PostgreSQL-like syntax
- Pseudocode uses Python-like syntax
- C-like struct definitions for data formats

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | January 2026 | Initial complete specification suite |

---

## Contact and Feedback

For questions or feedback on these specifications, please file issues in the ScratchBird project repository.

---

*End of Index Document*
