# Error Codes Reference

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-18

---

## Overview

ScratchBird uses structured error codes to identify and categorize errors. Error codes follow a hierarchical pattern that indicates the error category and specific cause.

---

## Error Code Format

Error codes are organized by category:

```
0xCCXX
  │ │
  │ └── Specific error within category
  └──── Error category
```

---

## Error Categories

| Category | Range | Description |
|----------|-------|-------------|
| Syntax | 0x1000-0x1FFF | SQL parse errors |
| Semantic | 0x2000-0x2FFF | Type and name resolution errors |
| Execution | 0x3000-0x3FFF | Runtime errors |
| Constraint | 0x4000-0x4FFF | Constraint violations |
| Transaction | 0x5000-0x5FFF | Transaction errors |
| Storage | 0x6000-0x6FFF | I/O and storage errors |
| Security | 0x7000-0x7FFF | Authentication/authorization errors |
| System | 0x8000-0x8FFF | Internal system errors |

---

## Syntax Errors (0x1xxx)

| Code | Name | Description |
|------|------|-------------|
| 0x1001 | SYNTAX_ERROR | General syntax error |
| 0x1002 | UNEXPECTED_TOKEN | Unexpected token in input |
| 0x1003 | MISSING_KEYWORD | Required keyword missing |
| 0x1004 | INVALID_IDENTIFIER | Invalid identifier format |
| 0x1005 | UNTERMINATED_STRING | String literal not closed |
| 0x1006 | INVALID_NUMBER | Invalid numeric literal |
| 0x1007 | RESERVED_WORD | Reserved word used as identifier |

---

## Semantic Errors (0x2xxx)

| Code | Name | Description |
|------|------|-------------|
| 0x2001 | UNDEFINED_TABLE | Table does not exist |
| 0x2002 | UNDEFINED_COLUMN | Column does not exist |
| 0x2003 | UNDEFINED_FUNCTION | Function does not exist |
| 0x2004 | UNDEFINED_TYPE | Type does not exist |
| 0x2005 | AMBIGUOUS_COLUMN | Column reference is ambiguous |
| 0x2006 | TYPE_MISMATCH | Incompatible types |
| 0x2007 | WRONG_ARGUMENT_COUNT | Function called with wrong number of arguments |
| 0x2008 | UNDEFINED_SCHEMA | Schema does not exist |
| 0x2009 | UNDEFINED_INDEX | Index does not exist |
| 0x200A | UNDEFINED_CONSTRAINT | Constraint does not exist |

---

## Execution Errors (0x3xxx)

| Code | Name | Description |
|------|------|-------------|
| 0x3001 | DIVISION_BY_ZERO | Division by zero |
| 0x3002 | NULL_POINTER | Unexpected NULL value |
| 0x3003 | OVERFLOW | Numeric overflow |
| 0x3004 | UNDERFLOW | Numeric underflow |
| 0x3005 | INVALID_CAST | Cannot cast value to target type |
| 0x3006 | OUT_OF_RANGE | Value out of valid range |
| 0x3007 | INVALID_ARGUMENT | Invalid function argument |
| 0x3008 | STRING_TOO_LONG | String exceeds maximum length |
| 0x3009 | INVALID_REGEX | Invalid regular expression |
| 0x300A | TIMEOUT | Operation timed out |

---

## Constraint Errors (0x4xxx)

| Code | Name | Description |
|------|------|-------------|
| 0x4001 | NOT_NULL_VIOLATION | NOT NULL constraint violated |
| 0x4002 | UNIQUE_VIOLATION | UNIQUE constraint violated |
| 0x4003 | PRIMARY_KEY_VIOLATION | PRIMARY KEY constraint violated |
| 0x4004 | FOREIGN_KEY_VIOLATION | FOREIGN KEY constraint violated |
| 0x4005 | CHECK_VIOLATION | CHECK constraint violated |
| 0x4006 | EXCLUSION_VIOLATION | Exclusion constraint violated |
| 0x4007 | RESTRICT_VIOLATION | RESTRICT action violated |

---

## Transaction Errors (0x5xxx)

| Code | Name | Description |
|------|------|-------------|
| 0x5001 | TRANSACTION_CONFLICT | Transaction conflict detected |
| 0x5002 | DEADLOCK_DETECTED | Deadlock detected |
| 0x5003 | SERIALIZATION_FAILURE | Serializable isolation violation |
| 0x5004 | TRANSACTION_ABORTED | Transaction has been aborted |
| 0x5005 | INVALID_TRANSACTION_STATE | Invalid transaction state |
| 0x5006 | SAVEPOINT_NOT_FOUND | Savepoint does not exist |
| 0x5007 | TRANSACTION_ROLLBACK | Transaction rolled back |
| 0x5008 | LOCK_TIMEOUT | Lock acquisition timed out |
| 0x5009 | TRANSACTION_TOO_LONG | Transaction exceeded time limit |

---

## Storage Errors (0x6xxx)

| Code | Name | Description |
|------|------|-------------|
| 0x6001 | IO_ERROR | General I/O error |
| 0x6002 | DISK_FULL | No space left on device |
| 0x6003 | FILE_NOT_FOUND | Database file not found |
| 0x6004 | PERMISSION_DENIED | File permission denied |
| 0x6005 | CORRUPTED_PAGE | Page checksum failure |
| 0x6006 | CORRUPTED_INDEX | Index corruption detected |
| 0x6007 | OUT_OF_MEMORY | Insufficient memory |
| 0x6008 | TOO_MANY_CONNECTIONS | Connection limit exceeded |
| 0x6009 | TABLE_FULL | Table size limit exceeded |

---

## Security Errors (0x7xxx)

| Code | Name | Description |
|------|------|-------------|
| 0x7001 | AUTHENTICATION_FAILED | Authentication failed |
| 0x7002 | PERMISSION_DENIED | Insufficient privileges |
| 0x7003 | INVALID_PASSWORD | Invalid password |
| 0x7004 | ACCOUNT_LOCKED | User account is locked |
| 0x7005 | PASSWORD_EXPIRED | Password has expired |
| 0x7006 | SSL_REQUIRED | SSL connection required |
| 0x7007 | INVALID_CERTIFICATE | Invalid client certificate |
| 0x7008 | ACCESS_DENIED | Access to object denied |

---

## System Errors (0x8xxx)

| Code | Name | Description |
|------|------|-------------|
| 0x8001 | INTERNAL_ERROR | Internal error |
| 0x8002 | NOT_IMPLEMENTED | Feature not implemented |
| 0x8003 | CONFIGURATION_ERROR | Configuration error |
| 0x8004 | RESOURCE_EXHAUSTED | System resource exhausted |
| 0x8005 | SHUTDOWN_IN_PROGRESS | Server is shutting down |
| 0x8006 | FEATURE_DISABLED | Feature is disabled |
| 0x8007 | VERSION_MISMATCH | Version incompatibility |

---

## Error Context

Errors include additional context:

```cpp
struct ErrorContext {
    int error_code;          // Numeric error code
    std::string sqlstate;    // SQL state code (5 characters)
    std::string message;     // Human-readable message
    std::string detail;      // Additional details
    std::string hint;        // Suggested fix
    std::string where;       // Error location/stack trace
    int line;                // Line number in SQL
    int column;              // Column number in SQL
};
```

---

## SQLSTATE Codes

ScratchBird also provides standard SQLSTATE codes for compatibility:

| Class | Description |
|-------|-------------|
| 00 | Successful completion |
| 01 | Warning |
| 02 | No data |
| 08 | Connection exception |
| 22 | Data exception |
| 23 | Integrity constraint violation |
| 25 | Invalid transaction state |
| 28 | Invalid authorization specification |
| 40 | Transaction rollback |
| 42 | Syntax error or access rule violation |
| 53 | Insufficient resources |
| 54 | Program limit exceeded |
| 55 | Object not in prerequisite state |
| 57 | Operator intervention |
| 58 | System error |
| XX | Internal error |

---

## Handling Errors

### In SQL

```sql
-- Transaction with error handling
BEGIN;
INSERT INTO users (name) VALUES ('John');
-- If error occurs, transaction is aborted
COMMIT;  -- Will fail if previous statement had error
```

### In Application Code

```python
try:
    conn.execute("INSERT INTO users (name) VALUES (?)", ("John",))
except DatabaseError as e:
    if e.error_code == 0x4002:  # UNIQUE_VIOLATION
        print("User already exists")
    elif e.error_code == 0x4001:  # NOT_NULL_VIOLATION
        print("Name is required")
    else:
        raise
```

---

## Related Documents

- [SQL Syntax Reference](SQL-Syntax.md)
- [Troubleshooting Guide](../troubleshooting/Common-Errors.md)
