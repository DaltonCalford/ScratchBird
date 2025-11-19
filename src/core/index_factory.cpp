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
#include "scratchbird/core/columnstore.h"
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
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open newly created B-Tree");
                return Status::IO_ERROR;
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
                std::string error_msg = "Failed to create LSM-Tree directory: " + std::string(strerror(errno));
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, error_msg.c_str());
                return Status::IO_ERROR;
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
            uint32_t meta_page = 0;
            Status status = HashIndex::create(
                db,
                index_info.index_id,
                &meta_page,
                ctx);

            if (status != Status::OK)
            {
                return status;
            }

            // Open the created Hash index
            auto hash = HashIndex::open(db, index_info.index_id, meta_page, ctx);
            if (!hash)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open newly created Hash index");
                return Status::IO_ERROR;
            }

            *index_out = hash.release();
            return Status::OK;
        }

        case CatalogManager::IndexType::GIN:
        {
            // GIN index - page-based storage
            uint32_t meta_page = 0;
            Status status = GinIndex::create(
                db,
                index_info.index_id,
                &meta_page,
                ctx);

            if (status != Status::OK)
            {
                return status;
            }

            // Open the created GIN index
            auto gin = GinIndex::open(db, index_info.index_id, meta_page, ctx);
            if (!gin)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open newly created GIN index");
                return Status::IO_ERROR;
            }

            *index_out = gin.release();
            return Status::OK;
        }

        case CatalogManager::IndexType::BITMAP:
        {
            // Bitmap index - page-based storage
            uint32_t meta_page = 0;
            Status status = BitmapIndex::create(
                db,
                index_info.index_id,
                &meta_page,
                ctx);

            if (status != Status::OK)
            {
                return status;
            }

            // Open the created Bitmap index
            auto bitmap = BitmapIndex::open(db, index_info.index_id, meta_page, ctx);
            if (!bitmap)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open newly created Bitmap index");
                return Status::IO_ERROR;
            }

            *index_out = bitmap.release();
            return Status::OK;
        }

        case CatalogManager::IndexType::RTREE:
        {
            // R-Tree spatial index - page-based storage
            // Use default max_entries from index_info or standard value
            uint32_t max_entries = index_info.rtree_max_entries; // Already has default of 50
            uint32_t root_page = 0;

            Status status = RTree::create(
                db,
                index_info.index_id,
                index_info.table_id,
                index_info.column_ids,
                max_entries,
                &root_page,
                ctx);

            if (status != Status::OK)
            {
                return status;
            }

            // Open the created R-Tree index
            auto rtree = RTree::open(db, index_info.index_id, root_page, max_entries, ctx);
            if (!rtree)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open newly created R-Tree index");
                return Status::IO_ERROR;
            }

            *index_out = rtree.release();
            return Status::OK;
        }

        case CatalogManager::IndexType::COLUMNSTORE:
        {
            // Columnstore - page-based storage with default segment size and RLE compression
            uint32_t root_page = 0;
            Status status = ColumnstoreIndex::create(
                db,
                index_info.index_id,
                index_info.table_id,
                index_info.column_ids,
                1024,  // Default segment_size
                CompressionType::RLE,  // Default compression
                &root_page,
                ctx);

            if (status != Status::OK)
            {
                return status;
            }

            // Open the created Columnstore index
            auto columnstore = ColumnstoreIndex::open(db, index_info.index_id, root_page, 1024, ctx);
            if (!columnstore)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open newly created Columnstore index");
                return Status::IO_ERROR;
            }

            *index_out = columnstore.release();
            return Status::OK;
        }

        case CatalogManager::IndexType::HNSW:
        {
            // HNSW requires dimensions which must be determined from vector column type
            // This information is not available in IndexInfo and requires schema inspection
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                "HNSW index requires vector dimensions (inspect column schema to determine)");
            return Status::NOT_IMPLEMENTED;
        }

        case CatalogManager::IndexType::BRIN:
        {
            // BRIN requires value_type (DataType enum) from indexed column
            // This information requires schema inspection to determine the column's data type
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                "BRIN index requires column data type (inspect column schema to determine)");
            return Status::NOT_IMPLEMENTED;
        }

        case CatalogManager::IndexType::GIST:
        case CatalogManager::IndexType::SPGIST:
        {
            // GiST and SP-GiST require operator classes which are not yet fully integrated
            // For now, return NOT_IMPLEMENTED until operator class registry is ready
            std::string error_msg = "Index type requires operator class (not yet integrated): " + indexTypeToString(index_type);
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, error_msg.c_str());
            return Status::NOT_IMPLEMENTED;
        }

        case CatalogManager::IndexType::FULLTEXT:
        {
            // FULLTEXT is GIN-based but needs special text processing
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "FULLTEXT index not yet implemented (planned as GIN-based)");
            return Status::NOT_IMPLEMENTED;
        }

        default:
        {
            std::string error_msg = "Unknown index type: " + std::to_string(static_cast<uint8_t>(index_type));
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, error_msg.c_str());
            return Status::INVALID_ARGUMENT;
        }
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
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open B-Tree index");
                return Status::IO_ERROR;
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
        {
            // Open existing Hash index
            auto hash = HashIndex::open(db, index_info.index_id, index_info.root_page, ctx);
            if (!hash)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open Hash index");
                return Status::IO_ERROR;
            }

            *index_out = hash.release();
            return Status::OK;
        }

        case CatalogManager::IndexType::GIN:
        {
            // Open existing GIN index
            auto gin = GinIndex::open(db, index_info.index_id, index_info.root_page, ctx);
            if (!gin)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open GIN index");
                return Status::IO_ERROR;
            }

            *index_out = gin.release();
            return Status::OK;
        }

        case CatalogManager::IndexType::BITMAP:
        {
            // Open existing Bitmap index
            auto bitmap = BitmapIndex::open(db, index_info.index_id, index_info.root_page, ctx);
            if (!bitmap)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open Bitmap index");
                return Status::IO_ERROR;
            }

            *index_out = bitmap.release();
            return Status::OK;
        }

        case CatalogManager::IndexType::RTREE:
        {
            // Open existing R-Tree index with max_entries from IndexInfo
            uint32_t max_entries = index_info.rtree_max_entries;
            auto rtree = RTree::open(db, index_info.index_id, index_info.root_page, max_entries, ctx);
            if (!rtree)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open R-Tree index");
                return Status::IO_ERROR;
            }

            *index_out = rtree.release();
            return Status::OK;
        }

        case CatalogManager::IndexType::COLUMNSTORE:
        {
            // Open existing Columnstore index with default segment size
            auto columnstore = ColumnstoreIndex::open(db, index_info.index_id, index_info.root_page, 1024, ctx);
            if (!columnstore)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open Columnstore index");
                return Status::IO_ERROR;
            }

            *index_out = columnstore.release();
            return Status::OK;
        }

        case CatalogManager::IndexType::HNSW:
        {
            // HNSW open is simple but creation requires dimensions
            auto hnsw = HnswIndex::open(db, index_info.index_id, index_info.root_page, ctx);
            if (!hnsw)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open HNSW index");
                return Status::IO_ERROR;
            }

            *index_out = hnsw.release();
            return Status::OK;
        }

        case CatalogManager::IndexType::BRIN:
        {
            // BRIN open is simple but creation requires value_type
            auto brin = BrinIndex::open(db, index_info.index_id, index_info.root_page, ctx);
            if (!brin)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open BRIN index");
                return Status::IO_ERROR;
            }

            *index_out = brin.release();
            return Status::OK;
        }

        case CatalogManager::IndexType::GIST:
        case CatalogManager::IndexType::SPGIST:
        {
            // GiST and SP-GiST require operator classes which are not yet fully integrated
            std::string error_msg = "Index type requires operator class (not yet integrated): " + indexTypeToString(index_type);
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, error_msg.c_str());
            return Status::NOT_IMPLEMENTED;
        }

        case CatalogManager::IndexType::FULLTEXT:
        {
            // FULLTEXT is GIN-based but needs special text processing
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "FULLTEXT index not yet implemented (planned as GIN-based)");
            return Status::NOT_IMPLEMENTED;
        }

        default:
        {
            std::string error_msg = "Unknown index type: " + std::to_string(static_cast<uint8_t>(index_type));
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, error_msg.c_str());
            return Status::INVALID_ARGUMENT;
        }
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

        case CatalogManager::IndexType::HNSW:
        {
            auto *hnsw = static_cast<HnswIndex*>(index_ptr);
            delete hnsw;
            return Status::OK;
        }

        case CatalogManager::IndexType::GIN:
        {
            auto *gin = static_cast<GinIndex*>(index_ptr);
            delete gin;
            return Status::OK;
        }

        case CatalogManager::IndexType::BRIN:
        {
            auto *brin = static_cast<BrinIndex*>(index_ptr);
            delete brin;
            return Status::OK;
        }

        case CatalogManager::IndexType::RTREE:
        {
            auto *rtree = static_cast<RTree*>(index_ptr);
            delete rtree;
            return Status::OK;
        }

        case CatalogManager::IndexType::BITMAP:
        {
            auto *bitmap = static_cast<BitmapIndex*>(index_ptr);
            delete bitmap;
            return Status::OK;
        }

        case CatalogManager::IndexType::COLUMNSTORE:
        {
            auto *columnstore = static_cast<ColumnstoreIndex*>(index_ptr);
            delete columnstore;
            return Status::OK;
        }

        case CatalogManager::IndexType::GIST:
        {
            auto *gist = static_cast<GiSTIndex*>(index_ptr);
            delete gist;
            return Status::OK;
        }

        case CatalogManager::IndexType::SPGIST:
        {
            auto *spgist = static_cast<SPGiSTIndex*>(index_ptr);
            delete spgist;
            return Status::OK;
        }

        case CatalogManager::IndexType::FULLTEXT:
        {
            // FULLTEXT not yet implemented
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
        // Convert UUID bytes to hex string
        snprintf(index_id_hex, sizeof(index_id_hex),
                 "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                 index_id.bytes[0], index_id.bytes[1], index_id.bytes[2], index_id.bytes[3],
                 index_id.bytes[4], index_id.bytes[5], index_id.bytes[6], index_id.bytes[7],
                 index_id.bytes[8], index_id.bytes[9], index_id.bytes[10], index_id.bytes[11],
                 index_id.bytes[12], index_id.bytes[13], index_id.bytes[14], index_id.bytes[15]);

        return db_dir + "/indexes/idx_" + std::string(index_id_hex);
    }

    // Page-based indexes don't need paths
    return "";
}

} // namespace core
} // namespace scratchbird
