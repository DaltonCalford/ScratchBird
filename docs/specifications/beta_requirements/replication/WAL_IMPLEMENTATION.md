# Write-after Log (WAL) Implementation Specification

## 1. Overview

This document provides the complete specification for the write-after log (WAL) system in ScratchBird. The write-after log (WAL) stream is an optional replication/PITR mechanism and is **not** used for local crash recovery (MGA provides recovery semantics).

## 2. Write-after Log (WAL) Record Format

A write-after log (WAL) record represents a single change to a page in the database. The format of a write-after log (WAL) record is as follows:

| Offset | Size | Data Type | Description |
| :--- | :--- | :--- | :--- |
| 0 | 8 | `uint64_t` | LSN (Log Sequence Number): A unique, monotonically increasing 64-bit integer that identifies the write-after log (WAL) record. |
| 8 | 8 | `uint64_t` | Previous LSN: The LSN of the previous write-after log (WAL) record for the same page. |
| 16 | 4 | `uint32_t` | Record Type: The type of operation represented by the write-after log (WAL) record. |
| 20 | 4 | `uint32_t` | Page ID: The ID of the page that was modified. |
| 24 | 4 | `uint32_t` | Data Length: The length of the data in the write-after log (WAL) record. |
| 28 | variable | `uint8_t[]` | Data: The actual data that was changed. For a page update, this would be the new version of the page. |
| 28 + Data Length | 4 | `uint32_t` | Checksum: A checksum of the write-after log (WAL) record to ensure its integrity. |

### 2.1. Record Types

- **`PAGE_UPDATE`**: A page was updated. The `Data` field contains the new page content.
- **`TRANSACTION_COMMIT`**: A transaction was committed.
- **`TRANSACTION_ABORT`**: A transaction was aborted.
- **`CHECKPOINT_START`**: A checkpoint is starting.
- **`CHECKPOINT_END`**: A checkpoint has completed.

## 3. Write-after Log (WAL) File Format

The write-after log (WAL) is a sequence of files that are written to disk. Each write-after log (WAL) file has a fixed size (e.g., 16MB) and contains a series of write-after log (WAL) records. The format of a write-after log (WAL) file is as follows:

### 3.1. Write-after Log (WAL) File Header

The write-after log (WAL) file header is a fixed-size structure at the beginning of each write-after log (WAL) file.

| Offset | Size | Data Type | Description |
| :--- | :--- | :--- | :--- |
| 0 | 8 | `char[8]` | Magic Number: `SBWAL` |
| 8 | 4 | `uint32_t` | Version: The version of the write-after log (WAL) format. |
| 12 | 8 | `uint64_t` | Start LSN: The LSN of the first record in this file. |

### 3.2. Write-after Log (WAL) Records

Following the header, the write-after log (WAL) file contains a sequence of write-after log (WAL) records, as described in section 2.

## 4. Writing to the Write-after Log (WAL)

Write-after log (WAL) records are written to the write-after log (WAL) buffer in memory before being flushed to disk. The process is as follows:

1.  **Record Creation:** When a page is modified, a `PAGE_UPDATE` write-after log (WAL) record is created in the write-after log (WAL) buffer.
2.  **Transaction Commit:** When a transaction is committed, a `TRANSACTION_COMMIT` record is written to the write-after log (WAL) buffer.
3.  **Flushing to Disk:** The write-after log (WAL) buffer is flushed to disk at certain points, such as when a transaction commits or when the buffer is full. This ensures that the write-after log (WAL) records are durable before the corresponding changes are made to the database file.

## 5. Checkpointing

A checkpoint is a point in time at which all dirty pages in the buffer pool are flushed to disk. This limits the amount of work that needs to be done during recovery.

The checkpoint process is as follows:

1.  **Start Checkpoint:** A `CHECKPOINT_START` record is written to the write-after log (WAL).
2.  **Flush Dirty Pages:** All dirty pages in the buffer pool are written to the database file.
3.  **End Checkpoint:** A `CHECKPOINT_END` record is written to the write-after log (WAL). This record contains the LSN of the `CHECKPOINT_START` record.

## 6. Recovery Process

If the database server crashes, the recovery process is initiated when the server is restarted. The recovery process uses the write-after log (WAL) to restore the database to a consistent state. The process is as follows:

1.  **Find Last Checkpoint:** The recovery process finds the last successful `CHECKPOINT_END` record in the write-after log (WAL).
2.  **Redo Phase:** The recovery process replays the write-after log (WAL) records from the LSN of the `CHECKPOINT_START` record forward. For each `PAGE_UPDATE` record, it checks the LSN of the corresponding page in the database file. If the page's LSN is less than the write-after log (WAL) record's LSN, the page is updated with the data from the write-after log (WAL) record.
3.  **Undo Phase (Not Required for MGA):** In a traditional write-after log (WAL) system, an undo phase would be required to roll back uncommitted transactions. However, because ScratchBird uses MGA, the undo phase is not necessary. The MGA system handles the visibility of uncommitted changes.

## 7. Interaction with MGA

The write-after log (WAL) system and the MGA transaction system work together to provide both durability and high concurrency. The key points of their interaction are:

- **MGA is Primary:** The MGA system is the primary mechanism for transaction isolation and concurrency control. It allows for non-blocking reads and efficient handling of multiple concurrent transactions.
- **Write-after log (WAL) is Secondary:** The write-after log (WAL) system is a secondary mechanism that provides durability. It ensures that committed changes are not lost in the event of a crash.
- **No Undo in write-after log (WAL):** The write-after log (WAL) does not need to store undo information because the MGA system handles the rollback of uncommitted transactions.
- **LSN in Page Headers:** Each page in the database contains the LSN of the last write-after log (WAL) record that modified it. This is used during recovery to determine if a page needs to be updated.
