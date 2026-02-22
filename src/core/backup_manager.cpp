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
// P2-23: Backup/Restore Manager Implementation
//
// November 25, 2025

#include "scratchbird/core/backup_manager.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/tablespace.h"
#include "scratchbird/core/config.h"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <fcntl.h>
#include "scratchbird/core/posix_compat.h"
#include <sys/stat.h>
#include <zlib.h>

namespace scratchbird::core {

// Magic number for backup files
static constexpr char BACKUP_MAGIC[8] = {'S', 'B', 'K', 'P', '0', '0', '0', '1'};
static constexpr uint64_t BACKUP_VERSION = 3;

struct BackupTablespaceInfo
{
    uint16_t tablespace_id = 0;
    uint64_t total_pages = 0;
    std::vector<std::string> file_paths;
    std::vector<uint64_t> file_start_pages;
    std::vector<uint64_t> file_page_counts;
};

static Status readTablespaceFilePageCount(const std::string& path,
                                          uint32_t page_size,
                                          uint64_t* pages_out,
                                          ErrorContext* ctx)
{
    if (!pages_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing pages_out");
        return Status::INVALID_ARGUMENT;
    }
    *pages_out = 0;

    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, ("Failed to open tablespace file: " + path).c_str());
        return Status::IO_ERROR;
    }

    std::vector<uint8_t> buffer(page_size, 0);
    ssize_t bytes_read = ::pread(fd, buffer.data(), page_size, 0);
    ::close(fd);
    if (bytes_read != static_cast<ssize_t>(page_size))
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, ("Failed to read tablespace header: " + path).c_str());
        return Status::IO_ERROR;
    }

    const auto* page_header = reinterpret_cast<const PageHeader*>(buffer.data());
    if (page_header->magic != K_MAGIC_SBRD)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          ("Invalid tablespace file (bad magic): " + path).c_str());
        return Status::INVALID_ARGUMENT;
    }

    if (page_header->version == TABLESPACE_HEADER_VERSION_V1)
    {
        const auto* legacy = reinterpret_cast<const TablespaceHeaderV1*>(buffer.data());
        *pages_out = legacy->total_pages;
        return Status::OK;
    }
    if (page_header->version == TABLESPACE_HEADER_VERSION_CURRENT)
    {
        const auto* header = reinterpret_cast<const TablespaceHeader*>(buffer.data());
        *pages_out = header->total_pages;
        return Status::OK;
    }

    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                      ("Unsupported tablespace header version in: " + path).c_str());
    return Status::INVALID_ARGUMENT;
}

Status writeTablespaceManifest(int fd,
                               const std::vector<BackupTablespaceInfo> &tablespaces,
                               bool include_file_ranges,
                               uint64_t *offset_out,
                               uint64_t *size_out,
                               ErrorContext *ctx)
{
    if (offset_out)
    {
        *offset_out = 0;
    }
    if (size_out)
    {
        *size_out = 0;
    }

    if (tablespaces.empty())
    {
        return Status::OK;
    }

    off_t offset = ::lseek(fd, 0, SEEK_CUR);
    if (offset < 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to seek for tablespace manifest");
        return Status::IO_ERROR;
    }

    uint32_t count = static_cast<uint32_t>(tablespaces.size());
    ssize_t written = ::write(fd, &count, sizeof(count));
    if (written != static_cast<ssize_t>(sizeof(count)))
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write tablespace count");
        return Status::IO_ERROR;
    }

    for (const auto &tablespace : tablespaces)
    {
        BackupTablespaceEntryHeader entry{};
        entry.tablespace_id = tablespace.tablespace_id;
        entry.file_count = static_cast<uint16_t>(tablespace.file_paths.size());
        entry.total_pages = tablespace.total_pages;

        written = ::write(fd, &entry, sizeof(entry));
        if (written != static_cast<ssize_t>(sizeof(entry)))
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write tablespace entry");
            return Status::IO_ERROR;
        }

        for (size_t i = 0; i < tablespace.file_paths.size(); ++i)
        {
            const auto &path = tablespace.file_paths[i];
            uint32_t path_len = static_cast<uint32_t>(path.size());
            written = ::write(fd, &path_len, sizeof(path_len));
            if (written != static_cast<ssize_t>(sizeof(path_len)))
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write tablespace path length");
                return Status::IO_ERROR;
            }
            if (path_len > 0)
            {
                written = ::write(fd, path.data(), path_len);
                if (written != static_cast<ssize_t>(path_len))
                {
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write tablespace path");
                    return Status::IO_ERROR;
                }
            }

            if (include_file_ranges)
            {
                uint64_t start_page = 0;
                uint64_t page_count = 0;
                if (i < tablespace.file_start_pages.size())
                {
                    start_page = tablespace.file_start_pages[i];
                }
                if (i < tablespace.file_page_counts.size())
                {
                    page_count = tablespace.file_page_counts[i];
                }
                written = ::write(fd, &start_page, sizeof(start_page));
                if (written != static_cast<ssize_t>(sizeof(start_page)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write tablespace start page");
                    return Status::IO_ERROR;
                }
                written = ::write(fd, &page_count, sizeof(page_count));
                if (written != static_cast<ssize_t>(sizeof(page_count)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write tablespace page count");
                    return Status::IO_ERROR;
                }
            }
        }
    }

    off_t end_offset = ::lseek(fd, 0, SEEK_CUR);
    if (end_offset < 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to finalize tablespace manifest offset");
        return Status::IO_ERROR;
    }

    if (offset_out)
    {
        *offset_out = static_cast<uint64_t>(offset);
    }
    if (size_out)
    {
        *size_out = static_cast<uint64_t>(end_offset - offset);
    }

    return Status::OK;
}

Status readTablespaceManifest(int fd,
                              uint64_t offset,
                              uint64_t size,
                              uint64_t backup_version,
                              std::vector<BackupTablespaceInfo> *tablespaces_out,
                              ErrorContext *ctx)
{
    if (!tablespaces_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null tablespace output");
        return Status::INVALID_ARGUMENT;
    }

    tablespaces_out->clear();

    if (size == 0)
    {
        return Status::OK;
    }

    if (::lseek(fd, static_cast<off_t>(offset), SEEK_SET) != static_cast<off_t>(offset))
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to seek to tablespace manifest");
        return Status::IO_ERROR;
    }

    uint32_t count = 0;
    ssize_t read_bytes = ::read(fd, &count, sizeof(count));
    if (read_bytes != static_cast<ssize_t>(sizeof(count)))
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read tablespace count");
        return Status::IO_ERROR;
    }

    tablespaces_out->reserve(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        BackupTablespaceEntryHeader entry{};
        read_bytes = ::read(fd, &entry, sizeof(entry));
        if (read_bytes != static_cast<ssize_t>(sizeof(entry)))
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read tablespace entry");
            return Status::IO_ERROR;
        }

        BackupTablespaceInfo info;
        info.tablespace_id = entry.tablespace_id;
        info.total_pages = entry.total_pages;

        for (uint16_t file_index = 0; file_index < entry.file_count; ++file_index)
        {
            uint32_t path_len = 0;
            read_bytes = ::read(fd, &path_len, sizeof(path_len));
            if (read_bytes != static_cast<ssize_t>(sizeof(path_len)))
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read tablespace path length");
                return Status::IO_ERROR;
            }
            std::string path;
            if (path_len > 0)
            {
                path.resize(path_len);
                read_bytes = ::read(fd, path.data(), path_len);
                if (read_bytes != static_cast<ssize_t>(path_len))
                {
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read tablespace path");
                    return Status::IO_ERROR;
                }
            }
            info.file_paths.push_back(std::move(path));

            if (backup_version >= 3)
            {
                uint64_t start_page = 0;
                uint64_t page_count = 0;
                read_bytes = ::read(fd, &start_page, sizeof(start_page));
                if (read_bytes != static_cast<ssize_t>(sizeof(start_page)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read tablespace start page");
                    return Status::IO_ERROR;
                }
                read_bytes = ::read(fd, &page_count, sizeof(page_count));
                if (read_bytes != static_cast<ssize_t>(sizeof(page_count)))
                {
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read tablespace page count");
                    return Status::IO_ERROR;
                }
                info.file_start_pages.push_back(start_page);
                info.file_page_counts.push_back(page_count);
            }
        }

        tablespaces_out->push_back(std::move(info));
    }

    return Status::OK;
}

// =================================================================================================
// BackupManager Implementation
// =================================================================================================

BackupManager::BackupManager(Database* db)
    : db_(db), buffer_pool_(db ? db->buffer_pool() : nullptr)
{
}

BackupManager::~BackupManager() = default;

Status BackupManager::createBackup(const std::string& backup_path,
                                   const BackupConfig& config,
                                   BackupProgress* progress,
                                   ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(backup_mutex_);

    if (!db_ || !db_->is_open()) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
        return Status::INVALID_ARGUMENT;
    }

    // Initialize progress
    if (progress) {
        progress->start_time = std::chrono::steady_clock::now();
        progress->pages_total = db_->total_pages();
        progress->pages_processed = 0;
        progress->bytes_written = 0;
        progress->completed = false;
        progress->cancelled = false;
    }

    // Create backup file
    int backup_fd = ::open(backup_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (backup_fd < 0) {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to create backup file");
        return Status::IO_ERROR;
    }

    // Prepare header
    BackupManifestHeader header{};
    std::memcpy(header.magic, BACKUP_MAGIC, 8);
    header.version = BACKUP_VERSION;
    std::memcpy(header.db_uuid, &db_->uuid(), 16);
    header.type = config.type;
    header.compression = config.compression;
    header.compression_level = static_cast<uint8_t>(config.compression_level);
    header.page_size = db_->page_size();
    header.total_pages = db_->total_pages();
    header.backup_start_time = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    // Get transaction info
    if (db_->transaction_manager()) {
        // header.start_transaction_id would come from transaction manager
        header.start_transaction_id = 0; // Placeholder
    }

    // Copy label
    std::strncpy(header.label, config.label.c_str(), sizeof(header.label) - 1);

    // Write header placeholder (will update after backup)
    auto status = writeBackupHeader(backup_fd, header, ctx);
    if (status != Status::OK) {
        ::close(backup_fd);
        return status;
    }

    // Build tablespace manifest
    std::vector<BackupTablespaceInfo> tablespace_manifest;
    std::vector<TablespaceInfo> catalog_tablespaces;
    auto *catalog = db_->catalog_manager();
    auto *page_mgr = db_->page_manager();
    if (!catalog || !page_mgr)
    {
        ::close(backup_fd);
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog or page manager not available");
        return Status::INVALID_ARGUMENT;
    }
    status = catalog->listTablespaces(catalog_tablespaces, ctx);
    if (status != Status::OK)
    {
        ::close(backup_fd);
        return status;
    }

    bool has_primary = false;
    for (const auto &tablespace : catalog_tablespaces)
    {
        BackupTablespaceInfo info;
        info.tablespace_id = tablespace.tablespace_id;
        info.file_paths = tablespace.file_paths;
        if (info.tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            has_primary = true;
            if (info.file_paths.empty())
            {
                info.file_paths.push_back(db_->path());
            }
            info.total_pages = db_->total_pages();
        }
        else
        {
            uint32_t total_pages = 0;
            Status total_status = page_mgr->getTablespaceTotalPages(info.tablespace_id, &total_pages, ctx);
            if (total_status != Status::OK)
            {
                ::close(backup_fd);
                return total_status;
            }
            info.total_pages = total_pages;
        }

        if (!info.file_paths.empty())
        {
            info.file_start_pages.clear();
            info.file_page_counts.clear();
            info.file_start_pages.reserve(info.file_paths.size());
            info.file_page_counts.reserve(info.file_paths.size());

            uint64_t start_page = 0;
            for (const auto &path : info.file_paths)
            {
                uint64_t page_count = 0;
                if (info.tablespace_id == PRIMARY_TABLESPACE_ID)
                {
                    page_count = info.total_pages;
                }
                else
                {
                    Status count_status = readTablespaceFilePageCount(path, db_->page_size(),
                                                                      &page_count, ctx);
                    if (count_status != Status::OK)
                    {
                        ::close(backup_fd);
                        return count_status;
                    }
                }
                info.file_start_pages.push_back(start_page);
                info.file_page_counts.push_back(page_count);
                start_page += page_count;
            }
        }

        tablespace_manifest.push_back(std::move(info));
    }

    if (!has_primary)
    {
        BackupTablespaceInfo primary;
        primary.tablespace_id = PRIMARY_TABLESPACE_ID;
        primary.total_pages = db_->total_pages();
        primary.file_paths.push_back(db_->path());
        tablespace_manifest.push_back(std::move(primary));
    }

    bool include_file_ranges = (header.version >= 3);
    status = writeTablespaceManifest(backup_fd, tablespace_manifest, include_file_ranges,
                                     &header.tablespace_info_offset,
                                     &header.tablespace_info_size,
                                     ctx);
    if (status != Status::OK)
    {
        ::close(backup_fd);
        return status;
    }

    // Determine pages to backup
    std::vector<GPID> pages_to_backup;
    if (config.type == BackupType::FULL) {
        // Backup all pages in all tablespaces
        uint64_t total_pages = 0;
        for (const auto &tablespace : tablespace_manifest)
        {
            total_pages += tablespace.total_pages;
        }
        pages_to_backup.reserve(total_pages);
        for (const auto &tablespace : tablespace_manifest)
        {
            for (uint64_t i = 0; i < tablespace.total_pages; ++i)
            {
                pages_to_backup.push_back(makeGPID(tablespace.tablespace_id,
                                                   static_cast<uint32_t>(i)));
            }
        }
    } else {
        // Incremental/Differential - only modified pages
        std::lock_guard<std::mutex> mp_lock(modified_pages_mutex_);
        pages_to_backup.assign(modified_pages_.begin(), modified_pages_.end());
        std::sort(pages_to_backup.begin(), pages_to_backup.end());
    }

    if (progress) {
        progress->pages_total = pages_to_backup.size();
    }

    // Write page index placeholder
    size_t index_offset = ::lseek(backup_fd, 0, SEEK_CUR);
    std::vector<BackupPageEntry> page_entries;
    page_entries.reserve(pages_to_backup.size());

    // Reserve space for page index
    BackupPageEntry empty_entry{};
    for (size_t i = 0; i < pages_to_backup.size(); ++i) {
        ::write(backup_fd, &empty_entry, sizeof(BackupPageEntry));
    }

    // Backup pages (parallel)
    size_t data_offset = ::lseek(backup_fd, 0, SEEK_CUR);
    uint32_t num_workers = std::min(config.parallel_workers,
                                     static_cast<uint32_t>(pages_to_backup.size()));
    if (num_workers == 0) num_workers = 1;

    size_t pages_per_worker = (pages_to_backup.size() + num_workers - 1) / num_workers;
    std::vector<std::thread> workers;
    std::vector<std::vector<BackupPageEntry>> worker_entries(num_workers);
    std::atomic<Status> worker_status{Status::OK};

    // Single-threaded for simplicity in this version
    // (Full parallel implementation would need more synchronization)
    std::vector<uint8_t> page_buffer(db_->page_size());
    std::vector<uint8_t> compress_buffer(db_->page_size() * 2);

    for (size_t i = 0; i < pages_to_backup.size(); ++i) {
        if (progress && progress->cancelled) {
            ::close(backup_fd);
            SET_ERROR_CONTEXT(ctx, Status::CANCELLED, "Backup cancelled");
            return Status::CANCELLED;
        }

        GPID gpid = pages_to_backup[i];

        // Read page from database
        status = db_->read_page_global(gpid, page_buffer.data(), ctx);
        if (status != Status::OK) {
            LOG_ERROR(GENERAL, "Failed to read page %lu during backup", gpid);
            continue; // Skip failed pages
        }

        // Calculate checksum
        uint32_t checksum = calculateChecksum(page_buffer.data(), db_->page_size());

        // Compress if enabled
        uint32_t compressed_size = 0;
        const uint8_t* write_data = page_buffer.data();
        uint32_t write_size = db_->page_size();

        if (config.compression != CompressionType::NONE) {
            status = compressPage(page_buffer.data(), db_->page_size(),
                                 compress_buffer, config.compression,
                                 config.compression_level, ctx);
            if (status == Status::OK && compress_buffer.size() < db_->page_size()) {
                write_data = compress_buffer.data();
                write_size = static_cast<uint32_t>(compress_buffer.size());
                compressed_size = write_size;
            }
        }

        // Write page data
        uint64_t file_offset = ::lseek(backup_fd, 0, SEEK_CUR);
        ssize_t written = ::write(backup_fd, write_data, write_size);
        if (written != static_cast<ssize_t>(write_size)) {
            ::close(backup_fd);
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write page data");
            return Status::IO_ERROR;
        }

        // Record entry
        BackupPageEntry entry{};
        entry.gpid = gpid;
        entry.compressed_size = compressed_size;
        entry.original_size = db_->page_size();
        entry.checksum = checksum;
        entry.file_offset = file_offset;
        page_entries.push_back(entry);

        if (progress) {
            progress->pages_processed++;
            progress->bytes_written += write_size;
        }
    }

    // Update page index
    ::lseek(backup_fd, static_cast<off_t>(index_offset), SEEK_SET);
    for (const auto& entry : page_entries) {
        ::write(backup_fd, &entry, sizeof(BackupPageEntry));
    }

    // Update header with final info
    header.total_pages = page_entries.size();
    header.backup_end_time = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    header.checksum = calculateChecksum(reinterpret_cast<const uint8_t*>(&header),
                                        sizeof(header) - sizeof(header.checksum));

    ::lseek(backup_fd, 0, SEEK_SET);
    writeBackupHeader(backup_fd, header, ctx);

    ::fsync(backup_fd);
    ::close(backup_fd);

    // Clear modified pages for incremental backup
    if (config.type == BackupType::INCREMENTAL) {
        clearModifiedPages();
    }

    // Update last backup info
    last_backup_time_ = header.backup_end_time;
    last_backup_id_ = generateUuidV7();

    if (progress) {
        progress->completed = true;
    }

    LOG_INFO(GENERAL, "Backup completed: %zu pages, %.2f MB",
             page_entries.size(),
             progress ? progress->bytes_written.load() / (1024.0 * 1024.0) : 0.0);

    return Status::OK;
}

Status BackupManager::createIncrementalBackup(const std::string& backup_path,
                                               const std::string& parent_backup_path,
                                               BackupProgress* progress,
                                               ErrorContext* ctx)
{
    // Read parent backup to get its ID
    BackupMetadata parent_meta;
    auto status = getBackupMetadata(parent_backup_path, &parent_meta, ctx);
    if (status != Status::OK) {
        return status;
    }

    BackupConfig config;
    config.type = BackupType::INCREMENTAL;
    config.label = "Incremental backup";

    return createBackup(backup_path, config, progress, ctx);
}

void BackupManager::cancelBackup(BackupProgress* progress)
{
    if (progress) {
        progress->cancelled = true;
    }
}

Status BackupManager::restoreBackup(const std::string& backup_path,
                                     const std::string& target_path,
                                     const RestoreConfig& config,
                                     BackupProgress* progress,
                                     ErrorContext* ctx)
{
    // Open backup file
    int backup_fd = ::open(backup_path.c_str(), O_RDONLY);
    if (backup_fd < 0) {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open backup file");
        return Status::IO_ERROR;
    }

    // Read and validate header
    BackupManifestHeader header;
    auto status = readBackupHeader(backup_fd, &header, ctx);
    if (status != Status::OK) {
        ::close(backup_fd);
        return status;
    }

    // Read tablespace manifest (if present)
    std::vector<BackupTablespaceInfo> tablespaces;
    if (header.tablespace_info_size > 0)
    {
        status = readTablespaceManifest(backup_fd, header.tablespace_info_offset,
                                        header.tablespace_info_size, header.version,
                                        &tablespaces, ctx);
        if (status != Status::OK)
        {
            ::close(backup_fd);
            return status;
        }
    }

    // Initialize progress
    if (progress) {
        progress->start_time = std::chrono::steady_clock::now();
        progress->pages_total = header.total_pages;
        progress->pages_processed = 0;
        progress->completed = false;
        progress->cancelled = false;
    }

    // Create target database file
    int target_fd = ::open(target_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (target_fd < 0) {
        ::close(backup_fd);
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to create target database");
        return Status::IO_ERROR;
    }

    const auto recovery_mode =
        Config::getInstance().getString("storage", "tablespace_recovery_mode", "strict");
    std::string recovery_mode_lower = recovery_mode;
    std::transform(recovery_mode_lower.begin(), recovery_mode_lower.end(),
                   recovery_mode_lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    bool allow_missing_tablespaces = (recovery_mode_lower == "allow_missing");

    struct TablespaceFileRange
    {
        uint64_t start_page = 0;
        uint64_t page_count = 0;
        int fd = -1;
    };

    std::unordered_map<uint16_t, std::vector<TablespaceFileRange>> tablespace_files;
    tablespace_files[PRIMARY_TABLESPACE_ID] = {TablespaceFileRange{0, header.total_pages, target_fd}};

    for (const auto &tablespace : tablespaces)
    {
        if (tablespace.tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            continue;
        }

        const auto override_it = config.tablespace_path_overrides.find(tablespace.tablespace_id);
        const std::vector<std::string>* file_paths = &tablespace.file_paths;
        if (override_it != config.tablespace_path_overrides.end() &&
            !override_it->second.empty())
        {
            if (!tablespace.file_paths.empty() &&
                override_it->second.size() != tablespace.file_paths.size())
            {
                ::close(backup_fd);
                ::close(target_fd);
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Tablespace relocation path count mismatch");
                return Status::INVALID_ARGUMENT;
            }
            file_paths = &override_it->second;
        }

        if (file_paths->empty())
        {
            ::close(backup_fd);
            ::close(target_fd);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Tablespace file path missing in backup");
            return Status::INVALID_ARGUMENT;
        }

        uint64_t next_start = 0;
        std::vector<TablespaceFileRange> ranges;
        ranges.reserve(file_paths->size());

        for (size_t i = 0; i < file_paths->size(); ++i)
        {
            const auto &path = (*file_paths)[i];
            struct stat st;
            bool exists = (stat(path.c_str(), &st) == 0);
            bool can_create = config.allow_tablespace_create || allow_missing_tablespaces;
            if (!exists && !can_create)
            {
                ::close(backup_fd);
                ::close(target_fd);
                SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND,
                                  ("Missing tablespace file: " + path).c_str());
                return Status::FILE_NOT_FOUND;
            }
            if (!exists && allow_missing_tablespaces)
            {
                LOG_WARNING(GENERAL,
                            "Missing tablespace file '%s'; creating due to recovery mode",
                            path.c_str());
            }

            int ts_fd = ::open(path.c_str(), O_RDWR | O_CREAT, 0644);
            if (ts_fd < 0)
            {
                ::close(backup_fd);
                ::close(target_fd);
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open tablespace file");
                return Status::IO_ERROR;
            }

            uint64_t start_page = 0;
            uint64_t page_count = 0;
            if (!tablespace.file_start_pages.empty() && i < tablespace.file_start_pages.size())
            {
                start_page = tablespace.file_start_pages[i];
            }
            else
            {
                start_page = next_start;
            }

            if (!tablespace.file_page_counts.empty() && i < tablespace.file_page_counts.size())
            {
                page_count = tablespace.file_page_counts[i];
            }
            else if (i == 0)
            {
                page_count = tablespace.total_pages;
            }

            if (page_count > 0)
            {
                off_t size = static_cast<off_t>(page_count) * header.page_size;
                if (::ftruncate(ts_fd, size) != 0)
                {
                    ::close(ts_fd);
                    ::close(backup_fd);
                    ::close(target_fd);
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to size tablespace file");
                    return Status::IO_ERROR;
                }
            }

            ranges.push_back(TablespaceFileRange{start_page, page_count, ts_fd});
            next_start = start_page + page_count;
        }

        tablespace_files[tablespace.tablespace_id] = std::move(ranges);
    }

    // Read page index
    std::vector<BackupPageEntry> page_entries(header.total_pages);
    uint64_t index_offset = header.tablespace_info_offset + header.tablespace_info_size;
    if (index_offset == 0)
    {
        index_offset = sizeof(BackupManifestHeader);
    }
    ::lseek(backup_fd, static_cast<off_t>(index_offset), SEEK_SET);
    ssize_t read_bytes = ::read(backup_fd, page_entries.data(),
                                header.total_pages * sizeof(BackupPageEntry));
    if (read_bytes != static_cast<ssize_t>(header.total_pages * sizeof(BackupPageEntry))) {
        ::close(backup_fd);
        ::close(target_fd);
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read page index");
        return Status::IO_ERROR;
    }

    // Restore pages
    std::vector<uint8_t> read_buffer(header.page_size * 2);
    std::vector<uint8_t> decompress_buffer(header.page_size);

    for (const auto& entry : page_entries) {
        if (progress && progress->cancelled) {
            ::close(backup_fd);
            ::close(target_fd);
            SET_ERROR_CONTEXT(ctx, Status::CANCELLED, "Restore cancelled");
            return Status::CANCELLED;
        }

        // Seek to page data in backup
        ::lseek(backup_fd, static_cast<off_t>(entry.file_offset), SEEK_SET);

        // Determine read size
        uint32_t read_size = entry.compressed_size > 0 ? entry.compressed_size : entry.original_size;
        read_bytes = ::read(backup_fd, read_buffer.data(), read_size);
        if (read_bytes != static_cast<ssize_t>(read_size)) {
            if (!config.partial_restore) {
                ::close(backup_fd);
                ::close(target_fd);
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read page data");
                return Status::IO_ERROR;
            }
            LOG_ERROR(GENERAL, "Failed to read page %lu, skipping", entry.gpid);
            continue;
        }

        // Decompress if needed
        const uint8_t* page_data = read_buffer.data();
        if (entry.compressed_size > 0) {
            status = decompressPage(read_buffer.data(), entry.compressed_size,
                                   decompress_buffer.data(), entry.original_size,
                                   header.compression, ctx);
            if (status != Status::OK) {
                if (!config.partial_restore) {
                    ::close(backup_fd);
                    ::close(target_fd);
                    return status;
                }
                LOG_ERROR(GENERAL, "Failed to decompress page %lu, skipping", entry.gpid);
                continue;
            }
            page_data = decompress_buffer.data();
        }

        // Verify checksum
        if (config.verify_checksums) {
            uint32_t checksum = calculateChecksum(page_data, entry.original_size);
            if (checksum != entry.checksum) {
                if (!config.partial_restore) {
                    ::close(backup_fd);
                    ::close(target_fd);
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Page checksum mismatch");
                    return Status::DATA_CORRUPTED;
                }
                LOG_ERROR(GENERAL, "Checksum mismatch for page %lu, skipping", entry.gpid);
                continue;
            }
        }

        if (entry.original_size >= sizeof(PageHeader))
        {
            auto *page_header = reinterpret_cast<PageHeader *>(
                const_cast<uint8_t *>(page_data));
            page_header->checksum =
                calculatePageChecksum(reinterpret_cast<uint8_t *>(page_header),
                                      entry.original_size);
        }

        // Write to target database
        uint16_t tablespace_id = getTablespaceID(entry.gpid);
        uint32_t page_id = static_cast<uint32_t>(getPageNumber(entry.gpid));
        auto it = tablespace_files.find(tablespace_id);
        if (it == tablespace_files.end())
        {
            if (!config.partial_restore)
            {
                ::close(backup_fd);
                ::close(target_fd);
                SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND, "Tablespace file not available");
                return Status::FILE_NOT_FOUND;
            }
            LOG_ERROR(GENERAL, "Missing tablespace %u for page %lu, skipping",
                      tablespace_id, entry.gpid);
            continue;
        }

        bool found_file = false;
        int fd = -1;
        uint64_t local_page = page_id;
        for (const auto &range : it->second)
        {
            uint64_t start = range.start_page;
            uint64_t end = start + range.page_count;
            if (range.page_count == 0)
            {
                continue;
            }
            if (page_id >= start && page_id < end)
            {
                fd = range.fd;
                local_page = page_id - start;
                found_file = true;
                break;
            }
        }

        if (!found_file)
        {
            if (!config.partial_restore)
            {
                ::close(backup_fd);
                ::close(target_fd);
                SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND, "Tablespace page not mapped to file");
                return Status::FILE_NOT_FOUND;
            }
            LOG_ERROR(GENERAL, "No tablespace file range for page %u in tablespace %u",
                      page_id, tablespace_id);
            continue;
        }

        off_t offset = static_cast<off_t>(local_page) * header.page_size;
        ::lseek(fd, offset, SEEK_SET);
        ssize_t written = ::write(fd, page_data, entry.original_size);
        if (written != static_cast<ssize_t>(entry.original_size)) {
            if (!config.partial_restore) {
                ::close(backup_fd);
                ::close(target_fd);
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write page");
                return Status::IO_ERROR;
            }
            LOG_ERROR(GENERAL, "Failed to write page %lu, skipping", entry.gpid);
            continue;
        }

        if (progress) {
            progress->pages_processed++;
            progress->bytes_written += entry.original_size;
        }
    }

    for (const auto &entry : tablespace_files)
    {
        for (const auto &range : entry.second)
        {
            if (range.fd >= 0)
            {
                ::fsync(range.fd);
                if (entry.first != PRIMARY_TABLESPACE_ID)
                {
                    ::close(range.fd);
                }
            }
        }
    }
    ::close(backup_fd);
    ::close(target_fd);

    if (progress) {
        progress->completed = true;
    }

    LOG_INFO(GENERAL, "Restore completed: %lu pages", header.total_pages);

    if (!config.tablespace_path_overrides.empty())
    {
        Database restored_db;
        Status open_status = restored_db.open(target_path, ctx);
        if (open_status != Status::OK)
        {
            return open_status;
        }

        auto *catalog = restored_db.catalog_manager();
        if (!catalog)
        {
            restored_db.close();
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager unavailable");
            return Status::INVALID_ARGUMENT;
        }

        for (const auto &override_entry : config.tablespace_path_overrides)
        {
            TablespaceInfo info;
            Status status = catalog->getTablespace(override_entry.first, info, ctx);
            if (status != Status::OK)
            {
                restored_db.close();
                return status;
            }
            info.file_paths = override_entry.second;
            status = catalog->writeTablespaceRecord(info, ctx);
            if (status != Status::OK)
            {
                restored_db.close();
                return status;
            }
            status = catalog->writeTablespaceFileRecords(info, ctx);
            if (status != Status::OK)
            {
                restored_db.close();
                return status;
            }
        }

        Status sync_status = restored_db.sync(ctx);
        restored_db.close();
        if (sync_status != Status::OK)
        {
            return sync_status;
        }
    }

    return Status::OK;
}

Status BackupManager::restoreToPointInTime(const std::vector<std::string>& backup_chain,
                                            const std::string& target_path,
                                            uint64_t target_time,
                                            BackupProgress* progress,
                                            ErrorContext* ctx)
{
    if (backup_chain.empty()) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Empty backup chain");
        return Status::INVALID_ARGUMENT;
    }

    // Restore base backup first
    RestoreConfig config;
    config.verify_checksums = true;

    auto status = restoreBackup(backup_chain[0], target_path, config, progress, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Apply incremental backups up to target time
    for (size_t i = 1; i < backup_chain.size(); ++i) {
        BackupMetadata meta;
        status = getBackupMetadata(backup_chain[i], &meta, ctx);
        if (status != Status::OK) {
            continue;
        }

        // Stop if we've passed the target time
        if (target_time > 0 && meta.end_time > target_time) {
            break;
        }

        // Apply incremental
        status = restoreBackup(backup_chain[i], target_path, config, progress, ctx);
        if (status != Status::OK) {
            LOG_ERROR(GENERAL, "Failed to apply incremental backup %s", backup_chain[i].c_str());
        }
    }

    return Status::OK;
}

Status BackupManager::verifyBackup(const std::string& backup_path,
                                    BackupProgress* progress,
                                    ErrorContext* ctx)
{
    int backup_fd = ::open(backup_path.c_str(), O_RDONLY);
    if (backup_fd < 0) {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open backup file");
        return Status::IO_ERROR;
    }

    // Read and validate header
    BackupManifestHeader header;
    auto status = readBackupHeader(backup_fd, &header, ctx);
    if (status != Status::OK) {
        ::close(backup_fd);
        return status;
    }

    // Initialize progress
    if (progress) {
        progress->start_time = std::chrono::steady_clock::now();
        progress->pages_total = header.total_pages;
        progress->pages_processed = 0;
        progress->completed = false;
    }

    // Read page index
    std::vector<BackupPageEntry> page_entries(header.total_pages);
    uint64_t index_offset = header.tablespace_info_offset + header.tablespace_info_size;
    if (index_offset == 0)
    {
        index_offset = sizeof(BackupManifestHeader);
    }
    ::lseek(backup_fd, static_cast<off_t>(index_offset), SEEK_SET);
    ::read(backup_fd, page_entries.data(), header.total_pages * sizeof(BackupPageEntry));

    // Verify each page
    std::vector<uint8_t> read_buffer(header.page_size * 2);
    std::vector<uint8_t> decompress_buffer(header.page_size);
    uint64_t errors = 0;

    for (const auto& entry : page_entries) {
        ::lseek(backup_fd, static_cast<off_t>(entry.file_offset), SEEK_SET);

        uint32_t read_size = entry.compressed_size > 0 ? entry.compressed_size : entry.original_size;
        ::read(backup_fd, read_buffer.data(), read_size);

        const uint8_t* page_data = read_buffer.data();
        if (entry.compressed_size > 0) {
            status = decompressPage(read_buffer.data(), entry.compressed_size,
                                   decompress_buffer.data(), entry.original_size,
                                   header.compression, ctx);
            if (status != Status::OK) {
                ++errors;
                continue;
            }
            page_data = decompress_buffer.data();
        }

        uint32_t checksum = calculateChecksum(page_data, entry.original_size);
        if (checksum != entry.checksum) {
            ++errors;
            LOG_ERROR(GENERAL, "Checksum mismatch for page %lu", entry.gpid);
        }

        if (progress) {
            progress->pages_processed++;
        }
    }

    ::close(backup_fd);

    if (progress) {
        progress->completed = true;
    }

    if (errors > 0) {
        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Backup verification found errors");
        return Status::DATA_CORRUPTED;
    }

    LOG_INFO(GENERAL, "Backup verification passed: %lu pages", header.total_pages);
    return Status::OK;
}

Status BackupManager::getBackupMetadata(const std::string& backup_path,
                                         BackupMetadata* metadata_out,
                                         ErrorContext* ctx)
{
    if (!metadata_out) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null metadata output");
        return Status::INVALID_ARGUMENT;
    }

    int backup_fd = ::open(backup_path.c_str(), O_RDONLY);
    if (backup_fd < 0) {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open backup file");
        return Status::IO_ERROR;
    }

    BackupManifestHeader header;
    auto status = readBackupHeader(backup_fd, &header, ctx);
    ::close(backup_fd);

    if (status != Status::OK) {
        return status;
    }

    // Get file size
    struct stat st;
    stat(backup_path.c_str(), &st);

    metadata_out->type = header.type;
    metadata_out->path = backup_path;
    metadata_out->label = header.label;
    metadata_out->start_time = header.backup_start_time;
    metadata_out->end_time = header.backup_end_time;
    metadata_out->total_pages = header.total_pages;
    metadata_out->size_bytes = static_cast<uint64_t>(st.st_size);
    metadata_out->start_xid = header.start_transaction_id;
    metadata_out->end_xid = header.end_transaction_id;
    metadata_out->valid = true;

    return Status::OK;
}

Status BackupManager::listBackups(const std::string& backup_dir,
                                   std::vector<BackupMetadata>* backups_out,
                                   ErrorContext* ctx)
{
    // Placeholder - would scan directory for .sbkp files
    if (!backups_out) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null backups output");
        return Status::INVALID_ARGUMENT;
    }
    backups_out->clear();
    return Status::OK;
}

Status BackupManager::buildBackupChain(const std::string& backup_path,
                                        std::vector<std::string>* chain_out,
                                        ErrorContext* ctx)
{
    if (!chain_out) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null chain output");
        return Status::INVALID_ARGUMENT;
    }

    chain_out->clear();
    chain_out->push_back(backup_path);

    // Would follow parent_backup_uuid chain in full implementation
    return Status::OK;
}

void BackupManager::markPageModified(GPID gpid)
{
    if (!change_tracking_enabled_) return;

    std::lock_guard<std::mutex> lock(modified_pages_mutex_);
    modified_pages_.insert(gpid);
}

std::vector<GPID> BackupManager::getModifiedPages() const
{
    std::lock_guard<std::mutex> lock(modified_pages_mutex_);
    return std::vector<GPID>(modified_pages_.begin(), modified_pages_.end());
}

void BackupManager::clearModifiedPages()
{
    std::lock_guard<std::mutex> lock(modified_pages_mutex_);
    modified_pages_.clear();
}

void BackupManager::setChangeTrackingEnabled(bool enabled)
{
    change_tracking_enabled_ = enabled;
}

// Internal helpers

Status BackupManager::writeBackupHeader(int fd, const BackupManifestHeader& header,
                                         ErrorContext* ctx)
{
    ssize_t written = ::write(fd, &header, sizeof(header));
    if (written != sizeof(header)) {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write backup header");
        return Status::IO_ERROR;
    }
    return Status::OK;
}

Status BackupManager::readBackupHeader(int fd, BackupManifestHeader* header,
                                        ErrorContext* ctx)
{
    ssize_t read_bytes = ::read(fd, header, sizeof(*header));
    if (read_bytes != sizeof(*header)) {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read backup header");
        return Status::IO_ERROR;
    }

    // Validate magic
    if (std::memcmp(header->magic, BACKUP_MAGIC, 8) != 0) {
        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid backup file magic");
        return Status::DATA_CORRUPTED;
    }

    // Validate version
    if (header->version > BACKUP_VERSION) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "Backup version too new");
        return Status::NOT_SUPPORTED;
    }

    return Status::OK;
}

Status BackupManager::compressPage(const uint8_t* input, uint32_t input_size,
                                    std::vector<uint8_t>& output,
                                    CompressionType type, int level,
                                    ErrorContext* ctx)
{
    if (type == CompressionType::NONE) {
        output.assign(input, input + input_size);
        return Status::OK;
    }

    if (type == CompressionType::ZLIB) {
        uLongf dest_len = compressBound(input_size);
        output.resize(dest_len);

        int ret = compress2(output.data(), &dest_len, input, input_size, level);
        if (ret != Z_OK) {
            SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "zlib compression failed");
            return Status::INTERNAL_ERROR;
        }

        output.resize(dest_len);
        return Status::OK;
    }

    SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "Compression type not supported");
    return Status::NOT_SUPPORTED;
}

Status BackupManager::decompressPage(const uint8_t* input, uint32_t input_size,
                                      uint8_t* output, uint32_t output_size,
                                      CompressionType type, ErrorContext* ctx)
{
    if (type == CompressionType::NONE) {
        std::memcpy(output, input, std::min(input_size, output_size));
        return Status::OK;
    }

    if (type == CompressionType::ZLIB) {
        uLongf dest_len = output_size;
        int ret = uncompress(output, &dest_len, input, input_size);
        if (ret != Z_OK) {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "zlib decompression failed");
            return Status::DATA_CORRUPTED;
        }
        return Status::OK;
    }

    SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "Compression type not supported");
    return Status::NOT_SUPPORTED;
}

uint32_t BackupManager::calculateChecksum(const uint8_t* data, size_t size)
{
    // Use CRC32 from zlib
    return static_cast<uint32_t>(crc32(0L, data, static_cast<uInt>(size)));
}

// =================================================================================================
// BackupCatalog Implementation
// =================================================================================================

BackupCatalog::BackupCatalog(const std::string& catalog_path)
    : catalog_path_(catalog_path)
{
}

BackupCatalog::~BackupCatalog()
{
    if (modified_) {
        save(nullptr);
    }
}

Status BackupCatalog::addBackup(const BackupMetadata& metadata, ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);
    backups_.push_back(metadata);
    modified_ = true;
    return Status::OK;
}

Status BackupCatalog::removeBackup(const UuidV7Bytes& backup_id, ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(backups_.begin(), backups_.end(),
                          [&](const BackupMetadata& m) {
                              return std::memcmp(&m.backup_id, &backup_id, 16) == 0;
                          });

    if (it == backups_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Backup not found");
        return Status::NOT_FOUND;
    }

    backups_.erase(it);
    modified_ = true;
    return Status::OK;
}

Status BackupCatalog::getBackup(const UuidV7Bytes& backup_id, BackupMetadata* metadata_out,
                                 ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(backups_.begin(), backups_.end(),
                          [&](const BackupMetadata& m) {
                              return std::memcmp(&m.backup_id, &backup_id, 16) == 0;
                          });

    if (it == backups_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Backup not found");
        return Status::NOT_FOUND;
    }

    *metadata_out = *it;
    return Status::OK;
}

Status BackupCatalog::listBackups(std::vector<BackupMetadata>* backups_out, ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);
    *backups_out = backups_;
    return Status::OK;
}

Status BackupCatalog::getLatestFullBackup(BackupMetadata* metadata_out, ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    BackupMetadata* latest = nullptr;
    for (auto& backup : backups_) {
        if (backup.type == BackupType::FULL && backup.valid) {
            if (!latest || backup.end_time > latest->end_time) {
                latest = &backup;
            }
        }
    }

    if (!latest) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "No full backup found");
        return Status::NOT_FOUND;
    }

    *metadata_out = *latest;
    return Status::OK;
}

Status BackupCatalog::getIncrementalChain(const UuidV7Bytes& backup_id,
                                           std::vector<BackupMetadata>* chain_out,
                                           ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    chain_out->clear();

    // Find the target backup
    BackupMetadata* current = nullptr;
    for (auto& backup : backups_) {
        if (std::memcmp(&backup.backup_id, &backup_id, 16) == 0) {
            current = &backup;
            break;
        }
    }

    if (!current) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Backup not found");
        return Status::NOT_FOUND;
    }

    // Build chain backwards to full backup
    std::vector<BackupMetadata> chain;
    while (current) {
        chain.push_back(*current);

        if (current->type == BackupType::FULL) {
            break;
        }

        // Find parent
        BackupMetadata* parent = nullptr;
        for (auto& backup : backups_) {
            if (std::memcmp(&backup.backup_id, &current->parent_id, 16) == 0) {
                parent = &backup;
                break;
            }
        }
        current = parent;
    }

    // Reverse to get full -> incremental order
    std::reverse(chain.begin(), chain.end());
    *chain_out = std::move(chain);

    return Status::OK;
}

Status BackupCatalog::save(ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::ofstream file(catalog_path_, std::ios::binary);
    if (!file) {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open catalog file for writing");
        return Status::IO_ERROR;
    }

    // Simple format: count + entries
    uint64_t count = backups_.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& backup : backups_) {
        // Write fixed-size fields
        file.write(reinterpret_cast<const char*>(&backup.backup_id), 16);
        file.write(reinterpret_cast<const char*>(&backup.parent_id), 16);
        file.write(reinterpret_cast<const char*>(&backup.type), sizeof(backup.type));
        file.write(reinterpret_cast<const char*>(&backup.start_time), sizeof(backup.start_time));
        file.write(reinterpret_cast<const char*>(&backup.end_time), sizeof(backup.end_time));
        file.write(reinterpret_cast<const char*>(&backup.total_pages), sizeof(backup.total_pages));
        file.write(reinterpret_cast<const char*>(&backup.size_bytes), sizeof(backup.size_bytes));
        file.write(reinterpret_cast<const char*>(&backup.start_xid), sizeof(backup.start_xid));
        file.write(reinterpret_cast<const char*>(&backup.end_xid), sizeof(backup.end_xid));
        file.write(reinterpret_cast<const char*>(&backup.valid), sizeof(backup.valid));

        // Write variable-size strings
        uint32_t path_len = static_cast<uint32_t>(backup.path.size());
        file.write(reinterpret_cast<const char*>(&path_len), sizeof(path_len));
        file.write(backup.path.data(), path_len);

        uint32_t label_len = static_cast<uint32_t>(backup.label.size());
        file.write(reinterpret_cast<const char*>(&label_len), sizeof(label_len));
        file.write(backup.label.data(), label_len);
    }

    modified_ = false;
    return Status::OK;
}

Status BackupCatalog::load(ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::ifstream file(catalog_path_, std::ios::binary);
    if (!file) {
        // No catalog file yet - not an error
        return Status::OK;
    }

    uint64_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    backups_.clear();
    backups_.reserve(count);

    for (uint64_t i = 0; i < count; ++i) {
        BackupMetadata backup;

        file.read(reinterpret_cast<char*>(&backup.backup_id), 16);
        file.read(reinterpret_cast<char*>(&backup.parent_id), 16);
        file.read(reinterpret_cast<char*>(&backup.type), sizeof(backup.type));
        file.read(reinterpret_cast<char*>(&backup.start_time), sizeof(backup.start_time));
        file.read(reinterpret_cast<char*>(&backup.end_time), sizeof(backup.end_time));
        file.read(reinterpret_cast<char*>(&backup.total_pages), sizeof(backup.total_pages));
        file.read(reinterpret_cast<char*>(&backup.size_bytes), sizeof(backup.size_bytes));
        file.read(reinterpret_cast<char*>(&backup.start_xid), sizeof(backup.start_xid));
        file.read(reinterpret_cast<char*>(&backup.end_xid), sizeof(backup.end_xid));
        file.read(reinterpret_cast<char*>(&backup.valid), sizeof(backup.valid));

        uint32_t path_len;
        file.read(reinterpret_cast<char*>(&path_len), sizeof(path_len));
        backup.path.resize(path_len);
        file.read(backup.path.data(), path_len);

        uint32_t label_len;
        file.read(reinterpret_cast<char*>(&label_len), sizeof(label_len));
        backup.label.resize(label_len);
        file.read(backup.label.data(), label_len);

        backups_.push_back(std::move(backup));
    }

    modified_ = false;
    return Status::OK;
}

} // namespace scratchbird::core
