/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

#include "scratchbird/catalog/virtual_catalog.h"

namespace
{
    using scratchbird::catalog::ProtocolType;
    using scratchbird::catalog::VirtualCatalogHandler;
    using scratchbird::catalog::VirtualCatalogRouter;
    using scratchbird::catalog::VirtualResultSet;
    using scratchbird::core::CatalogManager;
    using scratchbird::core::ErrorContext;
    using scratchbird::core::Status;

    class NullVirtualCatalogHandler final : public VirtualCatalogHandler
    {
    public:
        explicit NullVirtualCatalogHandler(ProtocolType type)
            : type_(type)
        {
        }

        ProtocolType getProtocolType() const override
        {
            return type_;
        }

        bool ownsSchema(const std::string & /*schema_name*/) const override
        {
            return false;
        }

        bool ownsTable(const std::string & /*schema_name*/, const std::string & /*table_name*/) const override
        {
            return false;
        }

        Status queryTable(const std::string & /*schema_name*/, const std::string & /*table_name*/,
                          const std::string & /*where_clause*/, VirtualResultSet & /*results*/,
                          ErrorContext * /*ctx*/) override
        {
            return Status::NOT_FOUND;
        }

        Status getTableColumns(const std::string & /*schema_name*/, const std::string & /*table_name*/,
                               std::vector<CatalogManager::ColumnInfo> & /*columns*/,
                               ErrorContext * /*ctx*/) override
        {
            return Status::NOT_FOUND;
        }

        Status listTables(const std::string & /*schema_name*/, std::vector<std::string> & /*table_names*/,
                          ErrorContext * /*ctx*/) override
        {
            return Status::NOT_FOUND;
        }

        Status listSchemas(std::vector<std::string> & /*schema_names*/, ErrorContext * /*ctx*/) override
        {
            return Status::NOT_FOUND;
        }

    private:
        ProtocolType type_;
    };

    static const ProtocolType kCanonicalProtocols[] = {
        ProtocolType::SCRATCHBIRD,
        ProtocolType::POSTGRESQL,
        ProtocolType::MYSQL,
        ProtocolType::FIREBIRD,
        ProtocolType::CASSANDRA,
        ProtocolType::CLICKHOUSE,
        ProtocolType::DUCKDB,
        ProtocolType::INFLUXDB,
        ProtocolType::MARIADB,
        ProtocolType::MILVUS,
        ProtocolType::MONGODB,
        ProtocolType::NEO4J,
        ProtocolType::OPENSEARCH,
        ProtocolType::REDIS,
    };
} // namespace

TEST(VirtualCatalogProtocolMatrixContractTest, ProtocolStringMappingCoversCanonicalSet)
{
    using scratchbird::catalog::protocolTypeFromString;
    using scratchbird::catalog::protocolTypeToString;

    EXPECT_EQ(protocolTypeFromString("scratchbird"), ProtocolType::SCRATCHBIRD);
    EXPECT_EQ(protocolTypeFromString("postgres"), ProtocolType::POSTGRESQL);
    EXPECT_EQ(protocolTypeFromString("mysql"), ProtocolType::MYSQL);
    EXPECT_EQ(protocolTypeFromString("firebird"), ProtocolType::FIREBIRD);
    EXPECT_EQ(protocolTypeFromString("firebirdsql"), ProtocolType::FIREBIRD);
    EXPECT_EQ(protocolTypeFromString("cassandra"), ProtocolType::CASSANDRA);
    EXPECT_EQ(protocolTypeFromString("clickhouse"), ProtocolType::CLICKHOUSE);
    EXPECT_EQ(protocolTypeFromString("duckdb"), ProtocolType::DUCKDB);
    EXPECT_EQ(protocolTypeFromString("influxdb"), ProtocolType::INFLUXDB);
    EXPECT_EQ(protocolTypeFromString("mariadb"), ProtocolType::MARIADB);
    EXPECT_EQ(protocolTypeFromString("milvus"), ProtocolType::MILVUS);
    EXPECT_EQ(protocolTypeFromString("mongo"), ProtocolType::MONGODB);
    EXPECT_EQ(protocolTypeFromString("neo4j"), ProtocolType::NEO4J);
    EXPECT_EQ(protocolTypeFromString("opensearch"), ProtocolType::OPENSEARCH);
    EXPECT_EQ(protocolTypeFromString("redis"), ProtocolType::REDIS);

    EXPECT_STREQ(protocolTypeToString(ProtocolType::FIREBIRD), "firebirdsql");
    EXPECT_STREQ(protocolTypeToString(ProtocolType::CASSANDRA), "cassandra");
    EXPECT_STREQ(protocolTypeToString(ProtocolType::CLICKHOUSE), "clickhouse");
    EXPECT_STREQ(protocolTypeToString(ProtocolType::DUCKDB), "duckdb");
    EXPECT_STREQ(protocolTypeToString(ProtocolType::INFLUXDB), "influxdb");
    EXPECT_STREQ(protocolTypeToString(ProtocolType::MARIADB), "mariadb");
    EXPECT_STREQ(protocolTypeToString(ProtocolType::MILVUS), "milvus");
    EXPECT_STREQ(protocolTypeToString(ProtocolType::MONGODB), "mongodb");
    EXPECT_STREQ(protocolTypeToString(ProtocolType::NEO4J), "neo4j");
    EXPECT_STREQ(protocolTypeToString(ProtocolType::OPENSEARCH), "opensearch");
    EXPECT_STREQ(protocolTypeToString(ProtocolType::REDIS), "redis");
}

TEST(VirtualCatalogProtocolMatrixContractTest, RouterRegistersHandlersForCanonicalProtocolSet)
{
    VirtualCatalogRouter &router = VirtualCatalogRouter::getInstance();
    router.initialize(nullptr);

    for (ProtocolType protocol : kCanonicalProtocols)
    {
        router.registerHandler(protocol, std::make_unique<NullVirtualCatalogHandler>(protocol));
        EXPECT_NE(router.getHandler(protocol), nullptr);
    }

    const std::vector<ProtocolType> registered = router.listRegisteredProtocols();
    const std::set<ProtocolType> registered_set(registered.begin(), registered.end());

    for (ProtocolType protocol : kCanonicalProtocols)
    {
        EXPECT_TRUE(registered_set.find(protocol) != registered_set.end());
    }
}
