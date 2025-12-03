# Work Package 7: Server & Client

**Status:** NOT STARTED
**Priority:** P0-P2 Mixed
**Estimated Hours:** 16-20
**Files:** src/server/server_session.cpp, src/client/connection.cpp

---

## Overview

The client/server networking layer has incomplete prepared statement handling, missing savepoint support, and validation gaps.

---

## Tasks

### NET-1: PreparedStatement parameters (HIGH - SECURITY)
**File:** src/client/connection.cpp
**Lines:** 920-936
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Simple parameter substitution for now
// Phase 5 Enhancement: Proper parameter substitution with escaping
// Currently ignores stmt.impl_->params_ completely
```

**Required Changes:**
1. Parse SQL to find parameter placeholders ($1, $2, ?, etc.)
2. Safely substitute parameter values with proper escaping
3. Handle different data types correctly

**Implementation Options:**
A. Client-side substitution with escaping
B. Server-side prepared statement protocol (preferred)

**Option B (Recommended):**
1. Send PREPARE message with SQL template
2. Send EXECUTE message with parameter values
3. Server binds parameters and executes

**Verification:**
- [ ] Prepared statement with parameters works
- [ ] SQL injection attempts are blocked
- [ ] NULL parameters handled correctly
- [ ] All data types work

---

### NET-2: Server Savepoints (HIGH)
**File:** src/server/server_session.cpp
**Lines:** 334-339
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
sendError("Savepoints not yet implemented", "0A000");
return;
```

**Required Changes:**
1. Wire SAVEPOINT to TransactionManager::createSavepoint()
2. Wire RELEASE SAVEPOINT to TransactionManager::releaseSavepoint()
3. Wire ROLLBACK TO to TransactionManager::rollbackToSavepoint()

**Verification:**
- [ ] SAVEPOINT sp1 creates savepoint
- [ ] ROLLBACK TO sp1 works
- [ ] RELEASE SAVEPOINT sp1 works
- [ ] Nested savepoints work

---

### NET-M1: Query Cancellation (MEDIUM)
**File:** src/server/server_session.cpp
**Lines:** 367-371
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
sendError("Query cancellation not implemented", "0A000");
```

**Required Changes:**
1. Track currently executing query
2. Accept cancel request from client
3. Set cancellation flag
4. Check flag in executor loops
5. Abort cleanly on cancellation

**Verification:**
- [ ] Long-running query can be cancelled
- [ ] Clean abort without corruption

---

### NET-M2: releaseSavepoint validation (MEDIUM)
**File:** src/client/connection.cpp
**Lines:** 1020-1037
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Silently returns whatever status the receive returns
return receiveAndValidate();  // No validation of message type
```

**Required Changes:**
1. Check response message type
2. Verify success/error status
3. Parse error message if failed

**Verification:**
- [ ] Error from server is reported to client

---

### NET-M3: rollbackTo validation (MEDIUM)
**File:** src/client/connection.cpp
**Lines:** 1039-1056
**Status:** [ ] NOT STARTED

**Current Code:**
Same issue as NET-M2.

**Required Changes:**
Same as NET-M2.

**Verification:**
- [ ] Error from server is reported to client

---

### NET-L1: Transaction status parsing (LOW)
**File:** src/client/connection.cpp
**Lines:** 653-656
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
case MessageType::TRANSACTION_STATUS:
    // Update transaction state
    break;  // Does nothing
```

**Required Changes:**
1. Parse transaction status message
2. Update ConnectionImpl state
3. Make state queryable by client

**Verification:**
- [ ] conn.inTransaction() returns correct state

---

## Dependencies

- NET-1 may require wire protocol changes for server-side prepared statements
- NET-2 requires TransactionManager savepoint implementation (verify exists)
- NET-M1 requires executor cancellation points

---

## Security Considerations

NET-1 is a security-critical issue. The current implementation allows SQL injection because parameters are ignored. This MUST be fixed before any production use.

---

## Testing Plan

1. Prepared statement parameter binding tests
2. Savepoint lifecycle tests
3. Query cancellation tests (need long-running query)
4. Error propagation tests
5. Transaction state tracking tests

---

## Completion Checklist

- [ ] All 6 tasks implemented
- [ ] All 1020 existing tests pass
- [ ] SQL injection test cases pass
- [ ] Savepoint integration tests pass
- [ ] Code compiles without warnings

---

**Last Updated:** December 2, 2025
