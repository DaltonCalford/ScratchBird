# Backup and Restore Specification

## 1. Overview

This document provides the complete specification for the backup and restore functionality in ScratchBird. The backup and restore system is designed to be robust, flexible, and performant, with support for full, incremental, compressed, and encrypted backups.

## 2. Backup Format

A ScratchBird backup is a single file that contains a complete and consistent snapshot of the database at a specific point in time. The backup file is composed of a header followed by a series of data blocks.

### 2.1. Backup File Header

The backup file header is a fixed-size structure that contains metadata about the backup.

| Offset | Size | Data Type | Description |
| :--- | :--- | :--- | :--- |
| 0 | 8 | `char[8]` | Magic Number: `SBBACKUP` |
| 8 | 4 | `uint32_t` | Version: The version of the backup format. |
| 12 | 16 | `uint8_t[16]` | Database ID: The unique ID of the database that was backed up. |
| 28 | 8 | `uint64_t` | Creation Timestamp: The timestamp when the backup was created (in microseconds since Unix epoch). |
| 36 | 4 | `uint32_t` | Backup Type: 0 for full, 1 for incremental. |
| 40 | 4 | `uint32_t` | Compression Type: 0 for none, 1 for LZ4, 2 for Zstd. |
| 44 | 4 | `uint32_t` | Encryption Type: 0 for none, 1 for AES-256. |
| 48 | 4 | `uint32_t` | Page Size: The page size of the database. |
| 52 | 8 | `uint64_t` | Number of Pages: The total number of pages in the backup. |
| 60 | 8 | `uint64_t` | LSN of Backup: The Log Sequence Number at the time of the backup. |

### 2.2. Data Blocks

Following the header, the backup file contains a series of data blocks. Each data block corresponds to a single page in the database. The format of a data block is:

- **Page ID (4 bytes):** The ID of the page.
- **Data Length (4 bytes):** The length of the page data.
- **Page Data (variable):** The raw data of the page.

For incremental backups, only the pages that have changed since the LSN of the last backup are included in the data blocks.

## 3. Backup Process

The backup process is initiated by calling the `sb_backup_start` function from the C API. The process is as follows:

1.  **Start Backup:** The `sb_backup_start` function is called with the path to the backup file and a set of backup options.
2.  **Create Backup File:** The backup system creates the backup file and writes the backup header.
3.  **Backup Pages:** The `sb_backup_step` function is called repeatedly to back up the pages of the database. This function can be called in a loop to back up a specified number of pages at a time, allowing for progress reporting and cancellation.
4.  **Finish Backup:** The `sb_backup_finish` function is called to finalize the backup process. This function writes any remaining data to the backup file and closes the file.

### 3.1. Backup Options

The `sb_backup_options_t` struct allows for the following options:

- **Incremental:** If true, an incremental backup will be created. This requires a previous full backup to be available.
- **Compressed:** If true, the backup data will be compressed using the specified compression algorithm.
- **Encrypted:** If true, the backup data will be encrypted using the specified encryption algorithm.

## 4. Restore Process

The restore process is initiated by calling the `sb_restore` function from the C API. The process is as follows:

1.  **Start Restore:** The `sb_restore` function is called with the path to the backup file and a set of restore options.
2.  **Read Backup File:** The restore system reads the backup file header to verify the backup format and get the necessary metadata.
3.  **Create New Database:** A new database file is created with the same page size as the original database.
4.  **Restore Pages:** The restore system reads the data blocks from the backup file and writes the page data to the new database file. For an incremental restore, the system will first restore the base full backup, and then apply the changes from the incremental backups in order.
5.  **Finalize Restore:** After all the pages have been restored, the restore system finalizes the new database file, making it ready for use.

## 5. Error Handling and Recovery

- **Backup Interruption:** If a backup is interrupted, the backup file will be incomplete and should be discarded.
- **Restore Interruption:** If a restore is interrupted, the new database file will be in an inconsistent state and should be discarded.
- **Corrupted Backup File:** The restore process will fail if the backup file is corrupted. The magic number and checksums are used to detect corruption.

## 6. C API Reference

For detailed information on the C API for backup and restore, please see the `docs/specifications/C_API_SPECIFICATION.md` document.