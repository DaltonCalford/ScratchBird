# Phase 3 Revised Tasks - Storage Layer TOAST Integration

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: November 3, 2025
**Status**: Replacement for incorrect Phase 3 tasks in TOAST_MGA_COMPLIANCE_FIX_PLAN.md

---

## Tasks to Replace in TOAST_MGA_COMPLIANCE_FIX_PLAN.md

Replace Tasks 3.2 through 3.7 (lines ~797-1014) with the following:

---

#### Task 3.2: Integrate with Storage Engine Insert Path
**File**: `src/core/storage_engine.cpp` or `src/core/heap_page.cpp`
**Duration**: 6-8 hours
**Status**: ⏳ PENDING

**Purpose**: Use `IndexKeyExtractor` in storage engine's tuple insert path.

**Implementation**:
```cpp
Status StorageEngine::insertTuple(
    const ID& table_id,
    const uint8_t* tuple_data,
    size_t tuple_size,
    uint32_t* page_id_out,
    uint16_t* item_id_out,
    ErrorContext* ctx)
{
    // 1. TOAST large columns (existing code)
    // ... toastValue() for columns > 2KB ...

    // 2. Insert heap tuple (existing code)
    TID tid = insertHeapTuple(tuple_data, tuple_size, page_id_out, item_id_out, ctx);

    // 3. Update indexes - NEW CODE
    Table table_info = catalog_->getTable(table_id);
    ToastManager* toast_mgr = getToastManager(table_id);

    IndexKeyExtractor extractor;

    for (Index* index : table_info.indexes) {
        // Extract index-ready key with automatic detoasting
        std::vector<uint8_t> key;
        Status status = extractor.extractKey(
            tuple_data, tuple_size,
            table_info.column_offsets,
            table_info.column_sizes,
            index.column_indices,
            toast_mgr,
            xid,
            &key,
            ctx);

        if (status != Status::OK) {
            // Rollback tuple insert
            return status;
        }

        // Index receives ACTUAL VALUE, not TOAST pointer
        status = index->insert(key, tid, xid, ctx);
        if (status != Status::OK) {
            // Rollback
            return status;
        }
    }

    // Clear cache after processing all indexes
    extractor.clearCache();

    return Status::OK;
}
```

**Validation**:
- IndexKeyExtractor used for all index inserts ✅
- Keys are detoasted before passing to indexes ✅
- Cache cleared after operation ✅

---

#### Task 3.3: Integrate with Storage Engine Update Path
**File**: `src/core/storage_engine.cpp` or `src/core/heap_page.cpp`
**Duration**: 8-12 hours
**Status**: ⏳ PENDING

**Purpose**: Use `IndexKeyExtractor` for update operations (old and new keys).

**Implementation**:
```cpp
Status StorageEngine::updateTuple(
    const ID& table_id,
    const TID& tid,
    const uint8_t* new_tuple_data,
    size_t new_tuple_size,
    ErrorContext* ctx)
{
    // 1. Fetch old tuple
    std::vector<uint8_t> old_tuple_data;
    Status status = heapFetch(tid, &old_tuple_data, xid, ctx);

    // 2. TOAST new large columns (existing code)
    // ...

    // 3. Create back version (existing MGA code)
    createBackVersion(tid, old_tuple_data, ctx);

    // 4. Update primary tuple in-place (existing MGA code)
    updateTupleInPlace(tid, new_tuple_data, new_tuple_size, xid, ctx);

    // 5. Update indexes - NEW CODE
    Table table_info = catalog_->getTable(table_id);
    ToastManager* toast_mgr = getToastManager(table_id);

    IndexKeyExtractor extractor;

    for (Index* index : table_info.indexes) {
        // Check if indexed columns changed
        if (!indexedColumnsChanged(index, old_tuple_data, new_tuple_data)) {
            continue;  // TID stable, no index update needed
        }

        // Extract OLD and NEW keys with automatic detoasting
        std::vector<uint8_t> old_key, new_key;
        Status status = extractor.extractKeyForUpdate(
            old_tuple_data.data(), old_tuple_data.size(),
            table_info.column_offsets,
            table_info.column_sizes,
            new_tuple_data, new_tuple_size,
            table_info.column_offsets,
            table_info.column_sizes,
            index.column_indices,
            toast_mgr,
            xid,
            &old_key,
            &new_key,
            ctx);

        if (status != Status::OK) {
            return status;
        }

        // Soft delete old index entry
        status = index->softDelete(old_key, tid, xid, ctx);
        if (status != Status::OK) {
            return status;
        }

        // Insert new index entry (same TID, new key)
        status = index->insert(new_key, tid, xid, ctx);
        if (status != Status::OK) {
            return status;
        }
    }

    extractor.clearCache();

    return Status::OK;
}
```

**Validation**:
- Old key extracted from old tuple (before update) ✅
- New key extracted from new tuple (after update) ✅
- Both keys detoasted automatically ✅
- TID remains stable if indexed columns unchanged ✅

---

#### Task 3.4: Add ToastManager::isToastPointer() and detoastIfNeeded()
**File**: `include/scratchbird/core/toast.h`, `src/core/toast.cpp`
**Duration**: 3-5 hours
**Status**: ⏳ PENDING

**Purpose**: Provide static helpers for TOAST pointer detection.

**Implementation in toast.h**:
```cpp
class ToastManager {
public:
    // ... existing methods ...

    /**
     * Check if data is a TOAST pointer
     *
     * @param data Pointer to data
     * @param size Size of data
     * @return true if data is exactly 18 bytes and has TOAST magic
     */
    static bool isToastPointer(const uint8_t* data, size_t size);

    /**
     * Detoast a value if it's a TOAST pointer, otherwise return original
     *
     * @param data Input data (may be TOAST pointer or inline data)
     * @param size Size of input data
     * @param result Output buffer
     * @param xid Transaction ID for visibility
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status detoastIfNeeded(
        const uint8_t* data,
        size_t size,
        std::vector<uint8_t>* result,
        uint64_t xid,
        ErrorContext* ctx);
};
```

**Implementation in toast.cpp**:
```cpp
bool ToastManager::isToastPointer(const uint8_t* data, size_t size)
{
    // TOAST pointer is exactly 18 bytes
    if (size != sizeof(ToastPointer)) {
        return false;
    }

    // Check magic byte (va_header)
    const ToastPointer* ptr = reinterpret_cast<const ToastPointer*>(data);
    ToastStrategy strategy = static_cast<ToastStrategy>(ptr->va_tag);

    return (strategy == ToastStrategy::EXTENDED ||
            strategy == ToastStrategy::EXTERNAL ||
            strategy == ToastStrategy::COMPRESSED);
}

Status ToastManager::detoastIfNeeded(
    const uint8_t* data,
    size_t size,
    std::vector<uint8_t>* result,
    uint64_t xid,
    ErrorContext* ctx)
{
    if (isToastPointer(data, size)) {
        // Detoast value
        const ToastPointer* pointer = reinterpret_cast<const ToastPointer*>(data);
        return detoastValue(pointer, result, xid, ctx);
    } else {
        // Not a TOAST pointer, return original data
        result->assign(data, data + size);
        return Status::OK;
    }
}
```

**Validation**:
- isToastPointer() detects 18-byte pointers with magic ✅
- detoastIfNeeded() detoasts pointers, passes through inline data ✅

---

#### Task 3.5: Performance Optimization - Detoasting Cache
**File**: `src/core/index_key_extractor.cpp`
**Duration**: 3-5 hours
**Status**: ✅ COMPLETE (already implemented in IndexKeyExtractor)

**Purpose**: Avoid repeated detoasting when multiple indexes use same column.

**Implementation**: Already in IndexKeyExtractor::getColumnValue()
- Checks `detoast_cache_` first
- Only detoasts if not cached
- Caches result for reuse

**Benefit**: 1 detoast per column instead of N detoasts for N indexes.

---

### Phase 3 Validation Checklist (REVISED)

- [x] IndexKeyExtractor class created ✅
- [ ] Storage engine insert path uses IndexKeyExtractor
- [ ] Storage engine update path uses IndexKeyExtractor
- [ ] ToastManager::isToastPointer() implemented
- [ ] ToastManager::detoastIfNeeded() implemented
- [ ] Detoasting cache prevents repeated work
- [ ] All indexes receive actual values, never TOAST pointers
- [ ] TID stability maintained (indexes point to heap tuples)
- [ ] No changes required to any of the 7 index types

---

### Phase 3 Testing (REVISED)

**Create**: `tests/integration/test_storage_toast_integration.cpp`

**Test cases**:
1. **Insert with TOAST**:
   - Insert tuple with large column (triggers TOAST)
   - Create index on TOASTed column
   - Verify index contains actual value, not pointer bytes
   - Query via index, verify correct results

2. **Update with TOAST**:
   - Update TOASTed column (indexed)
   - Verify old index entry soft-deleted
   - Verify new index entry created with actual value
   - Verify TID stable (both entries point to same heap tuple)

3. **Multiple indexes on same TOAST column**:
   - Create 3 indexes on same TOASTed column
   - Insert tuple
   - Verify detoasting happens only ONCE (cache hit)
   - Verify all 3 indexes have actual value

4. **Mixed TOAST and inline columns**:
   - Index on composite key (TOASTed + inline columns)
   - Verify inline columns pass through, TOASTed columns detoasted
   - Verify concatenated key is correct

5. **Error handling**:
   - Corrupt TOAST pointer
   - Verify detoasting error caught
   - Verify index insert rollback

**Manual validation**:
```sql
-- Create table with TOASTed column
CREATE TABLE users (id INT, profile TEXT);

-- Insert large value (>2KB, triggers TOAST)
INSERT INTO users VALUES (1, <50KB text>);

-- Create B-tree index
CREATE INDEX idx_profile ON users(profile);

-- Query via index
SELECT * FROM users WHERE profile = '<50KB text>';
-- Should return row (index has actual value)

-- Verify index internals (debug query)
SELECT btree_dump_keys('idx_profile');
-- Should show actual text, NOT 18-byte pointer
```

---

## Summary of Changes from Original Phase 3

**Original (INCORRECT)**:
- Modify all 7 index types to detoast values
- Add ToastManager reference to each index class
- Code duplication across 7 implementations
- Violates separation of concerns
- Est: 40-60 hours

**Revised (CORRECT)**:
- Create IndexKeyExtractor helper (storage layer)
- Modify storage engine insert/update paths
- Indexes remain unchanged and TOAST-unaware
- Clean separation of concerns
- Est: 20-30 hours

**Reduction**: 20-30 hours saved by correct architecture.

**Key Insight**: Indexes don't need to know about TOAST. Storage layer handles everything.

---

**Instructions for Updating Main Plan**:
1. Replace Tasks 3.2-3.7 in TOAST_MGA_COMPLIANCE_FIX_PLAN.md with Tasks 3.2-3.5 above
2. Update Phase 3 Validation Checklist
3. Update Phase 3 Testing section
4. Update estimated hours: 20-30 (from 40-60)
