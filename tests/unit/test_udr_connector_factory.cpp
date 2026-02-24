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

#include <algorithm>
#include <set>
#include <vector>

namespace scratchbird::udr {
namespace {

constexpr ConnectorType kUnknownConnectorType = static_cast<ConnectorType>(0);

auto bootstrapScaffoldTypes() -> const std::vector<ConnectorType>& {
    static const std::vector<ConnectorType> kTypes{
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
    };
    return kTypes;
}

TEST(UDRConnectorFactoryTest, CreateImplementedConnectors) {
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
             ConnectorType::REDIS}) {
        auto connector = UDRConnectorFactory::create(type);
        ASSERT_NE(connector, nullptr);
        EXPECT_EQ(connector->getType(), type);
    }
}

TEST(UDRConnectorFactoryTest, UnsupportedTypesReturnNull) {
    EXPECT_EQ(UDRConnectorFactory::create(ConnectorType::ODBC), nullptr);
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

    auto mariadb = UDRConnectorFactory::create("mariadb://db.example:3306/app");
    ASSERT_NE(mariadb, nullptr);
    EXPECT_EQ(mariadb->getType(), ConnectorType::MARIADB);

    auto clickhouse = UDRConnectorFactory::create("clickhouse://db.example:9000/app");
    ASSERT_NE(clickhouse, nullptr);
    EXPECT_EQ(clickhouse->getType(), ConnectorType::CLICKHOUSE);

    auto duckdb = UDRConnectorFactory::create("duckdb:///tmp/test.duckdb");
    ASSERT_NE(duckdb, nullptr);
    EXPECT_EQ(duckdb->getType(), ConnectorType::DUCKDB);

    auto influxdb = UDRConnectorFactory::create("influxdb://db.example:8086/app");
    ASSERT_NE(influxdb, nullptr);
    EXPECT_EQ(influxdb->getType(), ConnectorType::INFLUXDB);

    auto cassandra = UDRConnectorFactory::create("cassandra://db.example:9042/app");
    ASSERT_NE(cassandra, nullptr);
    EXPECT_EQ(cassandra->getType(), ConnectorType::CASSANDRA);

    auto milvus = UDRConnectorFactory::create("milvus://db.example:19530/app");
    ASSERT_NE(milvus, nullptr);
    EXPECT_EQ(milvus->getType(), ConnectorType::MILVUS);

    auto mongodb = UDRConnectorFactory::create("mongodb://db.example:27017/app");
    ASSERT_NE(mongodb, nullptr);
    EXPECT_EQ(mongodb->getType(), ConnectorType::MONGODB);

    auto neo4j = UDRConnectorFactory::create("neo4j://db.example:7687/app");
    ASSERT_NE(neo4j, nullptr);
    EXPECT_EQ(neo4j->getType(), ConnectorType::NEO4J);

    auto opensearch = UDRConnectorFactory::create("opensearch://db.example:9200/app");
    ASSERT_NE(opensearch, nullptr);
    EXPECT_EQ(opensearch->getType(), ConnectorType::OPENSEARCH);

    auto redis = UDRConnectorFactory::create("redis://db.example:6379/0");
    ASSERT_NE(redis, nullptr);
    EXPECT_EQ(redis->getType(), ConnectorType::REDIS);

    EXPECT_EQ(UDRConnectorFactory::create("odbc://dsn=legacy"), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create("unknown://target"), nullptr);
    EXPECT_EQ(UDRConnectorFactory::create(""), nullptr);
}

TEST(UDRConnectorFactoryTest, BootstrapScaffoldConnectorsInitializeAndExposeCapabilities) {
    for (const ConnectorType type : bootstrapScaffoldTypes()) {
        auto connector = UDRConnectorFactory::create(type);
        ASSERT_NE(connector, nullptr);

        UDRServerConfig config{};
        config.host = "127.0.0.1";
        config.database = "bootstrap_db";
        config.user = "bootstrap_user";
        config.password = "bootstrap_password";

        core::ErrorContext ctx;
        EXPECT_EQ(connector->initialize(config, &ctx), core::Status::OK) << ctx.message;
        EXPECT_TRUE(connector->isConnected());
        EXPECT_EQ(connector->ping(&ctx), core::Status::OK) << ctx.message;
        EXPECT_EQ(connector->getType(), type);

        const auto features = connector->getSupportedFeatures();
        EXPECT_FALSE(features.empty());
        EXPECT_NE(std::find(features.begin(), features.end(), "e2_metadata_snapshot"),
                  features.end());
        EXPECT_NE(std::find(features.begin(), features.end(), "e3_query_passthrough"),
                  features.end());
        EXPECT_NE(std::find(features.begin(), features.end(), "e4_prepared_lifecycle"),
                  features.end());
        EXPECT_NE(std::find(features.begin(), features.end(), "e5_show_describe_comment_surface"),
                  features.end());
        EXPECT_NE(std::find(features.begin(), features.end(), "e6_degraded_mode"),
                  features.end());
        EXPECT_NE(std::find(features.begin(), features.end(), "e7_signoff_ready"),
                  features.end());

        EXPECT_EQ(connector->shutdown(&ctx), core::Status::OK);
        EXPECT_FALSE(connector->isConnected());
    }
}

TEST(UDRConnectorFactoryTest, BootstrapScaffoldConnectorsProvideMetadataProjection) {
    for (const ConnectorType type : bootstrapScaffoldTypes()) {
        auto connector = UDRConnectorFactory::create(type);
        ASSERT_NE(connector, nullptr);

        UDRServerConfig config{};
        config.host = "127.0.0.1";
        config.database = "projection_db";

        core::ErrorContext ctx;
        ASSERT_EQ(connector->initialize(config, &ctx), core::Status::OK) << ctx.message;

        std::vector<std::string> tables;
        EXPECT_EQ(connector->listTables("", tables, &ctx), core::Status::OK) << ctx.message;
        ASSERT_FALSE(tables.empty());

        RemoteTableInfo table_info;
        EXPECT_EQ(connector->getTableInfo("", tables.front(), table_info, &ctx), core::Status::OK)
            << ctx.message;
        EXPECT_FALSE(table_info.columns.empty());

        std::vector<std::string> procedures;
        EXPECT_EQ(connector->listProcedures("", procedures, &ctx), core::Status::OK)
            << ctx.message;
        ASSERT_FALSE(procedures.empty());

        RemoteProcedureInfo proc_info;
        EXPECT_EQ(connector->getProcedureInfo("", procedures.front(), proc_info, &ctx),
                  core::Status::OK)
            << ctx.message;
        EXPECT_FALSE(proc_info.output_params.empty());
    }
}

TEST(UDRConnectorFactoryTest, BootstrapScaffoldConnectorsExecuteCommandClassesDeterministically) {
    for (const ConnectorType type : bootstrapScaffoldTypes()) {
        auto connector = UDRConnectorFactory::create(type);
        ASSERT_NE(connector, nullptr);

        UDRServerConfig config{};
        config.host = "127.0.0.1";
        config.database = "exec_db";

        core::ErrorContext ctx;
        ASSERT_EQ(connector->initialize(config, &ctx), core::Status::OK) << ctx.message;

        RemoteResultSet query_result;
        EXPECT_EQ(connector->executeQuery("SELECT 1", query_result, &ctx), core::Status::OK)
            << ctx.message;
        EXPECT_EQ(query_result.rows_affected, 1u);
        EXPECT_FALSE(query_result.rows.empty());

        uint64_t rows_affected = 999;
        EXPECT_EQ(connector->executeCommand("INSERT INTO t VALUES (1)", rows_affected, &ctx),
                  core::Status::OK)
            << ctx.message;
        EXPECT_EQ(rows_affected, 1u);

        EXPECT_EQ(connector->executeCommand("CREATE TABLE t(id bigint)", rows_affected, &ctx),
                  core::Status::OK)
            << ctx.message;
        EXPECT_EQ(rows_affected, 0u);

        EXPECT_EQ(connector->executeCommand("ANALYZE TABLE t", rows_affected, &ctx),
                  core::Status::OK)
            << ctx.message;
        EXPECT_EQ(rows_affected, 0u);

        EXPECT_EQ(connector->executeCommand("SELECT 1", rows_affected, &ctx),
                  core::Status::NOT_SUPPORTED);
    }
}

TEST(UDRConnectorFactoryTest,
     BootstrapScaffoldConnectorsPreparedTxnTimeoutAndCancelLifecycle) {
    for (const ConnectorType type : bootstrapScaffoldTypes()) {
        auto connector = UDRConnectorFactory::create(type);
        ASSERT_NE(connector, nullptr);

        UDRServerConfig config{};
        config.host = "127.0.0.1";
        config.database = "prepared_txn_db";

        core::ErrorContext ctx;
        ASSERT_EQ(connector->initialize(config, &ctx), core::Status::OK) << ctx.message;

        std::vector<uint32_t> param_types;
        ASSERT_EQ(connector->prepareStatement("ps_query", "SELECT 1", param_types, &ctx),
                  core::Status::OK)
            << ctx.message;
        ASSERT_TRUE(param_types.empty());

        RemoteResultSet prepared_query_result;
        EXPECT_EQ(connector->executePrepared("ps_query", {}, prepared_query_result, &ctx),
                  core::Status::OK)
            << ctx.message;
        EXPECT_EQ(prepared_query_result.command_tag, "SELECT");
        EXPECT_EQ(prepared_query_result.rows_affected, 1u);

        ASSERT_EQ(connector->prepareStatement("ps_dml",
                                              "INSERT INTO t VALUES (1)",
                                              param_types,
                                              &ctx),
                  core::Status::OK)
            << ctx.message;
        RemoteResultSet prepared_dml_result;
        EXPECT_EQ(connector->executePrepared("ps_dml", {}, prepared_dml_result, &ctx),
                  core::Status::OK)
            << ctx.message;
        EXPECT_EQ(prepared_dml_result.command_tag, "EXECUTE");
        EXPECT_EQ(prepared_dml_result.rows_affected, 1u);

        EXPECT_EQ(connector->beginTransaction(&ctx), core::Status::OK) << ctx.message;
        EXPECT_EQ(connector->savepoint("sp1", &ctx), core::Status::OK) << ctx.message;
        EXPECT_EQ(connector->rollbackToSavepoint("sp1", &ctx), core::Status::OK) << ctx.message;
        EXPECT_EQ(connector->rollbackToSavepoint("missing", &ctx), core::Status::NOT_FOUND);
        EXPECT_EQ(connector->commitTransaction(&ctx), core::Status::OK) << ctx.message;
        EXPECT_EQ(connector->commitTransaction(&ctx), core::Status::NO_ACTIVE_TRANSACTION);

        RemoteResultSet timeout_result;
        EXPECT_EQ(connector->executeQuery("SELECT 1 /*sb_timeout*/", timeout_result, &ctx),
                  core::Status::LOCK_TIMEOUT);
        EXPECT_EQ(connector->executeQuery("SELECT 1 /*sb_cancel*/", timeout_result, &ctx),
                  core::Status::QUERY_CANCELED);
    }
}

TEST(UDRConnectorFactoryTest, BootstrapScaffoldConnectorsSupportShowDescribeCommentPaths) {
    for (const ConnectorType type : bootstrapScaffoldTypes()) {
        auto connector = UDRConnectorFactory::create(type);
        ASSERT_NE(connector, nullptr);

        UDRServerConfig config{};
        config.host = "127.0.0.1";
        config.database = "surface_db";

        core::ErrorContext ctx;
        ASSERT_EQ(connector->initialize(config, &ctx), core::Status::OK) << ctx.message;

        RemoteResultSet show_result;
        EXPECT_EQ(connector->executeQuery("SHOW TABLES", show_result, &ctx), core::Status::OK)
            << ctx.message;
        EXPECT_EQ(show_result.command_tag, "SELECT");

        RemoteResultSet describe_result;
        EXPECT_EQ(connector->executeQuery("DESCRIBE t", describe_result, &ctx), core::Status::OK)
            << ctx.message;
        EXPECT_EQ(describe_result.command_tag, "SELECT");

        uint64_t rows_affected = 999;
        EXPECT_EQ(connector->executeCommand("COMMENT ON TABLE t IS 'doc'", rows_affected, &ctx),
                  core::Status::OK)
            << ctx.message;
        EXPECT_EQ(rows_affected, 0u);
    }
}

TEST(UDRConnectorFactoryTest, BootstrapScaffoldConnectorsExposeDegradedModeSemantics) {
    for (const ConnectorType type : bootstrapScaffoldTypes()) {
        auto connector = UDRConnectorFactory::create(type);
        ASSERT_NE(connector, nullptr);

        UDRServerConfig config{};
        config.host = "127.0.0.1";
        config.database = "degraded_db";

        core::ErrorContext ctx;
        ASSERT_EQ(connector->initialize(config, &ctx), core::Status::OK) << ctx.message;

        uint64_t rows_affected = 0;
        EXPECT_EQ(connector->executeCommand("SET DEGRADED ON", rows_affected, &ctx),
                  core::Status::OK)
            << ctx.message;
        EXPECT_EQ(connector->ping(&ctx), core::Status::CONNECTION_FAILURE);
        EXPECT_EQ(connector->executeCommand("INSERT INTO t VALUES (1)", rows_affected, &ctx),
                  core::Status::CONNECTION_FAILURE);

        EXPECT_EQ(connector->executeCommand("SET DEGRADED OFF", rows_affected, &ctx),
                  core::Status::OK)
            << ctx.message;
        EXPECT_EQ(connector->ping(&ctx), core::Status::OK) << ctx.message;
    }
}

TEST(UDRConnectorFactoryTest, SysRemoteContractsValidateBindingBeforeDispatch) {
    core::ErrorContext ctx;
    uint64_t rows_affected = 123;
    EXPECT_EQ(sys_remote_exec("srv", "select 1", rows_affected, &ctx), core::Status::INVALID_ARGUMENT);
    EXPECT_EQ(rows_affected, 0u);
    EXPECT_EQ(ctx.code, core::Status::INVALID_ARGUMENT);

    RemoteResultSet result;
    result.rows_affected = 99;
    EXPECT_EQ(sys_remote_query("srv", "select 1", result, &ctx), core::Status::INVALID_ARGUMENT);
    EXPECT_EQ(result.rows_affected, 0u);
    EXPECT_TRUE(result.rows.empty());

    std::vector<RemoteValue> params;
    EXPECT_EQ(sys_remote_call("srv", "proc", params, result, &ctx), core::Status::INVALID_ARGUMENT);
    EXPECT_EQ(result.rows_affected, 0u);
}

} // namespace
} // namespace scratchbird::udr
