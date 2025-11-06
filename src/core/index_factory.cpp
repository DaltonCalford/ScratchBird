/**
 * Index Factory Implementation - LSM-Tree Integration Phase 3
 */

#include "scratchbird/core/index_factory.h"
#include "scratchbird/core/btree.h"
#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/lsm_tree.h"
#include "scratchbird/core/gin_index.h"
#include "scratchbird/core/gist_index.h"
#include "scratchbird/core/brin_index.h"
#include "scratchbird/core/rtree.h"
#include "scratchbird/core/spgist_index.h"
#include "scratchbird/core/bitmap_index.h"
#include "scratchbird/core/hnsw_index.h"
#include "scratchbird/core/transaction_manager.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>

namespace scratchbird
{
namespace core
{

Status IndexFactory::createIndex(
    CatalogManager::IndexType index_type,
    Database *db,
    const CatalogManager::IndexInfo &index_info,
    void **index_out,
    ErrorContext *ctx)
{
    if (!db || !index_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null database or output pointer");
        return Status::INVALID_ARGUMENT;
    }

    *index_out = nullptr;

    switch (index_type)
    {
        case CatalogManager::IndexType::BTREE:
        {
            // B-Tree uses page-based storage
            // Create new B-Tree index
            uint32_t root_page = 0;
            Status status = BTree::create(
                db,
                index_info.index_id,
                index_info.table_id,
                index_info.column_ids,
                &root_page,
                ctx);

            if (status != Status::OK)
            {
                return status;
            }

            // Open the created B-Tree
            auto btree = BTree::open(db, index_info.index_id, root_page, ctx);
            if (!btree)
            {
                SET_ERROR_CONTEXT(ctx, Status::ERROR, "Failed to open newly created B-Tree");
                return Status::ERROR;
            }

            *index_out = btree.release();  // Transfer ownership to caller
            return Status::OK;
        }

        case CatalogManager::IndexType::LSM:
        {
            // LSM-Tree uses file-based storage
            std::string index_path = generateIndexPath(db->path(), index_info.index_id, index_type);

            // Create directory for LSM-Tree
            if (mkdir(index_path.c_str(), 0755) != 0 && errno != EEXIST)
            {
                SET_ERROR_CONTEXT(ctx, Status::ERROR,
                    "Failed to create LSM-Tree directory: " + std::string(strerror(errno)));
                return Status::ERROR;
            }

            // Create LSM-Tree index
            auto *lsm = new LSMTreeIndex(
                index_path,
                db->transaction_manager(),
                4  // 4 MB memtable size
            );

            Status status = lsm->create(ctx);
            if (status != Status::OK)
            {
                delete lsm;
                return status;
            }

            *index_out = static_cast<void*>(lsm);
            return Status::OK;
        }

        case CatalogManager::IndexType::HASH:
        {
            // Hash index - page-based storage
            // TODO: Implement Hash::create() similar to BTree
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "Hash index creation not yet implemented");
            return Status::NOT_IMPLEMENTED;
        }

        case CatalogManager::IndexType::GIN:
        case CatalogManager::IndexType::GIST:
        case CatalogManager::IndexType::BRIN:
        case CatalogManager::IndexType::RTREE:
        case CatalogManager::IndexType::SPGIST:
        case CatalogManager::IndexType::BITMAP:
        case CatalogManager::IndexType::HNSW:
        {
            // Other index types - not yet implemented
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                "Index type not yet implemented: " + indexTypeToString(index_type));
            return Status::NOT_IMPLEMENTED;
        }

        case CatalogManager::IndexType::COLUMNSTORE:
        {
            // Columnstore - file-based storage (similar to LSM-Tree)
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "Columnstore index creation not yet implemented");
            return Status::NOT_IMPLEMENTED;
        }

        default:
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                "Unknown index type: " + std::to_string(static_cast<uint8_t>(index_type)));
            return Status::INVALID_ARGUMENT;
    }
}

Status IndexFactory::openIndex(
    CatalogManager::IndexType index_type,
    Database *db,
    const CatalogManager::IndexInfo &index_info,
    void **index_out,
    ErrorContext *ctx)
{
    if (!db || !index_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null database or output pointer");
        return Status::INVALID_ARGUMENT;
    }

    *index_out = nullptr;

    switch (index_type)
    {
        case CatalogManager::IndexType::BTREE:
        {
            // Open existing B-Tree
            auto btree = BTree::open(db, index_info.index_id, index_info.root_page, ctx);
            if (!btree)
            {
                SET_ERROR_CONTEXT(ctx, Status::ERROR, "Failed to open B-Tree index");
                return Status::ERROR;
            }

            *index_out = btree.release();  // Transfer ownership to caller
            return Status::OK;
        }

        case CatalogManager::IndexType::LSM:
        {
            // Open existing LSM-Tree
            std::string index_path = generateIndexPath(db->path(), index_info.index_id, index_type);

            auto *lsm = new LSMTreeIndex(
                index_path,
                db->transaction_manager(),
                4  // 4 MB memtable size
            );

            Status status = lsm->open(ctx);
            if (status != Status::OK)
            {
                delete lsm;
                return status;
            }

            *index_out = static_cast<void*>(lsm);
            return Status::OK;
        }

        case CatalogManager::IndexType::HASH:
        case CatalogManager::IndexType::GIN:
        case CatalogManager::IndexType::GIST:
        case CatalogManager::IndexType::BRIN:
        case CatalogManager::IndexType::RTREE:
        case CatalogManager::IndexType::SPGIST:
        case CatalogManager::IndexType::BITMAP:
        case CatalogManager::IndexType::COLUMNSTORE:
        case CatalogManager::IndexType::HNSW:
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                "Index type not yet implemented: " + indexTypeToString(index_type));
            return Status::NOT_IMPLEMENTED;
        }

        default:
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                "Unknown index type: " + std::to_string(static_cast<uint8_t>(index_type)));
            return Status::INVALID_ARGUMENT;
    }
}

Status IndexFactory::closeIndex(
    CatalogManager::IndexType index_type,
    void *index_ptr,
    ErrorContext *ctx)
{
    if (!index_ptr)
    {
        return Status::OK;  // Already closed/null
    }

    switch (index_type)
    {
        case CatalogManager::IndexType::BTREE:
        {
            auto *btree = static_cast<BTree*>(index_ptr);
            delete btree;
            return Status::OK;
        }

        case CatalogManager::IndexType::LSM:
        {
            auto *lsm = static_cast<LSMTreeIndex*>(index_ptr);
            Status status = lsm->close(ctx);
            delete lsm;
            return status;
        }

        case CatalogManager::IndexType::HASH:
        {
            auto *hash = static_cast<HashIndex*>(index_ptr);
            delete hash;
            return Status::OK;
        }

        case CatalogManager::IndexType::GIN:
        case CatalogManager::IndexType::GIST:
        case CatalogManager::IndexType::BRIN:
        case CatalogManager::IndexType::RTREE:
        case CatalogManager::IndexType::SPGIST:
        case CatalogManager::IndexType::BITMAP:
        case CatalogManager::IndexType::COLUMNSTORE:
        case CatalogManager::IndexType::HNSW:
        {
            // Other index types - just delete the pointer for now
            // (Proper cleanup should be implemented when these are supported)
            return Status::OK;
        }

        default:
            return Status::OK;
    }
}

std::string IndexFactory::generateIndexPath(
    const std::string &db_path,
    const ID &index_id,
    CatalogManager::IndexType index_type)
{
    // File-based indexes need directory paths
    if (index_type == CatalogManager::IndexType::LSM ||
        index_type == CatalogManager::IndexType::COLUMNSTORE)
    {
        // Extract directory from database path
        size_t last_slash = db_path.find_last_of("/\\");
        std::string db_dir = (last_slash != std::string::npos) ? db_path.substr(0, last_slash) : ".";

        // Format: <db_dir>/indexes/idx_<index_id_hex>
        char index_id_hex[33];  // 32 hex chars + null terminator
        snprintf(index_id_hex, sizeof(index_id_hex),
                 "%016lx%016lx", index_id.high, index_id.low);

        return db_dir + "/indexes/idx_" + std::string(index_id_hex);
    }

    // Page-based indexes don't need paths
    return "";
}

} // namespace core
} // namespace scratchbird
