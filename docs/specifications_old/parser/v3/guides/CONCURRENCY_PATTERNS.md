# ScratchBird Concurrency Patterns Guide

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Document Version**: 1.0
**Last Updated**: October 17, 2025
**Status**: Production Ready

---

## Overview

This document provides guidelines for writing thread-safe code in ScratchBird Database. It covers common concurrency patterns, synchronization primitives, and best practices for multi-threaded programming.

**See also**:
- `LOCKING_PROTOCOL.md` - Lock ordering and hierarchy
- `RESOURCE_MANAGEMENT.md` - Pin/unpin and resource lifecycle
- `ERROR_HANDLING_GUIDE.md` - Error handling in concurrent code

---

## Table of Contents

1. [Concurrency Primitives](#concurrency-primitives)
2. [Thread-Safety Patterns](#thread-safety-patterns)
3. [Lock-Free Patterns](#lock-free-patterns)
4. [Common Pitfalls](#common-pitfalls)
5. [Testing Concurrency](#testing-concurrency)
6. [Performance Considerations](#performance-considerations)

---

## Concurrency Primitives

### 1. Mutex (`std::mutex`)

**When to use**: Exclusive access to shared data

**Example**:
```cpp
class ThreadSafeCache {
    std::mutex mutex_;
    std::unordered_map<Key, Value> cache_;

public:
    void insert(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_[key] = value;
    }

    bool get(const Key& key, Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            value = it->second;
            return true;
        }
        return false;
    }
};
```

### 2. Shared Mutex (`std::shared_mutex`)

**When to use**: Read-heavy workloads (many readers, few writers)

**Example**:
```cpp
class ReadWriteCache {
    mutable std::shared_mutex mutex_;
    std::unordered_map<Key, Value> cache_;

public:
    bool get(const Key& key, Value& value) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);  // Read lock
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            value = it->second;
            return true;
        }
        return false;
    }

    void insert(const Key& key, const Value& value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);  // Write lock
        cache_[key] = value;
    }
};
```

### 3. Atomic Variables (`std::atomic`)

**When to use**: Simple counters, flags, or state that doesn't require compound operations

**Example**:
```cpp
class BufferPoolStats {
    std::atomic<uint64_t> hits_{0};
    std::atomic<uint64_t> misses_{0};

public:
    void recordHit() {
        hits_.fetch_add(1, std::memory_order_relaxed);  // Performance optimization
    }

    void recordMiss() {
        misses_.fetch_add(1, std::memory_order_relaxed);
    }

    uint64_t getHits() const {
        return hits_.load(std::memory_order_relaxed);
    }

    uint64_t getMisses() const {
        return misses_.load(std::memory_order_relaxed);
    }
};
```

### 4. Condition Variables (`std::condition_variable`)

**When to use**: Waiting for a condition to become true

**Example**:
```cpp
class BlockingQueue {
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<Task> queue_;
    bool stopped_{false};

public:
    void push(Task task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(task));
        }
        cv_.notify_one();  // Wake up one waiting thread
    }

    bool pop(Task& task) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || stopped_; });

        if (stopped_ && queue_.empty()) {
            return false;
        }

        task = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
        }
        cv_.notify_all();  // Wake up all waiting threads
    }
};
```

---

## Thread-Safety Patterns

### Pattern 1: Immutable Objects

**Best for**: Data that never changes after construction

```cpp
class ImmutablePage {
    const uint32_t page_id_;
    const std::vector<uint8_t> data_;

public:
    ImmutablePage(uint32_t page_id, std::vector<uint8_t> data)
        : page_id_(page_id), data_(std::move(data)) {}

    uint32_t getPageId() const { return page_id_; }
    const std::vector<uint8_t>& getData() const { return data_; }

    // No setters - immutable!
};

// Can be safely shared across threads without synchronization
std::shared_ptr<ImmutablePage> page = std::make_shared<ImmutablePage>(42, data);
```

### Pattern 2: Thread-Local Storage

**Best for**: Per-thread state that doesn't need sharing

```cpp
class ThreadLocalAllocator {
    thread_local static std::vector<uint8_t> scratch_buffer_;

public:
    static uint8_t* getScratchBuffer(size_t size) {
        if (scratch_buffer_.size() < size) {
            scratch_buffer_.resize(size);
        }
        return scratch_buffer_.data();
    }
};

thread_local std::vector<uint8_t> ThreadLocalAllocator::scratch_buffer_;
```

### Pattern 3: RAII Lock Guards

**Best for**: Automatic lock acquisition/release

```cpp
class TransactionManager {
    std::mutex mutex_;
    std::map<uint32_t, Transaction*> transactions_;

public:
    Status beginTransaction(uint32_t& xid, ErrorContext* ctx) {
        std::lock_guard<std::mutex> lock(mutex_);  // Automatic unlock on scope exit

        xid = generateXID();
        transactions_[xid] = new Transaction(xid);

        return Status::OK;
        // mutex_ automatically unlocked here, even on exception
    }
};
```

### Pattern 4: Double-Checked Locking (with Caution)

**Best for**: Lazy initialization

```cpp
class Singleton {
    static std::atomic<Singleton*> instance_;
    static std::mutex mutex_;

public:
    static Singleton* getInstance() {
        Singleton* tmp = instance_.load(std::memory_order_acquire);
        if (tmp == nullptr) {
            std::lock_guard<std::mutex> lock(mutex_);
            tmp = instance_.load(std::memory_order_relaxed);
            if (tmp == nullptr) {
                tmp = new Singleton();
                instance_.store(tmp, std::memory_order_release);
            }
        }
        return tmp;
    }
};
```

**Note**: In C++11+, prefer `std::call_once` or static local variables:
```cpp
static Singleton& getInstance() {
    static Singleton instance;  // Thread-safe in C++11+
    return instance;
}
```

### Pattern 5: Lock Coupling (B-Tree Traversal)

**Best for**: Tree/graph traversal where you hold multiple locks briefly

```cpp
Status BTree::search(const Key& key, RID& rid, ErrorContext* ctx) {
    uint32_t current_page_id = root_page_id_;
    uint32_t parent_page_id = 0;

    while (true) {
        // Lock current page
        acquireLock(current_page_id, LockMode::SHARED);

        // If we have parent, unlock it (we're done with it)
        if (parent_page_id != 0) {
            releaseLock(parent_page_id);
        }

        // Read current page
        BTreeNode node = readNode(current_page_id);

        if (node.isLeaf()) {
            // Found leaf, search for key
            bool found = node.findKey(key, rid);
            releaseLock(current_page_id);
            return found ? Status::OK : Status::KEY_NOT_FOUND;
        }

        // Internal node - find child
        parent_page_id = current_page_id;
        current_page_id = node.findChild(key);
        // Loop continues - will lock child, then unlock parent
    }
}
```

---

## Lock-Free Patterns

### Pattern 1: Lock-Free Stack

```cpp
template<typename T>
class LockFreeStack {
    struct Node {
        T data;
        Node* next;
    };

    std::atomic<Node*> head_{nullptr};

public:
    void push(const T& data) {
        Node* new_node = new Node{data, nullptr};
        new_node->next = head_.load(std::memory_order_relaxed);

        while (!head_.compare_exchange_weak(
            new_node->next,
            new_node,
            std::memory_order_release,
            std::memory_order_relaxed)) {
            // CAS failed, retry
        }
    }

    bool pop(T& result) {
        Node* old_head = head_.load(std::memory_order_relaxed);

        while (old_head && !head_.compare_exchange_weak(
            old_head,
            old_head->next,
            std::memory_order_acquire,
            std::memory_order_relaxed)) {
            // CAS failed, retry
        }

        if (old_head) {
            result = old_head->data;
            delete old_head;  // Memory reclamation issue - see hazard pointers
            return true;
        }
        return false;
    }
};
```

### Pattern 2: Atomic Flags for Spin Locks

```cpp
class SpinLock {
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

public:
    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) {
            // Spin - consider using pause instruction
            #if defined(__x86_64__) || defined(_M_X64)
            __builtin_ia32_pause();
            #endif
        }
    }

    void unlock() {
        flag_.clear(std::memory_order_release);
    }
};
```

**Warning**: Spin locks can waste CPU - use only for very short critical sections!

### Pattern 3: Atomic Reference Counting

```cpp
class RefCounted {
    mutable std::atomic<uint32_t> ref_count_{1};

public:
    void addRef() const {
        ref_count_.fetch_add(1, std::memory_order_relaxed);
    }

    void release() const {
        if (ref_count_.fetch_sub(1, std::memory_order_release) == 1) {
            std::atomic_thread_fence(std::memory_order_acquire);
            delete this;
        }
    }
};
```

---

## Common Pitfalls

### Pitfall 1: Deadlock from Lock Ordering Violation

**Problem**:
```cpp
// Thread 1
std::lock_guard<std::mutex> lock1(mutex_a_);
std::lock_guard<std::mutex> lock2(mutex_b_);

// Thread 2
std::lock_guard<std::mutex> lock1(mutex_b_);  // DEADLOCK!
std::lock_guard<std::mutex> lock2(mutex_a_);
```

**Solution**: Always acquire locks in the same order (see LOCKING_PROTOCOL.md)
```cpp
// Always lock mutex_a_ before mutex_b_
std::lock_guard<std::mutex> lock1(mutex_a_);
std::lock_guard<std::mutex> lock2(mutex_b_);
```

### Pitfall 2: Data Race on Non-Atomic Variable

**Problem**:
```cpp
class Counter {
    uint64_t count_ = 0;  // NOT ATOMIC!

public:
    void increment() { ++count_; }  // DATA RACE!
    uint64_t get() const { return count_; }  // DATA RACE!
};
```

**Solution**: Use `std::atomic`
```cpp
class Counter {
    std::atomic<uint64_t> count_{0};

public:
    void increment() { count_.fetch_add(1, std::memory_order_relaxed); }
    uint64_t get() const { return count_.load(std::memory_order_relaxed); }
};
```

### Pitfall 3: Lost Wakeups with Condition Variables

**Problem**:
```cpp
// Thread 1 (waiting)
while (!ready_) {
    cv_.wait(lock);  // BUG: What if notify happens before wait?
}

// Thread 2 (signaling)
ready_ = true;
cv_.notify_one();
```

**Solution**: Always check condition in a loop with predicate
```cpp
// Thread 1 (correct)
cv_.wait(lock, [this] { return ready_; });  // Predicate prevents lost wakeups
```

### Pitfall 4: ABA Problem in Lock-Free Code

**Problem**: CAS succeeds even though value changed and changed back

**Solution**: Use tagged pointers or versioned references
```cpp
template<typename T>
struct VersionedPointer {
    T* ptr;
    uint64_t version;
};

std::atomic<VersionedPointer<Node>> head_;

// CAS now includes version check
VersionedPointer<Node> old_head = head_.load();
VersionedPointer<Node> new_head = {new_node, old_head.version + 1};
head_.compare_exchange_strong(old_head, new_head);
```

---

## Testing Concurrency

### Test Pattern 1: High Thread Count

```cpp
TEST(ConcurrencyTest, HighThreadCount) {
    const int NUM_THREADS = 100;
    const int OPS_PER_THREAD = 1000;

    ThreadSafeCounter counter;
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&counter, OPS_PER_THREAD]() {
            for (int j = 0; j < OPS_PER_THREAD; ++j) {
                counter.increment();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(counter.get(), NUM_THREADS * OPS_PER_THREAD);
}
```

### Test Pattern 2: Stress Test with Random Operations

```cpp
TEST(ConcurrencyTest, MixedOperations) {
    ThreadSafeMap<int, int> map;
    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    for (int i = 0; i < 20; ++i) {
        threads.emplace_back([&map, &stop]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 1000);

            while (!stop.load()) {
                int key = dis(gen);
                int value = dis(gen);

                switch (dis(gen) % 3) {
                    case 0: map.insert(key, value); break;
                    case 1: map.remove(key); break;
                    case 2: { int v; map.get(key, v); } break;
                }
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
    stop.store(true);

    for (auto& t : threads) {
        t.join();
    }

    // If we get here without crashing, concurrency is working
}
```

### Test Pattern 3: ThreadSanitizer (TSAN)

```cpp
// Compile with: -fsanitize=thread
// Run test - TSAN will detect data races
TEST(ConcurrencyTest, TSANDetection) {
    std::atomic<int> atomic_var{0};
    int non_atomic_var = 0;  // Intentional race for testing

    std::thread t1([&]() {
        for (int i = 0; i < 1000; ++i) {
            atomic_var.fetch_add(1, std::memory_order_relaxed);
            non_atomic_var++;  // TSAN will flag this!
        }
    });

    std::thread t2([&]() {
        for (int i = 0; i < 1000; ++i) {
            atomic_var.fetch_add(1, std::memory_order_relaxed);
            non_atomic_var++;  // TSAN will flag this!
        }
    });

    t1.join();
    t2.join();

    // TSAN will report: WARNING: ThreadSanitizer: data race
}
```

---

## Performance Considerations

### 1. Lock Granularity

**Fine-grained locking**: Better concurrency, higher overhead
```cpp
class FineGrainedMap {
    static const int NUM_SHARDS = 16;
    std::mutex mutexes_[NUM_SHARDS];
    std::unordered_map<Key, Value> shards_[NUM_SHARDS];

    int getShardIndex(const Key& key) {
        return std::hash<Key>{}(key) % NUM_SHARDS;
    }

public:
    void insert(const Key& key, const Value& value) {
        int shard = getShardIndex(key);
        std::lock_guard<std::mutex> lock(mutexes_[shard]);
        shards_[shard][key] = value;
    }
};
```

**Coarse-grained locking**: Simpler, less concurrency
```cpp
class CoarseGrainedMap {
    std::mutex mutex_;
    std::unordered_map<Key, Value> map_;

public:
    void insert(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        map_[key] = value;
    }
};
```

### 2. Memory Ordering

**Sequential consistency** (slowest, strongest):
```cpp
x.store(1, std::memory_order_seq_cst);
int y = x.load(std::memory_order_seq_cst);
```

**Acquire-release** (medium):
```cpp
// Producer
x.store(1, std::memory_order_release);

// Consumer
while (x.load(std::memory_order_acquire) != 1) {}
```

**Relaxed** (fastest, weakest):
```cpp
hits_.fetch_add(1, std::memory_order_relaxed);  // OK for independent counters
```

### 3. Avoid False Sharing

**Problem**: Cache line bouncing
```cpp
struct Counter {
    std::atomic<uint64_t> count;  // 8 bytes
};

Counter counters[4];  // All in same cache line!
```

**Solution**: Pad to cache line size
```cpp
struct alignas(64) Counter {  // 64 bytes = typical cache line
    std::atomic<uint64_t> count;
    char padding[56];
};

Counter counters[4];  // Each in separate cache line
```

---

## Best Practices

1. **Prefer higher-level primitives**: Use `std::lock_guard` over manual lock/unlock
2. **Minimize critical sections**: Hold locks for shortest time possible
3. **Avoid nested locks**: Prone to deadlocks
4. **Use Read-Write locks**: When reads >> writes
5. **Atomic for simple operations**: Faster than mutex for counters/flags
6. **Test with TSAN**: Always run ThreadSanitizer in CI/CD
7. **Document lock ordering**: Prevent deadlocks (see LOCKING_PROTOCOL.md)
8. **Use immutable data**: No synchronization needed
9. **Profile before optimizing**: Measure lock contention before switching to lock-free

---

## Conclusion

Concurrency is hard. Follow these principles:

- **Prefer simplicity**: Locks are easier to reason about than lock-free
- **Test thoroughly**: Use TSAN, stress tests, high thread counts
- **Document assumptions**: What locks protect what data?
- **Follow lock hierarchy**: Prevent deadlocks
- **Measure performance**: Profile before optimizing

**When in doubt, use a mutex**. Lock-free code is complex and error-prone. Only optimize after profiling shows lock contention is a bottleneck.

---

**Document Status**: ✅ Production Ready
**Maintainer**: ScratchBird Development Team
**Last Review**: October 17, 2025
