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

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/parser/ast_v3.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/opcodes.h"

namespace {

using ParserIndexType = scratchbird::parser::v3::IndexType;
using CatalogIndexType = scratchbird::core::CatalogManager::IndexType;
using SblrIndexType = scratchbird::sblr::IndexType;

}  // namespace

TEST(IndexTypeTaxonomyTest, ParserAndCatalogEnumsAreAligned)
{
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::BTREE), static_cast<uint8_t>(CatalogIndexType::BTREE));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::HASH), static_cast<uint8_t>(CatalogIndexType::HASH));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::HNSW), static_cast<uint8_t>(CatalogIndexType::HNSW));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::FULLTEXT), static_cast<uint8_t>(CatalogIndexType::FULLTEXT));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::GIN), static_cast<uint8_t>(CatalogIndexType::GIN));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::GIST), static_cast<uint8_t>(CatalogIndexType::GIST));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::BRIN), static_cast<uint8_t>(CatalogIndexType::BRIN));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::RTREE), static_cast<uint8_t>(CatalogIndexType::RTREE));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::SPGIST), static_cast<uint8_t>(CatalogIndexType::SPGIST));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::BITMAP), static_cast<uint8_t>(CatalogIndexType::BITMAP));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::COLUMNSTORE),
              static_cast<uint8_t>(CatalogIndexType::COLUMNSTORE));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::LSM), static_cast<uint8_t>(CatalogIndexType::LSM));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::IVF), static_cast<uint8_t>(CatalogIndexType::IVF));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::ZONEMAP), static_cast<uint8_t>(CatalogIndexType::ZONEMAP));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::ART), static_cast<uint8_t>(CatalogIndexType::ART));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::BLOOM), static_cast<uint8_t>(CatalogIndexType::BLOOM));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::VECTOR_FLAT),
              static_cast<uint8_t>(CatalogIndexType::VECTOR_FLAT));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::VECTOR_BIN_FLAT),
              static_cast<uint8_t>(CatalogIndexType::VECTOR_BIN_FLAT));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::IVF_FLAT), static_cast<uint8_t>(CatalogIndexType::IVF_FLAT));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::BIN_IVF_FLAT),
              static_cast<uint8_t>(CatalogIndexType::BIN_IVF_FLAT));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::IVF_PQ), static_cast<uint8_t>(CatalogIndexType::IVF_PQ));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::IVF_SQ8), static_cast<uint8_t>(CatalogIndexType::IVF_SQ8));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::IVF_SQ8_HYBRID),
              static_cast<uint8_t>(CatalogIndexType::IVF_SQ8_HYBRID));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::RHNSW_PQ),
              static_cast<uint8_t>(CatalogIndexType::RHNSW_PQ));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::RHNSW_SQ),
              static_cast<uint8_t>(CatalogIndexType::RHNSW_SQ));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::ANNOY),
              static_cast<uint8_t>(CatalogIndexType::ANNOY));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::NSG),
              static_cast<uint8_t>(CatalogIndexType::NSG));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::DISKANN),
              static_cast<uint8_t>(CatalogIndexType::DISKANN));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::SCANN),
              static_cast<uint8_t>(CatalogIndexType::SCANN));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::GPU_CAGRA),
              static_cast<uint8_t>(CatalogIndexType::GPU_CAGRA));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::MINHASH_LSH),
              static_cast<uint8_t>(CatalogIndexType::MINHASH_LSH));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::SPARSE_INVERTED),
              static_cast<uint8_t>(CatalogIndexType::SPARSE_INVERTED));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::SPARSE_WAND),
              static_cast<uint8_t>(CatalogIndexType::SPARSE_WAND));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::TRIE),
              static_cast<uint8_t>(CatalogIndexType::TRIE));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::NGRAM),
              static_cast<uint8_t>(CatalogIndexType::NGRAM));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::MONGODB_2D),
              static_cast<uint8_t>(CatalogIndexType::MONGODB_2D));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::MONGODB_2DSPHERE),
              static_cast<uint8_t>(CatalogIndexType::MONGODB_2DSPHERE));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::MONGODB_2DSPHERE_BUCKET),
              static_cast<uint8_t>(CatalogIndexType::MONGODB_2DSPHERE_BUCKET));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::MONGODB_GEO_HAYSTACK),
              static_cast<uint8_t>(CatalogIndexType::MONGODB_GEO_HAYSTACK));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::MONGODB_WILDCARD),
              static_cast<uint8_t>(CatalogIndexType::MONGODB_WILDCARD));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::MONGODB_ENCRYPTED_RANGE),
              static_cast<uint8_t>(CatalogIndexType::MONGODB_ENCRYPTED_RANGE));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::NEO4J_LOOKUP),
              static_cast<uint8_t>(CatalogIndexType::NEO4J_LOOKUP));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::NEO4J_TEXT),
              static_cast<uint8_t>(CatalogIndexType::NEO4J_TEXT));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::NEO4J_RANGE),
              static_cast<uint8_t>(CatalogIndexType::NEO4J_RANGE));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::NEO4J_POINT),
              static_cast<uint8_t>(CatalogIndexType::NEO4J_POINT));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::NEO4J_VECTOR),
              static_cast<uint8_t>(CatalogIndexType::NEO4J_VECTOR));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::CASSANDRA_SASI),
              static_cast<uint8_t>(CatalogIndexType::CASSANDRA_SASI));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::CASSANDRA_SAI),
              static_cast<uint8_t>(CatalogIndexType::CASSANDRA_SAI));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::REDIS_STRING),
              static_cast<uint8_t>(CatalogIndexType::REDIS_STRING));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::REDIS_HASH),
              static_cast<uint8_t>(CatalogIndexType::REDIS_HASH));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::REDIS_LIST),
              static_cast<uint8_t>(CatalogIndexType::REDIS_LIST));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::REDIS_SET),
              static_cast<uint8_t>(CatalogIndexType::REDIS_SET));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::REDIS_ZSET),
              static_cast<uint8_t>(CatalogIndexType::REDIS_ZSET));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::REDIS_STREAM),
              static_cast<uint8_t>(CatalogIndexType::REDIS_STREAM));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::REDIS_BITMAP),
              static_cast<uint8_t>(CatalogIndexType::REDIS_BITMAP));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::REDIS_HLL),
              static_cast<uint8_t>(CatalogIndexType::REDIS_HLL));
    EXPECT_EQ(static_cast<uint8_t>(ParserIndexType::REDIS_GEO),
              static_cast<uint8_t>(CatalogIndexType::REDIS_GEO));
}

TEST(IndexTypeTaxonomyTest, SblrAndCatalogEnumsAreAligned)
{
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::BTREE), static_cast<uint8_t>(CatalogIndexType::BTREE));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::HASH), static_cast<uint8_t>(CatalogIndexType::HASH));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::HNSW), static_cast<uint8_t>(CatalogIndexType::HNSW));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::FULLTEXT), static_cast<uint8_t>(CatalogIndexType::FULLTEXT));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::GIN), static_cast<uint8_t>(CatalogIndexType::GIN));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::GIST), static_cast<uint8_t>(CatalogIndexType::GIST));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::BRIN), static_cast<uint8_t>(CatalogIndexType::BRIN));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::RTREE), static_cast<uint8_t>(CatalogIndexType::RTREE));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::SPGIST), static_cast<uint8_t>(CatalogIndexType::SPGIST));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::BITMAP), static_cast<uint8_t>(CatalogIndexType::BITMAP));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::COLUMNSTORE), static_cast<uint8_t>(CatalogIndexType::COLUMNSTORE));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::LSM), static_cast<uint8_t>(CatalogIndexType::LSM));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::IVF), static_cast<uint8_t>(CatalogIndexType::IVF));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::ZONEMAP), static_cast<uint8_t>(CatalogIndexType::ZONEMAP));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::ART), static_cast<uint8_t>(CatalogIndexType::ART));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::BLOOM), static_cast<uint8_t>(CatalogIndexType::BLOOM));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::VECTOR_FLAT),
              static_cast<uint8_t>(CatalogIndexType::VECTOR_FLAT));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::VECTOR_BIN_FLAT),
              static_cast<uint8_t>(CatalogIndexType::VECTOR_BIN_FLAT));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::IVF_FLAT), static_cast<uint8_t>(CatalogIndexType::IVF_FLAT));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::BIN_IVF_FLAT),
              static_cast<uint8_t>(CatalogIndexType::BIN_IVF_FLAT));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::IVF_PQ), static_cast<uint8_t>(CatalogIndexType::IVF_PQ));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::IVF_SQ8), static_cast<uint8_t>(CatalogIndexType::IVF_SQ8));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::IVF_SQ8_HYBRID),
              static_cast<uint8_t>(CatalogIndexType::IVF_SQ8_HYBRID));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::RHNSW_PQ),
              static_cast<uint8_t>(CatalogIndexType::RHNSW_PQ));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::RHNSW_SQ),
              static_cast<uint8_t>(CatalogIndexType::RHNSW_SQ));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::ANNOY),
              static_cast<uint8_t>(CatalogIndexType::ANNOY));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::NSG),
              static_cast<uint8_t>(CatalogIndexType::NSG));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::DISKANN),
              static_cast<uint8_t>(CatalogIndexType::DISKANN));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::SCANN),
              static_cast<uint8_t>(CatalogIndexType::SCANN));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::GPU_CAGRA),
              static_cast<uint8_t>(CatalogIndexType::GPU_CAGRA));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::MINHASH_LSH),
              static_cast<uint8_t>(CatalogIndexType::MINHASH_LSH));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::SPARSE_INVERTED),
              static_cast<uint8_t>(CatalogIndexType::SPARSE_INVERTED));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::SPARSE_WAND),
              static_cast<uint8_t>(CatalogIndexType::SPARSE_WAND));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::TRIE),
              static_cast<uint8_t>(CatalogIndexType::TRIE));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::NGRAM),
              static_cast<uint8_t>(CatalogIndexType::NGRAM));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::MONGODB_2D),
              static_cast<uint8_t>(CatalogIndexType::MONGODB_2D));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::MONGODB_2DSPHERE),
              static_cast<uint8_t>(CatalogIndexType::MONGODB_2DSPHERE));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::MONGODB_2DSPHERE_BUCKET),
              static_cast<uint8_t>(CatalogIndexType::MONGODB_2DSPHERE_BUCKET));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::MONGODB_GEO_HAYSTACK),
              static_cast<uint8_t>(CatalogIndexType::MONGODB_GEO_HAYSTACK));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::MONGODB_WILDCARD),
              static_cast<uint8_t>(CatalogIndexType::MONGODB_WILDCARD));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::MONGODB_ENCRYPTED_RANGE),
              static_cast<uint8_t>(CatalogIndexType::MONGODB_ENCRYPTED_RANGE));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::NEO4J_LOOKUP),
              static_cast<uint8_t>(CatalogIndexType::NEO4J_LOOKUP));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::NEO4J_TEXT),
              static_cast<uint8_t>(CatalogIndexType::NEO4J_TEXT));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::NEO4J_RANGE),
              static_cast<uint8_t>(CatalogIndexType::NEO4J_RANGE));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::NEO4J_POINT),
              static_cast<uint8_t>(CatalogIndexType::NEO4J_POINT));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::NEO4J_VECTOR),
              static_cast<uint8_t>(CatalogIndexType::NEO4J_VECTOR));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::CASSANDRA_SASI),
              static_cast<uint8_t>(CatalogIndexType::CASSANDRA_SASI));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::CASSANDRA_SAI),
              static_cast<uint8_t>(CatalogIndexType::CASSANDRA_SAI));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::REDIS_STRING),
              static_cast<uint8_t>(CatalogIndexType::REDIS_STRING));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::REDIS_HASH),
              static_cast<uint8_t>(CatalogIndexType::REDIS_HASH));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::REDIS_LIST),
              static_cast<uint8_t>(CatalogIndexType::REDIS_LIST));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::REDIS_SET),
              static_cast<uint8_t>(CatalogIndexType::REDIS_SET));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::REDIS_ZSET),
              static_cast<uint8_t>(CatalogIndexType::REDIS_ZSET));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::REDIS_STREAM),
              static_cast<uint8_t>(CatalogIndexType::REDIS_STREAM));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::REDIS_BITMAP),
              static_cast<uint8_t>(CatalogIndexType::REDIS_BITMAP));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::REDIS_HLL),
              static_cast<uint8_t>(CatalogIndexType::REDIS_HLL));
    EXPECT_EQ(static_cast<uint8_t>(SblrIndexType::REDIS_GEO),
              static_cast<uint8_t>(CatalogIndexType::REDIS_GEO));
}

TEST(IndexTypeTaxonomyTest, CanonicalMappingRejectsUnknownTypes)
{
    auto empty = scratchbird::sblr::Executor::mapCanonicalIndexType("");
    ASSERT_TRUE(empty.has_value());
    EXPECT_EQ(*empty, CatalogIndexType::BTREE);

    auto hash = scratchbird::sblr::Executor::mapCanonicalIndexType("hash");
    ASSERT_TRUE(hash.has_value());
    EXPECT_EQ(*hash, CatalogIndexType::HASH);

    auto spgist_alias = scratchbird::sblr::Executor::mapCanonicalIndexType("sp-gist");
    ASSERT_TRUE(spgist_alias.has_value());
    EXPECT_EQ(*spgist_alias, CatalogIndexType::SPGIST);

    auto zonemap_alias = scratchbird::sblr::Executor::mapCanonicalIndexType("zone_map");
    ASSERT_TRUE(zonemap_alias.has_value());
    EXPECT_EQ(*zonemap_alias, CatalogIndexType::ZONEMAP);

    auto vector_alias = scratchbird::sblr::Executor::mapCanonicalIndexType("vector");
    ASSERT_TRUE(vector_alias.has_value());
    EXPECT_EQ(*vector_alias, CatalogIndexType::HNSW);

    auto ivf_sq8_hybrid = scratchbird::sblr::Executor::mapCanonicalIndexType("ivf_sq8_hybrid");
    ASSERT_TRUE(ivf_sq8_hybrid.has_value());
    EXPECT_EQ(*ivf_sq8_hybrid, CatalogIndexType::IVF_SQ8_HYBRID);

    auto rhnsw_pq = scratchbird::sblr::Executor::mapCanonicalIndexType("rhnsw_pq");
    ASSERT_TRUE(rhnsw_pq.has_value());
    EXPECT_EQ(*rhnsw_pq, CatalogIndexType::RHNSW_PQ);

    auto annoy = scratchbird::sblr::Executor::mapCanonicalIndexType("annoy");
    ASSERT_TRUE(annoy.has_value());
    EXPECT_EQ(*annoy, CatalogIndexType::ANNOY);

    auto gpu_cagra = scratchbird::sblr::Executor::mapCanonicalIndexType("gpu_cagra");
    ASSERT_TRUE(gpu_cagra.has_value());
    EXPECT_EQ(*gpu_cagra, CatalogIndexType::GPU_CAGRA);

    auto minhash = scratchbird::sblr::Executor::mapCanonicalIndexType("minhash_lsh");
    ASSERT_TRUE(minhash.has_value());
    EXPECT_EQ(*minhash, CatalogIndexType::MINHASH_LSH);

    auto sparse_wand = scratchbird::sblr::Executor::mapCanonicalIndexType("sparse_wand");
    ASSERT_TRUE(sparse_wand.has_value());
    EXPECT_EQ(*sparse_wand, CatalogIndexType::SPARSE_WAND);

    auto mongo_2d = scratchbird::sblr::Executor::mapCanonicalIndexType("mongodb_2d");
    ASSERT_TRUE(mongo_2d.has_value());
    EXPECT_EQ(*mongo_2d, CatalogIndexType::MONGODB_2D);

    auto neo4j_lookup = scratchbird::sblr::Executor::mapCanonicalIndexType("neo4j_lookup");
    ASSERT_TRUE(neo4j_lookup.has_value());
    EXPECT_EQ(*neo4j_lookup, CatalogIndexType::NEO4J_LOOKUP);

    auto cassandra_sai = scratchbird::sblr::Executor::mapCanonicalIndexType("cassandra_sai");
    ASSERT_TRUE(cassandra_sai.has_value());
    EXPECT_EQ(*cassandra_sai, CatalogIndexType::CASSANDRA_SAI);

    auto redis_hash = scratchbird::sblr::Executor::mapCanonicalIndexType("redis_hash");
    ASSERT_TRUE(redis_hash.has_value());
    EXPECT_EQ(*redis_hash, CatalogIndexType::REDIS_HASH);

    auto unknown = scratchbird::sblr::Executor::mapCanonicalIndexType("definitely_not_supported");
    EXPECT_FALSE(unknown.has_value());
}
