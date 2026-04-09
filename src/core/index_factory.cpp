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
 * Index Factory Implementation - LSM-Tree Integration Phase 3
 */

#include "scratchbird/core/index_factory.h"
#include "scratchbird/core/btree.h"
#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/lsm_tree_index.h"
#include "scratchbird/core/gin_index.h"
#include "scratchbird/core/gist_index.h"
#include "scratchbird/core/brin_index.h"
#include "scratchbird/core/rtree.h"
#include "scratchbird/core/spgist_index.h"
#include "scratchbird/core/bitmap_index.h"
#include "scratchbird/core/hnsw_index.h"
#include "scratchbird/core/columnstore.h"
#include "scratchbird/core/fulltext_index.h"
#include "scratchbird/core/inverted_index.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/index_params.h"
#include "scratchbird/core/posix_compat.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <array>
#include <cstring>
#if defined(_WIN32)
    #include <direct.h>
#endif

namespace scratchbird
{
namespace core
{

namespace {

int createDirectoryPortable(const std::string& path)
{
#if defined(_WIN32)
    return ::_mkdir(path.c_str());
#else
    return ::mkdir(path.c_str(), 0755);
#endif
}

using IndexType = CatalogManager::IndexType;
using IndexCaps = IndexFactory::IndexFamilyCapabilities;
using IndexStorageModel = IndexFactory::IndexStorageModel;
using IndexRuntimeClass = IndexFactory::IndexRuntimeClass;

constexpr std::array<IndexCaps, 59> kIndexFamilyRegistry = {{
    {IndexType::BTREE, "BTREE", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::BTREE,
     true, true, true, false, false, false, false, true},
    {IndexType::HASH, "HASH", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HASH,
     true, true, true, false, false, false, false, true},
    {IndexType::HNSW, "HNSW", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HNSW,
     true, true, true, false, true, true, false, false},
    {IndexType::FULLTEXT, "FULLTEXT", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::INVERTED,
     true, true, true, false, true, false, false, false},
    {IndexType::GIN, "GIN", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::GIN,
     true, true, true, false, false, false, false, true},
    {IndexType::GIST, "GIST", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::GIST,
     true, true, true, false, true, false, false, false},
    {IndexType::BRIN, "BRIN", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::BRIN,
     true, true, true, false, true, false, true, false},
    {IndexType::RTREE, "RTREE", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::RTREE,
     true, true, true, false, true, false, false, false},
    {IndexType::SPGIST, "SPGIST", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::SPGIST,
     true, true, true, false, true, false, false, false},
    {IndexType::BITMAP, "BITMAP", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::BITMAP,
     true, true, true, false, false, false, false, false},
    {IndexType::COLUMNSTORE, "COLUMNSTORE", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::COLUMNSTORE,
     true, true, true, false, false, false, false, false},
    {IndexType::LSM, "LSM", IndexStorageModel::FILE_BASED, IndexRuntimeClass::LSM,
     true, true, true, true, false, false, false, false},
    {IndexType::IVF, "IVF", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HNSW,
     true, true, true, false, true, true, false, false},
    {IndexType::ART, "ART", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::BTREE,
     true, true, true, false, false, false, false, true},
    {IndexType::BLOOM, "BLOOM", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::BRIN,
     true, true, true, false, true, false, true, false},
    {IndexType::VECTOR_FLAT, "VECTOR_FLAT", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HNSW,
     true, true, true, false, true, true, false, false},
    {IndexType::VECTOR_BIN_FLAT, "VECTOR_BIN_FLAT", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HNSW,
     true, true, true, false, true, true, false, false},
    {IndexType::IVF_FLAT, "IVF_FLAT", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HNSW,
     true, true, true, false, true, true, false, false},
    {IndexType::BIN_IVF_FLAT, "BIN_IVF_FLAT", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HNSW,
     true, true, true, false, true, true, false, false},
    {IndexType::IVF_PQ, "IVF_PQ", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HNSW,
     true, true, true, false, true, true, false, false},
    {IndexType::IVF_SQ8, "IVF_SQ8", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HNSW,
     true, true, true, false, true, true, false, false},
    {IndexType::IVF_SQ8_HYBRID, "IVF_SQ8_HYBRID", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HNSW,
     true, true, true, false, true, true, false, false},
    {IndexType::RHNSW_PQ, "RHNSW_PQ", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HNSW,
     true, true, true, false, true, true, false, false},
    {IndexType::RHNSW_SQ, "RHNSW_SQ", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HNSW,
     true, true, true, false, true, true, false, false},
    {IndexType::ANNOY, "ANNOY", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HNSW,
     true, true, true, false, true, true, false, false},
    {IndexType::NSG, "NSG", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HNSW,
     true, true, true, false, true, true, false, false},
    {IndexType::DISKANN, "DISKANN", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HNSW,
     true, true, true, false, true, true, false, false},
    {IndexType::SCANN, "SCANN", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HNSW,
     true, true, true, false, true, true, false, false},
    {IndexType::GPU_CAGRA, "GPU_CAGRA", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HNSW,
     true, true, true, false, true, true, false, false},
    {IndexType::MINHASH_LSH, "MINHASH_LSH", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::INVERTED,
     true, true, true, false, false, false, false, false},
    {IndexType::SPARSE_INVERTED, "SPARSE_INVERTED", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::INVERTED,
     true, true, true, false, false, false, false, false},
    {IndexType::SPARSE_WAND, "SPARSE_WAND", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::INVERTED,
     true, true, true, false, false, false, false, false},
    {IndexType::TRIE, "TRIE", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::INVERTED,
     true, true, true, false, false, false, false, false},
    {IndexType::INVERTED, "INVERTED", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::INVERTED,
     true, true, true, false, false, false, false, false},
    {IndexType::STL_SORT, "STL_SORT", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::BTREE,
     true, true, true, false, false, false, false, true},
    {IndexType::NGRAM, "NGRAM", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::INVERTED,
     true, true, true, false, false, false, false, false},
    {IndexType::MONGODB_2D, "MONGODB_2D", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::RTREE,
     true, true, true, false, true, false, false, false},
    {IndexType::MONGODB_2DSPHERE, "MONGODB_2DSPHERE", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::RTREE,
     true, true, true, false, true, false, false, false},
    {IndexType::MONGODB_2DSPHERE_BUCKET, "MONGODB_2DSPHERE_BUCKET", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::RTREE,
     true, true, true, false, true, false, false, false},
    {IndexType::MONGODB_GEO_HAYSTACK, "MONGODB_GEO_HAYSTACK", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::BTREE,
     true, true, true, false, true, false, false, true},
    {IndexType::MONGODB_WILDCARD, "MONGODB_WILDCARD", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::INVERTED,
     true, true, true, false, true, false, false, false},
    {IndexType::MONGODB_ENCRYPTED_RANGE, "MONGODB_ENCRYPTED_RANGE", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::INVERTED,
     true, true, true, false, true, false, false, false},
    {IndexType::NEO4J_LOOKUP, "NEO4J_LOOKUP", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::BITMAP,
     true, true, true, false, true, false, false, false},
    {IndexType::NEO4J_TEXT, "NEO4J_TEXT", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::INVERTED,
     true, true, true, false, true, false, false, false},
    {IndexType::NEO4J_RANGE, "NEO4J_RANGE", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::BTREE,
     true, true, true, false, true, false, false, true},
    {IndexType::NEO4J_POINT, "NEO4J_POINT", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::BTREE,
     true, true, true, false, true, false, false, true},
    {IndexType::NEO4J_VECTOR, "NEO4J_VECTOR", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HNSW,
     true, true, true, false, true, true, false, false},
    {IndexType::CASSANDRA_SASI, "CASSANDRA_SASI", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::INVERTED,
     true, true, true, false, true, false, false, false},
    {IndexType::CASSANDRA_SAI, "CASSANDRA_SAI", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::INVERTED,
     true, true, true, false, true, false, false, false},
    {IndexType::REDIS_STRING, "REDIS_STRING", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HASH,
     true, true, true, false, true, false, false, false},
    {IndexType::REDIS_HASH, "REDIS_HASH", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HASH,
     true, true, true, false, true, false, false, false},
    {IndexType::REDIS_LIST, "REDIS_LIST", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::BTREE,
     true, true, true, false, true, false, false, true},
    {IndexType::REDIS_SET, "REDIS_SET", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HASH,
     true, true, true, false, true, false, false, false},
    {IndexType::REDIS_ZSET, "REDIS_ZSET", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::BTREE,
     true, true, true, false, true, false, false, true},
    {IndexType::REDIS_STREAM, "REDIS_STREAM", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::BTREE,
     true, true, true, false, true, false, false, true},
    {IndexType::REDIS_BITMAP, "REDIS_BITMAP", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::BITMAP,
     true, true, true, false, true, false, false, false},
    {IndexType::REDIS_HLL, "REDIS_HLL", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::HASH,
     true, true, true, false, true, false, false, false},
    {IndexType::REDIS_GEO, "REDIS_GEO", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::RTREE,
     true, true, true, false, true, false, false, false},
    {IndexType::ZONEMAP, "ZONEMAP", IndexStorageModel::PAGE_BASED, IndexRuntimeClass::BRIN,
     true, true, true, false, true, false, true, false}
}};

const IndexCaps *findIndexCaps(IndexType index_type)
{
    for (const auto &caps : kIndexFamilyRegistry)
    {
        if (caps.index_type == index_type)
        {
            return &caps;
        }
    }
    return nullptr;
}

std::string indexTypeDebugString(IndexType index_type)
{
    const IndexCaps *caps = findIndexCaps(index_type);
    if (caps)
    {
        return caps->canonical_name;
    }
    return std::to_string(static_cast<uint8_t>(index_type));
}

const char *plannerFamilyGroupName(IndexType index_type)
{
    switch (index_type)
    {
        case IndexType::BTREE:
        case IndexType::ART:
        case IndexType::STL_SORT:
        case IndexType::MONGODB_GEO_HAYSTACK:
        case IndexType::NEO4J_RANGE:
        case IndexType::NEO4J_POINT:
        case IndexType::REDIS_LIST:
        case IndexType::REDIS_ZSET:
        case IndexType::REDIS_STREAM:
            return "BTREE";
        case IndexType::HASH:
        case IndexType::REDIS_STRING:
        case IndexType::REDIS_HASH:
        case IndexType::REDIS_SET:
        case IndexType::REDIS_HLL:
            return "HASH";
        case IndexType::LSM:
            return "LSM";
        case IndexType::BRIN:
            return "BRIN";
        case IndexType::ZONEMAP:
        case IndexType::BLOOM:
            return "SUMMARY_FILTER";
        case IndexType::BITMAP:
        case IndexType::NEO4J_LOOKUP:
        case IndexType::REDIS_BITMAP:
            return "BITMAP";
        case IndexType::COLUMNSTORE:
            return "COLUMNSTORE";
        case IndexType::GIN:
            return "GIN";
        case IndexType::GIST:
            return "GIST";
        case IndexType::SPGIST:
            return "SPGIST";
        case IndexType::RTREE:
        case IndexType::MONGODB_2D:
        case IndexType::MONGODB_2DSPHERE:
        case IndexType::MONGODB_2DSPHERE_BUCKET:
        case IndexType::REDIS_GEO:
            return "RTREE";
        case IndexType::FULLTEXT:
        case IndexType::INVERTED:
        case IndexType::MINHASH_LSH:
        case IndexType::SPARSE_INVERTED:
        case IndexType::SPARSE_WAND:
        case IndexType::TRIE:
        case IndexType::NGRAM:
        case IndexType::MONGODB_WILDCARD:
        case IndexType::MONGODB_ENCRYPTED_RANGE:
        case IndexType::NEO4J_TEXT:
        case IndexType::CASSANDRA_SASI:
        case IndexType::CASSANDRA_SAI:
            return "TEXT_SEARCH";
        case IndexType::VECTOR_FLAT:
        case IndexType::VECTOR_BIN_FLAT:
            return "VECTOR_FLAT";
        case IndexType::HNSW:
        case IndexType::RHNSW_PQ:
        case IndexType::RHNSW_SQ:
        case IndexType::ANNOY:
        case IndexType::NSG:
        case IndexType::DISKANN:
        case IndexType::SCANN:
        case IndexType::GPU_CAGRA:
        case IndexType::NEO4J_VECTOR:
            return "ANN_HNSW";
        case IndexType::IVF:
        case IndexType::IVF_FLAT:
        case IndexType::BIN_IVF_FLAT:
        case IndexType::IVF_PQ:
        case IndexType::IVF_SQ8:
        case IndexType::IVF_SQ8_HYBRID:
            return "ANN_IVF";
    }
    return "UNKNOWN";
}

optimizer::IndexFamilyMetricsType metricsTypeForIndex(IndexType index_type)
{
    switch (index_type)
    {
        case IndexType::BTREE:
        case IndexType::ART:
        case IndexType::STL_SORT:
        case IndexType::MONGODB_GEO_HAYSTACK:
        case IndexType::NEO4J_RANGE:
        case IndexType::NEO4J_POINT:
        case IndexType::REDIS_LIST:
        case IndexType::REDIS_ZSET:
        case IndexType::REDIS_STREAM:
        case IndexType::HASH:
        case IndexType::REDIS_STRING:
        case IndexType::REDIS_HASH:
        case IndexType::REDIS_SET:
        case IndexType::REDIS_HLL:
        case IndexType::LSM:
            return optimizer::IndexFamilyMetricsType::ORDERED_EXACT;
        case IndexType::BRIN:
        case IndexType::ZONEMAP:
        case IndexType::BLOOM:
        case IndexType::BITMAP:
        case IndexType::NEO4J_LOOKUP:
        case IndexType::REDIS_BITMAP:
        case IndexType::COLUMNSTORE:
            return optimizer::IndexFamilyMetricsType::SUMMARY_CANDIDATE;
        case IndexType::GIN:
        case IndexType::GIST:
        case IndexType::SPGIST:
        case IndexType::RTREE:
        case IndexType::MONGODB_2D:
        case IndexType::MONGODB_2DSPHERE:
        case IndexType::MONGODB_2DSPHERE_BUCKET:
        case IndexType::REDIS_GEO:
            return optimizer::IndexFamilyMetricsType::GENERALIZED_SPATIAL;
        case IndexType::FULLTEXT:
        case IndexType::INVERTED:
        case IndexType::MINHASH_LSH:
        case IndexType::SPARSE_INVERTED:
        case IndexType::SPARSE_WAND:
        case IndexType::TRIE:
        case IndexType::NGRAM:
        case IndexType::MONGODB_WILDCARD:
        case IndexType::MONGODB_ENCRYPTED_RANGE:
        case IndexType::NEO4J_TEXT:
        case IndexType::CASSANDRA_SASI:
        case IndexType::CASSANDRA_SAI:
            return optimizer::IndexFamilyMetricsType::TEXT_SEARCH;
        case IndexType::VECTOR_FLAT:
        case IndexType::VECTOR_BIN_FLAT:
        case IndexType::HNSW:
        case IndexType::IVF:
        case IndexType::IVF_FLAT:
        case IndexType::BIN_IVF_FLAT:
        case IndexType::IVF_PQ:
        case IndexType::IVF_SQ8:
        case IndexType::IVF_SQ8_HYBRID:
        case IndexType::RHNSW_PQ:
        case IndexType::RHNSW_SQ:
        case IndexType::ANNOY:
        case IndexType::NSG:
        case IndexType::DISKANN:
        case IndexType::SCANN:
        case IndexType::GPU_CAGRA:
        case IndexType::NEO4J_VECTOR:
            return optimizer::IndexFamilyMetricsType::ANN;
    }
    return optimizer::IndexFamilyMetricsType::UNKNOWN;
}

const char *lifecycleModelName(IndexStorageModel storage_model)
{
    switch (storage_model)
    {
        case IndexStorageModel::FILE_BASED:
            return "MGA_FILE_BACKED";
        case IndexStorageModel::PAGE_BASED:
        default:
            return "MGA_PAGE_BACKED";
    }
}

const char *familyModeName(IndexType index_type, const IndexCaps &caps)
{
    switch (index_type)
    {
        case IndexType::FULLTEXT:
            return "QUERY_ALIAS";
        case IndexType::BTREE:
        case IndexType::HASH:
        case IndexType::HNSW:
        case IndexType::GIN:
        case IndexType::GIST:
        case IndexType::BRIN:
        case IndexType::RTREE:
        case IndexType::SPGIST:
        case IndexType::BITMAP:
        case IndexType::COLUMNSTORE:
        case IndexType::LSM:
            return "DIRECT";
        default:
            break;
    }

    if (caps.runtime_class == IndexRuntimeClass::INVERTED ||
        caps.runtime_class == IndexRuntimeClass::HNSW ||
        caps.runtime_class == IndexRuntimeClass::BTREE ||
        caps.runtime_class == IndexRuntimeClass::HASH ||
        caps.runtime_class == IndexRuntimeClass::BRIN ||
        caps.runtime_class == IndexRuntimeClass::BITMAP ||
        caps.runtime_class == IndexRuntimeClass::RTREE)
    {
        return "SHARED_RUNTIME";
    }
    return "DIRECT";
}

const char *queryabilityStateName(uint8_t state)
{
    switch (state)
    {
        case static_cast<uint8_t>(CatalogManager::IndexState::BUILDING):
            return "BUILDING";
        case static_cast<uint8_t>(CatalogManager::IndexState::RETIRED):
            return "RETIRING";
        case static_cast<uint8_t>(CatalogManager::IndexState::FAILED):
            return "FAILED";
        case static_cast<uint8_t>(CatalogManager::IndexState::INACTIVE):
            return "STALE";
        case static_cast<uint8_t>(CatalogManager::IndexState::ACTIVE):
        default:
            return "QUERYABLE";
    }
}

bool isZeroId(const ID& id)
{
    for (auto b : id.bytes)
    {
        if (b != 0)
        {
            return false;
        }
    }
    return true;
}

void attachBloomFilterIfConfigured(CatalogManager::IndexType index_type,
                                   void *index_ptr,
                                   Database *db,
                                   const CatalogManager::IndexInfo &index_info,
                                   ErrorContext *ctx)
{
    if (!db || !index_ptr || isZeroId(index_info.index_params_oid))
    {
        return;
    }

    std::string params_str;
    if (db->catalog_manager()->loadStringFromToast(index_info.index_params_oid, 0,
                                                   params_str, ctx) != Status::OK)
    {
        return;
    }

    IndexParams params;
    if (!parseIndexParams(params_str, &params))
    {
        return;
    }

    if (!params.has_bloom || !params.bloom.enabled || params.bloom.meta_gpid == 0)
    {
        return;
    }

    Status status = Status::OK;
    switch (index_type)
    {
        case CatalogManager::IndexType::BTREE:
        {
            auto *btree = static_cast<BTree *>(index_ptr);
            status = btree->loadBloomFilter(params.bloom.meta_gpid, params.bloom.target_fpr, ctx);
            break;
        }
        case CatalogManager::IndexType::HASH:
        {
            auto *hash = static_cast<HashIndex *>(index_ptr);
            status = hash->loadBloomFilter(params.bloom.meta_gpid, params.bloom.target_fpr, ctx);
            break;
        }
        case CatalogManager::IndexType::GIN:
        {
            auto *gin = static_cast<GinIndex *>(index_ptr);
            status = gin->loadBloomFilter(params.bloom.meta_gpid, params.bloom.target_fpr, ctx);
            break;
        }
        default:
            return;
    }

    if (status != Status::OK)
    {
        LOG_WARNING(STORAGE, "Failed to load Bloom filter for index %s: %d",
                    index_info.index_id.toString().c_str(), static_cast<int>(status));
    }
}

/**
 * Helper: Get column data type from catalog
 *
 * @param db Database instance
 * @param table_id Table UUID
 * @param column_id Column UUID
 * @param data_type_out Output: column data type
 * @param ctx Error context
 * @return Status
 */
Status getColumnDataType(Database* db,
                        const ID& table_id,
                        const ID& column_id,
                        uint16_t* data_type_out,
                        ErrorContext* ctx)
{
    if (!db || !data_type_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments to getColumnDataType");
        return Status::INVALID_ARGUMENT;
    }

    // Get all columns for the table
    std::vector<CatalogManager::ColumnInfo> columns;
    Status status = db->catalog_manager()->getColumns(table_id, columns, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Find the matching column
    for (const auto& col : columns)
    {
        if (col.column_id == column_id)
        {
            *data_type_out = col.data_type;
            return Status::OK;
        }
    }

    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Column not found in catalog");
    return Status::NOT_FOUND;
}

/**
 * Helper: Get vector dimensions from column metadata
 *
 * For VECTOR/HNSW indexes, dimensions are stored in type_precision field.
 *
 * @param db Database instance
 * @param table_id Table UUID
 * @param column_id Column UUID
 * @param dimensions_out Output: vector dimensions
 * @param ctx Error context
 * @return Status
 */
Status getVectorDimensions(Database* db,
                          const ID& table_id,
                          const ID& column_id,
                          uint32_t* dimensions_out,
                          ErrorContext* ctx)
{
    if (!db || !dimensions_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments to getVectorDimensions");
        return Status::INVALID_ARGUMENT;
    }

    // Get all columns for the table
    std::vector<CatalogManager::ColumnInfo> columns;
    Status status = db->catalog_manager()->getColumns(table_id, columns, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Find the matching column
    for (const auto& col : columns)
    {
        if (col.column_id == column_id)
        {
            // For VECTOR type, type_precision contains dimensions
            if (col.type_precision == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "Vector column has no dimensions specified (type_precision = 0)");
                return Status::INVALID_ARGUMENT;
            }
            *dimensions_out = col.type_precision;
            return Status::OK;
        }
    }

    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Column not found in catalog");
    return Status::NOT_FOUND;
}

} // anonymous namespace

const IndexFactory::IndexFamilyCapabilities *IndexFactory::lookupCapabilities(
    CatalogManager::IndexType index_type)
{
    return findIndexCaps(index_type);
}

std::vector<IndexFactory::IndexFamilyCapabilities> IndexFactory::listCapabilities()
{
    return std::vector<IndexFamilyCapabilities>(kIndexFamilyRegistry.begin(), kIndexFamilyRegistry.end());
}

void IndexFactory::populateCanonicalMetadata(CatalogManager::IndexInfo &index_info)
{
    const IndexFamilyCapabilities *caps = lookupCapabilities(index_info.index_type);
    if (!caps)
    {
        return;
    }

    index_info.physical_family = caps->canonical_name;
    index_info.planner_family = plannerFamilyGroupName(index_info.index_type);
    index_info.family_mode = familyModeName(index_info.index_type, *caps);
    if (index_info.format_version == 0)
    {
        index_info.format_version = 1;
    }
    if (index_info.family_options_version == 0)
    {
        index_info.family_options_version = 1;
    }
    index_info.alias_origin.clear();
    index_info.lifecycle_model = lifecycleModelName(caps->storage_model);
    index_info.metrics_type = metricsTypeForIndex(index_info.index_type);
    if (index_info.metrics_version == 0)
    {
        index_info.metrics_version = 1;
    }
    index_info.queryability_state = queryabilityStateName(index_info.state);
}

bool IndexFactory::validateCanonicalMetadata(const CatalogManager::IndexInfo &index_info,
                                            std::string *reason_out)
{
    CatalogManager::IndexInfo expected = index_info;
    populateCanonicalMetadata(expected);

    auto fail = [&](const std::string &reason) {
        if (reason_out)
        {
            *reason_out = reason;
        }
        return false;
    };

    if (index_info.physical_family != expected.physical_family)
    {
        return fail("physical_family mismatch");
    }
    if (index_info.planner_family != expected.planner_family)
    {
        return fail("planner_family mismatch");
    }
    if (index_info.family_mode != expected.family_mode)
    {
        return fail("family_mode mismatch");
    }
    if (index_info.format_version != expected.format_version)
    {
        return fail("format_version mismatch");
    }
    if (index_info.lifecycle_model != expected.lifecycle_model)
    {
        return fail("lifecycle_model mismatch");
    }
    if (index_info.metrics_type != expected.metrics_type)
    {
        return fail("metrics_type mismatch");
    }
    if (index_info.metrics_version == 0)
    {
        return fail("metrics_version missing");
    }
    if (index_info.family_options_version == 0)
    {
        return fail("family_options_version missing");
    }
    if (index_info.queryability_state != expected.queryability_state)
    {
        return fail("queryability_state mismatch");
    }
    return true;
}

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

    const IndexFamilyCapabilities *caps = lookupCapabilities(index_type);
    if (!caps)
    {
        std::string error_msg = "IndexFactory type not registered: " +
            std::to_string(static_cast<uint8_t>(index_type));
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, error_msg.c_str());
        return Status::INVALID_ARGUMENT;
    }
    {
        std::string validation_reason;
        if (!validateCanonicalMetadata(index_info, &validation_reason))
        {
            std::string error_msg =
                "IndexFactory canonical metadata invalid for type: " +
                std::string(caps->canonical_name) + " (" + validation_reason + ")";
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, error_msg.c_str());
            return Status::INVALID_ARGUMENT;
        }
    }
    if (!caps->supports_create)
    {
        std::string error_msg = "IndexFactory create not supported for type: " + std::string(caps->canonical_name);
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, error_msg.c_str());
        return Status::NOT_IMPLEMENTED;
    }
    if (caps->requires_primary_tablespace && index_info.tablespace_id != PRIMARY_TABLESPACE_ID)
    {
        std::string error_msg = std::string(caps->canonical_name) +
            " indexes only supported in primary tablespace";
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, error_msg.c_str());
        return Status::NOT_IMPLEMENTED;
    }
    if (caps->requires_indexed_columns && index_info.column_ids.empty())
    {
        std::string error_msg = std::string(caps->canonical_name) + " index requires at least one column";
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, error_msg.c_str());
        return Status::INVALID_ARGUMENT;
    }

    uint32_t vector_dimensions = 0;
    if (caps->requires_vector_dimensions)
    {
        Status status = getVectorDimensions(
            db, index_info.table_id, index_info.column_ids[0], &vector_dimensions, ctx);
        if (status != Status::OK)
        {
            return status;
        }
    }

    uint16_t brin_value_type = 0;
    if (caps->requires_column_datatype)
    {
        Status status = getColumnDataType(
            db, index_info.table_id, index_info.column_ids[0], &brin_value_type, ctx);
        if (status != Status::OK)
        {
            return status;
        }
    }

    switch (caps->runtime_class)
    {
        case IndexRuntimeClass::BTREE:
        {
            // B-Tree uses page-based storage
            // Create new B-Tree index
            Status status = BTree::create(
                db,
                index_info.index_id,
                index_info.table_id,
                index_info.column_ids,
                index_info.root_gpid,
                ctx);

            if (status != Status::OK)
            {
                return status;
            }

            // Open the created B-Tree
            auto btree = BTree::open(db, index_info.index_id, index_info.root_gpid, ctx);
            if (!btree)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open newly created B-Tree");
                return Status::IO_ERROR;
            }

            *index_out = btree.release();  // Transfer ownership to caller
            if (caps->supports_bloom_attach)
            {
                attachBloomFilterIfConfigured(index_type, *index_out, db, index_info, ctx);
            }
            return Status::OK;
        }

        case IndexRuntimeClass::LSM:
        {
            // LSM-Tree uses file-based storage
            std::string index_path = generateIndexPath(db->path(), index_info.index_id, index_type);

            // Ensure base indexes directory exists
            std::string index_dir = index_path;
            size_t last_sep = index_dir.find_last_of("/\\");
            if (last_sep != std::string::npos)
            {
                index_dir = index_dir.substr(0, last_sep);
            }
            if (!index_dir.empty())
            {
                if (createDirectoryPortable(index_dir) != 0 && errno != EEXIST)
                {
                    std::string error_msg = "Failed to create LSM-Tree directory: " +
                        std::string(strerror(errno));
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, error_msg.c_str());
                    return Status::IO_ERROR;
                }
            }

            // Create directory for LSM-Tree index data
            if (createDirectoryPortable(index_path) != 0 && errno != EEXIST)
            {
                std::string error_msg = "Failed to create LSM-Tree directory: " + std::string(strerror(errno));
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, error_msg.c_str());
                return Status::IO_ERROR;
            }

            // Create LSM-Tree index
            auto *lsm = new LSMTreeIndex(
                db,
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

        case IndexRuntimeClass::HASH:
        {
            // Hash index - page-based storage
            Status status = HashIndex::create(
                db,
                index_info.index_id,
                index_info.root_gpid,
                ctx);

            if (status != Status::OK)
            {
                return status;
            }

            // Open the created Hash index
            auto hash = HashIndex::open(db, index_info.index_id, index_info.root_gpid, ctx);
            if (!hash)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open newly created Hash index");
                return Status::IO_ERROR;
            }

            *index_out = hash.release();
            if (caps->supports_bloom_attach)
            {
                attachBloomFilterIfConfigured(index_type, *index_out, db, index_info, ctx);
            }
            return Status::OK;
        }

        case IndexRuntimeClass::GIN:
        {
            // GIN index - page-based storage
            Status status = GinIndex::create(
                db,
                index_info.index_id,
                index_info.root_gpid,
                ctx);

            if (status != Status::OK)
            {
                return status;
            }

            // Open the created GIN index
            auto gin = GinIndex::open(db, index_info.index_id, index_info.root_gpid, ctx);
            if (!gin)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open newly created GIN index");
                return Status::IO_ERROR;
            }

            *index_out = gin.release();
            if (caps->supports_bloom_attach)
            {
                attachBloomFilterIfConfigured(index_type, *index_out, db, index_info, ctx);
            }
            return Status::OK;
        }

        case IndexRuntimeClass::BITMAP:
        {
            // Bitmap index - page-based storage
            Status status = BitmapIndex::create(
                db,
                index_info.index_id,
                index_info.root_gpid,
                ctx);

            if (status != Status::OK)
            {
                return status;
            }

            // Open the created Bitmap index
            auto bitmap = BitmapIndex::open(db, index_info.index_id, index_info.root_gpid, ctx);
            if (!bitmap)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open newly created Bitmap index");
                return Status::IO_ERROR;
            }

            *index_out = bitmap.release();
            return Status::OK;
        }

        case IndexRuntimeClass::RTREE:
        {
            // R-Tree spatial index - page-based storage
            // Use default max_entries from index_info or standard value
            uint32_t max_entries = index_info.rtree_max_entries; // Already has default of 50

            Status status = RTree::create(
                db,
                index_info.index_id,
                index_info.table_id,
                index_info.column_ids,
                max_entries,
                index_info.root_gpid,
                ctx);

            if (status != Status::OK)
            {
                return status;
            }

            // Open the created R-Tree index
            auto rtree = RTree::open(db, index_info.index_id, index_info.root_gpid, max_entries, ctx);
            if (!rtree)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open newly created R-Tree index");
                return Status::IO_ERROR;
            }

            *index_out = rtree.release();
            return Status::OK;
        }

        case IndexRuntimeClass::COLUMNSTORE:
        {
            // Columnstore - page-based storage with default segment size and RLE compression
            Status status = ColumnstoreIndex::create(
                db,
                index_info.index_id,
                index_info.table_id,
                index_info.column_ids,
                1024,  // Default segment_size
                CompressionType::RLE,  // Default compression
                index_info.root_gpid,
                ctx);

            if (status != Status::OK)
            {
                return status;
            }

            // Open the created Columnstore index
            auto columnstore = ColumnstoreIndex::open(db, index_info.index_id, index_info.root_gpid, 1024, ctx);
            if (!columnstore)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open newly created Columnstore index");
                return Status::IO_ERROR;
            }

            *index_out = columnstore.release();
            return Status::OK;
        }

        case IndexRuntimeClass::HNSW:
        {
            Status status = HnswIndex::create(
                db,
                index_info.index_id,
                index_info.table_id,
                index_info.column_ids,
                vector_dimensions,
                DistanceMetric::EUCLIDEAN,  // Default distance metric
                16,    // Default m (max connections)
                200,   // Default ef_construction
                100,   // Default ef_search
                index_info.root_gpid,
                ctx);

            if (status != Status::OK)
            {
                return status;
            }

            // Open the created HNSW index
            auto hnsw = HnswIndex::open(db, index_info.index_id, index_info.root_gpid, ctx);
            if (!hnsw)
            {
                std::string error_msg = "Failed to open newly created ";
                error_msg += indexTypeDebugString(index_type);
                error_msg += " index";
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, error_msg.c_str());
                return Status::IO_ERROR;
            }

            *index_out = hnsw.release();
            return Status::OK;
        }

        case IndexRuntimeClass::BRIN:
        {
            // BRIN and ZONEMAP share a common runtime. ZONEMAP uses smaller summary ranges.
            const uint32_t range_size =
                (index_type == CatalogManager::IndexType::ZONEMAP) ? 64 : 128;

            Status status = BrinIndex::create(
                db,
                index_info.index_id,
                index_info.table_id,
                index_info.column_ids,
                static_cast<uint8_t>(brin_value_type),
                range_size,
                index_info.root_gpid,
                ctx);

            if (status != Status::OK)
            {
                return status;
            }

            // Open the created BRIN index
            auto brin = BrinIndex::open(db, index_info.index_id, index_info.root_gpid, ctx);
            if (!brin)
            {
                std::string error_msg = "Failed to open newly created ";
                error_msg += indexTypeDebugString(index_type);
                error_msg += " index";
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, error_msg.c_str());
                return Status::IO_ERROR;
            }

            *index_out = brin.release();
            return Status::OK;
        }

        case IndexRuntimeClass::GIST:
        {
            // GiST index with default operator class
            // Get default operator class (ID 0)
            auto& registry = GiSTOperatorClassRegistry::instance();
            auto opclass = registry.getOperatorClass(0);
            if (!opclass)
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Default GiST operator class not registered");
                return Status::NOT_FOUND;
            }

            Status status = GiSTIndex::create(
                db,
                index_info.index_id,
                index_info.table_id,
                index_info.column_ids,
                opclass,
                index_info.root_gpid,
                ctx);

            if (status != Status::OK)
            {
                return status;
            }

            // Open the created GiST index
            auto gist = GiSTIndex::open(db, index_info.index_id, index_info.table_id,
                                       index_info.column_ids, opclass, index_info.root_gpid, ctx);
            if (!gist)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open newly created GiST index");
                return Status::IO_ERROR;
            }

            *index_out = gist.release();
            return Status::OK;
        }

        case IndexRuntimeClass::SPGIST:
        {
            // SP-GiST index with default operator class
            // Get default operator class (ID 0)
            auto& registry = SPGiSTOperatorClassRegistry::instance();
            auto opclass = registry.getOperatorClass(0);
            if (!opclass)
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Default SP-GiST operator class not registered");
                return Status::NOT_FOUND;
            }

            Status status = SPGiSTIndex::create(
                db,
                index_info.index_id,
                index_info.table_id,
                index_info.column_ids,
                opclass,
                index_info.root_gpid,
                ctx);

            if (status != Status::OK)
            {
                return status;
            }

            // Open the created SP-GiST index
            auto spgist = SPGiSTIndex::open(db, index_info.index_id, index_info.table_id,
                                           index_info.column_ids, opclass, index_info.root_gpid, ctx);
            if (!spgist)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open newly created SP-GiST index");
                return Status::IO_ERROR;
            }

            *index_out = spgist.release();
            return Status::OK;
        }

        case IndexRuntimeClass::INVERTED:
        {
            // FULLTEXT index - standalone inverted index
            InvertedIndexConfig config;
            Status status = InvertedIndex::create(
                db,
                index_info.index_id,
                index_info.table_id,
                index_info.column_ids.front(),
                index_info.root_gpid,
                config,
                ctx);

            if (status != Status::OK)
            {
                return status;
            }

            // Open the created FULLTEXT index
            auto inverted = InvertedIndex::open(db, index_info.index_id, index_info.table_id,
                                               index_info.column_ids.front(), index_info.root_gpid, ctx);
            if (!inverted)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open newly created FULLTEXT index");
                return Status::IO_ERROR;
            }

            *index_out = inverted.release();
            return Status::OK;
        }

        default:
        {
            std::string error_msg = "IndexFactory runtime class not implemented for type: " +
                indexTypeDebugString(index_type);
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, error_msg.c_str());
            return Status::NOT_IMPLEMENTED;
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

    const IndexFamilyCapabilities *caps = lookupCapabilities(index_type);
    if (!caps)
    {
        std::string error_msg = "IndexFactory type not registered: " +
            std::to_string(static_cast<uint8_t>(index_type));
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, error_msg.c_str());
        return Status::INVALID_ARGUMENT;
    }
    {
        std::string validation_reason;
        if (!validateCanonicalMetadata(index_info, &validation_reason))
        {
            std::string error_msg =
                "IndexFactory canonical metadata invalid for type: " +
                std::string(caps->canonical_name) + " (" + validation_reason + ")";
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, error_msg.c_str());
            return Status::INVALID_ARGUMENT;
        }
    }
    if (!caps->supports_open)
    {
        std::string error_msg = "IndexFactory open not supported for type: " + std::string(caps->canonical_name);
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, error_msg.c_str());
        return Status::NOT_IMPLEMENTED;
    }
    if (caps->requires_primary_tablespace && index_info.tablespace_id != PRIMARY_TABLESPACE_ID)
    {
        std::string error_msg = std::string(caps->canonical_name) +
            " indexes only supported in primary tablespace";
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, error_msg.c_str());
        return Status::NOT_IMPLEMENTED;
    }
    if (caps->requires_indexed_columns && index_info.column_ids.empty())
    {
        std::string error_msg = std::string(caps->canonical_name) + " index requires at least one column";
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, error_msg.c_str());
        return Status::INVALID_ARGUMENT;
    }

    switch (caps->runtime_class)
    {
        case IndexRuntimeClass::BTREE:
        {
            // Open existing B-Tree
            auto btree = BTree::open(db, index_info.index_id, index_info.root_gpid, ctx);
            if (!btree)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open B-Tree index");
                return Status::IO_ERROR;
            }

            *index_out = btree.release();  // Transfer ownership to caller
            if (caps->supports_bloom_attach)
            {
                attachBloomFilterIfConfigured(index_type, *index_out, db, index_info, ctx);
            }
            return Status::OK;
        }

        case IndexRuntimeClass::LSM:
        {
            // Open existing LSM-Tree
            std::string index_path = generateIndexPath(db->path(), index_info.index_id, index_type);

            auto *lsm = new LSMTreeIndex(
                db,
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

        case IndexRuntimeClass::HASH:
        {
            // Open existing Hash index
            auto hash = HashIndex::open(db, index_info.index_id, index_info.root_gpid, ctx);
            if (!hash)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open Hash index");
                return Status::IO_ERROR;
            }

            *index_out = hash.release();
            if (caps->supports_bloom_attach)
            {
                attachBloomFilterIfConfigured(index_type, *index_out, db, index_info, ctx);
            }
            return Status::OK;
        }

        case IndexRuntimeClass::GIN:
        {
            // Open existing GIN index
            auto gin = GinIndex::open(db, index_info.index_id, index_info.root_gpid, ctx);
            if (!gin)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open GIN index");
                return Status::IO_ERROR;
            }

            *index_out = gin.release();
            if (caps->supports_bloom_attach)
            {
                attachBloomFilterIfConfigured(index_type, *index_out, db, index_info, ctx);
            }
            return Status::OK;
        }

        case IndexRuntimeClass::BITMAP:
        {
            // Open existing Bitmap index
            auto bitmap = BitmapIndex::open(db, index_info.index_id, index_info.root_gpid, ctx);
            if (!bitmap)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open Bitmap index");
                return Status::IO_ERROR;
            }

            *index_out = bitmap.release();
            return Status::OK;
        }

        case IndexRuntimeClass::RTREE:
        {
            // Open existing R-Tree index with max_entries from IndexInfo
            uint32_t max_entries = index_info.rtree_max_entries;
            auto rtree = RTree::open(db, index_info.index_id, index_info.root_gpid, max_entries, ctx);
            if (!rtree)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open R-Tree index");
                return Status::IO_ERROR;
            }

            *index_out = rtree.release();
            return Status::OK;
        }

        case IndexRuntimeClass::COLUMNSTORE:
        {
            // Open existing Columnstore index with default segment size
            auto columnstore = ColumnstoreIndex::open(db, index_info.index_id, index_info.root_gpid, 1024, ctx);
            if (!columnstore)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open Columnstore index");
                return Status::IO_ERROR;
            }

            *index_out = columnstore.release();
            return Status::OK;
        }

        case IndexRuntimeClass::HNSW:
        {
            // HNSW and IVF currently share the same runtime backend.
            auto hnsw = HnswIndex::open(db, index_info.index_id, index_info.root_gpid, ctx);
            if (!hnsw)
            {
                std::string error_msg = "Failed to open ";
                error_msg += indexTypeDebugString(index_type);
                error_msg += " index";
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, error_msg.c_str());
                return Status::IO_ERROR;
            }

            *index_out = hnsw.release();
            return Status::OK;
        }

        case IndexRuntimeClass::BRIN:
        {
            // BRIN and ZONEMAP share the BRIN runtime backend.
            auto brin = BrinIndex::open(db, index_info.index_id, index_info.root_gpid, ctx);
            if (!brin)
            {
                std::string error_msg = "Failed to open ";
                error_msg += indexTypeDebugString(index_type);
                error_msg += " index";
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, error_msg.c_str());
                return Status::IO_ERROR;
            }

            *index_out = brin.release();
            return Status::OK;
        }

        case IndexRuntimeClass::GIST:
        {
            // Open existing GiST index with default operator class
            auto& registry = GiSTOperatorClassRegistry::instance();
            auto opclass = registry.getOperatorClass(0);
            if (!opclass)
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Default GiST operator class not registered");
                return Status::NOT_FOUND;
            }

            auto gist = GiSTIndex::open(db, index_info.index_id, index_info.table_id,
                                       index_info.column_ids, opclass, index_info.root_gpid, ctx);
            if (!gist)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open GiST index");
                return Status::IO_ERROR;
            }

            *index_out = gist.release();
            return Status::OK;
        }

        case IndexRuntimeClass::SPGIST:
        {
            // Open existing SP-GiST index with default operator class
            auto& registry = SPGiSTOperatorClassRegistry::instance();
            auto opclass = registry.getOperatorClass(0);
            if (!opclass)
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Default SP-GiST operator class not registered");
                return Status::NOT_FOUND;
            }

            auto spgist = SPGiSTIndex::open(db, index_info.index_id, index_info.table_id,
                                           index_info.column_ids, opclass, index_info.root_gpid, ctx);
            if (!spgist)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open SP-GiST index");
                return Status::IO_ERROR;
            }

            *index_out = spgist.release();
            return Status::OK;
        }

        case IndexRuntimeClass::INVERTED:
        {
            // Open existing FULLTEXT index
            auto inverted = InvertedIndex::open(db, index_info.index_id, index_info.table_id,
                                               index_info.column_ids.front(), index_info.root_gpid, ctx);
            if (!inverted)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open FULLTEXT index");
                return Status::IO_ERROR;
            }

            *index_out = inverted.release();
            return Status::OK;
        }

        default:
        {
            std::string error_msg = "IndexFactory runtime class not implemented for type: " +
                indexTypeDebugString(index_type);
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, error_msg.c_str());
            return Status::NOT_IMPLEMENTED;
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

    const IndexFamilyCapabilities *caps = lookupCapabilities(index_type);
    if (!caps)
    {
        std::string error_msg = "IndexFactory type not registered: " +
            std::to_string(static_cast<uint8_t>(index_type));
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, error_msg.c_str());
        return Status::INVALID_ARGUMENT;
    }
    if (!caps->supports_close)
    {
        std::string error_msg = "IndexFactory close not supported for type: " + std::string(caps->canonical_name);
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, error_msg.c_str());
        return Status::NOT_IMPLEMENTED;
    }

    switch (caps->runtime_class)
    {
        case IndexRuntimeClass::BTREE:
        {
            auto *btree = static_cast<BTree*>(index_ptr);
            delete btree;
            return Status::OK;
        }

        case IndexRuntimeClass::LSM:
        {
            auto *lsm = static_cast<LSMTreeIndex*>(index_ptr);
            Status status = lsm->close(ctx);
            delete lsm;
            return status;
        }

        case IndexRuntimeClass::HASH:
        {
            auto *hash = static_cast<HashIndex*>(index_ptr);
            delete hash;
            return Status::OK;
        }

        case IndexRuntimeClass::HNSW:
        {
            auto *hnsw = static_cast<HnswIndex*>(index_ptr);
            delete hnsw;
            return Status::OK;
        }

        case IndexRuntimeClass::GIN:
        {
            auto *gin = static_cast<GinIndex*>(index_ptr);
            delete gin;
            return Status::OK;
        }

        case IndexRuntimeClass::BRIN:
        {
            auto *brin = static_cast<BrinIndex*>(index_ptr);
            delete brin;
            return Status::OK;
        }

        case IndexRuntimeClass::RTREE:
        {
            auto *rtree = static_cast<RTree*>(index_ptr);
            delete rtree;
            return Status::OK;
        }

        case IndexRuntimeClass::BITMAP:
        {
            auto *bitmap = static_cast<BitmapIndex*>(index_ptr);
            delete bitmap;
            return Status::OK;
        }

        case IndexRuntimeClass::COLUMNSTORE:
        {
            auto *columnstore = static_cast<ColumnstoreIndex*>(index_ptr);
            delete columnstore;
            return Status::OK;
        }

        case IndexRuntimeClass::GIST:
        {
            auto *gist = static_cast<GiSTIndex*>(index_ptr);
            delete gist;
            return Status::OK;
        }

        case IndexRuntimeClass::SPGIST:
        {
            auto *spgist = static_cast<SPGiSTIndex*>(index_ptr);
            delete spgist;
            return Status::OK;
        }

        case IndexRuntimeClass::INVERTED:
        {
            auto *inverted = static_cast<InvertedIndex*>(index_ptr);
            delete inverted;
            return Status::OK;
        }

        default:
        {
            std::string error_msg = "IndexFactory runtime class not implemented for type: " +
                indexTypeDebugString(index_type);
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, error_msg.c_str());
            return Status::NOT_IMPLEMENTED;
        }
    }
}

std::string IndexFactory::generateIndexPath(
    const std::string &db_path,
    const ID &index_id,
    CatalogManager::IndexType index_type)
{
    // File-based indexes need directory paths
    const IndexFamilyCapabilities *caps = lookupCapabilities(index_type);
    if (caps && caps->storage_model == IndexStorageModel::FILE_BASED)
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
