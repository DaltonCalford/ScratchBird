# ScratchBird Security Levels Specification

**Document ID**: SBSEC-09  
**Version**: 1.0  
**Status**: Draft for Review  
**Date**: January 2026  
**Scope**: All deployment modes  

---

## 1. Introduction

### 1.1 Purpose

This document defines the seven security levels (0-6) supported by ScratchBird. Each level represents a distinct security posture with specific requirements, features, and trade-offs. This document serves as the master reference for what security features are required or optional at each level.

### 1.2 Design Philosophy

ScratchBird's security level system is designed around these principles:

1. **Incremental**: Each level builds upon the previous, adding security requirements
2. **Configurable**: Organizations choose the level appropriate for their needs
3. **Auditable**: Security level is logged and can be verified
4. **Enforceable**: The system prevents operation below the configured level

### 1.3 Related Documents

All SBSEC documents (01-08) reference this specification for security level requirements.

---

## 2. Security Level Overview

### 2.1 Level Summary

```
┌─────────────────────────────────────────────────────────────────┐
│                   SECURITY LEVELS OVERVIEW                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Level 0: OPEN                                                   │
│  ──────────────                                                  │
│  No security enforcement. Development/testing only.              │
│  • No authentication required                                   │
│  • No authorization checks                                      │
│  • No encryption                                                │
│  • No audit logging                                             │
│                                                                  │
│  Level 1: AUTHENTICATED                                          │
│  ─────────────────────────                                       │
│  Basic identity verification.                                    │
│  • Authentication required                                      │
│  • RBAC/GBAC enabled                                            │
│  • Object ownership enforced                                    │
│  • Basic audit optional                                         │
│                                                                  │
│  Level 2: ENCRYPTED                                              │
│  ──────────────────                                              │
│  Data protection at rest.                                        │
│  • Everything in Level 1 plus:                                  │
│  • Encryption at rest required                                  │
│  • TDE for all databases                                        │
│  • Write-after log (WAL) encryption                             │
│  • Backup encryption                                            │
│                                                                  │
│  Level 3: POLICY-CONTROLLED                                      │
│  ──────────────────────────                                      │
│  Fine-grained access control.                                    │
│  • Everything in Level 2 plus:                                  │
│  • Row-Level Security (RLS)                                     │
│  • Column-Level Security (CLS)                                  │
│  • Password policies enforced                                   │
│  • External auth providers supported                            │
│                                                                  │
│  Level 4: AUDITED                                                │
│  ────────────────                                                │
│  Comprehensive audit trail.                                      │
│  • Everything in Level 3 plus:                                  │
│  • Full audit logging required                                  │
│  • Chain hashing for tamper evidence                            │
│  • Authentication events logged                                 │
│  • Authorization events logged                                  │
│                                                                  │
│  Level 5: NETWORK-HARDENED                                       │
│  ─────────────────────────                                       │
│  Secure network communications.                                  │
│  • Everything in Level 4 plus:                                  │
│  • TLS required for all connections                             │
│  • mTLS for administrative access                               │
│  • IPC encryption required                                      │
│  • External audit storage required                              │
│  • HSM integration required for production                      │
│                                                                  │
│  Level 6: CLUSTER-HARDENED (Military Grade)                      │
│  ──────────────────────────────────────────                      │
│  Maximum security for sensitive environments.                    │
│  • Everything in Level 5 plus:                                  │
│  • Network Presence Binding (MEK protection)                    │
│  • Two-person rule for critical operations                      │
│  • Quorum required for security changes                         │
│  • Centralized security database                                │
│  • Real-time security alerts                                    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Level Selection Guidelines

| Use Case | Recommended Level |
|----------|-------------------|
| Development/testing | 0 or 1 |
| Internal applications (non-sensitive) | 2 |
| Business applications | 3 |
| Applications with compliance requirements | 4 |
| Production with external access | 5 |
| Financial, healthcare, government | 5 or 6 |
| Military, intelligence, critical infrastructure | 6 |

### 2.3 Deployment Mode Compatibility

| Level | Embedded | Shared Local | Full Server | Cluster |
|-------|:--------:|:------------:|:-----------:|:-------:|
| 0 | ✓ | ✓ | ✓ | ✓ |
| 1 | ✓ | ✓ | ✓ | ✓ |
| 2 | ✓ | ✓ | ✓ | ✓ |
| 3 | ✓ | ✓ | ✓ | ✓ |
| 4 | Limited¹ | ✓ | ✓ | ✓ |
| 5 | — | — | ✓ | ✓ |
| 6 | — | — | — | ✓ |

¹ External audit storage may not be available in embedded mode

---

## 3. Master Feature Matrix

### 3.1 Authentication Features

| Feature | L0 | L1 | L2 | L3 | L4 | L5 | L6 |
|---------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Anonymous access | ✓ | | | | | | |
| Internal authentication | | ● | ● | ● | ● | ● | ● |
| Password authentication | | ● | ● | ● | ● | ● | ● |
| Password policies | | ○ | ○ | ● | ● | ● | ● |
| Password complexity | | ○ | ○ | ● | ● | ● | ● |
| Password history | | | | ● | ● | ● | ● |
| Password expiry | | | | ● | ● | ● | ● |
| Account lockout | | ○ | ○ | ● | ● | ● | ● |
| External auth (LDAP/AD) | | ○ | ○ | ○ | ● | ● | ● |
| Kerberos/GSSAPI | | | | ○ | ○ | ● | ● |
| OIDC/OAuth2 | | | | ○ | ○ | ● | ● |
| Certificate authentication | | | | | ○ | ● | ● |
| Multi-factor authentication | | | | ○ | ○ | ● | ● |
| Session timeout | | ○ | ○ | ● | ● | ● | ● |
| Concurrent session limits | | | | ○ | ○ | ● | ● |

● = Required | ○ = Optional | ✓ = Allowed | (blank) = Not applicable/available

### 3.2 Authorization Features

| Feature | L0 | L1 | L2 | L3 | L4 | L5 | L6 |
|---------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Object ownership | | ● | ● | ● | ● | ● | ● |
| GRANT/REVOKE | | ● | ● | ● | ● | ● | ● |
| Roles (RBAC) | | ● | ● | ● | ● | ● | ● |
| Groups (GBAC) | | ○ | ○ | ● | ● | ● | ● |
| WITH GRANT OPTION | | ● | ● | ● | ● | ● | ● |
| Default privileges | | ○ | ○ | ● | ● | ● | ● |
| Row-Level Security | | | | ● | ● | ● | ● |
| Column-Level Security | | | | ● | ● | ● | ● |
| Column masking | | | | ○ | ○ | ● | ● |
| Schema isolation | | ● | ● | ● | ● | ● | ● |
| Catalog protection | | ● | ● | ● | ● | ● | ● |
| Namespace sandboxing | | ● | ● | ● | ● | ● | ● |
| Privilege caching | | ○ | ○ | ● | ● | ● | ● |

### 3.3 Encryption Features

| Feature | L0 | L1 | L2 | L3 | L4 | L5 | L6 |
|---------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Encryption at rest | | | ● | ● | ● | ● | ● |
| TDE (Transparent Data Encryption) | | | ● | ● | ● | ● | ● |
| Write-after log (WAL) encryption | | | ● | ● | ● | ● | ● |
| Backup encryption | | | ● | ● | ● | ● | ● |
| Wire encryption (TLS) | | | ○ | ○ | ○ | ● | ● |
| TLS 1.3 required | | | | | | ● | ● |
| mTLS (client certificates) | | | | | ○ | ● | ● |
| IPC encryption | | | | ○ | ○ | ● | ● |
| Key hierarchy (CMK→DBK→DEK) | | | ● | ● | ● | ● | ● |
| Passphrase-based CMK | | | ● | ● | ● | ● | ● |
| HSM integration | | | ○ | ○ | ○ | ● | ● |
| External KMS | | | ○ | ○ | ○ | ● | ● |
| Split knowledge CMK | | | | | | ○ | ● |
| Key rotation | | | ○ | ● | ● | ● | ● |
| Automated key rotation | | | | | ○ | ● | ● |
| Network Presence Binding | | | | | | | ● |

### 3.4 Audit Features

| Feature | L0 | L1 | L2 | L3 | L4 | L5 | L6 |
|---------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Basic logging | ○ | ○ | ○ | ○ | ● | ● | ● |
| Authentication events | | | | ○ | ● | ● | ● |
| Authorization events | | | | ○ | ● | ● | ● |
| Data access events | | | | | ○ | ● | ● |
| Administrative events | | | | | ● | ● | ● |
| Security events | | | | | ● | ● | ● |
| Chain hashing | | | | | ● | ● | ● |
| Signed checkpoints | | | | | ○ | ● | ● |
| Local audit storage | | | | ○ | ● | ● | ● |
| External audit storage | | | | | ○ | ● | ● |
| Audit log encryption | | | | | ● | ● | ● |
| SIEM integration | | | | | ○ | ● | ● |
| Real-time alerting | | | | | | ● | ● |
| Audit retention policies | | | | | ● | ● | ● |

### 3.5 IPC/Network Features

| Feature | L0 | L1 | L2 | L3 | L4 | L5 | L6 |
|---------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Named pipe IPC | ● | ● | ● | ● | ● | ● | ● |
| OS-level pipe protection | ● | ● | ● | ● | ● | ● | ● |
| Parser registration | | | | ● | ● | ● | ● |
| Channel tokens | | | | ● | ● | ● | ● |
| Message HMAC | | | | | ● | ● | ● |
| IPC encryption | | | | | ○ | ● | ● |
| Sequence numbers | | | | ● | ● | ● | ● |

### 3.6 Cluster Features

| Feature | L0 | L1 | L2 | L3 | L4 | L5 | L6 |
|---------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Multi-node cluster | ○ | ○ | ○ | ○ | ○ | ○ | ● |
| Inter-node TLS | | | ● | ● | ● | ● | ● |
| Inter-node mTLS | | | | ○ | ○ | ● | ● |
| Node certificates | | | ● | ● | ● | ● | ● |
| Security database | | | | | | ● | ● |
| Credential caching | | | | | | ● | ● |
| Quorum for reads | | | | | | | |
| Quorum for security changes | | | | | | ○ | ● |
| Two-person rule | | | | | | | ● |
| Fencing | | | | | | ● | ● |
| Network Presence Binding | | | | | | | ● |

---

## 4. Level Definitions

### 4.1 Level 0: OPEN

**Purpose**: Development, testing, and evaluation environments only.

**Characteristics**:
- No security enforcement
- Anonymous access permitted
- No permission checks
- No encryption
- Minimal logging

**Configuration**:
```sql
ALTER SYSTEM SET security.level = 0;
```

**Restrictions**:
- MUST NOT be used in production
- MUST NOT contain sensitive data
- SHOULD display warning on startup
- SHOULD be disabled in production builds

**Use Cases**:
- Local development
- Unit testing
- Performance benchmarking
- Feature evaluation

---

### 4.2 Level 1: AUTHENTICATED

**Purpose**: Basic security for internal applications without sensitive data.

**Requirements**:

| Category | Requirement |
|----------|-------------|
| Authentication | Required for all connections |
| Authorization | RBAC enabled, object ownership enforced |
| Encryption | None required |
| Audit | Optional |
| Network | None required |

**Configuration**:
```sql
ALTER SYSTEM SET security.level = 1;
ALTER SYSTEM SET auth.mode = 'LOCAL';
ALTER SYSTEM SET auth.allow_anonymous = FALSE;
```

**Minimum Settings**:
```sql
-- These are enforced at Level 1
auth.mode >= 'LOCAL'
auth.allow_anonymous = FALSE
authorization.enabled = TRUE
```

**Use Cases**:
- Internal tools and utilities
- Development databases with test data
- Non-sensitive internal applications

---

### 4.3 Level 2: ENCRYPTED

**Purpose**: Data protection for moderately sensitive information.

**Requirements**:

| Category | Requirement |
|----------|-------------|
| Authentication | Required |
| Authorization | RBAC/GBAC enabled |
| Encryption | TDE required, backup encryption required |
| Audit | Optional |
| Network | TLS recommended |

**Configuration**:
```sql
ALTER SYSTEM SET security.level = 2;
ALTER SYSTEM SET encryption.enabled = TRUE;
ALTER SYSTEM SET encryption.algorithm = 'AES-256-GCM';
```

**Minimum Settings**:
```sql
-- These are enforced at Level 2
encryption.enabled = TRUE
encryption.tde_required = TRUE
encryption.backup_encryption = TRUE
encryption.wal_encryption = TRUE
```

**Key Management**:
- CMK via passphrase minimum
- HSM optional
- Key rotation recommended

**Use Cases**:
- Internal applications with some sensitive data
- Databases requiring encryption for compliance
- Pre-production environments

---

### 4.4 Level 3: POLICY-CONTROLLED

**Purpose**: Fine-grained access control for business applications.

**Requirements**:

| Category | Requirement |
|----------|-------------|
| Authentication | Required with password policies |
| Authorization | RLS/CLS required, groups required |
| Encryption | TDE required |
| Audit | Authentication logging recommended |
| Network | TLS recommended |

**Configuration**:
```sql
ALTER SYSTEM SET security.level = 3;
ALTER SYSTEM SET password.policy_enforced = TRUE;
ALTER SYSTEM SET rls.default_enabled = TRUE;
```

**Minimum Settings**:
```sql
-- These are enforced at Level 3
password.min_length >= 12
password.require_complexity = TRUE
password.max_age_days <= 90
account.lockout_enabled = TRUE
account.lockout_threshold <= 5
rls.enabled = TRUE
cls.enabled = TRUE
```

**Use Cases**:
- Multi-tenant applications
- Applications with row-level data separation
- Business applications with moderate compliance needs

---

### 4.5 Level 4: AUDITED

**Purpose**: Comprehensive audit trail for compliance requirements.

**Requirements**:

| Category | Requirement |
|----------|-------------|
| Authentication | Required with full password policies |
| Authorization | Full RBAC/GBAC/RLS/CLS |
| Encryption | TDE required |
| Audit | Full audit logging, chain hashing |
| Network | TLS recommended |

**Configuration**:
```sql
ALTER SYSTEM SET security.level = 4;
ALTER SYSTEM SET audit.enabled = TRUE;
ALTER SYSTEM SET audit.chain.enabled = TRUE;
ALTER SYSTEM SET audit.min_severity = 'INFO';
```

**Minimum Settings**:
```sql
-- These are enforced at Level 4
audit.enabled = TRUE
audit.authentication_events = TRUE
audit.authorization_events = TRUE
audit.administrative_events = TRUE
audit.security_events = TRUE
audit.chain.enabled = TRUE
audit.retention_days >= 365
```

**Use Cases**:
- Applications requiring audit trails
- PCI-DSS compliance
- HIPAA compliance
- SOX compliance

---

### 4.6 Level 5: NETWORK-HARDENED

**Purpose**: Secure network communications for production environments with external access.

**Requirements**:

| Category | Requirement |
|----------|-------------|
| Authentication | MFA required for admin, external auth supported |
| Authorization | Full security model |
| Encryption | TDE + TLS required + IPC encryption |
| Audit | Full audit + external storage |
| Network | TLS 1.3 required, mTLS for admin |

**Configuration**:
```sql
ALTER SYSTEM SET security.level = 5;
ALTER SYSTEM SET wire.tls_mode = 'REQUIRED';
ALTER SYSTEM SET wire.min_tls_version = 'TLS13';
ALTER SYSTEM SET ipc.encryption_required = TRUE;
ALTER SYSTEM SET audit.external.enabled = TRUE;
```

**Minimum Settings**:
```sql
-- These are enforced at Level 5
wire.tls_mode = 'REQUIRED'
wire.min_tls_version = 'TLS13'
wire.client_cert_mode_admin = 'REQUIRED'
ipc.encryption_required = TRUE
ipc.require_registration = TRUE
audit.external.enabled = TRUE
encryption.hsm.required = TRUE  -- for production
mfa.required_for_admin = TRUE
```

**Use Cases**:
- Production databases with internet exposure
- Financial services applications
- Healthcare applications
- Government applications (non-classified)

---

### 4.7 Level 6: CLUSTER-HARDENED (Military Grade)

**Purpose**: Maximum security for highly sensitive environments.

**Requirements**:

| Category | Requirement |
|----------|-------------|
| Authentication | MFA required, split knowledge for admin |
| Authorization | Full + quorum for changes |
| Encryption | Full + Network Presence Binding |
| Audit | Full + real-time alerting |
| Network | Full hardening |
| Cluster | Required, security database, quorum |

**Configuration**:
```sql
ALTER SYSTEM SET security.level = 6;
ALTER SYSTEM SET npb.enabled = TRUE;
ALTER SYSTEM SET cluster.quorum.security_changes = 'SUPERMAJORITY';
ALTER SYSTEM SET admin.two_person_rule = TRUE;
```

**Minimum Settings**:
```sql
-- These are enforced at Level 6
-- All Level 5 settings plus:
cluster.mode = 'REQUIRED'
cluster.security_database = TRUE
cluster.quorum.security_changes = 'SUPERMAJORITY'
npb.enabled = TRUE
npb.threshold >= (n/2) + 1
admin.two_person_rule = TRUE
admin.superuser_quorum = TRUE
audit.alerting.enabled = TRUE
audit.alerting.realtime = TRUE
encryption.cmk.split_knowledge = TRUE
```

**Use Cases**:
- Military and defense systems
- Intelligence community
- Critical infrastructure
- High-value financial systems
- National security applications

---

## 5. Security Level Transitions

### 5.1 Upgrading Security Level

```
┌─────────────────────────────────────────────────────────────────┐
│              SECURITY LEVEL UPGRADE                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. Review current configuration                                │
│     SHOW SECURITY LEVEL;                                        │
│     SHOW SECURITY REQUIREMENTS FOR LEVEL n;                     │
│                                                                  │
│  2. Enable required features                                     │
│     • Configure missing security features                       │
│     • Enable required encryption                                │
│     • Configure required authentication                         │
│                                                                  │
│  3. Validate readiness                                           │
│     SELECT * FROM sys.security_level_check(target_level);       │
│                                                                  │
│  4. Upgrade level                                                │
│     ALTER SYSTEM SET security.level = n;                        │
│                                                                  │
│  5. Verify                                                       │
│     SHOW SECURITY LEVEL;                                        │
│     SELECT * FROM sys.security_status();                        │
│                                                                  │
│  Note: Upgrade may require restart for some features            │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 Downgrading Security Level

```
┌─────────────────────────────────────────────────────────────────┐
│             SECURITY LEVEL DOWNGRADE                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  WARNING: Downgrading reduces security posture.                 │
│                                                                  │
│  Requirements:                                                   │
│  • CLUSTER_ADMIN privilege required                             │
│  • TWO_PERSON approval required (from Level 6)                  │
│  • Audit event generated                                        │
│  • Confirmation required                                         │
│                                                                  │
│  Process:                                                        │
│  1. Request downgrade                                            │
│     ALTER SYSTEM SET security.level = n WITH CONFIRM;           │
│                                                                  │
│  2. Approve downgrade (second administrator, if Level 6)        │
│     APPROVE PENDING REQUEST 'request-uuid';                     │
│                                                                  │
│  3. System disables features not required at new level          │
│     (existing features remain but are no longer enforced)       │
│                                                                  │
│  Note: Some downgrades may require data migration               │
│  (e.g., encrypted → unencrypted is not automatic)               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 5.3 Level Check Function

```sql
-- Check readiness for a security level
CREATE FUNCTION sys.security_level_check(target_level INTEGER)
RETURNS TABLE (
    requirement VARCHAR(128),
    current_value VARCHAR(128),
    required_value VARCHAR(128),
    status VARCHAR(16)    -- 'OK', 'MISSING', 'INSUFFICIENT'
) AS $$
BEGIN
    -- Check all requirements for target level
    -- Return status for each
END;
$$ LANGUAGE plpgsql;

-- Example output
SELECT * FROM sys.security_level_check(5);
/*
requirement                  | current_value | required_value | status
-----------------------------+---------------+----------------+-------------
wire.tls_mode               | PREFERRED     | REQUIRED       | INSUFFICIENT
wire.min_tls_version        | TLS12         | TLS13          | INSUFFICIENT
ipc.encryption_required     | FALSE         | TRUE           | MISSING
audit.external.enabled      | FALSE         | TRUE           | MISSING
encryption.hsm.required     | FALSE         | TRUE           | MISSING
mfa.required_for_admin      | FALSE         | TRUE           | MISSING
*/
```

---

## 6. Compliance Mapping

### 6.1 Compliance Framework to Security Level

| Framework | Minimum Level | Recommended Level |
|-----------|---------------|-------------------|
| SOC 2 Type I | 3 | 4 |
| SOC 2 Type II | 4 | 5 |
| PCI-DSS | 4 | 5 |
| HIPAA | 4 | 5 |
| GDPR | 3 | 4 |
| FedRAMP Moderate | 5 | 5 |
| FedRAMP High | 6 | 6 |
| NIST 800-53 Moderate | 4 | 5 |
| NIST 800-53 High | 5 | 6 |
| ISO 27001 | 4 | 5 |

### 6.2 Security Level Feature Mapping

```sql
-- View compliance requirements for a standard
SELECT * FROM sys.compliance_requirements('PCI-DSS');

/*
requirement_id | description                  | security_level | feature
---------------+------------------------------+----------------+------------------
10.1           | Audit trails for access      | 4              | audit.enabled
10.2           | Log events                   | 4              | audit.*_events
10.5           | Secure audit logs            | 4              | audit.chain
3.4            | Render PAN unreadable        | 2              | encryption.tde
8.2            | Strong authentication        | 3              | password.policy
*/
```

---

## 7. Configuration Reference

### 7.1 Setting Security Level

```sql
-- Set security level (requires appropriate privileges)
ALTER SYSTEM SET security.level = n;

-- View current level
SHOW security.level;

-- View level details
SELECT * FROM sys.security_level_info();
```

### 7.2 Level-Specific Policies

| Policy Key | Type | L0 | L1 | L2 | L3 | L4 | L5 | L6 |
|------------|------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| `security.level` | int | 0 | 1 | 2 | 3 | 4 | 5 | 6 |
| `auth.required` | bool | F | T | T | T | T | T | T |
| `encryption.tde_required` | bool | F | F | T | T | T | T | T |
| `wire.tls_required` | bool | F | F | F | F | F | T | T |
| `audit.required` | bool | F | F | F | F | T | T | T |
| `cluster.required` | bool | F | F | F | F | F | F | T |

### 7.3 Enforcement Behavior

When a security level is set, the system:
1. Validates all required settings are met
2. Prevents changes that would violate the level
3. Logs all security level changes
4. Displays level in administrative interfaces
5. Includes level in audit records

---

## 8. Security Level Invariants

The following invariants MUST hold for each security level:

### Level 0 Invariants
- None (no security enforcement)

### Level 1 Invariants
- All connections are authenticated
- All objects have owners
- GRANT/REVOKE is functional

### Level 2 Invariants
- All Level 1 invariants plus:
- All data at rest is encrypted
- All backups are encrypted
- Write-after log (WAL) is encrypted

### Level 3 Invariants
- All Level 2 invariants plus:
- Password policies are enforced
- RLS/CLS are available
- External auth is supported

### Level 4 Invariants
- All Level 3 invariants plus:
- All security events are logged
- Audit chain is unbroken
- Audit logs are retained per policy

### Level 5 Invariants
- All Level 4 invariants plus:
- All network traffic is encrypted
- All IPC is encrypted
- Audit logs are externally stored
- HSM is used for key protection

### Level 6 Invariants
- All Level 5 invariants plus:
- MEK is protected by Network Presence
- Critical operations require quorum
- Administrative operations require two-person rule
- Security database is centralized

---

## Appendix A: Quick Reference Card

```
┌─────────────────────────────────────────────────────────────────┐
│           SCRATCHBIRD SECURITY LEVELS QUICK REFERENCE            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  L0  OPEN              Dev/test only, no security               │
│  L1  AUTHENTICATED     Basic auth + RBAC                        │
│  L2  ENCRYPTED         L1 + encryption at rest                  │
│  L3  POLICY-CONTROLLED L2 + RLS/CLS + password policies         │
│  L4  AUDITED           L3 + full audit + chain hashing          │
│  L5  NETWORK-HARDENED  L4 + TLS required + HSM                  │
│  L6  CLUSTER-HARDENED  L5 + NPB + quorum + two-person          │
│                                                                  │
├─────────────────────────────────────────────────────────────────┤
│  Commands:                                                       │
│    SHOW SECURITY LEVEL;                                         │
│    ALTER SYSTEM SET security.level = n;                         │
│    SELECT * FROM sys.security_level_check(n);                   │
│    SELECT * FROM sys.security_status();                         │
├─────────────────────────────────────────────────────────────────┤
│  Compliance:                                                     │
│    PCI-DSS: L4-5    HIPAA: L4-5    SOC2: L4-5                  │
│    FedRAMP Mod: L5  FedRAMP High: L6                            │
└─────────────────────────────────────────────────────────────────┘
```

---

*End of Document*
