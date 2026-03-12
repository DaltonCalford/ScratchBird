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
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/tablespace.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/portable_file_io.h"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <filesystem>
#include <limits>
#include <fcntl.h>
#include "scratchbird/core/posix_compat.h"
#include <sys/stat.h>
#include <zlib.h>

namespace scratchbird::core {

// Magic number for backup files
static constexpr char BACKUP_MAGIC[8] = {'S', 'B', 'K', 'P', '0', '0', '0', '1'};
static constexpr uint64_t BACKUP_VERSION = 4;
static constexpr char BACKUP_CATALOG_MAGIC[8] = {'S', 'B', 'C', 'A', 'T', '0', '0', '1'};
static constexpr uint32_t BACKUP_CATALOG_VERSION = 2;
static constexpr char BACKUP_CATALOG_FILENAME[] = ".scratchbird_backup_catalog.sbcat";
static constexpr char BACKUP_JOB_MAGIC[8] = {'S', 'B', 'K', 'J', '0', '0', '0', '1'};
static constexpr uint32_t BACKUP_JOB_VERSION = 1;

struct BackupCatalogFileHeader
{
    char magic[8];
    uint32_t version = BACKUP_CATALOG_VERSION;
    uint32_t flags = 0;
    uint32_t backup_count = 0;
    uint32_t retention_rule_count = 0;
};

struct LegacyBackupCatalogPolicyRecord
{
    uint32_t schema_version = 1;
    uint8_t enabled = 1;
    uint8_t reserved0[3]{};
};

struct BackupCatalogPolicyRecord
{
    uint32_t schema_version = 1;
    uint8_t enabled = 1;
    uint8_t pitr_mode = static_cast<uint8_t>(PITRMode::DISABLED);
    uint8_t reserved0[2]{};
    uint64_t pitr_retention_window_micros = 0;
};

struct BackupCatalogRetentionRuleRecord
{
    uint8_t type = 0;
    uint8_t require_parent_chain = 0;
    uint16_t reserved0 = 0;
    uint32_t retain_count = 0;
    uint64_t max_age_micros = 0;
};

struct BackupCatalogMetadataRecord
{
    UuidV7Bytes backup_id;
    UuidV7Bytes parent_id;
    uint8_t type = 0;
    uint8_t valid = 0;
    uint8_t reserved0[6]{};
    uint64_t start_time = 0;
    uint64_t end_time = 0;
    uint64_t total_pages = 0;
    uint64_t size_bytes = 0;
    uint64_t start_xid = 0;
    uint64_t end_xid = 0;
};

struct BackupJobCheckpointHeader
{
    char magic[8];
    uint32_t version = BACKUP_JOB_VERSION;
    uint8_t state = 0;
    uint8_t backup_type = 0;
    uint8_t compression = 0;
    uint8_t compression_level = 0;
    uint32_t page_size = 0;
    uint64_t total_pages = 0;
    uint64_t pages_processed = 0;
    uint64_t bytes_written = 0;
    uint64_t started_time = 0;
    uint64_t last_updated_time = 0;
    uint64_t completed_time = 0;
    uint32_t resume_count = 0;
    uint32_t page_entries_count = 0;
    UuidV7Bytes backup_id;
    UuidV7Bytes parent_backup_id;
};

struct BackupTablespaceInfo
{
    uint16_t tablespace_id = 0;
    uint64_t total_pages = 0;
    std::vector<std::string> file_paths;
    std::vector<uint64_t> file_start_pages;
    std::vector<uint64_t> file_page_counts;
};

struct BackupJobCheckpoint
{
    BackupConfig config;
    BackupJobResult result;
    uint32_t page_size = 0;
    std::vector<GPID> pages_to_backup;
    std::vector<BackupPageEntry> page_entries;
};

static Status readTablespaceFilePageCount(const std::string& path,
                                          uint32_t page_size,
                                          uint64_t* pages_out,
                                          ErrorContext* ctx);
static auto calculateChecksumRaw(const uint8_t* data, size_t size) -> uint32_t;
static auto decodeBackupId(const BackupManifestHeader& header) -> UuidV7Bytes;

static auto isZeroUuid(const UuidV7Bytes& id) -> bool
{
    return id == UuidV7Bytes{};
}

static auto nowMicros() -> uint64_t
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

static auto normalizeBackupPath(const std::string& path) -> std::string
{
    if (path.empty())
    {
        return path;
    }

    try
    {
        const auto fs_path = std::filesystem::path(path);
        return std::filesystem::absolute(fs_path).lexically_normal().string();
    }
    catch (...)
    {
        return path;
    }
}

static auto backupDirectoryFromPath(const std::string& backup_path) -> std::string
{
    const auto fs_path = std::filesystem::path(backup_path).parent_path();
    if (fs_path.empty())
    {
        return std::filesystem::current_path().string();
    }
    return normalizeBackupPath(fs_path.string());
}

static auto backupCatalogPathForDir(const std::string& backup_dir) -> std::string
{
    return (std::filesystem::path(backup_dir) / BACKUP_CATALOG_FILENAME).string();
}

static auto backupJobStatePath(const std::string& backup_path) -> std::string
{
    return backup_path + ".sbkjob";
}

static auto defaultPITRPolicy() -> PITRPolicy
{
    return PITRPolicy{};
}

static auto isSupportedPITRMode(PITRMode mode) -> bool
{
    switch (mode)
    {
        case PITRMode::DISABLED:
        case PITRMode::SNAPSHOT_BOUNDARY:
            return true;
        default:
            return false;
    }
}

struct AppliedBackupSelection
{
    std::vector<BackupMetadata> chain_metadata;
    std::vector<BackupMetadata> applied_metadata;
    uint64_t latest_available_time = 0;
};

struct RollbackCheckpointAssessment
{
    UuidV7Bytes source_database_id;
    uint32_t marker_version = 0;
    uint32_t source_db_version = 0;
    uint32_t source_db_compat_version = 0;
    bool validated = false;
};

static auto validateBackupHeaderChecksumRaw(const BackupManifestHeader& header,
                                            ErrorContext* ctx) -> Status
{
    BackupManifestHeader checksum_header = header;
    const uint32_t stored_checksum = checksum_header.checksum;
    checksum_header.checksum = 0;
    const uint32_t computed_checksum =
        calculateChecksumRaw(reinterpret_cast<const uint8_t*>(&checksum_header),
                             sizeof(checksum_header));
    if (stored_checksum != computed_checksum)
    {
        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Backup header checksum mismatch");
        return Status::DATA_CORRUPTED;
    }
    return Status::OK;
}

static auto populateBackupMetadataFromHeader(const BackupManifestHeader& header,
                                             const std::string& normalized_backup_path,
                                             uint64_t size_bytes) -> BackupMetadata
{
    BackupMetadata metadata;
    metadata.backup_id = decodeBackupId(header);
    std::memcpy(metadata.parent_id.bytes.data(),
                header.parent_backup_uuid,
                metadata.parent_id.bytes.size());
    metadata.type = header.type;
    metadata.path = normalized_backup_path;
    metadata.storage_profile = "local_fs";
    metadata.label = header.label;
    metadata.start_time = header.backup_start_time;
    metadata.end_time = header.backup_end_time;
    metadata.total_pages = header.total_pages;
    metadata.size_bytes = size_bytes;
    metadata.start_xid = header.start_transaction_id;
    metadata.end_xid = header.end_transaction_id;
    metadata.valid = true;
    metadata.manifest_version = header.version;
    std::memcpy(metadata.source_database_id.bytes.data(),
                header.db_uuid,
                metadata.source_database_id.bytes.size());
    metadata.source_db_version = header.source_db_version;
    metadata.source_db_compat_version = header.source_db_compat_version;
    metadata.rollback_checkpoint_version = header.rollback_checkpoint_version;
    metadata.rollback_flags = header.rollback_flags;
    return metadata;
}

static auto validateRollbackCheckpointMetadata(const BackupMetadata& metadata,
                                               bool require_explicit_marker,
                                               RollbackCheckpointAssessment* assessment_out,
                                               ErrorContext* ctx) -> Status
{
    const bool has_marker = metadata.rollback_checkpoint_version != 0;
    if (!has_marker)
    {
        if (require_explicit_marker)
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::NOT_SUPPORTED,
                              "Backup chain includes an artifact without explicit rollback checkpoint markers");
            return Status::NOT_SUPPORTED;
        }
        return Status::OK;
    }

    if (metadata.rollback_checkpoint_version > BACKUP_ROLLBACK_CHECKPOINT_VERSION_CURRENT)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "Rollback checkpoint marker version too new");
        return Status::NOT_SUPPORTED;
    }
    if ((metadata.rollback_flags & BACKUP_ROLLBACK_REQUIRED_FLAGS) != BACKUP_ROLLBACK_REQUIRED_FLAGS)
    {
        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Rollback checkpoint flags are incomplete");
        return Status::DATA_CORRUPTED;
    }
    if (isZeroUuid(metadata.source_database_id))
    {
        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Rollback checkpoint source database UUID missing");
        return Status::DATA_CORRUPTED;
    }

    Status status = validateDatabaseFormatCompatibility(metadata.source_db_version,
                                                        metadata.source_db_compat_version,
                                                        ctx);
    if (status != Status::OK)
    {
        return status;
    }

    if (!assessment_out)
    {
        return Status::OK;
    }

    if (!assessment_out->validated)
    {
        assessment_out->source_database_id = metadata.source_database_id;
        assessment_out->marker_version = metadata.rollback_checkpoint_version;
        assessment_out->source_db_version = metadata.source_db_version;
        assessment_out->source_db_compat_version = metadata.source_db_compat_version;
        assessment_out->validated = true;
        return Status::OK;
    }

    assessment_out->marker_version =
        std::max(assessment_out->marker_version, metadata.rollback_checkpoint_version);
    if (metadata.source_database_id != assessment_out->source_database_id)
    {
        SET_ERROR_CONTEXT(ctx,
                          Status::NOT_SUPPORTED,
                          "Backup chain crosses source database identity boundary");
        return Status::NOT_SUPPORTED;
    }
    if (metadata.source_db_version != assessment_out->source_db_version)
    {
        SET_ERROR_CONTEXT(ctx,
                          Status::NOT_SUPPORTED,
                          "Backup chain crosses on-disk format boundary");
        return Status::NOT_SUPPORTED;
    }
    if (metadata.source_db_compat_version != assessment_out->source_db_compat_version)
    {
        SET_ERROR_CONTEXT(ctx,
                          Status::NOT_SUPPORTED,
                          "Backup chain crosses compatibility-floor boundary");
        return Status::NOT_SUPPORTED;
    }

    return Status::OK;
}

static auto validateRollbackCheckpointChain(const std::vector<BackupMetadata>& metadata_chain,
                                            RollbackCheckpointAssessment* assessment_out,
                                            ErrorContext* ctx) -> Status
{
    if (assessment_out)
    {
        *assessment_out = RollbackCheckpointAssessment{};
    }
    if (metadata_chain.empty())
    {
        return Status::OK;
    }

    RollbackCheckpointAssessment assessment;
    const bool require_explicit_marker = metadata_chain.size() > 1;
    for (const auto& metadata : metadata_chain)
    {
        Status status = validateRollbackCheckpointMetadata(metadata,
                                                          require_explicit_marker,
                                                          &assessment,
                                                          ctx);
        if (status != Status::OK)
        {
            return status;
        }
    }

    if (assessment_out)
    {
        *assessment_out = assessment;
    }
    return Status::OK;
}

static Status resolveAppliedBackupSelection(BackupManager* backup_mgr,
                                            const std::vector<std::string>& backup_chain,
                                            uint64_t target_time,
                                            AppliedBackupSelection* selection_out,
                                            ErrorContext* ctx)
{
    if (backup_mgr == nullptr || selection_out == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing PITR selection output");
        return Status::INVALID_ARGUMENT;
    }
    *selection_out = AppliedBackupSelection{};

    if (backup_chain.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Empty backup chain");
        return Status::INVALID_ARGUMENT;
    }

    selection_out->chain_metadata.reserve(backup_chain.size());
    for (const auto& backup_path : backup_chain)
    {
        BackupMetadata metadata;
        Status status = backup_mgr->getBackupMetadata(backup_path, &metadata, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        metadata.path = normalizeBackupPath(metadata.path);
        selection_out->latest_available_time =
            std::max(selection_out->latest_available_time, metadata.end_time);
        selection_out->chain_metadata.push_back(std::move(metadata));
    }

    if (selection_out->chain_metadata.front().type != BackupType::FULL)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          "Restore chain must start with a standalone FULL backup");
        return Status::INVALID_ARGUMENT;
    }
    for (size_t i = 1; i < selection_out->chain_metadata.size(); ++i)
    {
        const auto& previous = selection_out->chain_metadata[i - 1];
        const auto& current = selection_out->chain_metadata[i];
        if (current.type == BackupType::FULL)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Restore chain cannot contain a second FULL backup");
            return Status::INVALID_ARGUMENT;
        }
        if (current.end_time < previous.end_time)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Restore chain timestamps must be monotonic");
            return Status::INVALID_ARGUMENT;
        }
    }

    if (target_time > 0 &&
        selection_out->chain_metadata.front().end_time > 0 &&
        target_time < selection_out->chain_metadata.front().end_time)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          "Requested target time predates the base FULL backup");
        return Status::INVALID_ARGUMENT;
    }

    selection_out->applied_metadata.reserve(selection_out->chain_metadata.size());
    selection_out->applied_metadata.push_back(selection_out->chain_metadata.front());
    for (size_t i = 1; i < selection_out->chain_metadata.size(); ++i)
    {
        const auto& metadata = selection_out->chain_metadata[i];
        if (target_time > 0 && metadata.end_time > target_time)
        {
            break;
        }
        selection_out->applied_metadata.push_back(metadata);
    }

    return Status::OK;
}

static Status enforcePITRTargetPolicy(BackupManager* backup_mgr,
                                      const AppliedBackupSelection& selection,
                                      uint64_t target_time,
                                      ErrorContext* ctx)
{
    if (backup_mgr == nullptr || target_time == 0 || selection.chain_metadata.empty())
    {
        return Status::OK;
    }

    PITRPolicy pitr_policy;
    Status status = backup_mgr->getPITRPolicy(
        backupDirectoryFromPath(selection.chain_metadata.front().path),
        &pitr_policy,
        ctx);
    if (status != Status::OK)
    {
        return status;
    }
    if (pitr_policy.mode == PITRMode::DISABLED)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                          "PITR extension is disabled; MGA snapshot restore remains authoritative");
        return Status::NOT_IMPLEMENTED;
    }
    if (pitr_policy.mode != PITRMode::SNAPSHOT_BOUNDARY)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "Configured PITR mode is not supported");
        return Status::NOT_SUPPORTED;
    }
    if (pitr_policy.retention_window_micros > 0 &&
        selection.latest_available_time > target_time &&
        (selection.latest_available_time - target_time) > pitr_policy.retention_window_micros)
    {
        SET_ERROR_CONTEXT(ctx, Status::CONFIGURATION_LIMIT_EXCEEDED,
                          "Requested target time falls outside the PITR retention window");
        return Status::CONFIGURATION_LIMIT_EXCEEDED;
    }

    return Status::OK;
}

struct RestoredDatabaseSnapshot
{
    uint32_t page_size = 0;
    uint64_t total_pages = 0;
    ID database_id{};
    uint32_t db_version = 0;
    uint32_t db_compat_version = 0;
};

static Status inspectRestoredDatabase(const std::string& database_path,
                                      RestoredDatabaseSnapshot* snapshot_out,
                                      ErrorContext* ctx)
{
    if (snapshot_out == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing restored database snapshot output");
        return Status::INVALID_ARGUMENT;
    }
    *snapshot_out = RestoredDatabaseSnapshot{};

    int fd = platform::openFd(database_path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND, "Restored database file not found");
        return Status::FILE_NOT_FOUND;
    }

    PageHeader page_header{};
    const ssize_t header_bytes = platform::readAt(fd, &page_header, sizeof(page_header), 0);
    if (header_bytes != static_cast<ssize_t>(sizeof(page_header)))
    {
        ::close(fd);
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read restored database header");
        return Status::IO_ERROR;
    }

    if (page_header.magic != K_MAGIC_SBRD)
    {
        ::close(fd);
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Restored database header has invalid magic");
        return Status::PAGE_CORRUPT;
    }
    if (!isValidAlphaPageSize(page_header.page_size))
    {
        ::close(fd);
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Restored database header has invalid page size");
        return Status::PAGE_CORRUPT;
    }

    std::vector<uint8_t> header_buffer(page_header.page_size, 0);
    const ssize_t full_header_bytes =
        platform::readAt(fd, header_buffer.data(), page_header.page_size, 0);
    if (full_header_bytes != static_cast<ssize_t>(page_header.page_size))
    {
        ::close(fd);
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read restored database header page");
        return Status::IO_ERROR;
    }

    struct stat st{};
    if (fstat(fd, &st) != 0)
    {
        ::close(fd);
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to stat restored database file");
        return Status::IO_ERROR;
    }
    ::close(fd);

    const auto* db_header =
        reinterpret_cast<const DatabaseHeader*>(header_buffer.data());
    Status status = validatePageHeaderContract(db_header->page_header,
                                               page_header.page_size,
                                               PAGE_TYPE_DATABASE_HEADER,
                                               nullptr,
                                               nullptr);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, status, "Restored database header contract is invalid");
        return status;
    }
    if (!validatePageChecksum(header_buffer.data(), page_header.page_size))
    {
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Restored database header checksum mismatch");
        return Status::PAGE_CORRUPT;
    }
    if (db_header->block_size != db_header->page_header.page_size)
    {
        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Restored database block size mismatch");
        return Status::DATA_CORRUPTED;
    }
    status = validateDatabaseFormatCompatibility(db_header->db_version,
                                                 db_header->db_compat_version,
                                                 ctx);
    if (status != Status::OK)
    {
        return status;
    }
    if (db_header->total_pages == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Restored database is empty");
        return Status::DATA_CORRUPTED;
    }

    const uint64_t page_size = db_header->page_header.page_size;
    if (db_header->total_pages > (std::numeric_limits<uint64_t>::max() / page_size))
    {
        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Restored database header size overflows");
        return Status::DATA_CORRUPTED;
    }
    const uint64_t minimum_size_bytes = db_header->total_pages * page_size;
    if (st.st_size < 0 || static_cast<uint64_t>(st.st_size) < minimum_size_bytes)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Restored database file is truncated");
        return Status::IO_ERROR;
    }

    snapshot_out->page_size = db_header->page_header.page_size;
    snapshot_out->total_pages = db_header->total_pages;
    snapshot_out->database_id = db_header->database_uuid;
    snapshot_out->db_version = db_header->db_version;
    snapshot_out->db_compat_version = db_header->db_compat_version;
    return Status::OK;
}

static auto backupTempPath(const std::string& backup_path) -> std::string
{
    return backup_path + ".part";
}

static auto backupIndexOffset(const BackupManifestHeader& header) -> uint64_t
{
    const uint64_t offset = header.tablespace_info_offset + header.tablespace_info_size;
    return offset == 0 ? sizeof(BackupManifestHeader) : offset;
}

static auto backupDataOffset(const BackupManifestHeader& header) -> uint64_t
{
    return backupIndexOffset(header) +
           (header.total_pages * static_cast<uint64_t>(sizeof(BackupPageEntry)));
}

static auto backupJobStateIsTerminal(BackupJobState state) -> bool
{
    return state == BackupJobState::COMPLETED ||
           state == BackupJobState::FAILED ||
           state == BackupJobState::CANCELLED;
}

static auto defaultBackupPolicy() -> BackupPolicy
{
    BackupPolicy policy;
    policy.schema_version = 1;
    policy.enabled = true;
    policy.policy_name = "default_local_backup";
    policy.storage_profile = "local_fs";
    policy.retention_rules = {
        BackupRetentionRule{BackupType::FULL, 2, false, 0},
        BackupRetentionRule{BackupType::INCREMENTAL, 7, true, 0},
        BackupRetentionRule{BackupType::DIFFERENTIAL, 2, true, 0},
    };
    return policy;
}

static auto lookupRetentionRule(const BackupPolicy& policy, BackupType type) -> const BackupRetentionRule*
{
    for (const auto& rule : policy.retention_rules)
    {
        if (rule.type == type)
        {
            return &rule;
        }
    }
    return nullptr;
}

static void encodeBackupId(BackupManifestHeader* header, const UuidV7Bytes& backup_id)
{
    if (!header)
    {
        return;
    }
    std::memcpy(header->reserved2, backup_id.bytes.data(), backup_id.bytes.size());
}

static auto decodeBackupId(const BackupManifestHeader& header) -> UuidV7Bytes
{
    UuidV7Bytes backup_id;
    if (header.version >= 4)
    {
        std::memcpy(backup_id.bytes.data(), header.reserved2, backup_id.bytes.size());
    }
    return backup_id;
}

static auto currentConnectionUserId() -> ID
{
    auto* current = ConnectionContext::getCurrent();
    if (!current)
    {
        return ID{};
    }
    return current->getCurrentUserId();
}

static auto mapBackupKind(BackupType type) -> CatalogManager::BackupHistoryKind
{
    switch (type)
    {
        case BackupType::INCREMENTAL:
            return CatalogManager::BackupHistoryKind::INCREMENTAL;
        case BackupType::DIFFERENTIAL:
            return CatalogManager::BackupHistoryKind::DIFFERENTIAL;
        case BackupType::FULL:
        default:
            return CatalogManager::BackupHistoryKind::FULL;
    }
}

static auto mapBackupJobStateToString(BackupJobState state) -> const char*
{
    switch (state)
    {
        case BackupJobState::PREPARING:
            return "PREPARING";
        case BackupJobState::WRITING:
            return "WRITING";
        case BackupJobState::FINALIZING:
            return "FINALIZING";
        case BackupJobState::PAUSED:
            return "PAUSED";
        case BackupJobState::COMPLETED:
            return "COMPLETED";
        case BackupJobState::FAILED:
            return "FAILED";
        case BackupJobState::CANCELLED:
            return "CANCELLED";
    }
    return "UNKNOWN";
}

static auto readLengthPrefixedString(std::istream& input, std::string* value) -> bool
{
    if (!value)
    {
        return false;
    }

    uint32_t size = 0;
    input.read(reinterpret_cast<char*>(&size), sizeof(size));
    if (!input)
    {
        return false;
    }

    value->assign(size, '\0');
    if (size > 0)
    {
        input.read(value->data(), size);
    }
    return static_cast<bool>(input);
}

static auto writeLengthPrefixedString(std::ostream& output, const std::string& value) -> bool
{
    const uint32_t size = static_cast<uint32_t>(value.size());
    output.write(reinterpret_cast<const char*>(&size), sizeof(size));
    if (!output)
    {
        return false;
    }
    if (size > 0)
    {
        output.write(value.data(), size);
    }
    return static_cast<bool>(output);
}

static auto writeVectorBytes(std::ostream& output, const void* data, size_t size) -> bool
{
    if (size == 0)
    {
        return static_cast<bool>(output);
    }
    output.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(output);
}

static auto readVectorBytes(std::istream& input, void* data, size_t size) -> bool
{
    if (size == 0)
    {
        return static_cast<bool>(input);
    }
    input.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(input);
}

static auto saveBackupJobCheckpoint(const BackupJobCheckpoint& checkpoint,
                                    ErrorContext* ctx) -> Status
{
    const std::string checkpoint_path = backupJobStatePath(checkpoint.result.backup_path);
    const std::string temp_path = checkpoint_path + ".tmp";
    std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open backup job checkpoint file");
        return Status::IO_ERROR;
    }

    BackupJobCheckpointHeader header{};
    std::memcpy(header.magic, BACKUP_JOB_MAGIC, sizeof(header.magic));
    header.version = BACKUP_JOB_VERSION;
    header.state = static_cast<uint8_t>(checkpoint.result.state);
    header.backup_type = static_cast<uint8_t>(checkpoint.result.type);
    header.compression = static_cast<uint8_t>(checkpoint.config.compression);
    header.compression_level = static_cast<uint8_t>(checkpoint.config.compression_level);
    header.page_size = checkpoint.page_size;
    header.total_pages = checkpoint.result.pages_total;
    header.pages_processed = checkpoint.result.pages_processed;
    header.bytes_written = checkpoint.result.bytes_written;
    header.started_time = checkpoint.result.started_time;
    header.last_updated_time = checkpoint.result.last_updated_time;
    header.completed_time = checkpoint.result.completed_time;
    header.resume_count = checkpoint.result.resume_count;
    header.page_entries_count = static_cast<uint32_t>(checkpoint.page_entries.size());
    header.backup_id = checkpoint.result.backup_id;
    header.parent_backup_id = checkpoint.result.parent_backup_id;
    output.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!output)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write backup job checkpoint header");
        return Status::IO_ERROR;
    }

    if (!writeLengthPrefixedString(output, checkpoint.result.backup_path) ||
        !writeLengthPrefixedString(output, checkpoint.result.storage_profile) ||
        !writeLengthPrefixedString(output, checkpoint.config.label) ||
        !writeLengthPrefixedString(output, checkpoint.result.error_message))
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write backup job checkpoint strings");
        return Status::IO_ERROR;
    }

    const uint64_t pages_count = checkpoint.pages_to_backup.size();
    output.write(reinterpret_cast<const char*>(&pages_count), sizeof(pages_count));
    if (!output)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write backup job page count");
        return Status::IO_ERROR;
    }

    if (!writeVectorBytes(output,
                          checkpoint.pages_to_backup.data(),
                          checkpoint.pages_to_backup.size() * sizeof(GPID)) ||
        !writeVectorBytes(output,
                          checkpoint.page_entries.data(),
                          checkpoint.page_entries.size() * sizeof(BackupPageEntry)))
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write backup job checkpoint vectors");
        return Status::IO_ERROR;
    }

    output.flush();
    if (!output)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to flush backup job checkpoint file");
        return Status::IO_ERROR;
    }
    output.close();

    std::error_code ec;
    std::filesystem::rename(temp_path, checkpoint_path, ec);
    if (ec)
    {
        std::filesystem::remove(checkpoint_path, ec);
        ec.clear();
        std::filesystem::rename(temp_path, checkpoint_path, ec);
    }
    if (ec)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to publish backup job checkpoint file");
        return Status::IO_ERROR;
    }

    return Status::OK;
}

static auto loadBackupJobCheckpoint(const std::string& backup_path,
                                    BackupJobCheckpoint* checkpoint_out,
                                    ErrorContext* ctx) -> Status
{
    if (!checkpoint_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null backup job checkpoint output");
        return Status::INVALID_ARGUMENT;
    }

    const std::string normalized_backup_path = normalizeBackupPath(backup_path);
    std::ifstream input(backupJobStatePath(normalized_backup_path), std::ios::binary);
    if (!input)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Backup job checkpoint not found");
        return Status::NOT_FOUND;
    }

    BackupJobCheckpointHeader header{};
    input.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!input)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read backup job checkpoint header");
        return Status::IO_ERROR;
    }
    if (std::memcmp(header.magic, BACKUP_JOB_MAGIC, sizeof(header.magic)) != 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid backup job checkpoint magic");
        return Status::DATA_CORRUPTED;
    }
    if (header.version > BACKUP_JOB_VERSION)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "Backup job checkpoint version too new");
        return Status::NOT_SUPPORTED;
    }

    BackupJobCheckpoint checkpoint;
    checkpoint.page_size = header.page_size;
    checkpoint.config.type = static_cast<BackupType>(header.backup_type);
    checkpoint.config.compression = static_cast<CompressionType>(header.compression);
    checkpoint.config.compression_level = static_cast<int>(header.compression_level);
    checkpoint.result.backup_id = header.backup_id;
    checkpoint.result.parent_backup_id = header.parent_backup_id;
    checkpoint.result.type = static_cast<BackupType>(header.backup_type);
    checkpoint.result.state = static_cast<BackupJobState>(header.state);
    checkpoint.result.pages_total = header.total_pages;
    checkpoint.result.pages_processed = header.pages_processed;
    checkpoint.result.bytes_written = header.bytes_written;
    checkpoint.result.resume_count = header.resume_count;
    checkpoint.result.started_time = header.started_time;
    checkpoint.result.last_updated_time = header.last_updated_time;
    checkpoint.result.completed_time = header.completed_time;

    if (!readLengthPrefixedString(input, &checkpoint.result.backup_path) ||
        !readLengthPrefixedString(input, &checkpoint.result.storage_profile) ||
        !readLengthPrefixedString(input, &checkpoint.config.label) ||
        !readLengthPrefixedString(input, &checkpoint.result.error_message))
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read backup job checkpoint strings");
        return Status::IO_ERROR;
    }
    checkpoint.result.backup_path = normalizeBackupPath(checkpoint.result.backup_path);

    uint64_t pages_count = 0;
    input.read(reinterpret_cast<char*>(&pages_count), sizeof(pages_count));
    if (!input)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read backup job page count");
        return Status::IO_ERROR;
    }

    checkpoint.pages_to_backup.resize(static_cast<size_t>(pages_count));
    checkpoint.page_entries.resize(header.page_entries_count);
    if (!readVectorBytes(input,
                         checkpoint.pages_to_backup.data(),
                         checkpoint.pages_to_backup.size() * sizeof(GPID)) ||
        !readVectorBytes(input,
                         checkpoint.page_entries.data(),
                         checkpoint.page_entries.size() * sizeof(BackupPageEntry)))
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read backup job checkpoint vectors");
        return Status::IO_ERROR;
    }

    *checkpoint_out = std::move(checkpoint);
    return Status::OK;
}

static auto upsertBackupHistoryState(Database* db,
                                     const BackupJobCheckpoint& checkpoint,
                                     CatalogManager::BackupHistoryStatus status_value,
                                     const std::string& error_message,
                                     ErrorContext* ctx) -> Status
{
    if (!db || !db->catalog_manager())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
        return Status::INVALID_ARGUMENT;
    }

    CatalogManager::BackupHistoryCatalogInfo history_info{};
    history_info.backup_id = checkpoint.result.backup_id;
    history_info.database_id = db->uuid();
    history_info.backup_kind = mapBackupKind(checkpoint.result.type);
    history_info.backup_status = status_value;
    history_info.storage_profile = checkpoint.result.storage_profile;
    history_info.storage_uri = checkpoint.result.backup_path;
    history_info.started_time = checkpoint.result.started_time;
    history_info.created_by_user_id = currentConnectionUserId();
    history_info.error_message = error_message;
    history_info.has_size_bytes = checkpoint.result.bytes_written > 0;
    history_info.size_bytes = checkpoint.result.bytes_written;
    history_info.has_checksum = false;
    if (status_value == CatalogManager::BackupHistoryStatus::SUCCESS)
    {
        history_info.completed_time = checkpoint.result.completed_time;
    }
    else if (status_value == CatalogManager::BackupHistoryStatus::FAILED ||
             status_value == CatalogManager::BackupHistoryStatus::CANCELLED)
    {
        history_info.completed_time = checkpoint.result.last_updated_time;
    }
    return db->catalog_manager()->upsertBackupHistoryCatalogEntry(history_info, ctx);
}

static auto buildTablespaceManifest(Database* db,
                                    std::vector<BackupTablespaceInfo>* tablespace_manifest,
                                    ErrorContext* ctx) -> Status
{
    if (!db || !tablespace_manifest)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid tablespace manifest arguments");
        return Status::INVALID_ARGUMENT;
    }
    auto* catalog = db->catalog_manager();
    auto* page_mgr = db->page_manager();
    if (!catalog || !page_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog or page manager not available");
        return Status::INVALID_ARGUMENT;
    }

    tablespace_manifest->clear();
    std::vector<TablespaceInfo> catalog_tablespaces;
    auto status = catalog->listTablespaces(catalog_tablespaces, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    bool has_primary = false;
    for (const auto& tablespace : catalog_tablespaces)
    {
        BackupTablespaceInfo info;
        info.tablespace_id = tablespace.tablespace_id;
        info.file_paths = tablespace.file_paths;
        if (info.tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            has_primary = true;
            if (info.file_paths.empty())
            {
                info.file_paths.push_back(db->path());
            }
            info.total_pages = db->total_pages();
        }
        else
        {
            uint32_t total_pages = 0;
            status = page_mgr->getTablespaceTotalPages(info.tablespace_id, &total_pages, ctx);
            if (status != Status::OK)
            {
                return status;
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
            for (const auto& path : info.file_paths)
            {
                uint64_t page_count = 0;
                if (info.tablespace_id == PRIMARY_TABLESPACE_ID)
                {
                    page_count = info.total_pages;
                }
                else
                {
                    status = readTablespaceFilePageCount(path, db->page_size(), &page_count, ctx);
                    if (status != Status::OK)
                    {
                        return status;
                    }
                }
                info.file_start_pages.push_back(start_page);
                info.file_page_counts.push_back(page_count);
                start_page += page_count;
            }
        }

        tablespace_manifest->push_back(std::move(info));
    }

    if (!has_primary)
    {
        BackupTablespaceInfo primary;
        primary.tablespace_id = PRIMARY_TABLESPACE_ID;
        primary.total_pages = db->total_pages();
        primary.file_paths.push_back(db->path());
        tablespace_manifest->push_back(std::move(primary));
    }

    return Status::OK;
}

static auto determinePagesToBackup(Database* db,
                                   BackupType type,
                                   const std::vector<BackupTablespaceInfo>& tablespace_manifest,
                                   const std::unordered_set<GPID>& modified_pages,
                                   std::vector<GPID>* pages_out,
                                   ErrorContext* ctx) -> Status
{
    if (!db || !pages_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid pages_to_backup arguments");
        return Status::INVALID_ARGUMENT;
    }

    pages_out->clear();
    if (type == BackupType::FULL)
    {
        uint64_t total_pages = 0;
        for (const auto& tablespace : tablespace_manifest)
        {
            total_pages += tablespace.total_pages;
        }
        pages_out->reserve(total_pages);
        for (const auto& tablespace : tablespace_manifest)
        {
            for (uint64_t i = 0; i < tablespace.total_pages; ++i)
            {
                pages_out->push_back(makeGPID(tablespace.tablespace_id,
                                              static_cast<uint32_t>(i)));
            }
        }
        return Status::OK;
    }

    pages_out->assign(modified_pages.begin(), modified_pages.end());
    std::sort(pages_out->begin(), pages_out->end());
    return Status::OK;
}

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

    int fd = platform::openFd(path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, ("Failed to open tablespace file: " + path).c_str());
        return Status::IO_ERROR;
    }

    std::vector<uint8_t> buffer(page_size, 0);
    ssize_t bytes_read = platform::readAt(fd, buffer.data(), page_size, 0);
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

    off_t offset = platform::seekFd(fd, 0, SEEK_CUR);
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

    off_t end_offset = platform::seekFd(fd, 0, SEEK_CUR);
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

    if (platform::seekFd(fd, static_cast<off_t>(offset), SEEK_SET) != static_cast<off_t>(offset))
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

static auto writeBackupHeaderRaw(int fd,
                                 const BackupManifestHeader& header,
                                 ErrorContext* ctx) -> Status
{
    const ssize_t written = ::write(fd, &header, sizeof(header));
    if (written != static_cast<ssize_t>(sizeof(header)))
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write backup header");
        return Status::IO_ERROR;
    }
    return Status::OK;
}

static auto readBackupHeaderRaw(int fd,
                                BackupManifestHeader* header,
                                ErrorContext* ctx) -> Status
{
    if (!header)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null backup header output");
        return Status::INVALID_ARGUMENT;
    }

    const ssize_t read_bytes = ::read(fd, header, sizeof(*header));
    if (read_bytes != static_cast<ssize_t>(sizeof(*header)))
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read backup header");
        return Status::IO_ERROR;
    }
    if (std::memcmp(header->magic, BACKUP_MAGIC, sizeof(header->magic)) != 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid backup file magic");
        return Status::DATA_CORRUPTED;
    }
    if (header->version > BACKUP_VERSION)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "Backup version too new");
        return Status::NOT_SUPPORTED;
    }
    return validateBackupHeaderChecksumRaw(*header, ctx);
}

static auto compressPageRaw(const uint8_t* input,
                            uint32_t input_size,
                            std::vector<uint8_t>* output,
                            CompressionType type,
                            int level,
                            ErrorContext* ctx) -> Status
{
    if (!output)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null compression output");
        return Status::INVALID_ARGUMENT;
    }

    if (type == CompressionType::NONE)
    {
        output->assign(input, input + input_size);
        return Status::OK;
    }

    if (type == CompressionType::ZLIB)
    {
        uLongf dest_len = compressBound(input_size);
        output->resize(dest_len);

        const int ret = compress2(output->data(), &dest_len, input, input_size, level);
        if (ret != Z_OK)
        {
            SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "zlib compression failed");
            return Status::INTERNAL_ERROR;
        }

        output->resize(dest_len);
        return Status::OK;
    }

    SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "Compression type not supported");
    return Status::NOT_SUPPORTED;
}

static auto calculateChecksumRaw(const uint8_t* data, size_t size) -> uint32_t
{
    return static_cast<uint32_t>(crc32(0L, data, static_cast<uInt>(size)));
}

static void initializeBackupProgressFromResult(const BackupJobResult& result,
                                               BackupProgress* progress)
{
    if (!progress)
    {
        return;
    }

    progress->start_time = std::chrono::steady_clock::now();
    progress->pages_total = result.pages_total;
    progress->pages_processed = result.pages_processed;
    progress->bytes_written = result.bytes_written;
    progress->completed = false;
    progress->cancelled = false;
}

static auto removePathIfExists(const std::string& path,
                               ErrorContext* ctx) -> Status
{
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, ("Failed to remove path: " + path).c_str());
        return Status::IO_ERROR;
    }
    return Status::OK;
}

static auto runBackupJobExecution(BackupManager* manager,
                                  Database* db,
                                  BackupJobCheckpoint* checkpoint,
                                  const BackupExecutionConfig& execution_config,
                                  BackupProgress* progress,
                                  ErrorContext* ctx) -> Status
{
    if (!manager || !db || !checkpoint)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid backup job execution arguments");
        return Status::INVALID_ARGUMENT;
    }

    initializeBackupProgressFromResult(checkpoint->result, progress);

    auto fail_job = [&](Status status_code,
                        BackupJobState terminal_state,
                        const std::string& message) -> Status {
        const std::string effective_message =
            !message.empty() ? message :
            ((ctx && !ctx->message.empty()) ? ctx->message : "Backup job failed");
        checkpoint->result.state = terminal_state;
        checkpoint->result.error_message = effective_message;
        checkpoint->result.last_updated_time = nowMicros();
        checkpoint->result.completed_time = checkpoint->result.last_updated_time;

        ErrorContext checkpoint_ctx;
        Status checkpoint_status = saveBackupJobCheckpoint(*checkpoint, &checkpoint_ctx);
        if (checkpoint_status != Status::OK)
        {
            if (ctx && ctx->message.empty())
            {
                ctx->message = checkpoint_ctx.message;
            }
            return checkpoint_status;
        }

        ErrorContext history_ctx;
        const auto history_status = upsertBackupHistoryState(
            db,
            *checkpoint,
            terminal_state == BackupJobState::CANCELLED
                ? CatalogManager::BackupHistoryStatus::CANCELLED
                : CatalogManager::BackupHistoryStatus::FAILED,
            effective_message,
            &history_ctx);
        if (history_status != Status::OK)
        {
            if (ctx && ctx->message.empty())
            {
                ctx->message = history_ctx.message;
            }
            return history_status;
        }

        if (progress)
        {
            progress->completed = true;
            progress->cancelled = (terminal_state == BackupJobState::CANCELLED);
            progress->pages_processed = checkpoint->result.pages_processed;
            progress->bytes_written = checkpoint->result.bytes_written;
        }

        if (ctx)
        {
            SET_ERROR_CONTEXT(ctx, status_code, effective_message.c_str());
        }
        return status_code;
    };

    const std::string temp_path = backupTempPath(checkpoint->result.backup_path);
    if (!std::filesystem::exists(temp_path))
    {
        return fail_job(Status::FILE_NOT_FOUND,
                        BackupJobState::FAILED,
                        "Backup job temp artifact missing during resume");
    }

    int backup_fd = platform::openFd(temp_path.c_str(), O_RDWR);
    if (backup_fd < 0)
    {
        return fail_job(Status::IO_ERROR,
                        BackupJobState::FAILED,
                        "Failed to open backup job temp artifact");
    }

    auto close_fd = [&]() {
        if (backup_fd >= 0)
        {
            ::close(backup_fd);
            backup_fd = -1;
        }
    };

    BackupManifestHeader header{};
    Status status = readBackupHeaderRaw(backup_fd, &header, ctx);
    if (status != Status::OK)
    {
        close_fd();
        return fail_job(status, BackupJobState::FAILED, "");
    }

    if (decodeBackupId(header) != checkpoint->result.backup_id)
    {
        close_fd();
        return fail_job(Status::DATA_CORRUPTED,
                        BackupJobState::FAILED,
                        "Backup job checkpoint does not match temp artifact");
    }

    if (header.total_pages != checkpoint->pages_to_backup.size() ||
        checkpoint->page_entries.size() != checkpoint->pages_to_backup.size())
    {
        close_fd();
        return fail_job(Status::DATA_CORRUPTED,
                        BackupJobState::FAILED,
                        "Backup job plan does not match stored page index");
    }

    const uint64_t index_offset = backupIndexOffset(header);
    uint64_t expected_end = backupDataOffset(header);
    const uint64_t processed_pages = std::min(checkpoint->result.pages_processed,
                                              static_cast<uint64_t>(checkpoint->page_entries.size()));
    for (uint64_t i = 0; i < processed_pages; ++i)
    {
        const auto& entry = checkpoint->page_entries[static_cast<size_t>(i)];
        if (entry.original_size == 0)
        {
            close_fd();
            return fail_job(Status::DATA_CORRUPTED,
                            BackupJobState::FAILED,
                            "Backup job checkpoint is missing a persisted page entry");
        }
        const uint64_t entry_size =
            entry.compressed_size > 0 ? entry.compressed_size : entry.original_size;
        expected_end = std::max(expected_end, entry.file_offset + entry_size);
    }

    if (::ftruncate(backup_fd, static_cast<off_t>(expected_end)) != 0)
    {
        close_fd();
        return fail_job(Status::IO_ERROR,
                        BackupJobState::FAILED,
                        "Failed to reconcile backup temp artifact size");
    }

    checkpoint->result.state = BackupJobState::WRITING;
    checkpoint->result.error_message.clear();
    checkpoint->result.last_updated_time = nowMicros();
    {
        ErrorContext checkpoint_ctx;
        status = saveBackupJobCheckpoint(*checkpoint, &checkpoint_ctx);
        if (status != Status::OK)
        {
            close_fd();
            if (ctx && ctx->message.empty())
            {
                ctx->message = checkpoint_ctx.message;
            }
            return status;
        }
    }
    {
        ErrorContext history_ctx;
        status = upsertBackupHistoryState(db,
                                          *checkpoint,
                                          CatalogManager::BackupHistoryStatus::RUNNING,
                                          "",
                                          &history_ctx);
        if (status != Status::OK)
        {
            close_fd();
            if (ctx && ctx->message.empty())
            {
                ctx->message = history_ctx.message;
            }
            return status;
        }
    }

    std::vector<uint8_t> page_buffer(checkpoint->page_size);
    std::vector<uint8_t> compress_buffer;
    uint64_t processed_this_invocation = 0;

    for (size_t i = static_cast<size_t>(checkpoint->result.pages_processed);
         i < checkpoint->pages_to_backup.size();
         ++i)
    {
        if (progress && progress->cancelled)
        {
            close_fd();
            return fail_job(Status::CANCELLED,
                            BackupJobState::CANCELLED,
                            "Backup cancelled");
        }

        const GPID gpid = checkpoint->pages_to_backup[i];
        status = db->read_page_global(gpid, page_buffer.data(), ctx);
        if (status != Status::OK)
        {
            close_fd();
            return fail_job(status,
                            BackupJobState::FAILED,
                            "");
        }

        const uint32_t checksum = calculateChecksumRaw(page_buffer.data(), checkpoint->page_size);
        uint32_t compressed_size = 0;
        const uint8_t* write_data = page_buffer.data();
        uint32_t write_size = checkpoint->page_size;

        if (checkpoint->config.compression != CompressionType::NONE)
        {
            status = compressPageRaw(page_buffer.data(),
                                     checkpoint->page_size,
                                     &compress_buffer,
                                     checkpoint->config.compression,
                                     checkpoint->config.compression_level,
                                     ctx);
            if (status != Status::OK)
            {
                close_fd();
                return fail_job(status, BackupJobState::FAILED, "");
            }

            if (compress_buffer.size() < checkpoint->page_size)
            {
                write_data = compress_buffer.data();
                write_size = static_cast<uint32_t>(compress_buffer.size());
                compressed_size = write_size;
            }
        }

        if (platform::seekFd(backup_fd, static_cast<off_t>(expected_end), SEEK_SET) !=
            static_cast<off_t>(expected_end))
        {
            close_fd();
            return fail_job(Status::IO_ERROR,
                            BackupJobState::FAILED,
                            "Failed to seek backup temp artifact");
        }

        const ssize_t written = ::write(backup_fd, write_data, write_size);
        if (written != static_cast<ssize_t>(write_size))
        {
            close_fd();
            return fail_job(Status::IO_ERROR,
                            BackupJobState::FAILED,
                            "Failed to write backup page data");
        }

        BackupPageEntry entry{};
        entry.gpid = gpid;
        entry.compressed_size = compressed_size;
        entry.original_size = checkpoint->page_size;
        entry.checksum = checksum;
        entry.file_offset = expected_end;
        checkpoint->page_entries[i] = entry;

        expected_end += write_size;
        checkpoint->result.pages_processed = i + 1;
        checkpoint->result.bytes_written += write_size;
        checkpoint->result.last_updated_time = nowMicros();
        checkpoint->result.error_message.clear();

        ErrorContext checkpoint_ctx;
        status = saveBackupJobCheckpoint(*checkpoint, &checkpoint_ctx);
        if (status != Status::OK)
        {
            close_fd();
            if (ctx && ctx->message.empty())
            {
                ctx->message = checkpoint_ctx.message;
            }
            return status;
        }

        if (progress)
        {
            progress->pages_processed = checkpoint->result.pages_processed;
            progress->bytes_written = checkpoint->result.bytes_written;
        }

        ++processed_this_invocation;
        if (execution_config.max_pages_per_invocation > 0 &&
            processed_this_invocation >= execution_config.max_pages_per_invocation &&
            checkpoint->result.pages_processed < checkpoint->result.pages_total)
        {
            checkpoint->result.state = BackupJobState::PAUSED;
            checkpoint->result.last_updated_time = nowMicros();

            ErrorContext pause_ctx;
            status = saveBackupJobCheckpoint(*checkpoint, &pause_ctx);
            if (status != Status::OK)
            {
                close_fd();
                if (ctx && ctx->message.empty())
                {
                    ctx->message = pause_ctx.message;
                }
                return status;
            }

            platform::syncFd(backup_fd);
            close_fd();
            return Status::OK;
        }
    }

    checkpoint->result.state = BackupJobState::FINALIZING;
    checkpoint->result.last_updated_time = nowMicros();
    {
        ErrorContext checkpoint_ctx;
        status = saveBackupJobCheckpoint(*checkpoint, &checkpoint_ctx);
        if (status != Status::OK)
        {
            close_fd();
            if (ctx && ctx->message.empty())
            {
                ctx->message = checkpoint_ctx.message;
            }
            return status;
        }
    }

    if (platform::seekFd(backup_fd, static_cast<off_t>(index_offset), SEEK_SET) !=
        static_cast<off_t>(index_offset))
    {
        close_fd();
        return fail_job(Status::IO_ERROR,
                        BackupJobState::FAILED,
                        "Failed to seek backup page index");
    }

    const size_t page_index_size =
        checkpoint->page_entries.size() * sizeof(BackupPageEntry);
    if (page_index_size > 0)
    {
        const ssize_t index_written = ::write(backup_fd,
                                              checkpoint->page_entries.data(),
                                              page_index_size);
        if (index_written != static_cast<ssize_t>(page_index_size))
        {
            close_fd();
            return fail_job(Status::IO_ERROR,
                            BackupJobState::FAILED,
                            "Failed to write backup page index");
        }
    }

    header.total_pages = checkpoint->page_entries.size();
    header.backup_end_time = nowMicros();
    auto* txn_mgr = db->transaction_manager();
    header.end_transaction_id = txn_mgr ? txn_mgr->getCurrentXid()
                                        : header.start_transaction_id;
    header.checksum = 0;
    header.checksum = calculateChecksumRaw(reinterpret_cast<const uint8_t*>(&header),
                                          sizeof(header));

    if (platform::seekFd(backup_fd, 0, SEEK_SET) != 0)
    {
        close_fd();
        return fail_job(Status::IO_ERROR,
                        BackupJobState::FAILED,
                        "Failed to rewind backup temp artifact");
    }

    status = writeBackupHeaderRaw(backup_fd, header, ctx);
    if (status != Status::OK)
    {
        close_fd();
        return fail_job(status, BackupJobState::FAILED, "");
    }

    platform::syncFd(backup_fd);
    close_fd();

    status = removePathIfExists(checkpoint->result.backup_path, ctx);
    if (status != Status::OK)
    {
        return fail_job(status, BackupJobState::FAILED, "");
    }

    std::error_code rename_ec;
    std::filesystem::rename(temp_path, checkpoint->result.backup_path, rename_ec);
    if (rename_ec)
    {
        return fail_job(Status::IO_ERROR,
                        BackupJobState::FAILED,
                        "Failed to publish completed backup artifact");
    }

    struct stat st{};
    if (stat(checkpoint->result.backup_path.c_str(), &st) != 0)
    {
        return fail_job(Status::IO_ERROR,
                        BackupJobState::FAILED,
                        "Failed to stat completed backup artifact");
    }

    BackupMetadata backup_metadata =
        populateBackupMetadataFromHeader(header,
                                         checkpoint->result.backup_path,
                                         static_cast<uint64_t>(st.st_size));
    backup_metadata.storage_profile = checkpoint->result.storage_profile;

    BackupCatalog backup_catalog(
        backupCatalogPathForDir(backupDirectoryFromPath(checkpoint->result.backup_path)));
    ErrorContext catalog_ctx;
    status = backup_catalog.load(&catalog_ctx);
    if (status != Status::OK)
    {
        if (ctx && ctx->message.empty())
        {
            ctx->message = catalog_ctx.message;
        }
        return fail_job(status, BackupJobState::FAILED, catalog_ctx.message);
    }
    status = backup_catalog.addBackup(backup_metadata, &catalog_ctx);
    if (status != Status::OK)
    {
        if (ctx && ctx->message.empty())
        {
            ctx->message = catalog_ctx.message;
        }
        return fail_job(status, BackupJobState::FAILED, catalog_ctx.message);
    }
    status = backup_catalog.save(&catalog_ctx);
    if (status != Status::OK)
    {
        if (ctx && ctx->message.empty())
        {
            ctx->message = catalog_ctx.message;
        }
        return fail_job(status, BackupJobState::FAILED, catalog_ctx.message);
    }

    if (checkpoint->result.type != BackupType::FULL)
    {
        manager->clearModifiedPages();
    }

    checkpoint->result.state = BackupJobState::COMPLETED;
    checkpoint->result.completed_time = nowMicros();
    checkpoint->result.last_updated_time = checkpoint->result.completed_time;
    checkpoint->result.bytes_written = backup_metadata.size_bytes;
    checkpoint->result.error_message.clear();

    {
        ErrorContext checkpoint_ctx;
        status = saveBackupJobCheckpoint(*checkpoint, &checkpoint_ctx);
        if (status != Status::OK)
        {
            if (ctx && ctx->message.empty())
            {
                ctx->message = checkpoint_ctx.message;
            }
            return status;
        }
    }
    {
        ErrorContext history_ctx;
        status = upsertBackupHistoryState(db,
                                          *checkpoint,
                                          CatalogManager::BackupHistoryStatus::SUCCESS,
                                          "",
                                          &history_ctx);
        if (status != Status::OK)
        {
            if (ctx && ctx->message.empty())
            {
                ctx->message = history_ctx.message;
            }
            return status;
        }
    }

    if (progress)
    {
        progress->pages_processed = checkpoint->result.pages_processed;
        progress->bytes_written = checkpoint->result.bytes_written;
        progress->completed = true;
    }

    return Status::OK;
}

// =================================================================================================
// BackupManager Implementation
// =================================================================================================

BackupManager::BackupManager(Database* db) : db_(db) {}

BackupManager::~BackupManager() = default;

Status BackupManager::createBackup(const std::string& backup_path,
                                   const BackupConfig& config,
                                   BackupProgress* progress,
                                   ErrorContext* ctx)
{
    BackupJobResult result;
    const Status status = executeBackupJob(backup_path,
                                           config,
                                           BackupExecutionConfig{},
                                           &result,
                                           progress,
                                           ctx);
    if (status != Status::OK)
    {
        return status;
    }
    if (result.state != BackupJobState::COMPLETED)
    {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR,
                          "Backup job did not reach a terminal completed state");
        return Status::INTERNAL_ERROR;
    }
    return Status::OK;
}

Status BackupManager::executeBackupJob(const std::string& backup_path,
                                       const BackupConfig& config,
                                       const BackupExecutionConfig& execution_config,
                                       BackupJobResult* result_out,
                                       BackupProgress* progress,
                                       ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(backup_mutex_);

    if (!db_ || !db_->is_open())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
        return Status::INVALID_ARGUMENT;
    }

    const std::string normalized_backup_path = normalizeBackupPath(backup_path);
    BackupJobCheckpoint checkpoint;
    ErrorContext checkpoint_ctx;
    Status status = loadBackupJobCheckpoint(normalized_backup_path, &checkpoint, &checkpoint_ctx);
    if (status == Status::OK)
    {
        if (!backupJobStateIsTerminal(checkpoint.result.state))
        {
            if (checkpoint.config.type != config.type ||
                checkpoint.config.compression != config.compression ||
                checkpoint.config.compression_level != config.compression_level ||
                checkpoint.config.label != config.label)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Backup job config does not match existing checkpoint");
                return Status::INVALID_ARGUMENT;
            }
            if (!config.storage_profile.empty() &&
                checkpoint.result.storage_profile != config.storage_profile)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Backup job storage profile does not match existing checkpoint");
                return Status::INVALID_ARGUMENT;
            }
            if (!isZeroUuid(config.parent_backup_id) &&
                checkpoint.result.parent_backup_id != config.parent_backup_id)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Backup job parent lineage does not match existing checkpoint");
                return Status::INVALID_ARGUMENT;
            }

            ++checkpoint.result.resume_count;
            status = runBackupJobExecution(this,
                                           db_,
                                           &checkpoint,
                                           execution_config,
                                           progress,
                                           ctx);
            if (result_out)
            {
                *result_out = checkpoint.result;
            }
            if (status == Status::OK &&
                checkpoint.result.state == BackupJobState::COMPLETED)
            {
                last_backup_id_ = checkpoint.result.backup_id;
                last_backup_time_ = checkpoint.result.completed_time;
            }
            return status;
        }

        status = removePathIfExists(backupJobStatePath(normalized_backup_path), ctx);
        if (status != Status::OK)
        {
            return status;
        }
        status = removePathIfExists(backupTempPath(normalized_backup_path), ctx);
        if (status != Status::OK)
        {
            return status;
        }
    }
    else if (status != Status::NOT_FOUND)
    {
        if (ctx && ctx->message.empty())
        {
            ctx->message = checkpoint_ctx.message;
        }
        return status;
    }

    const std::string backup_dir = backupDirectoryFromPath(normalized_backup_path);
    BackupCatalog backup_catalog(backupCatalogPathForDir(backup_dir));
    status = backup_catalog.load(ctx);
    if (status != Status::OK)
    {
        return status;
    }

    BackupPolicy active_policy;
    status = backup_catalog.getPolicy(&active_policy, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    checkpoint = BackupJobCheckpoint{};
    checkpoint.config = config;
    checkpoint.page_size = db_->page_size();
    checkpoint.result.backup_id = generateUuidV7();
    checkpoint.result.type = config.type;
    checkpoint.result.backup_path = normalized_backup_path;
    checkpoint.result.storage_profile =
        config.storage_profile.empty() ? active_policy.storage_profile : config.storage_profile;
    checkpoint.result.state = BackupJobState::PREPARING;
    checkpoint.result.started_time = nowMicros();
    checkpoint.result.last_updated_time = checkpoint.result.started_time;

    if (config.type != BackupType::FULL)
    {
        checkpoint.result.parent_backup_id = config.parent_backup_id;
        if (isZeroUuid(checkpoint.result.parent_backup_id) &&
            !config.parent_backup_path.empty())
        {
            BackupMetadata parent_metadata;
            status = getBackupMetadata(config.parent_backup_path, &parent_metadata, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            checkpoint.result.parent_backup_id = parent_metadata.backup_id;
        }

    if (isZeroUuid(checkpoint.result.parent_backup_id))
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          "Incremental and differential backups require resolvable parent metadata");
        return Status::INVALID_ARGUMENT;
    }
}

    if (auto* buffer_pool = db_ ? db_->buffer_pool() : nullptr; buffer_pool != nullptr)
    {
        status = buffer_pool->flushAll(ctx);
        if (status != Status::OK)
        {
            return status;
        }
    }

    std::vector<BackupTablespaceInfo> tablespace_manifest;
    status = buildTablespaceManifest(db_, &tablespace_manifest, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::unordered_set<GPID> modified_pages_snapshot;
    if (config.type != BackupType::FULL)
    {
        std::lock_guard<std::mutex> modified_lock(modified_pages_mutex_);
        modified_pages_snapshot = modified_pages_;
    }

    status = determinePagesToBackup(db_,
                                    config.type,
                                    tablespace_manifest,
                                    modified_pages_snapshot,
                                    &checkpoint.pages_to_backup,
                                    ctx);
    if (status != Status::OK)
    {
        return status;
    }

    checkpoint.page_entries.resize(checkpoint.pages_to_backup.size());
    checkpoint.result.pages_total = checkpoint.pages_to_backup.size();

    initializeBackupProgressFromResult(checkpoint.result, progress);

    const std::string temp_path = backupTempPath(normalized_backup_path);
    status = removePathIfExists(temp_path, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    int backup_fd = platform::openFd(temp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (backup_fd < 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to create backup job temp artifact");
        return Status::IO_ERROR;
    }

    auto cleanup_new_job = [&]() {
        if (backup_fd >= 0)
        {
            ::close(backup_fd);
            backup_fd = -1;
        }
        ErrorContext ignored_ctx;
        removePathIfExists(temp_path, &ignored_ctx);
    };

    BackupManifestHeader header{};
    std::memcpy(header.magic, BACKUP_MAGIC, sizeof(header.magic));
    header.version = BACKUP_VERSION;
    const auto db_uuid = db_->uuid();
    std::memcpy(header.db_uuid, db_uuid.bytes.data(), sizeof(header.db_uuid));
    header.type = config.type;
    header.compression = config.compression;
    header.compression_level = static_cast<uint8_t>(config.compression_level);
    header.page_size = checkpoint.page_size;
    header.total_pages = checkpoint.pages_to_backup.size();
    header.backup_start_time = checkpoint.result.started_time;
    auto* txn_mgr = db_->transaction_manager();
    header.start_transaction_id = txn_mgr ? txn_mgr->getCurrentXid() : 0;
    std::memcpy(header.parent_backup_uuid,
                checkpoint.result.parent_backup_id.bytes.data(),
                checkpoint.result.parent_backup_id.bytes.size());
    encodeBackupId(&header, checkpoint.result.backup_id);
    std::strncpy(header.label, config.label.c_str(), sizeof(header.label) - 1);
    header.rollback_checkpoint_version = BACKUP_ROLLBACK_CHECKPOINT_VERSION_CURRENT;
    header.source_db_version = db_->database_format_version();
    header.source_db_compat_version = db_->database_compat_version();
    header.rollback_flags = BACKUP_ROLLBACK_REQUIRED_FLAGS;
    header.checksum = 0;
    header.checksum = calculateChecksumRaw(reinterpret_cast<const uint8_t*>(&header),
                                          sizeof(header));

    status = writeBackupHeaderRaw(backup_fd, header, ctx);
    if (status != Status::OK)
    {
        cleanup_new_job();
        return status;
    }

    const bool include_file_ranges = (header.version >= 3);
    status = writeTablespaceManifest(backup_fd,
                                     tablespace_manifest,
                                     include_file_ranges,
                                     &header.tablespace_info_offset,
                                     &header.tablespace_info_size,
                                     ctx);
    if (status != Status::OK)
    {
        cleanup_new_job();
        return status;
    }

    BackupPageEntry empty_entry{};
    for (size_t i = 0; i < checkpoint.page_entries.size(); ++i)
    {
        const ssize_t reserved = ::write(backup_fd, &empty_entry, sizeof(empty_entry));
        if (reserved != static_cast<ssize_t>(sizeof(empty_entry)))
        {
            cleanup_new_job();
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to reserve backup page index");
            return Status::IO_ERROR;
        }
    }

    if (platform::seekFd(backup_fd, 0, SEEK_SET) != 0)
    {
        cleanup_new_job();
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to rewind new backup job artifact");
        return Status::IO_ERROR;
    }
    header.checksum = 0;
    header.checksum = calculateChecksumRaw(reinterpret_cast<const uint8_t*>(&header),
                                          sizeof(header));
    status = writeBackupHeaderRaw(backup_fd, header, ctx);
    if (status != Status::OK)
    {
        cleanup_new_job();
        return status;
    }

    platform::syncFd(backup_fd);
    ::close(backup_fd);
    backup_fd = -1;

    status = saveBackupJobCheckpoint(checkpoint, ctx);
    if (status != Status::OK)
    {
        cleanup_new_job();
        return status;
    }

    status = upsertBackupHistoryState(db_,
                                      checkpoint,
                                      CatalogManager::BackupHistoryStatus::STARTED,
                                      "",
                                      ctx);
    if (status != Status::OK)
    {
        if (result_out)
        {
            *result_out = checkpoint.result;
        }
        return status;
    }

    status = runBackupJobExecution(this,
                                   db_,
                                   &checkpoint,
                                   execution_config,
                                   progress,
                                   ctx);
    if (result_out)
    {
        *result_out = checkpoint.result;
    }
    if (status == Status::OK &&
        checkpoint.result.state == BackupJobState::COMPLETED)
    {
        last_backup_id_ = checkpoint.result.backup_id;
        last_backup_time_ = checkpoint.result.completed_time;
    }
    return status;
}

Status BackupManager::resumeBackupJob(const std::string& backup_path,
                                      const BackupExecutionConfig& execution_config,
                                      BackupJobResult* result_out,
                                      BackupProgress* progress,
                                      ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(backup_mutex_);

    if (!db_ || !db_->is_open())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
        return Status::INVALID_ARGUMENT;
    }

    BackupJobCheckpoint checkpoint;
    const std::string normalized_backup_path = normalizeBackupPath(backup_path);
    Status status = loadBackupJobCheckpoint(normalized_backup_path, &checkpoint, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    if (backupJobStateIsTerminal(checkpoint.result.state))
    {
        if (result_out)
        {
            *result_out = checkpoint.result;
        }
        if (progress)
        {
            initializeBackupProgressFromResult(checkpoint.result, progress);
            progress->completed = true;
            progress->cancelled = (checkpoint.result.state == BackupJobState::CANCELLED);
        }
        return Status::OK;
    }

    ++checkpoint.result.resume_count;
    status = runBackupJobExecution(this,
                                   db_,
                                   &checkpoint,
                                   execution_config,
                                   progress,
                                   ctx);
    if (result_out)
    {
        *result_out = checkpoint.result;
    }
    if (status == Status::OK &&
        checkpoint.result.state == BackupJobState::COMPLETED)
    {
        last_backup_id_ = checkpoint.result.backup_id;
        last_backup_time_ = checkpoint.result.completed_time;
    }
    return status;
}

Status BackupManager::getBackupJobResult(const std::string& backup_path,
                                         BackupJobResult* result_out,
                                         ErrorContext* ctx)
{
    if (!result_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null backup job result output");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(backup_mutex_);

    const std::string normalized_backup_path = normalizeBackupPath(backup_path);
    BackupJobCheckpoint checkpoint;
    Status status = loadBackupJobCheckpoint(normalized_backup_path, &checkpoint, ctx);
    if (status == Status::OK)
    {
        const std::string temp_path = backupTempPath(normalized_backup_path);
        const bool final_exists = std::filesystem::exists(normalized_backup_path);
        const bool temp_exists = std::filesystem::exists(temp_path);
        const bool completed_missing_final =
            checkpoint.result.state == BackupJobState::COMPLETED && !final_exists;
        const bool active_missing_temp =
            !backupJobStateIsTerminal(checkpoint.result.state) && !temp_exists;
        if (completed_missing_final || active_missing_temp)
        {
            checkpoint.result.state = BackupJobState::FAILED;
            checkpoint.result.error_message =
                completed_missing_final
                    ? "Completed backup job is missing its published backup artifact"
                    : "Backup job temp artifact missing during reconciliation";
            checkpoint.result.last_updated_time = nowMicros();
            checkpoint.result.completed_time = checkpoint.result.last_updated_time;
            ErrorContext checkpoint_ctx;
            saveBackupJobCheckpoint(checkpoint, &checkpoint_ctx);
            ErrorContext history_ctx;
            upsertBackupHistoryState(db_,
                                     checkpoint,
                                     CatalogManager::BackupHistoryStatus::FAILED,
                                     checkpoint.result.error_message,
                                     &history_ctx);
        }

        *result_out = checkpoint.result;
        return Status::OK;
    }
    if (status != Status::NOT_FOUND)
    {
        return status;
    }

    if (!std::filesystem::exists(normalized_backup_path))
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Backup job checkpoint not found");
        return Status::NOT_FOUND;
    }

    BackupMetadata metadata;
    status = getBackupMetadata(normalized_backup_path, &metadata, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    *result_out = BackupJobResult{};
    result_out->backup_id = metadata.backup_id;
    result_out->parent_backup_id = metadata.parent_id;
    result_out->type = metadata.type;
    result_out->backup_path = metadata.path;
    result_out->storage_profile = metadata.storage_profile;
    result_out->state = BackupJobState::COMPLETED;
    result_out->pages_total = metadata.total_pages;
    result_out->pages_processed = metadata.total_pages;
    result_out->bytes_written = metadata.size_bytes;
    result_out->started_time = metadata.start_time;
    result_out->last_updated_time = metadata.end_time;
    result_out->completed_time = metadata.end_time;
    return Status::OK;
}

Status BackupManager::runRestoreValidation(const std::string& backup_path,
                                           const std::string& target_path,
                                           const RestoreValidationConfig& config,
                                           RestoreValidationResult* result_out,
                                           BackupProgress* progress,
                                           ErrorContext* ctx)
{
    return runRestoreValidationInternal({backup_path},
                                        target_path,
                                        config,
                                        result_out,
                                        progress,
                                        ctx);
}

Status BackupManager::runDisasterRecoveryRehearsal(const std::vector<std::string>& backup_chain,
                                                   const std::string& target_path,
                                                   const RestoreValidationConfig& config,
                                                   RestoreValidationResult* result_out,
                                                   BackupProgress* progress,
                                                   ErrorContext* ctx)
{
    return runRestoreValidationInternal(backup_chain,
                                        target_path,
                                        config,
                                        result_out,
                                        progress,
                                        ctx);
}

Status BackupManager::runRestoreValidationInternal(const std::vector<std::string>& backup_chain,
                                                   const std::string& target_path,
                                                   const RestoreValidationConfig& config,
                                                   RestoreValidationResult* result_out,
                                                   BackupProgress* progress,
                                                   ErrorContext* ctx)
{
    if (!result_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null restore validation output");
        return Status::INVALID_ARGUMENT;
    }

    RestoreValidationResult result{};
    result.target_path = normalizeBackupPath(target_path);
    result.restore_threshold_micros = config.max_restore_micros;
    result.rpo_threshold_micros = config.max_rpo_micros;

    const auto storeResult = [&](Status status_value) -> Status {
        *result_out = result;
        return status_value;
    };
    const auto contextMessage = [&](const std::string& fallback) -> std::string {
        if (ctx && !ctx->message.empty())
        {
            return ctx->message;
        }
        return fallback;
    };

    if (backup_chain.empty())
    {
        const std::string message = "Restore validation requires at least one backup";
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, message.c_str());
        result.failure_reason = message;
        return storeResult(Status::INVALID_ARGUMENT);
    }
    if (result.target_path.empty())
    {
        const std::string message = "Restore validation target path is required";
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, message.c_str());
        result.failure_reason = message;
        return storeResult(Status::INVALID_ARGUMENT);
    }

    if (!config.restore_config.target_lsn.empty())
    {
        const std::string message =
            "LSN-based PITR replay is not implemented; MGA snapshot restore remains authoritative";
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, message.c_str());
        result.failure_reason = message;
        return storeResult(Status::NOT_IMPLEMENTED);
    }

    AppliedBackupSelection selection;
    Status status = resolveAppliedBackupSelection(this, backup_chain, config.target_time, &selection, ctx);
    if (status != Status::OK)
    {
        result.failure_reason = contextMessage("Failed to resolve restore rehearsal chain");
        return storeResult(status);
    }
    status = enforcePITRTargetPolicy(this, selection, config.target_time, ctx);
    if (status != Status::OK)
    {
        result.failure_reason = contextMessage("PITR policy rejected the requested restore target");
        return storeResult(status);
    }

    RollbackCheckpointAssessment rollback_assessment;
    status = validateRollbackCheckpointChain(selection.applied_metadata, &rollback_assessment, ctx);
    if (status != Status::OK)
    {
        result.failure_reason = contextMessage("Rollback checkpoint validation failed");
        return storeResult(status);
    }

    result.backup_chain.reserve(selection.chain_metadata.size());
    for (const auto& metadata : selection.chain_metadata)
    {
        result.backup_chain.push_back(metadata.path);
    }
    const auto& applied_metadata = selection.applied_metadata;
    result.rollback_checkpoint_version = rollback_assessment.marker_version;
    result.chain_source_db_version = rollback_assessment.source_db_version;
    result.chain_source_db_compat_version = rollback_assessment.source_db_compat_version;
    result.rollback_checkpoint_validated = rollback_assessment.validated;

    result.applied_backup_count = static_cast<uint32_t>(applied_metadata.size());
    result.recovered_until_time = applied_metadata.back().end_time;

    int backup_fd = platform::openFd(applied_metadata.front().path.c_str(), O_RDONLY);
    if (backup_fd < 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open base backup for restore validation");
        result.failure_reason = contextMessage("Failed to open base backup for restore validation");
        return storeResult(Status::IO_ERROR);
    }

    BackupManifestHeader base_header{};
    status = readBackupHeader(backup_fd, &base_header, ctx);
    ::close(backup_fd);
    if (status != Status::OK)
    {
        result.failure_reason = contextMessage("Failed to read base backup header");
        return storeResult(status);
    }

    result.source_page_size = base_header.page_size;
    std::memcpy(result.source_database_id.bytes.data(),
                base_header.db_uuid,
                result.source_database_id.bytes.size());

    if (config.require_backup_verification)
    {
        for (const auto& metadata : applied_metadata)
        {
            status = verifyBackup(metadata.path, nullptr, ctx);
            if (status != Status::OK)
            {
                result.failure_reason = contextMessage("Backup verification failed");
                return storeResult(status);
            }
            ++result.verified_backup_count;
        }
        result.backup_verified = true;
    }

    result.restore_started_time = nowMicros();
    if (result.restore_started_time > result.recovered_until_time)
    {
        result.measured_rpo_micros =
            result.restore_started_time - result.recovered_until_time;
    }

    if (applied_metadata.size() == 1)
    {
        status = restoreBackup(applied_metadata.front().path,
                               result.target_path,
                               config.restore_config,
                               progress,
                               ctx);
    }
    else
    {
        std::vector<std::string> applied_paths;
        applied_paths.reserve(applied_metadata.size());
        for (const auto& metadata : applied_metadata)
        {
            applied_paths.push_back(metadata.path);
        }
        status = restoreToPointInTime(applied_paths,
                                      result.target_path,
                                      0,
                                      progress,
                                      ctx);
    }

    result.restore_completed_time = nowMicros();
    if (result.restore_completed_time >= result.restore_started_time)
    {
        result.restore_elapsed_micros =
            result.restore_completed_time - result.restore_started_time;
    }

    if (status != Status::OK)
    {
        result.failure_reason = contextMessage("Restore rehearsal failed");
        return storeResult(status);
    }
    result.restore_completed = true;

    if (config.require_reopen_validation)
    {
        // Validation should inspect the restored file without triggering catalog backfills.
        RestoredDatabaseSnapshot restored_snapshot;
        status = inspectRestoredDatabase(result.target_path, &restored_snapshot, ctx);
        if (status != Status::OK)
        {
            result.failure_reason = contextMessage("Failed to validate restored database header");
            return storeResult(status);
        }

        result.restored_page_size = restored_snapshot.page_size;
        result.restored_total_pages = restored_snapshot.total_pages;
        result.restored_database_id = restored_snapshot.database_id;

        if (result.source_page_size != 0 &&
            result.restored_page_size != result.source_page_size)
        {
            const std::string message = "Restored database page size mismatch";
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, message.c_str());
            result.failure_reason = contextMessage(message);
            return storeResult(Status::DATA_CORRUPTED);
        }
        if (!isZeroUuid(result.source_database_id) &&
            result.restored_database_id != result.source_database_id)
        {
            const std::string message = "Restored database UUID mismatch";
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, message.c_str());
            result.failure_reason = contextMessage(message);
            return storeResult(Status::DATA_CORRUPTED);
        }

        result.reopen_validated = true;
    }

    std::string threshold_failure;
    const auto appendThresholdFailure = [&](const std::string& message) {
        if (!threshold_failure.empty())
        {
            threshold_failure += "; ";
        }
        threshold_failure += message;
    };

    if (config.max_rpo_micros > 0 &&
        result.measured_rpo_micros > config.max_rpo_micros)
    {
        appendThresholdFailure("Measured RPO exceeded configured threshold");
    }
    if (config.max_restore_micros > 0 &&
        result.restore_elapsed_micros > config.max_restore_micros)
    {
        appendThresholdFailure("Measured restore time exceeded configured threshold");
    }

    if (!threshold_failure.empty())
    {
        result.failure_reason = threshold_failure;
        SET_ERROR_CONTEXT(ctx, Status::CONFIGURATION_LIMIT_EXCEEDED, threshold_failure.c_str());
        return storeResult(Status::CONFIGURATION_LIMIT_EXCEEDED);
    }

    result.thresholds_passed = true;
    result.failure_reason.clear();
    return storeResult(Status::OK);
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
    config.parent_backup_id = parent_meta.backup_id;
    config.parent_backup_path = normalizeBackupPath(parent_backup_path);
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
    if (!config.target_lsn.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                          "LSN-based PITR replay is not implemented; MGA snapshot restore remains authoritative");
        return Status::NOT_IMPLEMENTED;
    }
    if (config.target_time != 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                          "Timestamped PITR requires restoreToPointInTime and an enabled PITR extension policy");
        return Status::NOT_IMPLEMENTED;
    }

    // Open backup file
    int backup_fd = platform::openFd(backup_path.c_str(), O_RDONLY);
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

    BackupMetadata backup_metadata =
        populateBackupMetadataFromHeader(header, normalizeBackupPath(backup_path), 0);
    status = validateRollbackCheckpointMetadata(backup_metadata, false, nullptr, ctx);
    if (status != Status::OK)
    {
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
    const int target_flags = config.overlay_into_existing_target
                                 ? (O_RDWR | O_CREAT)
                                 : (O_WRONLY | O_CREAT | O_TRUNC);
    int target_fd = platform::openFd(target_path.c_str(), target_flags, 0644);
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
    std::vector<TablespaceFileRange> primary_ranges;
    bool primary_ranges_from_manifest = false;
    for (const auto& tablespace : tablespaces)
    {
        if (tablespace.tablespace_id != PRIMARY_TABLESPACE_ID)
        {
            continue;
        }

        uint64_t next_start = 0;
        for (size_t i = 0; i < tablespace.file_paths.size(); ++i)
        {
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

            if (page_count == 0 && i == 0)
            {
                page_count = tablespace.total_pages;
            }

            primary_ranges.push_back(TablespaceFileRange{start_page, page_count, target_fd});
            next_start = start_page + page_count;
        }

        primary_ranges_from_manifest = !primary_ranges.empty();
        break;
    }

    if (!primary_ranges_from_manifest)
    {
        primary_ranges.push_back(TablespaceFileRange{0, header.total_pages, target_fd});
    }
    tablespace_files[PRIMARY_TABLESPACE_ID] = std::move(primary_ranges);

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

            int ts_fd = platform::openFd(path.c_str(), O_RDWR | O_CREAT, 0644);
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

            if (page_count > 0 && !config.overlay_into_existing_target)
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
    platform::seekFd(backup_fd, static_cast<off_t>(index_offset), SEEK_SET);
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
        platform::seekFd(backup_fd, static_cast<off_t>(entry.file_offset), SEEK_SET);

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
        platform::seekFd(fd, offset, SEEK_SET);
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
                platform::syncFd(range.fd);
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

    RestoredDatabaseSnapshot restored_snapshot;
    status = inspectRestoredDatabase(target_path, &restored_snapshot, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    return Status::OK;
}

Status BackupManager::restoreToPointInTime(const std::vector<std::string>& backup_chain,
                                            const std::string& target_path,
                                            uint64_t target_time,
                                            BackupProgress* progress,
                                            ErrorContext* ctx)
{
    AppliedBackupSelection selection;
    auto status = resolveAppliedBackupSelection(this, backup_chain, target_time, &selection, ctx);
    if (status != Status::OK)
    {
        return status;
    }
    status = enforcePITRTargetPolicy(this, selection, target_time, ctx);
    if (status != Status::OK)
    {
        return status;
    }
    status = validateRollbackCheckpointChain(selection.applied_metadata, nullptr, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    RestoreConfig config;
    config.verify_checksums = true;
    status = restoreBackup(selection.applied_metadata.front().path, target_path, config, progress, ctx);
    if (status != Status::OK) {
        return status;
    }

    for (size_t i = 1; i < selection.applied_metadata.size(); ++i) {
        config.overlay_into_existing_target = true;
        status = restoreBackup(selection.applied_metadata[i].path, target_path, config, progress, ctx);
        if (status != Status::OK) {
            return status;
        }
    }

    return Status::OK;
}

Status BackupManager::verifyBackup(const std::string& backup_path,
                                    BackupProgress* progress,
                                    ErrorContext* ctx)
{
    int backup_fd = platform::openFd(backup_path.c_str(), O_RDONLY);
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

    BackupMetadata backup_metadata =
        populateBackupMetadataFromHeader(header, normalizeBackupPath(backup_path), 0);
    status = validateRollbackCheckpointMetadata(backup_metadata, false, nullptr, ctx);
    if (status != Status::OK)
    {
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
    platform::seekFd(backup_fd, static_cast<off_t>(index_offset), SEEK_SET);
    const size_t page_index_size = header.total_pages * sizeof(BackupPageEntry);
    const ssize_t index_read = ::read(backup_fd, page_entries.data(), page_index_size);
    if (index_read != static_cast<ssize_t>(page_index_size))
    {
        ::close(backup_fd);
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read backup page index");
        return Status::IO_ERROR;
    }

    // Verify each page
    std::vector<uint8_t> read_buffer(header.page_size * 2);
    std::vector<uint8_t> decompress_buffer(header.page_size);
    uint64_t errors = 0;

    for (const auto& entry : page_entries) {
        platform::seekFd(backup_fd, static_cast<off_t>(entry.file_offset), SEEK_SET);

        uint32_t read_size = entry.compressed_size > 0 ? entry.compressed_size : entry.original_size;
        const ssize_t bytes_read = ::read(backup_fd, read_buffer.data(), read_size);
        if (bytes_read != static_cast<ssize_t>(read_size))
        {
            ++errors;
            continue;
        }

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

    const std::string normalized_backup_path = normalizeBackupPath(backup_path);
    int backup_fd = platform::openFd(normalized_backup_path.c_str(), O_RDONLY);
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

    struct stat st{};
    stat(normalized_backup_path.c_str(), &st);

    BackupMetadata metadata =
        populateBackupMetadataFromHeader(header,
                                         normalized_backup_path,
                                         static_cast<uint64_t>(st.st_size));
    status = validateRollbackCheckpointMetadata(metadata, false, nullptr, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    BackupCatalog backup_catalog(backupCatalogPathForDir(backupDirectoryFromPath(normalized_backup_path)));
    ErrorContext catalog_ctx;
    if (backup_catalog.load(&catalog_ctx) == Status::OK)
    {
        BackupMetadata catalog_metadata;
        if (backup_catalog.getBackupByPath(normalized_backup_path, &catalog_metadata, &catalog_ctx) ==
            Status::OK)
        {
            if (isZeroUuid(metadata.backup_id))
            {
                metadata.backup_id = catalog_metadata.backup_id;
            }
            if (isZeroUuid(metadata.parent_id))
            {
                metadata.parent_id = catalog_metadata.parent_id;
            }
            if (!catalog_metadata.storage_profile.empty())
            {
                metadata.storage_profile = catalog_metadata.storage_profile;
            }
            metadata.valid = metadata.valid && catalog_metadata.valid;
        }
    }

    *metadata_out = std::move(metadata);
    return Status::OK;
}

Status BackupManager::listBackups(const std::string& backup_dir,
                                   std::vector<BackupMetadata>* backups_out,
                                   ErrorContext* ctx)
{
    if (!backups_out) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null backups output");
        return Status::INVALID_ARGUMENT;
    }

    const std::string normalized_backup_dir = normalizeBackupPath(backup_dir);
    backups_out->clear();

    BackupCatalog backup_catalog(backupCatalogPathForDir(normalized_backup_dir));
    ErrorContext catalog_ctx;
    auto status = backup_catalog.load(&catalog_ctx);
    if (status == Status::OK)
    {
        status = backup_catalog.listBackups(backups_out, &catalog_ctx);
        if (status == Status::OK && !backups_out->empty())
        {
            for (auto& metadata : *backups_out)
            {
                BackupMetadata hydrated_metadata;
                ErrorContext metadata_ctx;
                if (getBackupMetadata(metadata.path, &hydrated_metadata, &metadata_ctx) == Status::OK)
                {
                    metadata = std::move(hydrated_metadata);
                }
            }
            std::sort(backups_out->begin(), backups_out->end(),
                      [](const BackupMetadata& lhs, const BackupMetadata& rhs) {
                          if (lhs.end_time != rhs.end_time)
                          {
                              return lhs.end_time > rhs.end_time;
                          }
                          return lhs.backup_id < rhs.backup_id;
                      });
            return Status::OK;
        }
    }

    std::error_code ec;
    if (!std::filesystem::exists(normalized_backup_dir, ec) ||
        !std::filesystem::is_directory(normalized_backup_dir, ec))
    {
        SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND, "Backup directory not found");
        return Status::FILE_NOT_FOUND;
    }

    for (const auto& entry : std::filesystem::directory_iterator(normalized_backup_dir, ec))
    {
        if (ec)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to enumerate backup directory");
            return Status::IO_ERROR;
        }
        if (!entry.is_regular_file())
        {
            continue;
        }
        if (entry.path().extension() != ".sbkp")
        {
            continue;
        }

        BackupMetadata metadata;
        ErrorContext metadata_ctx;
        if (getBackupMetadata(entry.path().string(), &metadata, &metadata_ctx) == Status::OK)
        {
            backups_out->push_back(std::move(metadata));
        }
    }

    std::sort(backups_out->begin(), backups_out->end(),
              [](const BackupMetadata& lhs, const BackupMetadata& rhs) {
                  if (lhs.end_time != rhs.end_time)
                  {
                      return lhs.end_time > rhs.end_time;
                  }
                  return lhs.backup_id < rhs.backup_id;
              });
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
    BackupMetadata current;
    auto status = getBackupMetadata(backup_path, &current, ctx);
    if (status != Status::OK)
    {
        return status;
    }
    if (isZeroUuid(current.backup_id))
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED,
                          "Backup does not include resolvable lineage metadata");
        return Status::NOT_SUPPORTED;
    }

    const std::string normalized_backup_dir = backupDirectoryFromPath(current.path);
    BackupCatalog backup_catalog(backupCatalogPathForDir(normalized_backup_dir));
    status = backup_catalog.load(ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::unordered_set<UuidV7Bytes, IDHash> visited;
    while (true)
    {
        if (!visited.insert(current.backup_id).second)
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Backup chain contains a cycle");
            return Status::DATA_CORRUPTED;
        }

        chain_out->push_back(current.path);
        if (current.type == BackupType::FULL)
        {
            break;
        }
        if (isZeroUuid(current.parent_id))
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Backup chain parent metadata missing");
            return Status::DATA_CORRUPTED;
        }

        BackupMetadata parent;
        status = backup_catalog.getBackup(current.parent_id, &parent, ctx);
        if (status != Status::OK)
        {
            std::vector<BackupMetadata> backups;
            status = listBackups(normalized_backup_dir, &backups, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            auto parent_it = std::find_if(backups.begin(), backups.end(),
                                          [&](const BackupMetadata& metadata) {
                                              return metadata.backup_id == current.parent_id;
                                          });
            if (parent_it == backups.end())
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                                  "Parent backup not found in catalog or backup directory");
                return Status::NOT_FOUND;
            }
            parent = *parent_it;
        }

        current = std::move(parent);
    }

    std::reverse(chain_out->begin(), chain_out->end());
    AppliedBackupSelection selection;
    status = resolveAppliedBackupSelection(this, *chain_out, 0, &selection, ctx);
    if (status != Status::OK)
    {
        return status;
    }
    return validateRollbackCheckpointChain(selection.applied_metadata, nullptr, ctx);
}

Status BackupManager::setBackupPolicy(const std::string& backup_dir,
                                      const BackupPolicy& policy,
                                      ErrorContext* ctx)
{
    BackupCatalog backup_catalog(backupCatalogPathForDir(normalizeBackupPath(backup_dir)));
    auto status = backup_catalog.load(ctx);
    if (status != Status::OK)
    {
        return status;
    }
    status = backup_catalog.setPolicy(policy, ctx);
    if (status != Status::OK)
    {
        return status;
    }
    return backup_catalog.save(ctx);
}

Status BackupManager::getBackupPolicy(const std::string& backup_dir,
                                      BackupPolicy* policy_out,
                                      ErrorContext* ctx)
{
    if (!policy_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null policy output");
        return Status::INVALID_ARGUMENT;
    }

    BackupCatalog backup_catalog(backupCatalogPathForDir(normalizeBackupPath(backup_dir)));
    auto status = backup_catalog.load(ctx);
    if (status != Status::OK)
    {
        return status;
    }
    return backup_catalog.getPolicy(policy_out, ctx);
}

Status BackupManager::evaluateRetentionPolicy(const std::string& backup_dir,
                                              std::vector<BackupRetentionDecision>* decisions_out,
                                              ErrorContext* ctx)
{
    if (!decisions_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null retention decisions output");
        return Status::INVALID_ARGUMENT;
    }

    BackupCatalog backup_catalog(backupCatalogPathForDir(normalizeBackupPath(backup_dir)));
    auto status = backup_catalog.load(ctx);
    if (status != Status::OK)
    {
        return status;
    }
    return backup_catalog.evaluateRetention(decisions_out, ctx);
}

Status BackupManager::setPITRPolicy(const std::string& backup_dir,
                                    const PITRPolicy& policy,
                                    ErrorContext* ctx)
{
    BackupCatalog backup_catalog(backupCatalogPathForDir(normalizeBackupPath(backup_dir)));
    auto status = backup_catalog.load(ctx);
    if (status != Status::OK)
    {
        return status;
    }
    status = backup_catalog.setPITRPolicy(policy, ctx);
    if (status != Status::OK)
    {
        return status;
    }
    return backup_catalog.save(ctx);
}

Status BackupManager::getPITRPolicy(const std::string& backup_dir,
                                    PITRPolicy* policy_out,
                                    ErrorContext* ctx)
{
    if (!policy_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null PITR policy output");
        return Status::INVALID_ARGUMENT;
    }

    BackupCatalog backup_catalog(backupCatalogPathForDir(normalizeBackupPath(backup_dir)));
    auto status = backup_catalog.load(ctx);
    if (status != Status::OK)
    {
        return status;
    }
    return backup_catalog.getPITRPolicy(policy_out, ctx);
}

Status BackupManager::evaluatePITRRetention(const std::string& backup_dir,
                                            std::vector<PITRRetentionDecision>* decisions_out,
                                            ErrorContext* ctx)
{
    if (!decisions_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null PITR retention decisions output");
        return Status::INVALID_ARGUMENT;
    }

    BackupCatalog backup_catalog(backupCatalogPathForDir(normalizeBackupPath(backup_dir)));
    auto status = backup_catalog.load(ctx);
    if (status != Status::OK)
    {
        return status;
    }
    return backup_catalog.evaluatePITRRetention(decisions_out, ctx);
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

    return validateBackupHeaderChecksumRaw(*header, ctx);
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
    : catalog_path_(normalizeBackupPath(catalog_path)),
      policy_(defaultBackupPolicy()),
      pitr_policy_(defaultPITRPolicy())
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
    BackupMetadata normalized = metadata;
    normalized.path = normalizeBackupPath(normalized.path);
    if (normalized.storage_profile.empty())
    {
        normalized.storage_profile = policy_.storage_profile;
    }

    auto it = std::find_if(backups_.begin(), backups_.end(),
                           [&](const BackupMetadata& existing) {
                               if (!isZeroUuid(normalized.backup_id) &&
                                   existing.backup_id == normalized.backup_id)
                               {
                                   return true;
                               }
                               return normalizeBackupPath(existing.path) == normalized.path;
                           });
    if (it != backups_.end())
    {
        *it = normalized;
    }
    else
    {
        backups_.push_back(std::move(normalized));
    }
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

Status BackupCatalog::getBackupByPath(const std::string& backup_path,
                                      BackupMetadata* metadata_out,
                                      ErrorContext* ctx)
{
    if (!metadata_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null metadata output");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const std::string normalized_backup_path = normalizeBackupPath(backup_path);
    auto it = std::find_if(backups_.begin(), backups_.end(),
                           [&](const BackupMetadata& metadata) {
                               return normalizeBackupPath(metadata.path) == normalized_backup_path;
                           });
    if (it == backups_.end())
    {
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
    if (!chain_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null chain output");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    chain_out->clear();

    auto current_it = std::find_if(backups_.begin(), backups_.end(),
                                   [&](const BackupMetadata& backup) {
                                       return backup.backup_id == backup_id;
                                   });
    if (current_it == backups_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Backup not found");
        return Status::NOT_FOUND;
    }

    std::vector<BackupMetadata> chain;
    std::unordered_set<UuidV7Bytes, IDHash> visited;
    const BackupMetadata* current = &(*current_it);
    while (current) {
        if (!visited.insert(current->backup_id).second)
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Backup chain contains a cycle");
            return Status::DATA_CORRUPTED;
        }

        chain.push_back(*current);
        if (current->type == BackupType::FULL) {
            break;
        }
        if (isZeroUuid(current->parent_id))
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Backup chain parent metadata missing");
            return Status::DATA_CORRUPTED;
        }

        const BackupMetadata* parent = nullptr;
        for (const auto& backup : backups_) {
            if (backup.backup_id == current->parent_id) {
                parent = &backup;
                break;
            }
        }
        if (!parent)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Parent backup not found");
            return Status::NOT_FOUND;
        }
        current = parent;
    }

    std::reverse(chain.begin(), chain.end());
    *chain_out = std::move(chain);

    return Status::OK;
}

Status BackupCatalog::setPolicy(const BackupPolicy& policy, ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (policy.schema_version != 1)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unsupported backup policy schema version");
        return Status::INVALID_ARGUMENT;
    }
    if (policy.policy_name.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Backup policy name is required");
        return Status::INVALID_ARGUMENT;
    }
    if (policy.storage_profile.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Backup policy storage_profile is required");
        return Status::INVALID_ARGUMENT;
    }

    bool seen_full_rule = false;
    std::unordered_set<int> seen_types;
    for (const auto& rule : policy.retention_rules)
    {
        const int type_key = static_cast<int>(rule.type);
        if (!seen_types.insert(type_key).second)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Duplicate backup retention rule type");
            return Status::INVALID_ARGUMENT;
        }
        if (rule.type == BackupType::FULL && rule.retain_count == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Full backup retention rule must retain at least one backup");
            return Status::INVALID_ARGUMENT;
        }
        if (rule.type == BackupType::FULL)
        {
            seen_full_rule = true;
        }
    }

    if (!seen_full_rule)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                          "Backup policy must define a full-backup retention rule");
        return Status::INVALID_ARGUMENT;
    }

    policy_ = policy;
    has_policy_ = true;
    modified_ = true;
    return Status::OK;
}

Status BackupCatalog::getPolicy(BackupPolicy* policy_out, ErrorContext* ctx)
{
    if (!policy_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null policy output");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!has_policy_)
    {
        policy_ = defaultBackupPolicy();
    }
    *policy_out = policy_;
    return Status::OK;
}

Status BackupCatalog::setPITRPolicy(const PITRPolicy& policy, ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (policy.schema_version != 1)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unsupported PITR policy schema version");
        return Status::INVALID_ARGUMENT;
    }
    if (!isSupportedPITRMode(policy.mode))
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unsupported PITR policy mode");
        return Status::INVALID_ARGUMENT;
    }

    pitr_policy_ = policy;
    modified_ = true;
    return Status::OK;
}

Status BackupCatalog::getPITRPolicy(PITRPolicy* policy_out, ErrorContext* ctx)
{
    if (!policy_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null PITR policy output");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    *policy_out = pitr_policy_;
    return Status::OK;
}

Status BackupCatalog::evaluateRetention(std::vector<BackupRetentionDecision>* decisions_out,
                                        ErrorContext* ctx)
{
    if (!decisions_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null retention decisions output");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    decisions_out->clear();

    const BackupPolicy active_policy = has_policy_ ? policy_ : defaultBackupPolicy();
    std::vector<BackupMetadata> ordered = backups_;
    std::sort(ordered.begin(), ordered.end(),
              [](const BackupMetadata& lhs, const BackupMetadata& rhs) {
                  if (lhs.end_time != rhs.end_time)
                  {
                      return lhs.end_time > rhs.end_time;
                  }
                  return lhs.backup_id < rhs.backup_id;
              });

    if (!active_policy.enabled)
    {
        for (const auto& backup : ordered)
        {
            decisions_out->push_back(
                BackupRetentionDecision{backup.backup_id, backup.path, true, "policy_disabled"});
        }
        return Status::OK;
    }

    std::unordered_map<UuidV7Bytes, BackupMetadata, IDHash> backups_by_id;
    for (const auto& backup : ordered)
    {
        if (!isZeroUuid(backup.backup_id))
        {
            backups_by_id[backup.backup_id] = backup;
        }
    }

    auto within_age = [&](const BackupMetadata& backup, const BackupRetentionRule* rule) {
        if (!rule || rule->max_age_micros == 0 || backup.end_time == 0)
        {
            return true;
        }
        const uint64_t now = nowMicros();
        if (now <= backup.end_time)
        {
            return true;
        }
        return (now - backup.end_time) <= rule->max_age_micros;
    };

    auto parent_chain_available = [&](const BackupMetadata& backup) {
        if (backup.type == BackupType::FULL)
        {
            return true;
        }

        std::unordered_set<UuidV7Bytes, IDHash> seen;
        BackupMetadata current = backup;
        while (current.type != BackupType::FULL)
        {
            if (isZeroUuid(current.parent_id))
            {
                return false;
            }
            if (!seen.insert(current.backup_id).second)
            {
                return false;
            }
            auto parent_it = backups_by_id.find(current.parent_id);
            if (parent_it == backups_by_id.end())
            {
                return false;
            }
            current = parent_it->second;
        }
        return true;
    };

    std::unordered_set<UuidV7Bytes, IDHash> selected;
    std::unordered_set<UuidV7Bytes, IDHash> retained;
    std::unordered_set<UuidV7Bytes, IDHash> orphaned;
    std::array<uint32_t, 3> retained_by_type{};

    for (const auto& backup : ordered)
    {
        if (!backup.valid)
        {
            continue;
        }

        const auto* rule = lookupRetentionRule(active_policy, backup.type);
        if (!rule || rule->retain_count == 0)
        {
            continue;
        }

        const size_t type_index = static_cast<size_t>(backup.type);
        if (type_index >= retained_by_type.size())
        {
            continue;
        }
        if (retained_by_type[type_index] >= rule->retain_count)
        {
            continue;
        }
        if (!within_age(backup, rule))
        {
            continue;
        }
        if (rule->require_parent_chain && !parent_chain_available(backup))
        {
            orphaned.insert(backup.backup_id);
            continue;
        }

        selected.insert(backup.backup_id);
        retained.insert(backup.backup_id);
        retained_by_type[type_index]++;
    }

    for (const auto& selected_id : selected)
    {
        auto current_it = backups_by_id.find(selected_id);
        if (current_it == backups_by_id.end())
        {
            continue;
        }

        BackupMetadata current = current_it->second;
        std::unordered_set<UuidV7Bytes, IDHash> seen;
        while (current.type != BackupType::FULL && !isZeroUuid(current.parent_id))
        {
            if (!seen.insert(current.backup_id).second)
            {
                break;
            }
            auto parent_it = backups_by_id.find(current.parent_id);
            if (parent_it == backups_by_id.end())
            {
                break;
            }
            retained.insert(parent_it->second.backup_id);
            current = parent_it->second;
        }
    }

    for (const auto& backup : ordered)
    {
        BackupRetentionDecision decision;
        decision.backup_id = backup.backup_id;
        decision.backup_path = backup.path;
        if (!backup.valid)
        {
            decision.retain = false;
            decision.reason = "invalid_backup";
        }
        else if (retained.count(backup.backup_id) > 0)
        {
            decision.retain = true;
            decision.reason =
                (selected.count(backup.backup_id) > 0) ? "retained_by_policy" : "retained_for_chain";
        }
        else if (orphaned.count(backup.backup_id) > 0)
        {
            decision.retain = false;
            decision.reason = "missing_parent_chain";
        }
        else
        {
            const auto* rule = lookupRetentionRule(active_policy, backup.type);
            if (!rule || rule->retain_count == 0)
            {
                decision.retain = false;
                decision.reason = "policy_rule_disabled";
            }
            else if (!within_age(backup, rule))
            {
                decision.retain = false;
                decision.reason = "exceeds_max_age";
            }
            else
            {
                decision.retain = false;
                decision.reason = "retention_limit_exceeded";
            }
        }
        decisions_out->push_back(std::move(decision));
    }

    return Status::OK;
}

Status BackupCatalog::evaluatePITRRetention(std::vector<PITRRetentionDecision>* decisions_out,
                                            ErrorContext* ctx)
{
    if (!decisions_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null PITR retention decisions output");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    decisions_out->clear();

    std::vector<BackupMetadata> ordered = backups_;
    std::sort(ordered.begin(), ordered.end(),
              [](const BackupMetadata& lhs, const BackupMetadata& rhs) {
                  if (lhs.end_time != rhs.end_time)
                  {
                      return lhs.end_time > rhs.end_time;
                  }
                  return lhs.backup_id < rhs.backup_id;
              });

    std::unordered_map<UuidV7Bytes, BackupMetadata, IDHash> backups_by_id;
    for (const auto& backup : ordered)
    {
        if (!isZeroUuid(backup.backup_id))
        {
            backups_by_id[backup.backup_id] = backup;
        }
    }

    auto parent_chain_available = [&](const BackupMetadata& backup) {
        if (backup.type == BackupType::FULL)
        {
            return true;
        }

        std::unordered_set<UuidV7Bytes, IDHash> seen;
        BackupMetadata current = backup;
        while (current.type != BackupType::FULL)
        {
            if (isZeroUuid(current.parent_id))
            {
                return false;
            }
            if (!seen.insert(current.backup_id).second)
            {
                return false;
            }
            auto parent_it = backups_by_id.find(current.parent_id);
            if (parent_it == backups_by_id.end())
            {
                return false;
            }
            current = parent_it->second;
        }
        return true;
    };

    const uint64_t latest_available_time =
        ordered.empty() ? 0 : ordered.front().end_time;

    for (const auto& backup : ordered)
    {
        PITRRetentionDecision decision;
        decision.backup_id = backup.backup_id;
        decision.backup_path = backup.path;
        decision.restore_point_time = backup.end_time;
        decision.exact_replay_available = false;

        if (!backup.valid)
        {
            decision.recoverable = false;
            decision.reason = "invalid_backup";
        }
        else if (pitr_policy_.mode == PITRMode::DISABLED)
        {
            decision.recoverable = false;
            decision.reason = "pitr_disabled";
        }
        else if (!parent_chain_available(backup))
        {
            decision.recoverable = false;
            decision.reason = "missing_parent_chain";
        }
        else if (pitr_policy_.retention_window_micros > 0 &&
                 latest_available_time > backup.end_time &&
                 (latest_available_time - backup.end_time) > pitr_policy_.retention_window_micros)
        {
            decision.recoverable = false;
            decision.reason = "outside_retention_window";
        }
        else
        {
            decision.recoverable = true;
            decision.reason = "snapshot_restore_point";
        }

        decisions_out->push_back(std::move(decision));
    }

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

    if (!has_policy_)
    {
        policy_ = defaultBackupPolicy();
        has_policy_ = true;
    }

    BackupCatalogFileHeader header{};
    std::memcpy(header.magic, BACKUP_CATALOG_MAGIC, sizeof(header.magic));
    header.version = BACKUP_CATALOG_VERSION;
    header.flags = has_policy_ ? 1u : 0u;
    header.backup_count = static_cast<uint32_t>(backups_.size());
    header.retention_rule_count = static_cast<uint32_t>(policy_.retention_rules.size());
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    BackupCatalogPolicyRecord policy_record{};
    policy_record.schema_version = policy_.schema_version;
    policy_record.enabled = policy_.enabled ? 1 : 0;
    policy_record.pitr_mode = static_cast<uint8_t>(pitr_policy_.mode);
    policy_record.pitr_retention_window_micros = pitr_policy_.retention_window_micros;
    file.write(reinterpret_cast<const char*>(&policy_record), sizeof(policy_record));
    if (!writeLengthPrefixedString(file, policy_.policy_name) ||
        !writeLengthPrefixedString(file, policy_.storage_profile))
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write backup policy strings");
        return Status::IO_ERROR;
    }

    for (const auto& rule : policy_.retention_rules)
    {
        BackupCatalogRetentionRuleRecord record{};
        record.type = static_cast<uint8_t>(rule.type);
        record.require_parent_chain = rule.require_parent_chain ? 1 : 0;
        record.retain_count = rule.retain_count;
        record.max_age_micros = rule.max_age_micros;
        file.write(reinterpret_cast<const char*>(&record), sizeof(record));
    }

    for (const auto& backup : backups_)
    {
        BackupCatalogMetadataRecord record{};
        record.backup_id = backup.backup_id;
        record.parent_id = backup.parent_id;
        record.type = static_cast<uint8_t>(backup.type);
        record.valid = backup.valid ? 1 : 0;
        record.start_time = backup.start_time;
        record.end_time = backup.end_time;
        record.total_pages = backup.total_pages;
        record.size_bytes = backup.size_bytes;
        record.start_xid = backup.start_xid;
        record.end_xid = backup.end_xid;
        file.write(reinterpret_cast<const char*>(&record), sizeof(record));
        if (!writeLengthPrefixedString(file, backup.path) ||
            !writeLengthPrefixedString(file, backup.storage_profile) ||
            !writeLengthPrefixedString(file, backup.label))
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write backup catalog strings");
            return Status::IO_ERROR;
        }
    }

    if (!file)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to persist backup catalog");
        return Status::IO_ERROR;
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

    backups_.clear();
    policy_ = defaultBackupPolicy();
    pitr_policy_ = defaultPITRPolicy();
    has_policy_ = false;

    char magic[8]{};
    file.read(magic, sizeof(magic));
    if (!file)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read backup catalog header");
        return Status::IO_ERROR;
    }

    file.seekg(0, std::ios::beg);
    if (std::memcmp(magic, BACKUP_CATALOG_MAGIC, sizeof(magic)) != 0)
    {
        uint64_t count = 0;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));
        if (!file)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read legacy backup catalog count");
            return Status::IO_ERROR;
        }

        backups_.reserve(count);
        for (uint64_t i = 0; i < count; ++i)
        {
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
            if (!readLengthPrefixedString(file, &backup.path) ||
                !readLengthPrefixedString(file, &backup.label))
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read legacy backup catalog entry");
                return Status::IO_ERROR;
            }
            backup.path = normalizeBackupPath(backup.path);
            backup.storage_profile = policy_.storage_profile;
            backups_.push_back(std::move(backup));
        }
        modified_ = false;
        return Status::OK;
    }

    BackupCatalogFileHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read backup catalog header");
        return Status::IO_ERROR;
    }
    if (header.version > BACKUP_CATALOG_VERSION)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "Backup catalog version too new");
        return Status::NOT_SUPPORTED;
    }

    BackupPolicy loaded_policy = defaultBackupPolicy();
    PITRPolicy loaded_pitr_policy = defaultPITRPolicy();
    if (header.version == 1)
    {
        LegacyBackupCatalogPolicyRecord policy_record{};
        file.read(reinterpret_cast<char*>(&policy_record), sizeof(policy_record));
        if (!file)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read backup policy record");
            return Status::IO_ERROR;
        }
        loaded_policy.schema_version = policy_record.schema_version;
        loaded_policy.enabled = policy_record.enabled != 0;
    }
    else
    {
        BackupCatalogPolicyRecord policy_record{};
        file.read(reinterpret_cast<char*>(&policy_record), sizeof(policy_record));
        if (!file)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read backup policy record");
            return Status::IO_ERROR;
        }
        loaded_policy.schema_version = policy_record.schema_version;
        loaded_policy.enabled = policy_record.enabled != 0;
        loaded_pitr_policy.schema_version = 1;
        loaded_pitr_policy.mode = static_cast<PITRMode>(policy_record.pitr_mode);
        loaded_pitr_policy.retention_window_micros =
            policy_record.pitr_retention_window_micros;
        if (!isSupportedPITRMode(loaded_pitr_policy.mode))
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Backup catalog PITR policy mode is invalid");
            return Status::DATA_CORRUPTED;
        }
    }
    if (!readLengthPrefixedString(file, &loaded_policy.policy_name) ||
        !readLengthPrefixedString(file, &loaded_policy.storage_profile))
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read backup policy strings");
        return Status::IO_ERROR;
    }
    loaded_policy.retention_rules.clear();
    loaded_policy.retention_rules.reserve(header.retention_rule_count);
    for (uint32_t i = 0; i < header.retention_rule_count; ++i)
    {
        BackupCatalogRetentionRuleRecord record{};
        file.read(reinterpret_cast<char*>(&record), sizeof(record));
        if (!file)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read backup retention rule");
            return Status::IO_ERROR;
        }
        loaded_policy.retention_rules.push_back(BackupRetentionRule{
            static_cast<BackupType>(record.type),
            record.retain_count,
            record.require_parent_chain != 0,
            record.max_age_micros,
        });
    }
    policy_ = loaded_policy;
    pitr_policy_ = loaded_pitr_policy;
    has_policy_ = (header.flags & 1u) != 0u || !policy_.retention_rules.empty();

    backups_.reserve(header.backup_count);
    for (uint32_t i = 0; i < header.backup_count; ++i)
    {
        BackupCatalogMetadataRecord record{};
        file.read(reinterpret_cast<char*>(&record), sizeof(record));
        if (!file)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read backup metadata record");
            return Status::IO_ERROR;
        }

        BackupMetadata backup;
        backup.backup_id = record.backup_id;
        backup.parent_id = record.parent_id;
        backup.type = static_cast<BackupType>(record.type);
        backup.valid = record.valid != 0;
        backup.start_time = record.start_time;
        backup.end_time = record.end_time;
        backup.total_pages = record.total_pages;
        backup.size_bytes = record.size_bytes;
        backup.start_xid = record.start_xid;
        backup.end_xid = record.end_xid;
        if (!readLengthPrefixedString(file, &backup.path) ||
            !readLengthPrefixedString(file, &backup.storage_profile) ||
            !readLengthPrefixedString(file, &backup.label))
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read backup catalog strings");
            return Status::IO_ERROR;
        }
        backup.path = normalizeBackupPath(backup.path);
        if (backup.storage_profile.empty())
        {
            backup.storage_profile = policy_.storage_profile;
        }
        backups_.push_back(std::move(backup));
    }

    modified_ = false;
    return Status::OK;
}

} // namespace scratchbird::core
