/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/config.h"
#include "scratchbird/core/decimal.h"
#include "scratchbird/core/plain_value_reader.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/table_stats_manager.h"
#include "scratchbird/core/telemetry.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/mga_backout_engine.h"
#include "scratchbird/core/btree.h"
#include "scratchbird/core/brin_index.h"
#include "scratchbird/core/gin_index.h"
#include "scratchbird/core/gist_index.h"
#include "scratchbird/core/hnsw_index.h"
#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/oversized_value_lifecycle.h"
#include "scratchbird/core/lsm_tree_index.h"  // LSM Integration Phase 4
#include "scratchbird/core/rtree_index.h"  // R-Tree DML Integration
#include "scratchbird/core/spgist_index.h"
#include "scratchbird/core/bitmap_index.h"  // TASK-DML-8: Bitmap Index DML Integration
#include "scratchbird/core/columnstore.h"  // TASK-DML-7: Columnstore Index DML Integration
#include "scratchbird/core/inverted_index.h"
#include "scratchbird/core/plain_value_reader.h"
#include "scratchbird/core/toast.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/tid_resolver.h" // Sprint 4 Task 5.4.2
#include "scratchbird/core/index_key_extractor.h" // Phase 3 Task 3.2: Storage Layer TOAST Integration
#include "scratchbird/core/vector.h"
#include "scratchbird/core/gpid.h"
#include <cctype>
#include <cstring>
#include <algorithm>
#include <limits>
#include <new>

namespace scratchbird::core
{
    namespace
    {
        [[nodiscard]] auto lockModeNameLocal(LockMode mode) -> const char *
        {
            switch (mode)
            {
                case LockMode::LOCK_ACCESS_SHARE:
                    return "ACCESS_SHARE";
                case LockMode::LOCK_ROW_SHARE:
                    return "ROW_SHARE";
                case LockMode::LOCK_ROW_EXCLUSIVE:
                    return "ROW_EXCLUSIVE";
                case LockMode::LOCK_SHARE_UPDATE_EXCLUSIVE:
                    return "SHARE_UPDATE_EXCLUSIVE";
                case LockMode::LOCK_SHARE:
                    return "SHARE";
                case LockMode::LOCK_SHARE_ROW_EXCLUSIVE:
                    return "SHARE_ROW_EXCLUSIVE";
                case LockMode::LOCK_EXCLUSIVE:
                    return "EXCLUSIVE";
                case LockMode::LOCK_ACCESS_EXCLUSIVE:
                    return "ACCESS_EXCLUSIVE";
            }

            return "UNKNOWN";
        }

        constexpr uint32_t READAHEAD_MIN_PAGES = 2;
        constexpr uint32_t READAHEAD_MAX_PAGES = 32;
        constexpr uint32_t READAHEAD_SEQ_THRESHOLD = 3;
        constexpr uint32_t READAHEAD_GROWTH_FACTOR = 2;
        constexpr uint32_t BACK_VERSION_EXTENT_PAGES = 32;
        constexpr uint32_t BACK_VERSION_LOCALITY_BUCKET_PAGES = 128;

        struct BackVersionPlacementCandidate
        {
            uint32_t page_id = 0;
            uint8_t tier = std::numeric_limits<uint8_t>::max();
            uint32_t distance = std::numeric_limits<uint32_t>::max();
            bool valid = false;
        };

        [[nodiscard]] auto sameBackVersionExtent(uint32_t lhs, uint32_t rhs) -> bool
        {
            return (lhs / BACK_VERSION_EXTENT_PAGES) == (rhs / BACK_VERSION_EXTENT_PAGES);
        }

        [[nodiscard]] auto sameBackVersionBucket(uint32_t lhs, uint32_t rhs) -> bool
        {
            return (lhs / BACK_VERSION_LOCALITY_BUCKET_PAGES) ==
                   (rhs / BACK_VERSION_LOCALITY_BUCKET_PAGES);
        }

        [[nodiscard]] auto scoreBackVersionPlacementCandidate(uint32_t primary_page_id,
                                                              uint32_t candidate_page_id)
            -> BackVersionPlacementCandidate
        {
            BackVersionPlacementCandidate candidate;
            candidate.page_id = candidate_page_id;
            candidate.distance = (candidate_page_id > primary_page_id)
                                     ? (candidate_page_id - primary_page_id)
                                     : (primary_page_id - candidate_page_id);

            if (sameBackVersionExtent(primary_page_id, candidate_page_id) &&
                sameBackVersionBucket(primary_page_id, candidate_page_id))
            {
                candidate.tier = 0;
            }
            else if (sameBackVersionBucket(primary_page_id, candidate_page_id))
            {
                candidate.tier = 1;
            }
            else
            {
                candidate.tier = 2;
            }

            candidate.valid = true;
            return candidate;
        }

        [[nodiscard]] auto isBetterBackVersionPlacement(
            const BackVersionPlacementCandidate &candidate,
            const BackVersionPlacementCandidate &best) -> bool
        {
            if (!candidate.valid)
            {
                return false;
            }
            if (!best.valid)
            {
                return true;
            }
            if (candidate.tier != best.tier)
            {
                return candidate.tier < best.tier;
            }
            if (candidate.distance != best.distance)
            {
                return candidate.distance < best.distance;
            }
            return candidate.page_id < best.page_id;
        }

        [[nodiscard]] auto indexUsesArrayUniqueness(const CatalogManager::IndexInfo &index_info,
                                                    CatalogManager *catalog_manager) -> bool
        {
            if (catalog_manager == nullptr || index_info.index_params_oid == ID{})
            {
                return false;
            }

            std::string params_blob;
            ErrorContext params_ctx;
            if (catalog_manager->loadStringFromToast(index_info.index_params_oid, 0, params_blob,
                                                     &params_ctx) != Status::OK)
            {
                return false;
            }

            return params_blob.find("sb.array_uniqueness=") != std::string::npos;
        }

        struct PreparedStableHeadTuple
        {
            const uint8_t *storage_tuple_data = nullptr;
            uint32_t storage_tuple_size = 0;
            std::vector<uint8_t> owned_storage_tuple;
            bool toasted = false;
            ID toast_value_id{};
        };

        auto prepareStableHeadTupleForMutation(const uint8_t *logical_tuple_data,
                                               uint32_t logical_tuple_size,
                                               uint64_t new_xmin,
                                               Database *db,
                                               ToastManager *toast_mgr,
                                               PreparedStableHeadTuple *prepared_out,
                                               ErrorContext *ctx) -> Status
        {
            if (prepared_out == nullptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Prepared stable-head tuple output is null");
                return Status::INVALID_ARGUMENT;
            }

            prepared_out->storage_tuple_data = logical_tuple_data;
            prepared_out->storage_tuple_size = logical_tuple_size;
            prepared_out->owned_storage_tuple.clear();
            prepared_out->toasted = false;
            prepared_out->toast_value_id = ID{};

            if (logical_tuple_data == nullptr || logical_tuple_size < sizeof(TupleHeader))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Logical tuple image is invalid for mutation preparation");
                return Status::INVALID_ARGUMENT;
            }

            if (toast_mgr == nullptr || db == nullptr ||
                !ToastManager::shouldToast(logical_tuple_size, db->page_size()))
            {
                return Status::OK;
            }

            if (logical_tuple_size <= sizeof(TupleHeader))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Tuple too small to TOAST during mutation preparation");
                return Status::INVALID_ARGUMENT;
            }

            try
            {
                prepared_out->owned_storage_tuple.resize(sizeof(TupleHeader) +
                                                         sizeof(ToastPointer));
            }
            catch (const std::bad_alloc &)
            {
                SET_ERROR_CONTEXT(ctx, Status::OOM,
                                  "Out of memory allocating prepared mutation TOAST buffer");
                return Status::OOM;
            }

            auto *new_hdr =
                reinterpret_cast<TupleHeader *>(prepared_out->owned_storage_tuple.data());
            const auto *orig_hdr = reinterpret_cast<const TupleHeader *>(logical_tuple_data);
            *new_hdr = *orig_hdr;

            ToastPointer toast_ptr{};
            Status toast_status = toast_mgr->toastValue(logical_tuple_data + sizeof(TupleHeader),
                                                        logical_tuple_size - sizeof(TupleHeader),
                                                        ToastStrategy::EXTERNAL,
                                                        new_xmin,
                                                        &toast_ptr,
                                                        ctx);
            if (toast_status != Status::OK)
            {
                return toast_status;
            }

            std::memcpy(prepared_out->owned_storage_tuple.data() + sizeof(TupleHeader),
                        &toast_ptr,
                        sizeof(ToastPointer));

            prepared_out->storage_tuple_data = prepared_out->owned_storage_tuple.data();
            prepared_out->storage_tuple_size =
                static_cast<uint32_t>(prepared_out->owned_storage_tuple.size());
            prepared_out->toasted = true;
            prepared_out->toast_value_id = toast_ptr.lob_uuid;
            return Status::OK;
        }
    }

    StorageEngine::StorageEngine(Database *db)
        : db_(db), buffer_pool_(db->buffer_pool()), page_manager_(db->page_manager()),
          catalog_manager_(db->catalog_manager())
    {
    }

    StorageEngine::~StorageEngine() = default;

    void StorageEngine::publishFragmentationAdvisory(const ID &table_id,
                                                     uint32_t page_id,
                                                     const FragmentationAdvisory &advisory)
    {
        std::lock_guard<std::mutex> lock(fragmentation_advisory_mutex_);
        fragmentation_advisories_[table_id][page_id] = advisory;
    }

    void StorageEngine::clearFragmentationAdvisory(const ID &table_id, uint32_t page_id)
    {
        std::lock_guard<std::mutex> lock(fragmentation_advisory_mutex_);
        auto table_it = fragmentation_advisories_.find(table_id);
        if (table_it == fragmentation_advisories_.end())
        {
            return;
        }

        table_it->second.erase(page_id);
        if (table_it->second.empty())
        {
            fragmentation_advisories_.erase(table_it);
        }
    }

    auto StorageEngine::getFragmentationAdvisory(const ID &table_id,
                                                 uint32_t page_id,
                                                 FragmentationAdvisory *advisory_out) const -> bool
    {
        if (advisory_out == nullptr)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(fragmentation_advisory_mutex_);
        auto table_it = fragmentation_advisories_.find(table_id);
        if (table_it == fragmentation_advisories_.end())
        {
            return false;
        }

        auto page_it = table_it->second.find(page_id);
        if (page_it == table_it->second.end())
        {
            return false;
        }

        *advisory_out = page_it->second;
        return true;
    }

    auto StorageEngine::listFragmentationAdvisories(
        std::vector<FragmentationAdvisorySnapshot>& advisories_out) const -> Status
    {
        advisories_out.clear();

        std::lock_guard<std::mutex> lock(fragmentation_advisory_mutex_);
        size_t advisory_count = 0;
        for (const auto& [table_id, page_map] : fragmentation_advisories_)
        {
            advisory_count += page_map.size();
        }

        advisories_out.reserve(advisory_count);
        for (const auto& [table_id, page_map] : fragmentation_advisories_)
        {
            for (const auto& [page_id, advisory] : page_map)
            {
                (void)page_id;
                FragmentationAdvisorySnapshot snapshot{};
                snapshot.table_id = table_id;
                snapshot.advisory = advisory;
                advisories_out.push_back(std::move(snapshot));
            }
        }

        std::sort(advisories_out.begin(),
                  advisories_out.end(),
                  [](const FragmentationAdvisorySnapshot& lhs,
                     const FragmentationAdvisorySnapshot& rhs) {
                      const std::string lhs_table_id = lhs.table_id.toString();
                      const std::string rhs_table_id = rhs.table_id.toString();
                      if (lhs_table_id != rhs_table_id)
                      {
                          return lhs_table_id < rhs_table_id;
                      }
                      return lhs.advisory.page_id < rhs.advisory.page_id;
                  });
        return Status::OK;
    }

    void StorageEngine::publishIndexCleanupPublication(
        const IndexCleanupPublicationRecord& publication)
    {
        std::lock_guard<std::mutex> lock(cleanup_publication_mutex_);
        cleanup_publications_[publication.index_id][publication.page_id] = publication;
    }

    auto StorageEngine::listIndexCleanupPublications(
        std::vector<IndexCleanupPublicationRecord>& publications_out) const -> Status
    {
        publications_out.clear();

        std::lock_guard<std::mutex> lock(cleanup_publication_mutex_);
        size_t publication_count = 0;
        for (const auto& [index_id, page_map] : cleanup_publications_)
        {
            (void)index_id;
            publication_count += page_map.size();
        }

        publications_out.reserve(publication_count);
        for (const auto& [index_id, page_map] : cleanup_publications_)
        {
            (void)index_id;
            for (const auto& [page_id, publication] : page_map)
            {
                (void)page_id;
                publications_out.push_back(publication);
            }
        }

        std::sort(publications_out.begin(),
                  publications_out.end(),
                  [](const IndexCleanupPublicationRecord& lhs,
                     const IndexCleanupPublicationRecord& rhs) {
                      const std::string lhs_table_id = lhs.table_id.toString();
                      const std::string rhs_table_id = rhs.table_id.toString();
                      if (lhs_table_id != rhs_table_id)
                      {
                          return lhs_table_id < rhs_table_id;
                      }
                      if (lhs.page_id != rhs.page_id)
                      {
                          return lhs.page_id < rhs.page_id;
                      }
                      return lhs.index_name < rhs.index_name;
                  });
        return Status::OK;
    }

    // LSM Integration Phase 4: Helper method to insert into any index type
    namespace {
        auto metricDbLabel(const Database *db) -> std::string
        {
            return db != nullptr ? db->uuid().toString() : std::string();
        }

        auto metricRelationLabel(CatalogManager *catalog, const ID &table_id) -> std::string
        {
            if (catalog != nullptr)
            {
                CatalogManager::TableInfo table_info;
                ErrorContext ctx;
                if (catalog->getTable(table_id, table_info, &ctx) == Status::OK &&
                    !table_info.table_name.empty())
                {
                    return table_info.table_name;
                }
            }
            return table_id.toString();
        }

        void incrementCanonicalCounter(const std::string &metric_name,
                                       const std::vector<std::string> &labels)
        {
            auto *counter = dynamic_cast<Counter *>(
                MetricsRegistry::getInstance().get(metric_name));
            if (counter != nullptr)
            {
                counter->inc(1.0, labels);
            }
        }

        std::vector<uint8_t> encodeLsmValue(const TID &tid)
        {
            std::vector<uint8_t> value;
            value.resize(sizeof(uint64_t) + sizeof(uint16_t));

            uint64_t gpid = tid.gpid;
            for (size_t i = 0; i < sizeof(uint64_t); ++i)
            {
                value[i] = static_cast<uint8_t>((gpid >> (i * 8)) & 0xFF);
            }

            uint16_t slot = tid.slot;
            value[sizeof(uint64_t)] = static_cast<uint8_t>(slot & 0xFF);
            value[sizeof(uint64_t) + 1] = static_cast<uint8_t>((slot >> 8) & 0xFF);

            return value;
        }

        Status insertIntoIndex(
            CatalogManager::IndexType index_type,
            void *index_ptr,
            const std::vector<uint8_t> &key,
            const TID &tid,
            uint64_t xid,
            ErrorContext *ctx)
        {
            if (!index_ptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null index pointer");
                return Status::INVALID_ARGUMENT;
            }

            switch (index_type)
            {
                case CatalogManager::IndexType::BTREE:
                case CatalogManager::IndexType::STL_SORT:
                case CatalogManager::IndexType::ART:
                case CatalogManager::IndexType::MONGODB_GEO_HAYSTACK:
                case CatalogManager::IndexType::NEO4J_RANGE:
                case CatalogManager::IndexType::NEO4J_POINT:
                case CatalogManager::IndexType::REDIS_LIST:
                case CatalogManager::IndexType::REDIS_ZSET:
                case CatalogManager::IndexType::REDIS_STREAM:
                {
                    auto *btree = static_cast<BTree*>(index_ptr);
                    return btree->insert(key, tid, xid, ctx);
                }

                case CatalogManager::IndexType::LSM:
                {
                    auto *lsm = static_cast<LSMTreeIndex*>(index_ptr);
                    // LSM-Tree: store TID as value payload
                    auto value = encodeLsmValue(tid);
                    return lsm->put(key, value, xid, ctx);
                }

                case CatalogManager::IndexType::HASH:
                case CatalogManager::IndexType::REDIS_STRING:
                case CatalogManager::IndexType::REDIS_HASH:
                case CatalogManager::IndexType::REDIS_SET:
                case CatalogManager::IndexType::REDIS_HLL:
                {
                    auto *hash = static_cast<HashIndex*>(index_ptr);
                    return hash->insert(key.data(), key.size(), tid, xid, ctx);
                }

                case CatalogManager::IndexType::RTREE:
                {
                    auto *rtree = static_cast<RTreeIndex*>(index_ptr);
                    // R-Tree insert expects: key (serialized bounding box), tid, xmin
                    return rtree->insert(key, tid, xid, ctx);
                }

                case CatalogManager::IndexType::MONGODB_2D:
                case CatalogManager::IndexType::MONGODB_2DSPHERE:
                case CatalogManager::IndexType::MONGODB_2DSPHERE_BUCKET:
                case CatalogManager::IndexType::REDIS_GEO:
                {
                    auto *rtree = static_cast<RTreeIndex*>(index_ptr);
                    return rtree->insert(key, tid, xid, ctx);
                }

                case CatalogManager::IndexType::BITMAP:
                case CatalogManager::IndexType::NEO4J_LOOKUP:
                case CatalogManager::IndexType::REDIS_BITMAP:
                {
                    // TASK-DML-8: Bitmap Index DML Integration
                    // Bitmap indexes store value → bitmap mapping for low-cardinality columns
                    // Insert tuple into bitmap for this value
                    auto *bitmap = static_cast<BitmapIndex*>(index_ptr);

                    // Bitmap insert expects: value_data, value_len, tid, ctx
                    // The 'key' vector contains the serialized indexed value
                    return bitmap->insert(key.data(), key.size(), tid, ctx);
                }

                case CatalogManager::IndexType::GIN:
                {
                    auto *gin = static_cast<GinIndex*>(index_ptr);
                    auto extractor = [](const void *data, size_t len) -> std::vector<std::vector<uint8_t>> {
                        std::vector<std::vector<uint8_t>> keys;
                        if (data && len > 0)
                        {
                            const auto *bytes = static_cast<const uint8_t *>(data);
                            keys.emplace_back(bytes, bytes + len);
                        }
                        return keys;
                    };
                    return gin->insert(key.data(), key.size(), tid, extractor, ctx);
                }

                case CatalogManager::IndexType::GIST:
                {
                    auto *gist = static_cast<GiSTIndex*>(index_ptr);
                    GiSTPredicate predicate(key, 0);
                    return gist->insert(predicate, tid, xid, ctx);
                }

                case CatalogManager::IndexType::BRIN:
                {
                    auto *brin = static_cast<BrinIndex*>(index_ptr);
                    uint32_t block_number = static_cast<uint32_t>(getPageNumber(tid.gpid));
                    return brin->insert(key, block_number, ctx);
                }

                case CatalogManager::IndexType::ZONEMAP:
                case CatalogManager::IndexType::BLOOM:
                {
                    auto *brin = static_cast<BrinIndex*>(index_ptr);
                    uint32_t block_number = static_cast<uint32_t>(getPageNumber(tid.gpid));
                    return brin->insert(key, block_number, ctx);
                }

                case CatalogManager::IndexType::SPGIST:
                {
                    auto *spgist = static_cast<SPGiSTIndex*>(index_ptr);
                    return spgist->insert(key, tid, xid, ctx);
                }

                case CatalogManager::IndexType::HNSW:
                case CatalogManager::IndexType::NEO4J_VECTOR:
                {
                    auto *hnsw = static_cast<HnswIndex*>(index_ptr);
                    if (key.empty())
                    {
                        return Status::OK;
                    }
                    auto decoded = Vector::decode(key);
                    if (!decoded)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Invalid vector encoding for HNSW index");
                        return Status::INVALID_ARGUMENT;
                    }
                    return hnsw->insert(*decoded, tid, ctx);
                }

                case CatalogManager::IndexType::IVF:
                case CatalogManager::IndexType::VECTOR_FLAT:
                case CatalogManager::IndexType::VECTOR_BIN_FLAT:
                case CatalogManager::IndexType::IVF_FLAT:
                case CatalogManager::IndexType::BIN_IVF_FLAT:
                case CatalogManager::IndexType::IVF_PQ:
                case CatalogManager::IndexType::IVF_SQ8:
                case CatalogManager::IndexType::IVF_SQ8_HYBRID:
                case CatalogManager::IndexType::RHNSW_PQ:
                case CatalogManager::IndexType::RHNSW_SQ:
                case CatalogManager::IndexType::ANNOY:
                case CatalogManager::IndexType::NSG:
                case CatalogManager::IndexType::DISKANN:
                case CatalogManager::IndexType::SCANN:
                case CatalogManager::IndexType::GPU_CAGRA:
                {
                    auto *hnsw = static_cast<HnswIndex*>(index_ptr);
                    if (key.empty())
                    {
                        return Status::OK;
                    }
                    auto decoded = Vector::decode(key);
                    if (!decoded)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Invalid vector encoding for vector-family index");
                        return Status::INVALID_ARGUMENT;
                    }
                    return hnsw->insert(*decoded, tid, ctx);
                }

                case CatalogManager::IndexType::FULLTEXT:
                case CatalogManager::IndexType::INVERTED:
                case CatalogManager::IndexType::MONGODB_WILDCARD:
                case CatalogManager::IndexType::MONGODB_ENCRYPTED_RANGE:
                case CatalogManager::IndexType::NEO4J_TEXT:
                case CatalogManager::IndexType::CASSANDRA_SASI:
                case CatalogManager::IndexType::CASSANDRA_SAI:
                case CatalogManager::IndexType::TRIE:
                case CatalogManager::IndexType::NGRAM:
                case CatalogManager::IndexType::SPARSE_INVERTED:
                case CatalogManager::IndexType::SPARSE_WAND:
                case CatalogManager::IndexType::MINHASH_LSH:
                {
                    auto *inverted = static_cast<InvertedIndex*>(index_ptr);
                    std::string text;
                    size_t offset = 0;
                    while (offset < key.size())
                    {
                        uint32_t len = 0;
                        if (!readUint32LE(key.data(), key.size(), offset, len))
                        {
                            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                              "FULLTEXT key length prefix invalid");
                            return Status::DATA_CORRUPTED;
                        }
                        if (offset + len > key.size())
                        {
                            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                              "FULLTEXT key length exceeds payload");
                            return Status::DATA_CORRUPTED;
                        }
                        if (!text.empty())
                        {
                            text.push_back(' ');
                        }
                        text.append(reinterpret_cast<const char*>(key.data() + offset), len);
                        offset += len;
                    }
                    return inverted->insert(text.data(), text.size(), tid, ctx);
                }

                default:
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unknown index type");
                    return Status::INVALID_ARGUMENT;
            }
        }

        Status removeFromIndex(
            CatalogManager::IndexType index_type,
            void *index_ptr,
            const std::vector<uint8_t> &key,
            const TID &tid,
            uint64_t xid,
            ErrorContext *ctx)
        {
            if (!index_ptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null index pointer");
                return Status::INVALID_ARGUMENT;
            }

            switch (index_type)
            {
                case CatalogManager::IndexType::BTREE:
                case CatalogManager::IndexType::STL_SORT:
                case CatalogManager::IndexType::ART:
                case CatalogManager::IndexType::MONGODB_GEO_HAYSTACK:
                case CatalogManager::IndexType::NEO4J_RANGE:
                case CatalogManager::IndexType::NEO4J_POINT:
                case CatalogManager::IndexType::REDIS_LIST:
                case CatalogManager::IndexType::REDIS_ZSET:
                case CatalogManager::IndexType::REDIS_STREAM:
                {
                    auto *btree = static_cast<BTree*>(index_ptr);
                    return btree->remove(key, tid, xid, ctx);
                }

                case CatalogManager::IndexType::LSM:
                {
                    auto *lsm = static_cast<LSMTreeIndex*>(index_ptr);
                    // LSM-Tree: remove by key
                    return lsm->remove(key, xid, ctx);
                }

                case CatalogManager::IndexType::HASH:
                case CatalogManager::IndexType::REDIS_STRING:
                case CatalogManager::IndexType::REDIS_HASH:
                case CatalogManager::IndexType::REDIS_SET:
                case CatalogManager::IndexType::REDIS_HLL:
                {
                    auto *hash = static_cast<HashIndex*>(index_ptr);
                    return hash->remove(key.data(), key.size(), tid, xid, ctx);
                }

                case CatalogManager::IndexType::RTREE:
                {
                    auto *rtree = static_cast<RTreeIndex*>(index_ptr);
                    // R-Tree remove expects: key (serialized bounding box), tid, xmax
                    // xid here represents xmax (transaction that deleted the entry)
                    return rtree->remove(key, tid, xid, ctx);
                }

                case CatalogManager::IndexType::MONGODB_2D:
                case CatalogManager::IndexType::MONGODB_2DSPHERE:
                case CatalogManager::IndexType::MONGODB_2DSPHERE_BUCKET:
                case CatalogManager::IndexType::REDIS_GEO:
                {
                    auto *rtree = static_cast<RTreeIndex*>(index_ptr);
                    return rtree->remove(key, tid, xid, ctx);
                }

                case CatalogManager::IndexType::BITMAP:
                case CatalogManager::IndexType::NEO4J_LOOKUP:
                case CatalogManager::IndexType::REDIS_BITMAP:
                {
                    // TASK-DML-8: Bitmap Index DML Integration
                    // Bitmap remove marks tuple as deleted in ALL bitmaps (value-independent)
                    // Per Firebird MGA: Logical deletion with xmax marking, NO physical removal
                    auto *bitmap = static_cast<BitmapIndex*>(index_ptr);

                    // Bitmap remove expects: tid, ctx
                    // Note: The 'key' parameter is not used for bitmap remove because
                    // bitmap->remove() scans all dictionary entries and marks the TID
                    // as deleted (sets xmax) in whichever bitmap contains it
                    return bitmap->remove(tid, ctx);
                }

                case CatalogManager::IndexType::GIN:
                {
                    auto *gin = static_cast<GinIndex*>(index_ptr);
                    auto extractor = [](const void *data, size_t len) -> std::vector<std::vector<uint8_t>> {
                        std::vector<std::vector<uint8_t>> keys;
                        if (data && len > 0)
                        {
                            const auto *bytes = static_cast<const uint8_t *>(data);
                            keys.emplace_back(bytes, bytes + len);
                        }
                        return keys;
                    };
                    return gin->remove(key.data(), key.size(), tid, extractor, xid, ctx);
                }

                case CatalogManager::IndexType::GIST:
                {
                    auto *gist = static_cast<GiSTIndex*>(index_ptr);
                    GiSTPredicate predicate(key, 0);
                    return gist->remove(predicate, tid, xid, ctx);
                }

                case CatalogManager::IndexType::BRIN:
                {
                    auto *brin = static_cast<BrinIndex*>(index_ptr);
                    uint32_t block_number = static_cast<uint32_t>(getPageNumber(tid.gpid));
                    return brin->remove(key, block_number, ctx);
                }

                case CatalogManager::IndexType::ZONEMAP:
                case CatalogManager::IndexType::BLOOM:
                {
                    auto *brin = static_cast<BrinIndex*>(index_ptr);
                    uint32_t block_number = static_cast<uint32_t>(getPageNumber(tid.gpid));
                    return brin->remove(key, block_number, ctx);
                }

                case CatalogManager::IndexType::SPGIST:
                {
                    auto *spgist = static_cast<SPGiSTIndex*>(index_ptr);
                    return spgist->remove(key, tid, xid, ctx);
                }

                case CatalogManager::IndexType::HNSW:
                case CatalogManager::IndexType::NEO4J_VECTOR:
                {
                    auto *hnsw = static_cast<HnswIndex*>(index_ptr);
                    return hnsw->remove(tid, ctx);
                }

                case CatalogManager::IndexType::IVF:
                case CatalogManager::IndexType::VECTOR_FLAT:
                case CatalogManager::IndexType::VECTOR_BIN_FLAT:
                case CatalogManager::IndexType::IVF_FLAT:
                case CatalogManager::IndexType::BIN_IVF_FLAT:
                case CatalogManager::IndexType::IVF_PQ:
                case CatalogManager::IndexType::IVF_SQ8:
                case CatalogManager::IndexType::IVF_SQ8_HYBRID:
                case CatalogManager::IndexType::RHNSW_PQ:
                case CatalogManager::IndexType::RHNSW_SQ:
                case CatalogManager::IndexType::ANNOY:
                case CatalogManager::IndexType::NSG:
                case CatalogManager::IndexType::DISKANN:
                case CatalogManager::IndexType::SCANN:
                case CatalogManager::IndexType::GPU_CAGRA:
                {
                    auto *hnsw = static_cast<HnswIndex*>(index_ptr);
                    return hnsw->remove(tid, ctx);
                }

                case CatalogManager::IndexType::FULLTEXT:
                case CatalogManager::IndexType::INVERTED:
                case CatalogManager::IndexType::MONGODB_WILDCARD:
                case CatalogManager::IndexType::MONGODB_ENCRYPTED_RANGE:
                case CatalogManager::IndexType::NEO4J_TEXT:
                case CatalogManager::IndexType::CASSANDRA_SASI:
                case CatalogManager::IndexType::CASSANDRA_SAI:
                case CatalogManager::IndexType::TRIE:
                case CatalogManager::IndexType::NGRAM:
                case CatalogManager::IndexType::SPARSE_INVERTED:
                case CatalogManager::IndexType::SPARSE_WAND:
                case CatalogManager::IndexType::MINHASH_LSH:
                {
                    auto *inverted = static_cast<InvertedIndex*>(index_ptr);
                    std::string text;
                    size_t offset = 0;
                    while (offset < key.size())
                    {
                        uint32_t len = 0;
                        if (!readUint32LE(key.data(), key.size(), offset, len))
                        {
                            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                              "FULLTEXT key length prefix invalid");
                            return Status::DATA_CORRUPTED;
                        }
                        if (offset + len > key.size())
                        {
                            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                              "FULLTEXT key length exceeds payload");
                            return Status::DATA_CORRUPTED;
                        }
                        if (!text.empty())
                        {
                            text.push_back(' ');
                        }
                        text.append(reinterpret_cast<const char*>(key.data() + offset), len);
                        offset += len;
                    }
                    return inverted->remove(text.data(), text.size(), tid, xid, ctx);
                }

                default:
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unknown index type");
                    return Status::INVALID_ARGUMENT;
            }
        }

        enum class ExactIndexRetirementMode : uint8_t
        {
            SOFT_DELETE,
            HARD_REMOVE,
        };

        Status retireExactIndexEntry(CatalogManager::IndexType index_type,
                                     void *index_ptr,
                                     const std::vector<uint8_t> &key,
                                     const TID &tid,
                                     uint64_t xid,
                                     ExactIndexRetirementMode mode,
                                     ErrorContext *ctx)
        {
            if (!index_ptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null index pointer");
                return Status::INVALID_ARGUMENT;
            }

            switch (index_type)
            {
                case CatalogManager::IndexType::BTREE:
                case CatalogManager::IndexType::STL_SORT:
                case CatalogManager::IndexType::ART:
                case CatalogManager::IndexType::MONGODB_GEO_HAYSTACK:
                case CatalogManager::IndexType::NEO4J_RANGE:
                case CatalogManager::IndexType::NEO4J_POINT:
                case CatalogManager::IndexType::REDIS_LIST:
                case CatalogManager::IndexType::REDIS_ZSET:
                case CatalogManager::IndexType::REDIS_STREAM:
                {
                    auto *btree = static_cast<BTree *>(index_ptr);
                    return mode == ExactIndexRetirementMode::SOFT_DELETE
                        ? btree->markDeleted(key, tid, xid, ctx)
                        : btree->purge(key, tid, ctx);
                }

                case CatalogManager::IndexType::HASH:
                case CatalogManager::IndexType::REDIS_STRING:
                case CatalogManager::IndexType::REDIS_HASH:
                case CatalogManager::IndexType::REDIS_SET:
                case CatalogManager::IndexType::REDIS_HLL:
                {
                    auto *hash = static_cast<HashIndex *>(index_ptr);
                    return mode == ExactIndexRetirementMode::SOFT_DELETE
                        ? hash->remove(key.data(), key.size(), tid, xid, ctx)
                        : hash->purge(key.data(), key.size(), tid, ctx);
                }

                default:
                    return removeFromIndex(index_type, index_ptr, key, tid, xid, ctx);
            }
        }

        Status restoreExactIndexEntry(CatalogManager::IndexType index_type,
                                      void *index_ptr,
                                      const std::vector<uint8_t> &key,
                                      const TID &tid,
                                      uint64_t deleting_xid,
                                      ErrorContext *ctx)
        {
            if (!index_ptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null index pointer");
                return Status::INVALID_ARGUMENT;
            }

            switch (index_type)
            {
                case CatalogManager::IndexType::BTREE:
                case CatalogManager::IndexType::STL_SORT:
                case CatalogManager::IndexType::ART:
                case CatalogManager::IndexType::MONGODB_GEO_HAYSTACK:
                case CatalogManager::IndexType::NEO4J_RANGE:
                case CatalogManager::IndexType::NEO4J_POINT:
                case CatalogManager::IndexType::REDIS_LIST:
                case CatalogManager::IndexType::REDIS_ZSET:
                case CatalogManager::IndexType::REDIS_STREAM:
                {
                    auto *btree = static_cast<BTree *>(index_ptr);
                    return btree->restoreDeleted(key, tid, deleting_xid, ctx);
                }

                case CatalogManager::IndexType::HASH:
                case CatalogManager::IndexType::REDIS_STRING:
                case CatalogManager::IndexType::REDIS_HASH:
                case CatalogManager::IndexType::REDIS_SET:
                case CatalogManager::IndexType::REDIS_HLL:
                {
                    auto *hash = static_cast<HashIndex *>(index_ptr);
                    return hash->restoreDeleted(key.data(), key.size(), tid, deleting_xid, ctx);
                }

                default:
                    return Status::NOT_FOUND;
            }
        }

        TypeInfo buildTypeInfo(const CatalogManager::ColumnInfo &column)
        {
            TypeInfo info(static_cast<DataType>(column.data_type));
            uint32_t precision = column.type_precision != 0 ? column.type_precision
                                                            : column.max_length;
            info.precision = precision;
            info.scale = column.type_scale;
            info.with_timezone = column.with_timezone;
            info.timezone_hint = column.timezone_hint;
            return info;
        }

        Status computeColumnLayout(const uint8_t *tuple_data,
                                   uint32_t tuple_size,
                                   const std::vector<CatalogManager::ColumnInfo> &columns,
                                   DomainManager *domain_mgr,
                                   std::vector<size_t> &column_offsets,
                                   std::vector<size_t> &column_sizes,
                                   ErrorContext *ctx)
        {
            if (tuple_data == nullptr || tuple_size < sizeof(TupleHeader))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Tuple data is invalid");
                return Status::INVALID_ARGUMENT;
            }

            column_offsets.clear();
            column_sizes.clear();
            column_offsets.reserve(columns.size());
            column_sizes.reserve(columns.size());

            const auto *header = reinterpret_cast<const TupleHeader *>(tuple_data);
            size_t data_offset = sizeof(TupleHeader);

            const uint8_t *null_bitmap = nullptr;
            if (header->hasNulls() && header->null_bitmap_offset > 0 &&
                header->null_bitmap_offset < tuple_size)
            {
                null_bitmap = tuple_data + header->null_bitmap_offset;
                size_t bitmap_bytes = (columns.size() + 7) / 8;
                data_offset = header->null_bitmap_offset + bitmap_bytes;
            }

            size_t current_offset = data_offset;
            for (size_t i = 0; i < columns.size(); i++)
            {
                bool is_null = false;
                if (null_bitmap)
                {
                    size_t byte_offset = i / 8;
                    size_t bit_pos = i % 8;
                    is_null = (null_bitmap[byte_offset] & (1 << bit_pos)) != 0;
                }

                if (is_null)
                {
                    column_offsets.push_back(0);
                    column_sizes.push_back(0);
                    continue;
                }

                bool is_encrypted = false;
                if (domain_mgr != nullptr && columns[i].domain_id != ID{})
                {
                    DomainInfo domain;
                    Status status = domain_mgr->getDomain(columns[i].domain_id, domain, ctx);
                    if (status != Status::OK)
                    {
                        return status;
                    }
                    is_encrypted = domain.security.encryption_enabled;
                }

                size_t col_size = 0;
                if (is_encrypted)
                {
                    size_t len_offset = current_offset;
                    uint32_t len = 0;
                    if (!readUint32LE(tuple_data, tuple_size, len_offset, len))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Encrypted value length out of bounds");
                        return Status::DATA_CORRUPTED;
                    }
                    col_size = sizeof(uint32_t) + len;
                    if (current_offset + col_size > tuple_size)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Encrypted value exceeds tuple bounds");
                        return Status::DATA_CORRUPTED;
                    }
                }
                else
                {
                    TypeInfo type_info = buildTypeInfo(columns[i]);
                    Status size_status = computePlainValueSize(type_info.type,
                                                               type_info,
                                                               tuple_data + current_offset,
                                                               tuple_size - current_offset,
                                                               col_size,
                                                               ctx);
                    if (size_status != Status::OK)
                    {
                        return size_status;
                    }
                }

                column_offsets.push_back(current_offset);
                column_sizes.push_back(col_size);
                current_offset += col_size;
            }

            return Status::OK;
        }

        auto materializeTupleForKeyExtraction(const uint8_t *tuple_data,
                                              uint32_t tuple_size,
                                              ToastManager *toast_mgr,
                                              uint64_t xid,
                                              std::vector<uint8_t> &owned_tuple,
                                              const uint8_t **materialized_data,
                                              uint32_t *materialized_size,
                                              ErrorContext *ctx) -> Status
        {
            if (materialized_data == nullptr || materialized_size == nullptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Invalid materialized tuple outputs");
                return Status::INVALID_ARGUMENT;
            }

            *materialized_data = tuple_data;
            *materialized_size = tuple_size;
            owned_tuple.clear();

            if (tuple_data == nullptr || tuple_size < sizeof(TupleHeader))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Tuple data is invalid");
                return Status::INVALID_ARGUMENT;
            }

            const auto *tuple_hdr = reinterpret_cast<const TupleHeader *>(tuple_data);
            const uint8_t *payload = tuple_data + sizeof(TupleHeader);
            size_t payload_size = tuple_size - sizeof(TupleHeader);
            bool has_toast_payload =
                payload_size == sizeof(ToastPointer) &&
                (tuple_hdr->hasRecordFlag(TupleHeader::RHD_TOAST_PTR) ||
                 ToastManager::isToastPointer(payload, payload_size));

            if (!has_toast_payload)
            {
                return Status::OK;
            }

            if (toast_mgr == nullptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "TOAST manager not available for key extraction");
                return Status::INVALID_ARGUMENT;
            }

            std::vector<uint8_t> detoasted_payload;
            Status detoast_status =
                toast_mgr->detoastIfNeeded(payload, payload_size, &detoasted_payload, xid, ctx);
            if (detoast_status != Status::OK)
            {
                return detoast_status;
            }

            try
            {
                owned_tuple.resize(sizeof(TupleHeader) + detoasted_payload.size());
            }
            catch (const std::bad_alloc &)
            {
                SET_ERROR_CONTEXT(ctx, Status::OOM,
                                  "Failed to allocate detoasted key-extraction buffer");
                return Status::OOM;
            }

            std::memcpy(owned_tuple.data(), tuple_data, sizeof(TupleHeader));
            std::memcpy(owned_tuple.data() + sizeof(TupleHeader),
                        detoasted_payload.data(),
                        detoasted_payload.size());

            auto *owned_hdr = reinterpret_cast<TupleHeader *>(owned_tuple.data());
            owned_hdr->setRecordFlag(TupleHeader::RHD_TOAST_PTR, false);
            owned_hdr->payload_len = static_cast<uint32_t>(detoasted_payload.size());

            *materialized_data = owned_tuple.data();
            *materialized_size = static_cast<uint32_t>(owned_tuple.size());
            return Status::OK;
        }

        bool isZeroId(const ID &id)
        {
            for (uint8_t byte : id.bytes)
            {
                if (byte != 0)
                {
                    return false;
                }
            }
            return true;
        }

        bool pageTableIdMatches(const PageHeader *header, const ID &table_id)
        {
            if (header == nullptr)
            {
                return false;
            }
            auto *page_bytes = reinterpret_cast<const uint8_t *>(header);
            const auto *special = reinterpret_cast<const HeapPageSpecial *>(
                page_bytes + header->page_size - sizeof(HeapPageSpecial));
            return std::memcmp(special->table_id.bytes.data(), table_id.bytes.data(),
                               table_id.bytes.size()) == 0;
        }

        void propagateErrorContext(ErrorContext *dst, const ErrorContext &src)
        {
            if (dst == nullptr)
            {
                return;
            }

            dst->code = src.code;
            dst->sqlstate_text = src.sqlstate_text;
            dst->sqlstate = dst->sqlstate_text.empty() ? src.sqlstate : dst->sqlstate_text.c_str();
            dst->message = src.message;
            dst->vnext_code = src.vnext_code;
            dst->file = src.file;
            dst->line = src.line;
            dst->function = src.function;
            dst->constraint_name = src.constraint_name;
            dst->table_name = src.table_name;
            dst->column_name = src.column_name;
            dst->violating_value = src.violating_value;
            dst->referenced_table = src.referenced_table;
            dst->referenced_column = src.referenced_column;
            dst->check_expression = src.check_expression;
            dst->hint = src.hint;
        }

        [[nodiscard]] bool supportsExactKeyLookup(CatalogManager::IndexType index_type)
        {
            switch (index_type)
            {
                case CatalogManager::IndexType::BTREE:
                case CatalogManager::IndexType::STL_SORT:
                case CatalogManager::IndexType::ART:
                case CatalogManager::IndexType::MONGODB_GEO_HAYSTACK:
                case CatalogManager::IndexType::NEO4J_RANGE:
                case CatalogManager::IndexType::NEO4J_POINT:
                case CatalogManager::IndexType::REDIS_LIST:
                case CatalogManager::IndexType::REDIS_ZSET:
                case CatalogManager::IndexType::REDIS_STREAM:
                case CatalogManager::IndexType::HASH:
                case CatalogManager::IndexType::REDIS_STRING:
                case CatalogManager::IndexType::REDIS_HASH:
                case CatalogManager::IndexType::REDIS_SET:
                case CatalogManager::IndexType::REDIS_HLL:
                    return true;
                default:
                    return false;
            }
        }

        Status searchExactIndexCandidates(CatalogManager::IndexType index_type,
                                          void *index_ptr,
                                          const std::vector<uint8_t> &key,
                                          uint64_t current_xid,
                                          std::vector<TID> *tids_out,
                                          ErrorContext *ctx)
        {
            if (index_ptr == nullptr || tids_out == nullptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid exact index lookup");
                return Status::INVALID_ARGUMENT;
            }

            switch (index_type)
            {
                case CatalogManager::IndexType::BTREE:
                case CatalogManager::IndexType::STL_SORT:
                case CatalogManager::IndexType::ART:
                case CatalogManager::IndexType::MONGODB_GEO_HAYSTACK:
                case CatalogManager::IndexType::NEO4J_RANGE:
                case CatalogManager::IndexType::NEO4J_POINT:
                case CatalogManager::IndexType::REDIS_LIST:
                case CatalogManager::IndexType::REDIS_ZSET:
                case CatalogManager::IndexType::REDIS_STREAM:
                    return static_cast<BTree *>(index_ptr)->search(key, current_xid, tids_out, ctx);

                case CatalogManager::IndexType::HASH:
                case CatalogManager::IndexType::REDIS_STRING:
                case CatalogManager::IndexType::REDIS_HASH:
                case CatalogManager::IndexType::REDIS_SET:
                case CatalogManager::IndexType::REDIS_HLL:
                    return static_cast<HashIndex *>(index_ptr)->find(
                        key.data(), key.size(), current_xid, tids_out, ctx);

                default:
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "Index type does not support exact key lookup");
                    return Status::INVALID_ARGUMENT;
            }
        }
    } // anonymous namespace

    // TASK-DML-2: Public wrapper for removeFromIndex (for executor)
    auto StorageEngine::removeFromIndexHelper(
        uint8_t index_type_value,
        void *index_ptr,
        const std::vector<uint8_t> &key,
        const TID &tid,
        uint64_t xid,
        ErrorContext *ctx) -> Status
    {
        // Cast uint8_t to IndexType enum to avoid circular include dependency
        auto index_type = static_cast<CatalogManager::IndexType>(index_type_value);
        return removeFromIndex(index_type, index_ptr, key, tid, xid, ctx);
    }

    auto StorageEngine::extractStoredIndexKey(const ID &table_id,
                                              const std::vector<ID> &indexed_column_ids,
                                              const Tuple &tuple,
                                              std::vector<uint8_t> *key_out,
                                              ErrorContext *ctx) -> Status
    {
        if (key_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid index key output buffer");
            return Status::INVALID_ARGUMENT;
        }

        std::vector<CatalogManager::ColumnInfo> columns;
        Status status = catalog_manager_->getColumns(table_id, columns, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<uint16_t> column_indices;
        column_indices.reserve(indexed_column_ids.size());
        for (const auto &column_id : indexed_column_ids)
        {
            bool found = false;
            for (size_t i = 0; i < columns.size(); ++i)
            {
                if (columns[i].column_id == column_id)
                {
                    column_indices.push_back(static_cast<uint16_t>(i));
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Indexed column not found in table metadata");
                return Status::INVALID_ARGUMENT;
            }
        }

        std::vector<size_t> column_offsets;
        std::vector<size_t> column_sizes;
        ToastManager *toast_mgr = getOrCreateToastManager(table_id, ctx);
        std::vector<uint8_t> materialized_tuple;
        const uint8_t *materialized_data = nullptr;
        uint32_t materialized_size = 0;
        Status materialize_status = materializeTupleForKeyExtraction(tuple.data,
                                                                     tuple.data_size,
                                                                     toast_mgr,
                                                                     getCurrentXid(),
                                                                     materialized_tuple,
                                                                     &materialized_data,
                                                                     &materialized_size,
                                                                     ctx);
        if (materialize_status != Status::OK)
        {
            return materialize_status;
        }

        Status layout_status = computeColumnLayout(materialized_data, materialized_size, columns,
                                                   db_->domain_manager(),
                                                   column_offsets, column_sizes, ctx);
        if (layout_status != Status::OK)
        {
            return layout_status;
        }

        IndexKeyExtractor extractor;
        return extractor.extractKey(materialized_data, materialized_size,
                                    column_offsets, column_sizes,
                                    column_indices,
                                    toast_mgr,
                                    getCurrentXid(),
                                    key_out, ctx);
    }

    auto StorageEngine::getVisibleTupleForStableTid(const ID &table_id,
                                                    const TID &stable_tid,
                                                    Tuple *tuple_out,
                                                    ErrorContext *ctx) -> Status
    {
        if (tuple_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid tuple output buffer");
            return Status::INVALID_ARGUMENT;
        }

        CatalogManager::TableInfo table_info;
        Status status = catalog_manager_->getTable(table_id, table_info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Table not found");
            return Status::NOT_FOUND;
        }

        uint16_t target_tablespace = getTablespaceID(stable_tid.gpid);
        if (table_info.migration_in_progress)
        {
            TIDResolver *tid_resolver = db_->tid_resolver();
            if (tid_resolver != nullptr)
            {
                target_tablespace = tid_resolver->resolveTablespace(stable_tid,
                                                                    table_info,
                                                                    nullptr,
                                                                    ctx);
            }
        }

        uint64_t page_number = getPageNumber(stable_tid.gpid);
        GPID resolved_gpid = makeGPID(target_tablespace, page_number);

        void *page_buffer = nullptr;
        status = buffer_pool_->pinPageGlobal(resolved_gpid, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page_data = static_cast<uint8_t *>(page_buffer);
        ToastManager *toast_mgr = getOrCreateToastManager(table_id, ctx);
        HeapPage heap_page(page_data, db_->page_size(), toast_mgr, db_, table_id);

        const uint8_t *visible_tuple_data = nullptr;
        uint32_t visible_tuple_size = 0;
        TID visible_tid = stable_tid;
        status = heap_page.findVisibleVersion(stable_tid.slot,
                                              getCurrentXid(),
                                              &visible_tuple_data,
                                              &visible_tuple_size,
                                              &visible_tid,
                                              ctx);

        buffer_pool_->unpinPageGlobal(resolved_gpid, false, ctx);

        if (status != Status::OK)
        {
            return status;
        }

        return getTuple(table_id, visible_tid, tuple_out, ctx);
    }

    auto StorageEngine::filterIndexCandidatesByVisibleHeap(
        const ID &table_id,
        const std::vector<ID> &indexed_column_ids,
        bool enforce_key_semantics,
        const std::vector<uint8_t> &search_key,
        const std::vector<TID> &candidate_tids,
        const TID *exclude_tid,
        std::vector<TID> *visible_tids,
        ErrorContext *ctx) -> Status
    {
        if (visible_tids == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid visible TID output buffer");
            return Status::INVALID_ARGUMENT;
        }

        visible_tids->clear();

        for (const auto &candidate_tid : candidate_tids)
        {
            if (exclude_tid != nullptr && candidate_tid == *exclude_tid)
            {
                continue;
            }

            Tuple tuple{};
            ErrorContext tuple_ctx;
            Status tuple_status = getVisibleTupleForStableTid(table_id, candidate_tid,
                                                              &tuple, &tuple_ctx);
            if (tuple_status == Status::NOT_FOUND)
            {
                continue;
            }
            if (tuple_status != Status::OK)
            {
                if (ctx != nullptr && ctx->message.empty())
                {
                    propagateErrorContext(ctx, tuple_ctx);
                }
                return tuple_status;
            }

            std::vector<uint8_t> tuple_copy(tuple.data, tuple.data + tuple.data_size);
            Tuple stable_tuple{tuple_copy.data(), static_cast<uint32_t>(tuple_copy.size()), candidate_tid};

            bool matches_key_semantics = true;
            if (enforce_key_semantics)
            {
                std::vector<uint8_t> visible_key;
                ErrorContext key_ctx;
                Status key_status = extractStoredIndexKey(table_id, indexed_column_ids, stable_tuple,
                                                          &visible_key, &key_ctx);
                if (key_status != Status::OK)
                {
                    if (ctx != nullptr && ctx->message.empty())
                    {
                        propagateErrorContext(ctx, key_ctx);
                    }
                    return key_status;
                }

                matches_key_semantics = (visible_key == search_key);
            }

            if (matches_key_semantics)
            {
                visible_tids->push_back(candidate_tid);
            }
        }

        return Status::OK;
    }

    auto StorageEngine::preflightUniqueInsert(const ID &table_id,
                                              const uint8_t *tuple_data,
                                              uint32_t tuple_size,
                                              uint64_t current_xid,
                                              ErrorContext *ctx) -> Status
    {
        std::vector<CatalogManager::IndexInfo> indexes;
        Status status = catalog_manager_->listIndexesForTable(table_id, indexes, ctx, false);
        if (status != Status::OK || indexes.empty())
        {
            return status == Status::OK ? Status::OK : status;
        }

        std::vector<CatalogManager::ColumnInfo> columns;
        status = catalog_manager_->getColumns(table_id, columns, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<size_t> column_offsets;
        std::vector<size_t> column_sizes;
        status = computeColumnLayout(tuple_data, tuple_size, columns,
                                     db_->domain_manager(),
                                     column_offsets, column_sizes, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        IndexKeyExtractor extractor;
        ToastManager *toast_mgr = getOrCreateToastManager(table_id, ctx);
        for (const auto &index_info : indexes)
        {
            if (!index_info.is_unique || index_info.is_expression_index || index_info.is_partial_index)
            {
                continue;
            }
            if (indexUsesArrayUniqueness(index_info, catalog_manager_))
            {
                continue;
            }

            CatalogManager::IndexType actual_index_type;
            void *index_ptr = catalog_manager_->getIndexPtr(index_info.index_id, &actual_index_type);
            if (!index_ptr || !supportsExactKeyLookup(actual_index_type))
            {
                continue;
            }

            std::vector<uint16_t> column_indices;
            column_indices.reserve(index_info.column_ids.size());
            for (const auto &column_id : index_info.column_ids)
            {
                for (size_t i = 0; i < columns.size(); ++i)
                {
                    if (columns[i].column_id == column_id)
                    {
                        column_indices.push_back(static_cast<uint16_t>(i));
                        break;
                    }
                }
            }

            std::vector<uint8_t> key;
            status = extractor.extractKey(tuple_data, tuple_size,
                                          column_offsets, column_sizes,
                                          column_indices,
                                          toast_mgr,
                                          current_xid,
                                          &key, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            std::vector<TID> candidate_tids;
            ErrorContext lookup_ctx;
            status = searchExactIndexCandidates(actual_index_type, index_ptr, key, current_xid,
                                                &candidate_tids, &lookup_ctx);
            if (status == Status::NOT_FOUND)
            {
                continue;
            }
            if (status != Status::OK)
            {
                if (ctx != nullptr)
                {
                    propagateErrorContext(ctx, lookup_ctx);
                }
                return status;
            }

            std::vector<TID> visible_tids;
            status = filterIndexCandidatesByVisibleHeap(table_id, index_info.column_ids,
                                                        true, key, candidate_tids, nullptr,
                                                        &visible_tids, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            if (!visible_tids.empty())
            {
                incrementCanonicalCounter(
                    "sb_lock_unique_conflicts_total",
                    {metricDbLabel(db_), metricRelationLabel(catalog_manager_, table_id)});
                std::string msg = "UNIQUE index violation on index '" + index_info.index_name + "'";
                SET_ERROR_CONTEXT(ctx, Status::UNIQUE_VIOLATION, msg.c_str());
                return Status::UNIQUE_VIOLATION;
            }
        }

        return Status::OK;
    }

    auto StorageEngine::preflightUniqueUpdate(const ID &table_id,
                                              const uint8_t *old_tuple_data,
                                              uint32_t old_tuple_size,
                                              const uint8_t *new_tuple_data,
                                              uint32_t new_tuple_size,
                                              const TID &stable_tid,
                                              uint64_t current_xid,
                                              ErrorContext *ctx) -> Status
    {
        std::vector<CatalogManager::IndexInfo> indexes;
        Status status = catalog_manager_->listIndexesForTable(table_id, indexes, ctx, false);
        if (status != Status::OK || indexes.empty())
        {
            return status == Status::OK ? Status::OK : status;
        }

        std::vector<CatalogManager::ColumnInfo> columns;
        status = catalog_manager_->getColumns(table_id, columns, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<size_t> old_offsets;
        std::vector<size_t> old_sizes;
        std::vector<size_t> new_offsets;
        std::vector<size_t> new_sizes;
        status = computeColumnLayout(old_tuple_data, old_tuple_size, columns,
                                     db_->domain_manager(), old_offsets, old_sizes, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        status = computeColumnLayout(new_tuple_data, new_tuple_size, columns,
                                     db_->domain_manager(), new_offsets, new_sizes, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        IndexKeyExtractor extractor;
        ToastManager *toast_mgr = getOrCreateToastManager(table_id, ctx);
        for (const auto &index_info : indexes)
        {
            if (!index_info.is_unique || index_info.is_expression_index || index_info.is_partial_index)
            {
                continue;
            }
            if (indexUsesArrayUniqueness(index_info, catalog_manager_))
            {
                continue;
            }

            CatalogManager::IndexType actual_index_type;
            void *index_ptr = catalog_manager_->getIndexPtr(index_info.index_id, &actual_index_type);
            if (!index_ptr || !supportsExactKeyLookup(actual_index_type))
            {
                continue;
            }

            std::vector<uint16_t> column_indices;
            column_indices.reserve(index_info.column_ids.size());
            for (const auto &column_id : index_info.column_ids)
            {
                for (size_t i = 0; i < columns.size(); ++i)
                {
                    if (columns[i].column_id == column_id)
                    {
                        column_indices.push_back(static_cast<uint16_t>(i));
                        break;
                    }
                }
            }

            std::vector<uint8_t> old_key;
            std::vector<uint8_t> new_key;
            status = extractor.extractKeyForUpdate(old_tuple_data, old_tuple_size, old_offsets, old_sizes,
                                                   new_tuple_data, new_tuple_size, new_offsets, new_sizes,
                                                   column_indices, toast_mgr, current_xid,
                                                   &old_key, &new_key, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            if (old_key == new_key)
            {
                continue;
            }

            std::vector<TID> candidate_tids;
            ErrorContext lookup_ctx;
            status = searchExactIndexCandidates(actual_index_type, index_ptr, new_key, current_xid,
                                                &candidate_tids, &lookup_ctx);
            if (status == Status::NOT_FOUND)
            {
                continue;
            }
            if (status != Status::OK)
            {
                if (ctx != nullptr)
                {
                    propagateErrorContext(ctx, lookup_ctx);
                }
                return status;
            }

            std::vector<TID> visible_tids;
            status = filterIndexCandidatesByVisibleHeap(table_id, index_info.column_ids,
                                                        true, new_key, candidate_tids, &stable_tid,
                                                        &visible_tids, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            if (!visible_tids.empty())
            {
                incrementCanonicalCounter(
                    "sb_lock_unique_conflicts_total",
                    {metricDbLabel(db_), metricRelationLabel(catalog_manager_, table_id)});
                std::string msg = "UNIQUE index violation on index '" + index_info.index_name + "'";
                SET_ERROR_CONTEXT(ctx, Status::UNIQUE_VIOLATION, msg.c_str());
                return Status::UNIQUE_VIOLATION;
            }
        }

        return Status::OK;
    }

    auto StorageEngine::insertTuple(const ID &table_id, const uint8_t *tuple_data,
                                    uint32_t tuple_size, uint32_t *page_id_out,
                                    uint16_t *item_id_out, ErrorContext *ctx) -> Status
    {
        // Sprint 4 Task 5.4.3: INSERT routing during ONLINE migration

        // Step 1: Check if table is being migrated
        CatalogManager::TableInfo table_info;
        Status status = catalog_manager_->getTable(table_id, table_info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Table not found");
            return Status::NOT_FOUND;
        }

        const uint8_t *tuple_data_ptr = tuple_data;
        std::vector<uint8_t> temp_tuple_buffer;
        if (table_info.temp_data_scope != CatalogManager::TempDataScope::NONE)
        {
            ConnectionContext *conn_ctx = ConnectionContext::getCurrent();
            ID session_id = conn_ctx ? conn_ctx->effectiveSessionId() : ID{};
            if (!isZeroId(session_id))
            {
                temp_tuple_buffer.assign(tuple_data, tuple_data + tuple_size);
                if (tuple_size >= sizeof(TupleHeader))
                {
                    auto *header = reinterpret_cast<TupleHeader *>(temp_tuple_buffer.data());
                    header->session_id = session_id;
                    tuple_data_ptr = temp_tuple_buffer.data();
                }
            }
        }

        // Step 2: Determine target tablespace for INSERT
        uint16_t target_tablespace = table_info.tablespace_id; // Default: source tablespace

        if (table_info.migration_in_progress)
        {
            // Get current XID to check if this INSERT should go to target tablespace
            uint64_t current_xid = ConnectionContext::getCurrentTransactionId();
            if (current_xid == 0)
            {
                current_xid = config::DEFAULT_INITIAL_XID;
            }

            // If INSERT is happening after migration started, route to target tablespace
            if (current_xid >= table_info.migration_xid)
            {
                target_tablespace = table_info.migration_target_ts;
            }
        }

        // Step 3: Find a page with free space in the target tablespace
        // NOTE: For Phase 1, findFreePage only supports tablespace 0
        // This will need to be updated when multi-tablespace support is added
        uint32_t page_id = 0;
        GPID gpid = INVALID_GPID;
        void *page_buffer = nullptr;
        uint16_t item_id = 0;

        // Get current XID from connection context
        uint64_t current_xid = ConnectionContext::getCurrentTransactionId();
        if (current_xid == 0)
        {
            // No active connection context - use fallback XID
            current_xid = config::DEFAULT_INITIAL_XID;
        }

        // Get or create ToastManager for this table
        ToastManager *toast_mgr = getOrCreateToastManager(table_id, ctx);
        // Note: toast_mgr can be nullptr if TOAST table doesn't exist
        // HeapPage will handle this gracefully by not TOASTing

        status = preflightUniqueInsert(table_id, tuple_data_ptr, tuple_size, current_xid, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // A stale free-space estimate can surface as PAGE_FULL after pinning.
        // Retry with a newly selected page a few times before surfacing failure.
        constexpr int kInsertRetryLimit = 4;
        for (int attempt = 0; attempt < kInsertRetryLimit; ++attempt)
        {
            status = findFreePage(table_id, tuple_size, &page_id, target_tablespace, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            gpid = makeGPID(target_tablespace, static_cast<uint64_t>(page_id));
            page_buffer = nullptr;
            status = buffer_pool_->pinPageGlobal(gpid, &page_buffer, ctx);
            auto *page_data = static_cast<uint8_t *>(page_buffer);
            if (status != Status::OK)
            {
                return status;
            }

            HeapPage heap_page(page_data, db_->page_size(), toast_mgr, db_, table_id);
            status = heap_page.insertTuple(tuple_data_ptr, tuple_size, current_xid, &item_id, ctx);
            if (status != Status::PAGE_FULL)
            {
                break;
            }

            buffer_pool_->unpinPageGlobal(gpid, false, ctx);
            gpid = INVALID_GPID;
        }

        if (status == Status::OK)
        {
            if (ConnectionContext* conn_ctx = ConnectionContext::getCurrent())
            {
                conn_ctx->trackTupleMutation(table_id, page_id, item_id, nullptr, 0);
            }

            // Sprint 4 Task 5.4.3: Dirty page tracking
            // If table is migrating and we wrote to SOURCE tablespace, mark page dirty
            if (table_info.migration_in_progress && target_tablespace == table_info.tablespace_id)
            {
                // Mark page as dirty in migration state (for catch-up phase)
                catalog_manager_->markPageDirty(table_info.migration_id, page_id, ctx);
            }

            // Phase 3 Task 3.2: Update indexes with detoasted values
            // Get all indexes for this table
            std::vector<CatalogManager::IndexInfo> indexes;
            Status index_status = catalog_manager_->listIndexesForTable(table_id, indexes, ctx, false);

            if (index_status == Status::OK && !indexes.empty())
            {
                // Get column information for extracting index keys
                std::vector<CatalogManager::ColumnInfo> columns;
                index_status = catalog_manager_->getColumns(table_id, columns, ctx);

                if (index_status == Status::OK)
                {
                    // Create TID for this tuple
                    TID tid = TID(makeGPID(target_tablespace, static_cast<uint64_t>(page_id)), item_id);

                    // Create IndexKeyExtractor for detoasting
                    IndexKeyExtractor extractor;

                    std::vector<size_t> column_offsets;
                    std::vector<size_t> column_sizes;
                    Status layout_status = computeColumnLayout(tuple_data_ptr, tuple_size, columns,
                                                               db_->domain_manager(),
                                                               column_offsets, column_sizes, ctx);
                    if (layout_status == Status::OK)
                    {
                        // Update each index
                        for (const auto &index_info : indexes)
                    {
                        // Get index type and pointer
                        CatalogManager::IndexType actual_index_type;
                        void *index_ptr = catalog_manager_->getIndexPtr(index_info.index_id, &actual_index_type);

                        if (!index_ptr)
                        {
                            LOG_WARNING(STORAGE, "Index %s not found in cache, skipping",
                                        index_info.index_name.c_str());
                            continue;
                        }

                        // TASK-DML-7: Special handling for columnstore (append-only columnar storage)
                        if (actual_index_type == CatalogManager::IndexType::COLUMNSTORE)
                        {
                            auto *columnstore = static_cast<ColumnstoreIndex*>(index_ptr);

                            // Insert each indexed column into columnstore
                            for (const auto &col_id : index_info.column_ids)
                            {
                                // Find column index and info
                                size_t col_idx = 0;
                                bool found = false;
                                for (size_t i = 0; i < columns.size(); i++)
                                {
                                    if (columns[i].column_id == col_id)
                                    {
                                        col_idx = i;
                                        found = true;
                                        break;
                                    }
                                }

                                if (!found)
                                {
                                    LOG_WARNING(STORAGE, "Column not found for columnstore index %s",
                                                index_info.index_name.c_str());
                                    continue;
                                }

                                // Check if column is NULL
                                bool is_null = (column_offsets[col_idx] == 0 && column_sizes[col_idx] == 0);

                                // Get column value pointer and size
                                const void *col_value = is_null ? nullptr : (tuple_data_ptr + column_offsets[col_idx]);
                                size_t col_value_len = column_sizes[col_idx];

                                // STOR-M1: Row-level OLTP insert into columnstore
                                // Buffers individual rows and auto-flushes when threshold reached
                                // Use full TID (GPID + slot) for columnstore tracking
                                Status insert_status = columnstore->insert(
                                    col_id,
                                    tid,
                                    col_value,
                                    col_value_len,
                                    is_null,
                                    ctx);

                                if (insert_status != Status::OK)
                                {
                                    LOG_WARNING(STORAGE, "Failed to insert into columnstore index %s: %s",
                                                index_info.index_name.c_str(),
                                                ctx ? ctx->message.c_str() : "unknown error");
                                }
                            }

                            continue; // Columnstore handled via row-level buffering
                        }

                        // Regular index handling (key-based indexes)
                        // Convert column IDs to column indices
                        std::vector<uint16_t> column_indices;
                        for (const auto &col_id : index_info.column_ids)
                        {
                            // Find column index by ID
                            for (size_t i = 0; i < columns.size(); i++)
                            {
                                if (columns[i].column_id == col_id)
                                {
                                    column_indices.push_back(static_cast<uint16_t>(i));
                                    break;
                                }
                            }
                        }

                        // Extract index key with automatic detoasting
                        std::vector<uint8_t> key;
                        Status extract_status = extractor.extractKey(
                            tuple_data_ptr, tuple_size,
                            column_offsets, column_sizes,
                            column_indices,
                            toast_mgr, current_xid,
                            &key, ctx);

                        if (extract_status != Status::OK)
                        {
                            LOG_WARNING(STORAGE, "Failed to extract index key for index %s: %s",
                                        index_info.index_name.c_str(),
                                        ctx ? ctx->message.c_str() : "unknown error");
                            continue; // Skip this index
                        }

                        // Insert into key-based index
                        Status insert_status = insertIntoIndex(
                            actual_index_type, index_ptr, key, tid, current_xid, ctx);

                        if (insert_status != Status::OK)
                        {
                            LOG_ERROR(STORAGE, "Failed to insert into %s index %s: %s",
                                      indexTypeToString(actual_index_type).c_str(),
                                      index_info.index_name.c_str(),
                                      ctx ? ctx->message.c_str() : "unknown error");
                        }
                    }

                        // Clear detoasting cache
                        extractor.clearCache();
                    }
                    else
                    {
                        LOG_WARNING(STORAGE, "Skipping index updates due to column layout failure");
                    }
                }
            }

            if (ConnectionContext* conn_ctx = ConnectionContext::getCurrent())
            {
                conn_ctx->recordTableDmlDelta(table_id, 1, 0, 0);
            }

            if (page_id_out != nullptr)
            {
                *page_id_out = page_id;
            }
            if (item_id_out != nullptr)
            {
                *item_id_out = item_id;
            }
        }

        if (gpid != INVALID_GPID)
        {
            buffer_pool_->unpinPageGlobal(gpid, status == Status::OK, ctx);
        }

        return status;
    }

    auto StorageEngine::deleteTuplesForSession(const ID &table_id, const ID &session_id,
                                               ErrorContext *ctx) -> Status
    {
        if (isZeroId(session_id))
        {
            return Status::OK;
        }

        CatalogManager::TableInfo table_info;
        Status info_status = catalog_manager_->getTable(table_id, table_info, ctx);
        if (info_status != Status::OK)
        {
            return info_status;
        }
        if (table_info.temp_data_scope == CatalogManager::TempDataScope::NONE)
        {
            return Status::OK;
        }

        uint32_t heap_start = 0;
        if (isZeroId(table_id))
        {
            heap_start = Config::getInstance().getUInt("storage", "heap_scan_start_page", 7);
        }
        HeapScanIterator scan(db_, this, table_id, heap_start, false);
        Tuple tuple;

        Status scan_status = Status::OK;
        while ((scan_status = scan.next(&tuple, ctx)) == Status::OK)
        {
            const auto *hdr = reinterpret_cast<const TupleHeader *>(tuple.data);
            if (hdr->session_id != session_id)
            {
                continue;
            }

            uint32_t page_id = static_cast<uint32_t>(getPageNumber(tuple.tid.gpid));
            uint16_t item_id = tuple.tid.slot;
            Status status = deleteTuple(table_id, page_id, item_id, UINT16_MAX, ctx);
            if (status != Status::OK && status != Status::NOT_FOUND)
            {
                return status;
            }
        }

        if (scan_status != Status::OK && scan_status != Status::NOT_FOUND)
        {
            return scan_status;
        }

        return Status::OK;
    }

    auto StorageEngine::getTuple(uint32_t page_id, uint16_t item_id, Tuple *tuple_out,
                                 ErrorContext *ctx) -> Status
    {
        // Pin the page
        void *page_buffer;
        Status status = buffer_pool_->pinPage(page_id, &page_buffer, ctx);
        auto *page_data = static_cast<uint8_t *>(page_buffer);
        if (status != Status::OK)
        {
            return status;
        }

        // Get tuple
        HeapPage heap_page(page_data, db_->page_size());
        const uint8_t *tuple_data;
        uint32_t tuple_size;

        status = heap_page.getTuple(item_id, &tuple_data, &tuple_size, ctx);

        if (status == Status::OK && (tuple_out != nullptr))
        {
            // Check visibility
            const auto *hdr = reinterpret_cast<const TupleHeader *>(tuple_data);
            uint64_t cur_xid = getCurrentXid();
            if (!isVisible(hdr->xmin, hdr->xmax, cur_xid))
            {
                status = Status::NOT_FOUND;
                SET_ERROR_CONTEXT(ctx, status, "Tuple not visible");
                if (ctx)
                {
                    ctx->message = "Tuple not visible (xmin=" + std::to_string(hdr->xmin) +
                                   " xmax=" + std::to_string(hdr->xmax) +
                                   " cur_xid=" + std::to_string(cur_xid) + ")";
                }
            }
            else
            {
                ConnectionContext *conn_ctx = ConnectionContext::getCurrent();
                ID session_id = conn_ctx ? conn_ctx->effectiveSessionId() : ID{};
                if (!isZeroId(hdr->session_id) &&
                    (isZeroId(session_id) || hdr->session_id != session_id))
                {
                    status = Status::NOT_FOUND;
                    SET_ERROR_CONTEXT(ctx, status, "Tuple not visible (session mismatch)");
                }

                // Set tuple data pointer (includes header for now)
                tuple_out->data = tuple_data;
                tuple_out->data_size = tuple_size;
                // PHASE 1.5: Set TID struct
                tuple_out->tid = TID(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id)), item_id);
            }
        }

        // Unpin the page
        buffer_pool_->unpinPage(page_id, status == Status::OK, ctx);

        // Cooperative GC hook - opportunistic cleanup
        if (db_->garbage_collector() != nullptr)
        {
            db_->garbage_collector()->processPageCooperative(page_id, ctx);
        }

        return status;
    }

    auto StorageEngine::getTuple(const ID &table_id, const TID &tid, Tuple *tuple_out,
                                 ErrorContext *ctx) -> Status
    {
        // Sprint 4 Task 5.4.2: Dual-Source Visibility during ONLINE migration

        // Step 1: Get table info to check if migration is in progress
        CatalogManager::TableInfo table_info;
        Status status = catalog_manager_->getTable(table_id, table_info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Table not found");
            return Status::NOT_FOUND;
        }

        // Step 2: Resolve which tablespace to read from
        // GPID is uint64_t, use helper function to extract tablespace_id
        uint16_t target_tablespace = getTablespaceID(tid.gpid); // Default: use TID's tablespace

        if (table_info.migration_in_progress)
        {
            // Migration in progress - use TIDResolver to determine correct tablespace
            TIDResolver *tid_resolver = db_->tid_resolver();
            if (tid_resolver != nullptr)
            {
                target_tablespace = tid_resolver->resolveTablespace(tid, table_info, nullptr, ctx);
            }
        }

        // Step 3: Construct GPID with resolved tablespace
        uint64_t page_number = getPageNumber(tid.gpid);
        GPID resolved_gpid = makeGPID(target_tablespace, page_number);

        // Step 4: Get file descriptor for target tablespace (validation only)
        int target_fd = db_->getTablespaceFd(target_tablespace);
        if (target_fd < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Tablespace not found");
            return Status::NOT_FOUND;
        }

        // Step 5: Pin the page from the correct tablespace
        void *page_buffer;
        status = buffer_pool_->pinPageGlobal(resolved_gpid, &page_buffer, ctx);
        auto *page_data = static_cast<uint8_t *>(page_buffer);
        if (status != Status::OK)
        {
            return status;
        }

        // Step 6: Get tuple from page
        HeapPage heap_page(page_data, db_->page_size());
        const uint8_t *tuple_data;
        uint32_t tuple_size;

        status = heap_page.getTuple(tid.slot, &tuple_data, &tuple_size, ctx);

        if (status == Status::OK && (tuple_out != nullptr))
        {
            // Check visibility
            const auto *hdr = reinterpret_cast<const TupleHeader *>(tuple_data);
            if (!isVisible(hdr->xmin, hdr->xmax, getCurrentXid()))
            {
                status = Status::NOT_FOUND;
                SET_ERROR_CONTEXT(ctx, status, "Tuple not visible");
            }
            else if (table_info.temp_data_scope != CatalogManager::TempDataScope::NONE)
            {
                ConnectionContext *conn_ctx = ConnectionContext::getCurrent();
                ID session_id = conn_ctx ? conn_ctx->effectiveSessionId() : ID{};
                if (isZeroId(session_id) || hdr->session_id != session_id)
                {
                    status = Status::NOT_FOUND;
                    SET_ERROR_CONTEXT(ctx, status, "Tuple not visible");
                }
                else
                {
                    // Set tuple data pointer (includes header for now)
                    tuple_out->data = tuple_data;
                    tuple_out->data_size = tuple_size;
                    // Set TID with resolved tablespace
                    tuple_out->tid = TID(resolved_gpid, tid.slot);
                }
            }
            else
            {
                // Set tuple data pointer (includes header for now)
                tuple_out->data = tuple_data;
                tuple_out->data_size = tuple_size;
                // Set TID with resolved tablespace
                tuple_out->tid = TID(resolved_gpid, tid.slot);
            }
        }

        // Unpin the page
        buffer_pool_->unpinPageGlobal(resolved_gpid, status == Status::OK, ctx);

        // Cooperative GC hook - opportunistic cleanup
        if (db_->garbage_collector() != nullptr && target_tablespace == PRIMARY_TABLESPACE_ID)
        {
            db_->garbage_collector()->processPageCooperative(static_cast<uint32_t>(page_number), ctx);
        }

        return status;
    }

    auto StorageEngine::deleteTuple(const ID &table_id, uint32_t page_id, uint16_t item_id,
                                    uint16_t tablespace_id_override, ErrorContext *ctx) -> Status
    {
        // Sprint 4 Task 5.4.3: Check if table is being migrated
        CatalogManager::TableInfo table_info;
        Status migration_check_status = catalog_manager_->getTable(table_id, table_info, ctx);
        bool is_migrating = (migration_check_status == Status::OK && table_info.migration_in_progress);
        uint16_t tablespace_id = (tablespace_id_override == UINT16_MAX)
            ? table_info.tablespace_id
            : tablespace_id_override;

        // Get proc_id from ConnectionContext (Phase 2 complete)
        int32_t proc_id_signed = ConnectionContext::getCurrentProcId();
        uint32_t proc_id = (proc_id_signed >= 0) ? static_cast<uint32_t>(proc_id_signed) : 0;

        // Acquire tuple lock (Phase 2.5 complete)
        // Get wait setting from ConnectionContext (default: wait for locks)
        ConnectionContext *conn_ctx = ConnectionContext::getCurrent();
        bool wait = conn_ctx ? conn_ctx->getWaitForLocks() : true;
        Status lock_status = acquireTupleLock(table_id, page_id, item_id, proc_id, wait, ctx);
        if (lock_status != Status::OK)
        {
            return lock_status;
        }
        struct TupleLockGuard
        {
            StorageEngine *engine;
            ID table_id;
            uint32_t page_id;
            uint16_t item_id;
            uint32_t proc_id;
            ErrorContext *ctx;
            bool armed;
            ~TupleLockGuard()
            {
                if (!armed || engine == nullptr)
                {
                    return;
                }
                Status release_status =
                    engine->releaseTupleLock(table_id, page_id, item_id, proc_id, ctx);
                if (release_status != Status::OK && release_status != Status::NOT_FOUND)
                {
                    LOG_WARNING(STORAGE, "Failed to release tuple lock: status=%d",
                                static_cast<int>(release_status));
                }
            }
        } tuple_lock_guard{this, table_id, page_id, item_id, proc_id, ctx, true};

        ToastManager *toast_mgr = getOrCreateToastManager(table_id, ctx);
        const bool defer_toast_cleanup =
            conn_ctx != nullptr && conn_ctx->hasActiveSavepoints() &&
            !conn_ctx->isSavepointRollbackInProgress();

        // Pin the page
        GPID gpid = makeGPID(tablespace_id, static_cast<uint64_t>(page_id));
        void *page_buffer;
        Status status = buffer_pool_->pinPageGlobal(gpid, &page_buffer, ctx);
        auto *page_data = static_cast<uint8_t *>(page_buffer);
        if (status != Status::OK)
        {
            return status;
        }

        // Resolve the currently visible physical version for the stable logical TID before
        // applying a delete. Rolled-back updates can leave an aborted head version in the root
        // slot while the committed row lives in the back-version chain.
        HeapPage heap_page(page_data, db_->page_size(), toast_mgr, db_, table_id);

        // Get current XID from connection context
        uint64_t current_xid = ConnectionContext::getCurrentTransactionId();
        if (current_xid == 0)
        {
            // No active connection context - use fallback XID
            current_xid = config::DEFAULT_INITIAL_XID;
        }

        const uint8_t *visible_tuple_data = nullptr;
        uint32_t visible_tuple_size = 0;
        TID visible_tuple_tid(gpid, item_id);
        status = heap_page.findVisibleVersion(item_id,
                                              current_xid,
                                              &visible_tuple_data,
                                              &visible_tuple_size,
                                              &visible_tuple_tid,
                                              ctx);
        if (status != Status::OK)
        {
            buffer_pool_->unpinPageGlobal(gpid, false, ctx);
            return status;
        }

        std::vector<uint8_t> deleted_tuple_image(
            visible_tuple_data, visible_tuple_data + visible_tuple_size);
        const auto *visible_tuple_hdr =
            reinterpret_cast<const TupleHeader *>(deleted_tuple_image.data());

        if (table_info.temp_data_scope != CatalogManager::TempDataScope::NONE)
        {
            ConnectionContext *conn_ctx = ConnectionContext::getCurrent();
            ID session_id = conn_ctx ? conn_ctx->effectiveSessionId() : ID{};
            if (isZeroId(session_id) || visible_tuple_hdr->session_id != session_id)
            {
                buffer_pool_->unpinPageGlobal(gpid, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Tuple not visible");
                return Status::NOT_FOUND;
            }
        }

        const bool delete_on_root_page = visible_tuple_tid.gpid == gpid;
        GPID delete_target_gpid = visible_tuple_tid.gpid;
        uint32_t delete_target_page_id =
            static_cast<uint32_t>(getPageNumber(delete_target_gpid));
        uint16_t delete_target_item_id = visible_tuple_tid.slot;

        void *delete_page_buffer = page_buffer;
        uint8_t *delete_page_data = page_data;
        if (!delete_on_root_page)
        {
            status = buffer_pool_->pinPageGlobal(delete_target_gpid, &delete_page_buffer, ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPageGlobal(gpid, false, ctx);
                return status;
            }
            delete_page_data = static_cast<uint8_t *>(delete_page_buffer);
        }

        HeapPage delete_heap_page(delete_page_data, db_->page_size(), toast_mgr, db_, table_id);
        status = delete_heap_page.deleteTuple(delete_target_item_id,
                                              current_xid,
                                              ctx,
                                              defer_toast_cleanup);

        if (status == Status::OK)
        {
            if (ConnectionContext* active_conn_ctx = ConnectionContext::getCurrent())
            {
                active_conn_ctx->trackTupleMutation(
                    table_id,
                    page_id,
                    item_id,
                    deleted_tuple_image.data(),
                    static_cast<uint32_t>(deleted_tuple_image.size()));
            }

            // Sprint 4 Task 5.4.3: Mark page dirty if migrating
            if (is_migrating)
            {
                catalog_manager_->markPageDirty(table_info.migration_id,
                                                delete_target_page_id,
                                                ctx);
            }

            // Mark page as dirty for GC
            if (db_->garbage_collector() != nullptr)
            {
                if (getTablespaceID(delete_target_gpid) == PRIMARY_TABLESPACE_ID)
                {
                    db_->garbage_collector()->markPageDirty(delete_target_page_id);
                }
            }

            // LSM Integration Phase 4: Remove from all indexes
            std::vector<CatalogManager::IndexInfo> indexes;
            Status index_status = catalog_manager_->listIndexesForTable(table_id, indexes, ctx, false);

            if (index_status == Status::OK && !indexes.empty())
            {
                // Get column information for extracting index keys
                std::vector<CatalogManager::ColumnInfo> columns;
                index_status = catalog_manager_->getColumns(table_id, columns, ctx);

                if (index_status == Status::OK)
                {
                    // Index identity is still the stable logical root TID even when the visible
                    // version being deleted currently lives in a back-version slot.
                    TID tid = TID(makeGPID(tablespace_id, static_cast<uint64_t>(page_id)), item_id);

                    const uint8_t *tuple_data = deleted_tuple_image.data();
                    uint32_t tuple_length = static_cast<uint32_t>(deleted_tuple_image.size());

                    // Create IndexKeyExtractor
                    IndexKeyExtractor extractor;

                    std::vector<size_t> column_offsets;
                    std::vector<size_t> column_sizes;
                    Status layout_status = computeColumnLayout(tuple_data, tuple_length, columns,
                                                               db_->domain_manager(),
                                                               column_offsets, column_sizes, ctx);
                    if (layout_status == Status::OK)
                    {
                        // Remove from each index
                        for (const auto &index_info : indexes)
                        {
                            // Convert column IDs to column indices
                            std::vector<uint16_t> column_indices;
                            for (const auto &col_id : index_info.column_ids)
                            {
                                for (size_t i = 0; i < columns.size(); i++)
                                {
                                    if (columns[i].column_id == col_id)
                                    {
                                        column_indices.push_back(static_cast<uint16_t>(i));
                                        break;
                                    }
                                }
                            }

                            // Extract index key
                            std::vector<uint8_t> key;
                            Status extract_status = extractor.extractKey(
                                tuple_data, tuple_length,
                                column_offsets, column_sizes,
                                column_indices,
                                getOrCreateToastManager(table_id, ctx),
                                current_xid,
                                &key, ctx);

                            if (extract_status != Status::OK)
                            {
                                LOG_WARNING(STORAGE, "Failed to extract index key for deletion from index %s: %s",
                                            index_info.index_name.c_str(),
                                            ctx ? ctx->message.c_str() : "unknown error");
                                continue;
                            }

                            CatalogManager::IndexType actual_index_type;
                            void *index_ptr = catalog_manager_->getIndexPtr(index_info.index_id, &actual_index_type);

                            if (index_ptr)
                            {
                                const bool exact_lookup_family = supportsExactKeyLookup(actual_index_type);
                                Status retire_status = exact_lookup_family
                                    ? retireExactIndexEntry(actual_index_type,
                                                            index_ptr,
                                                            key,
                                                            tid,
                                                            current_xid,
                                                            ExactIndexRetirementMode::SOFT_DELETE,
                                                            ctx)
                                    : removeFromIndex(actual_index_type, index_ptr, key, tid,
                                                      current_xid, ctx);

                                if (retire_status != Status::OK)
                                {
                                    LOG_ERROR(STORAGE, "Failed to retire from %s index %s: %s",
                                              indexTypeToString(actual_index_type).c_str(),
                                              index_info.index_name.c_str(),
                                              ctx ? ctx->message.c_str() : "unknown error");
                                }
                            }
                            else
                            {
                                LOG_WARNING(STORAGE, "Index %s not found in cache, skipping deletion",
                                            index_info.index_name.c_str());
                            }
                        }

                        // Clear detoasting cache
                        extractor.clearCache();
                    }
                    else
                    {
                        LOG_WARNING(STORAGE, "Skipping index lifecycle retirement due to column layout failure");
                    }
                }
            }

            if (ConnectionContext* conn_ctx = ConnectionContext::getCurrent())
            {
                conn_ctx->recordTableDmlDelta(table_id, 0, 0, 1);
            }
        }

        if (!delete_on_root_page)
        {
            buffer_pool_->unpinPageGlobal(delete_target_gpid, status == Status::OK, ctx);
        }

        // Unpin the root page used for stable-TID resolution.
        buffer_pool_->unpinPageGlobal(gpid, delete_on_root_page && status == Status::OK, ctx);

        return status;
    }

    auto StorageEngine::applyStableHeadBackout(const SavepointBackoutAction &action,
                                               uint64_t rollback_xid,
                                               ErrorContext *ctx) -> Status
    {
        return action.restoresPriorState()
            ? backoutRestoreStableHeadRow(action, rollback_xid, ctx)
            : backoutRemoveStableHeadRow(action, rollback_xid, ctx);
    }

    auto StorageEngine::backoutRemoveStableHeadRow(const SavepointBackoutAction &action,
                                                   uint64_t rollback_xid,
                                                   ErrorContext *ctx) -> Status
    {
        (void)rollback_xid;

        uint16_t tablespace_id = PRIMARY_TABLESPACE_ID;
        CatalogManager::TableInfo table_info;
        bool table_info_valid = false;
        if (!isZeroId(action.table_id) && catalog_manager_ != nullptr)
        {
            table_info_valid = catalog_manager_->getTable(action.table_id, table_info, ctx) == Status::OK;
            if (table_info_valid)
            {
                tablespace_id = table_info.tablespace_id;
            }
        }

        auto markDirtyPage = [&](GPID dirty_gpid) {
            const uint16_t dirty_tablespace_id = getTablespaceID(dirty_gpid);
            const uint32_t dirty_page_id = static_cast<uint32_t>(getPageNumber(dirty_gpid));
            if (table_info_valid && table_info.migration_in_progress)
            {
                catalog_manager_->markPageDirty(table_info.migration_id, dirty_page_id, ctx);
            }
            if (db_->garbage_collector() != nullptr &&
                dirty_tablespace_id == PRIMARY_TABLESPACE_ID)
            {
                db_->garbage_collector()->markPageDirty(dirty_page_id);
            }
        };

        auto copyTupleImage = [](const uint8_t *tuple_data,
                                 uint32_t tuple_size,
                                 std::vector<uint8_t> *out,
                                 ErrorContext *ctx) -> Status
        {
            if (out == nullptr || tuple_data == nullptr || tuple_size < sizeof(TupleHeader))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Tuple image is invalid");
                return Status::INVALID_ARGUMENT;
            }

            try
            {
                out->assign(tuple_data, tuple_data + tuple_size);
            }
            catch (const std::bad_alloc &)
            {
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate tuple image copy");
                return Status::OOM;
            }
            return Status::OK;
        };

        const GPID primary_gpid =
            makeGPID(tablespace_id, static_cast<uint64_t>(action.stable_page_id));
        ToastManager *toast_mgr =
            isZeroId(action.table_id) ? nullptr : getOrCreateToastManager(action.table_id, ctx);

        void *primary_page_buffer = nullptr;
        Status status = buffer_pool_->pinPageGlobal(primary_gpid, &primary_page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *primary_page_data = static_cast<uint8_t *>(primary_page_buffer);
        HeapPage primary_heap_page(primary_page_data, db_->page_size(), toast_mgr, db_,
                                   action.table_id);
        const uint8_t *current_tuple_data = nullptr;
        uint32_t current_tuple_size = 0;
        status = primary_heap_page.getTuple(action.stable_item_id,
                                            &current_tuple_data,
                                            &current_tuple_size,
                                            ctx);
        if (status != Status::OK)
        {
            buffer_pool_->unpinPageGlobal(primary_gpid, false, ctx);
            return status;
        }

        std::vector<uint8_t> current_tuple_image;
        status = copyTupleImage(current_tuple_data, current_tuple_size, &current_tuple_image, ctx);
        if (status != Status::OK)
        {
            buffer_pool_->unpinPageGlobal(primary_gpid, false, ctx);
            return status;
        }

        const auto *current_hdr =
            reinterpret_cast<const TupleHeader *>(current_tuple_image.data());
        std::vector<TID> version_chain_tids;
        std::vector<std::vector<uint8_t>> version_chain_images;
        std::vector<TID> visited_tids;
        for (TID next_tid = current_hdr->getBackVersionTID();
             next_tid.isValid();
             )
        {
            if (std::find(visited_tids.begin(), visited_tids.end(), next_tid) !=
                visited_tids.end())
            {
                buffer_pool_->unpinPageGlobal(primary_gpid, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Detected cycle while collecting savepoint backout chain");
                return Status::PAGE_CORRUPT;
            }
            visited_tids.push_back(next_tid);

            void *chain_page_buffer = nullptr;
            Status pin_status = buffer_pool_->pinPageGlobal(next_tid.gpid, &chain_page_buffer, ctx);
            if (pin_status != Status::OK)
            {
                buffer_pool_->unpinPageGlobal(primary_gpid, false, ctx);
                return pin_status;
            }

            auto *chain_page_data = static_cast<uint8_t *>(chain_page_buffer);
            HeapPage chain_heap_page(chain_page_data, db_->page_size(), toast_mgr, db_,
                                     action.table_id);
            const uint8_t *chain_tuple_data = nullptr;
            uint32_t chain_tuple_size = 0;
            Status chain_status = chain_heap_page.getTuple(next_tid.slot,
                                                           &chain_tuple_data,
                                                           &chain_tuple_size,
                                                           ctx);
            std::vector<uint8_t> chain_tuple_image;
            if (chain_status == Status::OK)
            {
                chain_status = copyTupleImage(chain_tuple_data,
                                              chain_tuple_size,
                                              &chain_tuple_image,
                                              ctx);
            }
            buffer_pool_->unpinPageGlobal(next_tid.gpid, false, ctx);
            if (chain_status != Status::OK)
            {
                buffer_pool_->unpinPageGlobal(primary_gpid, false, ctx);
                return chain_status;
            }

            version_chain_tids.push_back(next_tid);
            version_chain_images.emplace_back(std::move(chain_tuple_image));
            next_tid =
                reinterpret_cast<const TupleHeader *>(version_chain_images.back().data())
                    ->getBackVersionTID();
        }

        MgaBackoutEngine *backout_engine = db_->mga_backout_engine();
        if (backout_engine == nullptr)
        {
            buffer_pool_->unpinPageGlobal(primary_gpid, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "MGA backout engine not available");
            return Status::IO_ERROR;
        }

        status = backout_engine->applyStableHeadAncillaryBackout(
            action,
            tablespace_id,
            current_tuple_image.data(),
            static_cast<uint32_t>(current_tuple_image.size()),
            nullptr,
            0,
            version_chain_images,
            rollback_xid,
            ctx);
        if (status != Status::OK)
        {
            buffer_pool_->unpinPageGlobal(primary_gpid, false, ctx);
            return status;
        }

        bool primary_dirty = false;
        std::vector<uint16_t> primary_reclaim_slots;
        primary_reclaim_slots.push_back(action.stable_item_id);
        for (const TID &chain_tid : version_chain_tids)
        {
            if (chain_tid.gpid == primary_gpid)
            {
                primary_reclaim_slots.push_back(chain_tid.slot);
            }
        }
        uint32_t reclaimed_primary_tuples = 0;
        uint32_t reclaimed_primary_bytes = 0;
        status = primary_heap_page.reclaimVersionSlots(primary_reclaim_slots,
                                                       &reclaimed_primary_tuples,
                                                       &reclaimed_primary_bytes,
                                                       ctx);
        if (status != Status::OK)
        {
            buffer_pool_->unpinPageGlobal(primary_gpid, false, ctx);
            return status;
        }
        primary_dirty = reclaimed_primary_tuples > 0;
        buffer_pool_->unpinPageGlobal(primary_gpid, primary_dirty, ctx);
        if (primary_dirty)
        {
            markDirtyPage(primary_gpid);
        }

        for (const TID &chain_tid : version_chain_tids)
        {
            if (chain_tid.gpid == primary_gpid)
            {
                continue;
            }

            void *chain_page_buffer = nullptr;
            Status pin_status = buffer_pool_->pinPageGlobal(chain_tid.gpid, &chain_page_buffer, ctx);
            if (pin_status != Status::OK)
            {
                return pin_status;
            }

            auto *chain_page_data = static_cast<uint8_t *>(chain_page_buffer);
            HeapPage chain_heap_page(chain_page_data, db_->page_size(), toast_mgr, db_,
                                     action.table_id);
            uint32_t reclaimed_chain_tuples = 0;
            uint32_t reclaimed_chain_bytes = 0;
            Status chain_status =
                chain_heap_page.reclaimVersionSlots({chain_tid.slot},
                                                    &reclaimed_chain_tuples,
                                                    &reclaimed_chain_bytes,
                                                    ctx);
            buffer_pool_->unpinPageGlobal(chain_tid.gpid,
                                          chain_status == Status::OK &&
                                              reclaimed_chain_tuples > 0,
                                          ctx);
            if (chain_status != Status::OK)
            {
                return chain_status;
            }
            if (reclaimed_chain_tuples > 0)
            {
                markDirtyPage(chain_tid.gpid);
            }
        }

        return Status::OK;
    }

    auto StorageEngine::backoutRestoreStableHeadRow(const SavepointBackoutAction &action,
                                                    uint64_t rollback_xid,
                                                    ErrorContext *ctx) -> Status
    {
        if (action.prior_tuple_image.size() < sizeof(TupleHeader))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Savepoint prior tuple image is missing or invalid");
            return Status::INVALID_ARGUMENT;
        }

        uint16_t tablespace_id = PRIMARY_TABLESPACE_ID;
        CatalogManager::TableInfo table_info;
        bool table_info_valid = false;
        if (!isZeroId(action.table_id) && catalog_manager_ != nullptr)
        {
            table_info_valid = catalog_manager_->getTable(action.table_id, table_info, ctx) == Status::OK;
            if (table_info_valid)
            {
                tablespace_id = table_info.tablespace_id;
            }
        }

        auto markDirtyPage = [&](GPID dirty_gpid) {
            const uint16_t dirty_tablespace_id = getTablespaceID(dirty_gpid);
            const uint32_t dirty_page_id = static_cast<uint32_t>(getPageNumber(dirty_gpid));
            if (table_info_valid && table_info.migration_in_progress)
            {
                catalog_manager_->markPageDirty(table_info.migration_id, dirty_page_id, ctx);
            }
            if (db_->garbage_collector() != nullptr &&
                dirty_tablespace_id == PRIMARY_TABLESPACE_ID)
            {
                db_->garbage_collector()->markPageDirty(dirty_page_id);
            }
        };

        auto copyTupleImage = [](const uint8_t *tuple_data,
                                 uint32_t tuple_size,
                                 std::vector<uint8_t> *out,
                                 ErrorContext *ctx) -> Status
        {
            if (out == nullptr || tuple_data == nullptr || tuple_size < sizeof(TupleHeader))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Tuple image is invalid");
                return Status::INVALID_ARGUMENT;
            }

            try
            {
                out->assign(tuple_data, tuple_data + tuple_size);
            }
            catch (const std::bad_alloc &)
            {
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate tuple image copy");
                return Status::OOM;
            }
            return Status::OK;
        };

        const GPID primary_gpid =
            makeGPID(tablespace_id, static_cast<uint64_t>(action.stable_page_id));
        ToastManager *toast_mgr =
            isZeroId(action.table_id) ? nullptr : getOrCreateToastManager(action.table_id, ctx);

        void *primary_page_buffer = nullptr;
        Status status = buffer_pool_->pinPageGlobal(primary_gpid, &primary_page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *primary_page_data = static_cast<uint8_t *>(primary_page_buffer);
        std::vector<uint8_t> primary_page_snapshot(primary_page_data,
                                                   primary_page_data + db_->page_size());
        HeapPage primary_heap_page(primary_page_data, db_->page_size(), toast_mgr, db_,
                                   action.table_id);
        const uint8_t *current_tuple_data = nullptr;
        uint32_t current_tuple_size = 0;
        status = primary_heap_page.getTuple(action.stable_item_id,
                                            &current_tuple_data,
                                            &current_tuple_size,
                                            ctx);
        if (status != Status::OK)
        {
            buffer_pool_->unpinPageGlobal(primary_gpid, false, ctx);
            return status;
        }

        std::vector<uint8_t> current_tuple_image;
        status = copyTupleImage(current_tuple_data, current_tuple_size, &current_tuple_image, ctx);
        if (status != Status::OK)
        {
            buffer_pool_->unpinPageGlobal(primary_gpid, false, ctx);
            return status;
        }

        const auto *current_hdr =
            reinterpret_cast<const TupleHeader *>(current_tuple_image.data());
        const auto *restore_hdr =
            reinterpret_cast<const TupleHeader *>(action.prior_tuple_image.data());
        const TID preserved_back_tid = restore_hdr->getBackVersionTID();

        std::vector<TID> transient_chain_tids;
        std::vector<std::vector<uint8_t>> transient_chain_images;
        std::vector<TID> visited_tids;
        for (TID next_tid = current_hdr->getBackVersionTID();
             next_tid.isValid() && next_tid != preserved_back_tid;
             )
        {
            if (std::find(visited_tids.begin(), visited_tids.end(), next_tid) !=
                visited_tids.end())
            {
                buffer_pool_->unpinPageGlobal(primary_gpid, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Detected cycle while collecting transient savepoint versions");
                return Status::PAGE_CORRUPT;
            }
            visited_tids.push_back(next_tid);

            void *chain_page_buffer = nullptr;
            Status pin_status = buffer_pool_->pinPageGlobal(next_tid.gpid, &chain_page_buffer, ctx);
            if (pin_status != Status::OK)
            {
                buffer_pool_->unpinPageGlobal(primary_gpid, false, ctx);
                return pin_status;
            }

            auto *chain_page_data = static_cast<uint8_t *>(chain_page_buffer);
            HeapPage chain_heap_page(chain_page_data, db_->page_size(), toast_mgr, db_,
                                     action.table_id);
            const uint8_t *chain_tuple_data = nullptr;
            uint32_t chain_tuple_size = 0;
            Status chain_status = chain_heap_page.getTuple(next_tid.slot,
                                                           &chain_tuple_data,
                                                           &chain_tuple_size,
                                                           ctx);
            std::vector<uint8_t> chain_tuple_image;
            if (chain_status == Status::OK)
            {
                chain_status = copyTupleImage(chain_tuple_data,
                                              chain_tuple_size,
                                              &chain_tuple_image,
                                              ctx);
            }
            buffer_pool_->unpinPageGlobal(next_tid.gpid, false, ctx);
            if (chain_status != Status::OK)
            {
                buffer_pool_->unpinPageGlobal(primary_gpid, false, ctx);
                return chain_status;
            }

            transient_chain_tids.push_back(next_tid);
            transient_chain_images.emplace_back(std::move(chain_tuple_image));
            next_tid =
                reinterpret_cast<const TupleHeader *>(transient_chain_images.back().data())
                    ->getBackVersionTID();
        }

        bool primary_dirty = false;
        std::vector<uint16_t> primary_reclaim_slots;
        for (const TID &transient_tid : transient_chain_tids)
        {
            if (transient_tid.gpid == primary_gpid)
            {
                primary_reclaim_slots.push_back(transient_tid.slot);
            }
        }
        if (!primary_reclaim_slots.empty())
        {
            uint32_t reclaimed_bytes = 0;
            uint32_t reclaimed_tuples = 0;
            status = primary_heap_page.reclaimVersionSlots(primary_reclaim_slots,
                                                           &reclaimed_tuples,
                                                           &reclaimed_bytes,
                                                           ctx);
            if (status != Status::OK)
            {
                std::memcpy(primary_page_data,
                            primary_page_snapshot.data(),
                            primary_page_snapshot.size());
                buffer_pool_->unpinPageGlobal(primary_gpid, false, ctx);
                return status;
            }
            primary_dirty = reclaimed_tuples > 0;
        }

        status = primary_heap_page.restoreTupleImage(
            action.stable_item_id,
            action.prior_tuple_image.data(),
            static_cast<uint32_t>(action.prior_tuple_image.size()),
            ctx);
        if (status != Status::OK)
        {
            std::memcpy(primary_page_data,
                        primary_page_snapshot.data(),
                        primary_page_snapshot.size());
            buffer_pool_->unpinPageGlobal(primary_gpid, false, ctx);
            return status;
        }
        primary_dirty = true;

        MgaBackoutEngine *backout_engine = db_->mga_backout_engine();
        if (backout_engine == nullptr)
        {
            std::memcpy(primary_page_data,
                        primary_page_snapshot.data(),
                        primary_page_snapshot.size());
            buffer_pool_->unpinPageGlobal(primary_gpid, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "MGA backout engine not available");
            return Status::IO_ERROR;
        }

        status = backout_engine->applyStableHeadAncillaryBackout(
            action,
            tablespace_id,
            current_tuple_image.data(),
            static_cast<uint32_t>(current_tuple_image.size()),
            action.prior_tuple_image.data(),
            static_cast<uint32_t>(action.prior_tuple_image.size()),
            transient_chain_images,
            rollback_xid,
            ctx);
        if (status != Status::OK)
        {
            std::memcpy(primary_page_data,
                        primary_page_snapshot.data(),
                        primary_page_snapshot.size());
            buffer_pool_->unpinPageGlobal(primary_gpid, false, ctx);
            return status;
        }

        buffer_pool_->unpinPageGlobal(primary_gpid, primary_dirty, ctx);
        if (primary_dirty)
        {
            markDirtyPage(primary_gpid);
        }

        for (const TID &transient_tid : transient_chain_tids)
        {
            if (transient_tid.gpid == primary_gpid)
            {
                continue;
            }

            void *chain_page_buffer = nullptr;
            Status pin_status = buffer_pool_->pinPageGlobal(transient_tid.gpid,
                                                            &chain_page_buffer,
                                                            ctx);
            if (pin_status != Status::OK)
            {
                return pin_status;
            }

            auto *chain_page_data = static_cast<uint8_t *>(chain_page_buffer);
            HeapPage chain_heap_page(chain_page_data, db_->page_size(), toast_mgr, db_,
                                     action.table_id);
            uint32_t reclaimed_chain_tuples = 0;
            uint32_t reclaimed_chain_bytes = 0;
            Status chain_status =
                chain_heap_page.reclaimVersionSlots({transient_tid.slot},
                                                    &reclaimed_chain_tuples,
                                                    &reclaimed_chain_bytes,
                                                    ctx);
            buffer_pool_->unpinPageGlobal(transient_tid.gpid,
                                          chain_status == Status::OK &&
                                              reclaimed_chain_tuples > 0,
                                          ctx);
            if (chain_status != Status::OK)
            {
                return chain_status;
            }
            if (reclaimed_chain_tuples > 0)
            {
                markDirtyPage(transient_tid.gpid);
            }
        }

        return Status::OK;
    }

    auto StorageEngine::createScan(const ID &table_id, ErrorContext *ctx)
        -> std::unique_ptr<HeapScanIterator>
    {
        // For now, we don't need table info - just return a scanner
        // In a real system, we'd track heap pages per table in the catalog

        // For now, assume heap pages start after catalog pages
        // In a real system, we'd track this in the catalog
        uint32_t start_page = Config::getInstance().getUInt("storage", "heap_scan_start_page", 7);
        if (!isZeroId(table_id))
        {
            // Table-specific scans rely on table_id filtering, so scan from the start.
            start_page = 0;
        }
        else if (page_manager_ && start_page >= page_manager_->totalPages())
        {
            start_page = 0;
        }

        if (!isZeroId(table_id) && db_ && db_->table_stats_manager())
        {
            db_->table_stats_manager()->recordSeqScan(table_id);
        }

        return std::unique_ptr<HeapScanIterator>(
            new (std::nothrow) HeapScanIterator(db_, this, table_id, start_page, false));
    }

    auto StorageEngine::createScanAll(const ID &table_id, ErrorContext *ctx)
        -> std::unique_ptr<HeapScanIterator>
    {
        uint32_t start_page = Config::getInstance().getUInt("storage", "heap_scan_start_page", 7);
        if (!isZeroId(table_id))
        {
            start_page = 0;
        }
        else if (page_manager_ && start_page >= page_manager_->totalPages())
        {
            start_page = 0;
        }

        return std::unique_ptr<HeapScanIterator>(
            new (std::nothrow) HeapScanIterator(db_, this, table_id, start_page, true));
    }

    auto StorageEngine::isVisible(uint64_t xmin, uint64_t xmax, uint64_t current_xid) const -> bool
    {
        TransactionManager *tm = (db_ != nullptr) ? db_->transaction_manager() : nullptr;
        if (tm != nullptr)
        {
            ConnectionContext *conn_ctx = ConnectionContext::getCurrent();
            return tm->evaluateRuntimeRecordVisibility(xmin, xmax, current_xid, conn_ctx).visible;
        }

        return TransactionManager::evaluateBootstrapRecordVisibility(xmin, xmax, current_xid)
            .visible;
    }

    auto StorageEngine::getCurrentXid() const -> uint64_t
    {
        // Get current XID from ConnectionContext (Phase 2 complete)
        uint64_t xid = ConnectionContext::getCurrentTransactionId();
        if (xid != 0)
        {
            return xid;
        }

        // Fallback if no connection context
        if (db_->transaction_manager() != nullptr)
        {
            return db_->transaction_manager()->getCurrentXid();
        }
        return config::DEFAULT_INITIAL_XID; // Default if no transaction manager
    }

    auto StorageEngine::findFreePage(const ID &table_id, uint32_t tuple_size, uint32_t *page_id_out,
                                     uint16_t tablespace_id, ErrorContext *ctx) -> Status
    {
        // For simplicity, we'll scan existing heap pages linearly
        // In a real system, we'd maintain a free space map per table

        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            uint32_t total_pages = page_manager_->totalPages();
            // Start scanning after catalog pages
            uint32_t heap_start = Config::getInstance().getUInt("storage", "heap_scan_start_page", 7);
            for (uint32_t page_id = heap_start; page_id < total_pages; page_id++)
            { // Arbitrary limit
                void *page_buffer;
                Status status = buffer_pool_->pinPage(page_id, &page_buffer, ctx);
                auto *page_data = static_cast<uint8_t *>(page_buffer);

                if (status == Status::IO_ERROR)
                {
                    // Page doesn't exist, allocate it
                    status = allocateHeapPage(table_id, tablespace_id, page_id_out, ctx);
                    return status;
                }

                if (status != Status::OK)
                {
                    continue;
                }

                // Check if this is a heap page for our table
                auto *hdr = reinterpret_cast<PageHeader *>(page_data);
                if (hdr->page_type == PAGE_TYPE_HEAP)
                {
                    if (!isZeroId(table_id) && !pageTableIdMatches(hdr, table_id))
                    {
                        buffer_pool_->unpinPage(page_id, false, ctx);
                        continue;
                    }
                    HeapPage heap_page(page_data, db_->page_size());

                    if (heap_page.hasFreeSpace(tuple_size + sizeof(TupleHeader)))
                    {
                        buffer_pool_->unpinPage(page_id, false, ctx);
                        *page_id_out = page_id;
                        return Status::OK;
                    }
                }

                buffer_pool_->unpinPage(page_id, false, ctx);
            }

            // No existing page has space, allocate a new one
            return allocateHeapPage(table_id, tablespace_id, page_id_out, ctx);
        }

        std::vector<GPID> allocated_pages;
        Status status = page_manager_->getAllocatedPages(tablespace_id, allocated_pages, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        for (const auto &gpid : allocated_pages)
        {
            uint64_t page_number = getPageNumber(gpid);
            if (page_number < 2)
            {
                continue;
            }

            void *page_buffer;
            status = buffer_pool_->pinPageGlobal(gpid, &page_buffer, ctx);
            auto *page_data = static_cast<uint8_t *>(page_buffer);
            if (status != Status::OK)
            {
                continue;
            }

            auto *hdr = reinterpret_cast<PageHeader *>(page_data);
            if (hdr->page_type == PAGE_TYPE_HEAP)
            {
                if (!isZeroId(table_id) && !pageTableIdMatches(hdr, table_id))
                {
                    buffer_pool_->unpinPageGlobal(gpid, false, ctx);
                    continue;
                }
                HeapPage heap_page(page_data, db_->page_size());
                if (heap_page.hasFreeSpace(tuple_size + sizeof(TupleHeader)))
                {
                    buffer_pool_->unpinPageGlobal(gpid, false, ctx);
                    *page_id_out = static_cast<uint32_t>(page_number);
                    return Status::OK;
                }
            }

            buffer_pool_->unpinPageGlobal(gpid, false, ctx);
        }

        return allocateHeapPage(table_id, tablespace_id, page_id_out, ctx);
    }

    auto StorageEngine::findBackVersionPlacementPage(const ID &table_id, uint32_t tuple_size,
                                                     uint32_t primary_page_id,
                                                     uint16_t tablespace_id,
                                                     uint32_t *page_id_out,
                                                     ErrorContext *ctx) -> Status
    {
        if (page_id_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Back version placement output cannot be null");
            return Status::INVALID_ARGUMENT;
        }

        BackVersionPlacementCandidate best_candidate;

        auto considerPinnedPage = [&](uint32_t candidate_page_id, uint8_t *page_data) -> void
        {
            if (candidate_page_id == primary_page_id || page_data == nullptr)
            {
                return;
            }

            auto *hdr = reinterpret_cast<PageHeader *>(page_data);
            if (hdr->page_type != PAGE_TYPE_HEAP)
            {
                return;
            }
            if (!isZeroId(table_id) && !pageTableIdMatches(hdr, table_id))
            {
                return;
            }

            HeapPage heap_page(page_data, db_->page_size());
            if (!heap_page.hasFreeSpace(tuple_size))
            {
                return;
            }

            BackVersionPlacementCandidate candidate =
                scoreBackVersionPlacementCandidate(primary_page_id, candidate_page_id);
            if (isBetterBackVersionPlacement(candidate, best_candidate))
            {
                best_candidate = candidate;
            }
        };

        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            uint32_t total_pages = page_manager_->totalPages();
            uint32_t heap_start =
                Config::getInstance().getUInt("storage", "heap_scan_start_page", 7);
            for (uint32_t page_id = heap_start; page_id < total_pages; ++page_id)
            {
                void *page_buffer = nullptr;
                Status status = buffer_pool_->pinPage(page_id, &page_buffer, ctx);
                if (status != Status::OK)
                {
                    continue;
                }

                considerPinnedPage(page_id, static_cast<uint8_t *>(page_buffer));
                buffer_pool_->unpinPage(page_id, false, ctx);
            }
        }
        else
        {
            std::vector<GPID> allocated_pages;
            Status status = page_manager_->getAllocatedPages(tablespace_id, allocated_pages, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            for (const auto &candidate_gpid : allocated_pages)
            {
                uint32_t candidate_page_id =
                    static_cast<uint32_t>(getPageNumber(candidate_gpid));
                if (candidate_page_id < 2)
                {
                    continue;
                }

                void *page_buffer = nullptr;
                status = buffer_pool_->pinPageGlobal(candidate_gpid, &page_buffer, ctx);
                if (status != Status::OK)
                {
                    continue;
                }

                considerPinnedPage(candidate_page_id, static_cast<uint8_t *>(page_buffer));
                buffer_pool_->unpinPageGlobal(candidate_gpid, false, ctx);
            }
        }

        if (best_candidate.valid)
        {
            *page_id_out = best_candidate.page_id;
            return Status::OK;
        }

        return allocateHeapPage(table_id, tablespace_id, page_id_out, ctx);
    }

    auto StorageEngine::allocateHeapPage(const ID &table_id, uint16_t tablespace_id,
                                         uint32_t *page_id_out, ErrorContext *ctx) -> Status
    {
        CatalogManager::TableInfo table_info;
        Status status = catalog_manager_->getTable(table_id, table_info, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate a new page
        GPID gpid = INVALID_GPID;
        void *page_buffer;
        status = buffer_pool_->allocatePageGlobal(tablespace_id, &gpid, &page_buffer, ctx);
        auto *page_data = static_cast<uint8_t *>(page_buffer);
        if (status != Status::OK)
        {
            return status;
        }

        // Initialize as heap page
        std::memset(page_data, 0, db_->page_size());
        uint32_t page_id = static_cast<uint32_t>(getPageNumber(gpid));
        HeapPage heap_page(page_data, db_->page_size(), nullptr, db_, table_id);
        status = heap_page.initialize(page_id, ctx);
        if (status == Status::OK)
        {
            const bool temporary_work =
                table_info.temp_data_scope != CatalogManager::TempDataScope::NONE ||
                table_info.temp_metadata_scope != CatalogManager::TempMetadataScope::NONE;
            heap_page.applyOwningTableContract(temporary_work);
        }

        if (status == Status::OK)
        {
            // Page will be marked dirty on unpin
            *page_id_out = page_id;
            buffer_pool_->unpinPageGlobal(gpid, true, ctx);
        }
        else
        {
            // HIGH-7 FIX: Initialize failed - clean up allocated page
            // Unpin the page (no dirty flag needed since initialization failed)
            buffer_pool_->unpinPageGlobal(gpid, false, ctx);
            // Free the allocated page to prevent leak
            page_manager_->freePageGlobal(gpid, ctx);
        }

        return status;
    }

    // HeapScanIterator implementation

    HeapScanIterator::HeapScanIterator(Database *db, StorageEngine *engine, const ID &table_id,
                                       uint32_t start_page, bool ignore_visibility)
        : db_(db), engine_(engine), table_id_(table_id), current_page_(start_page),
          current_item_(0), last_page_(0), done_(false), ignore_visibility_(ignore_visibility)
    {
        ra_current_pages_ = READAHEAD_MIN_PAGES;

        if (db_ && db_->page_manager())
        {
            uint32_t total_pages = db_->page_manager()->totalPages();
            if (total_pages > 0)
            {
                last_page_ = total_pages - 1;
            }
        }

        if (db_ && !isZeroId(table_id_))
        {
            CatalogManager::TableInfo table_info;
            if (db_->catalog_manager()->getTable(table_id_, table_info, nullptr) == Status::OK)
            {
                if (table_info.temp_data_scope != CatalogManager::TempDataScope::NONE)
                {
                    ConnectionContext *conn_ctx = ConnectionContext::getCurrent();
                    session_id_ = conn_ctx ? conn_ctx->effectiveSessionId() : ID{};
                    filter_session_ = true;
                }
                tablespace_id_ = table_info.tablespace_id;
            }
        }

        if (db_ && db_->page_manager() && tablespace_id_ != PRIMARY_TABLESPACE_ID)
        {
            db_->page_manager()->getAllocatedPages(tablespace_id_, allocated_pages_, nullptr);
        }
    }

    HeapScanIterator::~HeapScanIterator()
    {
        if (page_data_ != nullptr)
        {
            if (tablespace_id_ == PRIMARY_TABLESPACE_ID)
            {
                db_->buffer_pool()->unpinPage(current_page_, false, nullptr);
            }
            else
            {
                db_->buffer_pool()->unpinPageGlobal(current_gpid_, false, nullptr);
            }
        }
    }

    auto HeapScanIterator::next(Tuple *tuple_out, ErrorContext *ctx) -> Status
    {
        if (done_)
        {
            return Status::NOT_FOUND;
        }

        if (tablespace_id_ == PRIMARY_TABLESPACE_ID)
        {
            while (current_page_ <= last_page_)
            {
                // Load current page if needed
                if (page_data_ == nullptr)
                {
                    Status status = loadPage(current_page_, ctx);
                    if (status == Status::IO_ERROR)
                    {
                        // Page doesn't exist, we're done
                        done_ = true;
                        return Status::NOT_FOUND;
                    }
                    if (status != Status::OK)
                    {
                        // Try next page
                        current_page_++;
                        current_item_ = 0;
                        continue;
                    }
                }

                // Check if this is a heap page
                auto *hdr = reinterpret_cast<PageHeader *>(page_data_);
                bool table_match = true;
                if (!isZeroId(table_id_))
                {
                    table_match = pageTableIdMatches(hdr, table_id_);
                }
                if (hdr->page_type != PAGE_TYPE_HEAP || !table_match)
                {
                    // Not a heap page, try next
                    db_->buffer_pool()->unpinPage(current_page_, false, ctx);
                    page_data_ = nullptr;
                    current_page_++;
                    current_item_ = 0;
                    continue;
                }

                // Scan items in current page
                HeapPage heap_page(page_data_, db_->page_size(), nullptr, db_, table_id_);
                ErrorContext validate_ctx;
                Status validate_status = heap_page.validate(&validate_ctx);
                if (validate_status != Status::OK)
                {
                    db_->buffer_pool()->unpinPage(current_page_, false, ctx);
                    page_data_ = nullptr;
                    current_page_++;
                    current_item_ = 0;
                    continue;
                }

                while (current_item_ < heap_page.getItemCount())
                {
                    const uint8_t *tuple_data;
                    uint32_t tuple_size;

                    Status status =
                        heap_page.getTuple(current_item_, &tuple_data, &tuple_size, nullptr);
                    current_item_++;

                    if (status == Status::OK)
                    {
                        if (!isZeroId(table_id_) && db_ && db_->table_stats_manager())
                        {
                            db_->table_stats_manager()->recordSeqRowsRead(table_id_, 1);
                        }
                        const auto *hdr = reinterpret_cast<const TupleHeader *>(tuple_data);
                        const TID candidate_tid(
                            makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(current_page_)),
                            current_item_ - 1);
                        if (ignore_visibility_)
                        {
                            if (filter_session_ && hdr->session_id != session_id_)
                            {
                                continue;
                            }
                            // Found visible tuple
                            if (tuple_out != nullptr)
                            {
                                tuple_out->data = tuple_data;
                                tuple_out->data_size = tuple_size;
                                // PHASE 1.5: Set TID struct
                                tuple_out->tid = TID(makeGPID(PRIMARY_TABLESPACE_ID,
                                                              static_cast<uint64_t>(current_page_)),
                                                     current_item_ - 1);
                            }
                            return Status::OK;
                        }

                        // Ordinary scans start from the current/root tuple slot only; historical
                        // versions are reached through the version chain rooted here.
                        if (hdr->ctid_gpid != candidate_tid.gpid ||
                            hdr->ctid_slot != candidate_tid.slot)
                        {
                            continue;
                        }

                        const uint8_t *visible_data = nullptr;
                        uint32_t visible_size = 0;
                        Status visible_status = heap_page.findVisibleVersion(
                            current_item_ - 1, engine_->getCurrentXid(), &visible_data,
                            &visible_size, nullptr, nullptr);
                        if (visible_status != Status::OK)
                        {
                            continue;
                        }

                        const auto *visible_hdr =
                            reinterpret_cast<const TupleHeader *>(visible_data);
                        if (filter_session_ && visible_hdr->session_id != session_id_)
                        {
                            continue;
                        }

                        if (tuple_out != nullptr)
                        {
                            const uint8_t *page_begin = page_data_;
                            const uint8_t *page_end = page_data_ + db_->page_size();
                            if (visible_data >= page_begin && visible_data < page_end)
                            {
                                tuple_out->data = visible_data;
                            }
                            else
                            {
                                visible_tuple_buffer_.assign(visible_data, visible_data + visible_size);
                                tuple_out->data = visible_tuple_buffer_.data();
                            }
                            tuple_out->data_size = visible_size;
                            tuple_out->tid = candidate_tid;
                        }
                        return Status::OK;
                    }
                }

                // Move to next page
                db_->buffer_pool()->unpinPage(current_page_, false, ctx);
                page_data_ = nullptr;
                current_page_++;
                current_item_ = 0;
            }

            done_ = true;
            return Status::NOT_FOUND;
        }

        while (current_page_index_ < allocated_pages_.size())
        {
            if (page_data_ == nullptr)
            {
                Status status = loadPage(static_cast<uint32_t>(current_page_index_), ctx);
                if (status != Status::OK)
                {
                    current_page_index_++;
                    current_item_ = 0;
                    continue;
                }
            }

            auto *hdr = reinterpret_cast<PageHeader *>(page_data_);
            bool table_match = true;
            if (!isZeroId(table_id_))
            {
                table_match = pageTableIdMatches(hdr, table_id_);
            }
            if (hdr->page_type != PAGE_TYPE_HEAP || !table_match)
            {
                db_->buffer_pool()->unpinPageGlobal(current_gpid_, false, ctx);
                page_data_ = nullptr;
                current_page_index_++;
                current_item_ = 0;
                continue;
            }

            HeapPage heap_page(page_data_, db_->page_size(), nullptr, db_, table_id_);
            ErrorContext validate_ctx;
            Status validate_status = heap_page.validate(&validate_ctx);
            if (validate_status != Status::OK)
            {
                db_->buffer_pool()->unpinPageGlobal(current_gpid_, false, ctx);
                page_data_ = nullptr;
                current_page_index_++;
                current_item_ = 0;
                continue;
            }

            while (current_item_ < heap_page.getItemCount())
            {
                const uint8_t *tuple_data;
                uint32_t tuple_size;

                Status status =
                    heap_page.getTuple(current_item_, &tuple_data, &tuple_size, nullptr);
                current_item_++;

                if (status == Status::OK)
                {
                    if (!isZeroId(table_id_) && db_ && db_->table_stats_manager())
                    {
                        db_->table_stats_manager()->recordSeqRowsRead(table_id_, 1);
                    }
                    const auto *hdr = reinterpret_cast<const TupleHeader *>(tuple_data);
                    const uint64_t page_number = getPageNumber(current_gpid_);
                    const TID candidate_tid(makeGPID(tablespace_id_, page_number),
                                            current_item_ - 1);
                    if (ignore_visibility_)
                    {
                        if (filter_session_ && hdr->session_id != session_id_)
                        {
                            continue;
                        }
                        if (tuple_out != nullptr)
                        {
                            tuple_out->data = tuple_data;
                            tuple_out->data_size = tuple_size;
                            tuple_out->tid = TID(makeGPID(tablespace_id_, page_number),
                                                 current_item_ - 1);
                        }
                        return Status::OK;
                    }

                    if (hdr->ctid_gpid != candidate_tid.gpid ||
                        hdr->ctid_slot != candidate_tid.slot)
                    {
                        continue;
                    }

                    const uint8_t *visible_data = nullptr;
                    uint32_t visible_size = 0;
                    Status visible_status = heap_page.findVisibleVersion(
                        current_item_ - 1, engine_->getCurrentXid(), &visible_data,
                        &visible_size, nullptr, nullptr);
                    if (visible_status != Status::OK)
                    {
                        continue;
                    }

                    const auto *visible_hdr =
                        reinterpret_cast<const TupleHeader *>(visible_data);
                    if (filter_session_ && visible_hdr->session_id != session_id_)
                    {
                        continue;
                    }

                    if (tuple_out != nullptr)
                    {
                        const uint8_t *page_begin = page_data_;
                        const uint8_t *page_end = page_data_ + db_->page_size();
                        if (visible_data >= page_begin && visible_data < page_end)
                        {
                            tuple_out->data = visible_data;
                        }
                        else
                        {
                            visible_tuple_buffer_.assign(visible_data, visible_data + visible_size);
                            tuple_out->data = visible_tuple_buffer_.data();
                        }
                        tuple_out->data_size = visible_size;
                        tuple_out->tid = candidate_tid;
                    }
                    return Status::OK;
                }
            }

            db_->buffer_pool()->unpinPageGlobal(current_gpid_, false, ctx);
            page_data_ = nullptr;
            current_page_index_++;
            current_item_ = 0;
        }

        done_ = true;
        return Status::NOT_FOUND;
    }

    auto HeapScanIterator::loadPage(uint32_t page_id, ErrorContext *ctx) -> Status
    {
        void *page_buffer;
        if (tablespace_id_ == PRIMARY_TABLESPACE_ID)
        {
            Status status = db_->buffer_pool()->pinPage(
                page_id, &page_buffer, ctx, BufferPool::AccessStrategy::Sequential);
            if (status == Status::OK)
            {
                page_data_ = static_cast<uint8_t *>(page_buffer);
                maybeReadAheadPrimary(page_id, ctx);
            }
            return status;
        }

        if (page_id >= allocated_pages_.size())
        {
            return Status::NOT_FOUND;
        }

        GPID gpid = allocated_pages_[page_id];
        if (getPageNumber(gpid) < 2)
        {
            return Status::NOT_FOUND;
        }

        Status status = db_->buffer_pool()->pinPageGlobal(
            gpid, &page_buffer, ctx, BufferPool::AccessStrategy::Sequential);
        if (status == Status::OK)
        {
            page_data_ = static_cast<uint8_t *>(page_buffer);
            current_gpid_ = gpid;
            maybeReadAheadTablespace(page_id, ctx);
        }
        return status;
    }

    void HeapScanIterator::maybeReadAheadPrimary(uint32_t page_id, ErrorContext *ctx)
    {
        bool sequential = (ra_last_page_ != UINT32_MAX && page_id == ra_last_page_ + 1);
        if (sequential)
        {
            ra_seq_count_++;
            if (ra_seq_count_ >= READAHEAD_SEQ_THRESHOLD)
            {
                if (ra_seq_count_ > READAHEAD_SEQ_THRESHOLD)
                {
                    ra_current_pages_ = std::min(READAHEAD_MAX_PAGES,
                                                 ra_current_pages_ * READAHEAD_GROWTH_FACTOR);
                }
                else if (ra_current_pages_ == 0)
                {
                    ra_current_pages_ = READAHEAD_MIN_PAGES;
                }
            }
        }
        else
        {
            ra_seq_count_ = 0;
            ra_current_pages_ = READAHEAD_MIN_PAGES;
        }

        ra_last_page_ = page_id;
        if (ra_seq_count_ < READAHEAD_SEQ_THRESHOLD)
        {
            return;
        }

        uint32_t start = page_id + 1;
        if (start > last_page_)
        {
            return;
        }

        uint32_t end = std::min(last_page_, page_id + ra_current_pages_);
        std::vector<uint32_t> page_ids;
        page_ids.reserve(end - start + 1);
        for (uint32_t next = start; next <= end; ++next)
        {
            page_ids.push_back(next);
        }

        db_->buffer_pool()->prefetchPages(page_ids, ctx, BufferPool::AccessStrategy::Sequential);
    }

    void HeapScanIterator::maybeReadAheadTablespace(size_t page_index, ErrorContext *ctx)
    {
        bool sequential = (ra_last_index_ != std::numeric_limits<size_t>::max() &&
                           page_index == ra_last_index_ + 1);
        if (sequential)
        {
            ra_seq_count_++;
            if (ra_seq_count_ >= READAHEAD_SEQ_THRESHOLD)
            {
                if (ra_seq_count_ > READAHEAD_SEQ_THRESHOLD)
                {
                    ra_current_pages_ = std::min(READAHEAD_MAX_PAGES,
                                                 ra_current_pages_ * READAHEAD_GROWTH_FACTOR);
                }
                else if (ra_current_pages_ == 0)
                {
                    ra_current_pages_ = READAHEAD_MIN_PAGES;
                }
            }
        }
        else
        {
            ra_seq_count_ = 0;
            ra_current_pages_ = READAHEAD_MIN_PAGES;
        }

        ra_last_index_ = page_index;
        if (ra_seq_count_ < READAHEAD_SEQ_THRESHOLD)
        {
            return;
        }

        size_t start = page_index + 1;
        if (start >= allocated_pages_.size())
        {
            return;
        }

        size_t end = std::min(allocated_pages_.size() - 1,
                              page_index + static_cast<size_t>(ra_current_pages_));
        std::vector<GPID> gpids;
        gpids.reserve(end - start + 1);
        for (size_t idx = start; idx <= end; ++idx)
        {
            GPID gpid = allocated_pages_[idx];
            if (getPageNumber(gpid) < 2)
            {
                continue;
            }
            gpids.push_back(gpid);
        }

        if (!gpids.empty())
        {
            db_->buffer_pool()->prefetchPagesGlobal(gpids, ctx,
                                                    BufferPool::AccessStrategy::Sequential);
        }
    }

    auto StorageEngine::deleteTuple(const ID &table_id, uint64_t tid, uint64_t xmax,
                                    ErrorContext *ctx) -> Status
    {
        TID decoded = convertLegacyTID(tid);
        return deleteTuple(table_id, decoded, ctx);
    }

    auto StorageEngine::deleteTuple(const ID &table_id, const TID &tid,
                                    ErrorContext *ctx) -> Status
    {
        uint32_t page_id = static_cast<uint32_t>(getPageNumber(tid.gpid));
        uint16_t item_id = tid.slot;
        uint16_t tablespace_id = getTablespaceID(tid.gpid);
        return deleteTuple(table_id, page_id, item_id, tablespace_id, ctx);
    }

    // MGA Phase 3: Version Chains

    auto StorageEngine::updateStableTidIndexesForMutation(const ID &table_id,
                                                          uint16_t tablespace_id,
                                                          uint32_t stable_page_id,
                                                          uint16_t stable_item_id,
                                                          const uint8_t *old_tuple_data,
                                                          uint32_t old_tuple_size,
                                                          const uint8_t *new_tuple_data,
                                                          uint32_t new_tuple_size,
                                                          uint64_t current_xid,
                                                          ErrorContext *ctx) -> Status
    {
        std::vector<CatalogManager::IndexInfo> indexes;
        Status index_status = catalog_manager_->listIndexesForTable(table_id, indexes, ctx, false);
        if (index_status != Status::OK || indexes.empty())
        {
            return Status::OK;
        }

        std::vector<CatalogManager::ColumnInfo> columns;
        index_status = catalog_manager_->getColumns(table_id, columns, ctx);
        if (index_status != Status::OK)
        {
            LOG_WARNING(STORAGE, "Skipping index updates due to column lookup failure");
            return Status::OK;
        }

        TID tid(makeGPID(tablespace_id, static_cast<uint64_t>(stable_page_id)), stable_item_id);
        IndexKeyExtractor extractor;

        ToastManager *toast_mgr = getOrCreateToastManager(table_id, ctx);
        std::vector<uint8_t> materialized_old_tuple;
        std::vector<uint8_t> materialized_new_tuple;
        const uint8_t *old_key_tuple_data = nullptr;
        const uint8_t *new_key_tuple_data = nullptr;
        uint32_t old_key_tuple_size = 0;
        uint32_t new_key_tuple_size = 0;

        Status old_materialize_status = materializeTupleForKeyExtraction(old_tuple_data,
                                                                         old_tuple_size,
                                                                         toast_mgr,
                                                                         current_xid,
                                                                         materialized_old_tuple,
                                                                         &old_key_tuple_data,
                                                                         &old_key_tuple_size,
                                                                         ctx);
        Status new_materialize_status = materializeTupleForKeyExtraction(new_tuple_data,
                                                                         new_tuple_size,
                                                                         toast_mgr,
                                                                         current_xid,
                                                                         materialized_new_tuple,
                                                                         &new_key_tuple_data,
                                                                         &new_key_tuple_size,
                                                                         ctx);
        if (old_materialize_status != Status::OK || new_materialize_status != Status::OK)
        {
            LOG_WARNING(STORAGE,
                        "Skipping index updates due to tuple materialization failure "
                        "(old=%d new=%d msg=%s)",
                        static_cast<int>(old_materialize_status),
                        static_cast<int>(new_materialize_status),
                        ctx ? ctx->message.c_str() : "unknown error");
            return Status::OK;
        }

        std::vector<size_t> old_offsets;
        std::vector<size_t> old_sizes;
        std::vector<size_t> new_offsets;
        std::vector<size_t> new_sizes;
        Status old_layout_status = computeColumnLayout(old_key_tuple_data,
                                                       old_key_tuple_size,
                                                       columns,
                                                       db_->domain_manager(),
                                                       old_offsets,
                                                       old_sizes,
                                                       ctx);
        Status new_layout_status = computeColumnLayout(new_key_tuple_data,
                                                       new_key_tuple_size,
                                                       columns,
                                                       db_->domain_manager(),
                                                       new_offsets,
                                                       new_sizes,
                                                       ctx);
        if (old_layout_status != Status::OK || new_layout_status != Status::OK)
        {
            LOG_WARNING(STORAGE, "Skipping index updates due to column layout failure");
            return Status::OK;
        }

        for (const auto &index_info : indexes)
        {
            extractor.clearCache();

            CatalogManager::IndexType actual_index_type;
            void *index_ptr = catalog_manager_->getIndexPtr(index_info.index_id, &actual_index_type);
            if (index_ptr == nullptr)
            {
                LOG_WARNING(STORAGE, "Index %s not found in cache, skipping",
                            index_info.index_name.c_str());
                continue;
            }

            if (actual_index_type == CatalogManager::IndexType::COLUMNSTORE)
            {
                auto *columnstore = static_cast<ColumnstoreIndex *>(index_ptr);
                for (const auto &col_id : index_info.column_ids)
                {
                    size_t col_idx = 0;
                    bool found = false;
                    for (size_t i = 0; i < columns.size(); i++)
                    {
                        if (columns[i].column_id == col_id)
                        {
                            col_idx = i;
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                    {
                        LOG_WARNING(STORAGE, "Column not found for columnstore index %s",
                                    index_info.index_name.c_str());
                        continue;
                    }

                    bool is_null = (new_offsets[col_idx] == 0 && new_sizes[col_idx] == 0);
                    const void *col_value =
                        is_null ? nullptr : (new_tuple_data + new_offsets[col_idx]);
                    size_t col_value_len = new_sizes[col_idx];

                    Status insert_status =
                        columnstore->insert(col_id, tid, col_value, col_value_len, is_null, ctx);
                    if (insert_status != Status::OK)
                    {
                        LOG_WARNING(STORAGE, "Failed to insert into columnstore index %s: %s",
                                    index_info.index_name.c_str(),
                                    ctx ? ctx->message.c_str() : "unknown error");
                    }
                }
                continue;
            }

            std::vector<uint16_t> column_indices;
            for (const auto &col_id : index_info.column_ids)
            {
                for (size_t i = 0; i < columns.size(); i++)
                {
                    if (columns[i].column_id == col_id)
                    {
                        column_indices.push_back(static_cast<uint16_t>(i));
                        break;
                    }
                }
            }

            std::vector<uint8_t> old_key;
            std::vector<uint8_t> new_key;
            Status extract_status = extractor.extractKeyForUpdate(
                old_key_tuple_data,
                old_key_tuple_size,
                old_offsets,
                old_sizes,
                new_key_tuple_data,
                new_key_tuple_size,
                new_offsets,
                new_sizes,
                column_indices,
                toast_mgr,
                current_xid,
                &old_key,
                &new_key,
                ctx);
            if (extract_status != Status::OK)
            {
                LOG_WARNING(STORAGE, "Failed to extract keys for index %s: %s",
                            index_info.index_name.c_str(),
                            ctx ? ctx->message.c_str() : "unknown error");
                continue;
            }

            if (old_key == new_key)
            {
                continue;
            }

            const bool exact_lookup_family = supportsExactKeyLookup(actual_index_type);
            Status retire_status = exact_lookup_family
                ? retireExactIndexEntry(actual_index_type,
                                        index_ptr,
                                        old_key,
                                        tid,
                                        current_xid,
                                        ExactIndexRetirementMode::SOFT_DELETE,
                                        ctx)
                : removeFromIndex(actual_index_type, index_ptr, old_key, tid, current_xid, ctx);
            if (retire_status != Status::OK)
            {
                LOG_WARNING(STORAGE, "Failed to retire old key from %s index %s: %s",
                            indexTypeToString(actual_index_type).c_str(),
                            index_info.index_name.c_str(),
                            ctx ? ctx->message.c_str() : "unknown error");
            }

            Status insert_status = insertIntoIndex(actual_index_type, index_ptr, new_key, tid,
                                                   current_xid, ctx);
            if (insert_status != Status::OK)
            {
                LOG_ERROR(STORAGE, "Failed to insert new key into %s index %s: %s",
                          indexTypeToString(actual_index_type).c_str(),
                          index_info.index_name.c_str(),
                          ctx ? ctx->message.c_str() : "unknown error");
            }
        }

        extractor.clearCache();
        return Status::OK;
    }

    auto StorageEngine::applyStableTidIndexBackout(const ID &table_id,
                                                   uint16_t tablespace_id,
                                                   uint32_t stable_page_id,
                                                   uint16_t stable_item_id,
                                                   const uint8_t *current_tuple_data,
                                                   uint32_t current_tuple_size,
                                                   const uint8_t *prior_tuple_data,
                                                   uint32_t prior_tuple_size,
                                                   const std::vector<std::vector<uint8_t>> &transient_tuple_images,
                                                   bool prior_row_present,
                                                   uint64_t current_xid,
                                                   ErrorContext *ctx) -> Status
    {
        return rewriteStableTidIndexesForRollback(table_id,
                                                  tablespace_id,
                                                  stable_page_id,
                                                  stable_item_id,
                                                  current_tuple_data,
                                                  current_tuple_size,
                                                  prior_tuple_data,
                                                  prior_tuple_size,
                                                  transient_tuple_images,
                                                  prior_row_present,
                                                  current_xid,
                                                  ctx);
    }

    auto StorageEngine::rewriteStableTidIndexesForRollback(const ID &table_id,
                                                           uint16_t tablespace_id,
                                                           uint32_t stable_page_id,
                                                           uint16_t stable_item_id,
                                                           const uint8_t *current_tuple_data,
                                                           uint32_t current_tuple_size,
                                                           const uint8_t *restored_tuple_data,
                                                           uint32_t restored_tuple_size,
                                                           const std::vector<std::vector<uint8_t>> &transient_tuple_images,
                                                           bool restored_row_present,
                                                           uint64_t current_xid,
                                                           ErrorContext *ctx) -> Status
    {
        if (isZeroId(table_id))
        {
            return Status::OK;
        }

        std::vector<CatalogManager::IndexInfo> indexes;
        Status index_status = catalog_manager_->listIndexesForTable(table_id, indexes, ctx, false);
        if (index_status != Status::OK || indexes.empty())
        {
            return Status::OK;
        }

        std::vector<CatalogManager::ColumnInfo> columns;
        index_status = catalog_manager_->getColumns(table_id, columns, ctx);
        if (index_status != Status::OK)
        {
            LOG_WARNING(STORAGE, "Skipping savepoint rollback index rewrite due to column lookup failure");
            return Status::OK;
        }

        if (current_tuple_data == nullptr || current_tuple_size < sizeof(TupleHeader))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Current tuple image is invalid for savepoint rollback");
            return Status::INVALID_ARGUMENT;
        }

        TID tid(makeGPID(tablespace_id, static_cast<uint64_t>(stable_page_id)), stable_item_id);
        IndexKeyExtractor extractor;
        ToastManager *toast_mgr = getOrCreateToastManager(table_id, ctx);
        auto appendUniqueKey = [](std::vector<std::vector<uint8_t>> &keys,
                                  const std::vector<uint8_t> &candidate) {
            if (std::find(keys.begin(), keys.end(), candidate) == keys.end())
            {
                keys.push_back(candidate);
            }
        };

        std::vector<uint8_t> materialized_current_tuple;
        const uint8_t *current_key_tuple_data = nullptr;
        uint32_t current_key_tuple_size = 0;
        Status current_materialize_status = materializeTupleForKeyExtraction(current_tuple_data,
                                                                             current_tuple_size,
                                                                             toast_mgr,
                                                                             current_xid,
                                                                             materialized_current_tuple,
                                                                             &current_key_tuple_data,
                                                                             &current_key_tuple_size,
                                                                             ctx);
        if (current_materialize_status != Status::OK)
        {
            LOG_WARNING(STORAGE,
                        "Skipping savepoint rollback index rewrite due to current tuple materialization failure (status=%d)",
                        static_cast<int>(current_materialize_status));
            return Status::OK;
        }

        std::vector<uint8_t> materialized_restored_tuple;
        const uint8_t *restored_key_tuple_data = nullptr;
        uint32_t restored_key_tuple_size = 0;
        if (restored_row_present)
        {
            Status restored_materialize_status =
                materializeTupleForKeyExtraction(restored_tuple_data,
                                                restored_tuple_size,
                                                toast_mgr,
                                                current_xid,
                                                materialized_restored_tuple,
                                                &restored_key_tuple_data,
                                                &restored_key_tuple_size,
                                                ctx);
            if (restored_materialize_status != Status::OK)
            {
                LOG_WARNING(STORAGE,
                            "Skipping savepoint rollback index rewrite due to restored tuple materialization failure (status=%d)",
                            static_cast<int>(restored_materialize_status));
                return Status::OK;
            }
        }

        std::vector<size_t> current_offsets;
        std::vector<size_t> current_sizes;
        Status current_layout_status = computeColumnLayout(current_key_tuple_data,
                                                           current_key_tuple_size,
                                                           columns,
                                                           db_->domain_manager(),
                                                           current_offsets,
                                                           current_sizes,
                                                           ctx);
        if (current_layout_status != Status::OK)
        {
            LOG_WARNING(STORAGE, "Skipping savepoint rollback index rewrite due to current column layout failure");
            return Status::OK;
        }

        std::vector<size_t> restored_offsets;
        std::vector<size_t> restored_sizes;
        if (restored_row_present)
        {
            Status restored_layout_status = computeColumnLayout(restored_key_tuple_data,
                                                                restored_key_tuple_size,
                                                                columns,
                                                                db_->domain_manager(),
                                                                restored_offsets,
                                                                restored_sizes,
                                                                ctx);
            if (restored_layout_status != Status::OK)
            {
                LOG_WARNING(STORAGE, "Skipping savepoint rollback index rewrite due to restored column layout failure");
                return Status::OK;
            }
        }

        std::vector<ID> refreshed_index_ids;
        for (const auto &index_info : indexes)
        {
            CatalogManager::IndexType actual_index_type;
            void *index_ptr = catalog_manager_->getIndexPtr(index_info.index_id, &actual_index_type);
            if (index_ptr == nullptr)
            {
                LOG_WARNING(STORAGE, "Index %s not found in cache during savepoint rollback",
                            index_info.index_name.c_str());
                continue;
            }

            if (actual_index_type == CatalogManager::IndexType::COLUMNSTORE)
            {
                auto *columnstore = static_cast<ColumnstoreIndex *>(index_ptr);
                for (const auto &col_id : index_info.column_ids)
                {
                    size_t col_idx = 0;
                    bool found = false;
                    for (size_t i = 0; i < columns.size(); i++)
                    {
                        if (columns[i].column_id == col_id)
                        {
                            col_idx = i;
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                    {
                        LOG_WARNING(STORAGE,
                                    "Column not found for columnstore rollback rewrite on index %s",
                                    index_info.index_name.c_str());
                        continue;
                    }

                    bool restored_is_null =
                        (restored_offsets[col_idx] == 0 && restored_sizes[col_idx] == 0);
                    const void *restored_value = restored_is_null
                        ? nullptr
                        : (restored_key_tuple_data + restored_offsets[col_idx]);
                    size_t restored_value_len = restored_sizes[col_idx];
                    Status insert_status = columnstore->insert(col_id,
                                                               tid,
                                                               restored_value,
                                                               restored_value_len,
                                                               restored_is_null,
                                                               ctx);
                    if (insert_status != Status::OK)
                    {
                        LOG_WARNING(STORAGE,
                                    "Failed to insert columnstore value into %s during savepoint rollback: %s",
                                    index_info.index_name.c_str(),
                                    ctx ? ctx->message.c_str() : "unknown error");
                    }
                }
                refreshed_index_ids.push_back(index_info.index_id);
                continue;
            }

            std::vector<uint16_t> column_indices;
            for (const auto &col_id : index_info.column_ids)
            {
                for (size_t i = 0; i < columns.size(); i++)
                {
                    if (columns[i].column_id == col_id)
                    {
                        column_indices.push_back(static_cast<uint16_t>(i));
                        break;
                    }
                }
            }

            std::vector<uint8_t> current_key;
            Status extract_current_status = extractor.extractKey(current_key_tuple_data,
                                                                 current_key_tuple_size,
                                                                 current_offsets,
                                                                 current_sizes,
                                                                 column_indices,
                                                                 toast_mgr,
                                                                 current_xid,
                                                                 &current_key,
                                                                 ctx);
            if (extract_current_status != Status::OK)
            {
                LOG_WARNING(STORAGE, "Failed to extract current key for rollback on index %s: %s",
                            index_info.index_name.c_str(),
                            ctx ? ctx->message.c_str() : "unknown error");
                continue;
            }

            const bool exact_lookup_family = supportsExactKeyLookup(actual_index_type);
            std::vector<uint8_t> restored_key;
            bool have_restored_key = false;
            if (restored_row_present)
            {
                extractor.clearCache();
                Status extract_restored_status = extractor.extractKey(restored_key_tuple_data,
                                                                      restored_key_tuple_size,
                                                                      restored_offsets,
                                                                      restored_sizes,
                                                                      column_indices,
                                                                      toast_mgr,
                                                                      current_xid,
                                                                      &restored_key,
                                                                      ctx);
                if (extract_restored_status != Status::OK)
                {
                    LOG_WARNING(STORAGE, "Failed to extract restored key for rollback on index %s: %s",
                                index_info.index_name.c_str(),
                                ctx ? ctx->message.c_str() : "unknown error");
                    continue;
                }
                have_restored_key = true;
            }

            if (exact_lookup_family)
            {
                std::vector<std::vector<uint8_t>> purge_keys;
                appendUniqueKey(purge_keys, current_key);

                for (const auto &transient_tuple_image : transient_tuple_images)
                {
                    if (transient_tuple_image.size() < sizeof(TupleHeader))
                    {
                        continue;
                    }

                    std::vector<uint8_t> materialized_transient_tuple;
                    const uint8_t *transient_key_tuple_data = nullptr;
                    uint32_t transient_key_tuple_size = 0;
                    Status transient_materialize_status =
                        materializeTupleForKeyExtraction(
                            transient_tuple_image.data(),
                            static_cast<uint32_t>(transient_tuple_image.size()),
                            toast_mgr,
                            current_xid,
                            materialized_transient_tuple,
                            &transient_key_tuple_data,
                            &transient_key_tuple_size,
                            ctx);
                    if (transient_materialize_status != Status::OK)
                    {
                        LOG_WARNING(STORAGE,
                                    "Skipping transient rollback key purge on index %s due to tuple materialization failure (status=%d)",
                                    index_info.index_name.c_str(),
                                    static_cast<int>(transient_materialize_status));
                        continue;
                    }

                    std::vector<size_t> transient_offsets;
                    std::vector<size_t> transient_sizes;
                    Status transient_layout_status =
                        computeColumnLayout(transient_key_tuple_data,
                                            transient_key_tuple_size,
                                            columns,
                                            db_->domain_manager(),
                                            transient_offsets,
                                            transient_sizes,
                                            ctx);
                    if (transient_layout_status != Status::OK)
                    {
                        LOG_WARNING(STORAGE,
                                    "Skipping transient rollback key purge on index %s due to column layout failure",
                                    index_info.index_name.c_str());
                        continue;
                    }

                    std::vector<uint8_t> transient_key;
                    extractor.clearCache();
                    Status extract_transient_status =
                        extractor.extractKey(transient_key_tuple_data,
                                             transient_key_tuple_size,
                                             transient_offsets,
                                             transient_sizes,
                                             column_indices,
                                             toast_mgr,
                                             current_xid,
                                             &transient_key,
                                             ctx);
                    if (extract_transient_status != Status::OK)
                    {
                        LOG_WARNING(STORAGE,
                                    "Failed to extract transient key for rollback on index %s: %s",
                                    index_info.index_name.c_str(),
                                    ctx ? ctx->message.c_str() : "unknown error");
                        continue;
                    }

                    appendUniqueKey(purge_keys, transient_key);
                }

                for (const auto &purge_key : purge_keys)
                {
                    if (have_restored_key && purge_key == restored_key)
                    {
                        continue;
                    }

                    Status purge_status = retireExactIndexEntry(actual_index_type,
                                                                index_ptr,
                                                                purge_key,
                                                                tid,
                                                                current_xid,
                                                                ExactIndexRetirementMode::HARD_REMOVE,
                                                                ctx);
                    if (purge_status != Status::OK)
                    {
                        LOG_WARNING(STORAGE, "Failed to purge current key from %s during savepoint rollback: %s",
                                    indexTypeToString(actual_index_type).c_str(),
                                    index_info.index_name.c_str(),
                                    ctx ? ctx->message.c_str() : "unknown error");
                    }
                }

                if (have_restored_key)
                {
                    Status restore_status = restoreExactIndexEntry(actual_index_type,
                                                                  index_ptr,
                                                                  restored_key,
                                                                  tid,
                                                                  current_xid,
                                                                  ctx);
                    if (restore_status == Status::NOT_FOUND)
                    {
                        Status insert_status = insertIntoIndex(actual_index_type,
                                                               index_ptr,
                                                               restored_key,
                                                               tid,
                                                               current_xid,
                                                               ctx);
                        if (insert_status != Status::OK)
                        {
                            LOG_WARNING(STORAGE, "Failed to insert restored key into %s during savepoint rollback: %s",
                                        indexTypeToString(actual_index_type).c_str(),
                                        index_info.index_name.c_str(),
                                        ctx ? ctx->message.c_str() : "unknown error");
                        }
                    }
                    else if (restore_status != Status::OK)
                    {
                        LOG_WARNING(STORAGE, "Failed to restore soft-deleted key in %s during savepoint rollback: %s",
                                    indexTypeToString(actual_index_type).c_str(),
                                    index_info.index_name.c_str(),
                                    ctx ? ctx->message.c_str() : "unknown error");
                    }
                }

                refreshed_index_ids.push_back(index_info.index_id);
                continue;
            }

            Status remove_status = removeFromIndex(actual_index_type, index_ptr, current_key, tid,
                                                   current_xid, ctx);
            if (remove_status != Status::OK)
            {
                LOG_WARNING(STORAGE, "Failed to remove current key from %s during savepoint rollback: %s",
                            indexTypeToString(actual_index_type).c_str(),
                            index_info.index_name.c_str(),
                            ctx ? ctx->message.c_str() : "unknown error");
            }

            if (!have_restored_key)
            {
                continue;
            }

            Status insert_status = insertIntoIndex(actual_index_type, index_ptr, restored_key, tid,
                                                   current_xid, ctx);
            if (insert_status != Status::OK)
            {
                LOG_WARNING(STORAGE, "Failed to insert restored key into %s during savepoint rollback: %s",
                            indexTypeToString(actual_index_type).c_str(),
                            index_info.index_name.c_str(),
                            ctx ? ctx->message.c_str() : "unknown error");
            }

            refreshed_index_ids.push_back(index_info.index_id);
        }

        for (const ID &index_id : refreshed_index_ids)
        {
            Status refresh_status = catalog_manager_->refreshIndexObject(index_id, ctx);
            if (refresh_status != Status::OK)
            {
                return refresh_status;
            }
        }

        extractor.clearCache();
        return Status::OK;
    }

    auto StorageEngine::updateTuple(const ID &table_id, uint32_t page_id, uint16_t item_id,
                                    const uint8_t *new_tuple_data, uint32_t new_tuple_size,
                                    uint32_t *new_page_id_out, uint16_t *new_item_id_out,
                                    ErrorContext *ctx) -> Status
    {
        // Sprint 4 Task 5.4.3: Check if table is being migrated
        CatalogManager::TableInfo table_info;
        Status migration_check_status = catalog_manager_->getTable(table_id, table_info, ctx);
        bool is_migrating = (migration_check_status == Status::OK && table_info.migration_in_progress);
        uint16_t tablespace_id = (migration_check_status == Status::OK) ? table_info.tablespace_id : 0;
        const uint8_t *tuple_data_ptr = new_tuple_data;
        std::vector<uint8_t> temp_tuple_buffer;
        if (migration_check_status == Status::OK &&
            table_info.temp_data_scope != CatalogManager::TempDataScope::NONE)
        {
            ConnectionContext *conn_ctx = ConnectionContext::getCurrent();
            ID session_id = conn_ctx ? conn_ctx->effectiveSessionId() : ID{};
            if (!isZeroId(session_id))
            {
                temp_tuple_buffer.assign(new_tuple_data, new_tuple_data + new_tuple_size);
                if (new_tuple_size >= sizeof(TupleHeader))
                {
                    auto *header = reinterpret_cast<TupleHeader *>(temp_tuple_buffer.data());
                    header->session_id = session_id;
                    tuple_data_ptr = temp_tuple_buffer.data();
                }
            }
        }

        // Get proc_id from ConnectionContext (Phase 2 complete)
        int32_t proc_id_signed = ConnectionContext::getCurrentProcId();
        uint32_t proc_id = (proc_id_signed >= 0) ? static_cast<uint32_t>(proc_id_signed) : 0;

        // Acquire lock on old tuple (Phase 2.5 complete)
        // Get wait setting from ConnectionContext (default: wait for locks)
        ConnectionContext *conn_ctx = ConnectionContext::getCurrent();
        bool wait = conn_ctx ? conn_ctx->getWaitForLocks() : true;
        Status lock_status = acquireTupleLock(table_id, page_id, item_id, proc_id, wait, ctx);
        if (lock_status != Status::OK)
        {
            return lock_status;
        }
        struct TupleLockGuard
        {
            StorageEngine *engine;
            ID table_id;
            uint32_t page_id;
            uint16_t item_id;
            uint32_t proc_id;
            ErrorContext *ctx;
            bool armed;
            ~TupleLockGuard()
            {
                if (!armed || engine == nullptr)
                {
                    return;
                }
                Status release_status =
                    engine->releaseTupleLock(table_id, page_id, item_id, proc_id, ctx);
                if (release_status != Status::OK && release_status != Status::NOT_FOUND)
                {
                    LOG_WARNING(STORAGE, "Failed to release tuple lock: status=%d",
                                static_cast<int>(release_status));
                }
            }
        } tuple_lock_guard{this, table_id, page_id, item_id, proc_id, ctx, true};

        // Get current XID from connection context (same approach as insertTuple)
        uint64_t xmax = ConnectionContext::getCurrentTransactionId();
        if (xmax == 0)
        {
            // No active connection context - use fallback XID
            xmax = config::DEFAULT_INITIAL_XID;
        }
        uint64_t new_xmin = xmax; // New version gets same XID as update

        // Pin the page
        GPID gpid = makeGPID(tablespace_id, static_cast<uint64_t>(page_id));
        void *page_buffer;
        Status status = buffer_pool_->pinPageGlobal(gpid, &page_buffer, ctx);
        auto *page_data = static_cast<uint8_t *>(page_buffer);
        if (status != Status::OK)
        {
            return status;
        }

        // Use full constructor with database to enable cross-page back versions
        ToastManager *toast_mgr = getOrCreateToastManager(table_id, ctx);

        // Capture the current tuple before update for index key comparison.
        std::vector<uint8_t> old_tuple_buffer;
        const bool old_payload_requires_savepoint_retention =
            conn_ctx != nullptr && conn_ctx->hasActiveSavepoints() &&
            !conn_ctx->isSavepointRollbackInProgress();
        OversizedValueLifecycleInput oversized_value_input{};
        oversized_value_input.family = OversizedValueFamily::heap_toast;
        oversized_value_input.savepoint_visible = old_payload_requires_savepoint_retention;
        oversized_value_input.old_payload_still_referenced =
            old_payload_requires_savepoint_retention;
        oversized_value_input.rewrite_publication_complete =
            !old_payload_requires_savepoint_retention;
        const OversizedValueLifecycleDecision oversized_value_decision =
            classifyOversizedValueLifecycle(oversized_value_input);
        const bool defer_old_toast_cleanup =
            oversized_value_decision.action ==
            OversizedValueActionKind::defer_old_payload_cleanup;
        uint32_t old_tuple_length = 0;
        {
            HeapPage snapshot_page(page_data, db_->page_size(), toast_mgr, db_, table_id);
            if (item_id >= snapshot_page.getItemCount())
            {
                buffer_pool_->unpinPageGlobal(gpid, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid item ID");
                return Status::INVALID_ARGUMENT;
            }

            const ItemPointer *items =
                reinterpret_cast<const ItemPointer *>(page_data + sizeof(PageHeader));
            uint32_t old_offset = items[item_id].offset;
            old_tuple_length = items[item_id].length;
            if (old_tuple_length == 0)
            {
                buffer_pool_->unpinPageGlobal(gpid, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Old tuple length is zero");
                return Status::INVALID_ARGUMENT;
            }

            old_tuple_buffer.resize(old_tuple_length);
            memcpy(old_tuple_buffer.data(), page_data + old_offset, old_tuple_length);
        }

        TID stable_tid = TID(makeGPID(tablespace_id, static_cast<uint64_t>(page_id)), item_id);
        Status unique_status = preflightUniqueUpdate(table_id,
                                                     old_tuple_buffer.data(),
                                                     old_tuple_length,
                                                     tuple_data_ptr,
                                                     new_tuple_size,
                                                     stable_tid,
                                                     xmax,
                                                     ctx);
        if (unique_status != Status::OK)
        {
            buffer_pool_->unpinPageGlobal(gpid, false, ctx);
            return unique_status;
        }

        PreparedStableHeadTuple prepared_head_tuple;
        status = prepareStableHeadTupleForMutation(tuple_data_ptr,
                                                  new_tuple_size,
                                                  new_xmin,
                                                  db_,
                                                  toast_mgr,
                                                  &prepared_head_tuple,
                                                  ctx);
        if (status != Status::OK)
        {
            buffer_pool_->unpinPageGlobal(gpid, false, ctx);
            return status;
        }

        auto cleanupPreparedHeadTuple = [&]()
        {
            if (!prepared_head_tuple.toasted || toast_mgr == nullptr)
            {
                return;
            }
            Status cleanup_status =
                toast_mgr->deleteToastValue(prepared_head_tuple.toast_value_id, xmax, ctx);
            if (cleanup_status != Status::OK && cleanup_status != Status::NOT_FOUND)
            {
                LOG_WARNING(STORAGE,
                            "Failed to cleanup prepared stable-head TOAST value (status=%d)",
                            static_cast<int>(cleanup_status));
            }
            prepared_head_tuple.toasted = false;
        };

        // Try to update tuple on same page
        HeapPage heap_page(page_data, db_->page_size(), toast_mgr, db_, table_id);
        uint16_t new_item_id;

        status = heap_page.updateTuple(item_id,
                                       prepared_head_tuple.storage_tuple_data,
                                       prepared_head_tuple.storage_tuple_size,
                                       xmax,
                                       new_xmin,
                                       &new_item_id,
                                       ctx,
                                       defer_old_toast_cleanup);

        if (status == Status::OK)
        {
            // Success - new version on same page. Stable-TID secondary effects
            // now route through the shared mutation helper used by cross-page
            // updates as well.
            updateStableTidIndexesForMutation(table_id,
                                              tablespace_id,
                                              page_id,
                                              item_id,
                                              old_tuple_buffer.data(),
                                              old_tuple_length,
                                              tuple_data_ptr,
                                              new_tuple_size,
                                              xmax,
                                              ctx);

            if (new_page_id_out != nullptr)
            {
                *new_page_id_out = page_id;
            }
            if (new_item_id_out != nullptr)
            {
                *new_item_id_out = new_item_id;
            }

            // Sprint 4 Task 5.4.3: Mark page dirty if migrating
            if (is_migrating)
            {
                catalog_manager_->markPageDirty(table_info.migration_id, page_id, ctx);
            }

            // Mark page as dirty for GC
            if (db_->garbage_collector() != nullptr && tablespace_id == PRIMARY_TABLESPACE_ID)
            {
                db_->garbage_collector()->markPageDirty(page_id);
            }

            if (ConnectionContext* conn_ctx = ConnectionContext::getCurrent())
            {
                conn_ctx->trackTupleMutation(table_id,
                                             page_id,
                                             item_id,
                                             old_tuple_buffer.data(),
                                             old_tuple_length);
                conn_ctx->recordTableDmlDelta(table_id, 0, 1, 0);
            }

            // Unpin with dirty flag
            buffer_pool_->unpinPageGlobal(gpid, true, ctx);
            return Status::OK;
        }
        else if (status == Status::PAGE_FULL)
        {
            // ====================================================================
            // SPRINT 0 FIX: CROSS-PAGE UPDATE USING FIREBIRD MGA
            // ====================================================================
            // CRITICAL FIX: Old (buggy) code created NEW tuple at NEW location (PostgreSQL MVCC)
            // NEW (correct) code creates BACK version at new location, modifies PRIMARY in-place (Firebird MGA)
            //
            // Key differences:
            // 1. Back version created (OLD data, not NEW data)
            // 2. Primary location overwritten in-place (NEW data)
            // 3. TID remains STABLE (same page_id, item_id)
            // 4. Indexes remain VALID (no index updates needed!)
            // ====================================================================

            // Step 1: Get OLD tuple data (we need to preserve it in back version)
            uint32_t old_length = old_tuple_length;

            // Get the original header; its xmin and chain metadata are preserved in the
            // back version stored through the shared mutation primitive.
            auto *old_tuple_hdr = reinterpret_cast<TupleHeader *>(old_tuple_buffer.data());

            // Unpin old page temporarily (will re-pin after creating back version)
            buffer_pool_->unpinPageGlobal(gpid, false, ctx);

            // Step 2: Allocate page for BACK version (OLD data)
            uint32_t back_version_page_id;
            status = findBackVersionPlacementPage(table_id, old_length, page_id,
                                                  tablespace_id, &back_version_page_id, ctx);
            if (status != Status::OK)
            {
                cleanupPreparedHeadTuple();
                SET_ERROR_CONTEXT(ctx, status, "Failed to find free page for back version");
                return status;
            }

            // Cross-page update contract: back version must not be placed on the
            // primary tuple page, otherwise overwriteTuple() can fail due to
            // self-consumed free space.
            if (back_version_page_id == page_id)
            {
                status = allocateHeapPage(table_id, tablespace_id, &back_version_page_id, ctx);
                if (status != Status::OK)
                {
                    cleanupPreparedHeadTuple();
                    SET_ERROR_CONTEXT(ctx, status,
                                      "Failed to allocate non-primary page for back version");
                    return status;
                }
            }

            // Pin the back version page
            void *back_page_buffer;
            GPID back_version_gpid = makeGPID(tablespace_id,
                                              static_cast<uint64_t>(back_version_page_id));
            status = buffer_pool_->pinPageGlobal(back_version_gpid, &back_page_buffer, ctx);
            if (status != Status::OK)
            {
                cleanupPreparedHeadTuple();
                SET_ERROR_CONTEXT(ctx, status, "Failed to pin page for back version");
                return status;
            }

            auto *back_page_data = static_cast<uint8_t *>(back_page_buffer);
            HeapPage back_heap_page(back_page_data, db_->page_size(), toast_mgr, db_, table_id);

            // Same-page and cross-page updates now share the same stored
            // back-version primitive instead of maintaining separate storage-side
            // mini state machines.
            uint16_t back_item_id;
            status = back_heap_page.storeBackVersionForMutation(old_tuple_buffer.data(),
                                                                old_length,
                                                                *old_tuple_hdr,
                                                                xmax,
                                                                makeGPID(tablespace_id,
                                                                         static_cast<uint64_t>(page_id)),
                                                                item_id,
                                                                true,
                                                                &back_item_id,
                                                                ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPageGlobal(back_version_gpid, false, ctx);
                cleanupPreparedHeadTuple();
                SET_ERROR_CONTEXT(ctx, status, "Failed to store back version");
                return status;
            }

            // Build GPID for back version (different page!)
            back_version_gpid = makeGPID(tablespace_id,
                                         static_cast<uint64_t>(back_version_page_id));

            // Unpin back version page (mark as dirty)
            buffer_pool_->unpinPageGlobal(back_version_gpid, true, ctx);

            // Mark back version page as dirty for GC
            if (db_->garbage_collector() != nullptr)
            {
                if (tablespace_id == PRIMARY_TABLESPACE_ID)
                {
                    db_->garbage_collector()->markPageDirty(back_version_page_id);
                }
            }

            // Step 3: Re-pin PRIMARY page and overwrite IN-PLACE
            status = buffer_pool_->pinPageGlobal(gpid, &page_buffer, ctx);
            if (status != Status::OK)
            {
                cleanupPreparedHeadTuple();
                SET_ERROR_CONTEXT(ctx, status, "Failed to re-pin primary page");
                return status;
            }

            page_data = static_cast<uint8_t *>(page_buffer);
            HeapPage primary_heap_page(page_data, db_->page_size(), toast_mgr, db_, table_id);

            // Overwrite primary tuple in-place (NEW data, back version on different page)
            status = primary_heap_page.overwriteTuple(item_id,
                                                      prepared_head_tuple.storage_tuple_data,
                                                      prepared_head_tuple.storage_tuple_size,
                                                      xmax,
                                                      new_xmin,
                                                      back_version_gpid,
                                                      back_item_id,
                                                      ctx);

            if (status != Status::OK)
            {
                buffer_pool_->unpinPageGlobal(gpid, false, ctx);
                cleanupPreparedHeadTuple();
                SET_ERROR_CONTEXT(ctx, status, "Failed to overwrite primary tuple");
                return status;
            }

            updateStableTidIndexesForMutation(table_id,
                                              tablespace_id,
                                              page_id,
                                              item_id,
                                              old_tuple_buffer.data(),
                                              old_length,
                                              tuple_data_ptr,
                                              new_tuple_size,
                                              xmax,
                                              ctx);

            // Sprint 4 Task 5.4.3: Mark pages dirty if migrating
            if (is_migrating)
            {
                // Mark both primary page and back version page as dirty
                catalog_manager_->markPageDirty(table_info.migration_id, page_id, ctx);
                catalog_manager_->markPageDirty(table_info.migration_id, back_version_page_id, ctx);
            }

            // Mark primary page as dirty for GC
            if (db_->garbage_collector() != nullptr)
            {
                if (tablespace_id == PRIMARY_TABLESPACE_ID)
                {
                    db_->garbage_collector()->markPageDirty(page_id);
                }
            }

            // Unpin primary page (mark as dirty)
            buffer_pool_->unpinPageGlobal(gpid, true, ctx);

            // Step 4: Return ORIGINAL TID (STABLE!)
            // This is the key benefit: TID never changes, indexes remain valid!
            if (new_page_id_out != nullptr)
            {
                *new_page_id_out = page_id;  // SAME page!
            }
            if (new_item_id_out != nullptr)
            {
                *new_item_id_out = item_id;  // SAME item!
            }

            // Step 5: Apply shared stable-TID secondary effects.
            // Stable TID alone is not enough when indexed column values change.
            // Secondary-index updates now flow through the same stable-TID
            // mutation helper as same-page updates.

            if (ConnectionContext* conn_ctx = ConnectionContext::getCurrent())
            {
                conn_ctx->trackTupleMutation(table_id,
                                             page_id,
                                             item_id,
                                             old_tuple_buffer.data(),
                                             old_length);
                conn_ctx->recordTableDmlDelta(table_id, 0, 1, 0);
            }

            return Status::OK;
        }
        else
        {
            // Other error
            cleanupPreparedHeadTuple();
            buffer_pool_->unpinPageGlobal(gpid, false, ctx);
            return status;
        }
    }

    auto StorageEngine::sequentialScan(const ID &table_id, const std::vector<uint32_t> &columns,
                                       uint64_t xmin, ErrorContext *ctx)
        -> std::unique_ptr<HeapScanIterator>
    {
        // For now, just use the existing create_scan method
        // In a real implementation, we would filter by columns and visibility
        return createScan(table_id, ctx);
    }

    // IndexScanIterator implementation

    IndexScanIterator::IndexScanIterator(Database *db, StorageEngine *engine, const ID &index_id,
                                         const ID &table_id)
        : db_(db), engine_(engine), index_id_(index_id), table_id_(table_id), done_(false),
          current_tuple_index_(0), initialized_(false)
    {
    }

    IndexScanIterator::~IndexScanIterator() = default;

    auto IndexScanIterator::seek(const std::vector<uint8_t> &key, ErrorContext *ctx) -> Status
    {
        // Get index information from catalog
        CatalogManager::IndexInfo index_info;
        Status status = db_->catalog_manager()->getIndex(index_id_, index_info, ctx);
        if (status != Status::OK)
        {
            done_ = true;
            return status;
        }

        CatalogManager::IndexType actual_index_type;
        void *index_ptr = db_->catalog_manager()->getIndexPtr(index_info.index_id, &actual_index_type);
        if (index_ptr == nullptr)
        {
            done_ = true;
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Index object not found in cache");
            return Status::NOT_FOUND;
        }

        current_tuple_ids_.clear();
        current_tuple_index_ = 0;
        current_key_ = key;

        std::vector<TID> candidate_tids;
        status = searchExactIndexCandidates(actual_index_type, index_ptr, key,
                                            engine_->getCurrentXid(),
                                            &candidate_tids, ctx);
        if (status == Status::NOT_FOUND)
        {
            done_ = true;
            initialized_ = true;
            return Status::OK;
        }
        else if (status != Status::OK)
        {
            done_ = true;
            return status;
        }

        status = engine_->filterIndexCandidatesByVisibleHeap(
            table_id_,
            index_info.column_ids,
            !index_info.is_expression_index && !index_info.is_partial_index &&
                supportsExactKeyLookup(actual_index_type),
            key, candidate_tids,
                                                             nullptr, &current_tuple_ids_, ctx);
        if (status != Status::OK)
        {
            done_ = true;
            return status;
        }

        initialized_ = true;
        done_ = current_tuple_ids_.empty();
        return Status::OK;
    }

    auto IndexScanIterator::next(Tuple *tuple_out, ErrorContext *ctx) -> Status
    {
        if (!initialized_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Must call seek() before next()");
            return Status::INVALID_ARGUMENT;
        }

        if (done_ || current_tuple_index_ >= current_tuple_ids_.size())
        {
            done_ = true;
            return Status::NOT_FOUND;
        }

        // Get the next tuple ID
        // PHASE 1.5: current_tuple_ids_ now contains TID structs
        TID tid = current_tuple_ids_[current_tuple_index_++];

        if (!isZeroId(table_id_) && db_ && db_->table_stats_manager())
        {
            db_->table_stats_manager()->recordIndexRowsFetch(table_id_, 1);
        }

        // Fill tuple_out with the location information
        if (tuple_out != nullptr)
        {
            tuple_out->tid = tid;
            tuple_out->data = nullptr; // Caller must fetch actual tuple data
            tuple_out->data_size = 0;
        }

        // Check if we've exhausted this key's tuples
        if (current_tuple_index_ >= current_tuple_ids_.size())
        {
            done_ = true;
        }

        return Status::OK;
    }

    auto StorageEngine::createIndexScan(const ID &index_id, ErrorContext *ctx)
        -> std::unique_ptr<IndexScanIterator>
    {
        ID table_id{};
        if (catalog_manager_)
        {
            CatalogManager::IndexInfo index_info;
            if (catalog_manager_->getIndex(index_id, index_info, ctx) == Status::OK)
            {
                table_id = index_info.table_id;
                if (db_ && db_->table_stats_manager())
                {
                    db_->table_stats_manager()->recordIndexScan(table_id);
                }
            }
        }

        return std::make_unique<IndexScanIterator>(db_, this, index_id, table_id);
    }

    // Lock management helpers

    auto StorageEngine::acquireTupleLock(const ID &table_id, uint32_t page_id, uint16_t item_id,
                                         uint32_t proc_id, bool wait, ErrorContext *ctx) -> Status
    {
        // Build lock tag for tuple
        LockTag tag{};
        tag.target_type = LockTarget::LOCK_TARGET_TUPLE;
        tag.object_uuid = table_id;
        tag.page_num = page_id;
        tag.offset_num = item_id;
        tag.padding = 0;

        // Acquire ROW_EXCLUSIVE lock (for UPDATE/DELETE)
        LockManager *lock_mgr = db_->lock_manager();
        if (lock_mgr == nullptr)
        {
            // No lock manager, skip locking (single-connection mode)
            return Status::OK;
        }

        ConnectionContext *conn_ctx = ConnectionContext::getCurrent();
        const bool read_consistency_active =
            conn_ctx != nullptr &&
            conn_ctx->getIsolationLevel() == IsolationLevel::READ_COMMITTED_READ_CONSISTENCY &&
            !conn_ctx->isForensicReplayActive();
        const bool effective_wait = read_consistency_active ? false : wait;

        uint32_t blocker_proc_id = 0;
        LockMode blocker_mode = LockMode::LOCK_ACCESS_SHARE;
        Status status = lock_mgr->acquireLock(proc_id, tag, LockMode::LOCK_ROW_EXCLUSIVE,
                                              effective_wait, 0, ctx,
                                              &blocker_proc_id, &blocker_mode);
        if (read_consistency_active &&
            (status == Status::LOCK_CONFLICT || status == Status::LOCK_TIMEOUT ||
             status == Status::DEADLOCK))
        {
            const auto decision = db_->transaction_manager()->evaluateReadConsistencyRestart(
                status,
                conn_ctx->getCurrentXid(),
                conn_ctx->statementTrackingActive(),
                conn_ctx->isForensicReplayActive(),
                conn_ctx->getStatementTransactionSnapshot(),
                tag,
                LockMode::LOCK_ROW_EXCLUSIVE,
                blocker_proc_id,
                blocker_mode);
            if (decision.restart_required)
            {
                incrementCanonicalCounter(
                    "sb_lock_read_consistency_restarts_total",
                    {metricDbLabel(db_),
                     TransactionManager::statementRestartReasonName(decision.reason)});
                incrementCanonicalCounter(
                    "sb_mga_statement_restarts_total",
                    {metricDbLabel(db_),
                     TransactionManager::statementRestartReasonName(decision.reason)});
                return conn_ctx->registerReadConsistencyRestart(decision, ctx);
            }
        }

        if (!read_consistency_active && !effective_wait && status == Status::LOCK_CONFLICT)
        {
            const std::string message =
                "UPDATE_CONFLICT_NO_WAIT: blocker_proc_id=" +
                std::to_string(blocker_proc_id) + " requested_mode=ROW_EXCLUSIVE blocker_mode=" +
                lockModeNameLocal(blocker_mode) + " table_id=" + table_id.toString() +
                " page_id=" + std::to_string(page_id) + " item_id=" + std::to_string(item_id);
            SET_ERROR_CONTEXT(ctx, Status::LOCK_NOT_AVAILABLE, message.c_str());
            return Status::LOCK_NOT_AVAILABLE;
        }

        return status;
    }

    auto StorageEngine::releaseTupleLock(const ID &table_id, uint32_t page_id, uint16_t item_id,
                                         uint32_t proc_id, ErrorContext *ctx) -> Status
    {
        // Build lock tag for tuple
        LockTag tag{};
        tag.target_type = LockTarget::LOCK_TARGET_TUPLE;
        tag.object_uuid = table_id;
        tag.page_num = page_id;
        tag.offset_num = item_id;
        tag.padding = 0;

        // Release ROW_EXCLUSIVE lock
        LockManager *lock_mgr = db_->lock_manager();
        if (lock_mgr == nullptr)
        {
            // No lock manager, nothing to release
            return Status::OK;
        }

        return lock_mgr->releaseLock(proc_id, tag, LockMode::LOCK_ROW_EXCLUSIVE, ctx);
    }

    // Helper function to extract indexed column values and build an index key
    // This is a simplified implementation that assumes basic column layout.
    // NOTE: This function is primarily used as a fallback path. The main index key
    // extraction uses IndexKeyExtractor::extractKey() which has full support for:
    // - Variable-length columns (VARCHAR, TEXT, BYTEA)
    // - NULL values and null bitmaps
    // - Complex data types (arrays, JSON, etc.)
    // - TOAST decompression for large values
    // This simplified version handles basic fixed-width columns for bootstrap/testing.
    static Status buildIndexKey(const uint8_t *tuple_data, uint32_t tuple_size,
                                const std::vector<CatalogManager::ColumnInfo> &all_columns,
                                const std::vector<ID> &indexed_column_ids,
                                std::vector<uint8_t> *key_out, ErrorContext *ctx)
    {
        // Skip tuple header to get to actual data
        if (tuple_size < sizeof(TupleHeader))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Tuple size too small");
            return Status::INVALID_ARGUMENT;
        }

        const uint8_t *data = tuple_data + sizeof(TupleHeader);
        uint32_t data_size = tuple_size - sizeof(TupleHeader);

        // Build a map of column_id to column_info for quick lookup
        std::unordered_map<ID, const CatalogManager::ColumnInfo *> column_map;
        for (const auto &col : all_columns)
        {
            column_map[col.column_id] = &col;
        }

        // For now, use a simplified approach: concatenate raw column values.
        // This assumes fixed-width columns in order. For full column extraction with
        // type-aware serialization, use IndexKeyExtractor::extractKey() which handles
        // all column types including variable-length and complex types.

        key_out->clear();

        // Simple approach: for single-column indexes, just use the first few bytes
        // For multi-column indexes, concatenate the values
        // This is a placeholder that works for simple integer keys
        if (data_size == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Empty tuple data");
            return Status::INVALID_ARGUMENT;
        }

        // For now, copy the raw data as the key (simplified)
        // In a real implementation, we would:
        // 1. Parse the tuple layout
        // 2. Extract each indexed column value by ordinal position
        // 3. Serialize them in order into the key
        key_out->assign(data, data + std::min(data_size, static_cast<uint32_t>(256)));

        return Status::OK;
    }

    // Helper function to update all indexes when a tuple relocates to a different page
    auto StorageEngine::updateIndexesForRelocation(const ID &table_id, uint32_t old_page_id,
                                                   uint16_t old_item_id, uint32_t new_page_id,
                                                   uint16_t new_item_id, const uint8_t *tuple_data,
                                                   uint32_t tuple_size, ErrorContext *ctx) -> Status
    {
        // Get all indexes for this table
        std::vector<CatalogManager::IndexInfo> indexes;
        Status status = catalog_manager_->listIndexesForTable(table_id, indexes, ctx, false);
        if (status != Status::OK)
        {
            // If no indexes or error, just warn and continue
            if (status == Status::NOT_FOUND)
            {
                // No indexes on this table - this is fine
                return Status::OK;
            }
            LOG_WARNING(STORAGE, "Failed to list indexes for table during cross-page update: %s",
                        ctx ? ctx->message.c_str() : "unknown error");
            return Status::OK; // Don't fail the update if we can't update indexes
        }

        if (indexes.empty())
        {
            // No indexes to update
            return Status::OK;
        }

        // Get column information for the table (needed to build index keys)
        std::vector<CatalogManager::ColumnInfo> columns;
        status = catalog_manager_->getColumns(table_id, columns, ctx);
        if (status != Status::OK)
        {
            LOG_WARNING(STORAGE, "Failed to get column info for table during index update");
            return Status::OK; // Don't fail the update
        }

        // Calculate old and new TIDs
        // PHASE 1.5: Use TID struct instead of uint64_t
        TID old_tid = TID(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(old_page_id)), old_item_id);
        TID new_tid = TID(makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(new_page_id)), new_item_id);

        // HIGH-8 FIX: Track failed index updates for corruption reporting
        std::vector<std::string> failed_indexes;
        bool had_critical_failure = false;

        // Update each index
        for (const auto &index_info : indexes)
        {
            uint32_t root_page = static_cast<uint32_t>(getPageNumber(index_info.root_gpid));
            // Build the index key from tuple data
            std::vector<uint8_t> key;
            status =
                buildIndexKey(tuple_data, tuple_size, columns, index_info.column_ids, &key, ctx);
            if (status != Status::OK)
            {
                LOG_WARNING(STORAGE,
                            "Failed to build index key for index %s during cross-page update",
                            index_info.index_name.c_str());
                failed_indexes.push_back(index_info.index_name + " (key build failed)");
                continue; // Skip this index, try others
            }

            // Update based on index type
            if (index_info.index_type == CatalogManager::IndexType::BTREE)
            {
                // Open the BTree index
                auto btree = BTree::open(db_, index_info.index_id, index_info.root_gpid, ctx);
                if (!btree)
                {
                    LOG_WARNING(STORAGE, "Failed to open BTree index %s for update",
                                index_info.index_name.c_str());
                    failed_indexes.push_back(index_info.index_name + " (open failed)");
                    continue;
                }

                // Remove old entry
                // Task 17 MGA Phase 3.1: Pass xid = 0 for system operations (tuple relocation)
                status = btree->remove(key, old_tid, 0, ctx);
                if (status != Status::OK && status != Status::NOT_FOUND)
                {
                    LOG_WARNING(STORAGE, "Failed to remove old entry from BTree index %s: %s",
                                index_info.index_name.c_str(),
                                ctx ? ctx->message.c_str() : "unknown error");
                    // Continue anyway - try to insert new entry
                }

                // Insert new entry
                // Task 17 MGA Phase 3.1: Pass xid = 0 for system operations (tuple relocation)
                status = btree->insert(key, new_tid, 0, ctx);
                if (status != Status::OK)
                {
                    LOG_ERROR(STORAGE, "Failed to insert new entry into BTree index %s: %s",
                              index_info.index_name.c_str(),
                              ctx ? ctx->message.c_str() : "unknown error");
                    // HIGH-8 FIX: Track this critical failure
                    failed_indexes.push_back(index_info.index_name + " (insert failed)");
                    had_critical_failure = true;
                }
            }
            else if (index_info.index_type == CatalogManager::IndexType::HASH)
            {
                // Open the Hash index
                auto hash_index =
                    HashIndex::open(db_, index_info.index_id, index_info.root_gpid, ctx);
                if (!hash_index)
                {
                    LOG_WARNING(STORAGE, "Failed to open Hash index %s for update",
                                index_info.index_name.c_str());
                    failed_indexes.push_back(index_info.index_name + " (open failed)");
                    continue;
                }

                // Remove old entry
                status = hash_index->remove(key.data(), key.size(), old_tid, 0, ctx);
                if (status != Status::OK && status != Status::NOT_FOUND)
                {
                    LOG_WARNING(STORAGE, "Failed to remove old entry from Hash index %s: %s",
                                index_info.index_name.c_str(),
                                ctx ? ctx->message.c_str() : "unknown error");
                    // Continue anyway - try to insert new entry
                }

                // Insert new entry
                status = hash_index->insert(key.data(), key.size(), new_tid, 0, ctx);
                if (status != Status::OK)
                {
                    LOG_ERROR(STORAGE, "Failed to insert new entry into Hash index %s: %s",
                              index_info.index_name.c_str(),
                              ctx ? ctx->message.c_str() : "unknown error");
                    // HIGH-8 FIX: Track this critical failure
                    failed_indexes.push_back(index_info.index_name + " (insert failed)");
                    had_critical_failure = true;
                }
            }
            else
            {
                // Unsupported index type
                LOG_WARNING(STORAGE,
                            "Unsupported index type %d for index %s during cross-page update",
                            static_cast<int>(index_info.index_type), index_info.index_name.c_str());
                failed_indexes.push_back(index_info.index_name + " (unsupported type)");
            }
        }

        // HIGH-8 FIX: Report index corruption if any updates failed
        if (!failed_indexes.empty())
        {
            // Build comprehensive error message
            std::string error_msg = "Index corruption detected - failed to update " +
                                    std::to_string(failed_indexes.size()) + " index(es): ";
            for (size_t i = 0; i < failed_indexes.size(); ++i)
            {
                if (i > 0)
                    error_msg += ", ";
                error_msg += failed_indexes[i];
            }
            error_msg += ". REINDEX recommended for affected indexes.";

            LOG_ERROR(STORAGE, "%s", error_msg.c_str());

            // Return error status if we had critical failures (insert failures)
            if (had_critical_failure)
            {
                SET_ERROR_CONTEXT(ctx, Status::INDEX_CORRUPTED, error_msg.c_str());
                return Status::INDEX_CORRUPTED;
            }
        }

        return Status::OK;
    }

    auto StorageEngine::getOrCreateToastManager(const ID &table_id, ErrorContext *ctx)
        -> ToastManager *
    {
        // Avoid recursive TOAST creation for TOAST tables.
        if (catalog_manager_)
        {
            CatalogManager::TableInfo table_info;
            ErrorContext lookup_ctx;
            if (catalog_manager_->getTable(table_id, table_info, &lookup_ctx) == Status::OK)
            {
                const std::string prefix = "sb_toast_";
                bool is_toast_table = table_info.table_type == CatalogManager::TableType::TOAST;
                if (!is_toast_table && table_info.table_name.size() >= prefix.size())
                {
                    is_toast_table = std::equal(prefix.begin(), prefix.end(),
                                                table_info.table_name.begin(),
                                                [](char a, char b)
                                                {
                                                    return std::tolower(static_cast<unsigned char>(a)) ==
                                                           std::tolower(static_cast<unsigned char>(b));
                                                });
                }
                if (is_toast_table)
                {
                    return nullptr;
                }
            }
        }

        // Check if we already have a ToastManager for this table
        {
            std::lock_guard<std::mutex> lock(toast_mutex_);
            auto it = toast_managers_.find(table_id);
            if (it != toast_managers_.end())
            {
                return it->second.get();
            }
        }

        // Create new ToastManager (outside the lock to avoid holding it during initialization)
        auto toast_mgr = std::make_unique<ToastManager>(db_, table_id);

        // Initialize the ToastManager
        Status status = toast_mgr->initialize(ctx);
        if (status != Status::OK)
        {
            // Initialization failed - return nullptr
            // This can happen if TOAST table doesn't exist yet
            // In production, we might want to create it automatically
            return nullptr;
        }

        // Store it in the map
        ToastManager *result = toast_mgr.get();
        {
            std::lock_guard<std::mutex> lock(toast_mutex_);
            // Check again in case another thread created it
            auto it = toast_managers_.find(table_id);
            if (it != toast_managers_.end())
            {
                return it->second.get();
            }
            toast_managers_[table_id] = std::move(toast_mgr);
        }

        return result;
    }

} // namespace scratchbird::core
