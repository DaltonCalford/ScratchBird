# ScratchBird Connection Recovery and Continuation Model


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](transaction/FIREBIRD_CONSTANTS_REFERENCE.md)


**Version:** 1.0  
**Status:** Authoritative (V3)
**Last Updated:** February 2026  

---

## 1. Core Philosophy

**The server's job: Be a server.**

- Maintain shared state (buffer pool, lock manager, transactions)
- Execute SBLR (ScratchBird Language Runtime)
- Enable connection recovery and continuation
- The parser is expendable; the connection context persists

---

## 2. Connection Recovery Architecture

### 2.1 Separation of Concerns

```
PERSISTENT (Server):
- Connection Context (identified by UUID)
  * connection_id: UUID
  * user_id: UUID
  * database_id: UUID
  * transaction_id: UUID (if in transaction)
  * session_variables
  * prepared_statements
  * cursor_states
  * temporary_tables
  * created_at, last_activity

- Transaction State
  * Transaction ID
  * Start timestamp (for MGA visibility)
  * Isolation level
  * Read/write set tracking
  * Savepoints
  * Undo log references

TRANSIENT (Parser):
- Parser Process State
  * Client socket fd
  * Protocol state machine
  * Parse tree cache
  * Protocol-specific buffers
  * Current query being parsed
  * Dies with parser process
```

### 2.2 Connection UUID and Recovery Token

```cpp
struct ConnectionContext {
    // Primary identification (survives parser death)
    UUID connection_id;
    UUID session_token;
    
    // User context
    UUID user_id;
    std::string user_name;
    UUID database_id;
    
    // Transaction state (critical for recovery)
    UUID current_transaction;
    TransactionState tx_state;
    uint64_t snapshot_ost;
    
    // Session state
    SessionVariables variables;
    PreparedStatementCache prepared;
    CursorRegistry cursors;
    TempTableRegistry temp_tables;
    
    // Recovery settings
    bool allow_reconnect;
    Duration reconnect_timeout;
};
```

---

## 3. Connection Recovery Scenarios

### 3.1 Network Interruption

When network drops:
1. Parser detects socket error
2. Parser exits cleanly
3. Engine marks connection as "disconnected"
4. Connection context kept alive (5 min timeout)
5. Transaction remains active

When client reconnects:
1. New TCP connection to listener
2. Reconnection protocol: UUID + session token
3. Engine validates context exists and token matches
4. New parser assigned from pool
5. Context restored: same session vars, transaction, cursors
6. Client continues where they left off

If timeout expires:
- Transaction rolled back
- Temporary tables dropped
- Connection context freed
- Client must create new connection

### 3.2 Parser Crash

During query execution, parser crashes:
1. Engine detects parser death (socket close)
2. Connection marked as "disconnected, needs triage"
3. Transaction kept active (not rolled back)
4. Reconnection timer starts

Client reconnects:
1. New parser assigned
2. Triage performed:
   - Check if last query completed via MGA
   - Determine transaction state
3. Client informed of triage result:
   - "Transaction still active"
   - "Last query: FAILED/COMPLETED/UNKNOWN"
4. Client decides: COMMIT, ROLLBACK, or VERIFY

---

## 4. Triage Process

```
Parser disconnects/crashes:
        |
        v
Was transaction active?
    |         |
   YES        NO
    |         |
    v         v
Check MGA:     Check query status
Did writes      (read-only)
occur?
    |
  YES/NO
    |
    v
Determine action:
- Keep tx active + notify user
- Release locks + mark done
- Report error
```

### Triage Outcomes

| Scenario | Action | Client Sees |
|----------|--------|-------------|
| Read query completed | Release locks, mark done | "Reconnected, ready" |
| Write query completed | Keep tx active | "Reconnected, transaction active" |
| Query failed | Keep tx active | "Reconnected, last query failed" |
| Query status unknown | Keep tx active | "Reconnected, triage required" |

---

## 5. Server Responsibilities

```cpp
class ConnectionManager {
public:
    // Create new connection context
    ConnectionContext* createConnection(
        const UUID& user_id,
        const UUID& database_id,
        const ConnectionOptions& options
    );
    
    // Validate reconnection attempt
    ReconnectResult attemptReconnect(
        const UUID& connection_id,
        const UUID& session_token,
        const UUID& last_known_tx_id
    );
    
    // Mark connection as disconnected (parser died/lost)
    void markDisconnected(
        const UUID& connection_id,
        DisconnectReason reason
    );
    
    // Perform triage on disconnected connection
    TriageResult performTriage(const UUID& connection_id);
    
    // Clean up expired connections
    void cleanupExpiredConnections();
};
```

### Configuration

```ini
[connection_recovery]
enabled = true
reconnect_timeout = 5m
max_reconnect_attempts = 3
allow_ip_change = false
require_session_token = true

[triage]
auto_rollback_on_timeout = true
keep_read_only_transactions = true
log_triage = true
```

---

## 6. Summary

| Aspect | Design |
|--------|--------|
| Server Role | Maintains persistent connection state |
| Parser Role | Translates protocol, disposable |
| Connection Identity | UUID + session token |
| Transaction State | Stored in server, survives disconnect |
| Recovery | New parser picks up existing context |
| Triage | Server analyzes, client decides |

### Benefits

1. **Fault Tolerance**: Parser crashes don't lose work
2. **Network Resilience**: Temporary disconnects are recoverable
3. **Transparency**: Client sees seamless continuation
4. **Safety**: Triage lets client decide on ambiguous states
5. **Scalability**: Parsers are stateless, easy to replace

---

## 7. Related Specifications

- SCRATCHBIRD_SECURITY_AND_ACCESS_MODEL.md
- ARCHITECTURE_CLARIFICATIONS.md
- SERVER_LIFECYCLE_AND_STARTUP_SPECIFICATION.md