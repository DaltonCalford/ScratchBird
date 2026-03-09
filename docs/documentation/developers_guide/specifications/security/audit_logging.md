# Specification: Audit Logging

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | security/audit |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Security Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/security/auth_manager.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/security/auth_manager.h`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_security_issues.cpp`

## Synopsis

This specification defines the audit logging system in ScratchBird, including authentication events, authorization checks, DDL/DML logging, and compliance reporting.

## Scope

### In Scope

- Authentication audit events
- Authorization audit events
- DDL audit logging
- DML audit logging (optional)
- Audit log format and storage
- Audit configuration

### Out of Scope

- General query logging (slow query log)
- Performance metrics
- Debug logging

## Background

Audit logging provides an immutable record of security-relevant events for compliance, forensics, and monitoring. ScratchBird's audit system is designed to meet regulatory requirements while minimizing performance impact.

## Specification

### Audit Event Types

```cpp
// From auth_manager.h:260-284

enum class AuditEventType : uint8_t {
    // Authentication events
    AUTH_START = 1,
    AUTH_SUCCESS = 2,
    AUTH_FAILURE = 3,
    PASSWORD_CHANGE = 4,
    ACCOUNT_LOCKED = 5,
    ACCOUNT_UNLOCKED = 6,
    
    // Session events
    SESSION_START = 10,
    SESSION_END = 11,
    SESSION_TERMINATED = 12,
    
    // Authorization events
    PERMISSION_DENIED = 20,
    PERMISSION_GRANTED = 21,
    
    // DDL events
    DDL_CREATE = 30,
    DDL_ALTER = 31,
    DDL_DROP = 32,
    DDL_TRUNCATE = 33,
    
    // DML events (optional)
    DML_INSERT = 40,
    DML_UPDATE = 41,
    DML_DELETE = 42,
    DML_SELECT = 43,
    
    // RLS/CLS events
    RLS_POLICY_VIOLATION = 50,
    CLS_MASKING_APPLIED = 51,
    
    // System events
    CONFIG_CHANGE = 60,
    BACKUP_START = 61,
    BACKUP_COMPLETE = 62,
    RESTORE_START = 63,
    RESTORE_COMPLETE = 64
};
```

### Audit Event Structure

```cpp
// Base audit event
struct AuditEvent {
    // Event identification
    UUID event_id;                    // Unique event identifier
    AuditEventType type;              // Event type
    uint8_t severity;                 // 0-10 scale
    
    // Timestamp
    std::chrono::system_clock::time_point timestamp;
    uint64_t timestamp_ns;            // Nanosecond precision
    
    // Actor information
    std::string username;
    UUID user_id;
    std::string client_address;
    uint16_t client_port;
    std::string session_id;
    std::string application_name;
    
    // Object information (for DDL/DML)
    std::string database_name;
    std::string schema_name;
    std::string object_name;
    ObjectType object_type;
    UUID object_id;
    
    // Action details
    std::string command_tag;          // e.g., "CREATE TABLE"
    std::string command_text;         // SQL text (hashed for sensitive)
    uint64_t rows_affected;
    
    // Result
    bool success;
    std::string error_code;
    std::string error_message;
    
    // Additional context
    std::map<std::string, std::string> parameters;
    
    // Integrity
    std::string previous_hash;        // Chain hashing
    std::string signature;            // Digital signature
};
```

### Authentication Audit Events

```cpp
struct AuthAuditEvent : AuditEvent {
    AuthType auth_type;               // Method used
    AuthFailReason failure_reason;    // If failed
    std::string failure_message;
    uint32_t failed_attempts;         // Consecutive failures
    std::string mfa_method;           // TOTP, HOTP, etc.
    bool mfa_success;
};
```

### Audit Log Format

#### JSON Format (Structured)

```json
{
    "version": "1.0",
    "event_id": "550e8400-e29b-41d4-a716-446655440000",
    "type": "AUTH_SUCCESS",
    "severity": 3,
    "timestamp": "2026-03-08T14:30:00.123456789Z",
    "actor": {
        "username": "alice",
        "user_id": "550e8400-e29b-41d4-a716-446655440001",
        "client_address": "192.168.1.100",
        "client_port": 54321,
        "session_id": "sess_abc123",
        "application": "psql"
    },
    "object": {
        "database": "production",
        "type": "DATABASE"
    },
    "action": {
        "command_tag": "AUTHENTICATE",
        "auth_type": "SCRAM_SHA_256",
        "mfa_method": "TOTP",
        "mfa_success": true
    },
    "result": {
        "success": true
    },
    "integrity": {
        "previous_hash": "sha256:abc...",
        "hash": "sha256:def...",
        "signature": "rsa-sha256:ghi..."
    }
}
```

#### CEF Format (SIEM Integration)

```
CEF:0|ScratchBird|Database|0.1.0|AUTH_SUCCESS|Authentication Success|3|
    rt=Mar 08 2026 14:30:00Z
    src=192.168.1.100
    suser=alice
    duser=alice
    cs1=SCRAM_SHA_256 cs1Label=AuthMethod
    cs2=TOTP cs2Label=MFAMethod
    outcome=success
```

#### Syslog Format

```
<134>1 2026-03-08T14:30:00.123Z dbserver scratchbird 1234 AUTH_SUCCESS -
    [event_id="550e8400-e29b-41d4-a716-446655440000"]
    [username="alice"]
    [client="192.168.1.100"]
    [auth_method="SCRAM_SHA_256"]
    [outcome="success"]
```

### Audit Configuration

```sql
-- Enable audit logging
ALTER SYSTEM SET audit_logging = 'on';

-- Configure audit scope
ALTER SYSTEM SET audit_log_level = 'all';  -- none, auth, ddl, dml, all

-- Configure events to log
ALTER SYSTEM SET audit_log_events = 
    'AUTH_SUCCESS,AUTH_FAILURE,DDL_CREATE,DDL_DROP,PERMISSION_DENIED';

-- Configure output format
ALTER SYSTEM SET audit_log_format = 'json';  -- json, cef, syslog
ALTER SYSTEM SET audit_log_destination = 'file';  -- file, syslog, table
ALTER SYSTEM SET audit_log_directory = '/var/log/scratchbird/audit';
ALTER SYSTEM SET audit_log_filename = 'scratchbird_audit_%Y%m%d_%H%M%S';
ALTER SYSTEM SET audit_log_rotation_age = '1d';
ALTER SYSTEM SET audit_log_rotation_size = '100MB';

-- Configure object-specific auditing
CREATE AUDIT POLICY sensitive_tables
    FOR TABLE hr.employees, hr.salaries
    ACTIONS INSERT, UPDATE, DELETE;

-- Configure user-specific auditing
CREATE AUDIT POLICY privileged_users
    FOR USER admin_role
    ACTIONS ALL;
```

### Audit Logger Interface

```cpp
// From auth_manager.h:286-330

class AuditLogger {
public:
    virtual ~AuditLogger() = default;
    
    // Log authentication event
    virtual void logAuthEvent(const AuthAuditEvent& event) = 0;
    
    // Log general audit event
    virtual void logEvent(const AuditEvent& event) = 0;
    
    // Flush pending logs
    virtual void flush() = 0;
    
    // Configure logger
    virtual Status configure(const AuditConfig& config) = 0;
};

// File-based logger
class FileAuditLogger : public AuditLogger {
public:
    Status open(const std::string& path);
    void logEvent(const AuditEvent& event) override;
    void flush() override;
    
private:
    std::ofstream file_;
    std::mutex mutex_;
    bool chain_hashing_ = true;
    std::string last_hash_;
};

// Syslog logger
class SyslogAuditLogger : public AuditLogger {
public:
    Status open(const std::string& facility);
    void logEvent(const AuditEvent& event) override;
    void flush() override;
};
```

### Audit Log Integrity

```
Chain Hashing for Tamper Detection:

Event 1:  [data1] -> H(data1) = hash1
Event 2:  [data2 + hash1] -> H(data2 + hash1) = hash2
Event 3:  [data3 + hash2] -> H(data3 + hash2) = hash3
...

Verification:
  Recompute hashes and verify chain
  Any modification breaks the chain
```

### Audit Log Rotation

```cpp
struct AuditRotationConfig {
    // Time-based rotation
    std::chrono::hours rotation_age{24};  // Daily
    
    // Size-based rotation
    uint64_t rotation_size_bytes = 100 * 1024 * 1024;  // 100MB
    
    // Retention
    uint32_t max_files = 30;  // Keep 30 days
    bool compress_old = true;
    
    // Integrity
    bool sign_files = true;
};

class AuditLogRotator {
public:
    Status rotateIfNeeded();
    Status cleanupOldFiles();
    Status verifyFileIntegrity(const std::string& path);
};
```

### Audit Event Filtering

```cpp
class AuditFilter {
public:
    bool shouldLog(const AuditEvent& event) const {
        // Check event type
        if (!config_.event_types.contains(event.type)) {
            return false;
        }
        
        // Check severity threshold
        if (event.severity < config_.min_severity) {
            return false;
        }
        
        // Check user exclusions
        if (config_.excluded_users.contains(event.username)) {
            return false;
        }
        
        // Check object filters
        if (!config_.object_types.empty() && 
            !config_.object_types.contains(event.object_type)) {
            return false;
        }
        
        return true;
    }
};
```

### SQL Audit Log Views

```sql
-- View recent audit events
SELECT * FROM sb_audit_log 
ORDER BY timestamp DESC 
LIMIT 100;

-- View authentication failures
SELECT * FROM sb_audit_log 
WHERE type = 'AUTH_FAILURE' 
  AND timestamp > now() - interval '1 hour';

-- View DDL changes
SELECT * FROM sb_audit_log 
WHERE type LIKE 'DDL_%' 
ORDER BY timestamp DESC;

-- View permission denied events
SELECT username, object_name, command_tag, timestamp
FROM sb_audit_log 
WHERE type = 'PERMISSION_DENIED';

-- Audit statistics
SELECT 
    type,
    count(*) as event_count,
    min(timestamp) as first_seen,
    max(timestamp) as last_seen
FROM sb_audit_log 
WHERE timestamp > now() - interval '7 days'
GROUP BY type
ORDER BY event_count DESC;
```

### Compliance Reports

```sql
-- SOX compliance: Track schema changes
SELECT 
    timestamp,
    username,
    command_tag,
    schema_name,
    object_name
FROM sb_audit_log 
WHERE type IN ('DDL_CREATE', 'DDL_ALTER', 'DDL_DROP')
  AND timestamp BETWEEN '2026-01-01' AND '2026-03-31'
ORDER BY timestamp;

-- PCI-DSS: Failed authentication attempts
SELECT 
    timestamp,
    username,
    client_address,
    failed_attempts
FROM sb_audit_log 
WHERE type = 'AUTH_FAILURE'
  AND failed_attempts > 3;

-- HIPAA: Access to PHI tables
SELECT 
    timestamp,
    username,
    command_tag,
    rows_affected
FROM sb_audit_log 
WHERE schema_name = 'patient_data'
  AND type LIKE 'DML_%';
```

## Invariants

1. **Immutability**: Written audit logs cannot be modified
   - Verification: Chain hashing and signatures

2. **Completeness**: Configured events are always logged
   - Verification: Synchronous logging for critical events

3. **Ordering**: Events logged in chronological order
   - Verification: Timestamp ordering preserved

4. **Non-Repudiation**: Events cryptographically signed
   - Verification: Digital signatures

## Performance Considerations

| Configuration | Impact | Recommendation |
|---------------|--------|----------------|
| Async logging | Low | Use for high-volume |
| Sync logging | Medium | Use for critical events |
| DML logging | High | Enable only when required |
| Full SQL text | Medium | Hash sensitive content |

## Related Specifications

- `authentication_flow.md` - Authentication events
- `authorization_model.md` - Permission events
- `rls_policy_enforcement.md` - RLS events

## Appendix

### Audit Log Retention Policy

| Event Type | Retention | Reason |
|------------|-----------|--------|
| Authentication | 1 year | Security forensics |
| DDL | 7 years | Compliance (SOX) |
| DML (sensitive) | 7 years | Compliance (HIPAA) |
| DML (general) | 90 days | Operational |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
