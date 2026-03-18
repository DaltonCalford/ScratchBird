/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/index_factory.h"
#include "scratchbird/core/status.h"

namespace {

using IndexType = scratchbird::core::CatalogManager::IndexType;
using Cap = scratchbird::core::IndexFactory::IndexFamilyCapabilities;
using Runtime = scratchbird::core::IndexFactory::IndexRuntimeClass;
using Storage = scratchbird::core::IndexFactory::IndexStorageModel;
using Status = scratchbird::core::Status;
using ErrorContext = scratchbird::core::ErrorContext;

const Cap* findCaps(const std::vector<Cap>& matrix, IndexType type)
{
    for (const auto& caps : matrix)
    {
        if (caps.index_type == type)
        {
            return &caps;
        }
    }
    return nullptr;
}

scratchbird::core::ID makeTestId()
{
    scratchbird::core::ID id{};
    for (size_t i = 0; i < id.bytes.size(); ++i)
    {
        id.bytes[i] = static_cast<uint8_t>(i + 1);
    }
    return id;
}

} // namespace

TEST(IndexFactoryRegistryTest, CapabilityMatrixContainsExplicitFamilies)
{
    auto matrix = scratchbird::core::IndexFactory::listCapabilities();
    ASSERT_EQ(matrix.size(), 59U);

    std::set<uint8_t> seen;
    const std::set<uint8_t> create_unsupported = {
        static_cast<uint8_t>(IndexType::ANNOY),
        static_cast<uint8_t>(IndexType::SCANN),
        static_cast<uint8_t>(IndexType::DISKANN),
        static_cast<uint8_t>(IndexType::GPU_CAGRA),
    };
    for (const auto& caps : matrix)
    {
        const uint8_t index_type_value = static_cast<uint8_t>(caps.index_type);
        EXPECT_TRUE(seen.insert(index_type_value).second);
        EXPECT_NE(caps.canonical_name, nullptr);
        EXPECT_FALSE(std::string(caps.canonical_name).empty());
        if (create_unsupported.count(index_type_value) != 0)
        {
            EXPECT_FALSE(caps.supports_create);
        }
        else
        {
            EXPECT_TRUE(caps.supports_create);
        }
        EXPECT_TRUE(caps.supports_open);
        EXPECT_TRUE(caps.supports_close);
    }
}

TEST(IndexFactoryRegistryTest, ExplicitBackendSharingIsDeclaredInRegistry)
{
    auto matrix = scratchbird::core::IndexFactory::listCapabilities();

    const Cap* hnsw = findCaps(matrix, IndexType::HNSW);
    ASSERT_NE(hnsw, nullptr);
    EXPECT_EQ(hnsw->runtime_class, Runtime::HNSW);
    EXPECT_TRUE(hnsw->requires_vector_dimensions);

    const Cap* ivf = findCaps(matrix, IndexType::IVF);
    ASSERT_NE(ivf, nullptr);
    EXPECT_EQ(ivf->runtime_class, Runtime::HNSW);
    EXPECT_TRUE(ivf->requires_vector_dimensions);

    const Cap* ivf_sq8_hybrid = findCaps(matrix, IndexType::IVF_SQ8_HYBRID);
    ASSERT_NE(ivf_sq8_hybrid, nullptr);
    EXPECT_EQ(ivf_sq8_hybrid->runtime_class, Runtime::HNSW);
    EXPECT_TRUE(ivf_sq8_hybrid->requires_vector_dimensions);

    const Cap* rhnsw_pq = findCaps(matrix, IndexType::RHNSW_PQ);
    ASSERT_NE(rhnsw_pq, nullptr);
    EXPECT_EQ(rhnsw_pq->runtime_class, Runtime::HNSW);
    EXPECT_TRUE(rhnsw_pq->requires_vector_dimensions);

    const Cap* annoy = findCaps(matrix, IndexType::ANNOY);
    ASSERT_NE(annoy, nullptr);
    EXPECT_EQ(annoy->runtime_class, Runtime::HNSW);
    EXPECT_TRUE(annoy->requires_vector_dimensions);

    const Cap* diskann = findCaps(matrix, IndexType::DISKANN);
    ASSERT_NE(diskann, nullptr);
    EXPECT_EQ(diskann->runtime_class, Runtime::HNSW);
    EXPECT_TRUE(diskann->requires_vector_dimensions);

    const Cap* gpu_cagra = findCaps(matrix, IndexType::GPU_CAGRA);
    ASSERT_NE(gpu_cagra, nullptr);
    EXPECT_EQ(gpu_cagra->runtime_class, Runtime::HNSW);
    EXPECT_TRUE(gpu_cagra->requires_vector_dimensions);

    const Cap* art = findCaps(matrix, IndexType::ART);
    ASSERT_NE(art, nullptr);
    EXPECT_EQ(art->runtime_class, Runtime::BTREE);

    const Cap* bloom = findCaps(matrix, IndexType::BLOOM);
    ASSERT_NE(bloom, nullptr);
    EXPECT_EQ(bloom->runtime_class, Runtime::BRIN);

    const Cap* trie = findCaps(matrix, IndexType::TRIE);
    ASSERT_NE(trie, nullptr);
    EXPECT_EQ(trie->runtime_class, Runtime::INVERTED);

    const Cap* inverted = findCaps(matrix, IndexType::INVERTED);
    ASSERT_NE(inverted, nullptr);
    EXPECT_EQ(inverted->runtime_class, Runtime::INVERTED);

    const Cap* stl_sort = findCaps(matrix, IndexType::STL_SORT);
    ASSERT_NE(stl_sort, nullptr);
    EXPECT_EQ(stl_sort->runtime_class, Runtime::BTREE);

    const Cap* sparse_wand = findCaps(matrix, IndexType::SPARSE_WAND);
    ASSERT_NE(sparse_wand, nullptr);
    EXPECT_EQ(sparse_wand->runtime_class, Runtime::INVERTED);

    const Cap* brin = findCaps(matrix, IndexType::BRIN);
    ASSERT_NE(brin, nullptr);
    EXPECT_EQ(brin->runtime_class, Runtime::BRIN);
    EXPECT_TRUE(brin->requires_column_datatype);

    const Cap* zonemap = findCaps(matrix, IndexType::ZONEMAP);
    ASSERT_NE(zonemap, nullptr);
    EXPECT_EQ(zonemap->runtime_class, Runtime::BRIN);
    EXPECT_TRUE(zonemap->requires_column_datatype);

    const Cap* mongo_2d = findCaps(matrix, IndexType::MONGODB_2D);
    ASSERT_NE(mongo_2d, nullptr);
    EXPECT_EQ(mongo_2d->runtime_class, Runtime::RTREE);

    const Cap* mongo_2dsphere = findCaps(matrix, IndexType::MONGODB_2DSPHERE);
    ASSERT_NE(mongo_2dsphere, nullptr);
    EXPECT_EQ(mongo_2dsphere->runtime_class, Runtime::RTREE);

    const Cap* mongo_geo_haystack = findCaps(matrix, IndexType::MONGODB_GEO_HAYSTACK);
    ASSERT_NE(mongo_geo_haystack, nullptr);
    EXPECT_EQ(mongo_geo_haystack->runtime_class, Runtime::BTREE);

    const Cap* mongo_wildcard = findCaps(matrix, IndexType::MONGODB_WILDCARD);
    ASSERT_NE(mongo_wildcard, nullptr);
    EXPECT_EQ(mongo_wildcard->runtime_class, Runtime::INVERTED);

    const Cap* mongo_encrypted = findCaps(matrix, IndexType::MONGODB_ENCRYPTED_RANGE);
    ASSERT_NE(mongo_encrypted, nullptr);
    EXPECT_EQ(mongo_encrypted->runtime_class, Runtime::INVERTED);

    const Cap* neo4j_lookup = findCaps(matrix, IndexType::NEO4J_LOOKUP);
    ASSERT_NE(neo4j_lookup, nullptr);
    EXPECT_EQ(neo4j_lookup->runtime_class, Runtime::BITMAP);

    const Cap* neo4j_text = findCaps(matrix, IndexType::NEO4J_TEXT);
    ASSERT_NE(neo4j_text, nullptr);
    EXPECT_EQ(neo4j_text->runtime_class, Runtime::INVERTED);

    const Cap* neo4j_range = findCaps(matrix, IndexType::NEO4J_RANGE);
    ASSERT_NE(neo4j_range, nullptr);
    EXPECT_EQ(neo4j_range->runtime_class, Runtime::BTREE);

    const Cap* neo4j_vector = findCaps(matrix, IndexType::NEO4J_VECTOR);
    ASSERT_NE(neo4j_vector, nullptr);
    EXPECT_EQ(neo4j_vector->runtime_class, Runtime::HNSW);
    EXPECT_TRUE(neo4j_vector->requires_vector_dimensions);

    const Cap* cassandra_sasi = findCaps(matrix, IndexType::CASSANDRA_SASI);
    ASSERT_NE(cassandra_sasi, nullptr);
    EXPECT_EQ(cassandra_sasi->runtime_class, Runtime::INVERTED);

    const Cap* cassandra_sai = findCaps(matrix, IndexType::CASSANDRA_SAI);
    ASSERT_NE(cassandra_sai, nullptr);
    EXPECT_EQ(cassandra_sai->runtime_class, Runtime::INVERTED);

    const Cap* redis_hash = findCaps(matrix, IndexType::REDIS_HASH);
    ASSERT_NE(redis_hash, nullptr);
    EXPECT_EQ(redis_hash->runtime_class, Runtime::HASH);

    const Cap* redis_list = findCaps(matrix, IndexType::REDIS_LIST);
    ASSERT_NE(redis_list, nullptr);
    EXPECT_EQ(redis_list->runtime_class, Runtime::BTREE);

    const Cap* redis_bitmap = findCaps(matrix, IndexType::REDIS_BITMAP);
    ASSERT_NE(redis_bitmap, nullptr);
    EXPECT_EQ(redis_bitmap->runtime_class, Runtime::BITMAP);

    const Cap* redis_geo = findCaps(matrix, IndexType::REDIS_GEO);
    ASSERT_NE(redis_geo, nullptr);
    EXPECT_EQ(redis_geo->runtime_class, Runtime::RTREE);
}

TEST(IndexFactoryRegistryTest, StorageModelIsDeterministicFromCapabilityMatrix)
{
    auto matrix = scratchbird::core::IndexFactory::listCapabilities();

    const Cap* lsm = findCaps(matrix, IndexType::LSM);
    ASSERT_NE(lsm, nullptr);
    EXPECT_EQ(lsm->storage_model, Storage::FILE_BASED);
    EXPECT_TRUE(lsm->requires_primary_tablespace);

    const Cap* columnstore = findCaps(matrix, IndexType::COLUMNSTORE);
    ASSERT_NE(columnstore, nullptr);
    EXPECT_EQ(columnstore->storage_model, Storage::PAGE_BASED);
}

TEST(IndexFactoryRegistryTest, LookupRejectsUnregisteredType)
{
    const Cap* unknown =
        scratchbird::core::IndexFactory::lookupCapabilities(static_cast<IndexType>(254));
    EXPECT_EQ(unknown, nullptr);
}

TEST(IndexFactoryRegistryTest, CloseRejectsUnregisteredTypeDeterministically)
{
    int dummy = 0;
    ErrorContext ctx;
    Status status = scratchbird::core::IndexFactory::closeIndex(
        static_cast<IndexType>(254), &dummy, &ctx);
    EXPECT_EQ(status, Status::INVALID_ARGUMENT);
    EXPECT_NE(ctx.message.find("type not registered"), std::string::npos) << ctx.message;
}

TEST(IndexFactoryRegistryTest, GenerateIndexPathUsesStorageModelContracts)
{
    scratchbird::core::ID id = makeTestId();

    std::string lsm_path =
        scratchbird::core::IndexFactory::generateIndexPath("/tmp/test_db.sbrd", id, IndexType::LSM);
    EXPECT_NE(lsm_path.find("/indexes/idx_"), std::string::npos) << lsm_path;

    EXPECT_EQ(
        scratchbird::core::IndexFactory::generateIndexPath("/tmp/test_db.sbrd", id, IndexType::BTREE),
        "");
    EXPECT_EQ(
        scratchbird::core::IndexFactory::generateIndexPath("/tmp/test_db.sbrd", id, IndexType::COLUMNSTORE),
        "");
}
