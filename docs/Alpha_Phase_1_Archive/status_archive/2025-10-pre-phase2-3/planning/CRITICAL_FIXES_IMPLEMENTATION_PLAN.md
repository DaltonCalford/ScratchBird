# Critical Code Issues - Analysis and Implementation Plan

**Date:** 2025-09-30
**Priority:** P0 - CRITICAL (Blocks production use)
**Estimated Time:** 8-12 hours
**Branch:** Will create `critical-fixes-20250930`

---

## Executive Summary

This document provides a detailed analysis and implementation plan for 6 critical code issues that pose serious risks:
- 2 CRITICAL issues that cause crashes/memory corruption
- 2 HIGH issues that cause memory leaks and buffer overflows
- 2 MEDIUM issues that violate best practices

**Impact if not fixed:**
- Memory corruption and crashes (ErrorContext)
- Memory leaks (Database destructor)
- Buffer overflows on non-8KB pages (TransactionManager)
- Out-of-bounds memory access (HeapPage)
- Compilation errors (B-Tree static methods)
- Linker errors (Template ODR violations)

---

## Issue #1: B-Tree Static Method Violations (CRITICAL)

### Current State

**File:** `include/scratchbird/core/btree.h` (Lines 143, 147)
**Severity:** CRITICAL
**Risk:** Code won't compile/link correctly

**Problem Code:**
```cpp
class BTree {
public:
    BTree(Database *db, SBBTreeIndex index_info);
    ~BTree();

    static Status insert(const std::vector<uint8_t> &key, uint64_t tuple_id,
                  ErrorContext *ctx = nullptr);  // ❌ Static but needs instance data
    Status search(const std::vector<uint8_t> &key, std::vector<uint64_t> *tuple_ids_out,
                  ErrorContext *ctx = nullptr);
    static Status remove(const std::vector<uint8_t> &key, uint64_t tuple_id,
                  ErrorContext *ctx = nullptr);  // ❌ Static but needs instance data
private:
    Database *db_;              // Instance member
    SBBTreeIndex index_info_;   // Instance member
};
```

**Why This is Wrong:**
- `insert()` and `remove()` are declared `static`
- Static methods cannot access instance members (`db_`, `index_info_`)
- Implementation in `btree.cpp` returns `INVALID_ARGUMENT` stubs
- `search()` is correctly non-static

**Current Implementation (btree.cpp):**
```cpp
// Line 24
Status BTree::insert(const std::vector<uint8_t> &key, uint64_t tuple_id, ErrorContext *ctx) {
    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "B-tree insert not implemented");
    return Status::INVALID_ARGUMENT;  // Just a stub
}

// Line 146
Status BTree::remove(const std::vector<uint8_t> &key, uint64_t tuple_id, ErrorContext *ctx) {
    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "B-tree remove not implemented");
    return Status::INVALID_ARGUMENT;  // Just a stub
}
```

### Root Cause Analysis

This appears to be a **copy-paste error** or **incomplete refactoring**. The methods were likely:
1. Initially designed as static utility functions
2. Later refactored to instance methods (hence the stub implementations)
3. Declaration not updated to remove `static` keyword

### Implementation Plan

#### Step 1: Fix Method Declarations

**File:** `include/scratchbird/core/btree.h`

```cpp
// CHANGE FROM:
static Status insert(const std::vector<uint8_t> &key, uint64_t tuple_id,
              ErrorContext *ctx = nullptr);
Status search(const std::vector<uint8_t> &key, std::vector<uint64_t> *tuple_ids_out,
              ErrorContext *ctx = nullptr);
static Status remove(const std::vector<uint8_t> &key, uint64_t tuple_id,
              ErrorContext *ctx = nullptr);

// CHANGE TO:
Status insert(const std::vector<uint8_t> &key, uint64_t tuple_id,
              ErrorContext *ctx = nullptr);  // Removed 'static'
Status search(const std::vector<uint8_t> &key, std::vector<uint64_t> *tuple_ids_out,
              ErrorContext *ctx = nullptr);
Status remove(const std::vector<uint8_t> &key, uint64_t tuple_id,
              ErrorContext *ctx = nullptr);  // Removed 'static'
```

#### Step 2: Verify Implementation

**File:** `src/core/btree.cpp`

No changes needed - implementation is already correct (non-static).

#### Step 3: Search for Call Sites

```bash
grep -rn "BTree::insert\|BTree::remove" src/ tests/
```

Verify no code is calling these as static methods like `BTree::insert(...)`.

#### Step 4: Test

- Compile and verify no errors
- If B-tree is used anywhere, verify instance method calls work

**Estimated Time:** 30 minutes

---

## Issue #2: ErrorContext Rule of Five Violation (CRITICAL)

### Current State

**File:** `include/scratchbird/core/error_context.h` (Lines 26-33)
**Severity:** CRITICAL
**Risk:** Double-free crashes, memory corruption

**Problem Code:**
```cpp
class ErrorContext {
public:
    ErrorContext() : status(Status::OK), message(nullptr), cause(nullptr) {}

    ~ErrorContext() {
        delete cause;  // ❌ Deletes owned pointer
    }

    // ❌ NO COPY CONSTRUCTOR
    // ❌ NO COPY ASSIGNMENT
    // ❌ NO MOVE CONSTRUCTOR
    // ❌ NO MOVE ASSIGNMENT

    Status status;
    const char *message;
    ErrorContext *cause;  // Owned pointer
};
```

**The Problem (Visual):**
```cpp
void dangerous_function() {
    ErrorContext ctx1;
    ctx1.cause = new ErrorContext();  // Allocate

    ErrorContext ctx2 = ctx1;  // ❌ SHALLOW COPY (default copy constructor)
    // Now both ctx1 and ctx2 point to SAME cause pointer

} // ❌ DOUBLE FREE: Both destructors try to delete same pointer
  // RESULT: Crash, heap corruption
```

### Rule of Five/Three Explanation

**C++ Rule of Five:** If you define **any one** of:
1. Destructor
2. Copy constructor
3. Copy assignment operator
4. Move constructor
5. Move assignment operator

You should define **all of them** (or explicitly delete them).

**Why:** Default implementations do shallow copies, which break when you have owned pointers.

### Implementation Plan

#### Option A: Delete Copy/Move (Recommended for ErrorContext)

**Reasoning:** ErrorContext is typically used as a local variable, rarely copied.

```cpp
class ErrorContext {
public:
    ErrorContext() : status(Status::OK), message(nullptr), cause(nullptr) {}

    ~ErrorContext() {
        delete cause;
    }

    // Delete copy operations (prevent accidental copies)
    ErrorContext(const ErrorContext&) = delete;
    ErrorContext& operator=(const ErrorContext&) = delete;

    // Delete move operations (prevent accidental moves)
    ErrorContext(ErrorContext&&) = delete;
    ErrorContext& operator=(ErrorContext&&) = delete;

    Status status;
    const char *message;
    ErrorContext *cause;
};
```

**Trade-off:** Cannot copy/move ErrorContext, but this is actually GOOD - it's meant to be used as a local variable only.

#### Option B: Implement Deep Copy/Move (More Complex)

Only if ErrorContext needs to be copyable:

```cpp
class ErrorContext {
public:
    ErrorContext() : status(Status::OK), message(nullptr), cause(nullptr) {}

    // Copy constructor - deep copy
    ErrorContext(const ErrorContext& other)
        : status(other.status), message(other.message), cause(nullptr) {
        if (other.cause) {
            cause = new ErrorContext(*other.cause);  // Deep copy recursively
        }
    }

    // Copy assignment - deep copy
    ErrorContext& operator=(const ErrorContext& other) {
        if (this != &other) {
            delete cause;  // Delete old
            status = other.status;
            message = other.message;
            cause = other.cause ? new ErrorContext(*other.cause) : nullptr;
        }
        return *this;
    }

    // Move constructor
    ErrorContext(ErrorContext&& other) noexcept
        : status(other.status), message(other.message), cause(other.cause) {
        other.cause = nullptr;  // Transfer ownership
    }

    // Move assignment
    ErrorContext& operator=(ErrorContext&& other) noexcept {
        if (this != &other) {
            delete cause;
            status = other.status;
            message = other.message;
            cause = other.cause;
            other.cause = nullptr;
        }
        return *this;
    }

    ~ErrorContext() {
        delete cause;
    }

    Status status;
    const char *message;
    ErrorContext *cause;
};
```

#### Recommendation: Use Option A (Delete Copy/Move)

**Reasons:**
1. ErrorContext is used as local variable in function signatures
2. Never needs to be copied/moved in practice
3. Simpler and safer
4. Catches bugs at compile time if someone tries to copy

#### Step 1: Modify error_context.h

Apply Option A changes.

#### Step 2: Search for Usage Patterns

```bash
grep -rn "ErrorContext" src/ tests/ | grep -E "(= |return |push_back|emplace)"
```

Verify no code is copying ErrorContext objects.

#### Step 3: Test

Compile and verify all code still works.

**Estimated Time:** 1 hour

---

## Issue #3: Database Destructor Memory Leaks (HIGH)

### Current State

**File:** `include/scratchbird/core/database.h` (Lines 173-177)
**Severity:** HIGH
**Risk:** Memory leaks on Database destruction

**Problem Code:**
```cpp
class Database {
    // ... public methods ...

private:
    BufferPool *buffer_pool_;           // (owned) ❌ No delete in destructor
    PageManager *page_manager_;         // (owned) ❌ No delete in destructor
    CatalogManager *catalog_mgr_;       // (owned) ❌ No delete in destructor
    ToastManager *toast_manager_;       // (owned) ❌ No delete in destructor
    TransactionManager *txn_manager_;   // (owned) ❌ No delete in destructor
};
```

**Header declares destructor:**
```cpp
~Database();
```

**But implementation might not delete all owned pointers.**

### Root Cause Analysis

Need to check `src/core/database.cpp` destructor implementation to see what it actually deletes.

### Implementation Plan

#### Step 1: Analyze Current Destructor

```bash
grep -A 20 "Database::~Database" src/core/database.cpp
```

#### Step 2: Proper Destructor Implementation

**File:** `src/core/database.cpp`

```cpp
Database::~Database() {
    // Close file handle first (if open)
    if (fd_ >= 0) {
        close();  // Flushes buffers, releases locks
    }

    // Delete in reverse order of construction
    // (dependencies: catalog uses buffer_pool, buffer_pool uses page_manager, etc.)

    delete txn_manager_;
    txn_manager_ = nullptr;

    delete toast_manager_;
    toast_manager_ = nullptr;

    delete catalog_mgr_;
    catalog_mgr_ = nullptr;

    delete buffer_pool_;
    buffer_pool_ = nullptr;

    delete page_manager_;
    page_manager_ = nullptr;

    // Note: header_ is freed in close() via munmap if it was mapped
    // or should be deleted here if allocated with new
}
```

#### Step 3: Alternative - Use Smart Pointers (Better Long-term)

**Recommended:** Convert to `std::unique_ptr` for automatic cleanup.

**File:** `include/scratchbird/core/database.h`

```cpp
#include <memory>

class Database {
private:
    std::unique_ptr<BufferPool> buffer_pool_;
    std::unique_ptr<PageManager> page_manager_;
    std::unique_ptr<CatalogManager> catalog_mgr_;
    std::unique_ptr<ToastManager> toast_manager_;
    std::unique_ptr<TransactionManager> txn_manager_;

    // Destructor now automatic - no need to manually delete
};
```

**Changes required in database.cpp:**
```cpp
// CHANGE FROM:
buffer_pool_ = new BufferPool(this, 32);

// CHANGE TO:
buffer_pool_ = std::make_unique<BufferPool>(this, 32);

// All raw pointer -> takes care of itself
```

#### Recommendation: Use Smart Pointers (Option 2)

**Benefits:**
- Automatic cleanup, impossible to leak
- Exception-safe
- Modern C++ best practice
- Cleaner code

#### Step 4: Update All Construction Sites

Search for all places that assign to these pointers:
```bash
grep -rn "buffer_pool_ = \|page_manager_ = \|catalog_mgr_ = " src/core/database.cpp
```

Convert `new` to `std::make_unique`.

#### Step 5: Update All Dereferences

Change `buffer_pool_->method()` to... wait, it's the same! `unique_ptr` works identically.

**Estimated Time:** 2-3 hours

---

## Issue #4: TransactionManager Hardcoded Page Size (HIGH)

### Current State

**File:** `include/scratchbird/core/transaction_manager.h` (Lines 137-138)
**Severity:** HIGH
**Risk:** Buffer overflow/underflow with non-8KB pages

**Problem Code:**
```cpp
class TransactionManager {
    // ...

    // TIP (Transaction Information Page) structure
    static constexpr uint32_t TIP_ENTRIES_PER_PAGE =
        (8192 - sizeof(TIPPageHeader)) / sizeof(TIPEntry);  // ❌ HARDCODED 8192
};
```

**Why This is Wrong:**
- ScratchBird supports 8KB, 16KB, 32KB, 64KB, 128KB page sizes
- Hardcoding 8192 causes:
  - **16KB pages:** Only using half the page (waste)
  - **8KB pages:** Correct (by accident)
  - **32KB+ pages:** Only using 1/4 or less (massive waste)
- `TIPEntry` array would be wrong size at runtime

**Example Buffer Overflow Scenario:**
```cpp
// Database created with 16KB pages
Database db;
db.create("test.db", 16384);  // 16KB pages

// TransactionManager allocates TIP page
// Assumes TIP_ENTRIES_PER_PAGE entries fit
// But calculation was for 8KB!

// Later, code writes to entry 500
tip_entries[500] = ...;  // ❌ Out of bounds if array sized for 8KB
```

### Implementation Plan

#### Step 1: Make TIP Entries Dynamic

**File:** `include/scratchbird/core/transaction_manager.h`

```cpp
// CHANGE FROM:
static constexpr uint32_t TIP_ENTRIES_PER_PAGE =
    (8192 - sizeof(TIPPageHeader)) / sizeof(TIPEntry);

// CHANGE TO:
// Calculate TIP entries per page based on actual page size
static constexpr uint32_t calculateTIPEntriesPerPage(uint32_t page_size) {
    return (page_size - sizeof(TIPPageHeader)) / sizeof(TIPEntry);
}

// Will be set in constructor based on db->page_size()
uint32_t tip_entries_per_page_;
```

#### Step 2: Update Constructor

**File:** `src/core/transaction_manager.cpp`

```cpp
TransactionManager::TransactionManager(Database *db) : db_(db) {
    // Calculate based on actual page size
    tip_entries_per_page_ = calculateTIPEntriesPerPage(db->page_size());

    // Rest of initialization...
}
```

#### Step 3: Replace All Uses of TIP_ENTRIES_PER_PAGE

```bash
grep -rn "TIP_ENTRIES_PER_PAGE" src/core/transaction_manager.cpp
```

Replace hardcoded constant with `tip_entries_per_page_` instance variable.

#### Step 4: Add Validation

```cpp
// In constructor, after calculation
if (tip_entries_per_page_ < 10) {
    // Sanity check: at minimum must fit some entries
    throw std::runtime_error("Page size too small for TIP structure");
}
```

#### Step 5: Test with Multiple Page Sizes

```cpp
// Test 8KB
Database db1;
db1.create("test8k.db", 8192);
assert(db1.transaction_manager()->tip_entries_per_page_ == ...);

// Test 16KB
Database db2;
db2.create("test16k.db", 16384);
assert(db2.transaction_manager()->tip_entries_per_page_ == ...);

// Should be roughly double
```

**Estimated Time:** 2-3 hours

---

## Issue #5: HeapPage Bounds Checking (HIGH)

### Current State

**File:** `include/scratchbird/core/heap_page.h` (Line 24)
**Severity:** HIGH
**Risk:** Out-of-bounds memory access from corrupted data

**Problem Code:**
```cpp
struct ItemPointer {
    uint32_t offset;  // ❌ No validation
    uint16_t length;  // ❌ No validation
    uint16_t flags;

    auto isDeleted() const -> bool { return (flags & 0x0001) != 0; }
    void setDeleted(bool deleted) {
        if (deleted) flags |= 0x0001;
        else flags &= ~0x0001;
    }
};
```

**The Danger:**
```cpp
// Corrupted database or malicious file
ItemPointer *item = getItemPointer(slot);
uint8_t *data = page_data + item->offset;  // ❌ No bounds check

// If offset = 0xFFFFFFFF, this accesses arbitrary memory!
memcpy(output, data, item->length);  // ❌ Buffer overflow
```

**Current Validation (heap_page.cpp:232-239):**
```cpp
uint32_t offset = items[item_id].offset;
uint32_t length = items[item_id].length;

if (offset >= page_size_ || offset + length > page_size_) {
    SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                      "Item pointer extends beyond page boundary");
    return Status::PAGE_CORRUPT;
}
```

**Issue:** This validation is in `getTuple()`, but not consistently applied everywhere ItemPointer is used.

### Implementation Plan

#### Step 1: Add Validation Method to ItemPointer

**File:** `include/scratchbird/core/heap_page.h`

```cpp
struct ItemPointer {
    uint32_t offset;
    uint16_t length;
    uint16_t flags;

    auto isDeleted() const -> bool { return (flags & 0x0001) != 0; }
    void setDeleted(bool deleted) {
        if (deleted) flags |= 0x0001;
        else flags &= ~0x0001;
    }

    // NEW: Validation method
    [[nodiscard]] auto isValid(uint32_t page_size, uint32_t special_offset) const -> bool {
        // Check offset is within valid range
        if (offset < sizeof(PageHeader)) return false;
        if (offset >= special_offset) return false;

        // Check length doesn't overflow
        if (offset + length > special_offset) return false;

        // Check reasonable length (not a huge value that looks like corruption)
        if (length > page_size) return false;

        return true;
    }
};
```

#### Step 2: Use Validation Consistently

**File:** `src/core/heap_page.cpp`

Add validation at the start of every method that uses ItemPointer:

```cpp
Status HeapPage::getTuple(uint16_t item_id, const uint8_t **data_out,
                          uint32_t *size_out, ErrorContext *ctx) {
    // Existing checks
    if (item_id >= header()->item_count) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid item ID");
        return Status::INVALID_ARGUMENT;
    }

    ItemPointer *items = getItemArray();

    if (items[item_id].isDeleted()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Tuple is deleted");
        return Status::NOT_FOUND;
    }

    // NEW: Validate item pointer
    HeapPageSpecial *special = getSpecial();
    if (!items[item_id].isValid(page_size_, special->pd_special)) {
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                          "Item pointer is invalid or corrupted");
        return Status::PAGE_CORRUPT;
    }

    // Now safe to use offset/length
    *data_out = page_data_ + items[item_id].offset;
    *size_out = items[item_id].length;
    return Status::OK;
}
```

#### Step 3: Add Validation in All Access Points

Search for all uses of `items[item_id].offset` or `items[item_id].length`:
```bash
grep -rn "items\[.*\]\.offset\|items\[.*\]\.length" src/core/heap_page.cpp
```

Add validation before each access.

#### Step 4: Add Fuzz Testing

Create test that deliberately corrupts page data:
```cpp
TEST(HeapPageSecurityTest, RejectsCorruptedItemPointer) {
    HeapPage page(...);
    page.initialize(...);

    // Corrupt an item pointer
    ItemPointer *items = page.getItemArray();
    items[0].offset = 0xFFFFFFFF;  // Malicious offset

    const uint8_t *data;
    uint32_t size;
    ErrorContext ctx;

    Status s = page.getTuple(0, &data, &size, &ctx);
    EXPECT_EQ(s, Status::PAGE_CORRUPT);  // Should reject, not crash
}
```

**Estimated Time:** 3-4 hours

---

## Issue #6: Template ODR Violation (MEDIUM)

### Current State

**File:** `include/scratchbird/core/catalog_manager.h` (Lines 191-233)
**Severity:** MEDIUM
**Risk:** Linker errors with multiple translation units

**Problem Code:**
```cpp
// In header file (catalog_manager.h)
template <typename RecordType, typename InfoType>
auto CatalogManager::readRecordsFromHeapPage(  // ❌ Full implementation in header
    uint32_t page_id,
    std::vector<InfoType> &results,
    std::function<bool(const RecordType &)> filter,
    std::function<void(const RecordType &, InfoType &)> converter,
    ErrorContext *ctx) -> Status {

    // ... 42 lines of implementation ...

    return Status::OK;
}
```

**ODR (One Definition Rule) Violation:**
- Template implementations in headers are OK
- BUT this is a member function template with full implementation
- If multiple .cpp files include this header and use this template, linker may complain
- Should be marked `inline` to prevent ODR violations

### Implementation Plan

#### Step 1: Add inline Keyword

**File:** `include/scratchbird/core/catalog_manager.h`

```cpp
// CHANGE FROM:
template <typename RecordType, typename InfoType>
auto CatalogManager::readRecordsFromHeapPage(

// CHANGE TO:
template <typename RecordType, typename InfoType>
inline auto CatalogManager::readRecordsFromHeapPage(  // Added 'inline'
```

**Why inline?**
- Tells linker multiple definitions are OK (they're the same)
- Standard practice for template implementations in headers
- No performance impact (templates are implicitly inline for instantiations)

#### Alternative: Move to .cpp (More Complex)

Only possible if all template instantiations are known:

```cpp
// catalog_manager.h
template <typename RecordType, typename InfoType>
auto CatalogManager::readRecordsFromHeapPage(...) -> Status;  // Declaration only

// catalog_manager.cpp
template <typename RecordType, typename InfoType>
auto CatalogManager::readRecordsFromHeapPage(...) -> Status {
    // Implementation
}

// Explicit instantiations for known types
template auto CatalogManager::readRecordsFromHeapPage<SchemaRecord, SchemaInfo>(...);
template auto CatalogManager::readRecordsFromHeapPage<TableRecord, TableInfo>(...);
// ... etc
```

**Recommendation:** Just add `inline` - it's simpler and correct.

#### Step 2: Search for Similar Issues

```bash
grep -rn "^template" include/ | grep -v "inline"
```

Check if other template implementations need `inline`.

**Estimated Time:** 30 minutes

---

## Implementation Order

### Phase 1: Critical Fixes (Must Do First)
**Estimated: 4-5 hours**

1. ✅ **ErrorContext Rule of Five** (Issue #2) - 1 hour
   - Prevents crashes and memory corruption
   - Blocking all error handling

2. ✅ **B-Tree Static Methods** (Issue #1) - 30 minutes
   - Simple fix, prevents compilation issues
   - Required for B-tree to ever work

3. ✅ **Database Destructor** (Issue #3) - 2-3 hours
   - Prevents memory leaks
   - Move to smart pointers (safer long-term)

### Phase 2: High-Priority Fixes (Should Do Next)
**Estimated: 5-7 hours**

4. ✅ **TransactionManager Page Size** (Issue #4) - 2-3 hours
   - Prevents buffer overflows
   - Required for multi-page-size support

5. ✅ **HeapPage Bounds Checking** (Issue #5) - 3-4 hours
   - Security critical
   - Prevents exploitation of corrupted data

### Phase 3: Code Quality (Can Defer)
**Estimated: 30 minutes**

6. ✅ **Template ODR** (Issue #6) - 30 minutes
   - Minor issue, easy fix
   - Just add `inline` keyword

---

## Testing Strategy

### Unit Tests Needed

```cpp
// test_error_context.cpp
TEST(ErrorContextTest, CannotCopy) {
    ErrorContext ctx1;
    // ErrorContext ctx2 = ctx1;  // Should not compile
}

// test_database.cpp
TEST(DatabaseTest, NoMemoryLeaksOnDestruction) {
    {
        Database db;
        db.create("test.db", 8192);
        db.open("test.db");
    }  // Destructor called
    // Use valgrind or ASAN to verify no leaks
}

// test_transaction_manager.cpp
TEST(TransactionManagerTest, SupportsMultiplePageSizes) {
    Database db8k, db16k, db32k;
    db8k.create("test8k.db", 8192);
    db16k.create("test16k.db", 16384);
    db32k.create("test32k.db", 32768);

    // Verify TIP calculations are different
    EXPECT_NE(db8k.transaction_manager()->tip_entries_per_page_,
              db16k.transaction_manager()->tip_entries_per_page_);
}

// test_heap_page_security.cpp
TEST(HeapPageTest, RejectsCorruptedPointers) {
    // Test malicious offsets/lengths don't crash
}
```

### Integration Tests

```cpp
TEST(IntegrationTest, CreateAndQueryWithErrorHandling) {
    Database db;
    ErrorContext ctx;

    // Test ErrorContext is not accidentally copied
    auto status = db.create("test.db", 8192, &ctx);
    EXPECT_EQ(status, Status::OK);
}
```

### Memory Safety Tools

```bash
# Run with AddressSanitizer
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address" ..
./scratchbird_tests

# Run with Valgrind
valgrind --leak-check=full ./scratchbird_tests

# Run with Thread Sanitizer (for future multi-threading)
cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread" ..
./scratchbird_tests
```

---

## Success Criteria

### Must Pass:
- ✅ All code compiles with 0 errors
- ✅ All existing tests pass
- ✅ New unit tests for each fix pass
- ✅ Valgrind reports 0 memory leaks
- ✅ AddressSanitizer reports 0 errors
- ✅ No ODR violations with multiple translation units

### Documentation:
- ✅ Update this plan as work progresses
- ✅ Add comments explaining fixes in code
- ✅ Create commit messages referencing issue numbers

---

## Risk Assessment

### Low Risk:
- Issue #1 (B-Tree) - Simple declaration fix
- Issue #6 (Template) - Just add inline

### Medium Risk:
- Issue #2 (ErrorContext) - Need to verify no code copies it
- Issue #4 (TIP) - Need to update all usage sites

### High Risk:
- Issue #3 (Database) - Converting to smart pointers touches many files
- Issue #5 (HeapPage) - Adding validation changes performance slightly

---

## Rollback Plan

If any fix causes problems:

1. Each fix should be a separate commit
2. Can revert individual commits: `git revert <commit-hash>`
3. Branch name: `critical-fixes-20250930`
4. Can always return to: `build-fix-20250930`

---

## Next Steps

1. Create branch: `git checkout -b critical-fixes-20250930`
2. Start with Phase 1, Issue #2 (ErrorContext) - highest risk
3. Commit after each fix with clear message
4. Run tests after each fix
5. Document any unexpected findings

**Start Date:** 2025-09-30
**Target Completion:** 2025-10-01
**Status:** READY TO BEGIN
