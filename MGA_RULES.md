# FIREBIRD MGA IMPLEMENTATION RULES

**CRITICAL**: Read this file BEFORE implementing ANY transaction-related or index-related code.

**Purpose**: This file defines the absolute rules for Firebird's Multi-Generational Architecture (MGA). Any deviation from these rules means you are implementing PostgreSQL MVCC, not Firebird MGA.

---

## Rule 0: The Fundamental Distinction

**ScratchBird uses Firebird MGA, NOT PostgreSQL MVCC.**

These are **completely different architectures** that are often confused because both are referred to as "MVCC".

---

## Rule 1: NO SNAPSHOTS

### ❌ FORBIDDEN (PostgreSQL MVCC):
```cpp
// WRONG - PostgreSQL MVCC
struct Snapshot {
    TransactionId xmin;
    TransactionId xmax;
    TransactionId *active_xids;
    uint32_t xcnt;
    bool takenDuringRecovery;
};

bool isSnapshotVisible(TransactionId xid, const Snapshot* snapshot);
Status getSnapshot(Snapshot& snapshot, ErrorContext* ctx);
```

### ✅ REQUIRED (Firebird MGA):
```cpp
// CORRECT - Firebird MGA
enum TxState {
    TX_ACTIVE = 0,      // 00 - Transaction in progress
    TX_COMMITTED = 1,   // 01 - Transaction committed
    TX_ABORTED = 2,     // 10 - Transaction rolled back
    TX_LIMBO = 3        // 11 - Prepared (2PC)
};

TxState getTransactionState(TransactionId xid);  // Looks up TIP
bool isVersionVisible(TransactionId version_xid, TransactionId reader_xid);
```

**If you see `Snapshot` anywhere in transaction-related code, it's WRONG.**

---

## Rule 2: Transaction Inventory Pages (TIP) Required

### What is TIP?

TIP is a **bitmap** that stores 2 bits per transaction, representing the transaction's state.

```c
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

**TIP Capacity by Page Size**:
- 8KB page: ~32,000 transactions per TIP page
- 16KB page: ~65,000 transactions per TIP page
- 32KB page: ~130,000 transactions per TIP page

### How TIP is Used

```cpp
// Get transaction state from TIP
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

**If TIP lookups are not present, the implementation is WRONG.**

---

## Rule 3: Visibility Check Uses TIP, Not Snapshots

### ❌ WRONG (PostgreSQL MVCC):
```cpp
bool is_visible(TransactionId xid, const Snapshot* snapshot) {
    // Check if xid is in snapshot's active transaction array
    if (xid >= snapshot->xmin && xid < snapshot->xmax) {
        for (int i = 0; i < snapshot->xcnt; i++) {
            if (snapshot->active_xids[i] == xid) {
                return false;  // Active in snapshot
            }
        }
    }
    return true;
}
```

### ✅ CORRECT (Firebird MGA):
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

**Key Difference**:
- PostgreSQL: "Is this XID in the snapshot's active list?"
- Firebird: "Is this XID committed and older than me?"

---

## Rule 4: Transaction Markers (OIT/OAT/OST) Required

### What are Transaction Markers?

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

**Definitions**:
- **OIT**: Oldest transaction NOT in committed state
- **OAT**: Oldest transaction in active state
- **OST**: Oldest transaction with SNAPSHOT isolation

### Sweep Trigger Formula

```cpp
// CORRECT: Firebird MGA sweep trigger
if ((OST - OIT) > sweep_interval) {
    trigger_sweep();
}
```

**NOT** `(Next - OIT)` or `(OAT - OIT)`

**If OIT/OAT/OST are not present, the implementation is WRONG.**

---

## Rule 5: Back-Versioning, NOT Forward-Versioning

### ❌ WRONG (PostgreSQL MVCC - Forward-Versioning):
```
UPDATE record at (Page 5, Offset 100):

Old Tuple (Page 5, Offset 100):
  t_xmin: 50
  t_xmax: 100              ← Marked deleted
  t_ctid: (8, 200)         ← Points FORWARD to new tuple
  data: salary=50000

New Tuple (Page 8, Offset 200):  ← DIFFERENT LOCATION
  t_xmin: 100
  t_xmax: 0
  t_ctid: (8, 200)
  data: salary=60000

ALL INDEXES MUST BE UPDATED → Index bloat!
```

### ✅ CORRECT (Firebird MGA - Back-Versioning):
```
UPDATE record at (Page 5, Line 3):

Primary Record (Page 5, Line 3):  ← SAME LOCATION
  rhd_transaction: 100     ← New transaction
  rhd_b_page: 7            ← Points BACKWARD to old version
  rhd_b_line: 12
  rhd_data: salary=60000   ← NEW DATA

Back Version (Page 7, Line 12):  ← OLD DATA preserved
  rhd_transaction: 50
  rhd_b_page: 0
  rhd_b_line: 0
  rhd_data: salary=50000

INDEXES NEVER CHANGE → No index bloat!
```

**Key Rules**:
1. Primary record is modified **in-place**
2. Old data moved to **back version**
3. Primary points **backward** to old version
4. Index entries **never change** (unless indexed column modified)

---

## Rule 6: In-Place Updates with Stable TIDs

### Record Header Structure

```c
struct rhd {  // Record Header
    uint32_t rhd_transaction;  // Transaction ID that created this version
    uint32_t rhd_b_page;       // Back version page number (0 = no back version)
    uint16_t rhd_b_line;       // Back version line number
    uint16_t rhd_flags;        // Record flags
    uint8_t  rhd_format;       // Format version
    uint8_t  rhd_data[];       // Actual data (RLE compressed)
};
```

### Flags

```c
#define rhd_deleted    0x01  // Logically deleted
#define rhd_chain      0x02  // Has back version
#define rhd_fragment   0x04  // Multi-fragment record
#define rhd_incomplete 0x08  // First fragment
#define rhd_delta      0x10  // Delta-compressed back version
#define rhd_gc_active  0x20  // Being garbage collected
#define rhd_damaged    0x40  // Corrupted
```

### Update Algorithm

```cpp
// CORRECT: Firebird MGA update
void update_record(TID primary_tid, const RecordData* new_data, TransactionId xid) {
    // 1. Fetch current record
    Record *current = fetch_record(primary_tid);

    // 2. Create back version (old data)
    TID back_tid = allocate_record_space();
    copy_record_data(back_tid, current->data);
    set_back_version_xid(back_tid, current->rhd_transaction);

    // 3. Modify primary record IN-PLACE
    overwrite_record_data(primary_tid, new_data);
    current->rhd_transaction = xid;
    current->rhd_b_page = back_tid.page;
    current->rhd_b_line = back_tid.line;
    current->rhd_flags |= rhd_chain;

    // 4. Indexes NEVER CHANGE (still point to primary_tid)
}
```

**If new tuples are created at new locations, it's WRONG.**

---

## Rule 7: Newest-to-Oldest (N2O) Version Chains

### Chain Traversal

```cpp
// CORRECT: Firebird MGA version traversal
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

**Chain direction: Newest → Older → Oldest**

**If chains go Oldest → Newest, it's WRONG (PostgreSQL O2N).**

---

## Rule 8: Index Behavior

### When Indexes Are Updated

```cpp
// CORRECT: Firebird MGA index update logic
void update_indexes_on_update(Table *table, TID primary_tid,
                              const Record *old_data, const Record *new_data,
                              TransactionId xid) {
    for (Index *idx : table->indexes) {
        // Check if ANY indexed column changed
        bool indexed_col_changed = false;
        for (uint32_t col : idx->columns) {
            if (old_data->columns[col] != new_data->columns[col]) {
                indexed_col_changed = true;
                break;
            }
        }

        if (indexed_col_changed) {
            // Only NOW update index
            idx->remove(old_key, primary_tid, xid);
            idx->insert(new_key, primary_tid, xid);
        }
        // Otherwise, index entry UNCHANGED
    }
}
```

**Index entries store stable TIDs that never change unless indexed column modified.**

**If indexes are updated on every UPDATE, it's WRONG.**

---

## Rule 9: No Index Bloat

### Space Efficiency

In PostgreSQL MVCC:
```
UPDATE salary WHERE id = 1;  -- 100 times

Result:
- 100 tuples in heap (old versions + new version)
- 100 index entries pointing to different tuple locations
- Massive index bloat
- VACUUM required frequently
```

In Firebird MGA:
```
UPDATE salary WHERE id = 1;  -- 100 times

Result:
- 1 primary record at stable location
- 100 back versions (may be delta-compressed)
- 1 index entry (never changed)
- Minimal index growth
- Sweep only needed when version chains get long
```

**If index size grows proportionally to UPDATE count, it's WRONG.**

---

## Rule 10: Garbage Collection via Sweep

### Sweep Process

```cpp
// CORRECT: Firebird MGA sweep
void sweep_table(Table *table, TransactionId oit) {
    for (TID primary_tid : table->all_records()) {
        Record *primary = fetch_record(primary_tid);
        TID back_tid = TID(primary->rhd_b_page, primary->rhd_b_line);

        // Traverse back chain
        while (!is_null(back_tid)) {
            Record *back = fetch_record(back_tid);

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

**If VACUUM removes tuples from heap, it's WRONG (PostgreSQL).**

---

## Rule 11: API Signatures

### ❌ FORBIDDEN Signatures (PostgreSQL MVCC):

```cpp
// WRONG
Status search(const Key& key, Snapshot* snapshot, std::vector<TID>* tids, ErrorContext* ctx);
bool isEntryVisible(uint64_t xmin, uint64_t xmax, Snapshot* snapshot);
Status rangeScan(const Key* start, const Key* end, Snapshot* snapshot, ...);
```

### ✅ REQUIRED Signatures (Firebird MGA):

```cpp
// CORRECT
Status search(const Key& key, TransactionId current_xid, std::vector<TID>* tids, ErrorContext* ctx);
bool isEntryVisible(uint64_t xmin, uint64_t xmax, TransactionId current_xid);
Status rangeScan(const Key* start, const Key* end, TransactionId current_xid, ...);
```

**Parameters**:
- ❌ `Snapshot* snapshot` - WRONG
- ✅ `TransactionId current_xid` - CORRECT

---

## Rule 12: TransactionManager API

### ❌ FORBIDDEN API (PostgreSQL MVCC):

```cpp
class TransactionManager {
    struct Snapshot { ... };  // NO!

    Status getSnapshot(Snapshot& snapshot, ErrorContext* ctx);
    bool isSnapshotVisible(TransactionId xid, const Snapshot* snapshot);
};
```

### ✅ REQUIRED API (Firebird MGA):

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

---

## Rule 13: Detection Checklist

### MVCC Contamination Indicators

If you see ANY of these, the code is WRONG:

- ❌ `Snapshot` structure
- ❌ `snapshot` parameter names
- ❌ `isSnapshotVisible()` function calls
- ❌ `xmin`, `xmax` as snapshot markers (instead of transaction IDs)
- ❌ `active_xids[]` array
- ❌ Forward pointers (old → new)
- ❌ Tuples created at new locations
- ❌ Index TID updates on every UPDATE
- ❌ "Append-only" or "heap-only tuple" terminology

### MGA Compliance Indicators

If you see ALL of these, the code is CORRECT:

- ✅ TIP (Transaction Inventory Page) implementation
- ✅ `getTransactionState(xid)` function calls
- ✅ `TxState` enum (TX_COMMITTED, TX_ACTIVE, TX_ABORTED)
- ✅ OIT/OAT/OST markers
- ✅ Back pointers (new → old)
- ✅ In-place updates
- ✅ Stable TIDs
- ✅ "Back version" terminology

---

## Rule 14: Specification References

**ALWAYS consult these documents BEFORE implementing transaction/index code**:

1. `/docs/specifications/MGA_IMPLEMENTATION.md`
   - Complete MGA architecture
   - TIP structure and usage
   - Back-versioning details

2. `/docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md`
   - Transaction markers (OIT/OAT/OST)
   - Isolation levels
   - Sweep mechanism

3. `/docs/analysis/CRITICAL_MGA_MVCC_CONFUSION_ANALYSIS.md`
   - Detailed comparison of MGA vs MVCC
   - Common mistakes and how to avoid them

---

## Rule 15: When In Doubt

If you're unsure whether to implement something the "PostgreSQL way" or "Firebird way":

**ASK THESE QUESTIONS**:

1. Does this involve snapshots? → If YES, it's WRONG
2. Does this use TIP lookups? → If NO, it's WRONG
3. Are TIDs stable? → If NO, it's WRONG
4. Are indexes updated on every UPDATE? → If YES, it's WRONG
5. Do version chains go newest-to-oldest? → If NO, it's WRONG

**DEFAULT RULE**: When in doubt, assume Firebird MGA is correct and PostgreSQL MVCC is wrong.

---

## Summary: The Core Principle

**PostgreSQL MVCC**: "Create a snapshot of active transactions and check if each XID is in it"

**Firebird MGA**: "Look up each transaction's state in TIP and check if it's committed and older than me"

**ONE uses snapshots, ONE uses TIP. ScratchBird uses TIP (Firebird MGA).**

---

**If this file's rules are violated, the implementation is architecturally incorrect and must be rewritten.**

**No exceptions. No mixing. Pure Firebird MGA only.**
