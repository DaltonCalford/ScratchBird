# MGA Rules Implementation: Preventing PostgreSQL MVCC Contamination

**Date**: November 1, 2025
**Purpose**: Document the permanent solution to prevent PostgreSQL MVCC contamination
**Status**: COMPLETE

---

## Problem Statement

### The Issue

Task 17 and subsequent index analysis were implemented with **PostgreSQL MVCC** (snapshot-based visibility) instead of **Firebird MGA** (TIP-based visibility). This architectural violation affected:

- All 7 index types
- ~2,500 lines of contaminated code
- Complete misanalysis of MGA compliance
- 215-300 hours of required correction effort

### Root Cause

**Context compaction caused loss of MGA understanding**. While initial work correctly understood Firebird MGA, memory cleanup destroyed this knowledge. Even explicit instructions to read README and PROJECT_CONTEXT.md were insufficient because:

1. **PROJECT_CONTEXT.md Section 8 was too brief** - only showed table of differences, not implementation rules
2. **No mandatory reading enforcement** - easy to skip or forget
3. **No detection checklist** - couldn't identify contamination
4. **No code examples** - only conceptual understanding, not implementation details

---

## Solution: MGA_RULES.md

### What Was Created

A comprehensive, mandatory reference document containing:

- **15 absolute rules** for Firebird MGA implementation
- **Side-by-side examples** (❌ WRONG PostgreSQL vs ✅ CORRECT Firebird)
- **Detection checklists** for contamination
- **Complete code examples** with actual implementations
- **API specifications** (what to use, what to avoid)
- **Specification references** for deeper reading

**File**: `/MGA_RULES.md` (650 lines)

### Enforcement Mechanism

**1. CLAUDE.md Updated**

```
Session Start → Read /PROJECT_CONTEXT.md AND /MGA_RULES.md
After Compaction → Re-read /PROJECT_CONTEXT.md AND /MGA_RULES.md
Before ANY Transaction/Index Work → MUST read /MGA_RULES.md FIRST
```

**CRITICAL warning added**:
> If these rules are violated, the code is WRONG and must be rewritten.
> NO EXCEPTIONS. NO MIXING. Pure Firebird MGA only.

**2. PROJECT_CONTEXT.md Enhanced**

**Section 8** now includes:
- Mandatory reading requirement
- Detection rules (contamination indicators)
- MGA compliance indicators
- Direct link to `/MGA_RULES.md`

**Section 9** now emphasizes:
- Must read both files at session start
- Must read before transaction/index work
- Violations = architecturally WRONG code

---

## The 15 MGA Rules (Summary)

### Rule 0: The Fundamental Distinction
ScratchBird uses Firebird MGA, NOT PostgreSQL MVCC.

### Rule 1: NO SNAPSHOTS
- ❌ FORBIDDEN: `Snapshot` structures, `isSnapshotVisible()` API
- ✅ REQUIRED: `getTransactionState()`, TIP lookups

### Rule 2: Transaction Inventory Pages (TIP) Required
- TIP is a bitmap storing 2 bits per transaction
- Used for all visibility checks
- Capacity: ~32K-523K transactions per TIP page (based on page size)

### Rule 3: Visibility Check Uses TIP, Not Snapshots
```cpp
// WRONG (PostgreSQL):
bool is_visible(TransactionId xid, const Snapshot* snapshot);

// CORRECT (Firebird):
bool is_visible(TransactionId version_xid, TransactionId reader_xid) {
    TxState state = get_transaction_state(version_xid);  // TIP lookup
    return (state == TX_COMMITTED && version_xid < reader_xid);
}
```

### Rule 4: Transaction Markers (OIT/OAT/OST) Required
Stored in database header for garbage collection triggers.

### Rule 5: Back-Versioning, NOT Forward-Versioning
- Old data moved to back version
- New data written in-place
- Primary record points backward to old version

### Rule 6: In-Place Updates with Stable TIDs
- Primary record modified at original location
- TID never changes
- Indexes never updated (unless indexed column modified)

### Rule 7: Newest-to-Oldest (N2O) Version Chains
Traverse backward from primary to find visible version.

### Rule 8: Index Behavior
Indexes only updated when indexed column changes.

### Rule 9: No Index Bloat
Stable TIDs prevent index growth on UPDATEs.

### Rule 10: Garbage Collection via Sweep
Removes old back versions, not primary records.

### Rule 11: API Signatures
- ❌ FORBIDDEN: `Snapshot* snapshot` parameters
- ✅ REQUIRED: `TransactionId current_xid` parameters

### Rule 12: TransactionManager API
- ❌ FORBIDDEN: `Snapshot` structure, `getSnapshot()`, `isSnapshotVisible()`
- ✅ REQUIRED: `getTransactionState()`, `isVersionVisible()`

### Rule 13: Detection Checklist
Lists all contamination indicators and compliance indicators.

### Rule 14: Specification References
Links to `/docs/specifications/MGA_IMPLEMENTATION.md` and related docs.

### Rule 15: When In Doubt
Default to Firebird MGA being correct and PostgreSQL MVCC being wrong.

---

## Detection Rules

### MVCC Contamination Indicators (ALL WRONG)

If you see ANY of these, the code is **architecturally incorrect**:

- ❌ `Snapshot` structure
- ❌ `snapshot` parameter names
- ❌ `isSnapshotVisible()` function calls
- ❌ `xmin`, `xmax` as snapshot markers
- ❌ `active_xids[]` array
- ❌ Forward pointers (old → new)
- ❌ Tuples created at new locations
- ❌ Index TID updates on every UPDATE
- ❌ "Append-only" or "heap-only tuple" terminology

### MGA Compliance Indicators (ALL REQUIRED)

If you see ALL of these, the code is **correct**:

- ✅ TIP (Transaction Inventory Page) implementation
- ✅ `getTransactionState(xid)` function calls
- ✅ `TxState` enum (TX_COMMITTED, TX_ACTIVE, TX_ABORTED)
- ✅ OIT/OAT/OST markers
- ✅ Back pointers (new → old)
- ✅ In-place updates
- ✅ Stable TIDs
- ✅ "Back version" terminology

---

## Code Examples in MGA_RULES.md

### Example 1: TIP Structure
```c
struct SBTipPage {
    PageHeader      tip_header;
    TransactionId   tip_min;
    TransactionId   tip_max;
    uint32_t        tip_next_page;
    uint32_t        tip_transactions_count;
    uint8_t         tip_transactions[];  // 2 bits per transaction
};
```

### Example 2: TIP Lookup
```cpp
TxState get_transaction_state(TransactionId xid) {
    uint32_t page_num = xid / TRANSACTIONS_PER_TIP_PAGE;
    uint32_t offset = xid % TRANSACTIONS_PER_TIP_PAGE;
    SBTipPage *tip_page = pin_tip_page(page_num);
    uint8_t byte = tip_page->tip_transactions[offset / 4];
    uint8_t shift = (offset % 4) * 2;
    TxState state = (TxState)((byte >> shift) & 0x03);
    unpin_tip_page(page_num);
    return state;
}
```

### Example 3: Visibility Check (CORRECT)
```cpp
bool is_visible(TransactionId version_xid, TransactionId reader_xid) {
    if (version_xid == reader_xid) return true;  // Own changes
    TxState state = get_transaction_state(version_xid);  // TIP lookup
    return (state == TX_COMMITTED && version_xid < reader_xid);
}
```

### Example 4: Update Algorithm (CORRECT)
```cpp
void update_record(TID primary_tid, const RecordData* new_data, TransactionId xid) {
    Record *current = fetch_record(primary_tid);

    // Create back version (old data)
    TID back_tid = allocate_record_space();
    copy_record_data(back_tid, current->data);
    set_back_version_xid(back_tid, current->rhd_transaction);

    // Modify primary IN-PLACE
    overwrite_record_data(primary_tid, new_data);
    current->rhd_transaction = xid;
    current->rhd_b_page = back_tid.page;
    current->rhd_b_line = back_tid.line;
    current->rhd_flags |= rhd_chain;

    // Indexes NEVER CHANGE (still point to primary_tid)
}
```

### Example 5: Version Chain Traversal
```cpp
Record* find_visible_version(TID primary_tid, TransactionId reader_xid) {
    TID current_tid = primary_tid;
    while (!is_null(current_tid)) {
        Record *version = fetch_record(current_tid);
        if (is_visible(version->rhd_transaction, reader_xid)) {
            if (version->rhd_flags & rhd_deleted) return nullptr;
            return version;
        }
        current_tid = TID(version->rhd_b_page, version->rhd_b_line);
    }
    return nullptr;
}
```

---

## Benefits of MGA_RULES.md

### 1. Survives Context Compaction
Unlike dynamic memory, this file is permanent and will be read at every session start.

### 2. Comprehensive Coverage
All aspects of MGA vs MVCC covered with examples:
- TIP structure and usage
- Visibility checking
- Transaction markers
- Back-versioning
- Update algorithms
- Index behavior
- Garbage collection

### 3. Clear Detection Rules
Easy to identify contamination:
- Checklist of forbidden patterns
- Checklist of required patterns
- Side-by-side wrong/correct examples

### 4. Mandatory Reading
Enforced in CLAUDE.md:
- Session start
- After compaction
- Before transaction/index work

### 5. Single Source of Truth
One file contains all MGA rules - no ambiguity or scattered information.

### 6. Implementation-Focused
Not just theory - includes complete code examples that can be directly used.

---

## Verification Process

### How to Check MGA Compliance

**Step 1**: Search for contamination indicators
```bash
# Search for forbidden patterns
grep -r "Snapshot\*" include/ src/
grep -r "isSnapshotVisible" include/ src/
grep -r "active_xids" include/ src/
```

**Step 2**: Search for required indicators
```bash
# Search for required patterns
grep -r "getTransactionState" include/ src/
grep -r "TxState" include/ src/
grep -r "TIP" include/ src/
grep -r "OIT\|OAT\|OST" include/ src/
```

**Step 3**: Review API signatures
- Check all transaction-related functions
- Ensure no `Snapshot*` parameters
- Ensure `TransactionId current_xid` parameters

**Step 4**: Review update logic
- Ensure in-place modification
- Ensure back version creation
- Ensure stable TIDs

---

## Current Status

### Files Updated
1. **MGA_RULES.md** (NEW) - 650 lines of comprehensive rules
2. **CLAUDE.md** - Mandatory reading requirement added
3. **PROJECT_CONTEXT.md** - Section 8 and 9 enhanced

### Previous Contamination Documented
1. **CRITICAL_MGA_MVCC_CONFUSION_ANALYSIS.md** - 8,500 lines analysis
2. **INDEX_TYPES_CORRECT_MGA_ANALYSIS.md** - 1,100 lines corrected analysis

### Current Code Status
- ❌ B-tree: CONTAMINATED (PostgreSQL MVCC)
- ❌ All other indexes: CONTAMINATED
- ⚠️ TransactionManager API: WRONG (has Snapshot structure)
- ❌ TIP: NOT IMPLEMENTED

### Required Work
**Phase 1**: Infrastructure (85-110 hours)
- Implement TIP
- Implement OIT/OAT/OST markers
- Redesign TransactionManager API

**Phase 2**: Indexes (55-80 hours)
- Rewrite B-tree visibility
- Fix Hash and Bitmap
- Fix advanced indexes

**Total**: 140-190 hours (plus 55-85 hours for test rewrite)

---

## Preventing Future Contamination

### Mandatory Checks Before Commit

**1. Read MGA_RULES.md**
- Before starting any transaction/index work
- After context compaction
- When unsure about implementation

**2. Search for Contamination**
```bash
# Run this before every commit
git diff | grep -i snapshot
git diff | grep -i "active_xids"
git diff | grep -i "append.only"
```

**3. Verify API Signatures**
- No `Snapshot*` parameters
- Use `TransactionId current_xid` instead

**4. Verify Implementation**
- TIP lookups present
- In-place updates
- Back-versioning
- Stable TIDs

### Code Review Checklist

For any transaction or index code:

- [ ] Read MGA_RULES.md before review
- [ ] Check for `Snapshot` keyword (WRONG if present)
- [ ] Check for TIP lookups (WRONG if absent)
- [ ] Check for in-place updates (WRONG if append-only)
- [ ] Check for back pointers (WRONG if forward pointers)
- [ ] Check for stable TIDs (WRONG if TIDs change)
- [ ] Verify against rules 1-15 in MGA_RULES.md

---

## Lessons Learned

### Why This Happened

1. **Terminology confusion**: Both PostgreSQL and Firebird use "MVCC" term
2. **Context compaction**: Loss of understanding after memory cleanup
3. **Insufficient documentation**: PROJECT_CONTEXT.md too brief
4. **No enforcement**: Reading context was suggested, not mandatory
5. **No detection tools**: Couldn't identify contamination early

### What Changed

1. **Comprehensive rules**: MGA_RULES.md covers everything
2. **Mandatory reading**: Enforced in CLAUDE.md
3. **Detection checklist**: Easy to identify contamination
4. **Code examples**: Can't misunderstand with full implementations
5. **Permanent reference**: Survives context compaction

### Key Takeaways

1. ✅ **Architectural rules need permanent documentation**
2. ✅ **Mandatory reading must be enforced, not suggested**
3. ✅ **Detection rules must be explicit and checkable**
4. ✅ **Code examples are essential, not optional**
5. ✅ **When in doubt, consult the rules**

---

## Conclusion

**MGA_RULES.md solves the PostgreSQL MVCC contamination problem permanently.**

### What It Provides

1. **15 absolute rules** for Firebird MGA
2. **Clear detection** of contamination
3. **Complete code examples** for all aspects
4. **Mandatory enforcement** via CLAUDE.md
5. **Permanent reference** that survives compaction

### How It Works

1. Read at every session start
2. Read after every context compaction
3. Read before any transaction/index work
4. Enforce via CLAUDE.md instructions
5. Verify via detection checklists

### Expected Outcome

**Zero PostgreSQL MVCC contamination in future code.**

All transaction and index code will correctly implement:
- TIP-based visibility (not snapshot-based)
- In-place updates (not append-only)
- Back-versioning (not forward-versioning)
- Stable TIDs (not changing TIDs)
- OIT/OAT/OST markers (not just Next)

### The Core Principle

**PostgreSQL MVCC**: "Is this XID in the snapshot's active transaction array?"

**Firebird MGA**: "Is this XID committed and older than me?" (TIP lookup)

**ONE uses snapshots, ONE uses TIP.**

**ScratchBird uses TIP (Firebird MGA).**

---

**Document Status**: SOLUTION COMPLETE
**Date**: November 1, 2025
**Files Created**: MGA_RULES.md, CLAUDE.md updates, PROJECT_CONTEXT.md updates
**Enforcement**: MANDATORY reading via CLAUDE.md
**Expected Result**: Zero future PostgreSQL MVCC contamination
