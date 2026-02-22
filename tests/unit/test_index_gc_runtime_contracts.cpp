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
#include <type_traits>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/index_factory.h"
#include "scratchbird/core/index_gc_interface.h"
#include "scratchbird/core/btree.h"
#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/lsm_tree_index.h"
#include "scratchbird/core/gin_index.h"
#include "scratchbird/core/gist_index.h"
#include "scratchbird/core/brin_index.h"
#include "scratchbird/core/rtree.h"
#include "scratchbird/core/spgist_index.h"
#include "scratchbird/core/bitmap_index.h"
#include "scratchbird/core/columnstore.h"
#include "scratchbird/core/hnsw_index.h"
#include "scratchbird/core/inverted_index.h"

namespace {

using IndexType = scratchbird::core::CatalogManager::IndexType;
using RuntimeClass = scratchbird::core::IndexFactory::IndexRuntimeClass;
using Cap = scratchbird::core::IndexFactory::IndexFamilyCapabilities;

bool runtimeClassHasGcContract(RuntimeClass runtime_class)
{
    switch (runtime_class)
    {
        case RuntimeClass::BTREE:
        case RuntimeClass::HASH:
        case RuntimeClass::LSM:
        case RuntimeClass::GIN:
        case RuntimeClass::GIST:
        case RuntimeClass::BRIN:
        case RuntimeClass::RTREE:
        case RuntimeClass::SPGIST:
        case RuntimeClass::BITMAP:
        case RuntimeClass::COLUMNSTORE:
        case RuntimeClass::HNSW:
        case RuntimeClass::INVERTED:
            return true;
        default:
            return false;
    }
}

} // namespace

TEST(IndexGcRuntimeContractsTest, RuntimeBackendsImplementIndexGcInterface)
{
    static_assert(std::is_base_of_v<scratchbird::core::IndexGCInterface, scratchbird::core::BTree>);
    static_assert(std::is_base_of_v<scratchbird::core::IndexGCInterface, scratchbird::core::HashIndex>);
    static_assert(std::is_base_of_v<scratchbird::core::IndexGCInterface, scratchbird::core::LSMTreeIndex>);
    static_assert(std::is_base_of_v<scratchbird::core::IndexGCInterface, scratchbird::core::GinIndex>);
    static_assert(std::is_base_of_v<scratchbird::core::IndexGCInterface, scratchbird::core::GiSTIndex>);
    static_assert(std::is_base_of_v<scratchbird::core::IndexGCInterface, scratchbird::core::BrinIndex>);
    static_assert(std::is_base_of_v<scratchbird::core::IndexGCInterface, scratchbird::core::RTree>);
    static_assert(std::is_base_of_v<scratchbird::core::IndexGCInterface, scratchbird::core::SPGiSTIndex>);
    static_assert(std::is_base_of_v<scratchbird::core::IndexGCInterface, scratchbird::core::BitmapIndex>);
    static_assert(std::is_base_of_v<scratchbird::core::IndexGCInterface, scratchbird::core::ColumnstoreIndex>);
    static_assert(std::is_base_of_v<scratchbird::core::IndexGCInterface, scratchbird::core::HnswIndex>);
    static_assert(std::is_base_of_v<scratchbird::core::IndexGCInterface, scratchbird::core::InvertedIndex>);

    SUCCEED();
}

TEST(IndexGcRuntimeContractsTest, CapabilityMatrixOnlyUsesGcCapableRuntimeClasses)
{
    const std::vector<Cap> matrix = scratchbird::core::IndexFactory::listCapabilities();
    ASSERT_FALSE(matrix.empty());

    for (const auto& caps : matrix)
    {
        EXPECT_TRUE(runtimeClassHasGcContract(caps.runtime_class))
            << "index_type=" << static_cast<int>(caps.index_type)
            << " runtime_class=" << static_cast<int>(caps.runtime_class);
    }
}

TEST(IndexGcRuntimeContractsTest, InvertedFamilyMembershipIsDeterministic)
{
    const std::set<IndexType> expected_inverted = {
        IndexType::FULLTEXT,
        IndexType::MINHASH_LSH,
        IndexType::SPARSE_INVERTED,
        IndexType::SPARSE_WAND,
        IndexType::TRIE,
        IndexType::INVERTED,
        IndexType::NGRAM,
        IndexType::MONGODB_WILDCARD,
        IndexType::MONGODB_ENCRYPTED_RANGE,
        IndexType::NEO4J_TEXT,
        IndexType::CASSANDRA_SASI,
        IndexType::CASSANDRA_SAI
    };

    std::set<IndexType> actual_inverted;
    const std::vector<Cap> matrix = scratchbird::core::IndexFactory::listCapabilities();
    for (const auto& caps : matrix)
    {
        if (caps.runtime_class == RuntimeClass::INVERTED)
        {
            actual_inverted.insert(caps.index_type);
        }
    }

    EXPECT_EQ(actual_inverted, expected_inverted);
}
