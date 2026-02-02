# Transactions (MGA)

**Purpose:** Documents ScratchBird's transaction system using Firebird-style Multi-Generational Architecture (MGA) - NOT PostgreSQL MVCC.

**Last Updated:** 2026-01-30

---

## The Fundamental Distinction

**ScratchBird uses Firebird MGA, NOT PostgreSQL MVCC.**

These are **completely different architectures** that are often confused because both are referred to as "MVCC". If you implement PostgreSQL-style snapshots, **you are implementing the wrong system**.

| Aspect | PostgreSQL MVCC | Firebird MGA (ScratchBird) |
|--------|-----------------|---------------------------|
| Visibility check | Snapshot of active transactions | TIP lookup for transaction state |
| Version chains | Oldest → Newest (forward) | Newest → Oldest (back-versioning) |
| Primary record | Old data, points to new | New data, points to old |
| Index updates | Every UPDATE | Only when indexed column changes |
| Garbage collection | VACUUM removes heap tuples | Sweep removes back versions |

---

## Transaction Inventory Pages (TIP)

TIP is a **bitmap** that stores 2 bits per transaction, representing the transaction's state.

### Transaction States

```cpp
enum TxState {
    TX_ACTIVE = 0,      // 00 - Transaction in progress
    TX_COMMITTED = 1,   // 01 - Transaction committed
    TX_ABORTED = 2,     // 10 - Transaction rolled back
    TX_LIMBO = 3        // 11 - Prepared (2PC)
};
```

### TIP Structure

```cpp
struct SBTipPage {
    PageHeader      tip_header;            // Standard page header
    TransactionId   tip_min;               // Minimum transaction on page
    TransactionId   tip_max;               // Maximum transaction on page
    uint32_t        tip_next_page;         // Next TIP page
    uint32_t        tip_transactions_count;

    // 2 bits per transaction state (4 transactions per byte)
    uint8_t         tip_transactions[];
};
```

### TIP Capacity by Page Size

| Page Size | Transactions per TIP Page |
|-----------|---------------------------|
| 8KB | ~32,000 |
| 16KB | ~65,000 |
| 32KB | ~130,000 |

### Looking Up Transaction State

```cpp
TxState get_transaction_state(TransactionId xid) {
    // 1. Calculate which TIP page contains this transaction
    uint32_t page_num = xid / TRANSACTIONS_PER_TIP_PAGE;
    uint32_t offset = xid % TRANSACTIONS_PER_TIP_PAGE;

    // 2. Pin the TIP page
    SBTipPage *tip_page = pin_tip_page(page_num);

    // 3. Extract 2-bit state
    uint8_t byte = tip_page->tip_transactions[offset / 4];
    uint8_t shift = (offset % 4) * 2;
    TxState state = (TxState)((byte >> shift) & 0x03);

    // 4. Unpin page
    unpin_tip_page(page_num);

    return state;
}
```

---

## Visibility Rules

### Visibility Check (CORRECT - MGA)

```cpp
bool is_visible(TransactionId version_xid, TransactionId reader_xid) {
    // Own changes always visible
    if (version_xid == reader_xid) {
        return true;
    }

    // Look up transaction state in TIP
    TxState state = get_transaction_state(version_xid);

    // Only committed transactions older than reader are visible
    if (state == TX_COMMITTED && version_xid < reader_xid) {
        return true;
    }

    // Active or aborted = not visible
    return false;
}
```

### What NOT to do (PostgreSQL MVCC)

```cpp
// WRONG - DO NOT IMPLEMENT THIS
bool is_visible_WRONG(TransactionId xid, const Snapshot* snapshot) {
    // This is PostgreSQL MVCC - NOT Firebird MGA
    if (xid >= snapshot->xmin && xid < snapshot->xmax) {
        for (int i = 0; i < snapshot->xcnt; i++) {
            if (snapshot->active_xids[i] == xid) {
                return false;
            }
        }
    }
    return true;
}
```

**Key Difference:**
- PostgreSQL: "Is this XID in the snapshot's active list?"
- Firebird MGA: "Is this XID committed and older than me?"

---

## Transaction Markers

Stored in the database header, these markers are critical for garbage collection:

```cpp
struct DatabaseHeader {
    TransactionId next_transaction;    // Next TID to assign
    TransactionId oldest_transaction;  // OIT - Oldest Interesting Transaction
    TransactionId oldest_active;       // OAT - Oldest Active Transaction
    TransactionId oldest_snapshot;     // OST - Oldest Snapshot Transaction
    uint32_t sweep_interval;           // Sweep trigger threshold
};
```

### Definitions

| Marker | Name | Description |
|--------|------|-------------|
| OIT | Oldest Interesting Transaction | Oldest transaction NOT in committed state |
| OAT | Oldest Active Transaction | Oldest transaction in active state |
| OST | Oldest Snapshot Transaction | Oldest transaction with SNAPSHOT isolation |
| Next | Next Transaction | Next TID to be assigned |

### Sweep Trigger Formula

```cpp
// CORRECT: Firebird MGA sweep trigger
if ((OST - OIT) > sweep_interval) {
    trigger_sweep();
}
```

**NOT** `(Next - OIT)` or `(OAT - OIT)`

---

## Version Chains (N2O)

ScratchBird uses **Newest-to-Oldest (N2O)** version chains with back-versioning.

### Chain Direction

```
PRIMARY RECORD (newest data)
    │
    │ rhd_b_page, rhd_b_line
    ▼
BACK VERSION 1 (previous data)
    │
    │ rhd_b_page, rhd_b_line
    ▼
BACK VERSION 2 (older data)
    │
    ▼
... (chain continues)
```

### Finding Visible Version

```cpp
Record* find_visible_version(TID primary_tid, TransactionId reader_xid) {
    TID current_tid = primary_tid;

    while (!is_null(current_tid)) {
        // Fetch version
        Record *version = fetch_record(current_tid);

        // Check visibility
        if (is_visible(version->rhd_transaction, reader_xid)) {
            // Check if deleted
            if (version->rhd_flags & rhd_deleted) {
                return nullptr;  // Deleted
            }
            return version;  // Found visible version
        }

        // Follow back pointer
        current_tid = TID(version->rhd_b_page, version->rhd_b_line);
    }

    return nullptr;  // No visible version
}
```

---

## Transaction Lifecycle

### Begin Transaction

```cpp
TransactionId begin_transaction(IsolationLevel level, bool read_only) {
    // 1. Acquire lock on transaction counter
    // 2. Get next transaction ID
    TransactionId xid = next_transaction++;

    // 3. Mark as active in TIP
    set_transaction_state(xid, TX_ACTIVE);

    // 4. Update OAT if necessary
    if (xid < oldest_active) {
        oldest_active = xid;
    }

    return xid;
}
```

### Commit Transaction

```cpp
void commit_transaction(TransactionId xid) {
    // 1. Mark as committed in TIP
    set_transaction_state(xid, TX_COMMITTED);

    // 2. Update transaction markers
    update_transaction_markers();
}
```

### Rollback Transaction

```cpp
void rollback_transaction(TransactionId xid) {
    // 1. Mark as aborted in TIP
    set_transaction_state(xid, TX_ABORTED);

    // 2. Update transaction markers
    update_transaction_markers();
}
```

---

## Isolation Levels

ScratchBird supports standard isolation levels:

| Level | Description | MGA Behavior |
|-------|-------------|--------------|
| READ UNCOMMITTED | Not implemented (maps to READ COMMITTED) | N/A |
| READ COMMITTED | See committed data | Check TIP for each row |
| REPEATABLE READ | Consistent view | OST-based visibility |
| SERIALIZABLE | Strict isolation | OST + conflict detection |
| SNAPSHOT | Firebird-style | Record OST at start |

### Snapshot Isolation (Firebird-style)

```cpp
TransactionId begin_snapshot_transaction() {
    TransactionId xid = begin_transaction(SNAPSHOT, false);

    // Record OST for this transaction
    transaction_ost[xid] = oldest_snapshot;

    return xid;
}
```

---

## API Signatures

### CORRECT API (Firebird MGA)

```cpp
class TransactionManager {
    // NO Snapshot structure!

    TransactionId beginTransaction(IsolationLevel level, bool read_only);
    void commitTransaction(TransactionId xid);
    void rollbackTransaction(TransactionId xid);

    // TIP-based visibility
    TxState getTransactionState(TransactionId xid);  // Looks up TIP
    bool isVersionVisible(TransactionId version_xid, TransactionId reader_xid);

    // Transaction markers
    void getTransactionMarkers(TransactionId& oit, TransactionId& oat,
                               TransactionId& ost, TransactionId& next);
    void updateTransactionMarkers();
};
```

### FORBIDDEN API (PostgreSQL MVCC)

```cpp
// DO NOT IMPLEMENT
class TransactionManager_WRONG {
    struct Snapshot { ... };  // NO!

    Status getSnapshot(Snapshot& snapshot, ErrorContext* ctx);
    bool isSnapshotVisible(TransactionId xid, const Snapshot* snapshot);
};
```

---

## Garbage Collection (Sweep)

Sweep removes obsolete back versions that no transaction can see.

### Sweep Process

```cpp
void sweep_table(Table* table, TransactionId oit) {
    for (TID primary_tid : table->all_records()) {
        Record* primary = fetch_record(primary_tid);
        TID back_tid = TID(primary->rhd_b_page, primary->rhd_b_line);

        // Traverse back chain
        while (!is_null(back_tid)) {
            Record* back = fetch_record(back_tid);

            // If back version older than OIT, it's garbage
            if (back->rhd_transaction < oit) {
                TID next_back = TID(back->rhd_b_page, back->rhd_b_line);

                // Remove this back version
                free_record(back_tid);

                // Unlink from chain
                primary->rhd_b_page = next_back.page;
                primary->rhd_b_line = next_back.line;

                back_tid = next_back;
            } else {
                // Still visible to some transaction
                break;
            }
        }
    }
}
```

**Sweep removes old back versions, NOT old primary records.**

---

## Detection Checklist

### MVCC Contamination Indicators

If you see ANY of these in transaction-related code, it's WRONG:

- `Snapshot` structure
- `snapshot` parameter names
- `isSnapshotVisible()` function calls
- `xmin`, `xmax` as snapshot markers (instead of transaction IDs)
- `active_xids[]` array
- Forward pointers (old → new)
- Tuples created at new locations
- Index TID updates on every UPDATE
- "Append-only" or "heap-only tuple" terminology

### MGA Compliance Indicators

If you see ALL of these, the code is CORRECT:

- TIP (Transaction Inventory Page) implementation
- `getTransactionState(xid)` function calls
- `TxState` enum (TX_COMMITTED, TX_ACTIVE, TX_ABORTED)
- OIT/OAT/OST markers
- Back pointers (new → old)
- In-place updates
- Stable TIDs
- "Back version" terminology

---

## Summary

| Component | PostgreSQL MVCC | Firebird MGA (ScratchBird) |
|-----------|-----------------|---------------------------|
| Visibility method | Snapshot array lookup | TIP state lookup |
| Version direction | Oldest → Newest | Newest → Oldest |
| Primary location | Changes on UPDATE | Stable (never moves) |
| Index maintenance | Always update | Only if indexed col changes |
| GC target | Heap tuples | Back versions |
| Parameter type | `Snapshot*` | `TransactionId` |

**ONE uses snapshots, ONE uses TIP. ScratchBird uses TIP (Firebird MGA).**

---

## Source Code Reference

| Component | Implementation |
|-----------|----------------|
| Transaction Manager | `src/core/transaction_manager.cpp` |
| Commit Log (CLOG) | `src/core/clog.cpp` |
| TIP Compaction | `src/core/tip_compaction.cpp` |
| ProcArray (Active Txns) | `src/core/proc_array.cpp` |
| Sweep Manager | `src/core/sweep_manager.cpp` |
| Garbage Collector | `src/core/garbage_collector.cpp` |
| Vacuum | `src/core/vacuum.cpp` |
| Long Transaction Monitor | `src/core/long_transaction_monitor.cpp` |
| Lock Manager | `src/core/lock_manager.cpp` |

---

## Related Documents

- [Storage](Storage.md) - Back-versioning and heap page structure
- [Core Engine](Core-Engine.md) - Executor and visibility coordination
- [Architecture](Architecture.md) - Overall system design
