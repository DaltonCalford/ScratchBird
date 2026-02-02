/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * Index Factory - LSM-Tree Integration Phase 3
 *
 * Factory pattern for creating and opening different index types:
 * - BTREE: B-Tree index (page-based)
 * - HASH: Hash index (page-based)
 * - LSM: LSM-Tree index (file-based)
 * - GIN, GIST, BRIN, RTREE, SPGIST: Advanced indexes
 * - BITMAP, COLUMNSTORE: Column-oriented indexes
 * - HNSW: Vector similarity index
 *
 * Page-based indexes (BTREE, HASH, etc.) use the database buffer pool.
 * File-based indexes (LSM, COLUMNSTORE) use their own storage directories.
 */

#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/types.h"
#include <string>
#include <memory>

namespace scratchbird
{
namespace core
{

// Forward declarations
class BTree;
class HashIndex;
class LSMTree;
class GINIndex;
class GiSTIndex;
class BRINIndex;
class RTreeIndex;
class SPGiSTIndex;
class BitmapIndex;
class HNSWIndex;
class TransactionManager;

/**
 * IndexFactory - Creates and opens indexes based on type
 */
class IndexFactory
{
public:
    /**
     * Create new index
     *
     * @param index_type Type of index to create
     * @param db Database instance
     * @param index_info Index metadata from catalog
     * @param index_out Output: Pointer to index object (caller owns memory)
     * @param ctx Error context
     * @return Status::OK on success, error status otherwise
     */
    static Status createIndex(
        CatalogManager::IndexType index_type,
        Database *db,
        const CatalogManager::IndexInfo &index_info,
        void **index_out,
        ErrorContext *ctx = nullptr);

    /**
     * Open existing index
     *
     * @param index_type Type of index to open
     * @param db Database instance
     * @param index_info Index metadata from catalog
     * @param index_out Output: Pointer to index object (caller owns memory)
     * @param ctx Error context
     * @return Status::OK on success, error status otherwise
     */
    static Status openIndex(
        CatalogManager::IndexType index_type,
        Database *db,
        const CatalogManager::IndexInfo &index_info,
        void **index_out,
        ErrorContext *ctx = nullptr);

    /**
     * Close and deallocate index
     *
     * @param index_type Type of index
     * @param index_ptr Pointer to index object (will be deleted)
     * @param ctx Error context
     * @return Status::OK on success, error status otherwise
     */
    static Status closeIndex(
        CatalogManager::IndexType index_type,
        void *index_ptr,
        ErrorContext *ctx = nullptr);

    /**
     * Generate index storage path
     *
     * For file-based indexes (LSM, COLUMNSTORE), returns directory path.
     * For page-based indexes (BTREE, HASH, etc.), returns empty string.
     *
     * @param db_path Database file path
     * @param index_id Index ID
     * @param index_type Type of index
     * @return Storage path (empty for page-based indexes)
     */
    static std::string generateIndexPath(
        const std::string &db_path,
        const ID &index_id,
        CatalogManager::IndexType index_type);

private:
    IndexFactory() = delete;  // Static class, no instances
};

} // namespace core
} // namespace scratchbird
