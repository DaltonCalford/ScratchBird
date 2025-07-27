# ScratchBird API - Transaction Management

## Overview

The ScratchBird Transaction Management API provides comprehensive control over database transactions, including ACID compliance, isolation levels, savepoints, and distributed transactions. This covers both simple auto-commit scenarios and complex multi-phase transactions.

## Core Transaction Interface

### Primary Header Files
```cpp
#include <scratchbird/include/scratchbird.h>
#include <scratchbird/include/sbclient_pub.h>
#include <scratchbird/include/sb_transaction.h>
```

### Transaction Handle Structure

```cpp
typedef struct {
    void* tr_handle;
    SB_DB_HANDLE db_handle;
    int isolation_level;
    int lock_timeout;
    bool read_only;
    bool auto_commit;
    bool wait_for_locks;
    SB_TRANSACTION_STATE state;
    SB_ERROR_INFO last_error;
    void* savepoint_list;
} SB_TRANSACTION;
```

## Transaction Lifecycle

### sb_start_transaction()
Starts a new database transaction.

```cpp
SB_STATUS sb_start_transaction(
    SB_STATUS*      status_vector,
    SB_TR_HANDLE*   tr_handle,
    short           db_handle_count,
    SB_DB_HANDLE*   db_handles,
    short           tpb_length,
    const char*     tpb_buffer
);
```

**Parameters:**
- `tr_handle`: Output transaction handle
- `db_handle_count`: Number of databases in transaction
- `db_handles`: Array of database handles
- `tpb_length`: Transaction parameter buffer length
- `tpb_buffer`: Transaction parameter buffer

**Single Database Transaction:**
```cpp
SB_STATUS status[20];
SB_TR_HANDLE transaction = 0;
char tpb_buffer[256];
short tpb_length = 0;

// Build transaction parameter buffer
sb_expand_tpb(&tpb_buffer, &tpb_length,
    sb_tpb_version3,
    sb_tpb_write,                    // Read-write transaction
    sb_tpb_read_committed,           // Isolation level
    sb_tpb_rec_version,              // Read record versions
    sb_tpb_wait,                     // Wait for locks
    sb_tpb_lock_timeout, 30,         // 30 second lock timeout
    0);

if (sb_start_transaction(status, &transaction, 1, &db, 
                         tpb_length, tpb_buffer)) {
    sb_print_status(status);
    return;
}

printf("Transaction started successfully\n");
```

### sb_commit_transaction()
Commits a transaction and releases all locks.

```cpp
SB_STATUS sb_commit_transaction(
    SB_STATUS*      status_vector,
    SB_TR_HANDLE*   tr_handle
);
```

**Example:**
```cpp
SB_STATUS status[20];

if (sb_commit_transaction(status, &transaction)) {
    printf("Commit failed:\n");
    sb_print_status(status);
    // Transaction is still active, can retry or rollback
} else {
    printf("Transaction committed successfully\n");
    // Transaction handle is now invalid
}
```

### sb_rollback_transaction()
Rolls back a transaction and undoes all changes.

```cpp
SB_STATUS sb_rollback_transaction(
    SB_STATUS*      status_vector,
    SB_TR_HANDLE*   tr_handle
);
```

**Example:**
```cpp
SB_STATUS status[20];

if (sb_rollback_transaction(status, &transaction)) {
    printf("Rollback failed:\n");
    sb_print_status(status);
} else {
    printf("Transaction rolled back successfully\n");
}
```

### sb_commit_retaining()
Commits changes but keeps the transaction active.

```cpp
SB_STATUS sb_commit_retaining(
    SB_STATUS*      status_vector,
    SB_TR_HANDLE    tr_handle
);
```

**Example - Batch Processing:**
```cpp
SB_STATUS status[20];
int batch_size = 1000;
int processed = 0;

for (int i = 0; i < total_records; i++) {
    // Process record
    process_record(db, transaction, &records[i]);
    processed++;
    
    // Commit every 1000 records but keep transaction active
    if (processed % batch_size == 0) {
        if (sb_commit_retaining(status, transaction)) {
            sb_print_status(status);
            break;
        }
        printf("Committed batch of %d records\n", batch_size);
    }
}

// Final commit
sb_commit_transaction(status, &transaction);
```

### sb_rollback_retaining()
Rolls back to the last commit point but keeps the transaction active.

```cpp
SB_STATUS sb_rollback_retaining(
    SB_STATUS*      status_vector,
    SB_TR_HANDLE    tr_handle
);
```

## Transaction Parameter Buffer (TPB)

### Core TPB Parameters

```cpp
// Transaction access mode
sb_tpb_read                 // Read-only transaction
sb_tpb_write                // Read-write transaction

// Isolation levels
sb_tpb_consistency          // SERIALIZABLE isolation
sb_tpb_concurrency          // REPEATABLE READ
sb_tpb_read_committed       // READ COMMITTED
sb_tpb_read_uncommitted     // READ UNCOMMITTED (ScratchBird extension)

// Lock resolution
sb_tpb_wait                 // Wait for locks
sb_tpb_nowait               // Don't wait for locks
sb_tpb_lock_timeout         // Lock timeout in seconds

// Record version handling
sb_tpb_rec_version          // Read record versions
sb_tpb_no_rec_version       // Read latest committed versions

// ScratchBird-specific parameters
sb_tpb_read_consistency     // Consistent read timestamps
sb_tpb_snapshot_at          // Start at specific timestamp
sb_tpb_ignore_limbo         // Ignore limbo transactions
sb_tpb_auto_commit          // Auto-commit mode
sb_tpb_no_auto_undo         // Disable automatic undo
```

### TPB Builder Helper Function

```cpp
void sb_expand_tpb(char** tpb, short* tpb_length, ...);
```

**Advanced Transaction Configuration:**
```cpp
char tpb_buffer[512];
short tpb_length = 0;
int lock_timeout = 60;
SB_TIMESTAMP snapshot_time;

// Get current timestamp for snapshot
sb_get_current_timestamp(&snapshot_time);

sb_expand_tpb(&tpb_buffer, &tpb_length,
    sb_tpb_version3,
    sb_tpb_write,
    sb_tpb_read_committed,
    sb_tpb_rec_version,
    sb_tpb_wait,
    sb_tpb_lock_timeout, &lock_timeout,
    sb_tpb_read_consistency,         // ScratchBird enhancement
    sb_tpb_snapshot_at, &snapshot_time,
    0);
```

## Isolation Levels

### SERIALIZABLE (sb_tpb_consistency)
Highest isolation level with complete transaction isolation.

```cpp
char tpb[] = {
    sb_tpb_version3,
    sb_tpb_write,
    sb_tpb_consistency,
    sb_tpb_wait
};

sb_start_transaction(status, &transaction, 1, &db, 
                     sizeof(tpb), tpb);
```

**Characteristics:**
- Complete isolation from other transactions
- Serializable execution order guaranteed
- May cause deadlocks in high-concurrency scenarios
- Best for critical financial operations

### REPEATABLE READ (sb_tpb_concurrency)
Prevents dirty reads and non-repeatable reads.

```cpp
char tpb[] = {
    sb_tpb_version3,
    sb_tpb_write,
    sb_tpb_concurrency,
    sb_tpb_rec_version,
    sb_tpb_wait
};
```

**Characteristics:**
- Sees consistent snapshot of data
- Other transactions can insert new records (phantom reads possible)
- Good balance of consistency and concurrency

### READ COMMITTED (sb_tpb_read_committed)
Prevents dirty reads but allows non-repeatable reads.

```cpp
char tpb[] = {
    sb_tpb_version3,
    sb_tpb_write,
    sb_tpb_read_committed,
    sb_tpb_rec_version,
    sb_tpb_wait
};
```

**Characteristics:**
- Sees committed changes from other transactions
- Each statement sees fresh committed data
- Most commonly used isolation level

### READ UNCOMMITTED (sb_tpb_read_uncommitted)
Lowest isolation level allowing dirty reads (ScratchBird extension).

```cpp
char tpb[] = {
    sb_tpb_version3,
    sb_tpb_read,                    // Usually read-only
    sb_tpb_read_uncommitted,
    sb_tpb_no_rec_version,
    sb_tpb_nowait
};
```

**Characteristics:**
- Can see uncommitted changes from other transactions
- Fastest performance but lowest consistency
- Useful for reporting and analytics

## Savepoints

### sb_transaction_savepoint()
Creates a named savepoint within a transaction.

```cpp
SB_STATUS sb_transaction_savepoint(
    SB_STATUS*      status_vector,
    SB_TR_HANDLE    tr_handle,
    const char*     savepoint_name
);
```

### sb_rollback_to_savepoint()
Rolls back to a specific savepoint.

```cpp
SB_STATUS sb_rollback_to_savepoint(
    SB_STATUS*      status_vector,
    SB_TR_HANDLE    tr_handle,
    const char*     savepoint_name
);
```

### sb_release_savepoint()
Releases a savepoint and its resources.

```cpp
SB_STATUS sb_release_savepoint(
    SB_STATUS*      status_vector,
    SB_TR_HANDLE    tr_handle,
    const char*     savepoint_name
);
```

**Savepoint Example:**
```cpp
SB_STATUS status[20];

// Start transaction
sb_start_transaction(status, &transaction, 1, &db, tpb_length, tpb_buffer);

try {
    // Perform some operations
    insert_customer(db, transaction, "ACME Corp");
    
    // Create savepoint before risky operation
    if (sb_transaction_savepoint(status, transaction, "before_bulk_insert")) {
        sb_print_status(status);
        goto rollback;
    }
    
    // Attempt bulk insert operation
    int result = bulk_insert_orders(db, transaction, order_data, 10000);
    
    if (result != 0) {
        // Bulk insert failed, rollback to savepoint
        printf("Bulk insert failed, rolling back to savepoint\n");
        
        if (sb_rollback_to_savepoint(status, transaction, "before_bulk_insert")) {
            sb_print_status(status);
            goto rollback;
        }
        
        // Continue with single inserts
        single_insert_orders(db, transaction, order_data, 10000);
    }
    
    // Release savepoint if no longer needed
    sb_release_savepoint(status, transaction, "before_bulk_insert");
    
    // Commit entire transaction
    sb_commit_transaction(status, &transaction);
    printf("Transaction completed successfully\n");
    
} catch (...) {
rollback:
    sb_rollback_transaction(status, &transaction);
    printf("Transaction rolled back due to error\n");
}
```

## Multi-Database Transactions

### Distributed Transaction Setup

```cpp
SB_STATUS sb_start_multiple(
    SB_STATUS*      status_vector,
    SB_TR_HANDLE*   tr_handle,
    short           db_handle_count,
    SB_DB_HANDLE*   db_handles,
    short*          tpb_lengths,
    const char**    tpb_buffers
);
```

**Example - Cross-Database Transaction:**
```cpp
SB_STATUS status[20];
SB_TR_HANDLE distributed_tr = 0;
SB_DB_HANDLE databases[3] = {orders_db, inventory_db, accounting_db};

// Different TPB for each database
char orders_tpb[] = {sb_tpb_version3, sb_tpb_write, sb_tpb_read_committed, sb_tpb_wait};
char inventory_tpb[] = {sb_tpb_version3, sb_tpb_write, sb_tpb_consistency, sb_tpb_wait};
char accounting_tpb[] = {sb_tpb_version3, sb_tpb_write, sb_tpb_consistency, sb_tpb_wait};

const char* tpb_buffers[] = {orders_tpb, inventory_tpb, accounting_tpb};
short tpb_lengths[] = {sizeof(orders_tpb), sizeof(inventory_tpb), sizeof(accounting_tpb)};

if (sb_start_multiple(status, &distributed_tr, 3, databases, 
                      tpb_lengths, tpb_buffers)) {
    sb_print_status(status);
    return;
}

// Now all operations use the same transaction handle across databases
// Orders database
sb_dsql_execute_immediate(status, orders_db, distributed_tr, 0,
    "INSERT INTO orders (customer_id, order_date) VALUES (1001, CURRENT_DATE)", 4, NULL);

// Inventory database  
sb_dsql_execute_immediate(status, inventory_db, distributed_tr, 0,
    "UPDATE inventory SET quantity = quantity - 5 WHERE product_id = 'ABC123'", 4, NULL);

// Accounting database
sb_dsql_execute_immediate(status, accounting_db, distributed_tr, 0,
    "INSERT INTO transactions (account, amount) VALUES ('SALES', 299.99)", 4, NULL);

// Commit all databases atomically
if (sb_commit_transaction(status, &distributed_tr)) {
    printf("Distributed transaction commit failed\n");
    sb_rollback_transaction(status, &distributed_tr);
} else {
    printf("Distributed transaction committed successfully\n");
}
```

## Transaction Information

### sb_transaction_info()
Retrieves information about a running transaction.

```cpp
SB_STATUS sb_transaction_info(
    SB_STATUS*      status_vector,
    SB_TR_HANDLE    tr_handle,
    short           item_list_length,
    const char*     item_list,
    short           buffer_length,
    char*           buffer
);
```

**Information Items:**
```cpp
sb_info_tra_id              // Transaction ID
sb_info_tra_oldest_active   // Oldest active transaction
sb_info_tra_oldest_snapshot // Oldest snapshot transaction  
sb_info_tra_oldest_interesting // Oldest interesting transaction
sb_info_tra_isolation       // Isolation level
sb_info_tra_access          // Access mode (read/write)
sb_info_tra_lock_timeout    // Lock timeout setting
sb_info_tra_page_reads      // Page reads in transaction
sb_info_tra_page_writes     // Page writes in transaction
sb_info_tra_record_versions // Record version access mode
sb_info_tra_snapshot_number // Snapshot number
sb_info_tra_dbkey_scope     // Database key scope
```

**Example:**
```cpp
SB_STATUS status[20];
char info_buffer[512];
char request[] = {
    sb_info_tra_id,
    sb_info_tra_isolation,
    sb_info_tra_access,
    sb_info_tra_lock_timeout,
    sb_info_end
};

if (!sb_transaction_info(status, transaction, sizeof(request), request,
                        sizeof(info_buffer), info_buffer)) {
    // Parse transaction information
    sb_parse_transaction_info(info_buffer, sizeof(info_buffer));
}
```

## Lock Management

### Lock Timeout Configuration

```cpp
// Set lock timeout in TPB
int timeout_seconds = 120;
sb_expand_tpb(&tpb_buffer, &tpb_length,
    sb_tpb_version3,
    sb_tpb_write,
    sb_tpb_read_committed,
    sb_tpb_wait,
    sb_tpb_lock_timeout, &timeout_seconds,
    0);
```

### Lock Resolution Strategies

**Wait for Locks (sb_tpb_wait):**
```cpp
char tpb[] = {
    sb_tpb_version3,
    sb_tpb_write,
    sb_tpb_read_committed,
    sb_tpb_wait,                    // Wait for locks
    sb_tpb_lock_timeout, 30         // Up to 30 seconds
};
```

**No Wait (sb_tpb_nowait):**
```cpp
char tpb[] = {
    sb_tpb_version3,
    sb_tpb_write,
    sb_tpb_read_committed,
    sb_tpb_nowait                   // Fail immediately on conflict
};
```

## Auto-Commit Mode

### sb_set_autocommit()
Enables or disables auto-commit mode for a connection.

```cpp
SB_STATUS sb_set_autocommit(
    SB_STATUS*      status_vector,
    SB_DB_HANDLE    db_handle,
    bool            auto_commit
);
```

**Example:**
```cpp
SB_STATUS status[20];

// Enable auto-commit
if (sb_set_autocommit(status, db, true)) {
    sb_print_status(status);
    return;
}

// Now each statement automatically commits
sb_dsql_execute_immediate(status, db, 0, 0,
    "INSERT INTO log_entries (message, timestamp) "
    "VALUES ('System started', CURRENT_TIMESTAMP)", 4, NULL);
// Automatically committed

// Disable auto-commit for manual transaction control
sb_set_autocommit(status, db, false);
```

## Transaction Monitoring

### Active Transaction Detection

```cpp
// Check if transaction is active
bool sb_transaction_is_active(SB_TR_HANDLE tr_handle);

// Get transaction state
SB_TRANSACTION_STATE sb_get_transaction_state(SB_TR_HANDLE tr_handle);
```

**Transaction States:**
```cpp
typedef enum {
    SB_TR_STATE_INACTIVE,       // Transaction not started
    SB_TR_STATE_ACTIVE,         // Transaction active
    SB_TR_STATE_COMMITTED,      // Transaction committed
    SB_TR_STATE_ROLLED_BACK,    // Transaction rolled back
    SB_TR_STATE_PREPARING,      // Preparing for two-phase commit
    SB_TR_STATE_PREPARED,       // Prepared for commit
    SB_TR_STATE_LIMBO          // In limbo state
} SB_TRANSACTION_STATE;
```

### Transaction Statistics

```cpp
typedef struct {
    SB_INT64 transaction_id;
    SB_INT64 page_reads;
    SB_INT64 page_writes;
    SB_INT64 record_fetches;
    SB_INT64 record_marks;
    SB_INT64 record_deletes;
    SB_INT64 record_backouts;
    SB_INT64 record_purges;
    SB_INT64 record_expunges;
    int lock_timeout_count;
    int deadlock_count;
    SB_TIMESTAMP start_time;
    SB_TIMESTAMP last_activity;
} SB_TRANSACTION_STATS;

SB_STATUS sb_get_transaction_stats(
    SB_STATUS*              status_vector,
    SB_TR_HANDLE           tr_handle,
    SB_TRANSACTION_STATS*  stats
);
```

## Error Handling

### Transaction Error Codes

```cpp
#define sb_deadlock                 335544336L
#define sb_lock_timeout            335544337L
#define sb_lock_conflict           335544338L
#define sb_no_transaction          339544339L
#define sb_transaction_in_use      335544340L
#define sb_transaction_invalid     335544341L
#define sb_savepoint_not_found     335544342L
#define sb_savepoint_invalid       335544343L
#define sb_two_phase_timeout       335544344L
#define sb_transaction_readonly    335544345L
#define sb_distributed_tx_error    335544346L
```

### Deadlock Handling

```cpp
int handle_deadlock_retry(SB_DB_HANDLE db, 
                         int (*operation)(SB_DB_HANDLE, SB_TR_HANDLE),
                         int max_retries) {
    SB_STATUS status[20];
    SB_TR_HANDLE transaction = 0;
    
    for (int attempt = 0; attempt < max_retries; attempt++) {
        // Start fresh transaction
        sb_start_transaction(status, &transaction, 1, &db, 
                           tpb_length, tpb_buffer);
        
        int result = operation(db, transaction);
        
        if (result == 0) {
            // Success - commit and return
            if (sb_commit_transaction(status, &transaction) == 0) {
                return 0;
            }
        }
        
        // Check if it was a deadlock
        if (status[1] == sb_deadlock) {
            printf("Deadlock detected, attempt %d of %d\n", 
                   attempt + 1, max_retries);
            
            // Rollback and wait before retry
            sb_rollback_transaction(status, &transaction);
            
            // Exponential backoff
            usleep((1 << attempt) * 100000); // 100ms, 200ms, 400ms, etc.
            continue;
        } else {
            // Other error - don't retry
            sb_rollback_transaction(status, &transaction);
            return -1;
        }
    }
    
    printf("Operation failed after %d deadlock retries\n", max_retries);
    return -1;
}
```

## Complete Transaction Example

```cpp
#include <scratchbird/include/scratchbird.h>
#include <stdio.h>
#include <stdlib.h>

int transfer_funds(SB_DB_HANDLE db, int from_account, 
                  int to_account, double amount) {
    SB_STATUS status[20];
    SB_TR_HANDLE transaction = 0;
    SB_STMT_HANDLE stmt1 = 0, stmt2 = 0, stmt3 = 0;
    
    // Configure transaction for financial operations
    char tpb[] = {
        sb_tpb_version3,
        sb_tpb_write,
        sb_tpb_consistency,         // Serializable isolation
        sb_tpb_wait,
        sb_tpb_lock_timeout, 30     // 30 second timeout
    };
    
    // Start transaction
    if (sb_start_transaction(status, &transaction, 1, &db, 
                           sizeof(tpb), tpb)) {
        printf("Failed to start transaction:\n");
        sb_print_status(status);
        return -1;
    }
    
    try {
        // Create savepoint before operations
        if (sb_transaction_savepoint(status, transaction, "funds_transfer")) {
            throw "Savepoint creation failed";
        }
        
        // Allocate statements
        sb_dsql_allocate_statement(status, db, &stmt1);
        sb_dsql_allocate_statement(status, db, &stmt2);
        sb_dsql_allocate_statement(status, db, &stmt3);
        
        // Check source account balance
        char check_sql[] = "SELECT balance FROM accounts WHERE account_id = ?";
        // ... prepare and execute balance check ...
        
        double current_balance = 0.0;
        // ... fetch current balance ...
        
        if (current_balance < amount) {
            printf("Insufficient funds: %.2f < %.2f\n", current_balance, amount);
            throw "Insufficient funds";
        }
        
        // Debit source account
        char debit_sql[] = "UPDATE accounts SET balance = balance - ? "
                          "WHERE account_id = ?";
        // ... prepare and execute debit ...
        
        // Credit target account
        char credit_sql[] = "UPDATE accounts SET balance = balance + ? "
                           "WHERE account_id = ?";
        // ... prepare and execute credit ...
        
        // Create audit log entry
        char log_sql[] = "INSERT INTO transaction_log "
                        "(from_account, to_account, amount, timestamp) "
                        "VALUES (?, ?, ?, CURRENT_TIMESTAMP)";
        // ... prepare and execute audit log ...
        
        // Commit transaction
        if (sb_commit_transaction(status, &transaction)) {
            printf("Commit failed:\n");
            sb_print_status(status);
            return -1;
        }
        
        printf("Transfer completed: $%.2f from account %d to account %d\n",
               amount, from_account, to_account);
        return 0;
        
    } catch (const char* error) {
        printf("Transfer failed: %s\n", error);
        
        // Rollback to savepoint or entire transaction
        if (sb_rollback_to_savepoint(status, transaction, "funds_transfer")) {
            sb_rollback_transaction(status, &transaction);
        } else {
            sb_rollback_transaction(status, &transaction);
        }
        
        return -1;
    }
    
    // Cleanup statements
    if (stmt1) sb_dsql_free_statement(status, &stmt1, SB_DROP);
    if (stmt2) sb_dsql_free_statement(status, &stmt2, SB_DROP);
    if (stmt3) sb_dsql_free_statement(status, &stmt3, SB_DROP);
    
    return 0;
}
```

## Performance Optimization

### Transaction Batching

```cpp
int process_batch_operations(SB_DB_HANDLE db, int batch_size) {
    SB_STATUS status[20];
    SB_TR_HANDLE transaction = 0;
    int processed = 0;
    
    // Use read-committed for better concurrency
    char tpb[] = {
        sb_tpb_version3,
        sb_tpb_write,
        sb_tpb_read_committed,
        sb_tpb_rec_version,
        sb_tpb_wait,
        sb_tpb_lock_timeout, 10
    };
    
    sb_start_transaction(status, &transaction, 1, &db, sizeof(tpb), tpb);
    
    for (int i = 0; i < total_operations; i++) {
        // Process operation
        process_operation(db, transaction, &operations[i]);
        processed++;
        
        // Commit retaining every batch_size operations
        if (processed % batch_size == 0) {
            if (sb_commit_retaining(status, transaction)) {
                sb_print_status(status);
                break;
            }
        }
    }
    
    // Final commit
    sb_commit_transaction(status, &transaction);
    return processed;
}
```

## Implementation Files

### Core Transaction Management
- `src/jrd/tra.cpp` - Transaction lifecycle management
- `src/jrd/TxnManager.cpp` - Transaction manager implementation
- `src/jrd/tra.h` - Transaction handle structures
- `src/common/classes/TempSpace.cpp` - Transaction temporary space

### Lock Management
- `src/jrd/lck.cpp` - Lock manager implementation
- `src/jrd/lck.h` - Lock structures and definitions
- `src/jrd/deadlock.cpp` - Deadlock detection and resolution

### Savepoint Implementation
- `src/jrd/Savepoint.cpp` - Savepoint management
- `src/jrd/VerbAction.cpp` - Transaction verb processing
- `src/jrd/undo.cpp` - Transaction undo operations

---

*This documentation covers the complete ScratchBird transaction management API. See related API documentation for connection management, statement execution, and error handling.*