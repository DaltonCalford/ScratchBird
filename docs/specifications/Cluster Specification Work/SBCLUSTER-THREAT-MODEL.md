# ScratchBird Cluster Threat Model and Mitigation Matrix

**Security Architecture Analysis**

---

## 1. Purpose and Scope

This document defines the threat model for ScratchBird distributed clusters and maps explicit attacker models to security mitigations specified in SBCLUSTER-00 through SBCLUSTER-10.

**Scope**:
- Attacker models (capabilities, goals, constraints)
- Threat scenarios by attack surface
- Mitigation mapping to specifications
- Residual risks and assumptions

**Out of Scope**:
- Application-level vulnerabilities (SQL injection, XSS, etc.)
- Physical security (datacenter access, hardware tampering)
- Social engineering attacks on administrators

---

## 2. Attacker Models

### 2.1 Attacker Taxonomy

We define four attacker models based on capabilities and access:

| Attacker | Capabilities | Access Level | Goals |
|----------|-------------|--------------|-------|
| **A1: External Network Attacker** | Network traffic interception, MITM, replay | External (Internet-facing) | Eavesdrop, modify traffic, impersonate |
| **A2: Compromised Application** | API access, query execution, data read/write | Application tier | Data exfiltration, privilege escalation |
| **A3: Compromised Node** | Full node access, memory, disk, keys | Single cluster node | Lateral movement, cluster compromise |
| **A4: Malicious Insider** | Legitimate credentials, authorized access | Administrative | Sabotage, data theft, audit evasion |

---

### 2.2 Attacker Model Details

#### A1: External Network Attacker

**Capabilities**:
- Intercept network traffic between client and cluster
- Intercept network traffic between cluster nodes
- Man-in-the-middle (MITM) attacks
- Replay captured network packets
- Denial of Service (DoS) attacks

**Constraints**:
- No access to cluster nodes (no SSH, no physical access)
- No valid TLS certificates or private keys
- No legitimate user credentials

**Goals**:
- **Confidentiality breach**: Eavesdrop on queries and data
- **Integrity violation**: Modify queries or responses
- **Impersonation**: Pose as legitimate client or node
- **Availability attack**: Disrupt cluster operations

**Threat Level**: **HIGH** (external attackers are ubiquitous)

---

#### A2: Compromised Application

**Capabilities**:
- Execute SQL queries via legitimate API
- Read/write data within application's authorization scope
- Potentially exploit SQL injection vulnerabilities
- Potentially escalate privileges via application logic flaws

**Constraints**:
- Limited to application's credentials and permissions
- No direct access to cluster nodes or infrastructure
- No access to cryptographic keys or certificates

**Goals**:
- **Data exfiltration**: Steal data beyond authorized scope
- **Privilege escalation**: Gain higher privileges (DBA, admin)
- **Lateral movement**: Pivot to other databases or services
- **Persistence**: Maintain access after discovery

**Threat Level**: **MEDIUM** (common attack vector)

---

#### A3: Compromised Node

**Capabilities**:
- Full access to one cluster node (memory, disk, processes)
- Access to node's private keys and certificates
- Ability to read/modify local shard data
- Ability to inject network traffic as the node
- Potentially extract secrets from memory

**Constraints**:
- Limited to a single node (initially)
- Cluster CA and other nodes remain secure
- Raft quorum prevents unilateral control

**Goals**:
- **Lateral movement**: Compromise additional nodes
- **Data theft**: Exfiltrate shard data
- **Cluster takeover**: Gain control of entire cluster
- **Audit evasion**: Delete or tamper with audit logs

**Threat Level**: **CRITICAL** (full node compromise)

---

#### A4: Malicious Insider

**Capabilities**:
- Legitimate administrative credentials
- Authorized access to cluster management APIs
- Knowledge of cluster architecture and configuration
- Potentially access to backup encryption keys

**Constraints**:
- Actions are audited (unless they can evade audit)
- Raft quorum prevents unilateral dangerous actions
- Peer observation may detect anomalies

**Goals**:
- **Sabotage**: Corrupt data, delete shards, disrupt operations
- **Data theft**: Exfiltrate sensitive data for sale or espionage
- **Audit evasion**: Cover tracks by tampering with audit logs
- **Backdoor installation**: Create persistent unauthorized access

**Threat Level**: **CRITICAL** (trusted insider with legitimate access)

---

## 3. Attack Surfaces

### 3.1 Network Attack Surface

**Components**:
- Client → Cluster connections (SQL queries, admin API)
- Node ↔ Node connections (Raft, replication, query routing)
- Cluster → External services (backup targets, metrics exporters)

**Exposure**: **External** (Internet-facing) and **Internal** (intra-cluster)

**Threats**:
- MITM attacks (A1)
- Traffic interception (A1)
- Replay attacks (A1)
- DoS attacks (A1)

---

### 3.2 Authentication & Authorization Surface

**Components**:
- User authentication (credentials, certificates)
- Node authentication (mTLS, certificates)
- Role-Based Access Control (RBAC)
- Row-Level Security (RLS)
- Column-Level Security (CLS)

**Exposure**: **External** (client auth) and **Internal** (node auth)

**Threats**:
- Credential theft (A2)
- Privilege escalation (A2, A4)
- Authorization bypass (A2)
- Impersonation (A1, A3)

---

### 3.3 Data Storage Surface

**Components**:
- Shard data files (heap pages, TOAST)
- Write-after log (WAL)
- Backup files (S3, etc.)
- Audit log storage

**Exposure**: **Internal** (node disk) and **External** (backup storage)

**Threats**:
- Data theft from disk (A3)
- Backup exfiltration (A3, A4)
- Data corruption (A3, A4)
- Audit log tampering (A3, A4)

---

### 3.4 Cryptographic Key Surface

**Components**:
- Node private keys (Ed25519)
- CA private key (cluster root of trust)
- Backup encryption keys
- TLS session keys

**Exposure**: **Internal** (memory, HSM)

**Threats**:
- Private key theft (A3, A4)
- CA compromise (A3, A4)
- Backup key theft (A3, A4)
- Memory extraction (A3)

---

### 3.5 Cluster Control Plane Surface

**Components**:
- Raft consensus (leader election, log replication)
- CCE distribution
- Membership management
- Scheduler control plane

**Exposure**: **Internal** (inter-node)

**Threats**:
- Split-brain attacks (A1, A3)
- Malicious CCE injection (A3, A4)
- Node impersonation (A3)
- Raft log corruption (A3, A4)

---

## 4. Threat Scenarios and Mitigations

### 4.1 Network Threats (A1: External Network Attacker)

#### Threat: MITM Attack on Client ↔ Cluster Connection

**Scenario**: Attacker intercepts client connection, reads queries and data in transit.

**Mitigations**:
| Mitigation | Specification | Normative Clause |
|------------|---------------|------------------|
| TLS 1.3 encryption for all client connections | SBCLUSTER-04 (Security Bundle) | Clients MUST connect via TLS 1.3 |
| Certificate validation against cluster CA | SBCLUSTER-03 (CA Policy) | Clients MUST validate server certificates |
| Forward secrecy cipher suites | SBCLUSTER-04 (TLSPolicy) | Cipher suites MUST provide forward secrecy |

**Residual Risk**: **LOW** (requires CA compromise or TLS protocol break)

---

#### Threat: MITM Attack on Node ↔ Node Connection

**Scenario**: Attacker intercepts inter-node traffic (Raft, replication, query routing).

**Mitigations**:
| Mitigation | Specification | Normative Clause |
|------------|---------------|------------------|
| Mutual TLS (mTLS) for all node connections | SBCLUSTER-02 (Membership) | Nodes MUST authenticate via mTLS |
| Node certificate validation | SBCLUSTER-03 (CA Policy) | Nodes MUST validate peer certificates |
| Node UUID embedded in certificate SAN | SBCLUSTER-02 (Identity) | Certificates MUST include node UUID in SAN |

**Residual Risk**: **VERY LOW** (requires node private key theft)

---

#### Threat: Replay Attack

**Scenario**: Attacker captures and replays Raft consensus messages or queries.

**Mitigations**:
| Mitigation | Specification | Normative Clause |
|------------|---------------|------------------|
| Raft log sequence numbers | SBCLUSTER-01 (CCE) | Raft entries MUST have monotonic indices |
| TLS session keys (ephemeral) | SBCLUSTER-04 (TLS) | TLS MUST use ephemeral DH key exchange |
| Timestamp validation | SBCLUSTER-10 (Audit) | Audit events MUST include timestamp |

**Residual Risk**: **LOW** (TLS prevents replay within session)

---

### 4.2 Authentication & Authorization Threats (A2: Compromised Application)

#### Threat: SQL Injection

**Scenario**: Attacker injects malicious SQL via application to escalate privileges or exfiltrate data.

**Mitigations**:
| Mitigation | Specification | Normative Clause |
|------------|---------------|------------------|
| Prepared statements (application-level) | N/A (app responsibility) | Applications SHOULD use prepared statements |
| RBAC enforcement | SBCLUSTER-04 (Security Bundle) | Engine MUST enforce RBAC on all queries |
| RLS enforcement | SBCLUSTER-04 (RLSPolicy) | Engine MUST apply RLS predicates |
| CLS enforcement | SBCLUSTER-04 (CLSPolicy) | Engine MUST filter columns per CLS policy |

**Residual Risk**: **MEDIUM** (depends on application security practices)

**NOTE**: SQL injection is primarily an **application-level** vulnerability. Cluster mitigations provide **defense in depth**.

---

#### Threat: Privilege Escalation

**Scenario**: Attacker with low-privilege credentials attempts to gain admin or DBA access.

**Mitigations**:
| Mitigation | Specification | Normative Clause |
|------------|---------------|------------------|
| RBAC enforcement | SBCLUSTER-04 (AuthorizationPolicy) | All privilege grants MUST be explicit |
| Audit logging of privilege changes | SBCLUSTER-10 (Audit) | ROLE_GRANTED events MUST be audited |
| Least privilege principle | SBCLUSTER-04 (Security Bundle) | Roles SHOULD grant minimal privileges |

**Residual Risk**: **LOW** (with proper RBAC configuration)

---

### 4.3 Node Compromise Threats (A3: Compromised Node)

#### Threat: Lateral Movement to Other Nodes

**Scenario**: Attacker compromises one node, attempts to compromise additional nodes.

**Mitigations**:
| Mitigation | Specification | Normative Clause |
|------------|---------------|------------------|
| Nodes MUST NOT share private keys | SBCLUSTER-02 (Identity) | Each node MUST have unique Ed25519 keypair |
| Certificate Revocation List (CRL) | SBCLUSTER-03 (CA Policy) | Compromised node certs MUST be added to CRL |
| Raft quorum prevents unilateral action | SBCLUSTER-01 (CCE) | CCE updates REQUIRE Raft quorum |
| Peer health monitoring | SBCLUSTER-10 (Peer Observation) | Nodes MUST monitor peer health |

**Residual Risk**: **MEDIUM** (if CA private key is not protected)

**Mitigation Enhancement**: CA private key SHOULD be stored in HSM (Hardware Security Module).

---

#### Threat: Data Theft from Compromised Node

**Scenario**: Attacker exfiltrates shard data from compromised node's disk.

**Mitigations**:
| Mitigation | Specification | Normative Clause |
|------------|---------------|------------------|
| Data at rest encryption | SBCLUSTER-04 (EncryptionPolicy) | Shard data MUST be encrypted with AES-256-GCM |
| Encryption keys NOT stored on disk | SBCLUSTER-04 (Key Management) | Keys SHOULD be retrieved from KMS/HSM |
| Audit log of data access | SBCLUSTER-10 (Audit) | QUERY_EXECUTED events MUST be audited |

**Residual Risk**: **MEDIUM** (if encryption keys are accessible on node)

**Mitigation Enhancement**: Use **external KMS** (Key Management Service) for encryption keys.

---

#### Threat: Audit Log Tampering

**Scenario**: Attacker modifies or deletes audit logs on compromised node to cover tracks.

**Mitigations**:
| Mitigation | Specification | Normative Clause |
|------------|---------------|------------------|
| Cryptographic hash chain | SBCLUSTER-10 (Audit Chain) | Events MUST hash previous event (Merkle chain) |
| Digital signatures | SBCLUSTER-10 (Audit) | Events MUST be signed with Ed25519 node key |
| Raft replication of audit events | SBCLUSTER-10 (Audit) | Events MUST be replicated via Raft quorum |
| Merkle checkpoints | SBCLUSTER-10 (Checkpoints) | Checkpoints MUST be created hourly |

**Residual Risk**: **LOW** (tampering detected via chain verification)

**Detection**: `verify_audit_chain()` will detect broken chain if any event is modified.

---

### 4.4 Malicious Insider Threats (A4: Malicious Insider)

#### Threat: Sabotage (Data Deletion, Corruption)

**Scenario**: Malicious admin deletes critical shards or corrupts cluster configuration.

**Mitigations**:
| Mitigation | Specification | Normative Clause |
|------------|---------------|------------------|
| Raft quorum for destructive operations | SBCLUSTER-01 (CCE) | Dangerous ops REQUIRE quorum approval |
| Audit logging of admin actions | SBCLUSTER-10 (Audit) | All DDL/admin ops MUST be audited |
| Immutable backups | SBCLUSTER-08 (Backup) | Backups MUST be append-only (WORM) |
| Multi-party approval for critical ops | SBCLUSTER-09 (Scheduler) | QUORUM_REQUIRED job class exists |

**Residual Risk**: **MEDIUM** (legitimate admin can still cause damage, but actions are audited)

**Mitigation Enhancement**: Implement **two-person rule** for destructive operations in GA.

---

#### Threat: Audit Evasion

**Scenario**: Malicious admin attempts to delete or tamper with audit logs after malicious action.

**Mitigations**:
| Mitigation | Specification | Normative Clause |
|------------|---------------|------------------|
| Immutable audit chain | SBCLUSTER-10 (Audit) | Events MUST NOT be deleted or modified |
| Cryptographic verification | SBCLUSTER-10 (Audit Chain) | Chain MUST be verifiable via hash/signature |
| Off-cluster audit archival | SBCLUSTER-10 (Audit) | Audit events MAY be exported to external SIEM |
| Raft replication | SBCLUSTER-10 (Audit) | Events replicated to quorum before ack |

**Residual Risk**: **LOW** (tampering detected, but insider may still delete if quorum compromised)

**Mitigation Enhancement**: **Stream audit logs to external SIEM** for GA (write-only, no delete).

---

## 5. Trust Boundaries

### 5.1 Cluster Trust Boundary

**Inside Boundary** (Trusted):
- Cluster nodes with valid certificates
- Raft consensus group
- Cluster CA

**Outside Boundary** (Untrusted):
- Client applications
- External backup storage
- External observability systems (Prometheus, Grafana)
- The Internet

**Enforcement**:
- mTLS for all cross-boundary communication (SBCLUSTER-02)
- Certificate validation against cluster CA (SBCLUSTER-03)
- Encryption at rest for data crossing boundary (SBCLUSTER-08)

---

### 5.2 Node Trust Boundary

**Inside Boundary** (Trusted):
- Node's own processes (engine, scheduler, metrics)
- Node's memory and disk (encrypted)

**Outside Boundary** (Untrusted):
- Other cluster nodes (authenticated but verified)
- Network (even internal network is untrusted)

**Enforcement**:
- mTLS for all inter-node communication
- Verification of all incoming messages (signatures, hashes)

---

### 5.3 Application Trust Boundary

**Inside Boundary** (Trusted):
- Authenticated users with valid credentials
- Queries within user's authorization scope

**Outside Boundary** (Untrusted):
- Unauthenticated clients
- Queries outside authorization scope

**Enforcement**:
- Authentication (credentials, certificates)
- Authorization (RBAC, RLS, CLS) enforced on every query

---

## 6. Threat Matrix Summary

| Threat Scenario | Attacker | Attack Surface | Severity | Mitigations | Residual Risk |
|----------------|----------|----------------|----------|-------------|---------------|
| MITM on client connection | A1 | Network | HIGH | TLS 1.3, cert validation | LOW |
| MITM on node connection | A1 | Network | HIGH | mTLS, cert validation | VERY LOW |
| Replay attack | A1 | Network | MEDIUM | Raft sequence, TLS ephemeral keys | LOW |
| SQL injection | A2 | Auth/Authz | MEDIUM | RBAC, RLS, CLS | MEDIUM |
| Privilege escalation | A2 | Auth/Authz | HIGH | RBAC, audit | LOW |
| Lateral movement | A3 | Node compromise | CRITICAL | Unique keys, CRL, quorum | MEDIUM |
| Data theft from node | A3 | Data storage | CRITICAL | Encryption at rest, KMS | MEDIUM |
| Audit tampering | A3 | Audit log | HIGH | Hash chain, signatures, Raft | LOW |
| Sabotage (insider) | A4 | Control plane | CRITICAL | Quorum, audit, backups | MEDIUM |
| Audit evasion (insider) | A4 | Audit log | HIGH | Immutable chain, external SIEM | LOW |

**Overall Risk Profile**: **MODERATE** (strong mitigations, some residual risks requiring operational controls)

---

## 7. Residual Risks and Assumptions

### 7.1 Residual Risks

#### High-Severity Residual Risks

1. **CA Private Key Compromise** (A3, A4)
   - **Risk**: If CA private key is stolen, attacker can issue arbitrary certificates
   - **Mitigation**: CA key SHOULD be stored in HSM (SBCLUSTER-03)
   - **Detection**: Certificate issuance is audited (SBCLUSTER-10)
   - **Recovery**: Revoke all certificates, generate new CA, re-issue node certs

2. **Quorum Compromise** (A3, A4)
   - **Risk**: If majority of nodes are compromised, attacker controls cluster
   - **Mitigation**: Requires (N+1)/2 nodes to be compromised
   - **Detection**: Peer observation, audit log analysis
   - **Recovery**: Restore from backup, rebuild cluster with new trust

3. **Insider with Quorum Access** (A4)
   - **Risk**: Malicious admin with access to majority of nodes can bypass protections
   - **Mitigation**: Two-person rule, separation of duties (operational)
   - **Detection**: Audit logs (if not tampered)
   - **Recovery**: Forensic analysis, restore from backup

#### Medium-Severity Residual Risks

4. **Application-Level SQL Injection** (A2)
   - **Risk**: Application fails to sanitize input, allows SQL injection
   - **Mitigation**: RBAC/RLS/CLS provide defense in depth
   - **Detection**: Audit log analysis (unusual queries)
   - **Recovery**: Fix application, revoke compromised credentials

5. **Backup Encryption Key Theft** (A3, A4)
   - **Risk**: Attacker steals backup encryption keys, exfiltrates backups
   - **Mitigation**: Keys stored in KMS (SBCLUSTER-08)
   - **Detection**: Backup access is audited
   - **Recovery**: Rotate keys, re-encrypt backups

---

### 7.2 Security Assumptions

**The threat model assumes the following:**

1. **Physical Security**: Datacenters and hardware are physically secure
2. **Operating System Security**: Nodes run hardened OS with security patches
3. **Network Security**: Internal network is isolated and firewalled
4. **Operational Security**: Administrators follow security best practices
5. **HSM/KMS Availability**: External key management systems are secure and available
6. **Raft Correctness**: Raft implementation is correct and free of Byzantine faults

**If any assumption is violated**, the threat model MAY NOT hold, and additional mitigations MUST be applied.

---

## 8. Compliance Mapping

### 8.1 SOC 2 Type II

| Control | Specification | Implementation |
|---------|---------------|----------------|
| **CC6.1**: Logical access controls | SBCLUSTER-02, SBCLUSTER-04 | mTLS, RBAC, RLS, CLS |
| **CC6.6**: Encryption at rest | SBCLUSTER-04, SBCLUSTER-08 | AES-256-GCM |
| **CC6.7**: Encryption in transit | SBCLUSTER-04 | TLS 1.3 |
| **CC7.2**: Audit logging | SBCLUSTER-10 | Immutable audit chain |
| **CC7.3**: Monitoring and alerting | SBCLUSTER-10 | Metrics, tracing, alerts |

---

### 8.2 ISO 27001

| Control | Specification | Implementation |
|---------|---------------|----------------|
| **A.9.2.1**: User registration | SBCLUSTER-02 | Node identity, certificates |
| **A.9.4.1**: Access restriction | SBCLUSTER-04 | RBAC, RLS, CLS |
| **A.10.1.1**: Encryption policy | SBCLUSTER-04 | EncryptionPolicy, TLSPolicy |
| **A.12.4.1**: Event logging | SBCLUSTER-10 | Audit chain |
| **A.12.4.3**: Log protection | SBCLUSTER-10 | Hash chain, signatures |

---

### 8.3 GDPR

| Requirement | Specification | Implementation |
|-------------|---------------|----------------|
| **Art. 32**: Security of processing | SBCLUSTER-04 | Encryption, access control |
| **Art. 32(2)**: Pseudonymization | SBCLUSTER-04 | Column-level masking (CLS) |
| **Art. 30**: Audit trail | SBCLUSTER-10 | Immutable audit log |
| **Art. 33**: Breach notification | SBCLUSTER-10 | Audit alerts |

---

## 9. Security Testing Requirements

### 9.1 Penetration Testing

**Required tests (MUST)**:
- [ ] TLS configuration (cipher suites, protocol versions)
- [ ] Certificate validation (expired, revoked, wrong CA)
- [ ] mTLS authentication bypass attempts
- [ ] RBAC/RLS/CLS enforcement
- [ ] Audit log tampering attempts
- [ ] Raft consensus manipulation (Byzantine attacks)

**Recommended tests (SHOULD)**:
- [ ] SQL injection (defense in depth verification)
- [ ] Privilege escalation paths
- [ ] Backup encryption verification
- [ ] DoS resilience

---

### 9.2 Chaos Engineering

**Required chaos scenarios (MUST)**:
- [ ] Random node failures (verify Raft leader election)
- [ ] Network partitions (verify split-brain prevention)
- [ ] Certificate expiry (verify renewal)
- [ ] Audit chain verification under node failures

**Recommended chaos scenarios (SHOULD)**:
- [ ] Clock skew attacks
- [ ] Disk corruption (verify backup restore)
- [ ] High load (verify rate limiting, if implemented)

---

## 10. References

### 10.1 Internal References

- **SBCLUSTER-00**: Guiding Principles
- **SBCLUSTER-01**: Cluster Configuration Epoch
- **SBCLUSTER-02**: Membership and Identity
- **SBCLUSTER-03**: CA Policy
- **SBCLUSTER-04**: Security Bundle
- **SBCLUSTER-08**: Backup and Restore
- **SBCLUSTER-10**: Observability and Audit

### 10.2 External Standards

- **NIST SP 800-53**: Security and Privacy Controls
- **OWASP Top 10**: Web Application Security Risks
- **CIS Critical Security Controls**: Center for Internet Security
- **MITRE ATT&CK**: Adversarial Tactics, Techniques, and Common Knowledge

### 10.3 Threat Modeling Resources

- **STRIDE Model**: Microsoft threat modeling methodology
- **PASTA**: Process for Attack Simulation and Threat Analysis
- **OCTAVE**: Operationally Critical Threat, Asset, and Vulnerability Evaluation

---

## 11. Document Maintenance

**Ownership**: Security Lead

**Review Cycle**: Annually or after significant architecture changes

**Approval Required**: Security Lead, Chief Architect

**Last Threat Assessment**: 2026-01-02

---

**Document Status**: NORMATIVE (Security Analysis)

**Version**: 1.0

**Last Updated**: 2026-01-02

---

**End of Threat Model and Mitigation Matrix**
