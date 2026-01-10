

# **ScratchBird DB: A Low-Level Technical Specification for a Multi-Generational Database Architecture**

## **Section 1: Transaction Management Subsystem**

### **Overview and Core Principles**

The Transaction Management Subsystem is the architectural foundation of ScratchBird DB, providing the mechanisms necessary to guarantee the ACID properties of Atomicity and Durability.1 Its primary responsibility is to maintain an immutable and authoritative record of the final state of every transaction that occurs within the database. This ensures that the effects of a transaction are either made visible in their entirety (committed) or are completely discarded (aborted), even in the event of a catastrophic system failure.

The central component of this subsystem is the Transaction Status Log, hereafter referred to as the Commit Log or CLOG. This design is modeled on the highly efficient transaction status systems of PostgreSQL (pg\_xact) and Firebird (Transaction Inventory Page).3 The CLOG is a specialized, persistent, and logically append-only data structure. It provides a highly optimized, centralized lookup mechanism for determining transaction status, which is a frequent operation in Multi-Generational Architecture (MGA). By centralizing this information, the system avoids the performance penalty of scattering transaction state across millions of individual data pages.

A key architectural benefit of this CLOG-based design is the efficiency of transaction aborts. In traditional database systems, rolling back a transaction can be a resource-intensive process involving the application of undo logs to reverse changes. In ScratchBird DB, an abort is a simple state change within the CLOG. The tuple versions created by the aborted transaction are not immediately removed from data pages; they are simply rendered invisible to all other transactions by the visibility rules that consult the CLOG.5 These orphaned versions are later reclaimed by the garbage collection subsystem with no performance impact on the aborting transaction itself.

### **Transaction States and On-Disk Encoding**

To achieve maximum storage density and performance, the state of each transaction is encoded using exactly two bits. This allows the status of four distinct transactions to be stored within a single byte, dramatically increasing the number of transaction statuses that can be held in a single 8 KB disk page and, by extension, in the in-memory cache.5

The system defines three primary transaction states:

* **TRANSACTION\_STATUS\_ACTIVE**: The transaction is currently in progress. Its changes are not yet visible to any other transaction.  
* **TRANSACTION\_STATUS\_COMMITTED**: The transaction has completed successfully. Its changes are permanent and will be visible to subsequent transactions according to the visibility rules.  
* **TRANSACTION\_STATUS\_ABORTED**: The transaction has been rolled back. Its changes must be ignored by all other transactions.

The specific 2-bit encoding for these states is defined as follows:

| Bit Pattern | Value | Status Enum | Description |
| :---- | :---- | :---- | :---- |
| 00 | 0 | TRANSACTION\_STATUS\_ACTIVE | The transaction is in progress. This is the default state for a transaction ID that has been allocated but not yet completed. |
| 01 | 1 | TRANSACTION\_STATUS\_COMMITTED | The transaction has been successfully committed. |
| 10 | 2 | TRANSACTION\_STATUS\_ABORTED | The transaction has been rolled back. |
| 11 | 3 | TRANSACTION\_STATUS\_SUB\_COMMITTED | Reserved for sub-transaction commits. For the core specification, this state is unused but defined for future extensibility.3 |

### **CLOG On-Disk Structures and Layout**

The CLOG is persisted on disk as a collection of files, each containing a sequence of 8 KB pages. Each page is a simple, contiguous block of bytes containing only transaction status data. There is no per-page header; a page's identity and the range of transactions it covers are determined by its position within the CLOG file set.

#### **Data Structures**

The on-disk structure of a single CLOG page is a direct byte array.

C++

// Defines the size of a standard disk page in ScratchBird DB.  
\#**define** SB\_PAGE\_SIZE 8192

// Represents a single 8KB page of the on-disk Commit Log (CLOG).  
// This structure is a raw block of memory containing packed 2-bit transaction statuses.  
struct ClogPage {  
    uint8\_t transaction\_status\_data;  
};

#### **On-Disk Layout**

The layout of a ClogPage is straightforward. It is a dense array of 8192 bytes. Each byte stores the status of four sequential transactions.

* **Byte 0**: Contains statuses for TransactionIds 0, 1, 2, and 3\.  
* **Byte 1**: Contains statuses for TransactionIds 4, 5, 6, and 7\.  
* ...and so on.

The mapping within a byte is as follows:

* Bits 0-1: Status for TransactionId N  
* Bits 2-3: Status for TransactionId N+1  
* Bits 4-5: Status for TransactionId N+2  
* Bits 6-7: Status for TransactionId N+3

This dense packing allows a single 8 KB page to store the status of 8192×4=32768 transactions.

### **Core CLOG Algorithms**

The following algorithms define the fundamental operations for reading and writing transaction statuses. These operations are performance-critical and form the heart of the transaction management subsystem.

#### **Algorithm: Get Transaction Status**

This function maps a 64-bit TransactionId to its corresponding 2-bit status by calculating the correct page, byte offset, and bit offset within the CLOG.

C++

// Enumeration for the possible states of a transaction.  
enum TransactionStatus {  
    TRANSACTION\_STATUS\_ACTIVE \= 0b00,  
    TRANSACTION\_STATUS\_COMMITTED \= 0b01,  
    TRANSACTION\_STATUS\_ABORTED \= 0b10,  
    TRANSACTION\_STATUS\_SUB\_COMMITTED \= 0b11  
};

// Constants derived from the on-disk layout.  
const uint32\_t CLOG\_BITS\_PER\_XACT \= 2;  
const uint32\_t CLOG\_XACTS\_PER\_BYTE \= 8 / CLOG\_BITS\_PER\_XACT; // 4  
const uint32\_t CLOG\_XACTS\_PER\_PAGE \= SB\_PAGE\_SIZE \* CLOG\_XACTS\_PER\_BYTE; // 32768

/\*\*  
 \* @brief Retrieves the status of a given transaction from the CLOG.  
 \* @param xid The 64-bit TransactionId to look up.  
 \* @return The status of the transaction.  
 \*/  
TransactionStatus getTransactionStatus(uint64\_t xid) {  
    // 1\. Calculate which CLOG page contains the status for the given xid.  
    uint64\_t page\_number \= xid / CLOG\_XACTS\_PER\_PAGE;

    // 2\. Calculate the index of the transaction's status entry within that page.  
    uint32\_t entry\_index\_on\_page \= xid % CLOG\_XACTS\_PER\_PAGE;

    // 3\. Calculate the byte offset within the page's data array.  
    uint32\_t byte\_offset \= entry\_index\_on\_page / CLOG\_XACTS\_PER\_BYTE;

    // 4\. Calculate the position of the 2-bit status within the byte.  
    // (entry % 4\) gives the position (0-3), multiplied by 2 for the bit shift.  
    uint32\_t bit\_shift \= (entry\_index\_on\_page % CLOG\_XACTS\_PER\_BYTE) \* CLOG\_BITS\_PER\_XACT;

    // 5\. Read the required CLOG page from disk into a shared memory buffer.  
    // This operation is heavily optimized by an in-memory SLRU cache that holds  
    // recently accessed CLOG pages. If the page is not in the cache, it is read  
    // from the corresponding CLOG segment file.  
    ClogPage\* page\_buffer \= read\_clog\_page\_into\_buffer(page\_number);

    // 6\. Extract the byte containing the status.  
    uint8\_t status\_byte \= page\_buffer-\>transaction\_status\_data\[byte\_offset\];

    // 7\. Isolate the 2-bit status using a right shift and a bitmask.  
    uint8\_t status\_bits \= (status\_byte \>\> bit\_shift) & 0b11;

    return static\_cast\<TransactionStatus\>(status\_bits);  
}

#### **Algorithm: Set Transaction Status**

This function updates the status of a transaction. It is a read-modify-write operation that must be protected by a lightweight lock (latch) on the corresponding in-memory CLOG page buffer to prevent race conditions between concurrent transaction commits.

C++

/\*\*  
 \* @brief Sets the status for a given transaction in the CLOG.  
 \* @param xid The 64-bit TransactionId to update.  
 \* @param status The new status to set.  
 \*/  
void setTransactionStatus(uint64\_t xid, TransactionStatus status) {  
    uint64\_t page\_number \= xid / CLOG\_XACTS\_PER\_PAGE;  
    uint32\_t entry\_index\_on\_page \= xid % CLOG\_XACTS\_PER\_PAGE;  
    uint32\_t byte\_offset \= entry\_index\_on\_page / CLOG\_XACTS\_PER\_BYTE;  
    uint32\_t bit\_shift \= (entry\_index\_on\_page % CLOG\_XACTS\_PER\_BYTE) \* CLOG\_BITS\_PER\_XACT;

    // 1\. Acquire an exclusive lightweight lock on the buffer slot for this CLOG page.  
    // This prevents other backends from reading or writing this page concurrently.  
    lock\_clog\_buffer(page\_number, EXCLUSIVE);

    // 2\. Read the page into a shared memory buffer.  
    ClogPage\* page\_buffer \= read\_clog\_page\_into\_buffer(page\_number);

    // 3\. Read the current byte value.  
    uint8\_t current\_byte \= page\_buffer-\>transaction\_status\_data\[byte\_offset\];

    // 4\. Create a mask to clear the existing 2 bits for this transaction.  
    // e.g., for bit\_shift \= 2, mask is \~(0b1100) \= 0b0011.  
    uint8\_t clear\_mask \= \~((uint8\_t)0b11 \<\< bit\_shift);  
      
    // 5\. Create the new status value, shifted to the correct position.  
    uint8\_t new\_status\_bits \= (uint8\_t)status \<\< bit\_shift;

    // 6\. Apply the update: clear the old bits and OR in the new bits.  
    page\_buffer-\>transaction\_status\_data\[byte\_offset\] \= (current\_byte & clear\_mask) | new\_status\_bits;

    // 7\. Mark the in-memory buffer as dirty. This signals to the checkpoint  
    // process that this page needs to be written to persistent storage.  
    mark\_clog\_buffer\_dirty(page\_number);

    // 8\. Release the lightweight lock.  
    unlock\_clog\_buffer(page\_number);  
}

### **Scalability and Durability**

#### **Scaling to Billions of Transactions**

The CLOG subsystem is designed to scale to handle virtually limitless transactions. This is achieved by segmenting the on-disk CLOG into a series of files. A typical implementation would store 256 MB of CLOG data per file.3 For ScratchBird DB, we define a CLOG segment file as containing 32768 pages (

32768×8 KB=256 MB).

When getTransactionStatus or setTransactionStatus is called with a TransactionId that maps to a page number beyond the currently allocated segment files, a new segment file is created and zero-filled. The file-based segmentation allows the CLOG to grow as needed without requiring a single, massive contiguous file on disk.

#### **Durability via Careful Writes**

Following the Firebird model, ScratchBird DB achieves durability primarily through a "careful writes" strategy, which obviates the need for a traditional transaction log for crash recovery.32 Instead of logging changes sequentially, the system ensures durability by writing modified data pages to disk in a specific, dependency-aware order

*before* the transaction is marked as committed.4

The commit sequence is as follows:

1. **Flush Data Pages**: All data pages (heap, index, etc.) modified by the transaction are flushed to persistent storage. The order of these writes is critical to maintain on-disk consistency at all times.34 The system maintains a dependency graph of dirty pages to enforce rules such as:  
   * A new tuple version must be written to its data page before any index page that references it is written.  
   * In an update chain, the old tuple version (with its tdh\_xmax and tdh\_next\_version set) must be written before the new tuple version.  
   * Data pages must be written before their corresponding pointer pages are updated.  
2. **Force to Disk**: With the Forced Writes database setting enabled (the default for maximum safety), a system call like fsync() is issued to ensure all flushed pages have been physically written to stable storage, bypassing any OS or hardware write-back caches.34  
3. **Atomic Commit**: Only after all data pages are durably stored, the transaction's status is updated to COMMITTED in the corresponding *in-memory* CLOG page buffer by calling setTransactionStatus. This update is the atomic commit point; once this is done, the transaction is officially committed.35  
4. **Checkpointing**: The dirty in-memory CLOG page is written to its on-disk segment file at a later time by a background checkpointer process.

This "data-before-commit" approach guarantees that a committed transaction's changes are never lost. In the event of a system crash, recovery is nearly instantaneous.32 On restart, any transaction found in the CLOG with a status of

ACTIVE is simply changed to ABORTED. No "undo" or "redo" log processing is required, because the careful writes protocol ensures the on-disk state is always consistent.

#### **Optional Write-after Log (WAL) for Replication and Recovery**

While not the primary mechanism for transaction durability, ScratchBird DB supports the optional generation of a write-after log (WAL) stream, similar to PostgreSQL. When enabled, this stream records all data changes and transaction outcomes. This write-after log (WAL) is not used for local crash recovery but provides advanced high-availability and recovery features:

* **Point-in-Time Recovery (PITR)**: By archiving the write-after log (WAL) files to a separate location, administrators can restore a database from a physical backup and "replay" the write-after log (WAL) to recover the database to any specific moment in time.  
* **Streaming Replication**: Standby servers can connect to the primary server and consume the write-after log (WAL) stream in real-time. This allows for the maintenance of hot-standby replicas for high availability and read scaling.

Enabling the write-after log (WAL) generates additional I/O overhead but is essential for enterprise-grade disaster recovery and failover capabilities.

## **Section 2: Snapshot Management**

### **Overview**

Snapshot Management is the subsystem responsible for implementing transaction isolation. In an MGA system, each transaction must operate on a consistent, point-in-time view of the database. This view, known as a "snapshot," defines which other transactions' actions are visible and which are not.8 A snapshot effectively freezes the state of the database from the perspective of the transaction, ensuring that data read by the transaction remains stable and is not affected by concurrent commits or rollbacks from other transactions.10

The snapshot is a purely in-memory data structure created at the beginning of a transaction (for REPEATABLE READ or SERIALIZABLE isolation levels) or at the beginning of each statement (for READ COMMITTED isolation level).11 It captures the set of all transactions that were active at that precise moment. This information is then used by the visibility check algorithm to determine whether any given tuple version should be seen by the current transaction.

The core components of a snapshot are xmin, xmax, and xip 12:

* **xmin**: The oldest TransactionId that was still active system-wide when the snapshot was taken. Any transaction with an ID older than xmin is guaranteed to be completed (either committed or aborted) and its status can be definitively determined from the CLOG. This field serves as a powerful performance optimization.  
* **xmax**: The next TransactionId to be allocated. Any transaction with an ID greater than or equal to xmax had not yet started when the snapshot was taken and is therefore invisible.  
* **xip**: An explicit list of TransactionIds that were in-progress (active) at the moment the snapshot was created. This list covers the range between xmin and xmax.

### **In-Memory Data Structures**

The snapshot and the central list of active transactions are managed entirely in shared memory to allow for high-speed access by all concurrent backend processes.

#### **Snapshot Data Structure**

This structure is held in the private memory of each backend process for the duration of its transaction or statement.

C++

/\*\*  
 \* @brief Represents a transaction's consistent view of the database state.  
 \* This structure is populated once per transaction or statement and is used  
 \* for all visibility checks.  
 \*/  
struct TransactionSnapshot {  
    // The current transaction's own ID.  
    uint64\_t my\_xid;

    // Oldest TransactionId that was active when this snapshot was taken.  
    // All transactions with IDs \< xmin are guaranteed to be finished.  
    uint64\_t xmin;

    // The first TransactionId that had not yet been allocated.  
    // All transactions with IDs \>= xmax are invisible.  
    uint64\_t xmax;

    // The number of active transaction IDs in the xip array.  
    uint32\_t xip\_count;

    // An array of TransactionIds that were active at the time of the snapshot.  
    // This array is sorted for efficient searching.  
    uint64\_t\* xip;  
};

#### **Central Transaction Manager Data Structure (ProcArray)**

This is the authoritative, system-wide list of all active backend processes and their associated transaction states. It resides in shared memory and is protected by a lightweight lock to ensure consistent reads during snapshot creation. This structure is modeled after PostgreSQL's "ProcArray".15

C++

\#**include** \<mutex\> // For a conceptual lightweight lock

// Maximum number of concurrent backend processes supported by the system.  
\#**define** MAX\_BACKENDS 128

/\*\*  
 \* @brief Represents the state of a single backend process in shared memory.  
 \*/  
struct ProcessControlBlock {  
    uint32\_t process\_id;      // OS Process ID for this backend.  
    bool is\_active;           // Flag indicating if this slot is in use.  
      
    // The current top-level TransactionId for this backend.  
    // Set to 0 if the backend is not currently inside a transaction block.  
    uint64\_t transaction\_id;

    // The 'xmin' horizon for this backend. This is the oldest TransactionId  
    // that this backend's current snapshot needs to see. It prevents VACUUM  
    // from removing tuples that this backend might still need.  
    uint64\_t backend\_xmin;  
};

/\*\*  
 \* @brief The central, shared-memory structure for tracking all active transactions.  
 \*/  
struct ActiveTransactionList {  
    // A lightweight lock (latch or spinlock) to protect concurrent access.  
    std::mutex latch;

    // The next TransactionId to be assigned globally.  
    // Must be managed with atomic operations.  
    uint64\_t next\_transaction\_id;

    uint32\_t max\_processes;  
    uint32\_t active\_process\_count;

    // The array of all process control blocks.  
    ProcessControlBlock processes;  
};

### **Snapshot Creation Algorithm**

The create\_snapshot() function is the mechanism by which a backend process queries the central ActiveTransactionList to build its private, point-in-time TransactionSnapshot. This operation must be extremely fast and efficient, as it can be called for every single SQL statement in READ COMMITTED mode.

C++

/\*\*  
 \* @brief Creates a new transaction snapshot.  
 \* @param active\_tx\_list A pointer to the shared ActiveTransactionList.  
 \* @return A newly allocated and populated TransactionSnapshot.  
 \*/  
TransactionSnapshot\* create\_snapshot(ActiveTransactionList\* active\_tx\_list) {  
    auto\* snapshot \= new TransactionSnapshot();  
      
    // A temporary, dynamically-sized list to gather active XIDs.  
    std::vector\<uint64\_t\> in\_progress\_xids;

    // 1\. Acquire an exclusive lightweight lock on the ActiveTransactionList.  
    // This is a very short critical section to ensure we get a consistent  
    // view of all active processes at one moment in time.  
    active\_tx\_list-\>latch.lock();

    // 2\. Assign the next available TransactionId to snapshot-\>xmax. This defines  
    // the upper bound of our visibility horizon.  
    // The global next\_transaction\_id must be incremented atomically.  
    snapshot-\>xmax \= active\_tx\_list-\>next\_transaction\_id;

    // 3\. Initialize snapshot-\>xmin to the highest possible value. It will be  
    // lowered as we scan the active processes.  
    snapshot-\>xmin \= snapshot-\>xmax;

    // 4\. Iterate through all possible backend slots to find active transactions.  
    for (uint32\_t i \= 0; i \< active\_tx\_list-\>max\_processes; \++i) {  
        ProcessControlBlock& proc \= active\_tx\_list-\>processes\[i\];  
        if (proc.is\_active) {  
            // Consider the process's current transaction ID.  
            if (proc.transaction\_id\!= 0) {  
                in\_progress\_xids.push\_back(proc.transaction\_id);  
                if (proc.transaction\_id \< snapshot-\>xmin) {  
                    snapshot-\>xmin \= proc.transaction\_id;  
                }  
            }  
            // Also consider the process's own xmin horizon. This is critical  
            // for ensuring that VACUUM doesn't remove data needed by other  
            // transactions' snapshots.  
            if (proc.backend\_xmin\!= 0 && proc.backend\_xmin \< snapshot-\>xmin) {  
                snapshot-\>xmin \= proc.backend\_xmin;  
            }  
        }  
    }

    // 5\. Release the lock as quickly as possible.  
    active\_tx\_list-\>latch.unlock();

    // 6\. Finalize the snapshot data structure.  
    snapshot-\>xip\_count \= in\_progress\_xids.size();  
    if (snapshot-\>xip\_count \> 0) {  
        // Sort the list of in-progress XIDs to allow for efficient  
        // binary searching during visibility checks.  
        std::sort(in\_progress\_xids.begin(), in\_progress\_xids.end());  
          
        snapshot-\>xip \= new uint64\_t\[snapshot-\>xip\_count\];  
        std::copy(in\_progress\_xids.begin(), in\_progress\_xids.end(), snapshot-\>xip);  
    } else {  
        snapshot-\>xip \= nullptr;  
    }

    // The current transaction's ID would be assigned here.  
    // snapshot-\>my\_xid \= get\_current\_transaction\_id();

    return snapshot;  
}

## **Section 3: Tuple Header and Version Chain Management**

### **On-Disk Page and Tuple Layout**

The physical storage of data on disk is the most fundamental layer of the MGA system. ScratchBird DB utilizes a "slotted page" architecture, a robust design employed by PostgreSQL that provides an efficient and flexible way to manage variable-sized tuples on a fixed-size page.17 This architecture decouples the physical location of a tuple on a page from its stable identifier within that page, which is essential for allowing space compaction without invalidating all indexes pointing to the page.

Each 8 KB data page is structured into several distinct regions: a page header, an array of item identifiers (line pointers), free space, and the tuple data itself.

\+-----------------------------------------------------------------+

| DataPageHeader (24 bytes) |  
\+-----------------------------------------------------------------+

| ItemId Array (grows forward \-\>) |  
| \[ItemId\_1\]\[ItemId\_2\]\[ItemId\_3\]... |  
\+-----------------------------------------------------------------+

| |  
| Free Space |  
| |  
\+-----------------------------------------------------------------+

| (\<- grows backward) Tuple Data|  
|... |  
\+-----------------------------------------------------------------+

#### **Data Structures**

The following C++ structures precisely define the on-disk layout of these components.

C++

/\*\*  
 \* @brief An ItemId (or line pointer) is a 4-byte entry in the page header  
 \* that points to a specific tuple on the page. Its index in the array  
 \* (1-based) is the second part of a TupleId.  
 \*/  
struct ItemId {  
    uint32\_t lp\_offset:15; // Byte offset to the start of the tuple from the page start.  
    uint32\_t lp\_flags:2;   // Status: 0=UNUSED, 1=NORMAL, 2=REDIRECT, 3=DEAD.  
    uint32\_t lp\_len:15;    // Total length of the tuple in bytes, including its header.  
};

/\*\*  
 \* @brief The header at the beginning of every 8KB data page.  
 \*/  
struct DataPageHeader {  
    // Write-after log (WAL) information: Log Sequence Number of the last change to this page.  
    uint64\_t pd\_lsn;  
      
    // Checksum to detect on-disk corruption.  
    uint16\_t pd\_checksum;  
      
    // Offset to the end of the ItemId array (start of free space).  
    uint16\_t pd\_lower;  
      
    // Offset to the start of the tuple data area (end of free space).  
    uint16\_t pd\_upper;  
      
    // Other page-level flags and metadata.  
    uint16\_t pd\_flags;  
};

/\*\*  
 \* @brief A TupleId (TID) is the unique physical address of a tuple version.  
 \* It is composed of the page number and the 1-based index of its ItemId.  
 \*/  
struct TupleId {  
    uint32\_t page\_id;  
    uint16\_t item\_id; // 1-based index into the ItemId array on the page.  
};

/\*\*  
 \* @brief The header prepended to every tuple version stored on disk.  
 \* This header contains all the necessary metadata for MVCC visibility checks.  
 \*/  
struct TupleHeaderData {  
    // The TransactionId of the transaction that created this tuple version.  
    uint64\_t tdh\_xmin;

    // The TransactionId of the transaction that deleted or updated this version.  
    // A value of 0 indicates the tuple version is live.  
    uint64\_t tdh\_xmax;

    // In an update chain, this points to the TID of the newer version of the row.  
    // For the newest version or an un-updated row, this is invalid.  
    TupleId  tdh\_next\_version;

    // A bitmask containing various flags about the tuple.  
    // e.g., HEAP\_HASNULL, XMIN\_COMMITTED\_HINT, XMAX\_COMMITTED\_HINT  
    uint16\_t tdh\_infomask;

    // Offset to the start of the actual user data within the tuple.  
    uint8\_t  tdh\_hoff;

    // Number of user attributes (columns) in this tuple.  
    uint16\_t tdh\_natts;

    // Followed by an optional null bitmap and then the user data.  
};

This TupleHeaderData structure is the physical manifestation of the MGA principles. It directly encodes the creation (tdh\_xmin) and deletion (tdh\_xmax) history of each row version, enabling the visibility logic to function.19 The

tdh\_next\_version field provides the explicit link required to traverse update chains, a key requirement for certain operations and for efficient garbage collection.

### **Core Visibility Check Algorithm**

This algorithm is the heart of the MGA implementation. It takes a tuple's header and the current transaction's snapshot and applies the foundational visibility rule to determine if the tuple version is visible. The logic incorporates the xmin optimization, where transactions older than the snapshot's xmin can be quickly identified as committed without consulting the xip array.13

C++

/\*\*  
 \* @brief Determines if a transaction is present in the snapshot's in-progress list.  
 \* Assumes snapshot-\>xip is sorted.  
 \* @param xid The TransactionId to check.  
 \* @param snapshot The current transaction snapshot.  
 \* @return True if the xid is in the xip list, false otherwise.  
 \*/  
bool is\_in\_xip(uint64\_t xid, TransactionSnapshot\* snapshot) {  
    // A binary search would be used here in a real implementation.  
    for (uint32\_t i \= 0; i \< snapshot-\>xip\_count; \++i) {  
        if (snapshot-\>xip\[i\] \== xid) {  
            return true;  
        }  
    }  
    return false;  
}

/\*\*  
 \* @brief Core visibility check based on the foundational rule.  
 \* @param header Pointer to the on-disk tuple header.  
 \* @param snapshot Pointer to the current transaction's snapshot.  
 \* @return True if the tuple version is visible, false otherwise.  
 \*/  
bool is\_tuple\_visible(TupleHeaderData\* header, TransactionSnapshot\* snapshot) {  
    // \--- Rule 1: The creation transaction (xmin) must be visible. \---  
    bool xmin\_is\_visible \= false;  
      
    // Case 1.1: The tuple was created by our own transaction. It is visible.  
    if (header-\>tdh\_xmin \== snapshot-\>my\_xid) {  
        xmin\_is\_visible \= true;  
    } else {  
        // Case 1.2: Check if the creator transaction is committed.  
        TransactionStatus xmin\_status \= getTransactionStatus(header-\>tdh\_xmin);  
        if (xmin\_status \== TRANSACTION\_STATUS\_COMMITTED) {  
            // Case 1.2.1: The creator is older than our snapshot's horizon. Visible.  
            if (header-\>tdh\_xmin \< snapshot-\>xmin) {  
                xmin\_is\_visible \= true;  
            }  
            // Case 1.2.2: The creator is within our snapshot's uncertainty window.  
            // It must not have been in-progress when our snapshot was taken.  
            else if (header-\>tdh\_xmin \< snapshot-\>xmax &&\!is\_in\_xip(header-\>tdh\_xmin, snapshot)) {  
                xmin\_is\_visible \= true;  
            }  
        }  
    }

    if (\!xmin\_is\_visible) {  
        return false;  
    }

    // \--- Rule 2: The deletion transaction (xmax) must NOT be visible. \---  
      
    // Case 2.1: The tuple has not been deleted (xmax is invalid). It is visible.  
    if (header-\>tdh\_xmax \== 0) {  
        return true;  
    }

    // Case 2.2: The tuple was deleted by our own transaction. It is not visible.  
    if (header-\>tdh\_xmax \== snapshot-\>my\_xid) {  
        return false;  
    }

    // Case 2.3: The tuple was deleted by another transaction. We must check if that  
    // deleting transaction is visible to us. If it IS visible, then the deletion  
    // "happened" from our perspective, and the tuple is NOT visible.  
    TransactionStatus xmax\_status \= getTransactionStatus(header-\>tdh\_xmax);  
    if (xmax\_status \== TRANSACTION\_STATUS\_COMMITTED) {  
        // Case 2.3.1: The deleter is older than our snapshot's horizon. Deletion is visible.  
        if (header-\>tdh\_xmax \< snapshot-\>xmin) {  
            return false; // Deletion is visible, so tuple is not.  
        }  
        // Case 2.3.2: The deleter is in our uncertainty window.  
        // If it was NOT in-progress, its deletion is visible.  
        else if (header-\>tdh\_xmax \< snapshot-\>xmax &&\!is\_in\_xip(header-\>tdh\_xmax, snapshot)) {  
            return false; // Deletion is visible, so tuple is not.  
        }  
    }  
      
    // If the deleter transaction was aborted, or is still in progress (and not us),  
    // then its deletion is NOT visible to us. Therefore, the tuple IS visible.  
    return true;  
}

### **DML Operation Algorithms**

Data Manipulation Language (DML) operations like INSERT, DELETE, and UPDATE are implemented not as in-place modifications but as operations that create or logically invalidate tuple versions. This adherence to immutability is what enables readers and writers to operate without blocking each other.8

#### **handle\_insert(tuple\_data)**

1. **Find Space**: Query the table's Free Space Map (FSM) to find a data page with sufficient free space to hold the new tuple. If no such page exists, extend the table file with a new, empty page.  
2. **Acquire Buffer Lock**: Obtain an exclusive lightweight lock on the buffer containing the target data page.  
3. **Allocate Slot**: Add a new ItemId to the page's ItemId array and increment pd\_lower.  
4. **Allocate Tuple Space**: Allocate space for the new tuple (header \+ data) from the top of the tuple data area by decrementing pd\_upper.  
5. **Construct Header**: Create a TupleHeaderData in the allocated space.  
   * Set tdh\_xmin to the current transaction's TransactionId.  
   * Set tdh\_xmax to 0 (invalid), indicating the tuple is live.  
   * Set tdh\_next\_version to an invalid TupleId.  
6. **Write Data**: Copy the user-provided tuple\_data into the space immediately following the new header.  
7. **Finalize Slot**: Update the new ItemId with the final offset and length of the tuple.  
8. **Mark Dirty and Release**: Mark the buffer as dirty and release the buffer lock. The page will be flushed to disk as part of the transaction's commit process.

#### **handle\_delete(tuple\_id)**

1. **Acquire Row Lock**: First, acquire an EXCLUSIVE\_WRITE lock on the logical row identified by tuple\_id using the Lock Manager (see Section 4). This prevents concurrent UPDATE or DELETE operations on the same logical row, thus avoiding the "lost update" anomaly.  
2. **Locate Tuple**: Use the tuple\_id to locate the specific page and ItemId for the target tuple version.  
3. **Acquire Buffer Lock**: Obtain an exclusive lightweight lock on the buffer for that data page.  
4. **Visibility Check**: Fetch the tuple header and verify that the tuple version is visible to the current transaction's snapshot using is\_tuple\_visible(). If it is not visible, the row effectively does not exist for this transaction, and an error should be raised.  
5. **Check for Prior Deletion**: Read the tdh\_xmax field. If it is already non-zero, another transaction has already locked or deleted this version. The current transaction must check the status of that tdh\_xmax transaction. If it is committed, the row is already deleted. If it is still active, the current transaction must wait (which is handled by the acquire\_row\_lock step).  
6. **Mark as Deleted**: Set the tdh\_xmax field in the tuple's header to the current transaction's TransactionId. This is the core action of the delete; it does not remove any data.  
7. **Mark Dirty and Release**: Mark the buffer as dirty and release the buffer lock. The row-level lock from step 1 is held until the transaction commits or aborts.

#### **handle\_update(tuple\_id, new\_data)**

An UPDATE operation in this MGA system is a composition of a DELETE on the old version and an INSERT of the new version.19 This preserves the old version for any concurrent transactions that need to see it.

1. **Acquire Row Lock**: Acquire an EXCLUSIVE\_WRITE lock on the logical row, identical to the first step of handle\_delete.  
2. **Logical Delete**: Perform steps 2-6 of the handle\_delete algorithm on the tuple version identified by tuple\_id. This marks the old version as deleted by the current transaction.  
3. **Insert New Version**: Perform all the steps of the handle\_insert algorithm to create a new tuple version containing new\_data. This new version will have its tdh\_xmin set to the current TransactionId.  
4. **Chain Versions**: After creating the new tuple version and getting its TupleId, go back to the old tuple version (while still holding the buffer lock) and set its tdh\_next\_version field to the TupleId of the new version.  
5. **Mark Dirty and Release**: Both the old and new pages are marked as dirty. The buffer locks are released, and the row-level lock is held until transaction end.

## **Section 4: Concurrency Control and Lock Management**

### **Rationale for a Lock Manager**

While the MGA/MVCC mechanism of snapshots and visibility rules elegantly solves read-write conflicts, it does not, by itself, prevent write-write conflicts. The most notable of these is the "lost update" anomaly:

1. Transaction T1 reads version V1 of a row.  
2. Transaction T2 concurrently reads the same version V1.  
3. T1 computes a new value, performs an UPDATE (creating V2), and commits.  
4. T2, unaware of T1's change, computes its own new value based on the original V1, performs an UPDATE (creating V3), and commits.

The result is that T1's update is lost, as V3 was not based on V2. To prevent this, a higher-level concurrency control mechanism is required. ScratchBird DB employs a row-level lock manager to serialize write operations on the same logical row. Before a transaction can modify a row (via UPDATE or DELETE), it must first acquire an exclusive lock on it. This ensures that concurrent attempts to modify the same row will be forced to wait, preventing lost updates.21

These locks are "heavyweight" locks, managed in a central, shared-memory structure, and are distinct from the short-term "lightweight" locks (latches) used to protect internal data structures like hash table buckets or buffer pages.

### **Lock Manager Data Structures**

The lock manager is implemented as a partitioned hash table residing in shared memory, providing fast lookups while minimizing contention. The hash table maps a unique identifier for a lockable resource (in this case, a TupleId) to a LockObject that describes the current state of the lock.

To represent a logical row, the TupleId used as the key in the lock manager's hash table should be that of the *first* version in an update chain. This provides a stable identifier for the logical row, even as new versions are created.

#### **Core Structures**

C++

\#**include** \<list\>  
\#**include** \<mutex\>

// Defines the modes of locking. For this core specification, only  
// an exclusive write lock is defined.  
enum LockMode {  
    EXCLUSIVE\_WRITE  
};

/\*\*  
 \* @brief Represents a transaction waiting for a lock. This is an entry  
 \* in a LockObject's wait queue.  
 \*/  
struct LockRequest {  
    uint64\_t waiter\_xid; // The TransactionId of the waiting transaction.  
    // In a real implementation, this would also contain a pointer to the  
    // waiting process's control block for wakeup signaling (e.g., a semaphore).  
    void\* process\_handle;   
};

/\*\*  
 \* @brief Represents a lock on a specific tuple in shared memory.  
 \*/  
struct LockObject {  
    TupleId tuple\_id;       // The TID of the tuple being locked.  
    LockMode mode;          // The mode of the lock currently held.  
    uint64\_t owner\_xid;     // The TransactionId of the lock owner.  
      
    // A queue of transactions waiting for this lock.  
    std::list\<LockRequest\> wait\_queue;  
};

/\*\*  
 \* @brief The main lock manager structure in shared memory.  
 \*/  
struct LockManager {  
    // The lock table is partitioned to reduce contention. Each partition  
    // is protected by its own lightweight lock.  
    static const int NUM\_LOCK\_PARTITIONS \= 16;  
      
    std::mutex partition\_latches;  
    std::unordered\_map\<TupleId, LockObject\> lock\_table;  
};

### **Core Locking Algorithms**

#### **acquire\_row\_lock(tuple\_id, lock\_mode)**

This algorithm attempts to acquire a lock on a tuple. If the lock cannot be granted immediately due to a conflict, the transaction is put to sleep until the lock is released.

1. **Determine Partition**: Calculate a hash value from the tuple\_id to select one of the NUM\_LOCK\_PARTITIONS. This distributes lock management activity across multiple data structures and latches, improving concurrency. uint32\_t partition \= hash(tuple\_id) % NUM\_LOCK\_PARTITIONS;.  
2. **Acquire Partition Latch**: Acquire the lightweight lock (partition\_latches\[partition\]) for the chosen partition. This ensures exclusive access to this segment of the lock table.  
3. **Check for Existing Lock**: Search lock\_table\[partition\] for an entry corresponding to tuple\_id.  
4. Case A: No Existing Lock: If no LockObject is found for this tuple\_id:  
   a. Create a new LockObject.  
   b. Set its owner\_xid to the current transaction's ID and its mode to lock\_mode.  
   c. Insert the new LockObject into the hash table.  
   d. Release the partition latch. The lock is acquired successfully. Return SUCCESS.  
5. Case B: Existing Lock: If a LockObject is found:  
   a. Conflict Check: Check if the requested lock\_mode conflicts with the LockObject's current mode. In our simple case, EXCLUSIVE\_WRITE always conflicts with another EXCLUSIVE\_WRITE.  
   b. No Conflict: If there is no conflict (e.g., multiple shared locks), update the LockObject to reflect the new holder and release the latch. Return SUCCESS.  
   c. Conflict: If a conflict exists:  
   i. Create a LockRequest for the current transaction.  
   ii. Add the request to the end of the LockObject's wait\_queue.  
   iii. Release the partition latch.  
   iv. Put the current backend process to sleep (e.g., by waiting on a process-specific semaphore). It will be awakened by the lock-releasing process.  
   v. After waking up, the process must re-attempt the lock acquisition from Step 1, as the lock may have been granted to another waiter in the meantime.

#### **release\_row\_lock(tuple\_id)**

This function is typically called as part of a larger release\_all\_locks\_for\_transaction procedure when a transaction commits or aborts.

1. **Determine Partition**: Calculate the partition hash for tuple\_id as in acquire\_row\_lock.  
2. **Acquire Partition Latch**: Acquire the latch for the partition.  
3. **Find Lock**: Locate the LockObject for tuple\_id in the hash table. If it doesn't exist or is not owned by the current transaction, this is an error condition.  
4. Check Wait Queue:  
   a. Queue is Empty: If the wait\_queue is empty, simply remove the LockObject from the hash table.  
   b. Queue is Not Empty: If there are waiting transactions:  
   i. Dequeue the first LockRequest from the wait\_queue.  
   ii. Update the LockObject's owner\_xid and mode to that of the dequeued waiter.  
   iii. Signal the waiting process (using its process\_handle) to wake up and retry its lock acquisition.  
5. **Release Latch**: Release the partition latch.

### **Deadlock Detection and Resolution**

A deadlock occurs when two or more transactions form a circular chain of dependencies, each waiting for a resource held by the next transaction in the chain. ScratchBird DB will use a timeout-based, dynamic deadlock detection strategy.

1. **Detection Trigger**: Deadlock detection is an expensive process and is not run on every lock wait. Instead, it is triggered only when a transaction has been waiting for a lock for a duration exceeding a configurable deadlock\_timeout (e.g., 1 second).22  
2. **Building the Wait-For Graph (WFG)**: When detection is triggered for a waiting transaction (T\_waiter), the system builds a "wait-for" graph in memory.23  
   * The nodes of the graph are TransactionIds.  
   * A directed edge is drawn from T\_A to T\_B if T\_A is currently waiting for a lock held by T\_B.  
   * This information is gathered by traversing the LockObject structures in the lock manager. For T\_waiter, we find the LockObject it's waiting on and identify the owner\_xid (T\_owner). This creates the edge T\_waiter \-\> T\_owner. The algorithm then recursively checks what T\_owner is waiting for, and so on.  
3. **Cycle Detection**: The core of the algorithm is a depth-first search starting from T\_waiter. During the traversal, the algorithm keeps track of the nodes in the current path. If the search ever encounters a node that is already in the current path, a cycle has been found, and a deadlock is confirmed.  
4. **Resolution**: Once a deadlock is detected, the system must break the cycle by choosing a "victim" transaction to abort. The simplest and most common strategy is to abort the transaction that initiated the deadlock check.25  
   * The victim transaction is terminated, and an error is returned to its client application.  
   * The abort process will trigger the release of all locks held by the victim transaction via release\_all\_locks\_for\_transaction.  
   * This lock release breaks the cycle, allowing the other transaction(s) in the deadlock to proceed.

## **Section 5: Garbage Collection (Vacuum) Subsystem**

### **Overview**

The Multi-Generational Architecture, by its nature of never updating tuple versions in-place, generates obsolete data. Every UPDATE creates a new tuple version and marks the old one as dead. Every DELETE marks a tuple version as dead. These "dead tuples" remain in the data files, consuming disk space and degrading query performance by forcing scans to process more irrelevant data. The Garbage Collection subsystem, known as VACUUM, is the essential background process responsible for identifying and reclaiming the space occupied by these dead tuples.26

The VACUUM process in ScratchBird DB has two primary, equally critical functions:

1. **Space Reclamation**: It scans tables to find dead tuples that are no longer visible to any active or future transaction and makes their space available for reuse. This process updates the table's Free Space Map (FSM).27  
2. **Prevention of Transaction ID Wraparound**: It identifies very old tuple versions and "freezes" their creation TransactionId, replacing it with a special, permanent value. This is a crucial maintenance task that prevents data corruption when the main 64-bit TransactionId counter eventually wraps around.28

A fundamental design requirement is that the standard VACUUM operation must run concurrently with normal database activity. It cannot take exclusive locks that would block readers or writers, ensuring that routine maintenance does not disrupt application workload.27

### **Phase 1: Determine Horizon**

The cornerstone of safe garbage collection is the "vacuum horizon." The system cannot remove any tuple version that might still be visible to any currently running transaction. The VACUUM process must first establish a TransactionId before which all transactions are guaranteed to be completed. This ID is known as the vacuum horizon, or horizon\_xmin.

#### **Algorithm: find\_vacuum\_horizon()**

This algorithm determines the oldest TransactionId that is still of interest to any active part of the system.

1. **Acquire Lock on ProcArray**: Obtain a shared lightweight lock on the ActiveTransactionList (the ProcArray). A shared lock is sufficient as we are only reading the state.  
2. **Initialize Horizon**: Initialize a local variable, horizon\_xmin, to the next TransactionId that will be assigned (ActiveTransactionList.next\_transaction\_id).  
3. Iterate Active Backends: Scan through the ActiveTransactionList.processes array. For each active process:  
   a. If the process has an active transaction (process.transaction\_id\!= 0), update the horizon: horizon\_xmin \= min(horizon\_xmin, process.transaction\_id).  
   b. Crucially, also consider the snapshot horizon of each process (process.backend\_xmin). This value represents the oldest data that the process's current snapshot needs to see. Update the horizon accordingly: horizon\_xmin \= min(horizon\_xmin, process.backend\_xmin).29  
4. **Consider Replication Slots**: In a replicated environment, replication slots also maintain an xmin value, indicating the oldest transaction that a standby server or logical subscriber needs. The global horizon\_xmin must also be the minimum of its current value and the xmin of all active replication slots.29  
5. **Release Lock**: Release the lock on the ActiveTransactionList.  
6. **Return Horizon**: The final horizon\_xmin value is returned. Any tuple version whose tdh\_xmax is older than this horizon and was set by a committed transaction is a potential candidate for removal.

The calculation of this horizon demonstrates a critical system-wide dependency: a single long-running transaction can prevent VACUUM from cleaning up dead tuples across the entire database, leading to table bloat.

### **Phase 2: Identify Dead Tuples**

With the horizon\_xmin established, the VACUUM process can scan tables to identify removable dead tuples.

#### **Algorithm: Scan and Identify**

1. **Table Scan**: The VACUUM worker begins a sequential scan of the target table's heap pages.  
2. **Visibility Map Optimization**: Before reading a page, the worker consults the table's Visibility Map (VM). If the VM indicates that all tuples on a page are visible to all transactions, the page contains no dead tuples and can be skipped entirely, significantly speeding up the process.27  
3. **Page Processing**: For each page that is not skipped, the worker acquires an exclusive lightweight lock on the page's buffer. It then iterates through each ItemId on the page.  
4. **Tuple Check**: For each valid tuple version, it calls is\_dead\_to\_all() to determine if it can be removed.  
5. **Collect TIDs**: If a tuple is identified as dead, its TupleId is added to a local in-memory list of dead TIDs. This list is constrained by a memory limit (e.g., maintenance\_work\_mem).

#### **Algorithm: is\_dead\_to\_all(header, horizon\_xmin)**

This function contains the precise logic for determining if a tuple version is obsolete.

C++

/\*\*  
 \* @brief Checks if a tuple version is dead and removable with respect to the vacuum horizon.  
 \* @param header Pointer to the tuple's header.  
 \* @param horizon\_xmin The calculated vacuum horizon.  
 \* @return True if the tuple can be removed, false otherwise.  
 \*/  
bool is\_dead\_to\_all(TupleHeaderData\* header, uint64\_t horizon\_xmin) {  
    // A tuple is only a candidate for removal if it has been marked as deleted.  
    if (header-\>tdh\_xmax \== 0) {  
        return false; // Tuple is live.  
    }

    // The deleting transaction must be older than the oldest active transaction's horizon.  
    // If it's not, some active transaction might still need to see this version as live.  
    if (header-\>tdh\_xmax \>= horizon\_xmin) {  
        return false;  
    }

    // Now that we know the deleting transaction is old enough, we must confirm it committed.  
    // If it aborted, the deletion never logically occurred, and this tuple version  
    // might still be the live version of the row.  
    TransactionStatus status \= getTransactionStatus(header-\>tdh\_xmax);  
    if (status \== TRANSACTION\_STATUS\_COMMITTED) {  
        return true; // Deleted by a committed transaction older than the horizon. Safe to remove.  
    }

    // If the transaction aborted or is somehow still marked active (which would be  
    // an inconsistency if xmax \< horizon\_xmin), it is not dead.  
    return false;  
}

### **Phase 3: Reclaim Space**

After scanning a portion of the table and collecting a list of dead TupleIds, VACUUM proceeds to reclaim the space. This is a multi-step process that involves cleaning both the heap pages and any associated indexes.

1. **Index Cleanup**: The VACUUM worker first processes all indexes associated with the table. It scans the indexes and removes any index entries that point to the TupleIds in its collected list of dead tuples.  
2. **Heap Cleanup**: After cleaning the indexes, the worker revisits the heap pages containing the dead tuples.  
3. **Mark Slot as Unused**: For each dead tuple on a page, its corresponding ItemId in the page header has its lp\_flags changed to LP\_DEAD. This logically frees the slot. The physical space occupied by the tuple data is now considered free space within the page.  
4. **Update Free Space Map (FSM)**: After processing a page, the VACUUM worker calculates the new amount of free space on that page. It then updates the table's FSM with this new value.31 The FSM is a critical structure that allows subsequent  
   INSERT operations to quickly find pages with available space, preventing unnecessary table growth.  
5. **Page Compaction (Optional)**: While not part of a standard VACUUM, a more aggressive process (VACUUM FULL) could rewrite the page entirely, compacting the live tuples and removing the dead space completely. This, however, requires an exclusive lock on the table. The standard VACUUM simply marks the space as reusable.  
6. **Table Truncation**: After processing all pages, VACUUM can attempt to truncate empty pages from the end of the table file, returning the disk space to the operating system. This requires a brief exclusive lock on the table.27

This entire process is made safe for concurrent readers because the visibility rules prevent them from ever seeing a tuple version that VACUUM has identified as dead. A reader's snapshot will always be newer than or concurrent with the horizon\_xmin, so any tuple with a committed tdh\_xmax older than that horizon will correctly be evaluated as invisible by the is\_tuple\_visible algorithm.

#### **Works cited**

1. Documentation: 17: 3.4. Transactions \- PostgreSQL, accessed September 15, 2025, [https://www.postgresql.org/docs/current/tutorial-transactions.html](https://www.postgresql.org/docs/current/tutorial-transactions.html)  
2. 01-documentation:01-10-firebird-command-line-utilities:firebird-interactive-sql-utility:transaction-handling \[IBExpert\], accessed September 15, 2025, [https://ibexpert.com/docu/doku.php?id=01-documentation:01-10-firebird-command-line-utilities:firebird-interactive-sql-utility:transaction-handling](https://ibexpert.com/docu/doku.php?id=01-documentation:01-10-firebird-command-line-utilities:firebird-interactive-sql-utility:transaction-handling)  
3. 5.4. Commit Log (clog) :: Hironobu SUZUKI @ InterDB, accessed September 15, 2025, [https://www.interdb.jp/pg/pgsql05/04.html](https://www.interdb.jp/pg/pgsql05/04.html)  
4. 01-documentation:01-13-miscellaneous:glossary:transaction-inventory-page \[IBExpert\], accessed September 15, 2025, [https://ibexpert.com/docu/doku.php?id=01-documentation:01-13-miscellaneous:glossary:transaction-inventory-page](https://ibexpert.com/docu/doku.php?id=01-documentation:01-13-miscellaneous:glossary:transaction-inventory-page)  
5. Transaction Processing in PostgreSQL, accessed September 15, 2025, [https://www.postgresql.org/files/developer/transactions.pdf](https://www.postgresql.org/files/developer/transactions.pdf)  
6. src backend access transam clog.c \- PostgreSQL Source Code, accessed September 15, 2025, [https://doxygen.postgresql.org/clog\_8c\_source.html](https://doxygen.postgresql.org/clog_8c_source.html)  
7. Firebird Internals: Inside a Firebird Database, accessed September 15, 2025, [https://firebirdsql.org/file/documentation/html/en/firebirddocs/firebirdinternals/firebird-internals.html](https://firebirdsql.org/file/documentation/html/en/firebirddocs/firebirdinternals/firebird-internals.html)  
8. Getting Started with Multiversion Concurrency Control (MVCC) in PostgreSQL \- DbVisualizer, accessed September 15, 2025, [https://www.dbvis.com/thetable/getting-started-with-multiversion-concurrency-control-mvcc-in-postgresql/](https://www.dbvis.com/thetable/getting-started-with-multiversion-concurrency-control-mvcc-in-postgresql/)  
9. Multiversion concurrency control \- Wikipedia, accessed September 15, 2025, [https://en.wikipedia.org/wiki/Multiversion\_concurrency\_control](https://en.wikipedia.org/wiki/Multiversion_concurrency_control)  
10. Serializable Snapshot Isolation in PostgreSQL, accessed September 15, 2025, [https://www.eecs.umich.edu/courses/cse584/static\_files/papers/snapshot-psql.pdf](https://www.eecs.umich.edu/courses/cse584/static_files/papers/snapshot-psql.pdf)  
11. Mvcc Unmasked \- Bruce Momjian, accessed September 15, 2025, [https://momjian.us/main/writings/pgsql/mvcc.pdf](https://momjian.us/main/writings/pgsql/mvcc.pdf)  
12. 5.5. Transaction Snapshot \- Hironobu SUZUKI @ InterDB, accessed September 15, 2025, [https://www.interdb.jp/pg/pgsql05/05.html](https://www.interdb.jp/pg/pgsql05/05.html)  
13. Introduction to Snapshots and Tuple Visibility in PostgreSQL \- Jan Nidzwetzki, accessed September 15, 2025, [https://jnidzwetzki.github.io/2024/04/03/postgres-and-snapshots.html](https://jnidzwetzki.github.io/2024/04/03/postgres-and-snapshots.html)  
14. A Deep Dive into PostgreSQL Visibility Management | by Zack \- Level Up Coding, accessed September 15, 2025, [https://levelup.gitconnected.com/a-deep-dive-into-postgresql-visibility-management-18c2fd6746e4](https://levelup.gitconnected.com/a-deep-dive-into-postgresql-visibility-management-18c2fd6746e4)  
15. www.dbmarlin.com, accessed September 15, 2025, [https://www.dbmarlin.com/docs/kb/wait-events/postgresql/procarray/\#:\~:text=In%20PostgreSQL%2C%20the%20ProcArray%20is,active%20backend%20processes%20(transactions).](https://www.dbmarlin.com/docs/kb/wait-events/postgresql/procarray/#:~:text=In%20PostgreSQL%2C%20the%20ProcArray%20is,active%20backend%20processes%20\(transactions\).)  
16. ProcArray | DBmarlin Docs and Knowledge Base, accessed September 15, 2025, [https://www.dbmarlin.com/docs/kb/wait-events/postgresql/procarray/](https://www.dbmarlin.com/docs/kb/wait-events/postgresql/procarray/)  
17. PostgreSQL/Page Layout \- Wikibooks, open books for an open world, accessed September 15, 2025, [https://en.wikibooks.org/wiki/PostgreSQL/Page\_Layout](https://en.wikibooks.org/wiki/PostgreSQL/Page_Layout)  
18. 1.3. Internal Layout of a Heap Table File \- Hironobu SUZUKI @ InterDB, accessed September 15, 2025, [https://www.interdb.jp/pg/pgsql01/03.html](https://www.interdb.jp/pg/pgsql01/03.html)  
19. Deep Dive into PostgreSQL: Pages, Tuples, and Updates | by Duc Tran \- Medium, accessed September 15, 2025, [https://medium.com/@tranaduc9x/deep-dive-into-postgresql-pages-tuples-and-updates-cf9122f5f743](https://medium.com/@tranaduc9x/deep-dive-into-postgresql-pages-tuples-and-updates-cf9122f5f743)  
20. 5.2 Tuple Structure \- Hironobu SUZUKI @ InterDB, accessed September 15, 2025, [https://www.interdb.jp/pg/pgsql05/02.html](https://www.interdb.jp/pg/pgsql05/02.html)  
21. Row locks in PostgreSQL, accessed September 15, 2025, [https://www.cybertec-postgresql.com/en/row-locks-in-postgresql/](https://www.cybertec-postgresql.com/en/row-locks-in-postgresql/)  
22. PostgreSQL: Understanding deadlocks, accessed September 15, 2025, [https://www.cybertec-postgresql.com/en/postgresql-understanding-deadlocks/](https://www.cybertec-postgresql.com/en/postgresql-understanding-deadlocks/)  
23. Detecting and Resolving Deadlocks in PostgreSQL Databases \- Squash.io, accessed September 15, 2025, [https://www.squash.io/detecting-and-resolving-deadlocks-in-postgresql-databases/](https://www.squash.io/detecting-and-resolving-deadlocks-in-postgresql-databases/)  
24. Deadlock reimplementation notes (kinda long) \- PostgreSQL, accessed September 15, 2025, [https://www.postgresql.org/message-id/5946.979867205@sss.pgh.pa.us](https://www.postgresql.org/message-id/5946.979867205@sss.pgh.pa.us)  
25. Deadlocks Guide \- SQL Server \- Microsoft Learn, accessed September 15, 2025, [https://learn.microsoft.com/en-us/sql/relational-databases/sql-server-deadlocks-guide?view=sql-server-ver17](https://learn.microsoft.com/en-us/sql/relational-databases/sql-server-deadlocks-guide?view=sql-server-ver17)  
26. PostgreSQL VACUUM Guide and Best Practices \- EDB, accessed September 15, 2025, [https://www.enterprisedb.com/blog/postgresql-vacuum-and-analyze-best-practice-tips](https://www.enterprisedb.com/blog/postgresql-vacuum-and-analyze-best-practice-tips)  
27. Documentation: 17: VACUUM \- PostgreSQL, accessed September 15, 2025, [https://www.postgresql.org/docs/current/sql-vacuum.html](https://www.postgresql.org/docs/current/sql-vacuum.html)  
28. Managing Transaction ID Exhaustion (Wraparound) in PostgreSQL | Crunchy Data Blog, accessed September 15, 2025, [https://www.crunchydata.com/blog/managing-transaction-id-wraparound-in-postgresql](https://www.crunchydata.com/blog/managing-transaction-id-wraparound-in-postgresql)  
29. Postgres VACUUM and Xmin Horizon \- Keiko in Somewhere, accessed September 15, 2025, [https://blog.keikooda.net/2023/03/05/xmin-horizon/](https://blog.keikooda.net/2023/03/05/xmin-horizon/)  
30. “Understanding FSM and VM in PostgreSQL: How Free Space and Visibility Maps Optimize Performance” | by Kerian Amin N | Medium, accessed September 15, 2025, [https://medium.com/@ngukerian/understanding-fsm-and-vm-in-postgresql-how-free-space-and-visibility-maps-optimize-performance-2f2ac32b8113](https://medium.com/@ngukerian/understanding-fsm-and-vm-in-postgresql-how-free-space-and-visibility-maps-optimize-performance-2f2ac32b8113)  
31. Free Space Mapping file in details | David's Blog, accessed September 15, 2025, [https://idrawone.github.io/2020/10/23/Heap-freespace-in-details/](https://idrawone.github.io/2020/10/23/Heap-freespace-in-details/)  
32. Hire Firebird Database Programmer from India | Firebird Administrator \- UMANG Software, accessed September 15, 2025, [https://umangsoftware.com/firebird-technology/](https://umangsoftware.com/firebird-technology/)  
33. Firebird (database server) \- Wikipedia, accessed September 15, 2025, [https://en.wikipedia.org/wiki/Firebird\_(database\_server)](https://en.wikipedia.org/wiki/Firebird_\(database_server\))  
34. Insert your talk title \- Firebird, accessed September 15, 2025, [https://firebirdsql.org/file/community/conference-2016/firebird-and-disk-i-o.pdf](https://firebirdsql.org/file/community/conference-2016/firebird-and-disk-i-o.pdf)  
35. Inside Firebird transactions \- IBPhoenix, accessed September 15, 2025, [https://www.ibphoenix.com/files/conf2019/03Transactions-internals.pdf](https://www.ibphoenix.com/files/conf2019/03Transactions-internals.pdf)
