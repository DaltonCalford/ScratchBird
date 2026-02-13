# Issue #59: Transaction XID Validation Analysis

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Issue:** Transaction XID Not Validated Before Use (HIGH)
**Severity:** HIGH
**Status:** ANALYZING
**Date:** 2025-10-05
**Files Analyzed:** `src/core/transaction_manager.cpp`, `src/core/storage_engine.cpp`, `src/core/heap_page.cpp`

---

## Problem Description

From repair.md Issue #59:
> "TupleHeader has `xmin` and `xmax` fields, but they're never validated against TransactionManager state. When deserializing tuples, there's no check if the XIDs are still valid or have been vacuumed."

**Impact:** Tuple visibility corruption after XID wraparound

---

## Analysis of Current Implementation

### 1. XID Range and Reserved Values

**Defined in:** `include/scratchbird/core/transaction_manager.h:137-139`

```cpp
static constexpr uint64_t INVALID_XID = 0;
static constexpr uint64_t BOOTSTRAP_XID = 1;
static constexpr uint64_t FROZEN_XID = 2;
```

**Valid XID Range:** `>= 3` to `UINT64_MAX`

**First User XID:** `3` (FROZEN_XID + 1)

### 2. XID Allocation

**Location:** `src/core/transaction_manager.cpp:186-199`

```cpp
auto TransactionManager::beginTransaction(uint32_t proc_id, uint64_t &xid_out, ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Allocate new XID
    uint64_t new_xid = next_xid_++;

    // Prevent wraparound to reserved XIDs
    if (next_xid_ <= FROZEN_XID)
    {
        next_xid_ = FROZEN_XID + 1;
    }

    // Record transaction as active
    transaction_cache_[new_xid] = TransactionState::ACTIVE;
    // ...
}
```

**Analysis:**
- ✅ Prevents wraparound to reserved XIDs (0, 1, 2)
- ❌ No check for approaching UINT64_MAX
- ❌ No validation when reading XID from database header
- ❌ Wraparound check happens AFTER increment (next_xid_ could overflow)

### 3. Visibility Checks

#### 3.1 TransactionManager::isTransactionVisible()

**Location:** `src/core/transaction_manager.cpp:348-384`

```cpp
auto TransactionManager::isTransactionVisible(uint64_t xid, uint64_t snapshot_xid) -> bool
{
    // Simple visibility rules for single connection:
    // - Transaction sees its own changes
    // - Transaction sees all committed changes with XID < snapshot_xid
    // - Transaction does not see aborted changes
    // - Transaction does not see active changes from other transactions

    if (xid == snapshot_xid)
    {
        return true; // See own changes
    }

    if (xid > snapshot_xid)
    {
        return false; // Future transaction
    }

    // Frozen tuples are always visible
    if (xid <= FROZEN_XID)
    {
        return true;
    }

    TransactionState state;
    if (getTransactionState(xid, state, nullptr) != Status::OK)
    {
        // Error getting state, for old transactions assume committed
        if (xid < snapshot_xid)
        {
            return true; // Old transaction, assume committed
        }
        return false;
    }

    return state == TransactionState::COMMITTED;
}
```

**Analysis:**
- ✅ Handles FROZEN_XID correctly
- ✅ Handles equality check (own changes)
- ✅ Checks transaction state from TIP/CLOG
- ❌ **NO VALIDATION:** Accepts any XID value without bounds checking
- ❌ **DANGEROUS:** If `xid` is garbage (e.g., UINT64_MAX), it's treated as "future transaction"
- ❌ **ASSUMPTION:** "Old transactions assume committed" is risky for invalid XIDs

#### 3.2 StorageEngine::isVisible()

**Location:** `src/core/storage_engine.cpp:193-227`

```cpp
auto StorageEngine::isVisible(uint64_t xmin, uint64_t xmax, uint64_t current_xid) -> bool
{
    // Use transaction manager for visibility if available
    if (db_->transaction_manager() != nullptr)
    {
        TransactionManager *tm = db_->transaction_manager();

        // Check if creating transaction is visible
        if (!tm->isTransactionVisible(xmin, current_xid))
        {
            return false;
        }

        // If deleted, check if deleting transaction is visible
        if (xmax != 0 && tm->isTransactionVisible(xmax, current_xid))
        {
            return false;
        }

        return true;
    }

    // Fallback to simple visibility rules
    if (xmin > current_xid)
    {
        return false; // Created by future transaction
    }

    if (xmax > 0 && xmax < current_xid)
    {
        return false; // Deleted by committed transaction
    }

    return true;
}
```

**Analysis:**
- ❌ **NO VALIDATION:** Passes `xmin` and `xmax` directly to visibility checks
- ❌ **CORRUPTED DATA:** If tuple header contains garbage XIDs, treated as valid
- ❌ **FALLBACK LOGIC:** Simple visibility rules don't validate XID range

#### 3.3 HeapPage Visibility (Simple Check)

**Location:** `src/core/heap_page.cpp:655-667`

```cpp
auto *tuple_hdr = reinterpret_cast<TupleHeader *>(current_page_data + offset);

// Check visibility of this version
// Simple visibility: xmin <= snapshot_xid < xmax
bool visible = false;
if (tuple_hdr->xmin <= snapshot_xid)
{
    if (tuple_hdr->xmax == 0 || tuple_hdr->xmax > snapshot_xid)
    {
        visible = true;
    }
}
```

**Analysis:**
- ❌ **MOST DANGEROUS:** Direct comparison without ANY validation
- ❌ **CORRUPTION RISK:** If `xmin` or `xmax` is corrupted, visibility is wrong
- ❌ **NO BOUNDS CHECK:** Accepts any uint64_t value

### 4. XID Sources (Where XIDs Come From)

#### 4.1 From TupleHeader (Stored on Disk)

**Location:** `include/scratchbird/core/heap_page.h:66-70`

```cpp
struct TupleHeader
{
    // Transaction info (16 bytes)
    uint64_t xmin;               // Transaction ID that inserted this tuple
    uint64_t xmax;               // Transaction ID that deleted/updated this tuple (or 0)
    // ...
};
```

**Risk:** These are raw bytes from disk - could be:
- Corrupted by disk errors
- Maliciously crafted
- Leftover from previous database
- Garbage from uninitialized memory

#### 4.2 From Database Header

**Location:** `src/core/transaction_manager.cpp:111-121`

```cpp
auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
tip_root_page_ = db_header->tip_root_page;
next_xid_ = db_header->next_transaction_id;

if (next_xid_ <= FROZEN_XID)
{
    next_xid_ = FROZEN_XID + 1;
}
```

**Analysis:**
- ✅ Validates against FROZEN_XID
- ❌ No upper bound check (could be UINT64_MAX or corrupted)

---

## Vulnerability Scenarios

### Scenario 1: Corrupted Tuple Header

**Attack Vector:** Disk corruption or malicious database file

```
Tuple on disk:
  xmin = 0xFFFFFFFFFFFFFFFF (UINT64_MAX)
  xmax = 0
  data = "secret data"
```

**Current Behavior:**
1. `isTransactionVisible(UINT64_MAX, current_xid)` called
2. Check: `UINT64_MAX > current_xid` → Returns false
3. Tuple is **invisible** (benign outcome, but for wrong reason)

**Risk:** Low (accidentally safe)

### Scenario 2: XID from Old Database

**Attack Vector:** Database file copied from system with different XID counter

```
Old database last XID: 1,000,000
New database current XID: 1,000

Tuple from old database:
  xmin = 500,000
  xmax = 0
```

**Current Behavior:**
1. `isTransactionVisible(500000, 1000)` called
2. Check: `500000 > 1000` → Returns false
3. Tuple is **invisible** (data loss!)

**Risk:** HIGH - Valid data becomes invisible

### Scenario 3: XID = 0 (INVALID_XID)

**Attack Vector:** Uninitialized memory or corrupted data

```
Tuple:
  xmin = 0 (INVALID_XID)
  xmax = 0
```

**Current Behavior:**
1. `isTransactionVisible(0, current_xid)` called
2. Check: `xid <= FROZEN_XID` (0 <= 2) → Returns true
3. Tuple is **ALWAYS VISIBLE** (security issue!)

**Risk:** CRITICAL - Invalid tuples are visible

### Scenario 4: XID = 1 or 2 (Reserved XIDs)

```
Tuple:
  xmin = 1 (BOOTSTRAP_XID)
  xmax = 0
```

**Current Behavior:**
1. Check: `xid <= FROZEN_XID` → Returns true
2. Tuple is **ALWAYS VISIBLE** (may or may not be correct)

**Risk:** MEDIUM - Reserved XIDs have special meaning

### Scenario 5: Next XID Overflow

**Attack Vector:** Long-running database approaches UINT64_MAX

```
next_xid_ = UINT64_MAX
beginTransaction() called:
  new_xid = UINT64_MAX
  next_xid_++ → Wraps to 0
  Check: next_xid_ <= FROZEN_XID (0 <= 2) → Corrects to 3
```

**Current Behavior:**
- Next transaction gets XID = 3
- Previous transaction had XID = UINT64_MAX
- All old transactions (XIDs 3 to UINT64_MAX-1) appear "future"
- **CATASTROPHIC DATA LOSS**

**Risk:** CRITICAL (but requires ~18 quintillion transactions)

---

## What Should Happen

### Required Validations

#### 1. **Validate XID Range Before Visibility Check**

```cpp
auto isValidXid(uint64_t xid) -> bool
{
    // INVALID_XID is never valid for tuple headers
    if (xid == INVALID_XID)
    {
        return false;
    }

    // Reserved XIDs (BOOTSTRAP_XID, FROZEN_XID) are valid
    // All XIDs >= FROZEN_XID + 1 are potentially valid

    // Check if XID is within reasonable range of current next_xid
    // (Allows for historical XIDs but rejects far-future XIDs)

    return true;
}
```

#### 2. **Validate Against Current Transaction State**

```cpp
auto isXidInValidRange(uint64_t xid, uint64_t current_next_xid) -> bool
{
    // Special XIDs are always valid
    if (xid <= FROZEN_XID)
    {
        return true;
    }

    // XIDs must be less than next_xid (no future transactions)
    if (xid >= current_next_xid)
    {
        return false; // Future XID - invalid!
    }

    // TODO: Check if XID is too old (vacuumed away)
    // This requires tracking oldest valid XID

    return true;
}
```

#### 3. **Handle Invalid XIDs Gracefully**

```cpp
auto isTransactionVisible(uint64_t xid, uint64_t snapshot_xid) -> bool
{
    // VALIDATE FIRST
    if (!isXidInValidRange(xid, next_xid_))
    {
        // Invalid XID - treat as invisible or error
        return false;
    }

    // ... rest of visibility logic ...
}
```

---

## Recommended Fix

### Phase 1: Add XID Validation Functions

**Add to TransactionManager:**

```cpp
// Validate if XID is structurally valid
auto isValidXid(uint64_t xid) const -> bool;

// Validate if XID is in valid range for current database state
auto isXidInRange(uint64_t xid) const -> bool;
```

### Phase 2: Update Visibility Checks

**Update `isTransactionVisible()`:**
- Add validation at start of function
- Return false for invalid XIDs
- Log warning for out-of-range XIDs

**Update `StorageEngine::isVisible()`:**
- Validate xmin and xmax before delegating to TransactionManager

**Update `HeapPage` visibility:**
- Add validation in simple visibility checks

### Phase 3: Handle Edge Cases

1. **XID Wraparound Prevention:**
   - Track oldest valid XID
   - Require VACUUM before allowing wraparound
   - PostgreSQL-style "autovacuum to prevent wraparound"

2. **Database Load Validation:**
   - Validate `next_xid` from database header
   - Reject databases with suspicious XID values

3. **Tuple Validation:**
   - Optionally validate all tuple XIDs during page load
   - Mark pages as corrupted if invalid XIDs found

---

## Impact Assessment

### Current Risk Level: **HIGH**

**Actual Exploitability:**
- Low in normal operation (XIDs allocated correctly)
- High if database file is corrupted or tampered
- CRITICAL after UINT64_MAX wraparound (unlikely but catastrophic)

### After Fix: **MEDIUM**

**Remaining Risks:**
- XID wraparound still needs long-term solution (VACUUM integration)
- Need to track oldest valid XID for proper validation

---

## Effort Estimate

**Phase 1:** 2-3 hours (add validation functions)
**Phase 2:** 3-4 hours (update all visibility checks)
**Phase 3:** 4-6 hours (edge cases, wraparound handling)
**Testing:** 2-3 hours (unit tests, corruption tests)

**Total:** 11-16 hours

---

## Conclusion

**Issue #59 is a REAL vulnerability** - XIDs from TupleHeader are used directly in visibility checks without validation.

**Immediate Risk:** LOW (requires disk corruption or malicious database)
**Long-term Risk:** HIGH (XID wraparound, data integrity)

**Recommended Action:** Implement Phase 1 and Phase 2 immediately for defense-in-depth.

---

**Status:** Analysis complete, ready for implementation
**Next Step:** Implement validation functions in TransactionManager
