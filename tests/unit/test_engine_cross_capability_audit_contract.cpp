/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/engine_profile_contract.h"
#include "scratchbird/core/index_factory.h"
#include "scratchbird/core/types.h"

namespace
{
    using scratchbird::core::CatalogManager;
    using scratchbird::core::DataType;
    using scratchbird::core::EmulatedStorageKind;
    using scratchbird::core::EmulatedTypeMapping;
    using scratchbird::core::IndexFactory;
    using scratchbird::core::TypeSystem;
    using scratchbird::core::indexTypeToString;
    using scratchbird::core::parseIndexType;
    using scratchbird::core::profile_contract::canonicalEmulationEngineSet;
    using scratchbird::core::profile_contract::emulationEngineProfileId;

    struct TypeProbe
    {
        const char* emulated_type;
        DataType canonical_type;
        EmulatedStorageKind storage_kind;
    };

    struct EngineTypeProbe
    {
        CatalogManager::EmulationEngine engine;
        std::vector<TypeProbe> probes;
    };

    struct EngineIndexProbe
    {
        CatalogManager::EmulationEngine engine;
        std::vector<const char*> index_names;
    };

    const std::vector<EngineTypeProbe> kEngineTypeProbes = {
        {CatalogManager::EmulationEngine::POSTGRESQL,
         {{"SERIAL", DataType::INT32, EmulatedStorageKind::DOMAIN},
          {"TSVECTOR", DataType::TSVECTOR, EmulatedStorageKind::NATIVE},
          {"CIRCLE", DataType::GEOMETRY, EmulatedStorageKind::DOMAIN}}},
        {CatalogManager::EmulationEngine::MYSQL,
         {{"BOOLEAN", DataType::INT8, EmulatedStorageKind::DOMAIN},
          {"GEOMETRY", DataType::GEOMETRY, EmulatedStorageKind::NATIVE},
          {"JSON", DataType::JSON, EmulatedStorageKind::NATIVE}}},
        {CatalogManager::EmulationEngine::FIREBIRD,
         {{"INT128", DataType::INT128, EmulatedStorageKind::NATIVE},
          {"DECFLOAT34", DataType::DECFLOAT34, EmulatedStorageKind::NATIVE},
          {"BLOB", DataType::BLOB, EmulatedStorageKind::NATIVE}}},
        {CatalogManager::EmulationEngine::CASSANDRA,
         {{"VARINT", DataType::DECIMAL, EmulatedStorageKind::DOMAIN},
          {"TIME", DataType::INT64, EmulatedStorageKind::DOMAIN},
          {"FROZEN", DataType::COMPOSITE, EmulatedStorageKind::DOMAIN}}},
        {CatalogManager::EmulationEngine::MILVUS,
         {{"FLOATVECTOR", DataType::VECTOR, EmulatedStorageKind::NATIVE},
          {"BINARYVECTOR", DataType::VECTOR, EmulatedStorageKind::NATIVE},
          {"ARRAYOFSTRUCT", DataType::ARRAY, EmulatedStorageKind::DOMAIN}}},
        {CatalogManager::EmulationEngine::MONGODB,
         {{"OBJECTID", DataType::BLOB, EmulatedStorageKind::DOMAIN},
          {"DBPOINTER", DataType::COMPOSITE, EmulatedStorageKind::DOMAIN},
          {"ARRAY", DataType::BSON, EmulatedStorageKind::DOMAIN}}},
        {CatalogManager::EmulationEngine::NEO4J,
         {{"POINT", DataType::POINT, EmulatedStorageKind::NATIVE},
          {"DURATION", DataType::INTERVAL, EmulatedStorageKind::NATIVE},
          {"LIST", DataType::ARRAY, EmulatedStorageKind::NATIVE}}},
        {CatalogManager::EmulationEngine::REDIS,
         {{"HASH", DataType::MAP, EmulatedStorageKind::DOMAIN},
          {"STREAM", DataType::LIST, EmulatedStorageKind::DOMAIN},
          {"GEO", DataType::LIST, EmulatedStorageKind::DOMAIN}}},
        {CatalogManager::EmulationEngine::CLICKHOUSE,
         {{"INT256", DataType::INT256, EmulatedStorageKind::NATIVE},
          {"UINT256", DataType::UINT256, EmulatedStorageKind::NATIVE},
          {"LOWCARDINALITY", DataType::DICT_ENCODED, EmulatedStorageKind::DOMAIN}}},
        {CatalogManager::EmulationEngine::INFLUXDB,
         {{"TIMESTAMP", DataType::TIMESTAMP_NS, EmulatedStorageKind::NATIVE}}},
        {CatalogManager::EmulationEngine::DUCKDB,
         {{"UNION", DataType::TAGGED_UNION, EmulatedStorageKind::NATIVE},
          {"TIMESTAMP_NS", DataType::TIMESTAMP_NS, EmulatedStorageKind::NATIVE}}},
        {CatalogManager::EmulationEngine::OPENSEARCH,
         {{"COMPLETION", DataType::COMPLETION_FIELD, EmulatedStorageKind::DOMAIN},
          {"SEARCH_AS_YOU_TYPE", DataType::PREFIX_SEARCH_FIELD, EmulatedStorageKind::DOMAIN},
          {"FLAT_OBJECT", DataType::FLAT_OBJECT, EmulatedStorageKind::DOMAIN}}},
        {CatalogManager::EmulationEngine::MARIADB,
         {{"BOOLEAN", DataType::INT8, EmulatedStorageKind::DOMAIN},
          {"JSON", DataType::JSON, EmulatedStorageKind::NATIVE},
          {"GEOMETRY", DataType::GEOMETRY, EmulatedStorageKind::NATIVE}}},
    };

    const std::vector<EngineIndexProbe> kEngineIndexProbes = {
        {CatalogManager::EmulationEngine::POSTGRESQL, {"BTREE", "HASH", "GIN", "GIST", "SPGIST", "BRIN"}},
        {CatalogManager::EmulationEngine::MYSQL, {"BTREE", "HASH", "RTREE", "FULLTEXT"}},
        {CatalogManager::EmulationEngine::FIREBIRD, {"BTREE", "HASH"}},
        {CatalogManager::EmulationEngine::CASSANDRA, {"CASSANDRA_SASI", "CASSANDRA_SAI"}},
        {CatalogManager::EmulationEngine::MILVUS, {"HNSW", "IVF_FLAT", "IVF_PQ", "IVF_SQ8", "IVF_SQ8_HYBRID"}},
        {CatalogManager::EmulationEngine::MONGODB,
         {"MONGODB_2D", "MONGODB_2DSPHERE", "MONGODB_WILDCARD", "MONGODB_ENCRYPTED_RANGE"}},
        {CatalogManager::EmulationEngine::NEO4J, {"NEO4J_LOOKUP", "NEO4J_TEXT", "NEO4J_RANGE", "NEO4J_POINT", "NEO4J_VECTOR"}},
        {CatalogManager::EmulationEngine::REDIS,
         {"REDIS_STRING", "REDIS_HASH", "REDIS_LIST", "REDIS_SET", "REDIS_ZSET", "REDIS_STREAM", "REDIS_BITMAP", "REDIS_HLL", "REDIS_GEO"}},
        {CatalogManager::EmulationEngine::CLICKHOUSE, {"COLUMNSTORE", "ZONEMAP", "BLOOM"}},
        {CatalogManager::EmulationEngine::INFLUXDB, {"LSM"}},
        {CatalogManager::EmulationEngine::DUCKDB, {"ART"}},
        {CatalogManager::EmulationEngine::OPENSEARCH, {"FULLTEXT"}},
        {CatalogManager::EmulationEngine::MARIADB, {"BTREE", "HASH", "RTREE", "FULLTEXT"}},
    };

} // namespace

TEST(EngineCrossCapabilityAuditContractTest, CanonicalProfileIdsResolveRepresentativeDatatypeCoverage)
{
    for (CatalogManager::EmulationEngine engine : canonicalEmulationEngineSet())
    {
        const char* profile_id = emulationEngineProfileId(engine);
        ASSERT_NE(profile_id, nullptr);
        ASSERT_STRNE(profile_id, "unspecified");
        ASSERT_STRNE(profile_id, "native");

        const EngineTypeProbe* probe_set = nullptr;
        for (const auto& probes : kEngineTypeProbes)
        {
            if (probes.engine == engine)
            {
                probe_set = &probes;
                break;
            }
        }
        ASSERT_NE(probe_set, nullptr) << "missing datatype probe set for engine "
                                      << static_cast<int>(engine);
        ASSERT_FALSE(probe_set->probes.empty());

        for (const auto& probe : probe_set->probes)
        {
            EmulatedTypeMapping mapping{};
            ASSERT_TRUE(TypeSystem::resolveEmulatedType(profile_id, probe.emulated_type, mapping))
                << "missing datatype mapping for profile_id=" << profile_id
                << " type=" << probe.emulated_type;
            EXPECT_EQ(mapping.canonical_type, probe.canonical_type)
                << "canonical type mismatch for profile_id=" << profile_id
                << " type=" << probe.emulated_type;
            EXPECT_EQ(mapping.storage_kind, probe.storage_kind)
                << "storage-kind mismatch for profile_id=" << profile_id
                << " type=" << probe.emulated_type;
        }
    }
}

TEST(EngineCrossCapabilityAuditContractTest, CanonicalEmulationEnginesHaveRepresentativeIndexCoverage)
{
    for (CatalogManager::EmulationEngine engine : canonicalEmulationEngineSet())
    {
        const EngineIndexProbe* probe_set = nullptr;
        for (const auto& probes : kEngineIndexProbes)
        {
            if (probes.engine == engine)
            {
                probe_set = &probes;
                break;
            }
        }
        ASSERT_NE(probe_set, nullptr) << "missing index probe set for engine "
                                      << static_cast<int>(engine);
        ASSERT_FALSE(probe_set->index_names.empty());

        for (const char* index_name : probe_set->index_names)
        {
            auto parsed = parseIndexType(index_name);
            ASSERT_TRUE(parsed.has_value())
                << "index parser does not recognize required engine index name: "
                << index_name;

            const auto* caps = IndexFactory::lookupCapabilities(parsed.value());
            ASSERT_NE(caps, nullptr)
                << "IndexFactory capability row missing for index type " << index_name;

            EXPECT_TRUE(caps->supports_create) << index_name;
            EXPECT_TRUE(caps->supports_open) << index_name;
            EXPECT_TRUE(caps->supports_close) << index_name;
            EXPECT_FALSE(std::string(caps->canonical_name).empty()) << index_name;
        }
    }
}

TEST(EngineCrossCapabilityAuditContractTest, IndexCapabilityRegistryRoundTripsWithCatalogIndexTypeParser)
{
    const auto caps_matrix = IndexFactory::listCapabilities();
    ASSERT_FALSE(caps_matrix.empty());

    for (const auto& caps : caps_matrix)
    {
        auto parsed_from_registry_name = parseIndexType(caps.canonical_name);
        ASSERT_TRUE(parsed_from_registry_name.has_value())
            << "registry canonical name is not parseable: " << caps.canonical_name;
        EXPECT_EQ(parsed_from_registry_name.value(), caps.index_type);

        const std::string emitted_name = indexTypeToString(caps.index_type);
        auto parsed_from_emitted = parseIndexType(emitted_name);
        ASSERT_TRUE(parsed_from_emitted.has_value())
            << "indexTypeToString emitted non-parseable value: " << emitted_name;
        EXPECT_EQ(parsed_from_emitted.value(), caps.index_type);
    }
}
