# ScratchBird Complete Phase Breakdown
## Logical Implementation Order with All Core Components

## Overview
Each phase builds on previous work in logical order. All UUIDs use v7 (time-ordered) generation.
Page sizes supported: 8K, 16K, 32K, 64K, 128K

---

## ALPHA 1.01 - Foundation Layer

### Alpha 1.01.1 - Database File Creation
**Implementation Requirements:**
- Database file header structure
- Magic number and version
- Page size validation (8K, 16K, 32K, 64K, 128K)
- UUID v7 generation for database ID
- Initial file allocation

### Alpha 1.01.2 - Page Management Foundation
**Implementation Requirements:**
```cpp
class PageManager {
    PageNumber allocatePage();
    void freePage(PageNumber page);
    void readPage(PageNumber page, void* buffer);
    void writePage(PageNumber page, const void* buffer);
    uint32_t calculateChecksum(const void* page, size_t page_size);
    bool verifyChecksum(const void* page, size_t page_size);
};
```

### Alpha 1.01.3 - Schema Tree Initialization
**Implementation Requirements:**
- Create hierarchical schema structure
- Initialize [root] and 8 base schemas
- UUID v7 for each schema node
- Parent-child relationships
- Schema metadata pages

### Alpha 1.01.4 - System Catalog Creation
**Implementation Requirements:**
- sys.schemas table
- sys.tables table
- sys.columns table
- sys.indexes table
- sys.procedures table
- sys.triggers table
- All with UUID v7 identifiers

### Alpha 1.01.5 - Free Space Management
**Implementation Requirements:**
```cpp
class FreeSpaceManager {
    PageNumber findPageWithSpace(size_t needed_bytes);
    void updateFreeSpace(PageNumber page, size_t free_bytes);
    PageNumber allocateExtent(size_t page_count);
    void deallocateExtent(PageNumber start, size_t page_count);
};
```

---

## ALPHA 1.02 - Buffer Pool & Cache Layer

### Alpha 1.02.1 - Basic Buffer Pool
**Implementation Requirements:**
```cpp
class BufferPool {
    BufferFrame* getPage(PageNumber page_num);
    void pinPage(PageNumber page_num);
    void unpinPage(PageNumber page_num);
    void markDirty(PageNumber page_num);
    void flushPage(PageNumber page_num);
    size_t getPoolSize();
};
```

### Alpha 1.02.2 - LRU Page Replacement
**Implementation Requirements:**
```cpp
class LRUReplacer {
    PageNumber selectVictim();
    void recordAccess(PageNumber page);
    void pin(PageNumber page);
    void unpin(PageNumber page);
};
```

### Alpha 1.02.3 - Dirty Page Management
**Implementation Requirements:**
```cpp
class DirtyPageManager {
    void markDirty(PageNumber page);
    void markClean(PageNumber page);
    vector<PageNumber> getDirtyPages();
    void flushDirtyPages();
    void checkpoint();
};
```

### Alpha 1.02.4 - Buffer Pool Statistics
**Implementation Requirements:**
- Hit ratio tracking
- Page read/write counts
- Eviction statistics
- Hot/cold page tracking

---

## ALPHA 1.03 - Heap Storage Layer

### Alpha 1.03.1 - Tuple Format
**Implementation Requirements:**
```cpp
struct TupleHeader {
    UUID tuple_id;        // UUID v7
    uint32_t tuple_size;
    uint16_t column_count;
    uint16_t null_bitmap_offset;
    
    // MGA fields (populated later)
    TransactionId xmin;  // Creating transaction
    TransactionId xmax;  // Deleting transaction
    CommandId cmin;      // Creating command
    CommandId cmax;      // Deleting command
    ItemPointer t_ctid;  // Current tuple ID
    uint16_t t_infomask; // Flags
};
```

### Alpha 1.03.2 - Heap Page Structure
**Implementation Requirements:**
```cpp
class HeapPage {
    bool insertTuple(const Tuple& tuple);
    bool deleteTuple(TupleId tid);
    Tuple* getTuple(TupleId tid);
    size_t getFreeSpace();
    void compact();  // Defragment page
};
```

### Alpha 1.03.3 - Heap Scan
**Implementation Requirements:**
```cpp
class HeapScanner {
    void startScan(TableId table);
    Tuple* getNextTuple();
    void endScan();
    void resetScan();
};
```

---

## ALPHA 1.04 - Transaction Foundation (Pre-MGA)

### Alpha 1.04.1 - Transaction ID Management
**Implementation Requirements:**
```cpp
class TransactionIdManager {
    TransactionId getNextTransactionId();
    TransactionId getCurrentTransactionId();
    bool isTransactionInProgress(TransactionId xid);
    void registerTransaction(TransactionId xid);
    void unregisterTransaction(TransactionId xid);
};
```

### Alpha 1.04.2 - Transaction Inventory Page (TIP)
**Implementation Requirements:**
```cpp
class TransactionInventoryPage {
    void setTransactionStatus(TransactionId xid, TxnStatus status);
    TxnStatus getTransactionStatus(TransactionId xid);
    vector<TransactionId> getActiveTransactions();
    void cleanup();  // Remove old entries
};
```

### Alpha 1.04.3 - Basic Transaction Context
**Implementation Requirements:**
```cpp
class TransactionContext {
    TransactionId xid;
    IsolationLevel isolation_level;
    timestamp start_time;
    TransactionState state;
    vector<LockId> held_locks;
    vector<PageNumber> modified_pages;
};
```

### Alpha 1.04.4 - Simple Locking (Table Level)
**Implementation Requirements:**
```cpp
class SimpleLockManager {
    bool acquireTableLock(TableId table, LockMode mode);
    void releaseTableLock(TableId table);
    bool isLocked(TableId table);
    vector<LockInfo> getHeldLocks(TransactionId xid);
};
```

---

## ALPHA 1.05 - B-Tree Index Foundation

### Alpha 1.05.1 - B-Tree Node Structure
**Implementation Requirements:**
```cpp
class BTreeNode {
    bool is_leaf;
    uint16_t num_keys;
    uint16_t max_keys;
    PageNumber parent;
    PageNumber right_sibling;
    Key keys[];
    PageNumber children[];  // For internal nodes
    TupleId values[];       // For leaf nodes
};
```

### Alpha 1.05.2 - B-Tree Operations
**Implementation Requirements:**
```cpp
class BTree {
    void insert(const Key& key, TupleId tid);
    TupleId search(const Key& key);
    void remove(const Key& key);
    void splitNode(PageNumber node);
    void mergeNodes(PageNumber left, PageNumber right);
};
```

### Alpha 1.05.3 - B-Tree Iterator
**Implementation Requirements:**
```cpp
class BTreeIterator {
    void seekFirst();
    void seekLast();
    void seek(const Key& key);
    bool next();
    bool prev();
    pair<Key, TupleId> current();
};
```

---

## ALPHA 1.06 - DDL API Implementation

### Alpha 1.06.1 - Schema Operations
**Implementation Requirements:**
- createSchema with UUID v7
- dropSchema with cascade check
- alterSchema for options
- Schema path resolution

### Alpha 1.06.2 - Table Operations
**Implementation Requirements:**
- createTable with heap allocation
- dropTable with cleanup
- alterTable for structure changes
- Table UUID v7 assignment

### Alpha 1.06.3 - Column Operations
**Implementation Requirements:**
- addColumn with default values
- dropColumn with data migration
- alterColumn type changes
- Column UUID v7 assignment

### Alpha 1.06.4 - Index Operations
**Implementation Requirements:**
- createIndex with B-tree initialization
- dropIndex with cleanup
- rebuildIndex for maintenance
- Index UUID v7 assignment

---

## ALPHA 1.07 - DML API Implementation

### Alpha 1.07.1 - Insert Operations
**Implementation Requirements:**
- Single row insert
- Bulk insert optimization
- Constraint checking
- Index maintenance

### Alpha 1.07.2 - Select Operations
**Implementation Requirements:**
- Table scan
- Index scan
- Simple WHERE clause
- Projection

### Alpha 1.07.3 - Update Operations
**Implementation Requirements:**
- Update by predicate
- Index maintenance on update
- Constraint validation
- Old value preservation

### Alpha 1.07.4 - Delete Operations
**Implementation Requirements:**
- Delete by predicate
- Mark for deletion (not physical)
- Index cleanup
- Space reclamation

---

## ALPHA 1.08 - Write-Ahead Logging (WAL) Foundation

### Alpha 1.08.1 - WAL Structure
**Implementation Requirements:**
```cpp
struct WALRecord {
    LSN lsn;                // Log Sequence Number
    TransactionId xid;
    WALRecordType type;
    PageNumber page;
    uint32_t offset;
    uint32_t length;
    uint8_t data[];
};
```

### Alpha 1.08.2 - WAL Writer
**Implementation Requirements:**
```cpp
class WALWriter {
    LSN writeRecord(const WALRecord& record);
    void flush(LSN lsn);
    void forceFlush();
    LSN getCurrentLSN();
    void rotate();  // Start new WAL file
};
```

### Alpha 1.08.3 - WAL Recovery
**Implementation Requirements:**
```cpp
class WALRecovery {
    void replay(LSN start_lsn);
    void applyRecord(const WALRecord& record);
    LSN findCheckpoint();
    void performRecovery();
};
```

---

## ALPHA 1.09 - Multi-Generational Architecture (MGA)

### Alpha 1.09.1 - Version Chain Management
**Implementation Requirements:**
```cpp
class VersionChainManager {
    void createVersion(TupleId tid, const Tuple& new_version);
    Tuple* getVersionForTransaction(TupleId tid, TransactionId xid);
    void pruneOldVersions(TupleId tid);
    bool isVisible(const TupleHeader& tuple, TransactionId xid);
};
```

### Alpha 1.09.2 - Transaction Snapshots
**Implementation Requirements:**
```cpp
class TransactionSnapshot {
    TransactionId xmin;  // Earliest transaction still running
    TransactionId xmax;  // Latest completed transaction
    vector<TransactionId> xip;  // In-progress transactions
    
    bool isVisible(TransactionId xid);
    void update();
};
```

### Alpha 1.09.3 - MVCC Visibility Rules
**Implementation Requirements:**
```cpp
class MVCCVisibility {
    bool isVisible(const TupleHeader& tuple, const TransactionSnapshot& snapshot);
    bool satisfiesUpdate(const TupleHeader& tuple, TransactionId xid);
    bool satisfiesDelete(const TupleHeader& tuple, TransactionId xid);
};
```

### Alpha 1.09.4 - Garbage Collection
**Implementation Requirements:**
```cpp
class GarbageCollector {
    void identifyGarbage();
    void reclaimSpace();
    void updateFreeSpace();
    void scheduleCleanup();
    TransactionId getOldestActiveTransaction();
};
```

---

## ALPHA 1.10 - Isolation Levels

### Alpha 1.10.1 - Read Committed
**Implementation Requirements:**
- New snapshot for each statement
- See committed changes immediately
- No dirty reads

### Alpha 1.10.2 - Repeatable Read
**Implementation Requirements:**
- Snapshot at transaction start
- Consistent view throughout transaction
- No phantom reads within snapshot

### Alpha 1.10.3 - Serializable (Optional)
**Implementation Requirements:**
- Serializable Snapshot Isolation (SSI)
- Predicate locking
- Conflict detection

---

## ALPHA 1.11 - Advanced Buffer Management

### Alpha 1.11.1 - Adaptive Replacement Cache (ARC)
**Implementation Requirements:**
```cpp
class ARCBuffer {
    list<PageNumber> t1;  // Recent pages
    list<PageNumber> t2;  // Frequent pages
    list<PageNumber> b1;  // Ghost list for t1
    list<PageNumber> b2;  // Ghost list for t2
    
    PageNumber selectVictim();
    void adapt(bool from_b1);
};
```

### Alpha 1.11.2 - Direct I/O Support
**Implementation Requirements:**
```cpp
class DirectIOManager {
    void enableDirectIO();
    void* allocateAlignedBuffer(size_t size);
    void readDirect(int fd, void* buffer, size_t size, off_t offset);
    void writeDirect(int fd, const void* buffer, size_t size, off_t offset);
};
```

### Alpha 1.11.3 - Buffer Pool Partitioning
**Implementation Requirements:**
- Separate pools for indexes and data
- Priority-based allocation
- Per-table buffer quotas

---

## ALPHA 1.12 - Query Execution Foundation

### Alpha 1.12.1 - Execution Operators
**Implementation Requirements:**
```cpp
class ExecutionOperator {
    virtual void open() = 0;
    virtual Tuple* next() = 0;
    virtual void close() = 0;
    virtual void reset() = 0;
};

class SeqScanOperator : public ExecutionOperator {};
class IndexScanOperator : public ExecutionOperator {};
class FilterOperator : public ExecutionOperator {};
class ProjectOperator : public ExecutionOperator {};
```

### Alpha 1.12.2 - Simple Query Executor
**Implementation Requirements:**
```cpp
class QueryExecutor {
    ExecutionOperator* createPlan(const Query& query);
    ResultSet execute(ExecutionOperator* plan);
    void explainPlan(ExecutionOperator* plan);
};
```

---

## ALPHA 1.13 - Constraint Enforcement

### Alpha 1.13.1 - Primary Key Constraints
**Implementation Requirements:**
- Uniqueness checking
- NOT NULL enforcement
- Index creation

### Alpha 1.13.2 - Foreign Key Constraints
**Implementation Requirements:**
- Referential integrity
- Cascade operations
- Deferred checking

### Alpha 1.13.3 - Check Constraints
**Implementation Requirements:**
- Expression evaluation
- Row validation
- Constraint metadata

---

## ALPHA 1.14 - Schema Navigation & Resolution

### Alpha 1.14.1 - Path Resolution
**Implementation Requirements:**
```cpp
class SchemaResolver {
    UUID resolveSchemaPath(const string& path);
    UUID resolveTablePath(const string& path);
    string getFullPath(UUID object_id);
    vector<string> getSearchPath();
};
```

### Alpha 1.14.2 - Relative Navigation
**Implementation Requirements:**
- Support for ../.. notation
- Current schema concept
- Search path implementation

---

## ALPHA 1.15 - Security Foundation

### Alpha 1.15.1 - User Management
**Implementation Requirements:**
```cpp
class UserManager {
    UUID createUser(const string& username, const string& password);
    bool authenticateUser(const string& username, const string& password);
    void dropUser(const string& username);
    void changePassword(const string& username, const string& new_password);
};
```

### Alpha 1.15.2 - Role Management
**Implementation Requirements:**
```cpp
class RoleManager {
    UUID createRole(const string& rolename);
    void grantRole(const string& username, const string& rolename);
    void revokeRole(const string& username, const string& rolename);
    vector<string> getUserRoles(const string& username);
};
```

### Alpha 1.15.3 - Permission System
**Implementation Requirements:**
```cpp
class PermissionManager {
    void grant(const Permission& perm);
    void revoke(const Permission& perm);
    bool checkPermission(UUID user_id, UUID object_id, PermissionType type);
    vector<Permission> getObjectPermissions(UUID object_id);
};
```

---

## Test Requirements for Each Phase

Every phase requires tests for:
1. All 5 page sizes (8K, 16K, 32K, 64K, 128K)
2. Normal operations
3. Error conditions
4. Boundary conditions
5. Concurrent access (where applicable)
6. Performance benchmarks
7. File structure verification
8. Memory leak detection

---

## Success Criteria

Each phase is complete when:
- All code compiles without warnings
- All tests pass for all page sizes
- File structure is verified correct
- Documentation is complete
- Performance meets targets
- No memory leaks detected
- Ready for next phase to build upon

This logical ordering ensures each component has its dependencies in place before implementation!