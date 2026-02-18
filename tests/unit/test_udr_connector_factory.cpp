/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "gtest/gtest.h"

#include "scratchbird/udr/udr_connector.h"

#include <set>
#include <vector>

namespace scratchbird::udr {
namespace {

constexpr ConnectorType kUnknownConnectorType = static_cast<ConnectorType>(0);

TEST(UDRConnectorFactoryTest, CreateImplementedConnectors) {
    auto pg = UDRConnectorFactory::create(ConnectorType::POSTGRESQL);
    ASSERT_NE(pg, nullptr);
    EXPECT_EQ(pg->getType(), ConnectorType::POSTGRESQL);

    auto mysql = UDRConnectorFactory::create(ConnectorType::MYSQL);
    ASSERT_NE(mysql, nullptr);
    EXPECT_EQ(mysql->getType(), ConnectorType::MYSQL);

    auto firebird = UDRConnectorFactory::create(ConnectorType::FIREBIRD);
    ASSERT_NE(firebird, nullptr);
    EXPECT_EQ(firebird->getType(), ConnectorType::FIREBIRD);

    auto scratchbird = UDRConnectorFactory::create(ConnectorType::SCRATCHBIRD);
    ASSERT_NE(scratchbird, nullptr);
    EXPECT_EQ(scratchbird->getType(), ConnectorType::SCRATCHBIRD);
}

TEST(UDRConnectorFactoryTest, UnsupportedTypesReturnNull) {
    EXPECT_EQ(UDRConnectorFactory::create(ConnectorType::ODBC), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create(ConnectorType::CASSANDRA), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create(ConnectorType::CLICKHOUSE), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create(ConnectorType::DUCKDB), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create(ConnectorType::INFLUXDB), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create(ConnectorType::MARIADB), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create(ConnectorType::MILVUS), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create(ConnectorType::MONGODB), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create(ConnectorType::NEO4J), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create(ConnectorType::OPENSEARCH), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create(ConnectorType::REDIS), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create(kUnknownConnectorType), nullptr);
}

TEST(UDRConnectorFactoryTest, SupportedTypesMatchesFactoryBehavior) {
    const auto supported = UDRConnectorFactory::getSupportedTypes();
    const std::set<ConnectorType> supported_set(supported.begin(), supported.end());

    for (const ConnectorType type : {
             ConnectorType::POSTGRESQL,
             ConnectorType::MYSQL,
             ConnectorType::FIREBIRD,
             ConnectorType::SCRATCHBIRD,
             ConnectorType::CASSANDRA,
             ConnectorType::CLICKHOUSE,
             ConnectorType::DUCKDB,
             ConnectorType::INFLUXDB,
             ConnectorType::MARIADB,
             ConnectorType::MILVUS,
             ConnectorType::MONGODB,
             ConnectorType::NEO4J,
             ConnectorType::OPENSEARCH,
             ConnectorType::REDIS,
             ConnectorType::ODBC,
             kUnknownConnectorType}) {
        const bool listed = supported_set.find(type) != supported_set.end();
        const bool advertised = UDRConnectorFactory::isSupported(type);
        const bool creatable = (UDRConnectorFactory::create(type) != nullptr);

        EXPECT_EQ(advertised, listed);
        EXPECT_EQ(advertised, creatable);
    }
}

TEST(UDRConnectorFactoryTest, StringTypeMappingIsDeterministic) {
    EXPECT_EQ(UDRConnectorFactory::stringToType("postgresql"), ConnectorType::POSTGRESQL);
    EXPECT_EQ(UDRConnectorFactory::stringToType("Postgres"), ConnectorType::POSTGRESQL);
    EXPECT_EQ(UDRConnectorFactory::stringToType("pg"), ConnectorType::POSTGRESQL);
    EXPECT_EQ(UDRConnectorFactory::stringToType("mysql"), ConnectorType::MYSQL);
    EXPECT_EQ(UDRConnectorFactory::stringToType("mariadb"), ConnectorType::MARIADB);
    EXPECT_EQ(UDRConnectorFactory::stringToType("firebirdsql"), ConnectorType::FIREBIRD);
    EXPECT_EQ(UDRConnectorFactory::stringToType("cassandra"), ConnectorType::CASSANDRA);
    EXPECT_EQ(UDRConnectorFactory::stringToType("milvus"), ConnectorType::MILVUS);
    EXPECT_EQ(UDRConnectorFactory::stringToType("mongo"), ConnectorType::MONGODB);
    EXPECT_EQ(UDRConnectorFactory::stringToType("neo4j"), ConnectorType::NEO4J);
    EXPECT_EQ(UDRConnectorFactory::stringToType("redis"), ConnectorType::REDIS);
    EXPECT_EQ(UDRConnectorFactory::stringToType("influxdb"), ConnectorType::INFLUXDB);
    EXPECT_EQ(UDRConnectorFactory::stringToType("clickhouse"), ConnectorType::CLICKHOUSE);
    EXPECT_EQ(UDRConnectorFactory::stringToType("opensearch"), ConnectorType::OPENSEARCH);
    EXPECT_EQ(UDRConnectorFactory::stringToType("duckdb"), ConnectorType::DUCKDB);
    EXPECT_EQ(UDRConnectorFactory::stringToType("scratchbird"), ConnectorType::SCRATCHBIRD);
    EXPECT_EQ(UDRConnectorFactory::stringToType("sb"), ConnectorType::SCRATCHBIRD);
    EXPECT_EQ(UDRConnectorFactory::stringToType("odbc"), ConnectorType::ODBC);
    EXPECT_EQ(UDRConnectorFactory::stringToType("not-a-connector"), kUnknownConnectorType);

    EXPECT_STREQ(UDRConnectorFactory::typeToString(ConnectorType::POSTGRESQL), "postgresql");
    EXPECT_STREQ(UDRConnectorFactory::typeToString(ConnectorType::MYSQL), "mysql");
    EXPECT_STREQ(UDRConnectorFactory::typeToString(ConnectorType::FIREBIRD), "firebird");
    EXPECT_STREQ(UDRConnectorFactory::typeToString(ConnectorType::CASSANDRA), "cassandra");
    EXPECT_STREQ(UDRConnectorFactory::typeToString(ConnectorType::CLICKHOUSE), "clickhouse");
    EXPECT_STREQ(UDRConnectorFactory::typeToString(ConnectorType::DUCKDB), "duckdb");
    EXPECT_STREQ(UDRConnectorFactory::typeToString(ConnectorType::INFLUXDB), "influxdb");
    EXPECT_STREQ(UDRConnectorFactory::typeToString(ConnectorType::MARIADB), "mariadb");
    EXPECT_STREQ(UDRConnectorFactory::typeToString(ConnectorType::MILVUS), "milvus");
    EXPECT_STREQ(UDRConnectorFactory::typeToString(ConnectorType::MONGODB), "mongodb");
    EXPECT_STREQ(UDRConnectorFactory::typeToString(ConnectorType::NEO4J), "neo4j");
    EXPECT_STREQ(UDRConnectorFactory::typeToString(ConnectorType::OPENSEARCH), "opensearch");
    EXPECT_STREQ(UDRConnectorFactory::typeToString(ConnectorType::REDIS), "redis");
    EXPECT_STREQ(UDRConnectorFactory::typeToString(ConnectorType::SCRATCHBIRD), "scratchbird");
    EXPECT_STREQ(UDRConnectorFactory::typeToString(ConnectorType::ODBC), "odbc");
    EXPECT_STREQ(UDRConnectorFactory::typeToString(kUnknownConnectorType), "unknown");
}

TEST(UDRConnectorFactoryTest, CanonicalVnextEngineSetHasStableTypeIds) {
    const std::vector<ConnectorType> canonical_set{
        ConnectorType::CASSANDRA,
        ConnectorType::CLICKHOUSE,
        ConnectorType::DUCKDB,
        ConnectorType::FIREBIRD,
        ConnectorType::INFLUXDB,
        ConnectorType::MARIADB,
        ConnectorType::MILVUS,
        ConnectorType::MONGODB,
        ConnectorType::MYSQL,
        ConnectorType::NEO4J,
        ConnectorType::OPENSEARCH,
        ConnectorType::POSTGRESQL,
        ConnectorType::REDIS,
    };

    EXPECT_EQ(canonical_set.size(), 13u);

    std::set<uint8_t> ids;
    for (const ConnectorType type : canonical_set) {
        ids.insert(static_cast<uint8_t>(type));
    }
    EXPECT_EQ(ids.size(), canonical_set.size());
}

TEST(UDRConnectorFactoryTest, CreateFromConnectionStringUsesScheme) {
    auto pg = UDRConnectorFactory::create("postgresql://127.0.0.1:5432/app");
    ASSERT_NE(pg, nullptr);
    EXPECT_EQ(pg->getType(), ConnectorType::POSTGRESQL);

    auto mysql = UDRConnectorFactory::create(" MySQL://db.example.local:3306/app ");
    ASSERT_NE(mysql, nullptr);
    EXPECT_EQ(mysql->getType(), ConnectorType::MYSQL);

    auto firebird = UDRConnectorFactory::create("firebird:localhost/3050:employee");
    ASSERT_NE(firebird, nullptr);
    EXPECT_EQ(firebird->getType(), ConnectorType::FIREBIRD);

    auto scratchbird = UDRConnectorFactory::create("sb://localhost:8812/main");
    ASSERT_NE(scratchbird, nullptr);
    EXPECT_EQ(scratchbird->getType(), ConnectorType::SCRATCHBIRD);

    EXPECT_EQ(UDRConnectorFactory::create("mariadb://db.example:3306/app"), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create("clickhouse://db.example:9000/app"), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create("duckdb:///tmp/test.duckdb"), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create("influxdb://db.example:8086/app"), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create("cassandra://db.example:9042/app"), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create("milvus://db.example:19530/app"), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create("mongodb://db.example:27017/app"), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create("neo4j://db.example:7687/app"), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create("opensearch://db.example:9200/app"), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create("redis://db.example:6379/0"), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create("odbc://dsn=legacy"), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create("unknown://target"), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create(""), nullptr);
}

TEST(UDRConnectorFactoryTest, SysRemoteContractsRejectDeterministicallyUntilImplemented) {
    core::ErrorContext ctx;
    uint64_t rows_affected = 123;
    EXPECT_EQ(sys_remote_exec("srv", "select 1", rows_affected, &ctx), core::Status::NOT_IMPLEMENTED);
    EXPECT_EQ(rows_affected, 0u);
    EXPECT_EQ(ctx.code, core::Status::NOT_IMPLEMENTED);

    RemoteResultSet result;
    result.rows_affected = 99;
    EXPECT_EQ(sys_remote_query("srv", "select 1", result, &ctx), core::Status::NOT_IMPLEMENTED);
    EXPECT_EQ(result.rows_affected, 0u);
    EXPECT_TRUE(result.rows.empty());

    std::vector<RemoteValue> params;
    EXPECT_EQ(sys_remote_call("srv", "proc", params, result, &ctx), core::Status::NOT_IMPLEMENTED);
    EXPECT_EQ(result.rows_affected, 0u);
}

} // namespace
} // namespace scratchbird::udr
