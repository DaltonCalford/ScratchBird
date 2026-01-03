# ScratchBird Audit and Compliance Specification

**Document ID**: SBSEC-08  
**Version**: 1.0  
**Status**: Draft for Review  
**Date**: January 2026  
**Scope**: All deployment modes, Security Levels 4-6  

---

## 1. Introduction

### 1.1 Purpose

This document defines the audit logging framework for the ScratchBird database engine. It specifies what events are logged, how audit logs are protected, and how ScratchBird supports compliance with regulatory frameworks.

### 1.2 Scope

This specification covers:
- Audit event taxonomy
- Audit log format and storage
- Tamper-evident logging (chain hashing)
- Audit log management and retention
- SIEM integration
- Compliance framework mapping

### 1.3 Related Documents

| Document ID | Title |
|-------------|-------|
| SBSEC-01 | Security Architecture |
| SBSEC-02 | Identity and Authentication |
| SBSEC-03 | Authorization Model |

### 1.4 Security Level Applicability

| Feature | L0 | L1 | L2 | L3 | L4 | L5 | L6 |
|---------|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| Basic audit logging | | | | | ● | ● | ● |
| Authentication events | | | | ○ | ● | ● | ● |
| Authorization events | | | | ○ | ● | ● | ● |
| Data access events | | | | | ○ | ● | ● |
| Chain hashing | | | | | ● | ● | ● |
| External audit storage | | | | | ○ | ● | ● |
| SIEM integration | | | | | ○ | ● | ● |
| Real-time alerting | | | | | | ● | ● |

● = Required | ○ = Optional | (blank) = Not applicable

---

## 2. Audit Event Taxonomy

### 2.1 Event Categories

```
┌─────────────────────────────────────────────────────────────────┐
│                  AUDIT EVENT CATEGORIES                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  AUTHENTICATION (AUTH)                                          │
│  ─────────────────────                                           │
│  • Login attempts (success/failure)                             │
│  • Password changes                                             │
│  • MFA events                                                   │
│  • Session lifecycle                                            │
│  • Account lockouts                                             │
│                                                                  │
│  AUTHORIZATION (AUTHZ)                                          │
│  ──────────────────────                                          │
│  • Privilege grants/revokes                                     │
│  • Access denied events                                         │
│  • Role changes                                                 │
│  • Privilege escalation                                         │
│                                                                  │
│  DATA ACCESS (DATA)                                             │
│  ──────────────────                                              │
│  • SELECT on sensitive tables                                   │
│  • INSERT/UPDATE/DELETE operations                              │
│  • Bulk data exports                                            │
│  • Schema modifications                                         │
│                                                                  │
│  ADMINISTRATION (ADMIN)                                         │
│  ──────────────────────                                          │
│  • User/role/group management                                   │
│  • Security policy changes                                      │
│  • Configuration changes                                        │
│  • Backup/restore operations                                    │
│                                                                  │
│  SECURITY (SEC)                                                 │
│  ─────────────                                                   │
│  • Key management events                                        │
│  • Certificate operations                                       │
│  • Security incidents                                           │
│  • Fencing events                                               │
│                                                                  │
│  CLUSTER (CLUST)                                                │
│  ──────────────                                                  │
│  • Node membership changes                                      │
│  • Quorum events                                                │
│  • Replication events                                           │
│  • Failover events                                              │
│                                                                  │
│  SYSTEM (SYS)                                                   │
│  ────────────                                                    │
│  • Startup/shutdown                                             │
│  • Resource exhaustion                                          │
│  • Error conditions                                             │
│  • Maintenance operations                                       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Event Severity Levels

| Level | Name | Description | Example |
|-------|------|-------------|---------|
| 0 | EMERGENCY | System unusable | Database corruption detected |
| 1 | ALERT | Immediate action required | Security breach detected |
| 2 | CRITICAL | Critical condition | CMK compromise suspected |
| 3 | ERROR | Error condition | Authentication failure |
| 4 | WARNING | Warning condition | Near resource limits |
| 5 | NOTICE | Normal but significant | User created |
| 6 | INFO | Informational | Query executed |
| 7 | DEBUG | Debug information | Parser timing |

### 2.3 Event Definitions

#### 2.3.1 Authentication Events

| Event Code | Event Name | Severity | Description |
|------------|------------|----------|-------------|
| AUTH-001 | AUTH_ATTEMPT | INFO | Authentication attempted |
| AUTH-002 | AUTH_SUCCESS | INFO | Authentication succeeded |
| AUTH-003 | AUTH_FAILURE | WARNING | Authentication failed |
| AUTH-004 | AUTH_BLOCKED | WARNING | Account is blocked |
| AUTH-005 | AUTH_LOCKED | WARNING | Account locked due to failures |
| AUTH-006 | AUTH_UNLOCKED | NOTICE | Account unlocked |
| AUTH-007 | PASSWORD_CHANGED | NOTICE | Password changed |
| AUTH-008 | PASSWORD_EXPIRED | WARNING | Password expired |
| AUTH-009 | MFA_CHALLENGE | INFO | MFA challenge issued |
| AUTH-010 | MFA_SUCCESS | INFO | MFA verification succeeded |
| AUTH-011 | MFA_FAILURE | WARNING | MFA verification failed |
| AUTH-012 | SESSION_CREATED | INFO | Session established |
| AUTH-013 | SESSION_TERMINATED | INFO | Session ended |
| AUTH-014 | SESSION_TIMEOUT | INFO | Session timed out |
| AUTH-015 | AUTHKEY_CREATED | INFO | AuthKey issued |
| AUTH-016 | AUTHKEY_REVOKED | NOTICE | AuthKey revoked |

#### 2.3.2 Authorization Events

| Event Code | Event Name | Severity | Description |
|------------|------------|----------|-------------|
| AUTHZ-001 | PRIVILEGE_GRANTED | NOTICE | Privilege granted |
| AUTHZ-002 | PRIVILEGE_REVOKED | NOTICE | Privilege revoked |
| AUTHZ-003 | ACCESS_DENIED | WARNING | Access denied |
| AUTHZ-004 | ROLE_ACTIVATED | INFO | Role activated (SET ROLE) |
| AUTHZ-005 | ROLE_DEACTIVATED | INFO | Role deactivated |
| AUTHZ-006 | ROLE_CREATED | NOTICE | Role created |
| AUTHZ-007 | ROLE_DROPPED | NOTICE | Role dropped |
| AUTHZ-008 | GROUP_MEMBER_ADDED | NOTICE | User added to group |
| AUTHZ-009 | GROUP_MEMBER_REMOVED | NOTICE | User removed from group |
| AUTHZ-010 | RLS_POLICY_CREATED | NOTICE | RLS policy created |
| AUTHZ-011 | RLS_POLICY_DROPPED | NOTICE | RLS policy dropped |
| AUTHZ-012 | RLS_POLICY_APPLIED | DEBUG | RLS policy filtered rows |
| AUTHZ-013 | SUPERUSER_GRANTED | ALERT | Superuser privilege granted |
| AUTHZ-014 | SUPERUSER_REVOKED | NOTICE | Superuser privilege revoked |

#### 2.3.3 Data Access Events

| Event Code | Event Name | Severity | Description |
|------------|------------|----------|-------------|
| DATA-001 | SELECT_EXECUTED | INFO | SELECT query executed |
| DATA-002 | INSERT_EXECUTED | INFO | INSERT executed |
| DATA-003 | UPDATE_EXECUTED | INFO | UPDATE executed |
| DATA-004 | DELETE_EXECUTED | INFO | DELETE executed |
| DATA-005 | TRUNCATE_EXECUTED | WARNING | Table truncated |
| DATA-006 | BULK_EXPORT | WARNING | Large data export |
| DATA-007 | DDL_EXECUTED | NOTICE | Schema change executed |
| DATA-008 | SENSITIVE_ACCESS | NOTICE | Sensitive data accessed |

#### 2.3.4 Security Events

| Event Code | Event Name | Severity | Description |
|------------|------------|----------|-------------|
| SEC-001 | KEY_GENERATED | NOTICE | Encryption key generated |
| SEC-002 | KEY_ROTATED | NOTICE | Key rotation completed |
| SEC-003 | KEY_DESTROYED | NOTICE | Key destroyed |
| SEC-004 | KEY_EXPORT | WARNING | Key exported |
| SEC-005 | CERT_ISSUED | NOTICE | Certificate issued |
| SEC-006 | CERT_REVOKED | WARNING | Certificate revoked |
| SEC-007 | CERT_EXPIRED | WARNING | Certificate expired |
| SEC-008 | TLS_HANDSHAKE_FAILED | WARNING | TLS negotiation failed |
| SEC-009 | SECURITY_VIOLATION | ALERT | Security policy violated |
| SEC-010 | INTRUSION_SUSPECTED | CRITICAL | Possible intrusion |

#### 2.3.5 Cluster Events

| Event Code | Event Name | Severity | Description |
|------------|------------|----------|-------------|
| CLUST-001 | NODE_JOINED | NOTICE | Node joined cluster |
| CLUST-002 | NODE_LEFT | NOTICE | Node left cluster |
| CLUST-003 | NODE_FENCED | WARNING | Node fenced |
| CLUST-004 | NODE_UNFENCED | NOTICE | Node unfenced |
| CLUST-005 | QUORUM_ACHIEVED | INFO | Quorum obtained |
| CLUST-006 | QUORUM_LOST | CRITICAL | Quorum lost |
| CLUST-007 | FAILOVER_STARTED | WARNING | Failover initiated |
| CLUST-008 | FAILOVER_COMPLETED | NOTICE | Failover completed |
| CLUST-009 | SPLIT_BRAIN_DETECTED | CRITICAL | Network partition detected |
| CLUST-010 | DEGRADED_MODE_ENTERED | WARNING | Operating in degraded mode |
| CLUST-011 | DEGRADED_MODE_EXITED | NOTICE | Normal operation resumed |

---

## 3. Audit Log Format

### 3.1 Event Record Structure

```json
{
    "event_id": "0198f0b2-1234-7890-abcd-1234567890ab",
    "event_code": "AUTH-003",
    "event_name": "AUTH_FAILURE",
    "category": "AUTHENTICATION",
    "severity": 3,
    "severity_name": "WARNING",
    
    "timestamp": "2026-01-15T10:30:45.123456789Z",
    "timestamp_unix_ns": 1768489845123456789,
    
    "node": {
        "node_uuid": "0198f0b2-0001-...",
        "node_name": "node-1",
        "cluster_uuid": "0198f0b2-0000-..."
    },
    
    "session": {
        "session_uuid": "0198f0b2-2222-...",
        "user_uuid": null,
        "username": "alice",
        "client_address": "192.168.1.100",
        "client_port": 54321,
        "protocol": "postgresql",
        "tls_version": "TLSv1.3"
    },
    
    "details": {
        "reason": "INVALID_PASSWORD",
        "attempt_count": 3,
        "remaining_attempts": 2,
        "auth_provider": "internal"
    },
    
    "affected_objects": [],
    
    "context": {
        "transaction_id": null,
        "query_hash": null,
        "execution_time_ms": null
    },
    
    "chain": {
        "sequence": 1547892,
        "previous_hash": "a1b2c3d4e5f6...",
        "event_hash": "f6e5d4c3b2a1..."
    }
}
```

### 3.2 Field Definitions

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| event_id | UUID | Yes | Unique event identifier |
| event_code | string | Yes | Event type code |
| event_name | string | Yes | Human-readable event name |
| category | string | Yes | Event category |
| severity | integer | Yes | Severity level (0-7) |
| timestamp | ISO8601 | Yes | Event timestamp (UTC) |
| node | object | Yes | Source node information |
| session | object | No | Session context if applicable |
| details | object | Yes | Event-specific details |
| affected_objects | array | No | Objects affected by event |
| context | object | No | Additional context |
| chain | object | L4+ | Chain hashing data |

### 3.3 Affected Objects Format

```json
"affected_objects": [
    {
        "object_type": "TABLE",
        "object_uuid": "0198f0b2-3333-...",
        "object_name": "employees",
        "schema_name": "hr",
        "database_name": "company_db",
        "action": "SELECT",
        "row_count": 150
    }
]
```

---

## 4. Tamper-Evident Logging

### 4.1 Chain Hashing

Each audit event includes a hash that chains to the previous event:

```
┌─────────────────────────────────────────────────────────────────┐
│                    CHAIN HASHING                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Event N-1          Event N            Event N+1                │
│  ┌─────────┐        ┌─────────┐        ┌─────────┐             │
│  │ Data    │        │ Data    │        │ Data    │             │
│  │         │        │         │        │         │             │
│  │ Prev:   │        │ Prev:   │        │ Prev:   │             │
│  │ Hash(N-2)────────► Hash(N-1)────────► Hash(N) │             │
│  │         │        │         │        │         │             │
│  │ Hash:   │        │ Hash:   │        │ Hash:   │             │
│  │ H(N-1)  │        │ H(N)    │        │ H(N+1)  │             │
│  └─────────┘        └─────────┘        └─────────┘             │
│                                                                  │
│  Hash computation:                                               │
│  H(N) = SHA-256(sequence || previous_hash || event_data)        │
│                                                                  │
│  Tamper detection:                                               │
│  • Modifying any event breaks the chain                         │
│  • Deletion detectable via sequence gaps                        │
│  • Insertion would require re-hashing all subsequent events     │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 4.2 Hash Computation

```python
def compute_event_hash(event, previous_hash):
    """Compute hash for audit event."""
    
    # Canonical serialization (deterministic JSON)
    canonical_event = canonicalize_json({
        'event_id': event.event_id,
        'event_code': event.event_code,
        'timestamp': event.timestamp_unix_ns,
        'node_uuid': event.node.node_uuid,
        'session_uuid': event.session.session_uuid if event.session else None,
        'details': event.details,
        'affected_objects': event.affected_objects
    })
    
    # Combine with sequence and previous hash
    hash_input = struct.pack('>Q', event.sequence) + \
                 bytes.fromhex(previous_hash) + \
                 canonical_event.encode('utf-8')
    
    return hashlib.sha256(hash_input).hexdigest()
```

### 4.3 Chain Verification

```python
def verify_audit_chain(events):
    """Verify integrity of audit event chain."""
    
    errors = []
    
    for i, event in enumerate(events):
        # Check sequence continuity
        if i > 0 and event.chain.sequence != events[i-1].chain.sequence + 1:
            errors.append(ChainError(
                type='SEQUENCE_GAP',
                position=i,
                expected=events[i-1].chain.sequence + 1,
                found=event.chain.sequence
            ))
        
        # Check hash linkage
        if i > 0:
            expected_prev = events[i-1].chain.event_hash
            if event.chain.previous_hash != expected_prev:
                errors.append(ChainError(
                    type='HASH_MISMATCH',
                    position=i,
                    expected=expected_prev,
                    found=event.chain.previous_hash
                ))
        
        # Verify event hash
        computed_hash = compute_event_hash(event, event.chain.previous_hash)
        if computed_hash != event.chain.event_hash:
            errors.append(ChainError(
                type='HASH_INVALID',
                position=i,
                expected=computed_hash,
                found=event.chain.event_hash
            ))
    
    return len(errors) == 0, errors
```

### 4.4 Signed Checkpoints

Periodic checkpoints are signed for additional integrity:

```json
{
    "checkpoint_id": "0198f0b2-5555-...",
    "timestamp": "2026-01-15T11:00:00Z",
    "sequence_start": 1540000,
    "sequence_end": 1550000,
    "event_count": 10001,
    "first_hash": "abcdef123456...",
    "last_hash": "654321fedcba...",
    "merkle_root": "112233445566...",
    "signature": "BASE64_ENCODED_SIGNATURE",
    "signing_key_id": "audit-signing-key-2026"
}
```

---

## 5. Audit Log Storage

### 5.1 Storage Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                  AUDIT STORAGE ARCHITECTURE                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Engine                                                          │
│    │                                                             │
│    │ Audit events                                               │
│    ▼                                                             │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │              Audit Buffer (in-memory)                    │    │
│  │  • Ring buffer for recent events                        │    │
│  │  • Immediate durability mode: sync to disk on write     │    │
│  │  • Batched mode: periodic flush                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│    │                                                             │
│    ├────────────────────┬────────────────────┐                  │
│    ▼                    ▼                    ▼                  │
│  ┌──────────┐      ┌──────────┐      ┌──────────────────┐      │
│  │ Local    │      │ Syslog   │      │ External Storage │      │
│  │ Files    │      │          │      │ (S3, etc.)       │      │
│  └──────────┘      └──────────┘      └──────────────────┘      │
│                                                                  │
│  Local Files:                                                    │
│  • /var/log/scratchbird/audit/                                  │
│  • Rotated daily or by size                                     │
│  • Compressed after rotation                                    │
│  • Encrypted at rest (LEK)                                      │
│                                                                  │
│  External Storage (Level 5-6):                                  │
│  • Real-time streaming                                          │
│  • Write-once storage (WORM)                                    │
│  • Separate failure domain                                      │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 Local Audit Table

```sql
CREATE TABLE sys.audit_log (
    event_id            UUID PRIMARY KEY,
    event_code          VARCHAR(16) NOT NULL,
    event_name          VARCHAR(64) NOT NULL,
    category            VARCHAR(32) NOT NULL,
    severity            SMALLINT NOT NULL,
    
    -- Timestamp
    event_timestamp     TIMESTAMP WITH TIME ZONE NOT NULL,
    event_timestamp_ns  BIGINT NOT NULL,
    
    -- Node
    node_uuid           UUID NOT NULL,
    
    -- Session (nullable)
    session_uuid        UUID,
    user_uuid           UUID,
    username            VARCHAR(128),
    client_address      INET,
    
    -- Event data (JSONB for flexibility)
    details             JSONB NOT NULL,
    affected_objects    JSONB,
    context             JSONB,
    
    -- Chain hashing
    chain_sequence      BIGINT NOT NULL,
    previous_hash       CHAR(64) NOT NULL,
    event_hash          CHAR(64) NOT NULL,
    
    -- Indexing
    INDEX idx_audit_timestamp (event_timestamp),
    INDEX idx_audit_user (user_uuid),
    INDEX idx_audit_category (category),
    INDEX idx_audit_severity (severity),
    INDEX idx_audit_sequence (chain_sequence)
);
```

### 5.3 Audit Log Rotation

```python
# Rotation policy
ROTATION_POLICIES = {
    'time': '1 day',          # Rotate daily
    'size': '100 MB',         # Or when reaching size
    'events': 1000000,        # Or when reaching event count
}

def rotate_audit_log():
    """Rotate audit log files."""
    
    # 1. Close current log file
    current_log.close()
    
    # 2. Create checkpoint
    checkpoint = create_checkpoint(current_log)
    
    # 3. Rename with timestamp
    rotated_name = f"audit_{timestamp}.log"
    rename(current_log.path, rotated_name)
    
    # 4. Compress
    compress(rotated_name, f"{rotated_name}.zst")
    
    # 5. Start new log file
    new_log = create_audit_log()
    new_log.set_previous_hash(checkpoint.last_hash)
    
    # 6. Archive to external storage (if configured)
    if external_storage_enabled:
        upload_to_external(f"{rotated_name}.zst", checkpoint)
```

### 5.4 Retention Policy

```sql
-- Retention policy configuration
CREATE TABLE sys.audit_retention_policy (
    policy_id           UUID PRIMARY KEY,
    category            VARCHAR(32),        -- NULL = all categories
    severity_min        SMALLINT,           -- Minimum severity to retain
    
    retention_period    INTERVAL NOT NULL,  -- How long to keep
    archive_after       INTERVAL,           -- When to archive
    delete_after        INTERVAL,           -- When to delete
    
    storage_class       VARCHAR(32),        -- 'HOT', 'WARM', 'COLD', 'ARCHIVE'
    
    is_active           BOOLEAN DEFAULT TRUE
);

-- Example policies
INSERT INTO sys.audit_retention_policy VALUES
    -- Keep all CRITICAL+ events for 7 years
    (gen_uuid_v7(), NULL, 2, '7 years', '90 days', NULL, 'ARCHIVE', TRUE),
    
    -- Keep auth events for 2 years
    (gen_uuid_v7(), 'AUTHENTICATION', NULL, '2 years', '30 days', '2 years', 'WARM', TRUE),
    
    -- Keep data access events for 1 year
    (gen_uuid_v7(), 'DATA_ACCESS', NULL, '1 year', '7 days', '1 year', 'WARM', TRUE),
    
    -- Delete DEBUG events after 30 days
    (gen_uuid_v7(), NULL, 7, '30 days', NULL, '30 days', 'HOT', TRUE);
```

---

## 6. Audit Configuration

### 6.1 Audit Policies

```sql
-- Enable/disable audit categories
CREATE TABLE sys.audit_policies (
    policy_uuid         UUID PRIMARY KEY,
    
    -- Scope
    scope_type          ENUM('GLOBAL', 'DATABASE', 'SCHEMA', 'TABLE', 'USER'),
    scope_uuid          UUID,               -- NULL for GLOBAL
    
    -- What to audit
    category            VARCHAR(32),        -- NULL = all
    event_code          VARCHAR(16),        -- NULL = all in category
    
    -- Conditions
    min_severity        SMALLINT DEFAULT 7, -- Log this severity and above
    
    -- For data access
    audit_select        BOOLEAN DEFAULT FALSE,
    audit_insert        BOOLEAN DEFAULT TRUE,
    audit_update        BOOLEAN DEFAULT TRUE,
    audit_delete        BOOLEAN DEFAULT TRUE,
    
    -- Row filtering
    audit_condition     TEXT,               -- SQL predicate for row-level audit
    
    -- Status
    is_enabled          BOOLEAN DEFAULT TRUE,
    
    -- Metadata
    created_at          TIMESTAMP NOT NULL,
    created_by          UUID
);
```

### 6.2 SQL Configuration Commands

```sql
-- Enable full auditing on a table
ALTER TABLE hr.employees ENABLE AUDIT ALL;

-- Audit only sensitive columns
ALTER TABLE hr.employees ENABLE AUDIT SELECT (salary, ssn);

-- Audit with condition (only audit high-value transactions)
ALTER TABLE finance.transactions ENABLE AUDIT ALL
WHERE amount > 10000;

-- Audit specific user
ALTER USER admin_alice ENABLE AUDIT ALL;

-- Set minimum severity for logging
ALTER SYSTEM SET audit.min_severity = 'WARNING';

-- Enable external audit streaming
ALTER SYSTEM SET audit.external_destination = 's3://audit-bucket/';
```

---

## 7. SIEM Integration

### 7.1 Supported Formats

| Format | Description | Use Case |
|--------|-------------|----------|
| JSON Lines | One JSON object per line | General SIEM |
| CEF | Common Event Format | ArcSight, QRadar |
| LEEF | Log Event Extended Format | QRadar |
| Syslog | RFC 5424 structured data | Traditional SIEM |

### 7.2 Output Destinations

```
┌─────────────────────────────────────────────────────────────────┐
│                  SIEM INTEGRATION                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────┐                                            │
│  │  Audit Engine   │                                            │
│  └────────┬────────┘                                            │
│           │                                                      │
│           ├──────────────────────────────────────────────────   │
│           │                     │                     │          │
│           ▼                     ▼                     ▼          │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │     Syslog      │  │     Kafka       │  │      HTTP       │ │
│  │                 │  │                 │  │    Webhook      │ │
│  │  UDP/TCP/TLS    │  │  Topic per      │  │                 │ │
│  │  RFC 5424       │  │  category       │  │  POST JSON      │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘ │
│           │                     │                     │          │
│           ▼                     ▼                     ▼          │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │    Splunk       │  │   Elastic       │  │    Custom       │ │
│  │    QRadar       │  │   Datadog       │  │    SIEM         │ │
│  │    ArcSight     │  │   Sumo Logic    │  │                 │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘ │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 7.3 Syslog Configuration

```sql
-- Configure syslog output
ALTER SYSTEM SET audit.syslog.enabled = TRUE;
ALTER SYSTEM SET audit.syslog.server = 'siem.corp.example.com';
ALTER SYSTEM SET audit.syslog.port = 6514;
ALTER SYSTEM SET audit.syslog.protocol = 'TLS';
ALTER SYSTEM SET audit.syslog.facility = 'LOCAL0';
ALTER SYSTEM SET audit.syslog.format = 'CEF';
```

### 7.4 Kafka Configuration

```sql
-- Configure Kafka output
ALTER SYSTEM SET audit.kafka.enabled = TRUE;
ALTER SYSTEM SET audit.kafka.brokers = 'kafka1:9092,kafka2:9092';
ALTER SYSTEM SET audit.kafka.topic_prefix = 'scratchbird.audit';
ALTER SYSTEM SET audit.kafka.security_protocol = 'SASL_SSL';
ALTER SYSTEM SET audit.kafka.sasl_mechanism = 'SCRAM-SHA-512';
```

### 7.5 CEF Format Example

```
CEF:0|ScratchBird|Database|1.0|AUTH-003|Authentication Failure|5|
    src=192.168.1.100 
    suser=alice 
    outcome=Failure 
    reason=INVALID_PASSWORD 
    cs1Label=SessionUUID cs1=0198f0b2-2222-...
    cs2Label=Protocol cs2=postgresql
    cn1Label=AttemptCount cn1=3
```

---

## 8. Real-Time Alerting

### 8.1 Alert Rules

```sql
CREATE TABLE sys.audit_alert_rules (
    rule_uuid           UUID PRIMARY KEY,
    rule_name           VARCHAR(128) NOT NULL,
    
    -- Trigger condition
    event_pattern       JSONB NOT NULL,     -- Pattern to match
    threshold_count     INTEGER,            -- Events in window
    threshold_window    INTERVAL,           -- Time window
    
    -- Alert configuration
    severity            SMALLINT NOT NULL,
    alert_channels      VARCHAR(32)[],      -- 'email', 'slack', 'pagerduty'
    
    -- Cooldown
    cooldown_period     INTERVAL DEFAULT '5 minutes',
    
    is_enabled          BOOLEAN DEFAULT TRUE
);

-- Example rules
INSERT INTO sys.audit_alert_rules VALUES
    -- Alert on any superuser grant
    (gen_uuid_v7(), 'Superuser Granted', 
     '{"event_code": "AUTHZ-013"}', NULL, NULL,
     1, ARRAY['email', 'pagerduty'], '1 hour', TRUE),
    
    -- Alert on multiple auth failures
    (gen_uuid_v7(), 'Brute Force Detected',
     '{"event_code": "AUTH-003"}', 10, '1 minute',
     2, ARRAY['email', 'slack'], '5 minutes', TRUE),
    
    -- Alert on security violation
    (gen_uuid_v7(), 'Security Violation',
     '{"category": "SECURITY", "severity": {"$lte": 2}}', NULL, NULL,
     1, ARRAY['email', 'pagerduty', 'slack'], '0', TRUE);
```

### 8.2 Alert Channels

```sql
CREATE TABLE sys.audit_alert_channels (
    channel_uuid        UUID PRIMARY KEY,
    channel_type        VARCHAR(32) NOT NULL,   -- 'email', 'slack', etc.
    channel_name        VARCHAR(128) NOT NULL,
    
    configuration       JSONB NOT NULL,
    
    is_enabled          BOOLEAN DEFAULT TRUE
);

-- Email channel
INSERT INTO sys.audit_alert_channels VALUES
    (gen_uuid_v7(), 'email', 'Security Team',
     '{"recipients": ["security@corp.example.com"],
       "smtp_server": "smtp.corp.example.com",
       "from": "scratchbird-alerts@corp.example.com"}', TRUE);

-- Slack channel
INSERT INTO sys.audit_alert_channels VALUES
    (gen_uuid_v7(), 'slack', 'DB Alerts Channel',
     '{"webhook_url": "https://hooks.slack.com/services/...",
       "channel": "#db-alerts"}', TRUE);

-- PagerDuty channel
INSERT INTO sys.audit_alert_channels VALUES
    (gen_uuid_v7(), 'pagerduty', 'Critical Alerts',
     '{"integration_key": "...",
       "severity_mapping": {"1": "critical", "2": "error"}}', TRUE);
```

---

## 9. Compliance Framework Mapping

### 9.1 Overview

ScratchBird audit features map to requirements in major compliance frameworks:

### 9.2 PCI-DSS Mapping

| PCI-DSS Requirement | ScratchBird Feature |
|---------------------|---------------------|
| 10.1 Audit trail links | User UUID in all events |
| 10.2.1 User access to cardholder data | DATA-001 SELECT_EXECUTED |
| 10.2.2 Actions by root/admin | AUTHZ-013 SUPERUSER_GRANTED |
| 10.2.3 Access to audit trails | Separate audit permissions |
| 10.2.4 Invalid logical access attempts | AUTHZ-003 ACCESS_DENIED |
| 10.2.5 Use of identification mechanisms | AUTH events |
| 10.2.6 Initialization of audit logs | SYS-001 STARTUP |
| 10.2.7 Creation/deletion of objects | DATA-007 DDL_EXECUTED |
| 10.3.1 User identification | user_uuid, username fields |
| 10.3.2 Type of event | event_code, event_name |
| 10.3.3 Date and time | timestamp with nanoseconds |
| 10.3.4 Success/failure | Event-specific details |
| 10.3.5 Origin of event | client_address, node_uuid |
| 10.3.6 Affected resource | affected_objects array |
| 10.5.1 Limit viewing of audit trails | RBAC on audit table |
| 10.5.2 Protect audit trail files | Encryption, chain hashing |
| 10.5.3 Promptly backup audit trail | External storage, replication |
| 10.5.4 Write logs for external technologies | SIEM integration |
| 10.5.5 File-integrity monitoring | Chain hashing, signed checkpoints |
| 10.7 Retain audit history ≥1 year | Retention policies |

### 9.3 HIPAA Mapping

| HIPAA Requirement | Section | ScratchBird Feature |
|-------------------|---------|---------------------|
| 164.312(b) Audit controls | Technical | Full audit logging |
| 164.308(a)(1)(ii)(D) Information system activity review | Administrative | Audit reports, SIEM |
| 164.312(d) Person or entity authentication | Technical | AUTH events, session tracking |
| 164.308(a)(5)(ii)(C) Log-in monitoring | Administrative | AUTH-003, AUTH-005 events |
| 164.312(c)(1) Integrity | Technical | Chain hashing |
| 164.312(c)(2) Mechanism to authenticate ePHI | Technical | Signed checkpoints |

### 9.4 SOC 2 Mapping

| SOC 2 Criteria | ScratchBird Feature |
|----------------|---------------------|
| CC6.1 Logical access security | Authorization audit events |
| CC6.2 Access removal | AUTH-013 SESSION_TERMINATED |
| CC6.3 Role-based access | ROLE events |
| CC7.1 Detect security events | Real-time alerting |
| CC7.2 Monitor system components | SIEM integration |
| CC7.3 Evaluate security events | Audit reports |
| CC7.4 Respond to security incidents | Alert channels |

### 9.5 GDPR Mapping

| GDPR Article | Requirement | ScratchBird Feature |
|--------------|-------------|---------------------|
| Art. 5(1)(f) Integrity and confidentiality | Audit log protection | Encryption, chain hashing |
| Art. 17 Right to erasure | Crypto-shredding support | Key destruction logging |
| Art. 30 Records of processing | Data access logging | DATA events |
| Art. 33 Breach notification | Incident detection | Security alerts |

---

## 10. Audit Reports

### 10.1 Built-in Reports

```sql
-- User activity report
SELECT * FROM sys.audit_report_user_activity(
    user_uuid := '0198f0b2-2222-...',
    start_time := '2026-01-01',
    end_time := '2026-01-31'
);

-- Failed login report
SELECT * FROM sys.audit_report_failed_logins(
    start_time := NOW() - INTERVAL '24 hours'
);

-- Privilege change report
SELECT * FROM sys.audit_report_privilege_changes(
    start_time := NOW() - INTERVAL '7 days'
);

-- Data access report (for compliance)
SELECT * FROM sys.audit_report_data_access(
    table_uuid := '0198f0b2-3333-...',
    start_time := '2026-01-01',
    end_time := '2026-01-31'
);

-- Chain integrity verification
SELECT * FROM sys.audit_verify_chain(
    start_sequence := 1000000,
    end_sequence := 1100000
);
```

### 10.2 Custom Report Views

```sql
-- Create custom audit view for compliance team
CREATE VIEW compliance_audit AS
SELECT 
    event_id,
    event_timestamp,
    event_name,
    username,
    client_address,
    details->>'reason' as reason,
    affected_objects
FROM sys.audit_log
WHERE category IN ('AUTHENTICATION', 'AUTHORIZATION', 'DATA_ACCESS')
  AND event_timestamp > NOW() - INTERVAL '90 days';

GRANT SELECT ON compliance_audit TO ROLE compliance_auditor;
```

---

## 11. Configuration Reference

### 11.1 Audit Policies

| Policy Key | Type | Default | Description |
|------------|------|---------|-------------|
| `audit.enabled` | boolean | true | Enable audit logging |
| `audit.min_severity` | integer | 6 | Minimum severity to log |
| `audit.buffer_size` | integer | 10000 | In-memory buffer size |
| `audit.flush_interval` | interval | 1s | Buffer flush frequency |
| `audit.sync_mode` | enum | BUFFERED | IMMEDIATE, BUFFERED |

### 11.2 Chain Hashing Policies

| Policy Key | Type | Default | Description |
|------------|------|---------|-------------|
| `audit.chain.enabled` | boolean | true | Enable chain hashing |
| `audit.chain.checkpoint_interval` | integer | 10000 | Events per checkpoint |
| `audit.chain.sign_checkpoints` | boolean | true | Sign checkpoint records |

### 11.3 External Storage Policies

| Policy Key | Type | Default | Description |
|------------|------|---------|-------------|
| `audit.external.enabled` | boolean | false | Enable external storage |
| `audit.external.type` | string | | s3, azure, gcs, http |
| `audit.external.destination` | string | | Destination URL |
| `audit.external.batch_size` | integer | 1000 | Events per batch |

### 11.4 SIEM Policies

| Policy Key | Type | Default | Description |
|------------|------|---------|-------------|
| `audit.siem.enabled` | boolean | false | Enable SIEM streaming |
| `audit.siem.format` | enum | JSON | JSON, CEF, LEEF |
| `audit.siem.transport` | enum | | syslog, kafka, http |

---

*End of Document*
