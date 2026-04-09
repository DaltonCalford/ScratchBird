/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/optimizer/statistics_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/table_stats_manager.h"
#include "scratchbird/core/btree.h"
#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/bitmap_index.h"
#include "scratchbird/core/brin_index.h"
#include "scratchbird/core/gist_index.h"
#include "scratchbird/core/gin_index.h"
#include "scratchbird/core/spgist_index.h"
#include "scratchbird/core/hnsw_index.h"
#include "scratchbird/core/columnstore.h"
#include "scratchbird/core/lsm_tree_index.h"
#include "scratchbird/core/rtree.h"
#include "scratchbird/core/inverted_index.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/plain_value_reader.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/optimizer/index_family_lowering.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <unordered_map>
#include <random>
#include <cmath>
#include <cstring>
#include <ctime>
#include <chrono>
#include <limits>
#include <unordered_set>

// SECURITY FIX (LOW-8): Use OpenSSL for cryptographically secure random numbers
#ifdef __has_include
#if __has_include(<openssl/rand.h>)
#include <openssl/rand.h>
#define HAVE_OPENSSL_RAND 1
#endif
#endif

namespace scratchbird::optimizer
{
    static bool isZeroId(const core::ID& id)
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

    static bool parseUuidFromString(const std::string &text, ID &out)
    {
        std::string hex;
        hex.reserve(32);
        for (char c : text)
        {
            if (c == '-')
            {
                continue;
            }
            if ((c >= '0' && c <= '9') ||
                (c >= 'a' && c <= 'f') ||
                (c >= 'A' && c <= 'F'))
            {
                hex.push_back(c);
            }
            else
            {
                return false;
            }
        }

        if (hex.size() != 32)
        {
            return false;
        }

        auto hexToNibble = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9')
            {
                return static_cast<uint8_t>(c - '0');
            }
            if (c >= 'a' && c <= 'f')
            {
                return static_cast<uint8_t>(10 + (c - 'a'));
            }
            return static_cast<uint8_t>(10 + (c - 'A'));
        };

        for (size_t i = 0; i < 16; ++i)
        {
            const uint8_t hi = hexToNibble(hex[i * 2]);
            const uint8_t lo = hexToNibble(hex[i * 2 + 1]);
            out.bytes[i] = static_cast<uint8_t>((hi << 4) | lo);
        }
        return true;
    }

    auto getIndexMetricsCacheKey(const ID &index_id) -> uint64_t
    {
        uint64_t key = 0;
        std::memcpy(&key, index_id.bytes.data(), sizeof(uint64_t));
        return key;
    }

    auto indexMetricsRefreshXid(core::Database *db, uint64_t requested_xid) -> uint64_t
    {
        if (requested_xid != 0)
        {
            return requested_xid;
        }
        if (db != nullptr && db->storage_engine() != nullptr)
        {
            const uint64_t current_xid = db->storage_engine()->getCurrentXid();
            if (current_xid != 0)
            {
                return current_xid;
            }
        }
        return 1;
    }

    auto indexTypeName(core::CatalogManager::IndexType index_type) -> std::string
    {
        using IndexType = core::CatalogManager::IndexType;
        switch (index_type)
        {
            case IndexType::BTREE: return "BTREE";
            case IndexType::HASH: return "HASH";
            case IndexType::HNSW: return "HNSW";
            case IndexType::FULLTEXT: return "FULLTEXT";
            case IndexType::GIN: return "GIN";
            case IndexType::GIST: return "GIST";
            case IndexType::BRIN: return "BRIN";
            case IndexType::RTREE: return "RTREE";
            case IndexType::SPGIST: return "SPGIST";
            case IndexType::BITMAP: return "BITMAP";
            case IndexType::COLUMNSTORE: return "COLUMNSTORE";
            case IndexType::LSM: return "LSM";
            case IndexType::IVF: return "IVF";
            case IndexType::ZONEMAP: return "ZONEMAP";
            case IndexType::ART: return "ART";
            case IndexType::BLOOM: return "BLOOM";
            case IndexType::VECTOR_FLAT: return "VECTOR_FLAT";
            case IndexType::VECTOR_BIN_FLAT: return "VECTOR_BIN_FLAT";
            case IndexType::IVF_FLAT: return "IVF_FLAT";
            case IndexType::BIN_IVF_FLAT: return "BIN_IVF_FLAT";
            case IndexType::IVF_PQ: return "IVF_PQ";
            case IndexType::IVF_SQ8: return "IVF_SQ8";
            case IndexType::IVF_SQ8_HYBRID: return "IVF_SQ8_HYBRID";
            case IndexType::RHNSW_PQ: return "RHNSW_PQ";
            case IndexType::RHNSW_SQ: return "RHNSW_SQ";
            case IndexType::ANNOY: return "ANNOY";
            case IndexType::NSG: return "NSG";
            case IndexType::DISKANN: return "DISKANN";
            case IndexType::SCANN: return "SCANN";
            case IndexType::GPU_CAGRA: return "GPU_CAGRA";
            case IndexType::MINHASH_LSH: return "MINHASH_LSH";
            case IndexType::SPARSE_INVERTED: return "SPARSE_INVERTED";
            case IndexType::SPARSE_WAND: return "SPARSE_WAND";
            case IndexType::TRIE: return "TRIE";
            case IndexType::INVERTED: return "INVERTED";
            case IndexType::STL_SORT: return "STL_SORT";
            case IndexType::NGRAM: return "NGRAM";
            case IndexType::MONGODB_2D: return "MONGODB_2D";
            case IndexType::MONGODB_2DSPHERE: return "MONGODB_2DSPHERE";
            case IndexType::MONGODB_2DSPHERE_BUCKET: return "MONGODB_2DSPHERE_BUCKET";
            case IndexType::MONGODB_GEO_HAYSTACK: return "MONGODB_GEO_HAYSTACK";
            case IndexType::MONGODB_WILDCARD: return "MONGODB_WILDCARD";
            case IndexType::MONGODB_ENCRYPTED_RANGE: return "MONGODB_ENCRYPTED_RANGE";
            case IndexType::NEO4J_LOOKUP: return "NEO4J_LOOKUP";
            case IndexType::NEO4J_TEXT: return "NEO4J_TEXT";
            case IndexType::NEO4J_RANGE: return "NEO4J_RANGE";
            case IndexType::NEO4J_POINT: return "NEO4J_POINT";
            case IndexType::NEO4J_VECTOR: return "NEO4J_VECTOR";
            case IndexType::CASSANDRA_SASI: return "CASSANDRA_SASI";
            case IndexType::CASSANDRA_SAI: return "CASSANDRA_SAI";
            case IndexType::REDIS_STRING: return "REDIS_STRING";
            case IndexType::REDIS_HASH: return "REDIS_HASH";
            case IndexType::REDIS_LIST: return "REDIS_LIST";
            case IndexType::REDIS_SET: return "REDIS_SET";
            case IndexType::REDIS_ZSET: return "REDIS_ZSET";
            case IndexType::REDIS_STREAM: return "REDIS_STREAM";
            case IndexType::REDIS_BITMAP: return "REDIS_BITMAP";
            case IndexType::REDIS_HLL: return "REDIS_HLL";
            case IndexType::REDIS_GEO: return "REDIS_GEO";
            default: return std::to_string(static_cast<uint32_t>(index_type));
        }
    }

    auto runtimeIndexFamilyName(core::CatalogManager::IndexType index_type) -> const char *
    {
        using IndexType = core::CatalogManager::IndexType;
        switch (index_type)
        {
            case IndexType::BTREE:
            case IndexType::STL_SORT:
            case IndexType::ART:
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
            case IndexType::RTREE:
            case IndexType::MONGODB_2D:
            case IndexType::MONGODB_2DSPHERE:
            case IndexType::MONGODB_2DSPHERE_BUCKET:
            case IndexType::REDIS_GEO:
                return "RTREE";
            case IndexType::BITMAP:
            case IndexType::NEO4J_LOOKUP:
            case IndexType::REDIS_BITMAP:
                return "BITMAP";
            case IndexType::GIN:
                return "GIN";
            case IndexType::GIST:
                return "GIST";
            case IndexType::BRIN:
            case IndexType::ZONEMAP:
            case IndexType::BLOOM:
                return "BRIN";
            case IndexType::SPGIST:
                return "SPGIST";
            case IndexType::HNSW:
            case IndexType::IVF:
            case IndexType::VECTOR_FLAT:
            case IndexType::VECTOR_BIN_FLAT:
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
                return "HNSW";
            case IndexType::FULLTEXT:
            case IndexType::INVERTED:
            case IndexType::MONGODB_WILDCARD:
            case IndexType::MONGODB_ENCRYPTED_RANGE:
            case IndexType::NEO4J_TEXT:
            case IndexType::CASSANDRA_SASI:
            case IndexType::CASSANDRA_SAI:
            case IndexType::TRIE:
            case IndexType::NGRAM:
            case IndexType::SPARSE_INVERTED:
            case IndexType::SPARSE_WAND:
            case IndexType::MINHASH_LSH:
                return "INVERTED";
            case IndexType::COLUMNSTORE:
                return "COLUMNSTORE";
            case IndexType::LSM:
                return "LSM";
            default:
                return "UNKNOWN";
        }
    }

    auto isAliasSurface(core::CatalogManager::IndexType index_type) -> bool
    {
        using IndexType = core::CatalogManager::IndexType;
        switch (index_type)
        {
            case IndexType::BTREE:
            case IndexType::HASH:
            case IndexType::HNSW:
            case IndexType::FULLTEXT:
            case IndexType::GIN:
            case IndexType::GIST:
            case IndexType::BRIN:
            case IndexType::RTREE:
            case IndexType::SPGIST:
            case IndexType::BITMAP:
            case IndexType::COLUMNSTORE:
            case IndexType::LSM:
            case IndexType::IVF:
            case IndexType::VECTOR_FLAT:
            case IndexType::VECTOR_BIN_FLAT:
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
            case IndexType::INVERTED:
            case IndexType::TRIE:
            case IndexType::NGRAM:
            case IndexType::SPARSE_INVERTED:
            case IndexType::SPARSE_WAND:
            case IndexType::MINHASH_LSH:
                return false;
            default:
                return true;
        }
    }

    auto runtimeFamilyHasNativeMetrics(core::CatalogManager::IndexType index_type) -> bool
    {
        using IndexType = core::CatalogManager::IndexType;
        switch (index_type)
        {
            case IndexType::BTREE:
            case IndexType::STL_SORT:
            case IndexType::ART:
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
            case IndexType::FULLTEXT:
            case IndexType::GIST:
            case IndexType::BITMAP:
            case IndexType::NEO4J_LOOKUP:
            case IndexType::REDIS_BITMAP:
            case IndexType::GIN:
            case IndexType::BRIN:
            case IndexType::ZONEMAP:
            case IndexType::BLOOM:
            case IndexType::SPGIST:
            case IndexType::RTREE:
            case IndexType::MONGODB_2D:
            case IndexType::MONGODB_2DSPHERE:
            case IndexType::MONGODB_2DSPHERE_BUCKET:
            case IndexType::REDIS_GEO:
            case IndexType::HNSW:
            case IndexType::IVF:
            case IndexType::VECTOR_FLAT:
            case IndexType::VECTOR_BIN_FLAT:
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
            case IndexType::INVERTED:
            case IndexType::MONGODB_WILDCARD:
            case IndexType::MONGODB_ENCRYPTED_RANGE:
            case IndexType::NEO4J_TEXT:
            case IndexType::CASSANDRA_SASI:
            case IndexType::CASSANDRA_SAI:
            case IndexType::TRIE:
            case IndexType::NGRAM:
            case IndexType::SPARSE_INVERTED:
            case IndexType::SPARSE_WAND:
            case IndexType::MINHASH_LSH:
            case IndexType::COLUMNSTORE:
            case IndexType::LSM:
                return true;
            default:
                return false;
        }
    }

    auto indexMetricsNativeMode(core::CatalogManager::IndexType index_type) -> const char *
    {
        const bool alias_surface = isAliasSurface(index_type);
        if (runtimeFamilyHasNativeMetrics(index_type))
        {
            return alias_surface ? "routed_family_native" : "family_native";
        }
        return alias_surface ? "routed_derived_heuristic" : "derived_heuristic";
    }

    auto indexMetricsSemanticContractState(core::CatalogManager::IndexType index_type) -> const char *
    {
        if (isAliasSurface(index_type))
        {
            return "alias_surface_routed";
        }
        if (runtimeFamilyHasNativeMetrics(index_type))
        {
            return "current_runtime_family";
        }
        return "current_runtime_with_heuristic_metrics";
    }

    auto collectNativeIndexFamilyMetrics(core::CatalogManager *catalog,
                                         const core::CatalogManager::IndexInfo &index_info)
        -> nlohmann::json
    {
        if (catalog == nullptr)
        {
            return nlohmann::json::object();
        }

        core::CatalogManager::IndexType actual_type = index_info.index_type;
        void *index_ptr = catalog->getIndexPtr(index_info.index_id, &actual_type);
        if (index_ptr == nullptr)
        {
            return nlohmann::json::object();
        }

        core::ErrorContext local_ctx;
        using IndexType = core::CatalogManager::IndexType;

        switch (actual_type)
        {
            case IndexType::BTREE:
            case IndexType::STL_SORT:
            case IndexType::ART:
            case IndexType::MONGODB_GEO_HAYSTACK:
            case IndexType::NEO4J_RANGE:
            case IndexType::NEO4J_POINT:
            case IndexType::REDIS_LIST:
            case IndexType::REDIS_ZSET:
            case IndexType::REDIS_STREAM:
            {
                auto *btree = static_cast<core::BTree *>(index_ptr);
                const auto &stats = btree->getIndexInfo();
                return {
                    {"native_family", "BTREE"},
                    {"height", stats.idx_height},
                    {"tuple_count", stats.idx_tuple_count},
                    {"page_count", stats.idx_page_count},
                    {"deleted_count", stats.idx_deleted_count}
                };
            }
            case IndexType::HASH:
            case IndexType::REDIS_STRING:
            case IndexType::REDIS_HASH:
            case IndexType::REDIS_SET:
            case IndexType::REDIS_HLL:
            {
                auto *hash = static_cast<core::HashIndex *>(index_ptr);
                auto stats = hash->getStatistics(&local_ctx);
                return {
                    {"native_family", "HASH"},
                    {"num_tuples", stats.num_tuples},
                    {"num_deleted", stats.num_deleted},
                    {"global_depth", stats.global_depth},
                    {"num_buckets", stats.num_buckets},
                    {"num_overflow_pages", stats.num_overflow_pages},
                    {"avg_entries_per_bucket", stats.avg_entries_per_bucket},
                    {"load_factor", stats.load_factor}
                };
            }
            case IndexType::BITMAP:
            case IndexType::NEO4J_LOOKUP:
            case IndexType::REDIS_BITMAP:
            {
                auto *bitmap = static_cast<core::BitmapIndex *>(index_ptr);
                auto stats = bitmap->getStatistics(&local_ctx);
                return {
                    {"native_family", "BITMAP"},
                    {"num_distinct_values", stats.num_distinct_values},
                    {"total_tuples", stats.total_tuples},
                    {"total_pages", stats.total_pages},
                    {"avg_cardinality", stats.avg_cardinality},
                    {"compression_ratio", stats.compression_ratio}
                };
            }
            case IndexType::BRIN:
            case IndexType::ZONEMAP:
            case IndexType::BLOOM:
            {
                auto *brin = static_cast<core::BrinIndex *>(index_ptr);
                core::BrinIndex::BrinStats stats{};
                if (brin->getStats(&stats, &local_ctx) != core::Status::OK)
                {
                    return nlohmann::json::object();
                }
                return {
                    {"native_family", "BRIN"},
                    {"total_ranges", stats.total_ranges},
                    {"deleted_ranges", stats.deleted_ranges},
                    {"total_pages", stats.total_pages},
                    {"blocks_covered", stats.blocks_covered},
                    {"avg_range_selectivity", stats.avg_range_selectivity}
                };
            }
            case IndexType::GIN:
            {
                auto *gin = static_cast<core::GinIndex *>(index_ptr);
                auto stats = gin->getStatistics(&local_ctx);
                return {
                    {"native_family", "GIN"},
                    {"num_keys", stats.num_keys},
                    {"num_tuples", stats.num_tuples},
                    {"pending_list_count", stats.pending_list_count},
                    {"keys_tree_height", stats.keys_tree_height},
                    {"avg_tids_per_key", stats.avg_tids_per_key},
                    {"num_posting_trees", stats.num_posting_trees},
                    {"num_posting_lists", stats.num_posting_lists}
                };
            }
            case IndexType::GIST:
            {
                auto *gist = static_cast<core::GiSTIndex *>(index_ptr);
                return {
                    {"native_family", "GIST"},
                    {"height", gist->getHeight()},
                    {"entry_count", gist->getEntryCount()},
                    {"deleted_count", gist->getDeadEntryCount()}
                };
            }
            case IndexType::SPGIST:
            {
                auto *spgist = static_cast<core::SPGiSTIndex *>(index_ptr);
                auto stats = spgist->getStats();
                return {
                    {"native_family", "SPGIST"},
                    {"total_entries", stats.total_entries},
                    {"deleted_entries", stats.deleted_entries},
                    {"max_depth", stats.max_depth},
                    {"avg_leaf_density", stats.avg_leaf_density}
                };
            }
            case IndexType::RTREE:
            case IndexType::MONGODB_2D:
            case IndexType::MONGODB_2DSPHERE:
            case IndexType::MONGODB_2DSPHERE_BUCKET:
            case IndexType::REDIS_GEO:
            {
                auto *rtree = static_cast<core::RTree *>(index_ptr);
                return {
                    {"native_family", "RTREE"},
                    {"height", rtree->getHeight()},
                    {"total_entries", rtree->getTotalEntries()},
                    {"deleted_entries", rtree->getDeletedEntries()}
                };
            }
            case IndexType::HNSW:
            case IndexType::IVF:
            case IndexType::VECTOR_FLAT:
            case IndexType::VECTOR_BIN_FLAT:
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
            {
                auto *hnsw = static_cast<core::HnswIndex *>(index_ptr);
                core::HnswIndex::HnswStats stats{};
                if (hnsw->getStats(&stats, &local_ctx) != core::Status::OK)
                {
                    return nlohmann::json::object();
                }
                return {
                    {"native_family", "HNSW"},
                    {"total_nodes", stats.total_nodes},
                    {"deleted_nodes", stats.deleted_nodes},
                    {"total_pages", stats.total_pages},
                    {"max_layer", stats.max_layer},
                    {"avg_connections", stats.avg_connections},
                    {"avg_path_length", stats.avg_path_length}
                };
            }
            case IndexType::COLUMNSTORE:
            {
                auto *columnstore = static_cast<core::ColumnstoreIndex *>(index_ptr);
                core::ColumnstoreIndex::ColumnstoreStats stats{};
                if (columnstore->getStats(&stats, &local_ctx) != core::Status::OK)
                {
                    return nlohmann::json::object();
                }
                return {
                    {"native_family", "COLUMNSTORE"},
                    {"total_segments", stats.total_segments},
                    {"total_rows", stats.total_rows},
                    {"compressed_bytes", stats.compressed_bytes},
                    {"uncompressed_bytes", stats.uncompressed_bytes},
                    {"compression_ratio", stats.compression_ratio},
                    {"null_count", stats.null_count}
                };
            }
            case IndexType::LSM:
            {
                auto *lsm = static_cast<core::LSMTreeIndex *>(index_ptr);
                core::Statistics stats{};
                if (lsm->getStatistics(&stats, &local_ctx) != core::Status::OK)
                {
                    return nlohmann::json::object();
                }
                return {
                    {"native_family", "LSM"},
                    {"active_memtable_entries", stats.active_memtable_entries},
                    {"active_memtable_size", stats.active_memtable_size},
                    {"immutable_memtable_entries", stats.immutable_memtable_entries},
                    {"immutable_memtable_size", stats.immutable_memtable_size},
                    {"level0_sstables", stats.level0_sstables},
                    {"level1_sstables", stats.level1_sstables},
                    {"level2_sstables", stats.level2_sstables},
                    {"level3_sstables", stats.level3_sstables},
                    {"total_sstables", stats.level0_sstables + stats.level1_sstables +
                                           stats.level2_sstables + stats.level3_sstables},
                    {"total_size_bytes", stats.total_size_bytes}
                };
            }
            case IndexType::FULLTEXT:
            case IndexType::INVERTED:
            case IndexType::MONGODB_WILDCARD:
            case IndexType::MONGODB_ENCRYPTED_RANGE:
            case IndexType::NEO4J_TEXT:
            case IndexType::CASSANDRA_SASI:
            case IndexType::CASSANDRA_SAI:
            case IndexType::TRIE:
            case IndexType::NGRAM:
            case IndexType::SPARSE_INVERTED:
            case IndexType::SPARSE_WAND:
            case IndexType::MINHASH_LSH:
            {
                auto *inverted = static_cast<core::InvertedIndex *>(index_ptr);
                auto stats = inverted->getStatistics(&local_ctx);
                return {
                    {"native_family", "INVERTED"},
                    {"num_segments", stats.num_segments},
                    {"total_documents", stats.total_documents},
                    {"total_terms", stats.total_terms},
                    {"total_tokens", stats.total_tokens},
                    {"avg_doc_length", stats.avg_doc_length},
                    {"total_queries", stats.total_queries},
                    {"avg_query_time_us", stats.avg_query_time_us},
                    {"last_merge_time", stats.last_merge_time}
                };
            }
            default:
                return nlohmann::json::object();
        }
    }

    auto defaultMetricsLowering(const core::CatalogManager::IndexInfo &index_info)
        -> PlannerFamilyLoweringResult
    {
        PlannerFamilyLoweringRequest request;
        request.index_type = index_info.index_type;
        request.predicate_shape = PredicateMatchShape::EQUALITY;
        request.ordered_output = false;
        request.skip_scan = false;
        request.bitmap_combine = false;
        request.nearest_order = false;
        return lowerPlannerFamily(request);
    }

    auto canonicalPhysicalFamilyName(const core::CatalogManager::IndexInfo &index_info)
        -> std::string
    {
        if (!index_info.physical_family.empty())
        {
            return index_info.physical_family;
        }
        return indexTypeName(index_info.index_type);
    }

    auto canonicalPlannerFamilyName(const core::CatalogManager::IndexInfo &index_info,
                                    const PlannerFamilyLoweringResult &lowering)
        -> std::string
    {
        if (!index_info.planner_family.empty())
        {
            return index_info.planner_family;
        }
        return lowering.family_name;
    }

    auto metricsTypeForLowering(const PlannerFamilyLoweringResult &lowering)
        -> IndexFamilyMetricsType
    {
        switch (lowering.family)
        {
            case PlannerAccessFamily::BTREE_EQ_SCAN:
            case PlannerAccessFamily::BTREE_RANGE_SCAN:
            case PlannerAccessFamily::BTREE_ORDERED_SCAN:
            case PlannerAccessFamily::BTREE_SKIP_SCAN:
            case PlannerAccessFamily::HASH_EQ_SCAN:
            case PlannerAccessFamily::LSM_EQ_SCAN:
            case PlannerAccessFamily::LSM_RANGE_SCAN:
            case PlannerAccessFamily::LSM_ORDERED_RANGE_SCAN:
                return IndexFamilyMetricsType::ORDERED_EXACT;
            case PlannerAccessFamily::BRIN_SCAN:
            case PlannerAccessFamily::SUMMARY_FILTER_SCAN:
            case PlannerAccessFamily::BITMAP_STORAGE_SCAN:
            case PlannerAccessFamily::BITMAP_COMBINE_SCAN:
            case PlannerAccessFamily::COLUMNSTORE_SCAN:
                return IndexFamilyMetricsType::SUMMARY_CANDIDATE;
            case PlannerAccessFamily::GIST_SCAN:
            case PlannerAccessFamily::SPGIST_SCAN:
            case PlannerAccessFamily::RTREE_SCAN:
            case PlannerAccessFamily::GIST_NEAREST_SCAN:
            case PlannerAccessFamily::SPGIST_NEAREST_SCAN:
            case PlannerAccessFamily::RTREE_NEAREST_SCAN:
            case PlannerAccessFamily::GIN_FILTER_SCAN:
                return IndexFamilyMetricsType::GENERALIZED_SPATIAL;
            case PlannerAccessFamily::TEXT_BITMAP_SCAN:
            case PlannerAccessFamily::TEXT_SCORE_SCAN:
            case PlannerAccessFamily::TEXT_RECHECK_SCAN:
                return IndexFamilyMetricsType::TEXT_SEARCH;
            case PlannerAccessFamily::VECTOR_FLAT_SCAN:
            case PlannerAccessFamily::HNSW_SCAN:
            case PlannerAccessFamily::IVF_SCAN:
            case PlannerAccessFamily::ANN_RERANK_SCAN:
            case PlannerAccessFamily::ANN_HYBRID_FALLBACK_SCAN:
                return IndexFamilyMetricsType::ANN;
            case PlannerAccessFamily::SEQ_SCAN:
            case PlannerAccessFamily::UNKNOWN:
            default:
                return IndexFamilyMetricsType::UNKNOWN;
        }
    }

    auto canonicalMetricsType(const core::CatalogManager::IndexInfo &index_info,
                              const PlannerFamilyLoweringResult &lowering)
        -> IndexFamilyMetricsType
    {
        if (index_info.metrics_type != IndexFamilyMetricsType::UNKNOWN)
        {
            return index_info.metrics_type;
        }
        return metricsTypeForLowering(lowering);
    }

    auto parseCatalogQueryabilityState(const std::string &value)
        -> std::optional<IndexMetricsQueryabilityState>
    {
        if (value.empty())
        {
            return std::nullopt;
        }

        std::string normalized = value;
        std::transform(normalized.begin(),
                       normalized.end(),
                       normalized.begin(),
                       [](unsigned char ch) {
                           return static_cast<char>(std::toupper(ch));
                       });

        if (normalized == "QUERYABLE" || normalized == "ACTIVE")
        {
            return IndexMetricsQueryabilityState::QUERYABLE;
        }
        if (normalized == "BUILDING" || normalized == "RETIRED" ||
            normalized == "INACTIVE" || normalized == "LIMITED")
        {
            return IndexMetricsQueryabilityState::LIMITED;
        }
        if (normalized == "FAILED" || normalized == "INVALID")
        {
            return IndexMetricsQueryabilityState::INVALID;
        }
        return std::nullopt;
    }

    auto metricsQueryabilityFromAccessState(AccessPathQueryabilityState state)
        -> IndexMetricsQueryabilityState
    {
        switch (state)
        {
            case AccessPathQueryabilityState::QUERYABLE:
                return IndexMetricsQueryabilityState::QUERYABLE;
            case AccessPathQueryabilityState::LIMITED:
                return IndexMetricsQueryabilityState::LIMITED;
            case AccessPathQueryabilityState::INVALID:
                return IndexMetricsQueryabilityState::INVALID;
            case AccessPathQueryabilityState::UNKNOWN:
            default:
                return IndexMetricsQueryabilityState::UNKNOWN;
        }
    }

    auto classifyIndexMetricsConfidence(
        const PlannerFamilyLoweringResult &lowering,
        const core::CatalogManager::IndexHealthCatalogInfo *health,
        bool sampled_refresh,
        float sample_rate) -> IndexMetricsConfidenceClass
    {
        if (lowering.queryability_state == AccessPathQueryabilityState::INVALID)
        {
            return IndexMetricsConfidenceClass::INVALID;
        }
        if (health != nullptr &&
            (health->diagnostic_status == core::CatalogManager::IndexHealthStatus::CORRUPT ||
             health->diagnostic_status == core::CatalogManager::IndexHealthStatus::ERROR))
        {
            return IndexMetricsConfidenceClass::INVALID;
        }
        if (sampled_refresh)
        {
            if (sample_rate == 0.0f || sample_rate >= 0.20f)
            {
                return IndexMetricsConfidenceClass::HIGH;
            }
            if (sample_rate >= 0.05f)
            {
                return IndexMetricsConfidenceClass::MEDIUM;
            }
            return IndexMetricsConfidenceClass::LOW;
        }
        if (health != nullptr &&
            health->light_status == core::CatalogManager::IndexHealthStatus::WARNING)
        {
            return IndexMetricsConfidenceClass::LOW;
        }
        return IndexMetricsConfidenceClass::MEDIUM;
    }

    auto effectiveMetricsQueryability(
        const PlannerFamilyLoweringResult &lowering,
        const core::CatalogManager::IndexHealthCatalogInfo *health,
        IndexMetricsConfidenceClass confidence_class) -> IndexMetricsQueryabilityState
    {
        IndexMetricsQueryabilityState base =
            metricsQueryabilityFromAccessState(lowering.queryability_state);
        if (base == IndexMetricsQueryabilityState::INVALID ||
            confidence_class == IndexMetricsConfidenceClass::INVALID)
        {
            return IndexMetricsQueryabilityState::INVALID;
        }
        if (health != nullptr &&
            (health->light_status == core::CatalogManager::IndexHealthStatus::WARNING ||
             health->diagnostic_status == core::CatalogManager::IndexHealthStatus::WARNING))
        {
            return IndexMetricsQueryabilityState::LIMITED;
        }
        return base == IndexMetricsQueryabilityState::UNKNOWN
                   ? IndexMetricsQueryabilityState::LIMITED
                   : base;
    }

    auto classifyIndexMetricsFreshness(
        IndexMetricsQueryabilityState queryability_state,
        IndexMetricsConfidenceClass confidence_class,
        uint64_t publish_lag_xids,
        uint64_t maintenance_backlog_ops,
        uint64_t reclaim_lag_xids) -> IndexMetricsFreshnessClass
    {
        if (confidence_class == IndexMetricsConfidenceClass::INVALID ||
            queryability_state == IndexMetricsQueryabilityState::INVALID)
        {
            return IndexMetricsFreshnessClass::UNUSABLE;
        }
        if (publish_lag_xids > 0 || maintenance_backlog_ops > 0 ||
            reclaim_lag_xids > 0)
        {
            return IndexMetricsFreshnessClass::STALE_DEGRADED;
        }
        if (confidence_class == IndexMetricsConfidenceClass::LOW ||
            queryability_state == IndexMetricsQueryabilityState::LIMITED ||
            confidence_class == IndexMetricsConfidenceClass::UNKNOWN ||
            queryability_state == IndexMetricsQueryabilityState::UNKNOWN)
        {
            return IndexMetricsFreshnessClass::AGED;
        }
        return IndexMetricsFreshnessClass::CURRENT;
    }

    auto classifyIndexMetricsInvalidationState(
        IndexMetricsFreshnessClass freshness_class) -> IndexMetricsInvalidationState
    {
        switch (freshness_class)
        {
            case IndexMetricsFreshnessClass::CURRENT:
            case IndexMetricsFreshnessClass::AGED:
                return IndexMetricsInvalidationState::VALID;
            case IndexMetricsFreshnessClass::STALE_DEGRADED:
                return IndexMetricsInvalidationState::INVALIDATED_SOFT;
            case IndexMetricsFreshnessClass::UNUSABLE:
                return IndexMetricsInvalidationState::INVALIDATED_HARD;
            case IndexMetricsFreshnessClass::UNKNOWN:
            default:
                return IndexMetricsInvalidationState::UNKNOWN;
        }
    }

    auto classifyIndexMetricsInvalidationReason(
        IndexMetricsQueryabilityState queryability_state,
        IndexMetricsConfidenceClass confidence_class,
        uint64_t publish_lag_xids,
        uint64_t maintenance_backlog_ops,
        uint64_t reclaim_lag_xids) -> std::string
    {
        if (confidence_class == IndexMetricsConfidenceClass::INVALID)
        {
            return "CONFIDENCE_INVALID";
        }
        if (queryability_state == IndexMetricsQueryabilityState::INVALID)
        {
            return "QUERYABILITY_INVALID";
        }
        if (publish_lag_xids > 0)
        {
            return "INDEX_STRUCTURE_CHANGE";
        }
        if (maintenance_backlog_ops > 0)
        {
            return "MAINTENANCE_EVENT";
        }
        if (reclaim_lag_xids > 0)
        {
            return "RECLAIM_PENDING";
        }
        return std::string();
    }

    // Hash function for std::vector<uint8_t> (for use in unordered_set)
    struct VectorHash
    {
        size_t operator()(const std::vector<uint8_t> &v) const
        {
            size_t hash = 0;
            for (uint8_t byte : v)
            {
                hash = hash * 31 + byte;
            }
            return hash;
        }
    };

    core::TypeInfo buildTypeInfo(const core::CatalogManager::ColumnInfo &column)
    {
        core::TypeInfo info(static_cast<core::DataType>(column.data_type));
        uint32_t precision = column.type_precision != 0 ? column.type_precision
                                                        : column.max_length;
        info.precision = precision;
        info.scale = column.type_scale;
        info.with_timezone = column.with_timezone;
        info.timezone_hint = column.timezone_hint;
        return info;
    }

    bool resolveColumnEncryption(core::DomainManager *domain_mgr,
                                 const core::CatalogManager::ColumnInfo &column,
                                 bool &encrypted_out,
                                 core::ErrorContext *ctx)
    {
        encrypted_out = false;
        if (column.domain_id == core::ID{} || domain_mgr == nullptr)
        {
            return true;
        }

        core::DomainInfo domain;
        core::Status status = domain_mgr->getDomain(column.domain_id, domain, ctx);
        if (status == core::Status::NOT_FOUND)
        {
            return true;
        }
        if (status != core::Status::OK)
        {
            return false;
        }
        encrypted_out = domain.security.encryption_enabled;
        return true;
    }

    auto readStringValue(const std::vector<uint8_t> &value, std::string &out) -> bool
    {
        if (value.empty())
        {
            out.clear();
            return true;
        }
        size_t offset = 0;
        uint32_t len = 0;
        if (!core::readUint32LE(value.data(), value.size(), offset, len) ||
            offset + len > value.size())
        {
            return false;
        }
        out.assign(reinterpret_cast<const char *>(value.data() + offset), len);
        return true;
    }

    auto encodeStringValue(const std::string &text) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> out(sizeof(uint32_t) + text.size());
        uint32_t len = static_cast<uint32_t>(text.size());
        std::memcpy(out.data(), &len, sizeof(len));
        if (!text.empty())
        {
            std::memcpy(out.data() + sizeof(uint32_t), text.data(), text.size());
        }
        return out;
    }

    auto decodeNumericValue(const std::vector<uint8_t> &value,
                            core::DataType data_type,
                            double &out) -> bool
    {
        if (value.empty())
        {
            return false;
        }

        switch (data_type)
        {
            case core::DataType::INT8:
                if (value.size() < sizeof(int8_t)) return false;
                out = static_cast<double>(*reinterpret_cast<const int8_t *>(value.data()));
                return true;
            case core::DataType::INT16:
                if (value.size() < sizeof(int16_t)) return false;
                out = static_cast<double>(*reinterpret_cast<const int16_t *>(value.data()));
                return true;
            case core::DataType::INT32:
            case core::DataType::DATE:
                if (value.size() < sizeof(int32_t)) return false;
                out = static_cast<double>(*reinterpret_cast<const int32_t *>(value.data()));
                return true;
            case core::DataType::INT64:
            case core::DataType::TIMESTAMP:
            case core::DataType::TIME:
                if (value.size() < sizeof(int64_t)) return false;
                out = static_cast<double>(*reinterpret_cast<const int64_t *>(value.data()));
                return true;
            case core::DataType::FLOAT32:
                if (value.size() < sizeof(float)) return false;
                out = static_cast<double>(*reinterpret_cast<const float *>(value.data()));
                return true;
            case core::DataType::FLOAT64:
                if (value.size() < sizeof(double)) return false;
                out = *reinterpret_cast<const double *>(value.data());
                return true;
            default:
                return false;
        }
    }

    template <typename T>
    auto readScalarValue(const std::vector<uint8_t> &value, T &out) -> bool
    {
        if (value.size() < sizeof(T))
        {
            return false;
        }
        std::memcpy(&out, value.data(), sizeof(T));
        return true;
    }

    auto hexEncodeBytes(const std::vector<uint8_t> &bytes) -> std::string
    {
        static constexpr char HEX[] = "0123456789abcdef";
        std::string out;
        out.reserve(bytes.size() * 2);
        for (uint8_t byte : bytes)
        {
            out.push_back(HEX[(byte >> 4) & 0x0F]);
            out.push_back(HEX[byte & 0x0F]);
        }
        return out;
    }

    auto hexDecodeBytes(const std::string &text, std::vector<uint8_t> &out) -> bool
    {
        auto nibble = [](char ch) -> int {
            if (ch >= '0' && ch <= '9')
            {
                return ch - '0';
            }
            if (ch >= 'a' && ch <= 'f')
            {
                return 10 + (ch - 'a');
            }
            if (ch >= 'A' && ch <= 'F')
            {
                return 10 + (ch - 'A');
            }
            return -1;
        };

        if (text.size() % 2 != 0)
        {
            return false;
        }

        out.clear();
        out.reserve(text.size() / 2);
        for (size_t i = 0; i < text.size(); i += 2)
        {
            int high = nibble(text[i]);
            int low = nibble(text[i + 1]);
            if (high < 0 || low < 0)
            {
                return false;
            }
            out.push_back(static_cast<uint8_t>((high << 4) | low));
        }
        return true;
    }

    auto encodeCompositeStatisticValue(
        const std::vector<std::vector<uint8_t>> &values) -> std::vector<uint8_t>
    {
        size_t total_size = sizeof(uint32_t);
        for (const auto &value : values)
        {
            total_size += sizeof(uint32_t) + value.size();
        }

        std::vector<uint8_t> out(total_size);
        size_t offset = 0;
        const uint32_t count = static_cast<uint32_t>(values.size());
        std::memcpy(out.data() + offset, &count, sizeof(count));
        offset += sizeof(count);
        for (const auto &value : values)
        {
            const uint32_t length = static_cast<uint32_t>(value.size());
            std::memcpy(out.data() + offset, &length, sizeof(length));
            offset += sizeof(length);
            if (!value.empty())
            {
                std::memcpy(out.data() + offset, value.data(), value.size());
                offset += value.size();
            }
        }
        return out;
    }

    auto decodeCompositeStatisticValue(const std::vector<uint8_t> &bytes,
                                       std::vector<std::vector<uint8_t>> &values) -> bool
    {
        values.clear();
        if (bytes.size() < sizeof(uint32_t))
        {
            return false;
        }

        size_t offset = 0;
        uint32_t count = 0;
        std::memcpy(&count, bytes.data() + offset, sizeof(count));
        offset += sizeof(count);
        values.reserve(count);
        for (uint32_t index = 0; index < count; ++index)
        {
            if (offset + sizeof(uint32_t) > bytes.size())
            {
                values.clear();
                return false;
            }

            uint32_t length = 0;
            std::memcpy(&length, bytes.data() + offset, sizeof(length));
            offset += sizeof(length);
            if (offset + length > bytes.size())
            {
                values.clear();
                return false;
            }

            values.emplace_back(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                bytes.begin() +
                                    static_cast<std::ptrdiff_t>(offset + length));
            offset += length;
        }

        return offset == bytes.size();
    }

    auto compareByteVectors(const std::vector<uint8_t> &lhs,
                            const std::vector<uint8_t> &rhs) -> int
    {
        const size_t shared = std::min(lhs.size(), rhs.size());
        for (size_t i = 0; i < shared; ++i)
        {
            if (lhs[i] < rhs[i])
            {
                return -1;
            }
            if (lhs[i] > rhs[i])
            {
                return 1;
            }
        }
        if (lhs.size() < rhs.size())
        {
            return -1;
        }
        if (lhs.size() > rhs.size())
        {
            return 1;
        }
        return 0;
    }

    auto decodeLengthPrefixedPayload(const std::vector<uint8_t> &value,
                                     std::vector<uint8_t> &payload_out) -> bool
    {
        if (value.empty())
        {
            payload_out.clear();
            return true;
        }

        size_t offset = 0;
        uint32_t len = 0;
        if (!core::readUint32LE(value.data(), value.size(), offset, len) ||
            offset + len > value.size())
        {
            return false;
        }

        payload_out.assign(value.begin() + static_cast<std::ptrdiff_t>(offset),
                           value.begin() + static_cast<std::ptrdiff_t>(offset + len));
        return true;
    }

    auto isLengthPrefixedType(core::DataType type) -> bool;

    auto comparatorFamilyForType(core::DataType type) -> StatisticsComparatorFamily
    {
        switch (type)
        {
            case core::DataType::INT8:
            case core::DataType::INT16:
            case core::DataType::INT32:
            case core::DataType::INT64:
            case core::DataType::INT128:
            case core::DataType::MEDIUMINT:
                return StatisticsComparatorFamily::SIGNED_INTEGER;
            case core::DataType::UINT8:
            case core::DataType::UINT16:
            case core::DataType::UINT32:
            case core::DataType::UINT64:
            case core::DataType::UINT128:
                return StatisticsComparatorFamily::UNSIGNED_INTEGER;
            case core::DataType::FLOAT32:
            case core::DataType::FLOAT64:
            case core::DataType::DECIMAL:
            case core::DataType::MONEY:
            case core::DataType::DECFLOAT16:
            case core::DataType::DECFLOAT34:
                return StatisticsComparatorFamily::NUMERIC;
            case core::DataType::CHAR:
            case core::DataType::VARCHAR:
            case core::DataType::TEXT:
            case core::DataType::JSON:
            case core::DataType::JSONB:
            case core::DataType::XML:
            case core::DataType::BLOB_SUB_TYPE_TEXT:
                return StatisticsComparatorFamily::STRING;
            case core::DataType::DATE:
            case core::DataType::TIME:
            case core::DataType::TIMESTAMP:
            case core::DataType::TIMESTAMP_WITH_ZONE:
            case core::DataType::TIME_WITH_ZONE:
            case core::DataType::DATETIME:
            case core::DataType::YEAR:
                return StatisticsComparatorFamily::TEMPORAL;
            case core::DataType::UUID:
                return StatisticsComparatorFamily::UUID;
            case core::DataType::BOOLEAN:
            case core::DataType::BIT:
                return StatisticsComparatorFamily::BOOLEAN;
            case core::DataType::BINARY:
            case core::DataType::VARBINARY:
            case core::DataType::BLOB:
            case core::DataType::BYTEA:
            case core::DataType::VECTOR:
            case core::DataType::BSON:
                return StatisticsComparatorFamily::BINARY;
            default:
                return StatisticsComparatorFamily::UNKNOWN;
        }
    }

    auto valueEncodingForType(core::DataType type) -> StatisticsValueEncoding
    {
        if (type == core::DataType::UUID)
        {
            return StatisticsValueEncoding::UUID_BYTES;
        }
        if (isLengthPrefixedType(type))
        {
            return StatisticsValueEncoding::PLAIN_LENGTH_PREFIXED;
        }
        return StatisticsValueEncoding::PLAIN_FIXED;
    }

    auto applyDerivedMetadata(ColumnStatistics &stats) -> void
    {
        if (stats.comparator_family == StatisticsComparatorFamily::UNKNOWN)
        {
            stats.comparator_family = comparatorFamilyForType(stats.data_type);
        }
        if (stats.value_encoding == StatisticsValueEncoding::UNKNOWN)
        {
            stats.value_encoding = valueEncodingForType(stats.data_type);
        }
    }

    auto applyColumnMetadata(const core::CatalogManager::ColumnInfo &column,
                             ColumnStatistics &stats) -> void
    {
        stats.comparator_family =
            comparatorFamilyForType(static_cast<core::DataType>(column.data_type));
        stats.value_encoding =
            valueEncodingForType(static_cast<core::DataType>(column.data_type));
        stats.collation_id = column.collation_id;
        stats.type_precision =
            column.type_precision != 0 ? column.type_precision : column.max_length;
        stats.type_scale = column.type_scale;
    }

    auto effectiveSampleRatio(uint64_t total_rows,
                              uint64_t sample_size,
                              float sample_rate) -> double
    {
        if (sample_rate > 0.0f)
        {
            return std::max(0.0, std::min(1.0, static_cast<double>(sample_rate)));
        }
        if (total_rows == 0)
        {
            return 0.0;
        }
        return std::max(0.0,
                        std::min(1.0,
                                 static_cast<double>(sample_size) /
                                     static_cast<double>(total_rows)));
    }

    auto mixStatsSnapshot(uint64_t hash, uint64_t value) -> uint64_t
    {
        hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
        return hash;
    }

    auto computeStatsSnapshotId(const ID &table_id,
                                const ID &column_id,
                                uint64_t last_analyzed_time,
                                uint64_t sample_size,
                                double sample_ratio,
                                uint64_t modified_rows_since_analyze) -> uint64_t
    {
        uint64_t hash = 0xcbf29ce484222325ULL;
        uint64_t table_prefix = 0;
        uint64_t column_prefix = 0;
        std::memcpy(&table_prefix, table_id.bytes.data(), sizeof(table_prefix));
        std::memcpy(&column_prefix, column_id.bytes.data(), sizeof(column_prefix));
        hash = mixStatsSnapshot(hash, table_prefix);
        hash = mixStatsSnapshot(hash, column_prefix);
        hash = mixStatsSnapshot(hash, last_analyzed_time);
        hash = mixStatsSnapshot(hash, sample_size);
        hash = mixStatsSnapshot(
            hash,
            static_cast<uint64_t>(std::llround(sample_ratio * 1000000.0)));
        hash = mixStatsSnapshot(hash, modified_rows_since_analyze);
        return hash;
    }

    auto classifyStaleness(uint64_t last_analyzed_time,
                           uint64_t total_rows,
                           uint64_t modified_rows_since_analyze)
        -> StatisticsStalenessClass
    {
        if (last_analyzed_time == 0)
        {
            return StatisticsStalenessClass::EXPIRED;
        }

        const uint64_t now =
            static_cast<uint64_t>(std::time(nullptr));
        const uint64_t age_seconds =
            now > last_analyzed_time ? (now - last_analyzed_time) : 0;
        const double row_basis = static_cast<double>(
            std::max<uint64_t>(1, total_rows));
        const double modification_ratio =
            static_cast<double>(modified_rows_since_analyze) / row_basis;

        if (age_seconds >= 86400 || modification_ratio >= 0.50)
        {
            return StatisticsStalenessClass::EXPIRED;
        }
        if (age_seconds >= 3600 || modification_ratio >= 0.20)
        {
            return StatisticsStalenessClass::STALE;
        }
        if (age_seconds >= 300 || modification_ratio >= 0.05)
        {
            return StatisticsStalenessClass::WARM;
        }
        return StatisticsStalenessClass::FRESH;
    }

    auto classifyConfidence(uint64_t sample_size,
                            double sample_ratio,
                            StatisticsStalenessClass staleness)
        -> StatisticsConfidenceClass
    {
        StatisticsConfidenceClass confidence = StatisticsConfidenceClass::LOW;
        if (sample_ratio >= 0.50 || sample_size >= 30000)
        {
            confidence = StatisticsConfidenceClass::HIGH;
        }
        else if (sample_ratio >= 0.10 || sample_size >= 5000)
        {
            confidence = StatisticsConfidenceClass::MEDIUM;
        }

        if (staleness == StatisticsStalenessClass::WARM &&
            confidence == StatisticsConfidenceClass::HIGH)
        {
            confidence = StatisticsConfidenceClass::MEDIUM;
        }
        else if (staleness == StatisticsStalenessClass::STALE ||
                 staleness == StatisticsStalenessClass::EXPIRED)
        {
            confidence = StatisticsConfidenceClass::LOW;
        }
        return confidence;
    }

    auto buildStatsMetadataJson(const ColumnStatistics &stats) -> nlohmann::json
    {
        return nlohmann::json{
            {"comparator_family", static_cast<uint32_t>(stats.comparator_family)},
            {"value_encoding", static_cast<uint32_t>(stats.value_encoding)},
            {"collation_id", stats.collation_id},
            {"type_precision", stats.type_precision},
            {"type_scale", stats.type_scale}};
    }

    auto applyStatsMetadataJson(const nlohmann::json &metadata,
                                ColumnStatistics &stats) -> void
    {
        if (!metadata.is_object())
        {
            return;
        }
        stats.comparator_family = static_cast<StatisticsComparatorFamily>(
            metadata.value("comparator_family",
                           static_cast<uint32_t>(stats.comparator_family)));
        stats.value_encoding = static_cast<StatisticsValueEncoding>(
            metadata.value("value_encoding",
                           static_cast<uint32_t>(stats.value_encoding)));
        stats.collation_id = metadata.value("collation_id", stats.collation_id);
        stats.type_precision = metadata.value("type_precision", stats.type_precision);
        stats.type_scale = metadata.value("type_scale", stats.type_scale);
        applyDerivedMetadata(stats);
    }

    auto decodeComparableScalar(const std::vector<uint8_t> &value,
                                core::DataType type,
                                long double &out) -> bool
    {
        switch (type)
        {
            case core::DataType::INT8: {
                int8_t decoded = 0;
                if (!readScalarValue(value, decoded)) return false;
                out = static_cast<long double>(decoded);
                return true;
            }
            case core::DataType::INT16: {
                int16_t decoded = 0;
                if (!readScalarValue(value, decoded)) return false;
                out = static_cast<long double>(decoded);
                return true;
            }
            case core::DataType::INT32:
            case core::DataType::MEDIUMINT:
            case core::DataType::DATE:
            case core::DataType::YEAR: {
                int32_t decoded = 0;
                if (!readScalarValue(value, decoded)) return false;
                out = static_cast<long double>(decoded);
                return true;
            }
            case core::DataType::INT64:
            case core::DataType::TIME:
            case core::DataType::TIMESTAMP:
            case core::DataType::TIMESTAMP_WITH_ZONE:
            case core::DataType::TIME_WITH_ZONE:
            case core::DataType::DATETIME: {
                int64_t decoded = 0;
                if (!readScalarValue(value, decoded)) return false;
                out = static_cast<long double>(decoded);
                return true;
            }
            case core::DataType::UINT8:
            case core::DataType::BOOLEAN:
            case core::DataType::BIT: {
                uint8_t decoded = 0;
                if (!readScalarValue(value, decoded)) return false;
                out = static_cast<long double>(decoded);
                return true;
            }
            case core::DataType::UINT16: {
                uint16_t decoded = 0;
                if (!readScalarValue(value, decoded)) return false;
                out = static_cast<long double>(decoded);
                return true;
            }
            case core::DataType::UINT32: {
                uint32_t decoded = 0;
                if (!readScalarValue(value, decoded)) return false;
                out = static_cast<long double>(decoded);
                return true;
            }
            case core::DataType::UINT64: {
                uint64_t decoded = 0;
                if (!readScalarValue(value, decoded)) return false;
                out = static_cast<long double>(decoded);
                return true;
            }
            case core::DataType::FLOAT32: {
                float decoded = 0.0f;
                if (!readScalarValue(value, decoded)) return false;
                out = static_cast<long double>(decoded);
                return true;
            }
            case core::DataType::FLOAT64:
            case core::DataType::DECIMAL:
            case core::DataType::MONEY:
            case core::DataType::DECFLOAT16:
            case core::DataType::DECFLOAT34: {
                if (value.size() >= sizeof(double))
                {
                    double decoded = 0.0;
                    if (!readScalarValue(value, decoded)) return false;
                    out = static_cast<long double>(decoded);
                    return true;
                }
                if (value.size() >= sizeof(float))
                {
                    float decoded = 0.0f;
                    if (!readScalarValue(value, decoded)) return false;
                    out = static_cast<long double>(decoded);
                    return true;
                }
                return false;
            }
            default:
                return false;
        }
    }

    auto compareTypedStatisticValues(core::DataType type,
                                     const std::vector<uint8_t> &lhs,
                                     const std::vector<uint8_t> &rhs) -> int
    {
        const StatisticsComparatorFamily family = comparatorFamilyForType(type);

        if (family == StatisticsComparatorFamily::STRING)
        {
            std::string lhs_text;
            std::string rhs_text;
            if (readStringValue(lhs, lhs_text) && readStringValue(rhs, rhs_text))
            {
                if (lhs_text < rhs_text) return -1;
                if (lhs_text > rhs_text) return 1;
                return 0;
            }
        }
        else if (family == StatisticsComparatorFamily::BINARY &&
                 valueEncodingForType(type) == StatisticsValueEncoding::PLAIN_LENGTH_PREFIXED)
        {
            std::vector<uint8_t> lhs_payload;
            std::vector<uint8_t> rhs_payload;
            if (decodeLengthPrefixedPayload(lhs, lhs_payload) &&
                decodeLengthPrefixedPayload(rhs, rhs_payload))
            {
                return compareByteVectors(lhs_payload, rhs_payload);
            }
        }
        else if (family == StatisticsComparatorFamily::UUID ||
                 family == StatisticsComparatorFamily::BINARY)
        {
            return compareByteVectors(lhs, rhs);
        }
        else
        {
            long double lhs_scalar = 0.0;
            long double rhs_scalar = 0.0;
            if (decodeComparableScalar(lhs, type, lhs_scalar) &&
                decodeComparableScalar(rhs, type, rhs_scalar))
            {
                if (lhs_scalar < rhs_scalar) return -1;
                if (lhs_scalar > rhs_scalar) return 1;
                return 0;
            }
        }

        return compareByteVectors(lhs, rhs);
    }

    auto ltrimAsciiCopy(std::string_view text) -> std::string
    {
        size_t start = 0;
        while (start < text.size() &&
               std::isspace(static_cast<unsigned char>(text[start])) != 0)
        {
            ++start;
        }
        return std::string(text.substr(start));
    }

    auto rtrimAsciiCopy(std::string_view text) -> std::string
    {
        size_t end = text.size();
        while (end > 0 &&
               std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
        {
            --end;
        }
        return std::string(text.substr(0, end));
    }

    auto isLengthPrefixedType(core::DataType type) -> bool
    {
        switch (type)
        {
            case core::DataType::CHAR:
            case core::DataType::VARCHAR:
            case core::DataType::TEXT:
            case core::DataType::JSON:
            case core::DataType::JSONB:
            case core::DataType::XML:
            case core::DataType::BINARY:
            case core::DataType::VARBINARY:
            case core::DataType::BLOB:
            case core::DataType::BYTEA:
            case core::DataType::VECTOR:
                return true;
            default:
                return false;
        }
    }

    StatisticsManager::StatisticsManager(Database *db)
        : db_(db),
          catalog_(nullptr),
          statistics_page_id_(0)
    {
        // The statistics_page_id_ will be set during initialization
        // For now, use a placeholder value that will be updated
        DEBUG_LOG_DB("StatisticsManager created");
    }

    StatisticsManager::~StatisticsManager()
    {
        DEBUG_LOG_DB("StatisticsManager destroyed");
    }

    auto StatisticsManager::analyzeTable(const ID &table_id, float sample_rate,
                                         ErrorContext *ctx) -> Status
    {
        return analyzeTableInternal(table_id, sample_rate, false, ctx);
    }

    auto StatisticsManager::analyzeTableInternal(const ID &table_id,
                                                 float sample_rate,
                                                 bool automatic,
                                                 ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Analyzing table");

        if (!catalog_)
        {
            catalog_ = db_->catalog_manager();
        }

        const auto started_at = std::chrono::steady_clock::now();

        std::vector<core::CatalogManager::ColumnInfo> columns;
        Status status = catalog_->getColumns(table_id, columns, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to get table columns");
            return status;
        }

        core::CatalogManager::TableInfo table_info;
        status = catalog_->getTable(table_id, table_info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to get table metadata");
            return status;
        }

        if (columns.empty())
        {
            DEBUG_LOG_DB("Table has no columns, skipping analysis");
            return Status::OK;
        }

        core::TableStatsSnapshot table_snapshot;
        const bool have_table_snapshot =
            db_ != nullptr &&
            db_->table_stats_manager() != nullptr &&
            db_->table_stats_manager()->snapshotForTable(table_id, table_snapshot);

        uint64_t table_row_estimate = table_info.row_count;
        if (table_row_estimate == 0 && have_table_snapshot &&
            table_snapshot.live_rows_estimate > 0)
        {
            table_row_estimate =
                static_cast<uint64_t>(table_snapshot.live_rows_estimate);
        }
        if (table_row_estimate == 0 && have_table_snapshot)
        {
            table_row_estimate = table_snapshot.rows_inserted +
                                 table_snapshot.rows_updated +
                                 table_snapshot.rows_deleted;
        }

        uint64_t sample_size = 0;
        if (sample_rate > 0.0f && sample_rate <= 1.0f)
        {
            if (table_row_estimate == 0)
            {
                if (sample_rate >= 0.999f)
                {
                    sample_size = std::numeric_limits<uint64_t>::max();
                }
                else
                {
                    constexpr uint64_t k_unknown_row_estimate_basis = 30000;
                    sample_size = static_cast<uint64_t>(
                        std::ceil(static_cast<double>(k_unknown_row_estimate_basis) *
                                  static_cast<double>(sample_rate)));
                }
            }
            else
            {
                sample_size = static_cast<uint64_t>(
                    std::ceil(static_cast<double>(table_row_estimate) *
                              static_cast<double>(sample_rate)));
            }
        }
        else
        {
            const uint64_t auto_row_basis = std::max<uint64_t>(1, table_row_estimate);
            sample_size = std::max<uint64_t>(64, auto_row_basis / 10);
            sample_size = std::min<uint64_t>(30000, sample_size);
            sample_rate = 0.0f;
        }

        if (table_row_estimate > 0)
        {
            sample_size = std::min<uint64_t>(sample_size, table_row_estimate);
        }
        sample_size = std::max<uint64_t>(1, sample_size);

        std::vector<std::vector<uint8_t>> sample_rows;
        status = sampleTable(table_id, sample_size, sample_rows, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to sample table");
            return status;
        }

        if (sample_rows.empty())
        {
            DEBUG_LOG_DB("No rows sampled, table may be empty");
            if (db_ && db_->table_stats_manager())
            {
                db_->table_stats_manager()->recordAnalyze(table_id, automatic, 0);
            }
            return Status::OK;
        }

        const uint64_t analyzed_time = static_cast<uint64_t>(std::time(nullptr));
        const double actual_sample_ratio =
            effectiveSampleRatio(table_row_estimate,
                                 sample_rows.size(),
                                 sample_rate);
        DEBUG_LOG_DB("Sampled " + std::to_string(sample_rows.size()) + " rows");

        for (const auto &column_info : columns)
        {
            DEBUG_LOG_DB("Analyzing column " + column_info.column_name);

            std::vector<std::vector<uint8_t>> column_values =
                extractColumnValues(table_id, column_info.column_id, sample_rows, columns, ctx);

            if (column_values.empty())
            {
                DEBUG_LOG_DB("Failed to extract column values for " + column_info.column_name);
                continue;
            }

            ColumnStatistics col_stats;
            col_stats.table_id = table_id;
            col_stats.column_id = column_info.column_id;
            col_stats.column_name = column_info.column_name;
            col_stats.data_type = static_cast<core::DataType>(column_info.data_type);
            applyColumnMetadata(column_info, col_stats);

            uint64_t null_count = 0;
            uint64_t total_width = 0;
            for (const auto &val : column_values)
            {
                if (val.empty())
                {
                    ++null_count;
                }
                else
                {
                    total_width += val.size();
                }
            }

            col_stats.num_rows = std::max<uint64_t>(table_row_estimate,
                                                    column_values.size());
            col_stats.num_nulls = null_count;
            col_stats.null_fraction =
                static_cast<float>(null_count) / static_cast<float>(column_values.size());

            const uint64_t non_null_count = column_values.size() - null_count;
            col_stats.avg_width = non_null_count > 0
                ? static_cast<float>(total_width) / static_cast<float>(non_null_count)
                : 0.0f;
            col_stats.num_distinct =
                estimateNDistinct(column_values, col_stats.num_rows, sample_rows.size());
            col_stats.last_analyzed_time = analyzed_time;
            col_stats.sample_size = sample_rows.size();
            col_stats.sample_rate = static_cast<float>(actual_sample_ratio);

            status = generateHistogram(column_values,
                                       100,
                                       HistogramType::EQUAL_HEIGHT,
                                       col_stats.data_type,
                                       col_stats.histogram_buckets,
                                       ctx);
            if (status == Status::OK)
            {
                col_stats.histogram_type = HistogramType::EQUAL_HEIGHT;
                col_stats.histogram_bucket_count =
                    static_cast<uint32_t>(col_stats.histogram_buckets.size());
            }

            status = identifyMCVs(column_values, 100, col_stats.mcv_list, ctx);
            if (status != Status::OK)
            {
                DEBUG_LOG_DB("Failed to identify MCVs for column " + column_info.column_name);
            }

            status = storeColumnStatistics(col_stats, ctx);
            if (status != Status::OK)
            {
                DEBUG_LOG_DB("Failed to store statistics for column " + column_info.column_name);
            }
        }

        computeCorrelationStatistics(table_id, columns, sample_rows, analyzed_time, ctx);
        computeExpressionStatistics(table_id, columns, sample_rows, analyzed_time, ctx);
        computeMultivariateStatistics(table_id,
                                      columns,
                                      sample_rows,
                                      analyzed_time,
                                      table_row_estimate,
                                      ctx);

        if (db_ && db_->table_stats_manager())
        {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started_at);
            db_->table_stats_manager()->recordAnalyze(
                table_id,
                automatic,
                static_cast<uint64_t>(std::max<int64_t>(0, elapsed.count())));
        }

        DEBUG_LOG_DB("Successfully analyzed table with " + std::to_string(columns.size()) + " columns");
        return Status::OK;
    }

    auto StatisticsManager::analyzeColumn(const ID &table_id, const ID &column_id,
                                           float sample_rate, ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Analyzing column " << column_id.toString() << " of table " << table_id.toString());

        // Initialize catalog manager if needed
        if (!catalog_)
        {
            catalog_ = db_->catalog_manager();
        }

        // Step 1: Get table columns to find target column
        std::vector<core::CatalogManager::ColumnInfo> columns;
        Status status = catalog_->getColumns(table_id, columns, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to get table columns");
            return status;
        }

        // Step 2: Find the target column
        bool found = false;
        size_t column_index = 0;
        for (size_t i = 0; i < columns.size(); ++i)
        {
            if (std::memcmp(columns[i].column_id.bytes.data(), column_id.bytes.data(), 16) == 0)
            {
                found = true;
                column_index = i;
                break;
            }
        }

        if (!found)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Column not found in table");
            return Status::NOT_FOUND;
        }

        core::CatalogManager::TableInfo table_info;
        status = catalog_->getTable(table_id, table_info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to get table metadata");
            return status;
        }

        core::TableStatsSnapshot table_snapshot;
        const bool have_table_snapshot =
            db_ != nullptr &&
            db_->table_stats_manager() != nullptr &&
            db_->table_stats_manager()->snapshotForTable(table_id, table_snapshot);
        uint64_t table_row_estimate = table_info.row_count;
        if (table_row_estimate == 0 && have_table_snapshot &&
            table_snapshot.live_rows_estimate > 0)
        {
            table_row_estimate =
                static_cast<uint64_t>(table_snapshot.live_rows_estimate);
        }

        // Step 3: Sample the table
        uint64_t sample_size = 0;
        if (sample_rate > 0.0f && sample_rate <= 1.0f)
        {
            if (table_row_estimate == 0)
            {
                if (sample_rate >= 0.999f)
                {
                    sample_size = std::numeric_limits<uint64_t>::max();
                }
                else
                {
                    constexpr uint64_t k_unknown_row_estimate_basis = 30000;
                    sample_size = static_cast<uint64_t>(
                        std::ceil(static_cast<double>(k_unknown_row_estimate_basis) *
                                  static_cast<double>(sample_rate)));
                }
            }
            else
            {
                sample_size = static_cast<uint64_t>(
                    std::ceil(static_cast<double>(table_row_estimate) *
                              static_cast<double>(sample_rate)));
            }
        }
        else
        {
            const uint64_t auto_row_basis = std::max<uint64_t>(1, table_row_estimate);
            sample_size = std::max<uint64_t>(64, auto_row_basis / 10);
            sample_size = std::min<uint64_t>(30000, sample_size);
            sample_rate = 0.0f;
        }
        if (table_row_estimate > 0)
        {
            sample_size = std::min<uint64_t>(sample_size, table_row_estimate);
        }
        sample_size = std::max<uint64_t>(1, sample_size);
        std::vector<std::vector<uint8_t>> sample_rows;

        status = sampleTable(table_id, sample_size, sample_rows, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to sample table");
            return status;
        }

        DEBUG_LOG_DB("Sampled " << sample_rows.size() << " rows for column analysis");

        // Step 4: Compute statistics for this column
        ColumnStatistics col_stats;
        status = computeColumnStats(table_id, column_id, sample_rows, col_stats, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to compute column statistics");
            return status;
        }

        col_stats.num_rows = std::max<uint64_t>(table_row_estimate, sample_rows.size());
        col_stats.sample_size = sample_rows.size();
        col_stats.sample_rate = static_cast<float>(
            effectiveSampleRatio(table_row_estimate, sample_rows.size(), sample_rate));

        // Step 5: Store statistics in cache
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            uint64_t cache_key = getCacheKey(table_id, column_id);
            column_stats_cache_[cache_key] = col_stats;
        }

        // Step 6: Persist to catalog
        status = storeColumnStatistics(col_stats, ctx);
        if (status != Status::OK)
        {
            DEBUG_LOG_DB("Warning: Failed to persist column statistics to catalog");
            // Don't fail the operation if persistence fails
        }

        DEBUG_LOG_DB("Column analysis complete: " << col_stats.num_distinct
                     << " distinct values, " << col_stats.num_nulls << " nulls");

        return Status::OK;
    }

    auto StatisticsManager::getColumnStatistics(const ID &table_id, const ID &column_id,
                                                 ColumnStatistics &stats,
                                                 ErrorContext *ctx) -> Status
    {
        AnalyzeLifecycleDecision analyze_decision;
        auto load_column_statistics = [&]() -> Status {
            {
                std::lock_guard<std::mutex> lock(cache_mutex_);
                uint64_t cache_key = getCacheKey(table_id, column_id);
                auto it = column_stats_cache_.find(cache_key);
                if (it != column_stats_cache_.end())
                {
                    stats = it->second;
                    applyDerivedMetadata(stats);
                    applyFreshnessMetadata(table_id,
                                           analyze_decision.triggered,
                                           analyze_decision.threshold,
                                           stats);
                    return Status::OK;
                }
            }

            Status status = loadColumnStatistics(table_id, column_id, stats, ctx);
            if (status == Status::OK)
            {
                applyDerivedMetadata(stats);
                applyFreshnessMetadata(table_id,
                                       analyze_decision.triggered,
                                       analyze_decision.threshold,
                                       stats);
                std::lock_guard<std::mutex> lock(cache_mutex_);
                column_stats_cache_[getCacheKey(table_id, column_id)] = stats;
            }
            return status;
        };

        Status auto_status = maybeAutoAnalyze(table_id, &analyze_decision, ctx);
        if (auto_status != Status::OK)
        {
            return auto_status;
        }

        Status status = load_column_statistics();
        if (status == Status::OK || status != Status::NOT_FOUND)
        {
            return status;
        }

        ErrorContext analyze_ctx;
        status = analyzeTableInternal(table_id, 0.10f, true, &analyze_ctx);
        if (status != Status::OK)
        {
            if (ctx != nullptr && ctx->message.empty())
            {
                ctx->message = analyze_ctx.message;
            }
            return status;
        }
        analyze_decision.triggered = true;
        return load_column_statistics();
    }

    auto StatisticsManager::getTableStatistics(const ID &table_id, TableStatistics &stats,
                                                ErrorContext *ctx) -> Status
    {
        AnalyzeLifecycleDecision analyze_decision;
        Status auto_status = maybeAutoAnalyze(table_id, &analyze_decision, ctx);
        if (auto_status != Status::OK)
        {
            return auto_status;
        }

        std::lock_guard<std::mutex> lock(cache_mutex_);

        // Check cache first
        uint64_t cache_key = 0;
        std::memcpy(&cache_key, table_id.bytes.data(), sizeof(uint64_t));

        auto it = table_stats_cache_.find(cache_key);
        if (it != table_stats_cache_.end())
        {
            stats = it->second;
            applyFreshnessMetadata(table_id,
                                   analyze_decision.triggered,
                                   analyze_decision.threshold,
                                   stats);
            return Status::OK;
        }

        // Cache miss - load from catalog
        if (!catalog_)
        {
            catalog_ = db_->catalog_manager();
        }

        // Get table information from catalog
        core::CatalogManager::TableInfo table_info;
        Status status = catalog_->getTable(table_id, table_info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to get table information");
            return status;
        }

        // Build table statistics from catalog info
        stats.table_id = table_id;
        stats.table_name = table_info.table_name;
        stats.num_rows = table_info.row_count;

        // OPT-M1: Estimate num_pages from row count and average row size
        // Since TableInfo doesn't track page count directly, we estimate it
        // by calculating average row size from column definitions
        constexpr uint64_t PAGE_SIZE = 8192;          // 8KB pages
        constexpr uint64_t TUPLE_HEADER_SIZE = 24;    // Per-tuple overhead
        constexpr double PAGE_FILL_FACTOR = 0.8;      // 80% fill factor

        // Get column info to estimate row size
        std::vector<core::CatalogManager::ColumnInfo> columns;
        auto col_status = catalog_->getColumns(table_id, columns, ctx);

        double estimated_row_size = TUPLE_HEADER_SIZE;
        if (col_status == core::Status::OK && !columns.empty())
        {
            for (const auto& col : columns)
            {
                // Estimate size per column based on data type
                switch (static_cast<core::DataType>(col.data_type))
                {
                    case core::DataType::BOOLEAN:
                    case core::DataType::INT8:
                        estimated_row_size += 1;
                        break;
                    case core::DataType::INT16:
                        estimated_row_size += 2;
                        break;
                    case core::DataType::INT32:
                    case core::DataType::FLOAT32:
                    case core::DataType::DATE:
                        estimated_row_size += 4;
                        break;
                    case core::DataType::INT64:
                    case core::DataType::FLOAT64:
                    case core::DataType::TIMESTAMP:
                    case core::DataType::TIME:
                    case core::DataType::INTERVAL:
                    case core::DataType::DECFLOAT16:
                        estimated_row_size += 8;
                        break;
                    case core::DataType::UUID:
                    case core::DataType::DECIMAL:
                    case core::DataType::DECFLOAT34:
                        estimated_row_size += 16;
                        break;
                    case core::DataType::VARCHAR:
                    case core::DataType::BYTEA:
                    case core::DataType::TEXT:
                    case core::DataType::JSON:
                    case core::DataType::JSONB:
                        // Variable-length types: estimate average 20 bytes
                        estimated_row_size += 24; // 4-byte length prefix + avg 20 bytes
                        break;
                    default:
                        estimated_row_size += 8; // Default estimate
                        break;
                }
            }
        }
        else
        {
            // Fallback: use 100 bytes per row if no column info
            estimated_row_size = 100;
        }

        stats.avg_row_size = static_cast<float>(estimated_row_size);

        // Calculate number of pages needed
        if (stats.num_rows > 0)
        {
            double usable_page_size = PAGE_SIZE * PAGE_FILL_FACTOR;
            double rows_per_page = usable_page_size / estimated_row_size;
            stats.num_pages = static_cast<uint64_t>(
                std::ceil(static_cast<double>(stats.num_rows) / rows_per_page));
            // Ensure at least 1 page for non-empty tables
            if (stats.num_pages == 0)
            {
                stats.num_pages = 1;
            }
        }
        else
        {
            stats.num_pages = 0;
        }

        core::TableStatsSnapshot snapshot;
        if (db_ != nullptr && db_->table_stats_manager() != nullptr &&
            db_->table_stats_manager()->snapshotForTable(table_id, snapshot))
        {
            const int64_t latest_analyze =
                std::max(snapshot.last_analyze_at, snapshot.last_autoanalyze_at);
            stats.last_analyzed_time = latest_analyze > 0
                ? static_cast<uint64_t>(latest_analyze / 1000000)
                : 0;
        }
        else
        {
            stats.last_analyzed_time = 0;
        }
        applyFreshnessMetadata(table_id,
                               analyze_decision.triggered,
                               analyze_decision.threshold,
                               stats);

        // Cache for future use
        table_stats_cache_[cache_key] = stats;

        DEBUG_LOG_DB("Loaded table statistics: " << stats.num_rows << " rows, "
                     << stats.num_pages << " pages, avg_size=" << stats.avg_row_size);

        return Status::OK;
    }

    auto StatisticsManager::analyzeIndex(const ID &index_id,
                                         float sample_rate,
                                         uint64_t refresh_xid,
                                         ErrorContext *ctx) -> Status
    {
        if (!catalog_)
        {
            catalog_ = db_ != nullptr ? db_->catalog_manager() : nullptr;
        }
        if (catalog_ == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Catalog manager not available for ANALYZE INDEX");
            return Status::INVALID_ARGUMENT;
        }

        core::CatalogManager::IndexInfo index_info;
        Status status = catalog_->getIndex(index_id, index_info, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        core::CatalogManager::IndexStatsCatalogInfo stats{};
        if (catalog_->getIndexStatsCatalogEntry(index_id, stats, nullptr) != Status::OK)
        {
            stats = core::CatalogManager::IndexStatsCatalogInfo{};
            stats.index_id = index_id;
        }

        core::CatalogManager::IndexStorageCatalogInfo storage{};
        const bool have_storage =
            catalog_->getIndexStorageCatalogEntry(index_id, storage, nullptr) == Status::OK;
        core::CatalogManager::IndexHealthCatalogInfo health{};
        const bool have_health =
            catalog_->getIndexHealthCatalogEntry(index_id, health, nullptr) == Status::OK;
        core::CatalogManager::IndexUsageCatalogInfo usage{};
        const bool have_usage =
            catalog_->getIndexUsageCatalogEntry(index_id, usage, nullptr) == Status::OK;

        core::CatalogManager::TableInfo table_info{};
        const bool have_table =
            catalog_->getTable(index_info.table_id, table_info, nullptr) == Status::OK;
        std::vector<core::CatalogManager::ColumnInfo> table_columns;
        const bool have_columns =
            have_table &&
            catalog_->getColumns(index_info.table_id, table_columns, nullptr) == Status::OK;

        const PlannerFamilyLoweringResult lowering = defaultMetricsLowering(index_info);
        const uint64_t current_xid = indexMetricsRefreshXid(db_, refresh_xid);
        const uint64_t row_count_basis =
            stats.row_count_est > 0
                ? stats.row_count_est
                : (have_table && table_info.row_count > 0
                       ? table_info.row_count
                       : (have_storage ? storage.page_count : 0));
        const double duplicate_density =
            row_count_basis > 0 && stats.distinct_count_est <= row_count_basis
                ? static_cast<double>(row_count_basis - stats.distinct_count_est) /
                      static_cast<double>(row_count_basis)
                : 0.0;
        const double coverage_fraction =
            have_columns && !table_columns.empty()
                ? std::min(
                      1.0,
                      static_cast<double>(index_info.column_ids.size() +
                                          index_info.include_column_ids.size()) /
                          static_cast<double>(table_columns.size()))
                : 0.0;

        std::vector<core::CatalogManager::IndexMaintenanceCatalogInfo> maintenance_rows;
        uint64_t maintenance_backlog_ops = 0;
        if (catalog_->listIndexMaintenanceCatalogEntries(index_id, maintenance_rows, nullptr) == Status::OK)
        {
            for (const auto &row : maintenance_rows)
            {
                if (row.maintenance_state != core::CatalogManager::IndexMaintenanceState::COMPLETE &&
                    row.maintenance_state != core::CatalogManager::IndexMaintenanceState::FAILED)
                {
                    ++maintenance_backlog_ops;
                    std::vector<core::CatalogManager::IndexMaintenanceDeltaCatalogInfo> deltas;
                    if (catalog_->listIndexMaintenanceDeltaCatalogEntries(row.maintenance_id,
                                                                         deltas,
                                                                         nullptr) == Status::OK)
                    {
                        maintenance_backlog_ops += static_cast<uint64_t>(deltas.size());
                    }
                }
            }
        }

        const IndexMetricsConfidenceClass confidence_class =
            classifyIndexMetricsConfidence(lowering,
                                           have_health ? &health : nullptr,
                                           true,
                                           sample_rate);
        IndexMetricsQueryabilityState queryability_state =
            effectiveMetricsQueryability(lowering,
                                         have_health ? &health : nullptr,
                                         confidence_class);
        const bool alias_surface =
            !index_info.alias_origin.empty() || isAliasSurface(index_info.index_type);
        if (alias_surface &&
            queryability_state == IndexMetricsQueryabilityState::QUERYABLE)
        {
            queryability_state = IndexMetricsQueryabilityState::LIMITED;
        }
        if (const auto catalog_state =
                parseCatalogQueryabilityState(index_info.queryability_state);
            catalog_state.has_value())
        {
            if (*catalog_state == IndexMetricsQueryabilityState::INVALID ||
                queryability_state == IndexMetricsQueryabilityState::INVALID)
            {
                queryability_state = IndexMetricsQueryabilityState::INVALID;
            }
            else if (*catalog_state == IndexMetricsQueryabilityState::LIMITED ||
                     queryability_state == IndexMetricsQueryabilityState::LIMITED)
            {
                queryability_state = IndexMetricsQueryabilityState::LIMITED;
            }
            else
            {
                queryability_state = *catalog_state;
            }
        }
        const char *runtime_family = runtimeIndexFamilyName(index_info.index_type);
        const char *native_metrics_mode = indexMetricsNativeMode(index_info.index_type);
        const char *semantic_contract_state =
            indexMetricsSemanticContractState(index_info.index_type);

        IndexFamilyMetricsPacket packet;
        packet.index_id = index_id;
        packet.physical_family = indexTypeName(index_info.index_type);
        packet.planner_family = lowering.family_name;
        packet.queryability_state = queryability_state;
        packet.metrics_publication_epoch = current_xid;
        packet.metrics_last_refresh_xid = current_xid;
        packet.metrics_confidence_class = confidence_class;
        packet.leaf_pages = have_storage ? storage.page_count : stats.leaf_pages;
        packet.height = stats.height == 0
                            ? static_cast<uint16_t>(packet.leaf_pages > 0 ? 1 : 0)
                            : stats.height;
        packet.row_count_est = row_count_basis;
        const double bloat_ratio = std::clamp(
            static_cast<double>(have_storage ? storage.fragmentation_ratio
                                             : stats.bloat_ratio),
            0.0,
            1.0);
        packet.dead_fraction = bloat_ratio;
        packet.live_entry_count_est = static_cast<uint64_t>(
            std::llround(static_cast<double>(packet.row_count_est) * (1.0 - packet.dead_fraction)));
        packet.bloat_ratio = bloat_ratio;
        packet.recheck_ratio_est = lowering.requires_recheck
            ? (lowering.family == PlannerAccessFamily::GIN_FILTER_SCAN ? 0.20 : 0.10)
            : 0.0;
        packet.correlation = stats.correlation;
        packet.coverage_fraction = coverage_fraction;
        packet.maintenance_backlog_ops = maintenance_backlog_ops;
        packet.publish_lag_xids =
            index_info.valid_from_xid > current_xid ? (index_info.valid_from_xid - current_xid) : 0;
        packet.reclaim_lag_xids =
            index_info.retired_xid > current_xid ? (index_info.retired_xid - current_xid) : 0;
        packet.metrics_freshness_class =
            classifyIndexMetricsFreshness(packet.queryability_state,
                                          packet.metrics_confidence_class,
                                          packet.publish_lag_xids,
                                          packet.maintenance_backlog_ops,
                                          packet.reclaim_lag_xids);
        packet.metrics_invalidation_state =
            classifyIndexMetricsInvalidationState(
                packet.metrics_freshness_class);
        packet.metrics_invalidation_reason =
            classifyIndexMetricsInvalidationReason(
                packet.queryability_state,
                packet.metrics_confidence_class,
                packet.publish_lag_xids,
                packet.maintenance_backlog_ops,
                packet.reclaim_lag_xids);
        packet.family_metrics_version =
            stats.family_metrics_version == std::numeric_limits<uint32_t>::max()
                ? stats.family_metrics_version
                : std::max<uint32_t>(1, stats.family_metrics_version + 1);
        packet.family_metrics_type = metricsTypeForLowering(lowering);

        nlohmann::json envelope = {
            {"index_uuid", index_id.toString()},
            {"physical_family", packet.physical_family},
            {"runtime_family", runtime_family},
            {"planner_family", packet.planner_family},
            {"alias_surface", alias_surface},
            {"native_metrics_mode", native_metrics_mode},
            {"semantic_contract_state", semantic_contract_state},
            {"requires_fail_closed_stronger_semantics", alias_surface},
            {"metrics_publication_epoch", packet.metrics_publication_epoch},
            {"queryability_state", indexMetricsQueryabilityStateName(packet.queryability_state)},
            {"metrics_last_refresh_xid", packet.metrics_last_refresh_xid},
            {"metrics_confidence_class", indexMetricsConfidenceClassName(packet.metrics_confidence_class)},
            {"freshness_class",
             indexMetricsFreshnessClassName(packet.metrics_freshness_class)},
            {"invalidation_state",
             indexMetricsInvalidationStateName(
                 packet.metrics_invalidation_state)},
            {"invalidation_reason", packet.metrics_invalidation_reason},
            {"leaf_pages", packet.leaf_pages},
            {"height", packet.height},
            {"row_count_est", packet.row_count_est},
            {"live_entry_count_est", packet.live_entry_count_est},
            {"dead_fraction", packet.dead_fraction},
            {"bloat_ratio", packet.bloat_ratio},
            {"recheck_ratio_est", packet.recheck_ratio_est},
            {"correlation", packet.correlation},
            {"coverage_fraction", packet.coverage_fraction},
            {"maintenance_backlog_ops", packet.maintenance_backlog_ops},
            {"publish_lag_xids", packet.publish_lag_xids},
            {"reclaim_lag_xids", packet.reclaim_lag_xids}
        };

        nlohmann::json family_metrics;
        switch (packet.family_metrics_type)
        {
            case IndexFamilyMetricsType::ORDERED_EXACT:
                family_metrics = {
                    {"avg_probe_pages", static_cast<double>(std::max<uint64_t>(1, packet.height))},
                    {"avg_range_pages_per_row",
                     packet.row_count_est > 0
                         ? static_cast<double>(packet.leaf_pages) /
                               static_cast<double>(packet.row_count_est)
                         : static_cast<double>(packet.leaf_pages)},
                    {"duplicate_density", duplicate_density},
                    {"prefix_selectivity",
                     stats.distinct_count_est > 0
                         ? 1.0 / static_cast<double>(stats.distinct_count_est)
                         : 1.0},
                    {"skip_group_count", stats.distinct_count_est},
                    {"prefetchable_page_fraction",
                     std::clamp(std::max(packet.correlation, 0.0), 0.0, 1.0)},
                    {"secondary_lookup_fraction",
                     std::clamp(1.0 - packet.coverage_fraction, 0.0, 1.0)},
                    {"cluster_locality_gain_est",
                     std::clamp(packet.correlation < 0.0
                                    ? -packet.correlation
                                    : packet.correlation,
                                0.0,
                                1.0)},
                    {"early_stop_gain_est",
                     std::clamp(std::max(packet.correlation, 0.0) *
                                    (1.0 - packet.recheck_ratio_est),
                                0.0,
                                1.0)},
                    {"overflow_chain_depth",
                     index_info.index_type == core::CatalogManager::IndexType::HASH
                         ? std::max<int>(0, static_cast<int>(packet.height) - 1)
                         : 0},
                    {"run_count",
                     index_info.index_type == core::CatalogManager::IndexType::LSM
                         ? std::max<uint16_t>(1, packet.height)
                         : 0},
                    {"level_count", index_info.index_type == core::CatalogManager::IndexType::LSM
                                        ? packet.height
                                        : 0},
                    {"tombstone_fraction", packet.dead_fraction},
                    {"L0_run_count",
                     index_info.index_type == core::CatalogManager::IndexType::LSM
                         ? 1
                         : 0},
                    {"sort_avoidance_gain_est",
                     std::clamp((packet.correlation < 0.0
                                     ? -packet.correlation
                                     : packet.correlation) *
                                    (1.0 - packet.dead_fraction),
                                0.0,
                                1.0)}
                };
                break;
            case IndexFamilyMetricsType::SUMMARY_CANDIDATE:
                family_metrics = {
                    {"pages_per_range", std::max<uint64_t>(1, packet.leaf_pages)},
                    {"prune_ratio_est", std::clamp(1.0 - packet.dead_fraction, 0.0, 1.0)},
                    {"unsummarized_range_fraction",
                     confidence_class == IndexMetricsConfidenceClass::LOW ? 0.25 : 0.0},
                    {"summary_staleness_fraction",
                     confidence_class == IndexMetricsConfidenceClass::MEDIUM ? 0.10
                                                                            : (confidence_class == IndexMetricsConfidenceClass::LOW ? 0.35 : 0.0)},
                    {"bitmap_density", packet.coverage_fraction},
                    {"bitmap_false_positive_ratio", packet.recheck_ratio_est},
                    {"lossy_container_fraction",
                     std::clamp(packet.recheck_ratio_est + (packet.dead_fraction * 0.25),
                                0.0,
                                1.0)},
                    {"column_bytes_pruned_ratio", std::clamp(1.0 - packet.coverage_fraction, 0.0, 1.0)},
                    {"row_groups_touched_ratio", std::clamp(packet.coverage_fraction, 0.0, 1.0)},
                    {"chunk_prune_ratio", std::clamp(1.0 - packet.coverage_fraction, 0.0, 1.0)},
                    {"projection_layout_count",
                     index_info.index_type == core::CatalogManager::IndexType::COLUMNSTORE
                         ? 2.0
                         : 1.0},
                    {"bulk_filter_gain_est",
                     std::clamp((1.0 - packet.coverage_fraction) *
                                    (1.0 - packet.recheck_ratio_est),
                                0.0,
                                1.0)},
                    {"mutable_buffer_fraction",
                     index_info.index_type == core::CatalogManager::IndexType::COLUMNSTORE
                         ? std::clamp((packet.dead_fraction * 0.50) +
                                          (packet.publish_lag_xids > 0 ? 0.20 : 0.0),
                                      0.0,
                                      1.0)
                         : 0.0},
                    {"projection_width_bytes",
                     index_info.index_type == core::CatalogManager::IndexType::COLUMNSTORE
                         ? 16.0
                         : 8.0},
                    {"delta_fraction",
                     index_info.index_type == core::CatalogManager::IndexType::COLUMNSTORE
                         ? std::clamp((packet.dead_fraction * 0.50) +
                                          (packet.publish_lag_xids > 0 ? 0.10 : 0.0),
                                      0.0,
                                      1.0)
                         : 0.0},
                    {"late_materialization_gain_est",
                     std::clamp(1.0 - packet.dead_fraction, 0.0, 1.0)}
                };
                break;
            case IndexFamilyMetricsType::GENERALIZED_SPATIAL:
                family_metrics = {
                    {"overlap_ratio", std::clamp(packet.recheck_ratio_est, 0.0, 1.0)},
                    {"penalty_growth_factor", 1.0 + packet.dead_fraction},
                    {"all_the_same_fraction", duplicate_density},
                    {"branch_skew", 1.0 - std::min(1.0, std::abs(packet.correlation))},
                    {"candidate_amplification", 1.0 + (packet.recheck_ratio_est * 4.0)},
                    {"nearest_lb_tightness", std::clamp(1.0 - packet.recheck_ratio_est, 0.0, 1.0)},
                    {"distance_recheck_ratio",
                     std::clamp(packet.recheck_ratio_est, 0.0, 1.0)}
                };
                break;
            case IndexFamilyMetricsType::TEXT_SEARCH:
                family_metrics = {
                    {"term_df", stats.distinct_count_est},
                    {"term_df_skew", duplicate_density},
                    {"avg_postings_per_term",
                     stats.distinct_count_est > 0
                         ? static_cast<double>(packet.row_count_est) /
                               static_cast<double>(stats.distinct_count_est)
                         : 0.0},
                    {"pending_list_fraction", packet.dead_fraction},
                    {"phrase_hit_rate", std::clamp(1.0 - packet.recheck_ratio_est, 0.0, 1.0)},
                    {"score_rows_est", packet.row_count_est},
                    {"merge_debt", packet.maintenance_backlog_ops},
                    {"stale_hit_ratio",
                     std::clamp(packet.dead_fraction +
                                    (packet.publish_lag_xids > 0 ? 0.10 : 0.0),
                                0.0,
                                1.0)},
                    {"recheck_ratio_est",
                     std::clamp(packet.recheck_ratio_est, 0.0, 1.0)},
                    {"collector_early_stop_gain",
                     std::clamp(1.0 - (packet.recheck_ratio_est * 0.75),
                                0.0,
                                1.0)},
                    {"mutable_overlay_fraction",
                     std::clamp((packet.dead_fraction * 0.50) +
                                    (packet.publish_lag_xids > 0 ? 0.15 : 0.0),
                                0.0,
                                1.0)}
                };
                break;
            case IndexFamilyMetricsType::ANN:
                family_metrics = {
                    {"vector_dim", 0},
                    {"candidate_budget_default",
                     std::max<uint64_t>(10, std::min<uint64_t>(1000,
                         static_cast<uint64_t>(std::sqrt(static_cast<double>(std::max<uint64_t>(1, packet.row_count_est))))))},
                    {"avg_candidates_scanned",
                     have_usage && usage.scan_count > 0
                         ? static_cast<double>(usage.tuple_read) /
                               static_cast<double>(usage.scan_count)
                         : 0.0},
                    {"deleted_node_fraction", packet.dead_fraction},
                    {"orphan_link_fraction",
                     have_health && health.pages_scanned > 0
                         ? static_cast<double>(health.orphan_pages) /
                               static_cast<double>(health.pages_scanned)
                         : 0.0},
                    {"recall_estimate_at_k",
                     confidence_class == IndexMetricsConfidenceClass::HIGH ? 0.95
                                                                          : (confidence_class == IndexMetricsConfidenceClass::MEDIUM ? 0.85 : 0.70)},
                    {"rerank_fraction", packet.recheck_ratio_est},
                    {"stale_training_fraction",
                     confidence_class == IndexMetricsConfidenceClass::LOW ? 0.25 : 0.0},
                    {"segment_coverage_fraction", packet.coverage_fraction},
                    {"bytes_per_live_vector",
                     std::max<double>(16.0, static_cast<double>(stats.avg_entry_len))},
                    {"growing_fraction",
                     std::clamp(packet.dead_fraction * 0.50, 0.0, 1.0)},
                    {"segment_merge_cost_est",
                     std::max(0.0,
                              (packet.coverage_fraction * 2.0) +
                                  (packet.recheck_ratio_est * 2.0) +
                                  packet.maintenance_backlog_ops)}
                };
                break;
            case IndexFamilyMetricsType::UNKNOWN:
            default:
                family_metrics = nlohmann::json::object();
                break;
        }

        nlohmann::json native_family_metrics =
            collectNativeIndexFamilyMetrics(catalog_, index_info);
        if (!native_family_metrics.empty())
        {
            family_metrics["native_runtime_metrics"] = std::move(native_family_metrics);
        }

        packet.family_metrics_payload =
            nlohmann::json{
                {"shared_metrics_envelope", envelope},
                {"family_metrics_type", indexFamilyMetricsTypeName(packet.family_metrics_type)},
                {"family_metrics", family_metrics}}
                .dump();

        stats.index_id = index_id;
        stats.stats_version =
            stats.stats_version == std::numeric_limits<uint32_t>::max()
                ? stats.stats_version
                : std::max<uint32_t>(1, stats.stats_version + 1);
        stats.last_analyze_txid = current_xid;
        stats.row_count_est = packet.row_count_est;
        if (sample_rate > 0.0f && sample_rate <= 1.0f && packet.row_count_est > 0)
        {
            const double sampled_rows =
                static_cast<double>(packet.row_count_est) * sample_rate;
            stats.distinct_count_est =
                static_cast<uint64_t>(std::max(1.0, sampled_rows));
        }
        else if (stats.distinct_count_est == 0)
        {
            stats.distinct_count_est = packet.live_entry_count_est > 0
                                           ? packet.live_entry_count_est
                                           : packet.row_count_est;
        }
        stats.leaf_pages = static_cast<uint32_t>(
            std::min<uint64_t>(packet.leaf_pages,
                               std::numeric_limits<uint32_t>::max()));
        stats.height = packet.height;
        stats.bloat_ratio = static_cast<float>(packet.bloat_ratio);
        stats.metrics_last_refresh_xid = packet.metrics_last_refresh_xid;
        stats.family_metrics_version = packet.family_metrics_version;
        stats.family_metrics_type = packet.family_metrics_type;
        stats.metrics_confidence_class = packet.metrics_confidence_class;
        stats.queryability_state = packet.queryability_state;
        stats.family_metrics_payload = packet.family_metrics_payload;
        stats.is_valid = true;

        status = catalog_->upsertIndexStatsCatalogEntry(stats, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            index_family_metrics_cache_[getIndexMetricsCacheKey(index_id)] = packet;
        }
        return Status::OK;
    }

    auto StatisticsManager::getIndexFamilyMetrics(const ID &index_id,
                                                  IndexFamilyMetricsPacket &packet,
                                                  ErrorContext *ctx) -> Status
    {
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = index_family_metrics_cache_.find(getIndexMetricsCacheKey(index_id));
            if (it != index_family_metrics_cache_.end())
            {
                packet = it->second;
                return Status::OK;
            }
        }

        Status status = loadIndexFamilyMetrics(index_id, packet, ctx);
        if (status == Status::OK)
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            index_family_metrics_cache_[getIndexMetricsCacheKey(index_id)] = packet;
        }
        return status;
    }

    auto StatisticsManager::refreshIndexFamilyMetrics(const ID &index_id,
                                                      uint64_t refresh_xid,
                                                      ErrorContext *ctx) -> Status
    {
        if (!catalog_)
        {
            catalog_ = db_ != nullptr ? db_->catalog_manager() : nullptr;
        }
        if (catalog_ == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Catalog manager not available for index metrics refresh");
            return Status::INVALID_ARGUMENT;
        }

        core::CatalogManager::IndexInfo index_info;
        Status status = catalog_->getIndex(index_id, index_info, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        core::CatalogManager::IndexStatsCatalogInfo stats{};
        status = catalog_->getIndexStatsCatalogEntry(index_id, stats, ctx);
        if (status == Status::NOT_FOUND)
        {
            stats = core::CatalogManager::IndexStatsCatalogInfo{};
            stats.index_id = index_id;
        }
        else if (status != Status::OK)
        {
            return status;
        }

        core::CatalogManager::IndexStorageCatalogInfo storage{};
        const bool have_storage =
            catalog_->getIndexStorageCatalogEntry(index_id, storage, nullptr) == Status::OK;
        core::CatalogManager::IndexHealthCatalogInfo health{};
        const bool have_health =
            catalog_->getIndexHealthCatalogEntry(index_id, health, nullptr) == Status::OK;
        IndexFamilyMetricsPacket existing_packet;
        const bool have_existing_packet =
            loadIndexFamilyMetrics(index_id, existing_packet, nullptr) == Status::OK;
        const PlannerFamilyLoweringResult lowering = defaultMetricsLowering(index_info);
        const uint64_t current_xid = indexMetricsRefreshXid(db_, refresh_xid);
        const IndexMetricsConfidenceClass confidence_class =
            classifyIndexMetricsConfidence(lowering,
                                           have_health ? &health : nullptr,
                                           false,
                                           0.0f);
        IndexMetricsQueryabilityState queryability_state =
            effectiveMetricsQueryability(lowering,
                                         have_health ? &health : nullptr,
                                         confidence_class);
        const bool alias_surface = isAliasSurface(index_info.index_type);
        if (alias_surface &&
            queryability_state == IndexMetricsQueryabilityState::QUERYABLE)
        {
            queryability_state = IndexMetricsQueryabilityState::LIMITED;
        }
        const char *runtime_family = runtimeIndexFamilyName(index_info.index_type);
        const char *native_metrics_mode = indexMetricsNativeMode(index_info.index_type);
        const char *semantic_contract_state =
            indexMetricsSemanticContractState(index_info.index_type);

        IndexFamilyMetricsPacket packet = have_existing_packet
            ? existing_packet
            : IndexFamilyMetricsPacket{};
        packet.index_id = index_id;
        packet.physical_family = canonicalPhysicalFamilyName(index_info);
        packet.planner_family = canonicalPlannerFamilyName(index_info, lowering);
        packet.queryability_state = queryability_state;
        packet.metrics_publication_epoch = current_xid;
        packet.metrics_last_refresh_xid = current_xid;
        packet.metrics_confidence_class = confidence_class;
        packet.leaf_pages = have_storage ? storage.page_count : stats.leaf_pages;
        packet.height = stats.height;
        packet.row_count_est = stats.row_count_est;
        const double bloat_ratio = std::clamp(
            static_cast<double>(have_storage ? storage.fragmentation_ratio
                                             : stats.bloat_ratio),
            0.0,
            1.0);
        packet.live_entry_count_est = static_cast<uint64_t>(
            std::llround(static_cast<double>(packet.row_count_est) *
                         (1.0 - bloat_ratio)));
        packet.dead_fraction = bloat_ratio;
        packet.bloat_ratio = bloat_ratio;
        packet.recheck_ratio_est = lowering.requires_recheck
            ? (lowering.family == PlannerAccessFamily::GIN_FILTER_SCAN ? 0.20 : 0.10)
            : 0.0;
        packet.correlation = stats.correlation;
        packet.coverage_fraction = have_existing_packet
            ? existing_packet.coverage_fraction
            : 0.0;
        packet.maintenance_backlog_ops = have_existing_packet
            ? existing_packet.maintenance_backlog_ops
            : 0;
        packet.publish_lag_xids =
            index_info.valid_from_xid > current_xid ? (index_info.valid_from_xid - current_xid) : 0;
        packet.reclaim_lag_xids =
            index_info.retired_xid > current_xid ? (index_info.retired_xid - current_xid) : 0;
        packet.metrics_freshness_class =
            classifyIndexMetricsFreshness(packet.queryability_state,
                                          packet.metrics_confidence_class,
                                          packet.publish_lag_xids,
                                          packet.maintenance_backlog_ops,
                                          packet.reclaim_lag_xids);
        packet.metrics_invalidation_state =
            classifyIndexMetricsInvalidationState(
                packet.metrics_freshness_class);
        packet.metrics_invalidation_reason =
            classifyIndexMetricsInvalidationReason(
                packet.queryability_state,
                packet.metrics_confidence_class,
                packet.publish_lag_xids,
                packet.maintenance_backlog_ops,
                packet.reclaim_lag_xids);
        packet.family_metrics_version =
            stats.family_metrics_version == std::numeric_limits<uint32_t>::max()
                ? stats.family_metrics_version
                : std::max<uint32_t>(1, stats.family_metrics_version + 1);
        packet.family_metrics_type = canonicalMetricsType(index_info, lowering);

        nlohmann::json payload = nlohmann::json::object();
        if (!packet.family_metrics_payload.empty())
        {
            payload = nlohmann::json::parse(packet.family_metrics_payload, nullptr, false);
            if (payload.is_discarded() || !payload.is_object())
            {
                payload = nlohmann::json::object();
            }
        }
        payload["shared_metrics_envelope"] = {
            {"index_uuid", index_id.toString()},
            {"physical_family", packet.physical_family},
            {"runtime_family", runtime_family},
            {"planner_family", packet.planner_family},
            {"alias_surface", alias_surface},
            {"native_metrics_mode", native_metrics_mode},
            {"semantic_contract_state", semantic_contract_state},
            {"requires_fail_closed_stronger_semantics", alias_surface},
            {"metrics_publication_epoch", packet.metrics_publication_epoch},
            {"queryability_state", indexMetricsQueryabilityStateName(packet.queryability_state)},
            {"metrics_last_refresh_xid", packet.metrics_last_refresh_xid},
            {"metrics_confidence_class", indexMetricsConfidenceClassName(packet.metrics_confidence_class)},
            {"freshness_class",
             indexMetricsFreshnessClassName(packet.metrics_freshness_class)},
            {"invalidation_state",
             indexMetricsInvalidationStateName(
                 packet.metrics_invalidation_state)},
            {"invalidation_reason", packet.metrics_invalidation_reason},
            {"leaf_pages", packet.leaf_pages},
            {"height", packet.height},
            {"row_count_est", packet.row_count_est},
            {"live_entry_count_est", packet.live_entry_count_est},
            {"dead_fraction", packet.dead_fraction},
            {"bloat_ratio", packet.bloat_ratio},
            {"recheck_ratio_est", packet.recheck_ratio_est},
            {"correlation", packet.correlation},
            {"coverage_fraction", packet.coverage_fraction},
            {"maintenance_backlog_ops", packet.maintenance_backlog_ops},
            {"publish_lag_xids", packet.publish_lag_xids},
            {"reclaim_lag_xids", packet.reclaim_lag_xids}
        };
        payload["family_metrics_type"] =
            indexFamilyMetricsTypeName(packet.family_metrics_type);
        if (!payload.contains("family_metrics"))
        {
            payload["family_metrics"] = nlohmann::json::object();
        }
        nlohmann::json native_family_metrics =
            collectNativeIndexFamilyMetrics(catalog_, index_info);
        if (!native_family_metrics.empty())
        {
            payload["family_metrics"]["native_runtime_metrics"] =
                std::move(native_family_metrics);
        }
        packet.family_metrics_payload = payload.dump();

        stats.metrics_last_refresh_xid = packet.metrics_last_refresh_xid;
        stats.family_metrics_version = packet.family_metrics_version;
        stats.family_metrics_type = packet.family_metrics_type;
        stats.metrics_confidence_class = packet.metrics_confidence_class;
        stats.queryability_state = packet.queryability_state;
        stats.family_metrics_payload = packet.family_metrics_payload;

        status = catalog_->upsertIndexStatsCatalogEntry(stats, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            index_family_metrics_cache_[getIndexMetricsCacheKey(index_id)] = packet;
        }
        return Status::OK;
    }

    auto StatisticsManager::loadIndexFamilyMetrics(const ID &index_id,
                                                   IndexFamilyMetricsPacket &packet,
                                                   ErrorContext *ctx) -> Status
    {
        if (!catalog_)
        {
            catalog_ = db_ != nullptr ? db_->catalog_manager() : nullptr;
        }
        if (catalog_ == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Catalog manager not available for index family metrics load");
            return Status::INVALID_ARGUMENT;
        }

        core::CatalogManager::IndexInfo index_info;
        Status status = catalog_->getIndex(index_id, index_info, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        core::CatalogManager::IndexStatsCatalogInfo stats{};
        status = catalog_->getIndexStatsCatalogEntry(index_id, stats, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        const PlannerFamilyLoweringResult lowering = defaultMetricsLowering(index_info);

        packet = IndexFamilyMetricsPacket{};
        packet.index_id = index_id;
        packet.physical_family = canonicalPhysicalFamilyName(index_info);
        packet.planner_family = canonicalPlannerFamilyName(index_info, lowering);
        packet.queryability_state =
            parseCatalogQueryabilityState(index_info.queryability_state)
                .value_or(stats.queryability_state);
        packet.metrics_publication_epoch = stats.metrics_last_refresh_xid;
        packet.metrics_last_refresh_xid = stats.metrics_last_refresh_xid;
        packet.metrics_confidence_class = stats.metrics_confidence_class;
        packet.leaf_pages = stats.leaf_pages;
        packet.height = stats.height;
        packet.row_count_est = stats.row_count_est;
        packet.bloat_ratio = stats.bloat_ratio;
        packet.correlation = stats.correlation;
        packet.family_metrics_version = stats.family_metrics_version;
        packet.family_metrics_type =
            stats.family_metrics_type == IndexFamilyMetricsType::UNKNOWN
                ? canonicalMetricsType(index_info, lowering)
                : stats.family_metrics_type;
        packet.family_metrics_payload = stats.family_metrics_payload;

        if (packet.family_metrics_version == 0 ||
            packet.family_metrics_payload.empty())
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::NOT_FOUND,
                              "index family metrics packet not published");
            return Status::NOT_FOUND;
        }

        if (!packet.family_metrics_payload.empty())
        {
            nlohmann::json payload = nlohmann::json::parse(packet.family_metrics_payload,
                                                           nullptr,
                                                           false);
            if (!payload.is_discarded())
            {
                const auto envelope_it = payload.find("shared_metrics_envelope");
                if (envelope_it != payload.end() && envelope_it->is_object())
                {
                    const auto &envelope = *envelope_it;
                    packet.live_entry_count_est =
                        envelope.value("live_entry_count_est", packet.live_entry_count_est);
                    packet.dead_fraction =
                        envelope.value("dead_fraction", packet.dead_fraction);
                    packet.recheck_ratio_est =
                        envelope.value("recheck_ratio_est", packet.recheck_ratio_est);
                    packet.coverage_fraction =
                        envelope.value("coverage_fraction", packet.coverage_fraction);
                    packet.maintenance_backlog_ops =
                        envelope.value("maintenance_backlog_ops",
                                       packet.maintenance_backlog_ops);
                    packet.publish_lag_xids =
                        envelope.value("publish_lag_xids", packet.publish_lag_xids);
                    packet.reclaim_lag_xids =
                        envelope.value("reclaim_lag_xids", packet.reclaim_lag_xids);
                    packet.physical_family =
                        envelope.value("physical_family", packet.physical_family);
                    packet.planner_family =
                        envelope.value("planner_family", packet.planner_family);
                    packet.metrics_publication_epoch =
                        envelope.value("metrics_publication_epoch",
                                       packet.metrics_publication_epoch);
                    packet.metrics_freshness_class =
                        indexMetricsFreshnessClassFromName(
                            envelope.value("freshness_class",
                                           std::string()));
                    packet.metrics_invalidation_state =
                        indexMetricsInvalidationStateFromName(
                            envelope.value("invalidation_state",
                                           std::string()));
                    packet.metrics_invalidation_reason =
                        envelope.value("invalidation_reason", std::string());
                }
            }
        }

        if (packet.metrics_publication_epoch == 0)
        {
            packet.metrics_publication_epoch = packet.metrics_last_refresh_xid;
        }
        if (packet.metrics_freshness_class ==
            IndexMetricsFreshnessClass::UNKNOWN)
        {
            packet.metrics_freshness_class =
                classifyIndexMetricsFreshness(packet.queryability_state,
                                              packet.metrics_confidence_class,
                                              packet.publish_lag_xids,
                                              packet.maintenance_backlog_ops,
                                              packet.reclaim_lag_xids);
        }
        if (packet.metrics_invalidation_state ==
            IndexMetricsInvalidationState::UNKNOWN)
        {
            packet.metrics_invalidation_state =
                classifyIndexMetricsInvalidationState(
                    packet.metrics_freshness_class);
        }
        if (packet.metrics_invalidation_reason.empty() &&
            packet.metrics_invalidation_state !=
                IndexMetricsInvalidationState::VALID)
        {
            packet.metrics_invalidation_reason =
                classifyIndexMetricsInvalidationReason(
                    packet.queryability_state,
                    packet.metrics_confidence_class,
                    packet.publish_lag_xids,
                    packet.maintenance_backlog_ops,
                    packet.reclaim_lag_xids);
        }

        return Status::OK;
    }

    auto StatisticsManager::getColumnCorrelation(const ID &table_id,
                                                 const ID &left_column_id,
                                                 const ID &right_column_id,
                                                 ColumnCorrelationStatistics &stats_out,
                                                 ErrorContext *ctx) -> Status
    {
        const auto key = getCorrelationCacheKey(table_id, left_column_id, right_column_id);
        AnalyzeLifecycleDecision analyze_decision;
        Status auto_status = maybeAutoAnalyze(table_id, &analyze_decision, ctx);
        if (auto_status != Status::OK)
        {
            return auto_status;
        }

        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = correlation_stats_cache_.find(key);
            if (it != correlation_stats_cache_.end())
            {
                stats_out = it->second;
                return Status::OK;
            }
        }

        Status status =
            loadCorrelationStatistic(table_id, left_column_id, right_column_id, stats_out, ctx);
        if (status == Status::OK)
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            correlation_stats_cache_[key] = stats_out;
            return Status::OK;
        }
        if (status != Status::NOT_FOUND)
        {
            return status;
        }

        ErrorContext analyze_ctx;
        status = analyzeTableInternal(table_id, 0.10f, true, &analyze_ctx);
        if (status != Status::OK)
        {
            if (ctx != nullptr && ctx->message.empty())
            {
                ctx->message = analyze_ctx.message;
            }
            return status;
        }
        analyze_decision.triggered = true;

        status =
            loadCorrelationStatistic(table_id, left_column_id, right_column_id, stats_out, ctx);
        if (status == Status::OK)
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            correlation_stats_cache_[key] = stats_out;
        }
        return status;
    }

    auto StatisticsManager::getExpressionStatistics(const ID &table_id,
                                                    const std::string &expression_key,
                                                    ExpressionStatistics &stats_out,
                                                    ErrorContext *ctx) -> Status
    {
        ExpressionStatsDescriptor descriptor;
        const bool have_descriptor =
            parseExpressionStatsKey(expression_key, descriptor);
        const std::string canonical_expression_key =
            have_descriptor ? canonicalExpressionStatsKey(descriptor)
                            : core::IdentifierUtils::toUpper(expression_key);
        const auto key =
            getExpressionCacheKey(table_id, canonical_expression_key);
        AnalyzeLifecycleDecision analyze_decision;
        Status auto_status = maybeAutoAnalyze(table_id, &analyze_decision, ctx);
        if (auto_status != Status::OK)
        {
            return auto_status;
        }

        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = expression_stats_cache_.find(key);
            if (it != expression_stats_cache_.end())
            {
                stats_out = it->second;
                return Status::OK;
            }
        }

        ColumnStatistics stored_stats;
        auto load_expression_stats = [&](const std::string &cache_key_value)
            -> Status {
                const ID synthetic_column_id =
                    makeSyntheticStatisticId(table_id, "EXPR", cache_key_value);
                return loadColumnStatistics(table_id,
                                            synthetic_column_id,
                                            stored_stats,
                                            ctx);
            };

        Status status = load_expression_stats(key);
        const std::string legacy_expression_key =
            have_descriptor ? legacyExpressionStatsKey(descriptor) : std::string();
        const std::string legacy_cache_key =
            legacy_expression_key.empty()
                ? std::string()
                : getExpressionCacheKey(table_id, legacy_expression_key);
        if (status == Status::NOT_FOUND && !legacy_cache_key.empty() &&
            legacy_cache_key != key)
        {
            status = load_expression_stats(legacy_cache_key);
        }
        if (status != Status::OK && status == Status::NOT_FOUND)
        {
            ErrorContext analyze_ctx;
            status = analyzeTableInternal(table_id, 0.10f, true, &analyze_ctx);
            if (status != Status::OK)
            {
                if (ctx != nullptr && ctx->message.empty())
                {
                    ctx->message = analyze_ctx.message;
                }
                return status;
            }
            analyze_decision.triggered = true;
            status = load_expression_stats(key);
            if (status == Status::NOT_FOUND && !legacy_cache_key.empty() &&
                legacy_cache_key != key)
            {
                status = load_expression_stats(legacy_cache_key);
            }
        }
        if (status != Status::OK)
        {
            return status;
        }

        stats_out.table_id = table_id;
        stats_out.expression_key = canonical_expression_key;
        stats_out.expression_contract_id =
            have_descriptor ? descriptor.contract_id : kExpressionStatsContractId;
        stats_out.registry_contract_id =
            have_descriptor ? descriptor.registry_contract_id
                            : kExpressionStatsRegistryContractId;
        stats_out.coverage_contract_id =
            have_descriptor ? descriptor.coverage_contract_id
                            : kExpressionStatsCoverageContractId;
        stats_out.function_name =
            have_descriptor ? descriptor.function_name : std::string();
        stats_out.base_column_name =
            have_descriptor ? descriptor.column_name : std::string();
        stats_out.coverage_class =
            have_descriptor ? descriptor.coverage_class : "UNSUPPORTED";
        stats_out.input_data_type = stored_stats.data_type;
        stats_out.result_data_type =
            have_descriptor && descriptor.result_data_type != core::DataType::UNKNOWN
                ? descriptor.result_data_type
                : stored_stats.data_type;
        stored_stats.column_name = canonical_expression_key;
        applyFreshnessMetadata(table_id,
                               analyze_decision.triggered,
                               analyze_decision.threshold,
                               stored_stats);
        stats_out.stats = std::move(stored_stats);
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            expression_stats_cache_[key] = stats_out;
        }
        return Status::OK;
    }

    auto StatisticsManager::getMultivariateStatistics(
        const ID &table_id,
        const std::vector<ID> &column_ids,
        MultivariateStatistics &stats_out,
        ErrorContext *ctx) -> Status
    {
        if (column_ids.size() < 2)
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::INVALID_ARGUMENT,
                              "multivariate statistics require at least two columns");
            return Status::INVALID_ARGUMENT;
        }

        const auto key = getMultivariateCacheKey(table_id, column_ids);
        AnalyzeLifecycleDecision analyze_decision;
        Status auto_status = maybeAutoAnalyze(table_id, &analyze_decision, ctx);
        if (auto_status != Status::OK)
        {
            return auto_status;
        }

        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            const auto it = multivariate_stats_cache_.find(key);
            if (it != multivariate_stats_cache_.end())
            {
                stats_out = it->second;
                return Status::OK;
            }
        }

        Status status = loadMultivariateStatistics(table_id, column_ids, stats_out, ctx);
        if (status == Status::OK)
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            multivariate_stats_cache_[key] = stats_out;
            return Status::OK;
        }
        if (status != Status::NOT_FOUND)
        {
            return status;
        }

        ErrorContext analyze_ctx;
        status = analyzeTableInternal(table_id, 0.10f, true, &analyze_ctx);
        if (status != Status::OK)
        {
            if (ctx != nullptr && ctx->message.empty())
            {
                ctx->message = analyze_ctx.message;
            }
            return status;
        }

        status = loadMultivariateStatistics(table_id, column_ids, stats_out, ctx);
        if (status == Status::OK)
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            multivariate_stats_cache_[key] = stats_out;
        }
        return status;
    }

    auto StatisticsManager::dropStatistics(const ID &table_id, ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Dropping statistics for table " << table_id.toString());

        // Step 1: Clear from cache
        invalidateCache(table_id);

        {
            std::lock_guard<std::mutex> lock(cache_mutex_);

            // Remove table-level statistics
            uint64_t table_cache_key = 0;
            std::memcpy(&table_cache_key, table_id.bytes.data(), sizeof(uint64_t));
            table_stats_cache_.erase(table_cache_key);

            // Remove all column-level statistics for this table
            // We need to iterate through all cached columns and remove those belonging to this table
            std::vector<uint64_t> keys_to_remove;
            for (const auto& [key, col_stats] : column_stats_cache_)
            {
                if (std::memcmp(col_stats.table_id.bytes.data(), table_id.bytes.data(), 16) == 0)
                {
                    keys_to_remove.push_back(key);
                }
            }

            for (uint64_t key : keys_to_remove)
            {
                column_stats_cache_.erase(key);
            }

            DEBUG_LOG_DB("Removed " << keys_to_remove.size() << " column statistics from cache");
        }

        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            for (auto it = correlation_stats_cache_.begin();
                 it != correlation_stats_cache_.end();)
            {
                if (it->second.table_id == table_id)
                {
                    it = correlation_stats_cache_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            for (auto it = expression_stats_cache_.begin();
                 it != expression_stats_cache_.end();)
            {
                if (it->second.table_id == table_id)
                {
                    it = expression_stats_cache_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            for (auto it = multivariate_stats_cache_.begin();
                 it != multivariate_stats_cache_.end();)
            {
                if (it->second.table_id == table_id)
                {
                    it = multivariate_stats_cache_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        if (!catalog_)
        {
            catalog_ = db_->catalog_manager();
        }
        if (catalog_ != nullptr)
        {
            Status delete_status = catalog_->deleteStatisticsForTable(table_id, ctx);
            if (delete_status != Status::OK)
            {
                return delete_status;
            }
        }

        DEBUG_LOG_DB("Statistics dropped successfully for table");

        return Status::OK;
    }

    auto StatisticsManager::invalidateCache(const ID &table_id) -> void
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);

        if (std::all_of(table_id.bytes.begin(), table_id.bytes.end(),
                        [](uint8_t b) { return b == 0; }))
        {
            // Clear entire cache if table_id is zero
            column_stats_cache_.clear();
            table_stats_cache_.clear();
            correlation_stats_cache_.clear();
            expression_stats_cache_.clear();
            multivariate_stats_cache_.clear();
            index_family_metrics_cache_.clear();
            DEBUG_LOG_DB("Cleared entire statistics cache");
        }
        else
        {
            // OPT-L1: Targeted invalidation - remove only entries for this specific table
            // This preserves statistics for other tables, improving efficiency

            // Remove table-level statistics
            uint64_t table_cache_key = 0;
            std::memcpy(&table_cache_key, table_id.bytes.data(), sizeof(uint64_t));
            auto table_it = table_stats_cache_.find(table_cache_key);
            if (table_it != table_stats_cache_.end())
            {
                table_stats_cache_.erase(table_it);
            }

            // Remove column-level statistics for this table
            // The cache key is table_key XOR column_key, so we need to check each entry
            // to see if its table_id matches
            std::vector<uint64_t> keys_to_remove;
            for (const auto& [cache_key, stats] : column_stats_cache_)
            {
                if (std::memcmp(stats.table_id.bytes.data(), table_id.bytes.data(), 16) == 0)
                {
                    keys_to_remove.push_back(cache_key);
                }
            }

            for (uint64_t key : keys_to_remove)
            {
                column_stats_cache_.erase(key);
            }

            for (auto it = correlation_stats_cache_.begin();
                 it != correlation_stats_cache_.end();)
            {
                if (it->second.table_id == table_id)
                {
                    it = correlation_stats_cache_.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            for (auto it = expression_stats_cache_.begin();
                 it != expression_stats_cache_.end();)
            {
                if (it->second.table_id == table_id)
                {
                    it = expression_stats_cache_.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            for (auto it = multivariate_stats_cache_.begin();
                 it != multivariate_stats_cache_.end();)
            {
                if (it->second.table_id == table_id)
                {
                    it = multivariate_stats_cache_.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            // Index family metrics cache is conservative for now: table-level
            // invalidation drops all index packets because index->table ownership
            // is not recorded in the cache key.
            index_family_metrics_cache_.clear();

            DEBUG_LOG_DB("Invalidated statistics cache for table: removed " +
                         std::to_string(keys_to_remove.size()) + " column entries");
        }
    }

    // -------------------------------------------------------------------------
    // Private Helper Methods
    // -------------------------------------------------------------------------

    auto StatisticsManager::sampleTable(const ID &table_id, uint64_t sample_size,
                                         std::vector<std::vector<uint8_t>> &sample_rows,
                                         ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Sampling table using Vitter's Algorithm S");

        // Phase 1, Task 1.1.3 - Vitter's Algorithm S (Reservoir Sampling)
        //
        // Reference: Vitter, J. S. (1985). Random Sampling with a Reservoir.
        // ACM Transactions on Mathematical Software, 11(1), 37-57.
        //
        // This algorithm provides uniform random sampling of large datasets
        // with a single pass and O(sample_size) memory.
        //
        // Time Complexity: O(n * (1 + log(N/n))) - nearly linear
        // Space Complexity: O(n) - only reservoir in memory
        // Uniformity: Each row has exactly n/N probability of selection

        sample_rows.clear();

        if (sample_size == 0)
        {
            return Status::OK; // Nothing to sample
        }

        // Create a sequential scan iterator for the table
        auto iterator = db_->storage_engine()->createScan(table_id, ctx);
        if (!iterator)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                              "Failed to create table scan iterator");
            return Status::IO_ERROR;
        }

        // Initialize random number generator
        // SECURITY FIX (LOW-8): Use OpenSSL RAND_bytes for cryptographically secure seed
        uint64_t seed;
#ifdef HAVE_OPENSSL_RAND
        unsigned char seed_bytes[8];
        if (RAND_bytes(seed_bytes, sizeof(seed_bytes)) == 1)
        {
            // Use OpenSSL's cryptographically secure random for seed
            std::memcpy(&seed, seed_bytes, sizeof(seed));
            LOG_DEBUG(GENERAL, "Using OpenSSL RAND_bytes for statistics sampling seed");
        }
        else
        {
            // OpenSSL failed, fall back to random_device
            LOG_WARNING(GENERAL, "OpenSSL RAND_bytes failed, falling back to random_device");
            std::random_device rd;
            seed = (static_cast<uint64_t>(rd()) << 32) | rd();
        }
#else
        // OpenSSL not available, use random_device with entropy check
        std::random_device rd;
        if (rd.entropy() == 0.0)
        {
            // Fallback to time-based seed if random_device has zero entropy
            LOG_WARNING(GENERAL, "random_device has zero entropy, using time-based seed for statistics sampling");
            seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        }
        else
        {
            // Use random_device for seed
            seed = (static_cast<uint64_t>(rd()) << 32) | rd();
        }
#endif

        std::mt19937 gen(seed);
        std::uniform_real_distribution<double> uniform_real(0.0, 1.0);

        // Phase 1: Fill reservoir with first sample_size rows
        uint64_t rows_read = 0;
        Tuple tuple;

        while (rows_read < sample_size && !iterator->isDone())
        {
            Status status = iterator->next(&tuple, ctx);
            if (status != Status::OK)
            {
                if (status == Status::NOT_FOUND)
                {
                    break; // No more rows
                }
                SET_ERROR_CONTEXT(ctx, status, "Failed to read tuple during sampling");
                return status;
            }

            // Copy tuple data into sample
            std::vector<uint8_t> row_data(tuple.data, tuple.data + tuple.data_size);
            sample_rows.push_back(std::move(row_data));
            rows_read++;
        }

        DEBUG_LOG_DB("Filled reservoir with " + std::to_string(rows_read) + " rows");

        // If we read fewer rows than sample size, we're done
        if (rows_read < sample_size)
        {
            DEBUG_LOG_DB("Table has fewer rows than sample size");
            return Status::OK;
        }

        // Phase 2: Geometric skipping (Vitter's Algorithm S)
        // For each subsequent row, use geometric distribution to skip rows
        // and randomly replace items in the reservoir

        double W = std::exp(std::log(uniform_real(gen)) / static_cast<double>(sample_size));

        while (!iterator->isDone())
        {
            // Calculate number of rows to skip according to geometric distribution
            uint64_t skip = static_cast<uint64_t>(
                std::floor(std::log(uniform_real(gen)) / std::log(1.0 - W)));

            // Skip the calculated number of rows
            for (uint64_t i = 0; i <= skip && !iterator->isDone(); i++)
            {
                Status status = iterator->next(&tuple, ctx);
                if (status == Status::NOT_FOUND)
                {
                    // Reached end of table
                    DEBUG_LOG_DB("Completed sampling: " + std::to_string(rows_read) +
                                 " rows scanned, " + std::to_string(sample_rows.size()) +
                                 " rows in sample");
                    return Status::OK;
                }
                if (status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, status, "Failed to read tuple during sampling");
                    return status;
                }
                rows_read++;
            }

            if (iterator->isDone())
            {
                break;
            }

            // Replace a random item in the reservoir with the current row
            std::uniform_int_distribution<uint64_t> uniform_int(0, sample_size - 1);
            uint64_t replace_idx = uniform_int(gen);

            std::vector<uint8_t> row_data(tuple.data, tuple.data + tuple.data_size);
            sample_rows[replace_idx] = std::move(row_data);

            // Update W for next iteration
            W = W * std::exp(std::log(uniform_real(gen)) / static_cast<double>(sample_size));
        }

        DEBUG_LOG_DB("Completed sampling: " + std::to_string(rows_read) +
                     " rows scanned, " + std::to_string(sample_rows.size()) + " rows in sample");

        return Status::OK;
    }

    auto StatisticsManager::computeColumnStats(const ID &table_id, const ID &column_id,
                                                const std::vector<std::vector<uint8_t>> &sample_rows,
                                                ColumnStatistics &stats,
                                                ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Computing column statistics from sample");

        // Phase 1, Task 1.1.4 - Column statistics computation
        //
        // Steps:
        // 1. Get table schema from catalog
        // 2. Extract column values from sample_rows
        // 3. Compute null_fraction (count NULLs / total)
        // 4. Estimate n_distinct (using exact count or HyperLogLog)
        // 5. Compute avg_width (average bytes per value)
        // 6. Identify Most Common Values (top 100 by frequency)
        // 7. Generate histogram (equal-height or equal-width)
        // 8. Set metadata (last_analyzed_time, sample_size, sample_rate)

        if (sample_rows.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Empty sample");
            return Status::INVALID_ARGUMENT;
        }

        // Get table and column information from catalog
        if (!catalog_)
        {
            catalog_ = db_->catalog_manager();
        }

        std::vector<core::CatalogManager::ColumnInfo> columns;
        Status status = catalog_->getColumns(table_id, columns, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to get table columns");
            return status;
        }

        // Find the target column
        size_t target_column_idx = SIZE_MAX;
        core::CatalogManager::ColumnInfo target_column_info;

        for (size_t i = 0; i < columns.size(); i++)
        {
            if (std::memcmp(columns[i].column_id.bytes.data(), column_id.bytes.data(), 16) == 0)
            {
                target_column_idx = i;
                target_column_info = columns[i];
                break;
            }
        }

        if (target_column_idx == SIZE_MAX)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Column not found in table");
            return Status::NOT_FOUND;
        }

        // Extract column values from sample rows
        std::vector<std::vector<uint8_t>> column_values;
        column_values.reserve(sample_rows.size());
        uint64_t null_count = 0;
        uint64_t total_width = 0;

        for (const auto &tuple_data : sample_rows)
        {
            if (tuple_data.size() < sizeof(core::TupleHeader))
            {
                continue; // Skip malformed tuples
            }

            // Read TupleHeader
            const auto *header = reinterpret_cast<const core::TupleHeader *>(tuple_data.data());

            // Get null bitmap if present
            const uint8_t *null_bitmap = nullptr;
            if (header->hasNulls() && header->null_bitmap_offset > 0 &&
                header->null_bitmap_offset < tuple_data.size())
            {
                null_bitmap = tuple_data.data() + header->null_bitmap_offset;
            }

            // Check if our target column is null
            bool is_null = false;
            if (null_bitmap)
            {
                size_t byte_offset = target_column_idx / 8;
                size_t bit_pos = target_column_idx % 8;
                is_null = (null_bitmap[byte_offset] & (1 << bit_pos)) != 0;
            }

            if (is_null)
            {
                null_count++;
                column_values.push_back(std::vector<uint8_t>()); // Empty vector for NULL
                continue;
            }

            // Calculate data offset for our target column
            size_t data_offset = sizeof(core::TupleHeader);
            if (header->hasNulls() && null_bitmap)
            {
                size_t bitmap_bytes = (columns.size() + 7) / 8;
                data_offset = header->null_bitmap_offset + bitmap_bytes;
            }

            bool malformed = false;
            core::DomainManager *domain_mgr = db_->domain_manager();

            // Skip columns before our target
            for (size_t i = 0; i < target_column_idx; i++)
            {
                if (null_bitmap)
                {
                    size_t byte_offset = i / 8;
                    size_t bit_pos = i % 8;
                    if (null_bitmap[byte_offset] & (1 << bit_pos))
                    {
                        continue; // Null column, no data to skip
                    }
                }

                bool encrypted = false;
                core::ErrorContext enc_ctx;
                if (!resolveColumnEncryption(domain_mgr, columns[i], encrypted, &enc_ctx))
                {
                    malformed = true;
                    break;
                }

                size_t col_size = 0;
                if (encrypted)
                {
                    size_t len_offset = data_offset;
                    uint32_t len = 0;
                    if (!core::readUint32LE(tuple_data.data(), tuple_data.size(), len_offset, len))
                    {
                        malformed = true;
                        break;
                    }
                    col_size = sizeof(uint32_t) + len;
                }
                else
                {
                    core::TypeInfo type_info = buildTypeInfo(columns[i]);
                    core::ErrorContext size_ctx;
                    core::Status size_status = core::computePlainValueSize(type_info.type,
                                                                           type_info,
                                                                           tuple_data.data() + data_offset,
                                                                           tuple_data.size() - data_offset,
                                                                           col_size,
                                                                           &size_ctx);
                    if (size_status != core::Status::OK)
                    {
                        malformed = true;
                        break;
                    }
                }

                if (data_offset + col_size > tuple_data.size())
                {
                    malformed = true;
                    break;
                }
                data_offset += col_size;
            }

            if (malformed)
            {
                continue;
            }

            // Extract the target column value
            core::DataType target_type = static_cast<core::DataType>(target_column_info.data_type);
            std::vector<uint8_t> value;
            size_t value_size = 0;
            size_t payload_width = 0;

            bool target_encrypted = false;
            core::ErrorContext enc_ctx;
            if (!resolveColumnEncryption(domain_mgr, target_column_info, target_encrypted, &enc_ctx))
            {
                continue;
            }

            if (target_encrypted)
            {
                size_t len_offset = data_offset;
                uint32_t len = 0;
                if (!core::readUint32LE(tuple_data.data(), tuple_data.size(), len_offset, len))
                {
                    continue;
                }
                value_size = sizeof(uint32_t) + len;
                payload_width = len;
            }
            else
            {
                core::TypeInfo type_info = buildTypeInfo(target_column_info);
                core::ErrorContext size_ctx;
                core::Status size_status = core::computePlainValueSize(type_info.type,
                                                                       type_info,
                                                                       tuple_data.data() + data_offset,
                                                                       tuple_data.size() - data_offset,
                                                                       value_size,
                                                                       &size_ctx);
                if (size_status != core::Status::OK)
                {
                    continue;
                }
                if (isLengthPrefixedType(target_type))
                {
                    size_t len_offset = data_offset;
                    uint32_t len = 0;
                    if (!core::readUint32LE(tuple_data.data(), tuple_data.size(), len_offset, len))
                    {
                        continue;
                    }
                    payload_width = len;
                }
                else
                {
                    payload_width = value_size;
                }
            }

            if (data_offset + value_size > tuple_data.size())
            {
                continue;
            }

            value.resize(value_size);
            std::memcpy(value.data(), tuple_data.data() + data_offset, value_size);
            total_width += payload_width;

            column_values.push_back(std::move(value));
        }

        // Compute statistics
        stats.table_id = table_id;
        stats.column_id = column_id;
        stats.column_name = target_column_info.column_name;
        stats.data_type = static_cast<core::DataType>(target_column_info.data_type);
        applyColumnMetadata(target_column_info, stats);

        stats.num_rows = sample_rows.size();
        stats.num_nulls = null_count;
        stats.null_fraction = static_cast<float>(null_count) / static_cast<float>(sample_rows.size());

        // Compute avg_width (average bytes per non-null value)
        uint64_t non_null_count = sample_rows.size() - null_count;
        if (non_null_count > 0)
        {
            stats.avg_width = static_cast<float>(total_width) / static_cast<float>(non_null_count);
        }
        else
        {
            stats.avg_width = 0.0f;
        }

        // Estimate n_distinct
        // For now, we'll use exact counting; HyperLogLog can be added later
        stats.num_distinct = estimateNDistinct(column_values, stats.num_rows, sample_rows.size());

        // Set metadata
        stats.last_analyzed_time = std::time(nullptr);
        stats.sample_size = sample_rows.size();
        stats.sample_rate = 0.0f; // Will be set by caller

        DEBUG_LOG_DB("Column statistics computed: " + std::to_string(stats.num_rows) + " rows, " +
                     std::to_string(stats.num_nulls) + " nulls, " +
                     std::to_string(stats.num_distinct) + " distinct");

        return Status::OK;
    }

    auto StatisticsManager::generateHistogram(const std::vector<std::vector<uint8_t>> &values,
                                               uint32_t bucket_count,
                                               HistogramType histogram_type,
                                               core::DataType data_type,
                                               std::vector<HistogramBucket> &buckets,
                                               ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Generating histogram");

        // Phase 1, Task 1.1.5 - Histogram generation
        //
        // Two types supported:
        // - Equal-Height: Buckets contain ~equal number of values (PostgreSQL-style)
        // - Equal-Width: Buckets span equal value ranges (MySQL-style)

        buckets.clear();

        if (values.empty() || bucket_count == 0)
        {
            return Status::OK; // No histogram to generate
        }

        // Filter out NULL values (represented as empty vectors)
        std::vector<std::vector<uint8_t>> non_null_values;
        non_null_values.reserve(values.size());
        for (const auto &value : values)
        {
            if (!value.empty())
            {
                non_null_values.push_back(value);
            }
        }

        if (non_null_values.empty())
        {
            return Status::OK; // All values are NULL
        }

        // Cap bucket count at number of distinct values
        if (bucket_count > non_null_values.size())
        {
            bucket_count = non_null_values.size();
        }

        if (histogram_type == HistogramType::EQUAL_HEIGHT)
        {
            // Equal-Height Histogram (PostgreSQL-style)
            // Sort values and divide into buckets with ~equal number of values

            // Sort values
            std::vector<std::vector<uint8_t>> sorted_values = non_null_values;
            std::sort(sorted_values.begin(),
                      sorted_values.end(),
                      [data_type](const std::vector<uint8_t> &lhs,
                                  const std::vector<uint8_t> &rhs) {
                          return compareTypedStatisticValues(data_type, lhs, rhs) < 0;
                      });

            // Calculate values per bucket
            size_t values_per_bucket = sorted_values.size() / bucket_count;
            size_t remainder = sorted_values.size() % bucket_count;

            buckets.reserve(bucket_count);
            size_t idx = 0;

            for (uint32_t i = 0; i < bucket_count; i++)
            {
                HistogramBucket bucket;

                // Calculate how many values in this bucket
                size_t bucket_size = values_per_bucket + (i < remainder ? 1 : 0);

                if (bucket_size == 0)
                    break;

                // Set lower bound (first value in bucket)
                const auto &lower = sorted_values[idx];
                bucket.lower_bound = lower;

                // Set upper bound (last value in bucket)
                const auto &upper = sorted_values[idx + bucket_size - 1];
                bucket.upper_bound = upper;

                // Set row count and frequency
                bucket.row_count = bucket_size;
                bucket.frequency = static_cast<float>(bucket_size) / static_cast<float>(sorted_values.size());

                // TOAST references (0 = inline data, not TOASTed)
                bucket.lower_oid = {};
                bucket.upper_oid = {};

                buckets.push_back(bucket);
                idx += bucket_size;
            }

            DEBUG_LOG_DB("Generated equal-height histogram with " + std::to_string(buckets.size()) + " buckets");
        }
        else if (histogram_type == HistogramType::EQUAL_WIDTH)
        {
            // Equal-Width Histogram (MySQL-style)
            // Divide value range into equal intervals

            // Find min and max values
            auto comparator = [data_type](const std::vector<uint8_t> &lhs,
                                          const std::vector<uint8_t> &rhs) {
                return compareTypedStatisticValues(data_type, lhs, rhs) < 0;
            };
            auto min_value = *std::min_element(non_null_values.begin(),
                                               non_null_values.end(),
                                               comparator);
            auto max_value = *std::max_element(non_null_values.begin(),
                                               non_null_values.end(),
                                               comparator);

            // For discrete types or single-value columns, fall back to equal-height
            if (min_value == max_value)
            {
                HistogramBucket bucket;

                bucket.lower_bound = min_value;
                bucket.upper_bound = min_value;

                bucket.row_count = non_null_values.size();
                bucket.frequency = 1.0f;
                bucket.lower_oid = {};
                bucket.upper_oid = {};

                buckets.push_back(bucket);
                DEBUG_LOG_DB("Generated single-bucket histogram (all values equal)");
                return Status::OK;
            }

            // For now, equal-width is primarily useful for numeric types
            // For complex types, we fall back to equal-height
            // Phase 4 Enhancement: Implement proper equal-width for numeric types with range calculation
            DEBUG_LOG_DB("Equal-width histogram for complex types not yet implemented, using equal-height");
            return generateHistogram(values,
                                     bucket_count,
                                     HistogramType::EQUAL_HEIGHT,
                                     data_type,
                                     buckets,
                                     ctx);
        }
        else
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unknown histogram type");
            return Status::INVALID_ARGUMENT;
        }

        return Status::OK;
    }

    auto StatisticsManager::identifyMCVs(const std::vector<std::vector<uint8_t>> &values,
                                          uint32_t max_mcv_count,
                                          std::vector<MCVEntry> &mcv_list,
                                          ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Identifying Most Common Values");

        // Phase 1, Task 1.1.6 - MCV identification
        //
        // Algorithm:
        // 1. Build frequency map (value -> count)
        // 2. Sort by frequency (descending)
        // 3. Take top max_mcv_count entries
        // 4. Compute frequency as fraction of total values
        // 5. Store in mcv_list

        mcv_list.clear();

        if (values.empty() || max_mcv_count == 0)
        {
            return Status::OK; // No MCVs to identify
        }

        // Build frequency map
        std::unordered_map<std::vector<uint8_t>, uint64_t, VectorHash> frequency_map;

        uint64_t total_non_null = 0;
        for (const auto &value : values)
        {
            if (!value.empty()) // Skip NULLs
            {
                frequency_map[value]++;
                total_non_null++;
            }
        }

        if (frequency_map.empty())
        {
            return Status::OK; // All values are NULL
        }

        // Convert to vector for sorting
        std::vector<std::pair<std::vector<uint8_t>, uint64_t>> freq_vector;
        freq_vector.reserve(frequency_map.size());

        for (const auto &entry : frequency_map)
        {
            freq_vector.push_back(entry);
        }

        // Sort by frequency (descending)
        std::sort(freq_vector.begin(), freq_vector.end(),
                  [](const auto &a, const auto &b)
                  {
                      return a.second > b.second; // Higher frequency first
                  });

        // Take top max_mcv_count entries
        size_t mcv_count = std::min(static_cast<size_t>(max_mcv_count), freq_vector.size());

        mcv_list.reserve(mcv_count);

        for (size_t i = 0; i < mcv_count; i++)
        {
            const auto &value = freq_vector[i].first;
            uint64_t count = freq_vector[i].second;

            MCVEntry mcv;

            // Copy value data
            mcv.value_data = value;

            // Compute frequency as fraction
            mcv.frequency = static_cast<float>(count) / static_cast<float>(total_non_null);

            // TOAST reference (0 = inline data, not TOASTed)
            mcv.value_oid = {};

            mcv_list.push_back(mcv);
        }

        DEBUG_LOG_DB("Identified " + std::to_string(mcv_list.size()) + " most common values");

        return Status::OK;
    }

    auto StatisticsManager::estimateNDistinct(const std::vector<std::vector<uint8_t>> &values,
                                               uint64_t total_rows,
                                               uint64_t sample_size) -> uint64_t
    {
        DEBUG_LOG_DB("Estimating number of distinct values");

        // Phase 1, Task 1.1.7 - n_distinct estimation
        //
        // For now, use exact count in sample with simple extrapolation
        // Future enhancement: HyperLogLog for large cardinality estimation

        if (values.empty())
        {
            return 0;
        }

        // Count distinct values in sample using a hash set
        std::unordered_set<std::vector<uint8_t>, VectorHash> distinct_values;

        for (const auto &value : values)
        {
            if (!value.empty()) // Skip NULLs (represented as empty vectors)
            {
                distinct_values.insert(value);
            }
        }

        uint64_t distinct_in_sample = distinct_values.size();

        // If we sampled the entire table, return exact count
        if (sample_size >= total_rows)
        {
            return distinct_in_sample;
        }

        // For small distinct counts relative to sample size, likely saw everything
        if (distinct_in_sample < 100 || distinct_in_sample < sample_size / 10)
        {
            return distinct_in_sample;
        }

        // Otherwise, use linear extrapolation with a cap
        // n_distinct_estimate = distinct_in_sample * (total_rows / sample_size)
        //
        // But cap at total_rows since we can't have more distinct values than rows
        double sample_fraction = static_cast<double>(sample_size) / static_cast<double>(total_rows);
        uint64_t estimate = static_cast<uint64_t>(
            static_cast<double>(distinct_in_sample) / sample_fraction);

        // Cap at total rows
        if (estimate > total_rows)
        {
            estimate = total_rows;
        }

        DEBUG_LOG_DB("Estimated " + std::to_string(estimate) + " distinct values from " +
                     std::to_string(distinct_in_sample) + " in sample");

        return estimate;
    }

    auto StatisticsManager::storeColumnStatistics(const ColumnStatistics &stats,
                                                   ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Storing column statistics to catalog");

        // OPT-1: Statistics persistence implementation
        //
        // Steps:
        // 1. Serialize MCVs to JSON and store via TOAST if large
        // 2. Serialize histogram to JSON and store via TOAST if large
        // 3. Create StatisticInfo with basic stats + TOAST refs
        // 4. Write to sb_statistic catalog via CatalogManager
        // 5. Update in-memory cache

        // Ensure catalog is initialized
        if (!catalog_)
        {
            catalog_ = db_->catalog_manager();
        }

        // Create StatisticInfo from ColumnStatistics
        core::CatalogManager::StatisticInfo info;
        info.statistic_id = core::generateUuidV7();  // Generate new ID
        info.table_id = stats.table_id;
        info.column_id = stats.column_id;
        info.data_type = static_cast<uint16_t>(stats.data_type);
        info.num_rows = stats.num_rows;
        info.num_nulls = stats.num_nulls;
        info.null_fraction = stats.null_fraction;
        info.num_distinct = stats.num_distinct;
        info.avg_width = stats.avg_width;
        info.histogram_type = static_cast<uint8_t>(stats.histogram_type);
        info.histogram_bucket_count = static_cast<uint32_t>(stats.histogram_buckets.size());
        info.last_analyzed_time = stats.last_analyzed_time;
        info.sample_size = stats.sample_size;
        info.sample_rate = stats.sample_rate;

        // Get current timestamp
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        info.created_time = now_time;
        info.last_modified_time = now_time;

        ColumnStatistics persisted_stats = stats;
        applyDerivedMetadata(persisted_stats);

        // Serialize MCVs to JSON format for TOAST storage.
        if (!stats.mcv_list.empty())
        {
            nlohmann::json payload;
            payload["version"] = 2;
            payload["metadata"] = buildStatsMetadataJson(persisted_stats);
            payload["entries"] = nlohmann::json::array();
            for (const auto &mcv : persisted_stats.mcv_list)
            {
                payload["entries"].push_back({
                    {"value", hexEncodeBytes(mcv.value_data)},
                    {"frequency", mcv.frequency},
                    {"value_oid", mcv.value_oid.toString()}});
            }

            // Store via TOAST and get OID (xmin=0 for catalog operations)
            Status status = catalog_->storeStringInToast(payload.dump(), 0, info.mcv_oid, ctx);
            if (status != Status::OK)
            {
                DEBUG_LOG_DB("Failed to store MCV list via TOAST");
                // Continue anyway - stats will work without MCVs
                info.mcv_oid = {};
            }
        }

        // Serialize histogram to JSON format for TOAST storage.
        if (!stats.histogram_buckets.empty())
        {
            nlohmann::json payload;
            payload["version"] = 2;
            payload["metadata"] = buildStatsMetadataJson(persisted_stats);
            payload["buckets"] = nlohmann::json::array();
            for (const auto &bucket : persisted_stats.histogram_buckets)
            {
                payload["buckets"].push_back({
                    {"lower", hexEncodeBytes(bucket.lower_bound)},
                    {"upper", hexEncodeBytes(bucket.upper_bound)},
                    {"row_count", bucket.row_count},
                    {"frequency", bucket.frequency},
                    {"lower_oid", bucket.lower_oid.toString()},
                    {"upper_oid", bucket.upper_oid.toString()}});
            }

            // Store via TOAST and get OID (xmin=0 for catalog operations)
            Status status = catalog_->storeStringInToast(payload.dump(), 0, info.histogram_oid, ctx);
            if (status != Status::OK)
            {
                DEBUG_LOG_DB("Failed to store histogram via TOAST");
                // Continue anyway - stats will work without histogram
                info.histogram_oid = {};
            }
        }

        // Store to catalog
        Status status = catalog_->storeStatistic(info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to store statistics to catalog");
            return status;
        }

        // Also update in-memory cache for fast access
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            uint64_t cache_key = getCacheKey(stats.table_id, stats.column_id);
            column_stats_cache_[cache_key] = persisted_stats;
        }

        DEBUG_LOG_DB("Stored column statistics to catalog for table_id=" +
                     std::to_string(*reinterpret_cast<const uint64_t*>(stats.table_id.bytes.data())));

        return Status::OK;
    }

    auto StatisticsManager::loadColumnStatistics(const ID &table_id, const ID &column_id,
                                                  ColumnStatistics &stats,
                                                  ErrorContext *ctx) -> Status
    {
        DEBUG_LOG_DB("Loading column statistics from catalog");

        // OPT-2: Statistics loading from persistent storage
        //
        // Steps:
        // 1. Read StatisticInfo from sb_statistic catalog via CatalogManager
        // 2. Load MCVs from TOAST if OID is set
        // 3. Load histogram from TOAST if OID is set
        // 4. Deserialize into ColumnStatistics struct
        // 5. Cache for fast access

        // Ensure catalog is initialized
        if (!catalog_)
        {
            catalog_ = db_->catalog_manager();
        }

        // Try to load from catalog
        core::CatalogManager::StatisticInfo info;
        Status status = catalog_->getStatistic(table_id, column_id, info, ctx);
        if (status != Status::OK)
        {
            // Not found in catalog - this is normal for columns that haven't been analyzed
            return status;
        }

        // Convert StatisticInfo to ColumnStatistics
        stats.table_id = info.table_id;
        stats.column_id = info.column_id;
        stats.data_type = static_cast<core::DataType>(info.data_type);
        stats.num_rows = info.num_rows;
        stats.num_nulls = info.num_nulls;
        stats.null_fraction = info.null_fraction;
        stats.num_distinct = info.num_distinct;
        stats.avg_width = info.avg_width;
        stats.histogram_type = static_cast<HistogramType>(info.histogram_type);
        stats.histogram_bucket_count = info.histogram_bucket_count;
        stats.last_analyzed_time = info.last_analyzed_time;
        stats.sample_size = info.sample_size;
        stats.sample_rate = info.sample_rate;
        applyDerivedMetadata(stats);

        std::vector<core::CatalogManager::ColumnInfo> columns;
        if (catalog_->getColumns(table_id, columns, ctx) == Status::OK)
        {
            for (const auto &column : columns)
            {
                if (column.column_id == column_id)
                {
                    stats.column_name = column.column_name;
                    applyColumnMetadata(column, stats);
                    break;
                }
            }
        }

        // Load MCVs from TOAST if available
        if (!isZeroId(info.mcv_oid))
        {
            std::string json;
            status = catalog_->loadStringFromToast(info.mcv_oid, 0, json, ctx);
            if (status == Status::OK && !json.empty())
            {
                stats.mcv_list.clear();
                try
                {
                    const auto payload = nlohmann::json::parse(json);
                    const auto *entries = payload.is_array()
                                              ? &payload
                                              : (payload.contains("entries")
                                                     ? &payload["entries"]
                                                     : nullptr);
                    if (payload.is_object())
                    {
                        applyStatsMetadataJson(payload.value("metadata", nlohmann::json::object()),
                                               stats);
                    }
                    if (entries != nullptr && entries->is_array())
                    {
                        for (const auto &item : *entries)
                        {
                            if (!item.is_object())
                            {
                                continue;
                            }
                            MCVEntry entry;
                            const std::string hex_value =
                                item.value("value", item.value("v", std::string()));
                            if (!hexDecodeBytes(hex_value, entry.value_data))
                            {
                                continue;
                            }
                            entry.frequency = item.value("frequency",
                                                         item.value("f", 0.0f));
                            stats.mcv_list.push_back(entry);
                        }
                    }
                }
                catch (const nlohmann::json::exception &)
                {
                    SET_ERROR_CONTEXT(ctx,
                                      Status::PAGE_CORRUPT,
                                      "Failed to parse persisted MCV statistics");
                    return Status::PAGE_CORRUPT;
                }
            }
        }

        // Load histogram from TOAST if available
        if (!isZeroId(info.histogram_oid))
        {
            std::string json;
            status = catalog_->loadStringFromToast(info.histogram_oid, 0, json, ctx);
            if (status == Status::OK && !json.empty())
            {
                stats.histogram_buckets.clear();
                try
                {
                    const auto payload = nlohmann::json::parse(json);
                    const auto *buckets = payload.is_array()
                                              ? &payload
                                              : (payload.contains("buckets")
                                                     ? &payload["buckets"]
                                                     : nullptr);
                    if (payload.is_object())
                    {
                        applyStatsMetadataJson(payload.value("metadata", nlohmann::json::object()),
                                               stats);
                    }
                    if (buckets != nullptr && buckets->is_array())
                    {
                        for (const auto &item : *buckets)
                        {
                            if (!item.is_object())
                            {
                                continue;
                            }
                            HistogramBucket bucket;
                            const std::string lo_hex =
                                item.value("lower", item.value("lo", std::string()));
                            const std::string hi_hex =
                                item.value("upper", item.value("hi", std::string()));
                            if (!hexDecodeBytes(lo_hex, bucket.lower_bound) ||
                                !hexDecodeBytes(hi_hex, bucket.upper_bound))
                            {
                                continue;
                            }
                            bucket.row_count =
                                item.value("row_count", item.value("cnt", uint64_t{0}));
                            bucket.frequency = item.value("frequency",
                                                          item.value("f", 0.0f));
                            stats.histogram_buckets.push_back(bucket);
                        }
                    }
                }
                catch (const nlohmann::json::exception &)
                {
                    SET_ERROR_CONTEXT(ctx,
                                      Status::PAGE_CORRUPT,
                                      "Failed to parse persisted histogram statistics");
                    return Status::PAGE_CORRUPT;
                }
            }
        }
        stats.histogram_bucket_count =
            static_cast<uint32_t>(stats.histogram_buckets.size());
        applyDerivedMetadata(stats);

        // Cache for fast access
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            uint64_t cache_key = getCacheKey(table_id, column_id);
            column_stats_cache_[cache_key] = stats;
        }

        DEBUG_LOG_DB("Loaded column statistics from catalog for table_id=" +
                     std::to_string(*reinterpret_cast<const uint64_t*>(table_id.bytes.data())));

        return Status::OK;
    }

    auto StatisticsManager::getCorrelationCacheKey(const ID &table_id,
                                                   const ID &left_column_id,
                                                   const ID &right_column_id) const
        -> std::string
    {
        const bool left_first = left_column_id < right_column_id;
        const ID &first = left_first ? left_column_id : right_column_id;
        const ID &second = left_first ? right_column_id : left_column_id;
        return table_id.toString() + "|" + first.toString() + "|" + second.toString();
    }

    auto StatisticsManager::getExpressionCacheKey(const ID &table_id,
                                                  const std::string &expression_key) const
        -> std::string
    {
        return table_id.toString() + "|" + core::IdentifierUtils::toUpper(expression_key);
    }

    auto StatisticsManager::getMultivariateCacheKey(const ID &table_id,
                                                    const std::vector<ID> &column_ids) const
        -> std::string
    {
        std::vector<ID> sorted_ids = column_ids;
        std::sort(sorted_ids.begin(), sorted_ids.end());
        std::string key = table_id.toString();
        for (const auto &column_id : sorted_ids)
        {
            key.append("|");
            key.append(column_id.toString());
        }
        return key;
    }

    auto StatisticsManager::makeSyntheticStatisticId(const ID &table_id,
                                                     const std::string &kind,
                                                     const std::string &key) -> ID
    {
        const std::string material =
            kind + "|" + table_id.toString() + "|" + core::IdentifierUtils::toUpper(key);
        uint64_t hash_a = 14695981039346656037ULL;
        uint64_t hash_b = 1099511628211ULL;
        for (unsigned char byte : material)
        {
            hash_a ^= static_cast<uint64_t>(byte);
            hash_a *= 1099511628211ULL;
            hash_b ^= (static_cast<uint64_t>(byte) + 0x9e3779b97f4a7c15ULL);
            hash_b *= 14029467366897019727ULL;
        }

        ID id{};
        std::memcpy(id.bytes.data(), &hash_a, sizeof(hash_a));
        std::memcpy(id.bytes.data() + sizeof(hash_a), &hash_b, sizeof(hash_b));
        id.bytes[6] = static_cast<uint8_t>((id.bytes[6] & 0x0F) | 0x50);
        id.bytes[8] = static_cast<uint8_t>((id.bytes[8] & 0x3F) | 0x80);
        return id;
    }

    auto StatisticsManager::makeCorrelationStatisticKey(const ID &left_column_id,
                                                        const ID &right_column_id) -> std::string
    {
        const bool left_first = left_column_id < right_column_id;
        const ID &first = left_first ? left_column_id : right_column_id;
        const ID &second = left_first ? right_column_id : left_column_id;
        return first.toString() + "|" + second.toString();
    }

    auto StatisticsManager::storeCorrelationStatistic(const ColumnCorrelationStatistics &stats,
                                                      ErrorContext *ctx) -> Status
    {
        if (!catalog_)
        {
            catalog_ = db_->catalog_manager();
        }
        if (catalog_ == nullptr)
        {
            return Status::INTERNAL_ERROR;
        }

        core::CatalogManager::StatisticInfo info;
        info.statistic_id =
            makeSyntheticStatisticId(stats.table_id,
                                     "CORR_ID",
                                     makeCorrelationStatisticKey(stats.left_column_id,
                                                                 stats.right_column_id));
        info.table_id = stats.table_id;
        info.column_id =
            makeSyntheticStatisticId(stats.table_id,
                                     "CORR",
                                     makeCorrelationStatisticKey(stats.left_column_id,
                                                                 stats.right_column_id));
        info.data_type = static_cast<uint16_t>(core::DataType::FLOAT64);
        info.num_rows = stats.sample_size;
        info.num_distinct = 1;
        info.avg_width = static_cast<float>(stats.coefficient);
        info.null_fraction = static_cast<float>(stats.coefficient);
        info.last_analyzed_time = stats.last_analyzed_time;
        info.sample_size = stats.sample_size;
        info.sample_rate = 0.0f;
        return catalog_->storeStatistic(info, ctx);
    }

    auto StatisticsManager::loadCorrelationStatistic(const ID &table_id,
                                                     const ID &left_column_id,
                                                     const ID &right_column_id,
                                                     ColumnCorrelationStatistics &stats_out,
                                                     ErrorContext *ctx) -> Status
    {
        if (!catalog_)
        {
            catalog_ = db_->catalog_manager();
        }
        if (catalog_ == nullptr)
        {
            return Status::INTERNAL_ERROR;
        }

        core::CatalogManager::StatisticInfo info;
        const ID synthetic_column_id =
            makeSyntheticStatisticId(table_id,
                                     "CORR",
                                     makeCorrelationStatisticKey(left_column_id, right_column_id));
        Status status = catalog_->getStatistic(table_id, synthetic_column_id, info, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        if (isZeroId(info.histogram_oid))
        {
            stats_out.table_id = table_id;
            stats_out.left_column_id = left_column_id;
            stats_out.right_column_id = right_column_id;
            stats_out.coefficient = static_cast<double>(info.null_fraction);
            stats_out.sample_size = info.sample_size;
            stats_out.last_analyzed_time = info.last_analyzed_time;

            std::vector<core::CatalogManager::ColumnInfo> columns;
            if (catalog_->getColumns(table_id, columns, ctx) == Status::OK)
            {
                for (const auto &column : columns)
                {
                    if (column.column_id == left_column_id)
                    {
                        stats_out.left_column_name = column.column_name;
                    }
                    else if (column.column_id == right_column_id)
                    {
                        stats_out.right_column_name = column.column_name;
                    }
                }
            }
            return Status::OK;
        }

        std::string payload_text;
        status = catalog_->loadStringFromToast(info.histogram_oid, 0, payload_text, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        try
        {
            const auto payload = nlohmann::json::parse(payload_text);
            stats_out.table_id = table_id;
            stats_out.left_column_id = left_column_id;
            stats_out.right_column_id = right_column_id;
            stats_out.left_column_name =
                payload.value("left_column_name", std::string());
            stats_out.right_column_name =
                payload.value("right_column_name", std::string());
            stats_out.coefficient = payload.value("coefficient", 0.0);
            stats_out.sample_size = payload.value("sample_size", uint64_t{0});
            stats_out.last_analyzed_time =
                payload.value("last_analyzed_time", uint64_t{0});
            return Status::OK;
        }
        catch (const nlohmann::json::exception &)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                              "Failed to parse persisted correlation statistics");
            return Status::PAGE_CORRUPT;
        }
    }

    auto StatisticsManager::storeMultivariateStatistics(const MultivariateStatistics &stats,
                                                        ErrorContext *ctx) -> Status
    {
        if (!catalog_)
        {
            catalog_ = db_->catalog_manager();
        }
        if (catalog_ == nullptr)
        {
            return Status::INTERNAL_ERROR;
        }
        if (stats.column_ids.size() < 2)
        {
            return Status::INVALID_ARGUMENT;
        }

        const std::string key = getMultivariateCacheKey(stats.table_id, stats.column_ids);
        core::CatalogManager::StatisticInfo info;
        info.statistic_id = makeSyntheticStatisticId(stats.table_id, "MVSTAT_ID", key);
        info.table_id = stats.table_id;
        info.column_id = makeSyntheticStatisticId(stats.table_id, "MVSTAT", key);
        info.data_type = static_cast<uint16_t>(core::DataType::UNKNOWN);
        info.num_rows = stats.sample_size;
        info.num_distinct = stats.ndistinct.num_distinct;
        info.avg_width = static_cast<float>(stats.column_ids.size());
        info.last_analyzed_time = stats.last_analyzed_time;
        info.sample_size = stats.sample_size;
        info.sample_rate = 0.0f;
        info.histogram_type = static_cast<uint8_t>(HistogramType::NONE);
        info.histogram_bucket_count = 0;

        nlohmann::json mcv_payload;
        mcv_payload["entries"] = nlohmann::json::array();
        for (const auto &entry : stats.mcv_list)
        {
            nlohmann::json values = nlohmann::json::array();
            for (const auto &value : entry.values)
            {
                values.push_back(hexEncodeBytes(value));
            }
            mcv_payload["entries"].push_back(
                {{"values", std::move(values)}, {"frequency", entry.frequency}});
        }
        if (!stats.mcv_list.empty())
        {
            Status status =
                catalog_->storeStringInToast(mcv_payload.dump(), 0, info.mcv_oid, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        nlohmann::json metadata;
        metadata["column_ids"] = nlohmann::json::array();
        metadata["column_names"] = stats.column_names;
        for (const auto &column_id : stats.column_ids)
        {
            metadata["column_ids"].push_back(column_id.toString());
        }
        metadata["sample_size"] = stats.sample_size;
        metadata["last_analyzed_time"] = stats.last_analyzed_time;
        metadata["ndistinct"] = {
            {"num_distinct", stats.ndistinct.num_distinct},
            {"distinct_fraction", stats.ndistinct.distinct_fraction},
            {"sample_size", stats.ndistinct.sample_size},
            {"last_analyzed_time", stats.ndistinct.last_analyzed_time}};
        metadata["dependencies"] = nlohmann::json::array();
        for (const auto &dependency : stats.dependencies)
        {
            nlohmann::json determinant_indexes = nlohmann::json::array();
            nlohmann::json dependent_indexes = nlohmann::json::array();
            for (const auto &determinant_id : dependency.determinant_column_ids)
            {
                auto it = std::find(stats.column_ids.begin(),
                                    stats.column_ids.end(),
                                    determinant_id);
                if (it != stats.column_ids.end())
                {
                    determinant_indexes.push_back(
                        static_cast<uint32_t>(std::distance(stats.column_ids.begin(), it)));
                }
            }
            for (const auto &dependent_id : dependency.dependent_column_ids)
            {
                auto it = std::find(stats.column_ids.begin(),
                                    stats.column_ids.end(),
                                    dependent_id);
                if (it != stats.column_ids.end())
                {
                    dependent_indexes.push_back(
                        static_cast<uint32_t>(std::distance(stats.column_ids.begin(), it)));
                }
            }
            metadata["dependencies"].push_back(
                {{"determinant_indexes", std::move(determinant_indexes)},
                 {"dependent_indexes", std::move(dependent_indexes)},
                 {"strength", dependency.strength},
                 {"sample_size", dependency.sample_size},
                 {"last_analyzed_time", dependency.last_analyzed_time}});
        }
        metadata["pairwise_correlations"] = nlohmann::json::array();
        for (const auto &correlation : stats.pairwise_correlations)
        {
            auto left_it = std::find(stats.column_ids.begin(),
                                     stats.column_ids.end(),
                                     correlation.left_column_id);
            auto right_it = std::find(stats.column_ids.begin(),
                                      stats.column_ids.end(),
                                      correlation.right_column_id);
            if (left_it == stats.column_ids.end() || right_it == stats.column_ids.end())
            {
                continue;
            }
            metadata["pairwise_correlations"].push_back(
                {{"left_index",
                  static_cast<uint32_t>(std::distance(stats.column_ids.begin(), left_it))},
                 {"right_index",
                  static_cast<uint32_t>(std::distance(stats.column_ids.begin(), right_it))},
                 {"coefficient", correlation.coefficient},
                 {"sample_size", correlation.sample_size},
                 {"last_analyzed_time", correlation.last_analyzed_time}});
        }

        Status metadata_status =
            catalog_->storeStringInToast(metadata.dump(), 0, info.histogram_oid, ctx);
        if (metadata_status != Status::OK)
        {
            return metadata_status;
        }

        return catalog_->storeStatistic(info, ctx);
    }

    auto StatisticsManager::loadMultivariateStatistics(const ID &table_id,
                                                       const std::vector<ID> &column_ids,
                                                       MultivariateStatistics &stats_out,
                                                       ErrorContext *ctx) -> Status
    {
        if (!catalog_)
        {
            catalog_ = db_->catalog_manager();
        }
        if (catalog_ == nullptr)
        {
            return Status::INTERNAL_ERROR;
        }
        if (column_ids.size() < 2)
        {
            return Status::INVALID_ARGUMENT;
        }

        std::vector<ID> sorted_ids = column_ids;
        std::sort(sorted_ids.begin(), sorted_ids.end());
        const std::string key = getMultivariateCacheKey(table_id, sorted_ids);

        core::CatalogManager::StatisticInfo info;
        const ID synthetic_column_id =
            makeSyntheticStatisticId(table_id, "MVSTAT", key);
        Status status = catalog_->getStatistic(table_id, synthetic_column_id, info, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        stats_out = MultivariateStatistics{};
        stats_out.table_id = table_id;
        stats_out.column_ids = sorted_ids;
        stats_out.sample_size = info.sample_size;
        stats_out.last_analyzed_time = info.last_analyzed_time;
        stats_out.ndistinct.column_ids = sorted_ids;
        stats_out.ndistinct.num_distinct = info.num_distinct;
        stats_out.ndistinct.sample_size = info.sample_size;
        stats_out.ndistinct.last_analyzed_time = info.last_analyzed_time;

        std::vector<core::CatalogManager::ColumnInfo> columns;
        if (catalog_->getColumns(table_id, columns, ctx) == Status::OK)
        {
            for (const auto &column_id : sorted_ids)
            {
                auto it = std::find_if(
                    columns.begin(),
                    columns.end(),
                    [&](const core::CatalogManager::ColumnInfo &column) {
                        return column.column_id == column_id;
                    });
                stats_out.column_names.push_back(
                    it != columns.end() ? it->column_name : std::string());
            }
            stats_out.ndistinct.column_names = stats_out.column_names;
        }

        if (!isZeroId(info.histogram_oid))
        {
            std::string metadata_text;
            status = catalog_->loadStringFromToast(info.histogram_oid, 0, metadata_text, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            try
            {
                const auto metadata = nlohmann::json::parse(metadata_text);
                if (metadata.contains("column_ids") &&
                    metadata["column_ids"].is_array())
                {
                    std::vector<ID> persisted_column_ids;
                    persisted_column_ids.reserve(metadata["column_ids"].size());
                    for (const auto &entry : metadata["column_ids"])
                    {
                        if (!entry.is_string())
                        {
                            persisted_column_ids.clear();
                            break;
                        }
                        ID parsed_id{};
                        if (!parseUuidFromString(entry.get<std::string>(), parsed_id))
                        {
                            persisted_column_ids.clear();
                            break;
                        }
                        persisted_column_ids.push_back(parsed_id);
                    }
                    if (persisted_column_ids.size() == column_ids.size())
                    {
                        stats_out.column_ids = persisted_column_ids;
                        stats_out.ndistinct.column_ids = persisted_column_ids;
                    }
                }
                if (metadata.contains("column_names") &&
                    metadata["column_names"].is_array())
                {
                    stats_out.column_names.clear();
                    for (const auto &entry : metadata["column_names"])
                    {
                        stats_out.column_names.push_back(entry.get<std::string>());
                    }
                    stats_out.ndistinct.column_names = stats_out.column_names;
                }
                if (metadata.contains("sample_size"))
                {
                    stats_out.sample_size = metadata.value("sample_size", info.sample_size);
                }
                if (metadata.contains("last_analyzed_time"))
                {
                    stats_out.last_analyzed_time =
                        metadata.value("last_analyzed_time", info.last_analyzed_time);
                }
                if (metadata.contains("ndistinct") && metadata["ndistinct"].is_object())
                {
                    const auto &nd = metadata["ndistinct"];
                    stats_out.ndistinct.num_distinct =
                        nd.value("num_distinct", info.num_distinct);
                    stats_out.ndistinct.distinct_fraction =
                        nd.value("distinct_fraction", 0.0);
                    stats_out.ndistinct.sample_size =
                        nd.value("sample_size", info.sample_size);
                    stats_out.ndistinct.last_analyzed_time =
                        nd.value("last_analyzed_time", info.last_analyzed_time);
                }
                if (metadata.contains("dependencies") &&
                    metadata["dependencies"].is_array())
                {
                    for (const auto &entry : metadata["dependencies"])
                    {
                        if (!entry.is_object())
                        {
                            continue;
                        }
                        FunctionalDependencyStatistics dependency;
                        if (entry.contains("determinant_indexes") &&
                            entry["determinant_indexes"].is_array())
                        {
                            for (const auto &index_entry : entry["determinant_indexes"])
                            {
                                const size_t index = index_entry.get<size_t>();
                                if (index < stats_out.column_ids.size())
                                {
                                    dependency.determinant_column_ids.push_back(
                                        stats_out.column_ids[index]);
                                    dependency.determinant_column_names.push_back(
                                        index < stats_out.column_names.size()
                                            ? stats_out.column_names[index]
                                            : std::string());
                                }
                            }
                        }
                        if (entry.contains("dependent_indexes") &&
                            entry["dependent_indexes"].is_array())
                        {
                            for (const auto &index_entry : entry["dependent_indexes"])
                            {
                                const size_t index = index_entry.get<size_t>();
                                if (index < stats_out.column_ids.size())
                                {
                                    dependency.dependent_column_ids.push_back(
                                        stats_out.column_ids[index]);
                                    dependency.dependent_column_names.push_back(
                                        index < stats_out.column_names.size()
                                            ? stats_out.column_names[index]
                                            : std::string());
                                }
                            }
                        }
                        dependency.strength = entry.value("strength", 0.0);
                        dependency.sample_size =
                            entry.value("sample_size", stats_out.sample_size);
                        dependency.last_analyzed_time =
                            entry.value("last_analyzed_time",
                                        stats_out.last_analyzed_time);
                        stats_out.dependencies.push_back(std::move(dependency));
                    }
                }
                if (metadata.contains("pairwise_correlations") &&
                    metadata["pairwise_correlations"].is_array())
                {
                    for (const auto &entry : metadata["pairwise_correlations"])
                    {
                        if (!entry.is_object())
                        {
                            continue;
                        }
                        const size_t left_index = entry.value("left_index", size_t{0});
                        const size_t right_index = entry.value("right_index", size_t{0});
                        if (left_index >= stats_out.column_ids.size() ||
                            right_index >= stats_out.column_ids.size())
                        {
                            continue;
                        }
                        ColumnCorrelationStatistics correlation;
                        correlation.table_id = table_id;
                        correlation.left_column_id = stats_out.column_ids[left_index];
                        correlation.right_column_id = stats_out.column_ids[right_index];
                        correlation.left_column_name =
                            left_index < stats_out.column_names.size()
                                ? stats_out.column_names[left_index]
                                : std::string();
                        correlation.right_column_name =
                            right_index < stats_out.column_names.size()
                                ? stats_out.column_names[right_index]
                                : std::string();
                        correlation.coefficient = entry.value("coefficient", 0.0);
                        correlation.sample_size =
                            entry.value("sample_size", uint64_t{0});
                        correlation.last_analyzed_time =
                            entry.value("last_analyzed_time", uint64_t{0});
                        stats_out.pairwise_correlations.push_back(std::move(correlation));
                    }
                }
            }
            catch (const nlohmann::json::exception &)
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::PAGE_CORRUPT,
                                  "Failed to parse persisted multivariate statistics");
                return Status::PAGE_CORRUPT;
            }
        }

        if (!isZeroId(info.mcv_oid))
        {
            std::string mcv_text;
            status = catalog_->loadStringFromToast(info.mcv_oid, 0, mcv_text, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            try
            {
                const auto payload = nlohmann::json::parse(mcv_text);
                const auto *entries = payload.is_array()
                                          ? &payload
                                          : (payload.contains("entries")
                                                 ? &payload["entries"]
                                                 : nullptr);
                if (entries != nullptr && entries->is_array())
                {
                    for (const auto &item : *entries)
                    {
                        if (!item.is_object())
                        {
                            continue;
                        }
                        MultivariateMCVEntry entry;
                        if (item.contains("values") && item["values"].is_array())
                        {
                            for (const auto &value_entry : item["values"])
                            {
                                std::vector<uint8_t> decoded;
                                if (!hexDecodeBytes(value_entry.get<std::string>(), decoded))
                                {
                                    entry.values.clear();
                                    break;
                                }
                                entry.values.push_back(std::move(decoded));
                            }
                        }
                        if (entry.values.empty())
                        {
                            continue;
                        }
                        entry.frequency = item.value("frequency", 0.0f);
                        stats_out.mcv_list.push_back(std::move(entry));
                    }
                }
            }
            catch (const nlohmann::json::exception &)
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::PAGE_CORRUPT,
                                  "Failed to parse persisted multivariate MCV statistics");
                return Status::PAGE_CORRUPT;
            }
        }

        return Status::OK;
    }

    auto StatisticsManager::maybeAutoAnalyze(const ID &table_id,
                                             AnalyzeLifecycleDecision *decision_out,
                                             ErrorContext *ctx) -> Status
    {
        if (decision_out != nullptr)
        {
            *decision_out = AnalyzeLifecycleDecision{};
            decision_out->automatic = true;
        }

        if (db_ == nullptr || db_->table_stats_manager() == nullptr || isZeroId(table_id))
        {
            return Status::OK;
        }

        core::TableStatsSnapshot snapshot;
        if (!db_->table_stats_manager()->snapshotForTable(table_id, snapshot))
        {
            if (decision_out != nullptr)
            {
                decision_out->never_analyzed = true;
                decision_out->threshold = 64;
            }
            ErrorContext auto_ctx;
            Status status = analyzeTableInternal(table_id, 0.10f, true, &auto_ctx);
            if (status != Status::OK)
            {
                if (ctx != nullptr && ctx->message.empty())
                {
                    ctx->message = auto_ctx.message;
                }
                return status;
            }
            if (decision_out != nullptr)
            {
                decision_out->triggered = true;
                decision_out->modified_rows_since_analyze = 0;
            }
            return Status::OK;
        }

        const bool never_analyzed =
            snapshot.last_analyze_at == 0 && snapshot.last_autoanalyze_at == 0;
        uint64_t live_rows = snapshot.live_rows_estimate > 0
            ? static_cast<uint64_t>(snapshot.live_rows_estimate)
            : (snapshot.rows_inserted + snapshot.rows_updated + snapshot.rows_deleted);
        if (live_rows == 0)
        {
            live_rows = 1000;
        }
        const uint64_t threshold = std::max<uint64_t>(64, live_rows / 5);
        if (decision_out != nullptr)
        {
            decision_out->never_analyzed = never_analyzed;
            decision_out->live_rows = live_rows;
            decision_out->modified_rows_since_analyze = snapshot.mod_since_analyze;
            decision_out->threshold = threshold;
        }
        if (!never_analyzed && snapshot.mod_since_analyze < threshold)
        {
            return Status::OK;
        }

        ErrorContext auto_ctx;
        Status status = analyzeTableInternal(table_id, 0.10f, true, &auto_ctx);
        if (status != Status::OK)
        {
            if (ctx != nullptr && ctx->message.empty())
            {
                ctx->message = auto_ctx.message;
            }
            return status;
        }
        if (decision_out != nullptr)
        {
            decision_out->triggered = true;
            decision_out->modified_rows_since_analyze = 0;
        }
        return Status::OK;
    }

    auto StatisticsManager::applyFreshnessMetadata(const ID &table_id,
                                                   bool auto_analyze_applied,
                                                   uint64_t auto_analyze_threshold,
                                                   ColumnStatistics &stats) -> void
    {
        uint64_t modified_rows_since_analyze = 0;
        uint64_t live_rows = stats.num_rows;
        bool have_runtime_snapshot = false;
        if (db_ != nullptr && db_->table_stats_manager() != nullptr && !isZeroId(table_id))
        {
            core::TableStatsSnapshot snapshot;
            if (db_->table_stats_manager()->snapshotForTable(table_id, snapshot))
            {
                have_runtime_snapshot = true;
                modified_rows_since_analyze = snapshot.mod_since_analyze;
                if (snapshot.live_rows_estimate > 0)
                {
                    live_rows = static_cast<uint64_t>(snapshot.live_rows_estimate);
                }
                else if (live_rows == 0)
                {
                    live_rows = snapshot.rows_inserted + snapshot.rows_updated +
                                snapshot.rows_deleted;
                }
            }
        }
        if (!have_runtime_snapshot && !isZeroId(table_id))
        {
            if (!catalog_ && db_ != nullptr)
            {
                catalog_ = db_->catalog_manager();
            }
            if (catalog_ != nullptr)
            {
                core::CatalogManager::TableInfo table_info;
                core::ErrorContext local_ctx;
                if (catalog_->getTable(table_id, table_info, &local_ctx) == core::Status::OK &&
                    table_info.row_count > 0)
                {
                    live_rows = table_info.row_count;
                    if (stats.num_rows > 0)
                    {
                        modified_rows_since_analyze =
                            table_info.row_count > stats.num_rows
                                ? (table_info.row_count - stats.num_rows)
                                : (stats.num_rows - table_info.row_count);
                    }
                }
            }
        }

        const double sample_ratio =
            effectiveSampleRatio(std::max<uint64_t>(live_rows, stats.num_rows),
                                 stats.sample_size,
                                 stats.sample_rate);
        stats.sample_rate = static_cast<float>(sample_ratio);
        stats.modified_rows_since_analyze = modified_rows_since_analyze;
        stats.staleness_class = classifyStaleness(stats.last_analyzed_time,
                                                  std::max<uint64_t>(1, std::max(live_rows, stats.num_rows)),
                                                  modified_rows_since_analyze);
        stats.confidence_class = classifyConfidence(stats.sample_size,
                                                    sample_ratio,
                                                    stats.staleness_class);
        stats.auto_analyze_applied = auto_analyze_applied;
        stats.auto_analyze_threshold =
            auto_analyze_threshold > 0
                ? auto_analyze_threshold
                : std::max<uint64_t>(64, std::max<uint64_t>(1, live_rows) / 5);
        stats.stats_snapshot_id = computeStatsSnapshotId(table_id,
                                                         stats.column_id,
                                                         stats.last_analyzed_time,
                                                         stats.sample_size,
                                                         sample_ratio,
                                                         modified_rows_since_analyze);
    }

    auto StatisticsManager::applyFreshnessMetadata(const ID &table_id,
                                                   bool auto_analyze_applied,
                                                   uint64_t auto_analyze_threshold,
                                                   TableStatistics &stats) -> void
    {
        uint64_t modified_rows_since_analyze = 0;
        uint64_t live_rows = stats.num_rows;
        if (db_ != nullptr && db_->table_stats_manager() != nullptr && !isZeroId(table_id))
        {
            core::TableStatsSnapshot snapshot;
            if (db_->table_stats_manager()->snapshotForTable(table_id, snapshot))
            {
                modified_rows_since_analyze = snapshot.mod_since_analyze;
                if (snapshot.live_rows_estimate > 0)
                {
                    live_rows = static_cast<uint64_t>(snapshot.live_rows_estimate);
                }
            }
        }

        stats.modified_rows_since_analyze = modified_rows_since_analyze;
        stats.staleness_class = classifyStaleness(stats.last_analyzed_time,
                                                  std::max<uint64_t>(1, live_rows),
                                                  modified_rows_since_analyze);
        stats.confidence_class = stats.last_analyzed_time == 0
            ? StatisticsConfidenceClass::LOW
            : (stats.staleness_class == StatisticsStalenessClass::FRESH
                   ? StatisticsConfidenceClass::HIGH
                   : (stats.staleness_class == StatisticsStalenessClass::WARM
                          ? StatisticsConfidenceClass::MEDIUM
                          : StatisticsConfidenceClass::LOW));
        stats.auto_analyze_applied = auto_analyze_applied;
        stats.auto_analyze_threshold =
            auto_analyze_threshold > 0
                ? auto_analyze_threshold
                : std::max<uint64_t>(64, std::max<uint64_t>(1, live_rows) / 5);
        stats.stats_snapshot_id = computeStatsSnapshotId(table_id,
                                                         ID{},
                                                         stats.last_analyzed_time,
                                                         live_rows,
                                                         1.0,
                                                         modified_rows_since_analyze);
    }

    auto StatisticsManager::getCacheKey(const ID &table_id, const ID &column_id) -> uint64_t
    {
        // Combine table_id and column_id into a single uint64_t key
        // Use XOR of first 8 bytes of each ID
        uint64_t table_key = 0;
        uint64_t column_key = 0;
        std::memcpy(&table_key, table_id.bytes.data(), sizeof(uint64_t));
        std::memcpy(&column_key, column_id.bytes.data(), sizeof(uint64_t));
        return table_key ^ column_key;
    }

    auto StatisticsManager::computeCorrelationStatistics(
        const ID &table_id,
        const std::vector<core::CatalogManager::ColumnInfo> &columns,
        const std::vector<std::vector<uint8_t>> &sample_rows,
        uint64_t analyzed_time,
        ErrorContext *ctx) -> void
    {
        for (size_t left_idx = 0; left_idx < columns.size(); ++left_idx)
        {
            const auto left_type = static_cast<core::DataType>(columns[left_idx].data_type);
            std::vector<std::vector<uint8_t>> left_values =
                extractColumnValues(table_id, columns[left_idx].column_id, sample_rows, columns, nullptr);
            if (left_values.empty())
            {
                continue;
            }

            for (size_t right_idx = left_idx + 1; right_idx < columns.size(); ++right_idx)
            {
                const auto right_type = static_cast<core::DataType>(columns[right_idx].data_type);
                std::vector<std::vector<uint8_t>> right_values =
                    extractColumnValues(table_id,
                                        columns[right_idx].column_id,
                                        sample_rows,
                                        columns,
                                        nullptr);
                if (right_values.empty())
                {
                    continue;
                }

                double sum_x = 0.0;
                double sum_y = 0.0;
                double sum_x2 = 0.0;
                double sum_y2 = 0.0;
                double sum_xy = 0.0;
                uint64_t count = 0;

                const size_t value_count = std::min(left_values.size(), right_values.size());
                for (size_t i = 0; i < value_count; ++i)
                {
                    double x = 0.0;
                    double y = 0.0;
                    if (!decodeNumericValue(left_values[i], left_type, x) ||
                        !decodeNumericValue(right_values[i], right_type, y))
                    {
                        continue;
                    }
                    ++count;
                    sum_x += x;
                    sum_y += y;
                    sum_x2 += x * x;
                    sum_y2 += y * y;
                    sum_xy += x * y;
                }

                if (count < 8)
                {
                    continue;
                }

                const double numerator = static_cast<double>(count) * sum_xy - (sum_x * sum_y);
                const double denom_left = static_cast<double>(count) * sum_x2 - (sum_x * sum_x);
                const double denom_right = static_cast<double>(count) * sum_y2 - (sum_y * sum_y);
                if (denom_left <= 0.0 || denom_right <= 0.0)
                {
                    continue;
                }

                ColumnCorrelationStatistics corr;
                corr.table_id = table_id;
                corr.left_column_id = columns[left_idx].column_id;
                corr.right_column_id = columns[right_idx].column_id;
                corr.left_column_name = columns[left_idx].column_name;
                corr.right_column_name = columns[right_idx].column_name;
                corr.coefficient = numerator / std::sqrt(denom_left * denom_right);
                corr.sample_size = count;
                corr.last_analyzed_time = analyzed_time;
                {
                    std::lock_guard<std::mutex> lock(cache_mutex_);
                    correlation_stats_cache_[getCorrelationCacheKey(table_id,
                                                                   corr.left_column_id,
                                                                   corr.right_column_id)] = corr;
                }
                Status persist_status = storeCorrelationStatistic(corr, ctx);
                if (persist_status != Status::OK)
                {
                    DEBUG_LOG_DB("Failed to persist correlation statistics for " +
                                 corr.left_column_name + "/" + corr.right_column_name);
                }
            }
        }
    }

    auto StatisticsManager::computeExpressionStatistics(
        const ID &table_id,
        const std::vector<core::CatalogManager::ColumnInfo> &columns,
        const std::vector<std::vector<uint8_t>> &sample_rows,
        uint64_t analyzed_time,
        ErrorContext *ctx) -> void
    {
        for (const auto &column : columns)
        {
            const auto type = static_cast<core::DataType>(column.data_type);
            if (type != core::DataType::VARCHAR &&
                type != core::DataType::TEXT &&
                type != core::DataType::CHAR)
            {
                continue;
            }

            std::vector<std::vector<uint8_t>> column_values =
                extractColumnValues(table_id, column.column_id, sample_rows, columns, ctx);
            if (column_values.empty())
            {
                continue;
            }

            auto store_expression = [&](const ExpressionStatsDescriptor &descriptor) {
                std::vector<std::vector<uint8_t>> expr_values;
                expr_values.reserve(column_values.size());
                for (const auto &value : column_values)
                {
                    if (value.empty())
                    {
                        expr_values.push_back({});
                        continue;
                    }
                    std::string text;
                    if (!readStringValue(value, text))
                    {
                        return;
                    }
                    ::scratchbird::optimizer::applyExpressionStatsTextTransform(
                        text,
                        descriptor.function);
                    expr_values.push_back(encodeStringValue(text));
                }

                ColumnStatistics expr_stats;
                expr_stats.table_id = table_id;
                expr_stats.column_id = column.column_id;
                expr_stats.column_name = column.column_name;
                expr_stats.data_type = descriptor.result_data_type;
                applyColumnMetadata(column, expr_stats);
                expr_stats.num_rows = expr_values.size();
                expr_stats.sample_size = expr_values.size();
                expr_stats.sample_rate = 0.0f;
                expr_stats.last_analyzed_time = analyzed_time;
                uint64_t null_count = 0;
                uint64_t total_width = 0;
                for (const auto &value : expr_values)
                {
                    if (value.empty())
                    {
                        ++null_count;
                    }
                    else
                    {
                        total_width += value.size();
                    }
                }
                expr_stats.num_nulls = null_count;
                expr_stats.null_fraction = expr_values.empty()
                    ? 0.0f
                    : static_cast<float>(null_count) /
                          static_cast<float>(expr_values.size());
                expr_stats.avg_width = expr_values.empty()
                    ? 0.0f
                    : static_cast<float>(total_width) /
                          static_cast<float>(std::max<uint64_t>(1, expr_values.size() - null_count));
                expr_stats.num_distinct =
                    estimateNDistinct(expr_values, expr_stats.num_rows, expr_values.size());
                (void)generateHistogram(expr_values,
                                        32,
                                        HistogramType::EQUAL_HEIGHT,
                                        expr_stats.data_type,
                                        expr_stats.histogram_buckets,
                                        nullptr);
                expr_stats.histogram_type = expr_stats.histogram_buckets.empty()
                    ? HistogramType::NONE
                    : HistogramType::EQUAL_HEIGHT;
                expr_stats.histogram_bucket_count =
                    static_cast<uint32_t>(expr_stats.histogram_buckets.size());
                (void)identifyMCVs(expr_values, 32, expr_stats.mcv_list, nullptr);

                ExpressionStatistics info;
                const std::string expression_key =
                    canonicalExpressionStatsKey(descriptor);
                info.table_id = table_id;
                info.expression_key = expression_key;
                info.expression_contract_id = descriptor.contract_id;
                info.registry_contract_id = descriptor.registry_contract_id;
                info.coverage_contract_id = descriptor.coverage_contract_id;
                info.function_name = descriptor.function_name;
                info.base_column_name = descriptor.column_name;
                info.coverage_class = descriptor.coverage_class;
                info.input_data_type = type;
                info.result_data_type = descriptor.result_data_type;
                expr_stats.column_id =
                    makeSyntheticStatisticId(table_id,
                                             "EXPR",
                                             getExpressionCacheKey(table_id, expression_key));
                expr_stats.column_name = expression_key;
                info.stats = expr_stats;

                {
                    std::lock_guard<std::mutex> lock(cache_mutex_);
                    expression_stats_cache_[getExpressionCacheKey(table_id, expression_key)] =
                        info;
                }

                Status persist_status = storeColumnStatistics(expr_stats, ctx);
                if (persist_status != Status::OK)
                {
                    DEBUG_LOG_DB("Failed to persist expression statistics for " + expression_key);
                }
            };

            for (const auto &entry : expressionStatsRegistryEntries())
            {
                auto descriptor =
                    buildExpressionStatsDescriptor(entry.canonical_name,
                                                                 column.column_name,
                                                                 type);
                if (descriptor.has_value())
                {
                    store_expression(*descriptor);
                }
            }
        }
    }

    auto StatisticsManager::computeMultivariateStatistics(
        const ID &table_id,
        const std::vector<core::CatalogManager::ColumnInfo> &columns,
        const std::vector<std::vector<uint8_t>> &sample_rows,
        uint64_t analyzed_time,
        uint64_t table_row_estimate,
        ErrorContext *ctx) -> void
    {
        if (columns.size() < 2)
        {
            return;
        }

        std::vector<std::vector<std::vector<uint8_t>>> extracted_values(columns.size());
        for (size_t index = 0; index < columns.size(); ++index)
        {
            extracted_values[index] =
                extractColumnValues(table_id, columns[index].column_id, sample_rows, columns, ctx);
        }

        auto dependency_strength =
            [](const std::unordered_map<std::vector<uint8_t>,
                                        std::unordered_map<std::vector<uint8_t>,
                                                           uint64_t,
                                                           VectorHash>,
                                        VectorHash> &groups,
               uint64_t total_rows) -> double {
                if (total_rows == 0)
                {
                    return 0.0;
                }
                uint64_t supported_rows = 0;
                for (const auto &group : groups)
                {
                    uint64_t best = 0;
                    for (const auto &child : group.second)
                    {
                        best = std::max(best, child.second);
                    }
                    supported_rows += best;
                }
                return static_cast<double>(supported_rows) /
                       static_cast<double>(total_rows);
            };

        std::vector<size_t> subset_indices;
        const auto persist_subset =
            [&](const std::vector<size_t> &subset) -> void {
                size_t row_count = sample_rows.size();
                for (size_t index : subset)
                {
                    row_count = std::min(row_count, extracted_values[index].size());
                }

                std::vector<std::vector<uint8_t>> composite_values;
                composite_values.reserve(row_count);
                std::vector<std::unordered_map<std::vector<uint8_t>,
                                               std::unordered_map<std::vector<uint8_t>,
                                                                  uint64_t,
                                                                  VectorHash>,
                                               VectorHash>>
                    forward_dependencies(subset.size());
                std::vector<std::unordered_map<std::vector<uint8_t>,
                                               std::unordered_map<std::vector<uint8_t>,
                                                                  uint64_t,
                                                                  VectorHash>,
                                               VectorHash>>
                    reverse_dependencies(subset.size());

                for (size_t row = 0; row < row_count; ++row)
                {
                    std::vector<std::vector<uint8_t>> row_values;
                    row_values.reserve(subset.size());
                    bool complete = true;
                    for (size_t subset_index : subset)
                    {
                        const auto &value = extracted_values[subset_index][row];
                        if (value.empty())
                        {
                            complete = false;
                            break;
                        }
                        row_values.push_back(value);
                    }

                    if (!complete)
                    {
                        continue;
                    }

                    composite_values.push_back(encodeCompositeStatisticValue(row_values));
                    for (size_t pivot = 0; pivot < row_values.size(); ++pivot)
                    {
                        std::vector<std::vector<uint8_t>> dependent_values;
                        dependent_values.reserve(row_values.size() - 1);
                        for (size_t other = 0; other < row_values.size(); ++other)
                        {
                            if (other != pivot)
                            {
                                dependent_values.push_back(row_values[other]);
                            }
                        }

                        const std::vector<uint8_t> dependent_key =
                            encodeCompositeStatisticValue(dependent_values);
                        forward_dependencies[pivot][row_values[pivot]][dependent_key] += 1;
                        reverse_dependencies[pivot][dependent_key][row_values[pivot]] += 1;
                    }
                }

                if (composite_values.size() < 8)
                {
                    return;
                }

                MultivariateStatistics stats;
                stats.table_id = table_id;
                for (size_t subset_index : subset)
                {
                    stats.column_ids.push_back(columns[subset_index].column_id);
                    stats.column_names.push_back(columns[subset_index].column_name);
                }
                stats.sample_size = static_cast<uint64_t>(composite_values.size());
                stats.last_analyzed_time = analyzed_time;
                stats.ndistinct.column_ids = stats.column_ids;
                stats.ndistinct.column_names = stats.column_names;
                stats.ndistinct.sample_size = stats.sample_size;
                stats.ndistinct.last_analyzed_time = analyzed_time;
                stats.ndistinct.num_distinct = estimateNDistinct(
                    composite_values,
                    std::max<uint64_t>(table_row_estimate, composite_values.size()),
                    composite_values.size());
                stats.ndistinct.distinct_fraction =
                    stats.sample_size == 0
                        ? 0.0
                        : static_cast<double>(stats.ndistinct.num_distinct) /
                              static_cast<double>(std::max<uint64_t>(1, table_row_estimate));

                std::vector<MCVEntry> raw_mcv_list;
                if (identifyMCVs(composite_values, 64, raw_mcv_list, nullptr) == Status::OK)
                {
                    for (const auto &raw_entry : raw_mcv_list)
                    {
                        MultivariateMCVEntry entry;
                        if (!decodeCompositeStatisticValue(raw_entry.value_data, entry.values))
                        {
                            continue;
                        }
                        entry.frequency = raw_entry.frequency;
                        stats.mcv_list.push_back(std::move(entry));
                    }
                }

                for (size_t pivot = 0; pivot < subset.size(); ++pivot)
                {
                    FunctionalDependencyStatistics single_to_rest;
                    single_to_rest.determinant_column_ids = {stats.column_ids[pivot]};
                    single_to_rest.determinant_column_names = {stats.column_names[pivot]};
                    single_to_rest.strength =
                        dependency_strength(forward_dependencies[pivot], stats.sample_size);
                    single_to_rest.sample_size = stats.sample_size;
                    single_to_rest.last_analyzed_time = analyzed_time;

                    FunctionalDependencyStatistics rest_to_single;
                    rest_to_single.dependent_column_ids = {stats.column_ids[pivot]};
                    rest_to_single.dependent_column_names = {stats.column_names[pivot]};
                    rest_to_single.strength =
                        dependency_strength(reverse_dependencies[pivot], stats.sample_size);
                    rest_to_single.sample_size = stats.sample_size;
                    rest_to_single.last_analyzed_time = analyzed_time;

                    for (size_t other = 0; other < subset.size(); ++other)
                    {
                        if (other == pivot)
                        {
                            continue;
                        }
                        single_to_rest.dependent_column_ids.push_back(stats.column_ids[other]);
                        single_to_rest.dependent_column_names.push_back(stats.column_names[other]);
                        rest_to_single.determinant_column_ids.push_back(stats.column_ids[other]);
                        rest_to_single.determinant_column_names.push_back(
                            stats.column_names[other]);
                    }

                    if (subset.size() == 2 && pivot > 0)
                    {
                        continue;
                    }

                    stats.dependencies.push_back(std::move(single_to_rest));
                    stats.dependencies.push_back(std::move(rest_to_single));
                }

                {
                    std::lock_guard<std::mutex> lock(cache_mutex_);
                    for (size_t left = 0; left < stats.column_ids.size(); ++left)
                    {
                        for (size_t right = left + 1; right < stats.column_ids.size(); ++right)
                        {
                            const auto correlation_it = correlation_stats_cache_.find(
                                getCorrelationCacheKey(table_id,
                                                       stats.column_ids[left],
                                                       stats.column_ids[right]));
                            if (correlation_it != correlation_stats_cache_.end())
                            {
                                stats.pairwise_correlations.push_back(
                                    correlation_it->second);
                            }
                        }
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(cache_mutex_);
                    multivariate_stats_cache_[getMultivariateCacheKey(table_id,
                                                                     stats.column_ids)] = stats;
                }

                Status persist_status = storeMultivariateStatistics(stats, ctx);
                if (persist_status != Status::OK)
                {
                    std::string descriptor;
                    for (size_t index = 0; index < stats.column_names.size(); ++index)
                    {
                        if (index != 0)
                        {
                            descriptor += "/";
                        }
                        descriptor += stats.column_names[index];
                    }
                    DEBUG_LOG_DB("Failed to persist multivariate statistics for " +
                                 descriptor);
                }
            };

        const auto enumerate_subsets =
            [&](const auto &self, size_t start, size_t target_size) -> void {
                if (subset_indices.size() == target_size)
                {
                    persist_subset(subset_indices);
                    return;
                }

                for (size_t index = start; index < columns.size(); ++index)
                {
                    subset_indices.push_back(index);
                    self(self, index + 1, target_size);
                    subset_indices.pop_back();
                }
            };

        for (size_t subset_size = 2;
             subset_size <= std::min<size_t>(3, columns.size());
             ++subset_size)
        {
            enumerate_subsets(enumerate_subsets, 0, subset_size);
        }
    }

    auto StatisticsManager::extractColumnValues(
        const ID &table_id,
        const ID &column_id,
        const std::vector<std::vector<uint8_t>> &sample_rows,
        const std::vector<core::CatalogManager::ColumnInfo> &columns,
        ErrorContext *ctx) -> std::vector<std::vector<uint8_t>>
    {
        std::vector<std::vector<uint8_t>> column_values;
        column_values.reserve(sample_rows.size());

        // Find the target column index
        size_t target_column_idx = SIZE_MAX;
        core::CatalogManager::ColumnInfo target_column_info;

        for (size_t i = 0; i < columns.size(); i++)
        {
            if (std::memcmp(columns[i].column_id.bytes.data(), column_id.bytes.data(), 16) == 0)
            {
                target_column_idx = i;
                target_column_info = columns[i];
                break;
            }
        }

        if (target_column_idx == SIZE_MAX)
        {
            return column_values; // Empty vector
        }

        std::vector<bool> encryption_flags(columns.size(), false);
        auto *domain_mgr = db_->domain_manager();
        for (size_t i = 0; i < columns.size(); ++i)
        {
            if (columns[i].domain_id == core::ID{})
            {
                continue;
            }
            if (domain_mgr == nullptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "Domain manager unavailable for encryption lookup");
                return {};
            }
            core::DomainInfo domain;
            Status status = domain_mgr->getDomain(columns[i].domain_id, domain, ctx);
            if (status != Status::OK)
            {
                return {};
            }
            encryption_flags[i] = domain.security.encryption_enabled;
        }

        // Extract column values from each tuple
        for (const auto &tuple_data : sample_rows)
        {
            if (tuple_data.size() < sizeof(core::TupleHeader))
            {
                continue; // Skip malformed tuples
            }

            // Read TupleHeader
            const auto *header = reinterpret_cast<const core::TupleHeader *>(tuple_data.data());

            // Get null bitmap if present
            const uint8_t *null_bitmap = nullptr;
            if (header->hasNulls() && header->null_bitmap_offset > 0 &&
                header->null_bitmap_offset < tuple_data.size())
            {
                null_bitmap = tuple_data.data() + header->null_bitmap_offset;
            }

            // Check if target column is null
            bool is_null = false;
            if (null_bitmap)
            {
                size_t byte_offset = target_column_idx / 8;
                size_t bit_pos = target_column_idx % 8;
                is_null = (null_bitmap[byte_offset] & (1 << bit_pos)) != 0;
            }

            if (is_null)
            {
                column_values.push_back(std::vector<uint8_t>()); // Empty vector for NULL
                continue;
            }

            // Calculate data offset for target column
            size_t data_offset = sizeof(core::TupleHeader);
            if (header->hasNulls() && null_bitmap)
            {
                size_t bitmap_bytes = (columns.size() + 7) / 8;
                data_offset = header->null_bitmap_offset + bitmap_bytes;
            }

            bool malformed = false;

            // Skip columns before target
            for (size_t i = 0; i < target_column_idx; i++)
            {
                if (null_bitmap)
                {
                    size_t byte_offset = i / 8;
                    size_t bit_pos = i % 8;
                    if (null_bitmap[byte_offset] & (1 << bit_pos))
                    {
                        continue;
                    }
                }

                size_t col_size = 0;
                if (encryption_flags[i])
                {
                    size_t len_offset = data_offset;
                    uint32_t len = 0;
                    if (!core::readUint32LE(tuple_data.data(), tuple_data.size(), len_offset, len))
                    {
                        malformed = true;
                        break;
                    }
                    col_size = sizeof(uint32_t) + len;
                }
                else
                {
                    core::TypeInfo type_info = buildTypeInfo(columns[i]);
                    core::ErrorContext size_ctx;
                    core::Status size_status = core::computePlainValueSize(type_info.type,
                                                                           type_info,
                                                                           tuple_data.data() + data_offset,
                                                                           tuple_data.size() - data_offset,
                                                                           col_size,
                                                                           &size_ctx);
                    if (size_status != core::Status::OK)
                    {
                        malformed = true;
                        break;
                    }
                }

                if (data_offset + col_size > tuple_data.size())
                {
                    malformed = true;
                    break;
                }
                data_offset += col_size;
            }

            if (malformed)
            {
                continue;
            }

            core::DataType target_type = static_cast<core::DataType>(target_column_info.data_type);
            std::vector<uint8_t> value;
            size_t value_size = 0;

            if (encryption_flags[target_column_idx])
            {
                size_t len_offset = data_offset;
                uint32_t len = 0;
                if (!core::readUint32LE(tuple_data.data(), tuple_data.size(), len_offset, len))
                {
                    continue;
                }
                value_size = sizeof(uint32_t) + len;
            }
            else
            {
                core::TypeInfo type_info = buildTypeInfo(target_column_info);
                core::ErrorContext size_ctx;
                core::Status size_status = core::computePlainValueSize(type_info.type,
                                                                       type_info,
                                                                       tuple_data.data() + data_offset,
                                                                       tuple_data.size() - data_offset,
                                                                       value_size,
                                                                       &size_ctx);
                if (size_status != core::Status::OK)
                {
                    continue;
                }
            }

            if (data_offset + value_size > tuple_data.size())
            {
                continue;
            }

            value.resize(value_size);
            std::memcpy(value.data(), tuple_data.data() + data_offset, value_size);

            column_values.push_back(std::move(value));
        }

        return column_values;
    }

} // namespace scratchbird::optimizer
