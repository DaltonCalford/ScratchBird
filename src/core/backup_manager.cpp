// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P2-23: Backup/Restore Manager Implementation
//
// November 25, 2025

#include "scratchbird/core/backup_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/logger.h"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <zlib.h>

namespace scratchbird::core {

// Magic number for backup files
static constexpr char BACKUP_MAGIC[8] = {'S', 'B', 'K', 'P', '0', '0', '0', '1'};
static constexpr uint64_t BACKUP_VERSION = 1;

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

    // Determine pages to backup
    std::vector<GPID> pages_to_backup;
    if (config.type == BackupType::FULL) {
        // Backup all pages
        uint64_t total_pages = db_->total_pages();
        pages_to_backup.reserve(total_pages);
        for (uint64_t i = 0; i < total_pages; ++i) {
            pages_to_backup.push_back(makeGPID(0, static_cast<uint32_t>(i)));
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

    // Read page index
    std::vector<BackupPageEntry> page_entries(header.total_pages);
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

        // Write to target database
        uint32_t page_id = static_cast<uint32_t>(getPageNumber(entry.gpid));
        off_t offset = static_cast<off_t>(page_id) * header.page_size;
        ::lseek(target_fd, offset, SEEK_SET);
        ssize_t written = ::write(target_fd, page_data, entry.original_size);
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

    ::fsync(target_fd);
    ::close(backup_fd);
    ::close(target_fd);

    if (progress) {
        progress->completed = true;
    }

    LOG_INFO(GENERAL, "Restore completed: %lu pages", header.total_pages);

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
