/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/uuidv7.h"

namespace scratchbird::core
{

/**
 * Tablespace - Multi-file database support for ScratchBird
 *
 * Tablespaces enable:
 * - Multi-file database capacity expansion beyond single-file limits
 * - Storage tiering (place hot data on SSD, archive data on HDD)
 * - Data lifecycle management (detach/attach tablespace files for archival)
 * - Partition-based data distribution (different partitions on different tablespaces)
 * - Cross-database tablespace attachment (with validation)
 *
 * Architecture:
 * - Each tablespace is a separate .sbts file on disk
 * - Pages addressed via GPID (Global Page ID): 64-bit = 16-bit tablespace_id + 48-bit page_number
 * - Tablespace 0 = primary database file (.sbdb)
 * - Tablespace 1-65535 = custom tablespaces (.sbts)
 * - Preserves Firebird MGA stable TID invariant (TIDs never change)
 *
 * References:
 * - Specification: docs/specifications/TABLESPACE_SPECIFICATION.md
 * - Implementation Plan: docs/planning/TABLESPACE_IMPLEMENTATION_PLAN.md
 */

// Forward declarations
class Database;
class ErrorContext;

/**
 * TablespaceHeader - Page 0 of every tablespace file
 *
 * Contains metadata about the tablespace:
 * - Identification (name, UUIDs)
 * - Configuration (autoextend parameters)
 * - File layout (total pages, FSM location)
 * - Transaction tracking (OIT for MGA consistency)
 *
 * Size: Padded to page boundary (page_size bytes)
 */
#pragma pack(push, 1)
struct TablespaceHeaderV1
{
    PageHeader page_header; // Standard 80-byte header per ON_DISK_FORMAT.md v1.4.0 (includes table_id)

    // === Identification (64 bytes) ===
    char tablespace_name[32];      // Tablespace name (max 31 chars + null terminator)
    UuidV7Bytes tablespace_uuid;   // UUID v7 for this tablespace (16 bytes)
    UuidV7Bytes database_uuid;     // Database UUID (16 bytes) - for validation during attach

    // === Configuration (64 bytes) ===
    uint32_t tablespace_id;        // Tablespace ID (1-65535, 0 reserved for primary file)
    uint32_t page_size;            // Must match database page_size (e.g., 16384)
    uint64_t creation_time;        // Unix timestamp in microseconds
    uint64_t last_checkpoint;      // Last checkpoint timestamp (microseconds)
    uint32_t autoextend_enabled;   // 1 = autoextend enabled, 0 = disabled
    uint32_t autoextend_size_mb;   // Extend by N MB each time (default: 100)
    uint64_t max_size_mb;          // Maximum size in MB (0 = unlimited)
    uint64_t reserved1[3];         // Reserved for future use

    // === File Layout (32 bytes) ===
    uint64_t total_pages;          // Total pages in this tablespace file
    uint64_t free_pages;           // Number of free pages (tracked in FSM)
    uint64_t next_page_number;     // Next page to allocate (hint for FSM)
    uint64_t fsm_root_page;        // FSM root page for this tablespace (usually page 1)

    // === Transaction Info (32 bytes) ===
    // These are synced with primary database for MVCC consistency
    uint64_t oldest_transaction_id;  // Oldest Interesting Transaction (OIT)
    uint64_t latest_completed_xid;   // Latest completed transaction
    uint64_t reserved2[2];           // Reserved for future use

    // === Padding ===
    // Padded to page boundary at runtime based on page_size
    // Padding size = page_size - sizeof(fixed fields above)
    // Calculated: page_size - (80 + 64 + 64 + 32 + 32) = page_size - 272
};
#pragma pack(pop)

constexpr uint16_t TABLESPACE_HEADER_VERSION_V1 = 1;
constexpr uint16_t TABLESPACE_HEADER_VERSION_V2 = 2;

#pragma pack(push, 1)
struct TablespaceHeader
{
    PageHeader page_header; // Standard 80-byte header per ON_DISK_FORMAT.md v1.4.0 (includes table_id)

    // === Identification (96 bytes) ===
    char tablespace_name[64];      // Tablespace name (max 63 chars + null terminator)
    UuidV7Bytes tablespace_uuid;   // UUID v7 for this tablespace (16 bytes)
    UuidV7Bytes database_uuid;     // Database UUID (16 bytes) - for validation during attach

    // === Configuration (64 bytes) ===
    uint32_t tablespace_id;        // Tablespace ID (0 = primary, 1 = reserved, 2-65535 custom)
    uint32_t page_size;            // Must match database page_size (e.g., 16384)
    uint64_t creation_time;        // Unix timestamp in microseconds
    uint64_t last_checkpoint;      // Last checkpoint timestamp (microseconds)
    uint32_t autoextend_enabled;   // 1 = autoextend enabled, 0 = disabled
    uint32_t autoextend_size_mb;   // Extend by N MB each time (default: 100)
    uint64_t max_size_mb;          // Maximum size in MB (0 = unlimited)
    uint64_t reserved1[3];         // Reserved for future use

    // === File Layout (32 bytes) ===
    uint64_t total_pages;          // Total pages in this tablespace file
    uint64_t free_pages;           // Number of free pages (tracked in FSM)
    uint64_t next_page_number;     // Next page to allocate (hint for FSM)
    uint64_t fsm_root_page;        // FSM root page for this tablespace (usually page 1)

    // === Transaction Info (32 bytes) ===
    // These are synced with primary database for MVCC consistency
    uint64_t oldest_transaction_id;  // Oldest Interesting Transaction (OIT)
    uint64_t latest_completed_xid;   // Latest completed transaction
    uint64_t reserved2[2];           // Reserved for future use

    // === Padding ===
    // Padded to page boundary at runtime based on page_size
};
#pragma pack(pop)

/**
 * SBTablespaceCatalog - pg_tablespace system table entry
 *
 * Stored in the pg_tablespace catalog table (in primary database file).
 * Tracks all tablespaces and their configuration.
 */
#pragma pack(push, 1)
struct SBTablespaceCatalog
{
    // === Header ===
    uint8_t is_valid;              // 1 = valid entry, 0 = deleted (soft delete)
    uint8_t reserved1[7];          // Padding for alignment

    // === Identification (82 bytes) ===
    uint16_t tablespace_id;        // Tablespace ID (0 = primary, 1 = reserved, 2-65535 custom)
    char tablespace_name[64];      // Name (null-terminated, max 63 chars)
    UuidV7Bytes tablespace_uuid;   // UUID v7 (16 bytes)

    // === Configuration (24 bytes) ===
    uint32_t autoextend_enabled;   // 1 = autoextend enabled, 0 = disabled
    uint32_t autoextend_size_mb;   // Extend size in MB (default: 100)
    uint64_t max_size_mb;          // Maximum size in MB (0 = unlimited)
    uint32_t prealloc_pages;       // Pages preallocated during creation (0 = none)
    uint32_t flags;                // Bitmask of flags (reserved for future use)

    // === File Information (264 bytes) ===
    char primary_path[256];        // Absolute path to primary tablespace file (.sbts)
    uint32_t file_count;           // Number of files in this tablespace (usually 1)
    uint32_t reserved2;            // Padding

    // === Statistics (64 bytes) ===
    uint64_t total_size_mb;        // Current total size in MB
    uint64_t used_size_mb;         // Used size in MB (allocated pages)
    uint64_t free_size_mb;         // Free size in MB (free pages)
    uint64_t table_count;          // Number of tables in this tablespace
    uint64_t index_count;          // Number of indexes in this tablespace

    // === Timestamps (24 bytes) ===
    uint64_t created_time;         // Creation timestamp (microseconds)
    uint64_t last_modified_time;   // Last modification timestamp
    uint64_t last_extended_time;   // Last autoextend timestamp (0 = never extended)

    // === Reserved (padded to reach 528 bytes total) ===
    uint8_t reserved3[86];         // Reserved for future use (adjusted for actual struct size)
};
#pragma pack(pop)

/**
 * SBTablespaceFileCatalog - pg_tablespace_files system table entry
 *
 * Tracks individual files within a tablespace.
 * For future support of multi-file tablespaces (similar to Oracle datafiles).
 * Currently, each tablespace has exactly 1 file (file_index=0).
 */
#pragma pack(push, 1)
struct SBTablespaceFileCatalog
{
    // === Header ===
    uint8_t is_valid;              // 1 = valid entry, 0 = deleted
    uint8_t reserved1[7];          // Padding

    // === Identification (20 bytes) ===
    uint16_t tablespace_id;        // Parent tablespace ID
    uint16_t file_index;           // File index within tablespace (0, 1, 2, ...)
    UuidV7Bytes file_uuid;         // UUID v7 for this file (16 bytes)

    // === File Information (280 bytes) ===
    char file_path[256];           // Absolute path to file
    uint64_t starting_page;        // Starting page number in tablespace (0 for single file)
    uint64_t page_count;           // Number of pages in this file
    uint64_t max_pages;            // Maximum pages (0 = unlimited, grows with file)

    // === Status (8 bytes) ===
    uint8_t is_online;             // 1 = online, 0 = offline (for future attach/detach)
    uint8_t reserved2[7];          // Padding

    // === Timestamps (16 bytes) ===
    uint64_t created_time;         // Creation timestamp
    uint64_t last_modified_time;   // Last modification timestamp

    // === Reserved ===
    uint8_t reserved3[64];         // Reserved for future use

    // Total size: 8 + 20 + 280 + 8 + 16 + 64 = 396 bytes
};
#pragma pack(pop)

/**
 * TablespaceInfo - In-memory representation of a tablespace
 *
 * Used by CatalogManager and PageManager for runtime tablespace management.
 * Loaded from pg_tablespace catalog during database initialization.
 */
struct TablespaceInfo
{
    // === Identification ===
    uint16_t tablespace_id = 0;                // Tablespace ID (0 = primary, 1 = reserved, 2-65535 custom)
    std::string tablespace_name;               // Human-readable name
    UuidV7Bytes tablespace_uuid;               // UUID v7

    // === Configuration ===
    bool autoextend_enabled = true;            // Autoextend on/off
    uint32_t autoextend_size_mb = 100;         // Default: 100 MB increments
    uint64_t max_size_mb = 0;                  // 0 = unlimited
    uint32_t prealloc_pages = 0;               // Pages preallocated (0 = none)

    // === Files ===
    std::vector<std::string> file_paths;       // Absolute paths to files (usually 1 entry)

    // === Statistics ===
    uint64_t total_size_mb = 0;                // Current total size
    uint64_t used_size_mb = 0;                 // Used size
    uint64_t free_size_mb = 0;                 // Free size
    uint64_t table_count = 0;                  // Number of tables
    uint64_t index_count = 0;                  // Number of indexes

    // === Timestamps ===
    uint64_t created_time = 0;                 // Creation timestamp
    uint64_t last_modified_time = 0;           // Last modification timestamp
    uint64_t last_extended_time = 0;           // Last autoextend timestamp
};

/**
 * TablespaceStats - Statistics for a tablespace
 *
 * Returned by getTablespaceStats() for monitoring and diagnostics.
 */
struct TablespaceStats
{
    uint64_t total_pages = 0;                  // Total pages in tablespace
    uint64_t free_pages = 0;                   // Free pages
    uint64_t allocated_pages = 0;              // Allocated pages (total - free)
    uint64_t total_size_mb = 0;                // Total size in MB
    uint64_t used_size_mb = 0;                 // Used size in MB
    uint64_t free_size_mb = 0;                 // Free size in MB
    uint64_t table_count = 0;                  // Number of tables
    uint64_t index_count = 0;                  // Number of indexes
    uint64_t extension_count = 0;              // Number of times tablespace extended
    uint64_t last_extended_time = 0;           // Last autoextend timestamp
};

/**
 * TablespaceConfig - Configuration for creating a tablespace
 *
 * Used by createTablespace() to specify parameters.
 */
struct TablespaceConfig
{
    bool autoextend_enabled = true;            // Enable autoextend (default: true)
    uint32_t autoextend_size_mb = 100;         // Extend by 100 MB (default)
    uint64_t max_size_mb = 0;                  // 0 = unlimited (default)
    uint32_t prealloc_pages = 0;               // Preallocate pages (0 = none)
};

/**
 * Validate TablespaceHeader structure size
 *
 * The header must be padded to page_size. This is enforced at runtime
 * when creating/opening tablespace files.
 *
 * Fixed fields occupy 304 bytes (updated for ON_DISK_FORMAT.md v1.4.0):
 * - PageHeader: 80 bytes (includes table_id field at offset 0x30)
 * - Identification: 96 bytes
 * - Configuration: 64 bytes
 * - File Layout: 32 bytes
 * - Transaction Info: 32 bytes
 * Total: 304 bytes
 *
 * Remaining bytes (page_size - 304) are padding.
 */
static_assert(sizeof(PageHeader) == 80, "PageHeader must be 80 bytes per ON_DISK_FORMAT.md v1.4.0");
static_assert(offsetof(TablespaceHeader, tablespace_name) == 80,
              "tablespace_name offset incorrect");
static_assert(offsetof(TablespaceHeader, tablespace_id) == 176,
              "tablespace_id offset incorrect");
static_assert(offsetof(TablespaceHeader, total_pages) == 240,
              "total_pages offset incorrect");
static_assert(offsetof(TablespaceHeader, oldest_transaction_id) == 272,
              "oldest_transaction_id offset incorrect");

/**
 * Validate catalog structure sizes
 */
static_assert(sizeof(SBTablespaceCatalog) == 528,
              "SBTablespaceCatalog size must be 528 bytes");
static_assert(sizeof(SBTablespaceFileCatalog) == 396,
              "SBTablespaceFileCatalog size must be 396 bytes");

} // namespace scratchbird::core
