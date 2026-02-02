/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P2-23: Backup/Restore Manager
//
// Comprehensive backup and restore functionality including:
// - Full and incremental backups
// - Parallel backup/restore for performance
// - Compression support (zlib)
// - Point-in-time recovery infrastructure
//
// November 25, 2025

#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/gpid.h"
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace scratchbird::core {

// Forward declarations
class Database;
class BufferPool;

// Backup type enumeration
enum class BackupType : uint8_t {
    FULL = 0,           // Complete backup of all pages
    INCREMENTAL = 1,    // Only pages changed since last backup
    DIFFERENTIAL = 2    // Pages changed since last FULL backup
};

// Compression type enumeration
enum class CompressionType : uint8_t {
    NONE = 0,           // No compression
    ZLIB = 1,           // zlib compression (default)
    ZSTD = 2,           // Zstandard compression (future)
    LZ4 = 3             // LZ4 compression (future)
};

// Backup configuration
struct BackupConfig {
    BackupType type = BackupType::FULL;
    CompressionType compression = CompressionType::ZLIB;
    int compression_level = 6;          // 1-9 for zlib
    uint32_t parallel_workers = 4;      // Number of parallel workers
    size_t buffer_size = 1024 * 1024;   // 1MB buffer per worker
    bool verify_checksums = true;       // Verify page checksums
    bool include_wal = true;            // Include WAL in backup (future)
    std::string label;                  // Backup label/description
};

// Restore configuration
struct RestoreConfig {
    uint32_t parallel_workers = 4;      // Number of parallel workers
    size_t buffer_size = 1024 * 1024;   // 1MB buffer per worker
    bool verify_checksums = true;       // Verify page checksums after restore
    uint64_t target_time = 0;           // Target time for PITR (0 = latest)
    std::string target_lsn;             // Target LSN for PITR (empty = latest)
    bool partial_restore = false;       // Allow partial restore on errors
    bool allow_tablespace_create = false; // Create missing tablespace files when restoring
    std::unordered_map<uint16_t, std::vector<std::string>> tablespace_path_overrides; // Optional relocation
};

// Backup manifest header (stored at beginning of backup file)
#pragma pack(push, 1)
struct BackupManifestHeader {
    char magic[8];                      // "SBKP0001"
    uint64_t version;                   // Backup format version
    uint8_t db_uuid[16];                // Source database UUID
    BackupType type;                    // Backup type
    CompressionType compression;        // Compression type
    uint8_t compression_level;          // Compression level
    uint8_t reserved1;                  // Reserved
    uint32_t page_size;                 // Database page size
    uint64_t total_pages;               // Total pages in backup
    uint64_t backup_start_time;         // Backup start timestamp (micros)
    uint64_t backup_end_time;           // Backup end timestamp (micros)
    uint64_t start_transaction_id;      // Transaction ID at backup start
    uint64_t end_transaction_id;        // Transaction ID at backup end
    uint8_t parent_backup_uuid[16];     // Parent backup UUID (for incremental)
    char label[128];                    // Backup label
    uint32_t checksum;                  // Header checksum
    uint64_t tablespace_info_offset;    // Offset to tablespace manifest (0 if none)
    uint64_t tablespace_info_size;      // Size of tablespace manifest in bytes
    uint8_t reserved2[40];              // Reserved for future use
};
#pragma pack(pop)

// Backup page entry (for each page in backup)
#pragma pack(push, 1)
struct BackupPageEntry {
    GPID gpid;                          // Global page ID
    uint32_t compressed_size;           // Compressed size (0 if uncompressed)
    uint32_t original_size;             // Original page size
    uint32_t checksum;                  // Page checksum
    uint64_t file_offset;               // Offset in backup file
};
#pragma pack(pop)

// Tablespace manifest entry header (variable-length file paths follow)
#pragma pack(push, 1)
struct BackupTablespaceEntryHeader {
    uint16_t tablespace_id;             // Tablespace ID
    uint16_t file_count;                // Number of files in tablespace
    uint32_t reserved;                  // Reserved for alignment/future
    uint64_t total_pages;               // Total pages in tablespace
};
#pragma pack(pop)

// Backup progress information
struct BackupProgress {
    std::atomic<uint64_t> pages_processed{0};
    std::atomic<uint64_t> pages_total{0};
    std::atomic<uint64_t> bytes_written{0};
    std::atomic<uint64_t> bytes_read{0};
    std::atomic<bool> completed{false};
    std::atomic<bool> cancelled{false};
    std::chrono::steady_clock::time_point start_time;

    double getProgressPercent() const {
        uint64_t total = pages_total.load();
        if (total == 0) return 0.0;
        return 100.0 * pages_processed.load() / total;
    }

    double getElapsedSeconds() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - start_time).count();
    }

    double getPagesPerSecond() const {
        double elapsed = getElapsedSeconds();
        if (elapsed <= 0) return 0.0;
        return pages_processed.load() / elapsed;
    }
};

// Backup metadata (for catalog)
struct BackupMetadata {
    UuidV7Bytes backup_id;              // Unique backup ID
    UuidV7Bytes parent_id;              // Parent backup ID (for incremental)
    BackupType type;
    std::string path;                   // Backup file path
    std::string label;                  // Backup label
    uint64_t start_time;                // Start timestamp
    uint64_t end_time;                  // End timestamp
    uint64_t total_pages;               // Pages in backup
    uint64_t size_bytes;                // Backup file size
    uint64_t start_xid;                 // Start transaction ID
    uint64_t end_xid;                   // End transaction ID
    bool valid;                         // Backup validity flag
};

// Progress callback type
using ProgressCallback = std::function<void(const BackupProgress&)>;

// Backup manager class
class BackupManager {
public:
    explicit BackupManager(Database* db);
    ~BackupManager();

    // Disable copy
    BackupManager(const BackupManager&) = delete;
    BackupManager& operator=(const BackupManager&) = delete;

    // === Backup Operations ===

    // Create a backup
    Status createBackup(const std::string& backup_path,
                       const BackupConfig& config,
                       BackupProgress* progress = nullptr,
                       ErrorContext* ctx = nullptr);

    // Create incremental backup (convenience method)
    Status createIncrementalBackup(const std::string& backup_path,
                                   const std::string& parent_backup_path,
                                   BackupProgress* progress = nullptr,
                                   ErrorContext* ctx = nullptr);

    // Cancel ongoing backup
    void cancelBackup(BackupProgress* progress);

    // === Restore Operations ===

    // Restore from backup
    Status restoreBackup(const std::string& backup_path,
                        const std::string& target_path,
                        const RestoreConfig& config,
                        BackupProgress* progress = nullptr,
                        ErrorContext* ctx = nullptr);

    // Restore to point in time (requires full + incrementals)
    Status restoreToPointInTime(const std::vector<std::string>& backup_chain,
                               const std::string& target_path,
                               uint64_t target_time,
                               BackupProgress* progress = nullptr,
                               ErrorContext* ctx = nullptr);

    // === Verification ===

    // Verify backup integrity
    Status verifyBackup(const std::string& backup_path,
                       BackupProgress* progress = nullptr,
                       ErrorContext* ctx = nullptr);

    // === Metadata Operations ===

    // Get backup metadata
    Status getBackupMetadata(const std::string& backup_path,
                            BackupMetadata* metadata_out,
                            ErrorContext* ctx = nullptr);

    // List backups in a directory
    Status listBackups(const std::string& backup_dir,
                      std::vector<BackupMetadata>* backups_out,
                      ErrorContext* ctx = nullptr);

    // Build backup chain for incremental restore
    Status buildBackupChain(const std::string& backup_path,
                           std::vector<std::string>* chain_out,
                           ErrorContext* ctx = nullptr);

    // === Change Tracking (for incremental backups) ===

    // Mark page as modified (called by buffer pool)
    void markPageModified(GPID gpid);

    // Get modified pages since last backup
    std::vector<GPID> getModifiedPages() const;

    // Clear modified pages tracking (after backup)
    void clearModifiedPages();

    // Enable/disable change tracking
    void setChangeTrackingEnabled(bool enabled);
    bool isChangeTrackingEnabled() const { return change_tracking_enabled_; }

private:
    Database* db_;
    BufferPool* buffer_pool_;

    // Change tracking
    std::unordered_set<GPID> modified_pages_;
    mutable std::mutex modified_pages_mutex_;
    std::atomic<bool> change_tracking_enabled_{true};

    // Last backup info
    UuidV7Bytes last_backup_id_;
    uint64_t last_backup_time_ = 0;
    mutable std::mutex backup_mutex_;

    // Internal helpers
    Status writeBackupHeader(int fd, const BackupManifestHeader& header, ErrorContext* ctx);
    Status readBackupHeader(int fd, BackupManifestHeader* header, ErrorContext* ctx);
    Status compressPage(const uint8_t* input, uint32_t input_size,
                       std::vector<uint8_t>& output, CompressionType type,
                       int level, ErrorContext* ctx);
    Status decompressPage(const uint8_t* input, uint32_t input_size,
                         uint8_t* output, uint32_t output_size,
                         CompressionType type, ErrorContext* ctx);
    uint32_t calculateChecksum(const uint8_t* data, size_t size);

    // Parallel worker functions
    Status backupWorker(int backup_fd, const std::vector<GPID>& pages,
                       const BackupConfig& config, BackupProgress* progress,
                       std::vector<BackupPageEntry>* entries, ErrorContext* ctx);
    Status restoreWorker(int backup_fd, const std::vector<BackupPageEntry>& entries,
                        int target_fd, const RestoreConfig& config,
                        BackupProgress* progress, ErrorContext* ctx);
};

// Backup catalog for tracking backups
class BackupCatalog {
public:
    explicit BackupCatalog(const std::string& catalog_path);
    ~BackupCatalog();

    // Add backup to catalog
    Status addBackup(const BackupMetadata& metadata, ErrorContext* ctx = nullptr);

    // Remove backup from catalog
    Status removeBackup(const UuidV7Bytes& backup_id, ErrorContext* ctx = nullptr);

    // Get backup by ID
    Status getBackup(const UuidV7Bytes& backup_id, BackupMetadata* metadata_out,
                    ErrorContext* ctx = nullptr);

    // List all backups
    Status listBackups(std::vector<BackupMetadata>* backups_out,
                      ErrorContext* ctx = nullptr);

    // Get latest full backup
    Status getLatestFullBackup(BackupMetadata* metadata_out,
                              ErrorContext* ctx = nullptr);

    // Get incremental chain for a backup
    Status getIncrementalChain(const UuidV7Bytes& backup_id,
                              std::vector<BackupMetadata>* chain_out,
                              ErrorContext* ctx = nullptr);

    // Save catalog to disk
    Status save(ErrorContext* ctx = nullptr);

    // Load catalog from disk
    Status load(ErrorContext* ctx = nullptr);

private:
    std::string catalog_path_;
    std::vector<BackupMetadata> backups_;
    mutable std::mutex mutex_;
    bool modified_ = false;
};

} // namespace scratchbird::core
