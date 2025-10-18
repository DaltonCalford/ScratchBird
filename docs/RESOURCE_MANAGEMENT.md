# ScratchBird Resource Management Guide

**Document Version**: 1.0
**Last Updated**: October 17, 2025
**Status**: Production Ready

---

## Overview

This document defines resource management patterns for ScratchBird Database. Proper resource management prevents leaks, ensures data integrity, and maintains system stability.

**Critical Resources in ScratchBird**:
1. **Buffer Pool Pages** (pin/unpin)
2. **Locks** (acquire/release)
3. **Transactions** (begin/commit/rollback)
4. **File Handles** (open/close)
5. **Memory Allocations** (new/delete, malloc/free)

---

## RAII Philosophy

### Resource Acquisition Is Initialization

**Core Principle**: Resource lifetime bound to object lifetime

**Benefits**:
- Automatic cleanup (no manual tracking)
- Exception-safe (destructor always runs)
- Leak-proof (compiler enforces cleanup)

**Example**:
```cpp
{
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[size]);
    // Use buffer
} // Automatic cleanup - no leak possible
```

---

## Buffer Pool Pin/Unpin Pattern

### The Pin/Unpin Contract

**Pin**: Acquire reference to page in memory
**Unpin**: Release reference (page may be evicted)

### Pattern 1: Basic Pin/Unpin

```cpp
Status processPage(uint32_t page_id, ErrorContext* ctx) {
    void* buffer = nullptr;

    // Pin page
    Status s = buffer_pool_->pinPage(page_id, &buffer, ctx);
    if (s != Status::OK) {
        return s;  // No unpin needed - pin failed
    }

    // Use page data
    s = doWork(buffer, ctx);

    // Always unpin
    buffer_pool_->unpinPage(page_id, false, nullptr);

    return s;
}
```

### Pattern 2: Pin/Unpin with Error Handling

```cpp
Status processPageSafely(uint32_t page_id, ErrorContext* ctx) {
    void* buffer = nullptr;

    // Pin page
    Status s = buffer_pool_->pinPage(page_id, &buffer, ctx);
    if (s != Status::OK) {
        return s;
    }

    // Work with page - may fail
    s = step1(buffer, ctx);
    if (s != Status::OK) {
        buffer_pool_->unpinPage(page_id, false, nullptr);  // Cleanup
        return s;
    }

    s = step2(buffer, ctx);
    if (s != Status::OK) {
        buffer_pool_->unpinPage(page_id, false, nullptr);  // Cleanup
        return s;
    }

    // Mark dirty if modified
    buffer_pool_->unpinPage(page_id, true, nullptr);
    return Status::OK;
}
```

### Pattern 3: RAII Pin Guard

```cpp
class PinGuard {
    BufferPool* pool_;
    uint32_t page_id_;
    bool dirty_;
    bool released_;

public:
    PinGuard(BufferPool* pool, uint32_t page_id, bool dirty = false)
        : pool_(pool), page_id_(page_id), dirty_(dirty), released_(false) {}

    ~PinGuard() {
        if (!released_) {
            pool_->unpinPage(page_id_, dirty_, nullptr);
        }
    }

    void setDirty() { dirty_ = true; }
    void release() { released_ = true; }

    // Disable copy
    PinGuard(const PinGuard&) = delete;
    PinGuard& operator=(const PinGuard&) = delete;
};

// Usage
Status processPage(uint32_t page_id, ErrorContext* ctx) {
    void* buffer = nullptr;
    Status s = buffer_pool_->pinPage(page_id, &buffer, ctx);
    if (s != Status::OK) {
        return s;
    }

    PinGuard guard(buffer_pool_, page_id);

    // Work with page - automatic unpin on return/exception
    s = doWork(buffer, ctx);
    if (s != Status::OK) {
        return s;  // Guard unpins automatically
    }

    guard.setDirty();  // Mark dirty before unpin
    return Status::OK;
}
```

### Pattern 4: Multiple Pages

```cpp
Status processMultiplePages(const std::vector<uint32_t>& page_ids, ErrorContext* ctx) {
    std::vector<void*> buffers;
    std::vector<uint32_t> pinned_pages;

    // Pin all pages
    for (uint32_t page_id : page_ids) {
        void* buffer = nullptr;
        Status s = buffer_pool_->pinPage(page_id, &buffer, ctx);
        if (s != Status::OK) {
            // Unpin already pinned pages
            for (uint32_t pinned_id : pinned_pages) {
                buffer_pool_->unpinPage(pinned_id, false, nullptr);
            }
            return s;
        }
        buffers.push_back(buffer);
        pinned_pages.push_back(page_id);
    }

    // Work with all pages
    Status result = doWorkWithPages(buffers, ctx);

    // Unpin all pages
    for (uint32_t page_id : pinned_pages) {
        buffer_pool_->unpinPage(page_id, false, nullptr);
    }

    return result;
}
```

---

## Lock Acquire/Release Pattern

### Lock Contract

**Acquire**: Obtain exclusive or shared access
**Release**: Relinquish access

### Pattern 1: RAII Lock (Recommended)

```cpp
Status modifyData(ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Critical section - automatic unlock on scope exit
    data_.modify();

    return Status::OK;
    // Lock automatically released here
}
```

### Pattern 2: Manual Lock (Avoid)

```cpp
Status modifyData(ErrorContext* ctx) {
    mutex_.lock();

    // Work - but what if this throws?
    data_.modify();  // Exception = lock never released!

    mutex_.unlock();
    return Status::OK;
}
```

### Pattern 3: Multiple Locks (Lock Ordering)

```cpp
Status transferBetweenAccounts(Account& from, Account& to, ErrorContext* ctx) {
    // IMPORTANT: Always lock in consistent order (by address or ID)
    std::mutex* first_lock = &from.mutex_;
    std::mutex* second_lock = &to.mutex_;

    if (first_lock > second_lock) {
        std::swap(first_lock, second_lock);
    }

    std::lock_guard<std::mutex> lock1(*first_lock);
    std::lock_guard<std::mutex> lock2(*second_lock);

    // Both locked - safe to transfer
    from.balance -= amount;
    to.balance += amount;

    return Status::OK;
}
```

**See LOCKING_PROTOCOL.md for complete lock hierarchy**

---

## Transaction Begin/Commit/Rollback Pattern

### Transaction Lifecycle

1. **Begin**: Allocate transaction ID
2. **Work**: Perform operations
3. **Commit**: Make changes permanent
4. **Rollback**: Undo changes on error

### Pattern 1: Basic Transaction

```cpp
Status performTransaction(ErrorContext* ctx) {
    uint32_t xid;

    // Begin transaction
    Status s = txn_mgr_->beginTransaction(xid, ctx);
    if (s != Status::OK) {
        return s;
    }

    // Perform work
    s = doWork1(xid, ctx);
    if (s != Status::OK) {
        txn_mgr_->rollbackTransaction(xid, nullptr);
        return s;
    }

    s = doWork2(xid, ctx);
    if (s != Status::OK) {
        txn_mgr_->rollbackTransaction(xid, nullptr);
        return s;
    }

    // Commit transaction
    return txn_mgr_->commitTransaction(xid, ctx);
}
```

### Pattern 2: RAII Transaction Guard

```cpp
class TransactionGuard {
    TransactionManager* txn_mgr_;
    uint32_t xid_;
    bool committed_;

public:
    TransactionGuard(TransactionManager* mgr, uint32_t xid)
        : txn_mgr_(mgr), xid_(xid), committed_(false) {}

    ~TransactionGuard() {
        if (!committed_) {
            txn_mgr_->rollbackTransaction(xid_, nullptr);
        }
    }

    Status commit(ErrorContext* ctx) {
        Status s = txn_mgr_->commitTransaction(xid_, ctx);
        if (s == Status::OK) {
            committed_ = true;
        }
        return s;
    }

    // Disable copy
    TransactionGuard(const TransactionGuard&) = delete;
};

// Usage
Status performTransaction(ErrorContext* ctx) {
    uint32_t xid;
    Status s = txn_mgr_->beginTransaction(xid, ctx);
    if (s != Status::OK) {
        return s;
    }

    TransactionGuard guard(txn_mgr_, xid);

    // Work - automatic rollback on error
    s = doWork1(xid, ctx);
    if (s != Status::OK) {
        return s;  // Guard rolls back automatically
    }

    s = doWork2(xid, ctx);
    if (s != Status::OK) {
        return s;
    }

    // Explicit commit
    return guard.commit(ctx);
}
```

---

## File Handle Management

### Pattern: RAII File Wrapper

```cpp
class FileHandle {
    int fd_;

public:
    FileHandle(const char* path, int flags) {
        fd_ = open(path, flags);
        if (fd_ < 0) {
            throw std::runtime_error("Failed to open file");
        }
    }

    ~FileHandle() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    int getFd() const { return fd_; }

    // Disable copy, enable move
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    FileHandle(FileHandle&& other) : fd_(other.fd_) {
        other.fd_ = -1;
    }
};

// Usage
Status readFile(const char* path, ErrorContext* ctx) {
    try {
        FileHandle file(path, O_RDONLY);

        // Use file
        uint8_t buffer[4096];
        ssize_t n = read(file.getFd(), buffer, sizeof(buffer));

        return Status::OK;
    } catch (const std::exception& e) {
        if (ctx) {
            ctx->message = "Failed to read file: " + std::string(e.what());
        }
        return Status::IO_ERROR;
    }
    // File automatically closed here
}
```

---

## Memory Management

### Pattern 1: Smart Pointers (Recommended)

```cpp
// Unique ownership
std::unique_ptr<Table> table = std::make_unique<Table>("my_table");

// Shared ownership
std::shared_ptr<Index> index = std::make_shared<Index>("my_index");

// Weak reference (no ownership)
std::weak_ptr<Index> weak_index = index;
```

### Pattern 2: Manual Memory (Avoid)

```cpp
// BAD: Manual new/delete
Table* table = new Table("my_table");
// Use table
delete table;  // Easy to forget!

// GOOD: Use unique_ptr
auto table = std::make_unique<Table>("my_table");
// Automatic cleanup
```

### Pattern 3: Buffer Allocation

```cpp
Status allocateBuffer(size_t size, std::vector<uint8_t>& buffer, ErrorContext* ctx) {
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

---

## Common Resource Leaks

### Leak 1: Forgetting to Unpin

```cpp
// BAD
void* buffer;
buffer_pool_->pinPage(page_id, &buffer, ctx);
if (error) {
    return Status::ERROR;  // LEAK: Page still pinned!
}
buffer_pool_->unpinPage(page_id, false, ctx);
```

### Leak 2: Exception During Cleanup

```cpp
// BAD
try {
    doWork();
} catch (...) {
    cleanup();  // What if cleanup() throws?
}

// GOOD
try {
    doWork();
} catch (...) {
    try {
        cleanup();
    } catch (...) {
        // Ignore cleanup errors
    }
    throw;
}
```

### Leak 3: Early Returns

```cpp
// BAD
Status process(ErrorContext* ctx) {
    Resource* res = acquireResource();

    if (condition1) return Status::ERROR;  // LEAK!
    if (condition2) return Status::ERROR;  // LEAK!

    releaseResource(res);
    return Status::OK;
}

// GOOD
Status process(ErrorContext* ctx) {
    Resource* res = acquireResource();

    Status result = Status::OK;
    if (condition1) {
        result = Status::ERROR;
        goto cleanup;
    }
    if (condition2) {
        result = Status::ERROR;
        goto cleanup;
    }

cleanup:
    releaseResource(res);
    return result;
}

// BETTER: Use RAII
Status process(ErrorContext* ctx) {
    auto res = makeRAII_Resource();

    if (condition1) return Status::ERROR;  // Automatic cleanup
    if (condition2) return Status::ERROR;  // Automatic cleanup

    return Status::OK;
}
```

---

## Resource Balance Checking

### Automated Balance Verification

```cpp
class ResourceTracker {
    std::atomic<int64_t> pin_count_{0};
    std::atomic<int64_t> unpin_count_{0};

public:
    void recordPin() {
        pin_count_.fetch_add(1, std::memory_order_relaxed);
    }

    void recordUnpin() {
        unpin_count_.fetch_add(1, std::memory_order_relaxed);
    }

    bool isBalanced() const {
        return pin_count_.load() == unpin_count_.load();
    }

    int64_t getBalance() const {
        return pin_count_.load() - unpin_count_.load();
    }
};

// Usage in tests
TEST(ResourceTest, PinUnpinBalance) {
    ResourceTracker tracker;

    for (int i = 0; i < 1000; ++i) {
        tracker.recordPin();
        // Do work
        tracker.recordUnpin();
    }

    EXPECT_TRUE(tracker.isBalanced());
    EXPECT_EQ(tracker.getBalance(), 0);
}
```

**See CI_CD_GUIDE.md for automated balance checking in CI/CD**

---

## Best Practices

### DO

1. **Use RAII**: Prefer automatic cleanup over manual
2. **Check return values**: Always handle allocation failures
3. **Clean up on all paths**: Success and error paths
4. **Use smart pointers**: Avoid manual new/delete
5. **Balance resources**: Every acquire must have a release
6. **Test cleanup**: Verify no leaks with Valgrind/ASAN

### DON'T

1. **Ignore cleanup**: Never skip release on error paths
2. **Leak on exception**: Use RAII or try-catch
3. **Double-free**: Track ownership carefully
4. **Mix ownership**: Don't mix raw pointers and smart pointers
5. **Forget null checks**: Always validate before dereferencing

---

## Testing Resource Management

### Test 1: Basic Balance

```cpp
TEST(ResourceTest, BasicPinUnpin) {
    BufferPool pool(100);
    uint32_t page_id;

    ASSERT_EQ(pool.allocatePage(&page_id, nullptr), Status::OK);

    void* buffer;
    ASSERT_EQ(pool.pinPage(page_id, &buffer, nullptr), Status::OK);
    ASSERT_EQ(pool.unpinPage(page_id, false, nullptr), Status::OK);

    // Should be able to pin again
    ASSERT_EQ(pool.pinPage(page_id, &buffer, nullptr), Status::OK);
    ASSERT_EQ(pool.unpinPage(page_id, false, nullptr), Status::OK);
}
```

### Test 2: Leak Detection (Valgrind)

```bash
valgrind --leak-check=full --show-leak-kinds=all ./test_program

# Expected output:
# LEAK SUMMARY:
#   definitely lost: 0 bytes in 0 blocks
```

### Test 3: Exception Safety

```cpp
TEST(ResourceTest, ExceptionSafety) {
    BufferPool pool(100);
    uint32_t page_id;

    pool.allocatePage(&page_id, nullptr);

    void* buffer;
    pool.pinPage(page_id, &buffer, nullptr);

    // Simulate exception
    EXPECT_THROW({
        doWorkThatThrows(buffer);
    }, std::exception);

    // Page should still be unpinned properly (via RAII guard)
    // If not, next pin will fail or leak
    ASSERT_EQ(pool.pinPage(page_id, &buffer, nullptr), Status::OK);
    pool.unpinPage(page_id, false, nullptr);
}
```

---

## Conclusion

Resource management is critical for database stability:

1. **Use RAII**: Automatic cleanup prevents leaks
2. **Balance resources**: Every acquire → release
3. **Handle errors**: Clean up on all paths
4. **Test thoroughly**: Valgrind, ASAN, balance checkers
5. **Document ownership**: Who owns what resources?

**Remember**: Leaks are bugs. Every resource acquired must be released, on every code path, even when exceptions occur.

---

**Document Status**: ✅ Production Ready
**Maintainer**: ScratchBird Development Team
**Last Review**: October 17, 2025
