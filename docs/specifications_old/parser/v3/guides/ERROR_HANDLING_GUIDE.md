# ScratchBird Error Handling Guide

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Document Version**: 1.0
**Last Updated**: October 17, 2025
**Status**: Production Ready

---

## Table of Contents

1. [Overview](#overview)
2. [Error Handling Philosophy](#error-handling-philosophy)
3. [Status Codes](#status-codes)
4. [Error Context](#error-context)
5. [Exception Safety](#exception-safety)
6. [Error Handling Patterns](#error-handling-patterns)
7. [Best Practices](#best-practices)
8. [Common Pitfalls](#common-pitfalls)
9. [Testing Error Paths](#testing-error-paths)
10. [Examples](#examples)

---

## Overview

This document defines the error handling strategy for ScratchBird Database. Proper error handling is critical for:
- **Data integrity**: Ensure errors don't corrupt database state
- **Resource safety**: Prevent leaks when errors occur
- **Debugging**: Provide clear diagnostic information
- **Recovery**: Enable graceful degradation and recovery

### Key Principles

1. **Errors are expected**: Plan for failure, don't ignore it
2. **Context is king**: Always provide actionable error messages
3. **Resources are precious**: Clean up on all paths (success and error)
4. **Exceptions for allocation**: Use exceptions for `std::bad_alloc`, Status for everything else
5. **Never corrupt on error**: Database must remain consistent

---

## Error Handling Philosophy

### Two-Tier Error System

ScratchBird uses a **dual error handling approach**:

1. **Status codes** (primary): For all database operations
   - Return values indicating success/failure
   - Error context for diagnostic information
   - No exceptions in normal control flow

2. **Exceptions** (rare): Only for allocation failures
   - `std::bad_alloc` when memory allocation fails
   - Exception safety guarantees at all levels
   - RAII ensures cleanup

### Why This Approach?

**Advantages**:
- **Performance**: No exception overhead in normal operation
- **Predictability**: Error paths are explicit and testable
- **Control**: Caller can decide how to handle errors
- **Debugging**: Stack traces for allocation failures, contexts for logic errors

**When NOT to use exceptions**:
- I/O errors (disk full, permission denied)
- Logical errors (key not found, duplicate key)
- Resource exhaustion (buffer pool full, lock timeout)
- Concurrent conflicts (deadlock detected, serialization failure)

**When TO use exceptions**:
- Memory allocation failure (`std::bad_alloc`)
- Unrecoverable program errors (assert failures in debug builds)

---

## Status Codes

### Status Enum

Defined in `include/scratchbird/core/status.h`:

```cpp
enum class Status : uint32_t {
    // Success
    OK = 0,

    // General errors (1000-1099)
    ERROR = 1000,
    NOT_IMPLEMENTED = 1001,
    INVALID_ARGUMENT = 1002,
    OUT_OF_RANGE = 1003,

    // I/O errors (1100-1199)
    IO_ERROR = 1100,
    FILE_NOT_FOUND = 1101,
    PERMISSION_DENIED = 1102,
    DISK_FULL = 1103,

    // Database errors (1200-1299)
    DB_NOT_FOUND = 1200,
    DB_ALREADY_EXISTS = 1201,
    DB_CORRUPTED = 1202,
    DB_VERSION_MISMATCH = 1203,

    // Storage errors (1300-1399)
    PAGE_NOT_FOUND = 1300,
    PAGE_CORRUPTED = 1301,
    CHECKSUM_MISMATCH = 1302,

    // Transaction errors (1400-1499)
    TRANSACTION_ABORTED = 1400,
    TRANSACTION_CONFLICT = 1401,
    DEADLOCK_DETECTED = 1402,
    SERIALIZATION_FAILURE = 1403,

    // Resource errors (1500-1599)
    OUT_OF_MEMORY = 1500,
    BUFFER_POOL_EXHAUSTED = 1501,
    LOCK_TIMEOUT = 1502,

    // Index errors (1600-1699)
    KEY_NOT_FOUND = 1600,
    DUPLICATE_KEY = 1601,

    // Concurrency errors (1700-1799)
    BUSY = 1700,
    LOCKED = 1701,

    // Integrity errors (2000-2099)
    CONSTRAINT_VIOLATION = 2000,
    FOREIGN_KEY_VIOLATION = 2001,
    UNIQUE_VIOLATION = 2002,
    INDEX_CORRUPTED = 2003,

    // Internal errors (9000-9099)
    INTERNAL_ERROR = 9000,
    ASSERTION_FAILED = 9001
};
```

### Status Code Categories

| Range | Category | Recovery |
|-------|----------|----------|
| 0 | Success | N/A |
| 1000-1099 | General errors | Retry or fail |
| 1100-1199 | I/O errors | Retry, check disk |
| 1200-1299 | Database errors | Check db, may need recovery |
| 1300-1399 | Storage errors | Corruption detected, recovery needed |
| 1400-1499 | Transaction errors | Retry transaction |
| 1500-1599 | Resource errors | Wait and retry |
| 1600-1699 | Index errors | Expected (e.g., key not found) |
| 1700-1799 | Concurrency errors | Retry after delay |
| 2000-2099 | Integrity errors | User error or corruption |
| 9000-9099 | Internal errors | Bug in ScratchBird |

---

## Error Context

### ErrorContext Structure

Defined in `include/scratchbird/core/error_context.h`:

```cpp
struct ErrorContext {
    std::string message;      // Human-readable error description
    std::string file;         // Source file where error occurred
    int line;                 // Line number where error occurred
    std::string function;     // Function name where error occurred

    // Constructors
    ErrorContext();
    ErrorContext(const std::string& msg);
    ErrorContext(const std::string& msg, const char* f, int l, const char* func);

    // Helpers
    void clear();
    bool hasError() const;
    std::string toString() const;
};
```

### Setting Error Context

**Basic usage**:
```cpp
Status someFunction(ErrorContext* ctx) {
    if (error_condition) {
        if (ctx) {
            ctx->message = "Failed to do something: reason";
            ctx->file = __FILE__;
            ctx->line = __LINE__;
            ctx->function = __FUNCTION__;
        }
        return Status::ERROR;
    }
    return Status::OK;
}
```

**Macro for convenience** (consider adding):
```cpp
#define SET_ERROR(ctx, msg) do { \
    if (ctx) { \
        ctx->message = (msg); \
        ctx->file = __FILE__; \
        ctx->line = __LINE__; \
        ctx->function = __FUNCTION__; \
    } \
} while (0)

// Usage:
SET_ERROR(ctx, "Failed to allocate page: out of memory");
return Status::OUT_OF_MEMORY;
```

### Error Context Best Practices

1. **Always check for null**: `if (ctx)` before setting
2. **Be specific**: Include relevant details (page ID, key, etc.)
3. **Include cause**: If wrapping another error, mention it
4. **Use consistent format**: "Failed to <action>: <reason>"

**Good example**:
```cpp
ctx->message = "Failed to pin page 12345: buffer pool exhausted (size=1000, used=1000)";
```

**Bad example**:
```cpp
ctx->message = "Error";  // Too vague
```

---

## Exception Safety

### Exception Safety Levels

ScratchBird guarantees different levels of exception safety depending on the operation:

1. **No-throw guarantee**: Never throws exceptions
   - Destructors
   - Swap operations
   - Move constructors/assignments
   - Cleanup functions

2. **Strong guarantee**: Either succeeds or leaves state unchanged
   - insertTuple (rolls back on failure)
   - updateTuple (old value restored on failure)
   - Transaction commit (all or nothing)

3. **Basic guarantee**: No leaks, invariants maintained, but state may change
   - Most database operations
   - Buffer pool operations
   - Index operations

4. **No guarantee**: May leak or corrupt (avoid!)
   - None in ScratchBird (all code must have at least basic guarantee)

### Common Exception Sources

**Only `std::bad_alloc` is expected**:
```cpp
// Vector resize can throw
std::vector<uint8_t> data;
try {
    data.resize(large_size);  // May throw std::bad_alloc
} catch (const std::bad_alloc&) {
    if (ctx) {
        ctx->message = "Failed to allocate memory for data buffer";
    }
    return Status::OUT_OF_MEMORY;
}
```

**String operations can throw**:
```cpp
try {
    std::string path = base_path + "/" + filename;  // May throw
} catch (const std::bad_alloc&) {
    if (ctx) {
        ctx->message = "Failed to allocate memory for path string";
    }
    return Status::OUT_OF_MEMORY;
}
```

**Container operations can throw**:
```cpp
try {
    map.insert(std::make_pair(key, value));  // May throw
} catch (const std::bad_alloc&) {
    if (ctx) {
        ctx->message = "Failed to insert into map: out of memory";
    }
    return Status::OUT_OF_MEMORY;
}
```

### Exception Safety Patterns

**Pattern 1: Try-Catch for Allocations**
```cpp
Status allocateBuffer(uint32_t size, std::vector<uint8_t>& buffer, ErrorContext* ctx) {
    try {
        buffer.resize(size);
    } catch (const std::bad_alloc&) {
        if (ctx) {
            ctx->message = "Failed to allocate buffer of size " + std::to_string(size);
        }
        return Status::OUT_OF_MEMORY;
    }
    return Status::OK;
}
```

**Pattern 2: RAII for Resource Cleanup**
```cpp
Status processPage(uint32_t page_id, ErrorContext* ctx) {
    void* buffer = nullptr;

    // Pin page (acquire resource)
    Status s = buffer_pool_->pinPage(page_id, &buffer, ctx);
    if (s != Status::OK) {
        return s;
    }

    // RAII wrapper ensures unpin even on exception
    auto unpin_guard = [&]() {
        buffer_pool_->unpinPage(page_id, false, nullptr);
    };
    std::unique_ptr<void, decltype(unpin_guard)> guard(buffer, unpin_guard);

    // Work with buffer - if exception occurs, guard unpins automatically
    // ...

    return Status::OK;
}
```

**Pattern 3: Two-Phase Commit**
```cpp
Status updateWithRollback(const Key& key, const Value& newValue, ErrorContext* ctx) {
    // Phase 1: Save old state
    Value oldValue;
    Status s = getValue(key, oldValue, ctx);
    if (s != Status::OK) {
        return s;
    }

    // Phase 2: Try update
    s = setValue(key, newValue, ctx);
    if (s != Status::OK) {
        // Rollback: restore old value
        setValue(key, oldValue, nullptr);  // Ignore errors in rollback
        return s;
    }

    return Status::OK;
}
```

---

## Error Handling Patterns

### Pattern 1: Check Status, Propagate Errors

**Most common pattern**:
```cpp
Status outerFunction(ErrorContext* ctx) {
    Status s = innerFunction(ctx);
    if (s != Status::OK) {
        // Error context already set by innerFunction
        return s;
    }

    // Continue with success case
    return Status::OK;
}
```

### Pattern 2: Check Status, Add Context

**When outer function has more context**:
```cpp
Status processTuple(uint32_t page_id, uint16_t slot, ErrorContext* ctx) {
    Status s = pinPage(page_id, ctx);
    if (s != Status::OK) {
        // Add context about what we were doing
        if (ctx) {
            ctx->message = "Failed to process tuple at page=" + std::to_string(page_id) +
                          " slot=" + std::to_string(slot) + ": " + ctx->message;
        }
        return s;
    }
    // ...
}
```

### Pattern 3: Multiple Error Paths

**When multiple operations can fail**:
```cpp
Status complexOperation(ErrorContext* ctx) {
    // Step 1
    Status s = step1(ctx);
    if (s != Status::OK) {
        return s;  // Early return on error
    }

    // Step 2
    s = step2(ctx);
    if (s != Status::OK) {
        cleanup1();  // Clean up step 1
        return s;
    }

    // Step 3
    s = step3(ctx);
    if (s != Status::OK) {
        cleanup2();  // Clean up step 2
        cleanup1();  // Clean up step 1
        return s;
    }

    return Status::OK;
}
```

### Pattern 4: Graceful Degradation

**When partial success is acceptable**:
```cpp
Status updateIndexes(const std::vector<Index*>& indexes, const Tuple& tuple, ErrorContext* ctx) {
    bool any_failed = false;
    std::string error_msg;

    for (Index* idx : indexes) {
        ErrorContext idx_ctx;
        Status s = idx->insert(tuple, &idx_ctx);
        if (s != Status::OK) {
            any_failed = true;
            error_msg += idx->name() + ": " + idx_ctx.message + "; ";
            // Continue to try other indexes
        }
    }

    if (any_failed) {
        if (ctx) {
            ctx->message = "Some indexes failed: " + error_msg;
        }
        return Status::INDEX_CORRUPTED;  // Partial failure
    }

    return Status::OK;
}
```

### Pattern 5: Retry with Backoff

**When transient errors are expected**:
```cpp
Status acquireLockWithRetry(uint32_t page_id, LockMode mode, ErrorContext* ctx) {
    const int MAX_RETRIES = 3;
    const int BACKOFF_MS = 10;

    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
        Status s = acquireLock(page_id, mode, ctx);
        if (s == Status::OK) {
            return Status::OK;
        }

        if (s == Status::LOCK_TIMEOUT || s == Status::BUSY) {
            // Transient error, retry
            std::this_thread::sleep_for(std::chrono::milliseconds(BACKOFF_MS * (attempt + 1)));
            continue;
        }

        // Non-transient error, fail immediately
        return s;
    }

    if (ctx) {
        ctx->message = "Failed to acquire lock after " + std::to_string(MAX_RETRIES) + " retries";
    }
    return Status::LOCK_TIMEOUT;
}
```

---

## Best Practices

### DO

1. **Always check Status return values**
   ```cpp
   Status s = someFunction(ctx);
   if (s != Status::OK) {
       // Handle error
   }
   ```

2. **Set meaningful error contexts**
   ```cpp
   if (ctx) {
       ctx->message = "Failed to read page " + std::to_string(page_id) +
                     ": I/O error at offset " + std::to_string(offset);
   }
   ```

3. **Clean up resources on all paths**
   ```cpp
   if (s != Status::OK) {
       unpinPage(page_id);  // Cleanup before return
       return s;
   }
   ```

4. **Use RAII for automatic cleanup**
   ```cpp
   std::unique_ptr<uint8_t[]> buffer(new uint8_t[size]);
   // Automatically freed on scope exit, even on exception
   ```

5. **Wrap allocation-heavy operations in try-catch**
   ```cpp
   try {
       vector.resize(large_size);
   } catch (const std::bad_alloc&) {
       // Handle OOM
   }
   ```

### DON'T

1. **Never ignore Status return values**
   ```cpp
   someFunction(ctx);  // BAD: Ignored return value
   ```

2. **Never leave error context empty**
   ```cpp
   return Status::ERROR;  // BAD: No context set
   ```

3. **Never leak resources on error**
   ```cpp
   pinPage(page_id);
   if (error) {
       return Status::ERROR;  // BAD: Page still pinned!
   }
   ```

4. **Never use exceptions for control flow**
   ```cpp
   try {
       // Some database operation
   } catch (...) {
       // BAD: Exceptions should only be std::bad_alloc
   }
   ```

5. **Never suppress errors silently**
   ```cpp
   Status s = someFunction(nullptr);  // BAD: Passing null ctx to hide errors
   ```

---

## Common Pitfalls

### Pitfall 1: Forgetting to Unpin Pages

**Problem**:
```cpp
Status processPage(uint32_t page_id, ErrorContext* ctx) {
    void* buffer = nullptr;
    Status s = buffer_pool_->pinPage(page_id, &buffer, ctx);
    if (s != Status::OK) {
        return s;
    }

    s = doWork(buffer, ctx);
    if (s != Status::OK) {
        return s;  // BUG: Page still pinned!
    }

    buffer_pool_->unpinPage(page_id, false, ctx);
    return Status::OK;
}
```

**Solution**: Use RAII or ensure all paths unpin
```cpp
Status processPage(uint32_t page_id, ErrorContext* ctx) {
    void* buffer = nullptr;
    Status s = buffer_pool_->pinPage(page_id, &buffer, ctx);
    if (s != Status::OK) {
        return s;
    }

    // Work with buffer
    Status result = doWork(buffer, ctx);

    // Always unpin (even on error)
    buffer_pool_->unpinPage(page_id, false, nullptr);

    return result;
}
```

### Pitfall 2: Not Checking for Null Pointers

**Problem**:
```cpp
Status getValue(const Key& key, Value* value, ErrorContext* ctx) {
    *value = map_[key];  // BUG: Crashes if value is null
    return Status::OK;
}
```

**Solution**: Always validate pointers
```cpp
Status getValue(const Key& key, Value* value, ErrorContext* ctx) {
    if (!value) {
        if (ctx) {
            ctx->message = "getValue: value pointer is null";
        }
        return Status::INVALID_ARGUMENT;
    }

    auto it = map_.find(key);
    if (it == map_.end()) {
        if (ctx) {
            ctx->message = "Key not found: " + key.toString();
        }
        return Status::KEY_NOT_FOUND;
    }

    *value = it->second;
    return Status::OK;
}
```

### Pitfall 3: Overwriting Error Context

**Problem**:
```cpp
Status outer(ErrorContext* ctx) {
    Status s = inner(ctx);  // Sets ctx->message = "Inner error"
    if (s != Status::OK) {
        if (ctx) {
            ctx->message = "Outer error";  // BUG: Loses inner context!
        }
        return s;
    }
}
```

**Solution**: Append to context, don't overwrite
```cpp
Status outer(ErrorContext* ctx) {
    Status s = inner(ctx);
    if (s != Status::OK) {
        if (ctx && !ctx->message.empty()) {
            ctx->message = "Outer error: " + ctx->message;  // Preserve inner context
        }
        return s;
    }
}
```

---

## Testing Error Paths

### Unit Test Pattern

```cpp
TEST(ErrorHandlingTest, HandleBufferPoolExhaustion) {
    ErrorContext ctx;

    // Create small buffer pool to force exhaustion
    BufferPool pool(10);  // Only 10 pages

    // Allocate and pin all pages
    std::vector<uint32_t> page_ids;
    for (int i = 0; i < 10; ++i) {
        uint32_t page_id;
        Status s = pool.allocatePage(&page_id, &ctx);
        ASSERT_EQ(s, Status::OK);
        page_ids.push_back(page_id);

        void* buffer;
        s = pool.pinPage(page_id, &buffer, &ctx);
        ASSERT_EQ(s, Status::OK);
    }

    // Try to allocate one more - should fail
    uint32_t page_id;
    Status s = pool.allocatePage(&page_id, &ctx);
    EXPECT_EQ(s, Status::BUFFER_POOL_EXHAUSTED);
    EXPECT_FALSE(ctx.message.empty());
    EXPECT_NE(ctx.message.find("exhausted"), std::string::npos);

    // Cleanup
    for (uint32_t pid : page_ids) {
        pool.unpinPage(pid, false, nullptr);
    }
}
```

### Integration Test Pattern

```cpp
TEST(ErrorHandlingIntegrationTest, TransactionRollbackOnError) {
    Database db;
    ErrorContext ctx;

    ASSERT_EQ(db.open("test.db", &ctx), Status::OK);

    TransactionManager* txn_mgr = db.transaction_manager();
    uint32_t xid;

    // Begin transaction
    ASSERT_EQ(txn_mgr->beginTransaction(xid, &ctx), Status::OK);

    // Insert some tuples
    for (int i = 0; i < 10; ++i) {
        Tuple tuple = createTuple(i);
        Status s = db.insertTuple("table1", tuple, &ctx);
        ASSERT_EQ(s, Status::OK);
    }

    // Simulate error during transaction
    // (e.g., constraint violation, disk full, etc.)
    Tuple duplicate = createTuple(5);  // Duplicate key
    Status s = db.insertTuple("table1", duplicate, &ctx);
    EXPECT_NE(s, Status::OK);

    // Rollback transaction
    ASSERT_EQ(txn_mgr->rollbackTransaction(xid, &ctx), Status::OK);

    // Verify all tuples were rolled back
    for (int i = 0; i < 10; ++i) {
        Tuple result;
        s = db.getTuple("table1", i, result, &ctx);
        EXPECT_EQ(s, Status::KEY_NOT_FOUND);  // Should not exist
    }
}
```

---

## Examples

### Example 1: File I/O Error Handling

```cpp
Status PageManager::loadFSM(ErrorContext* ctx) {
    if (!db_) {
        if (ctx) {
            ctx->message = "Database not initialized";
        }
        return Status::INTERNAL_ERROR;
    }

    std::vector<uint8_t> buffer;
    try {
        buffer.resize(page_size_);
    } catch (const std::bad_alloc&) {
        if (ctx) {
            ctx->message = "Failed to allocate FSM buffer of size " +
                          std::to_string(page_size_);
        }
        return Status::OUT_OF_MEMORY;
    }

    ssize_t bytes_read = db_->read(1, buffer.data(), page_size_);
    if (bytes_read < 0) {
        if (ctx) {
            ctx->message = "Failed to read FSM from disk: I/O error (errno=" +
                          std::to_string(errno) + ")";
        }
        return Status::IO_ERROR;
    }

    if (static_cast<size_t>(bytes_read) != page_size_) {
        if (ctx) {
            ctx->message = "Partial FSM read: expected " + std::to_string(page_size_) +
                          " bytes, got " + std::to_string(bytes_read);
        }
        return Status::DB_CORRUPTED;
    }

    // Parse FSM from buffer
    Status s = parseFSM(buffer.data(), buffer.size(), ctx);
    if (s != Status::OK) {
        // Error context already set by parseFSM
        return s;
    }

    return Status::OK;
}
```

### Example 2: Transaction Error Handling

```cpp
Status TransactionManager::commitTransaction(uint32_t xid, ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Find transaction
    auto it = active_transactions_.find(xid);
    if (it == active_transactions_.end()) {
        if (ctx) {
            ctx->message = "Transaction " + std::to_string(xid) + " not found";
        }
        return Status::TRANSACTION_ABORTED;
    }

    Transaction* txn = it->second;

    // Check transaction state
    if (txn->state != TransactionState::IN_PROGRESS) {
        if (ctx) {
            ctx->message = "Transaction " + std::to_string(xid) +
                          " is not in progress (state=" +
                          std::to_string(static_cast<int>(txn->state)) + ")";
        }
        return Status::TRANSACTION_ABORTED;
    }

    // Flush dirty pages to disk
    Status s = buffer_pool_->flushAll(ctx);
    if (s != Status::OK) {
        // Flush failed - abort transaction
        if (ctx && !ctx->message.empty()) {
            ctx->message = "Failed to commit transaction " + std::to_string(xid) +
                          ": " + ctx->message;
        }

        txn->state = TransactionState::ABORTED;
        return Status::TRANSACTION_ABORTED;
    }

    // Mark transaction as committed
    txn->state = TransactionState::COMMITTED;
    txn->commit_time = std::chrono::system_clock::now();

    // Remove from active set
    active_transactions_.erase(it);

    return Status::OK;
}
```

### Example 3: Index Corruption Handling

```cpp
Status BTree::insert(const Key& key, const RID& rid, ErrorContext* ctx) {
    // Validate inputs
    if (key.size() == 0) {
        if (ctx) {
            ctx->message = "Cannot insert empty key";
        }
        return Status::INVALID_ARGUMENT;
    }

    // Find leaf page for key
    uint32_t leaf_page_id;
    Status s = findLeaf(key, leaf_page_id, ctx);
    if (s != Status::OK) {
        return s;
    }

    // Pin leaf page
    void* buffer = nullptr;
    s = buffer_pool_->pinPage(leaf_page_id, &buffer, ctx);
    if (s != Status::OK) {
        return s;
    }

    // Parse leaf page
    BTreeLeafPage leaf;
    s = leaf.deserialize(buffer, page_size_, ctx);
    if (s != Status::OK) {
        buffer_pool_->unpinPage(leaf_page_id, false, ctx);

        if (ctx && !ctx->message.empty()) {
            ctx->message = "BTree leaf page " + std::to_string(leaf_page_id) +
                          " corrupted: " + ctx->message;
        }
        return Status::INDEX_CORRUPTED;
    }

    // Check for duplicate key
    if (leaf.hasKey(key)) {
        buffer_pool_->unpinPage(leaf_page_id, false, ctx);

        if (ctx) {
            ctx->message = "Duplicate key in index: " + key.toString();
        }
        return Status::DUPLICATE_KEY;
    }

    // Insert into leaf (may split)
    bool did_split = false;
    s = leaf.insertKey(key, rid, did_split, ctx);
    if (s != Status::OK) {
        buffer_pool_->unpinPage(leaf_page_id, false, ctx);
        return s;
    }

    // Serialize and unpin
    leaf.serialize(buffer, page_size_);
    buffer_pool_->unpinPage(leaf_page_id, true, ctx);  // Mark dirty

    // Handle split if necessary
    if (did_split) {
        s = handleLeafSplit(leaf_page_id, ctx);
        if (s != Status::OK) {
            // Split failed - index may be corrupted
            if (ctx && !ctx->message.empty()) {
                ctx->message = "BTree split failed: " + ctx->message;
            }
            return Status::INDEX_CORRUPTED;
        }
    }

    return Status::OK;
}
```

---

## Conclusion

Proper error handling is critical for database reliability. Follow these guidelines:

1. **Always use Status codes** for expected errors
2. **Set meaningful error contexts** with file/line information
3. **Clean up resources** on all code paths
4. **Use exceptions only for `std::bad_alloc`**
5. **Test error paths** as thoroughly as success paths
6. **Document error conditions** in function comments

**Remember**: Errors are not exceptional - they are expected. Plan for them, handle them gracefully, and provide clear diagnostics to help debugging.

---

**Document Status**: ✅ Production Ready
**Maintainer**: ScratchBird Development Team
**Last Review**: October 17, 2025
