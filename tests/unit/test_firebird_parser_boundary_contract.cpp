/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "scratchbird/catalog/firebird_catalog.h"
#include "scratchbird/protocol/adapters/firebird_adapter.h"

// Include core types before firebird_parser_agent.h (header references core types).
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/typed_value.h"

#include "scratchbird/ipc/firebird_parser_agent.h"
#include "scratchbird/sblr/executor.h"

namespace {

std::string firebirdStoredIdentifier(std::string value) {
    std::transform(value.begin(),
                   value.end(),
                   value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return value;
}

template <typename AdapterT>
class CompileHarness : public AdapterT {
public:
    using AdapterT::AdapterT;

    scratchbird::core::Status runCompile(const std::string& sql,
                                         std::vector<uint8_t>& bytecode_out,
                                         std::string& error_out) {
        return AdapterT::compileQuery(sql, bytecode_out, error_out);
    }

    void applyFirebirdSessionSchemaContextForTest(scratchbird::core::ErrorContext* ctx) {
        if constexpr (std::is_base_of_v<scratchbird::protocol::FirebirdAdapter, AdapterT>) {
            AdapterT::applyFirebirdSessionSchemaContextForTest(ctx);
        }
    }

    scratchbird::core::ConnectionContext* connectionContextForTest() {
        return AdapterT::connection_ctx_.get();
    }

    scratchbird::core::Database* engineDatabaseForTest() {
        return AdapterT::engineDatabase();
    }
};

class FirebirdParserAgentHarness : public scratchbird::ipc::FirebirdParserAgent {
public:
    using scratchbird::ipc::FirebirdParserAgent::FirebirdParserAgent;
    using scratchbird::ipc::FirebirdParserAgent::mapProtocolErrorToSQLState;
    using scratchbird::ipc::FirebirdParserAgent::mapSQLStateToProtocol;
};

scratchbird::protocol::ProtocolAdapterConfig makeAdapterConfig(const std::string& name) {
    scratchbird::protocol::ProtocolAdapterConfig cfg;
    cfg.database_path = (std::filesystem::path("build") / "database" / name).string();
    cfg.auto_create_db = true;
    return cfg;
}

void cleanupDb(const std::string& name) {
    std::error_code ec;
    const auto path = std::filesystem::path("build") / "database" / name;
    std::filesystem::remove(path, ec);
    std::filesystem::create_directories(path.parent_path(), ec);
}

scratchbird::ipc::ParserAgentConfig makeParserAgentConfig() {
    scratchbird::ipc::ParserAgentConfig cfg;
    cfg.name = "fb_boundary_contract_agent";
    cfg.protocol = "firebird";
    cfg.listen_endpoint = "127.0.0.1:0";
    cfg.ipc_endpoint = "/tmp/fb_boundary_contract.sock";
    return cfg;
}

template <typename AdapterT>
scratchbird::core::Status compileSql(CompileHarness<AdapterT>& adapter, const std::string& sql) {
    std::vector<uint8_t> bytecode;
    std::string error;
    return adapter.runCompile(sql, bytecode, error);
}

template <typename AdapterT>
scratchbird::core::Status compileSql(CompileHarness<AdapterT>& adapter,
                                     const std::string& sql,
                                     std::string& error_out) {
    std::vector<uint8_t> bytecode;
    return adapter.runCompile(sql, bytecode, error_out);
}

void appendBe32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void appendBe64(std::vector<uint8_t>& out, uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<uint8_t>((value >> shift) & 0xFF));
    }
}

void appendXdrBuffer(std::vector<uint8_t>& out, const std::string& value) {
    appendBe32(out, static_cast<uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
    while (out.size() % 4 != 0) {
        out.push_back(0);
    }
}

std::vector<uint8_t> buildFirebirdErrorResponsePacket(uint32_t gds_code,
                                                      const std::string& sqlstate) {
    // op_response + handle + object_id + data_buffer(0) + status vector
    std::vector<uint8_t> packet;
    appendBe32(packet, 9);  // op_response
    appendBe32(packet, 0);  // handle
    appendBe64(packet, 0);  // object_id
    appendBe32(packet, 0);  // data length

    appendBe32(packet, 1);  // isc_arg_gds
    appendBe32(packet, gds_code);
    if (!sqlstate.empty()) {
        appendBe32(packet, 19);  // isc_arg_sql_state
        appendXdrBuffer(packet, sqlstate);
    }
    appendBe32(packet, 0);  // isc_arg_end
    return packet;
}

}  // namespace

TEST(FirebirdParserBoundaryContractTest, RejectAcceptPack) {
    cleanupDb("fb_boundary_contract.sbdb");
    CompileHarness<scratchbird::protocol::FirebirdAdapter> adapter(
        makeAdapterConfig("fb_boundary_contract.sbdb"));

    scratchbird::core::ErrorContext schema_ctx;
    adapter.applyFirebirdSessionSchemaContextForTest(&schema_ctx);
    ASSERT_TRUE(schema_ctx.message.empty()) << schema_ctx.message;
    ASSERT_NE(adapter.connectionContextForTest(), nullptr);

    std::string error;
    EXPECT_EQ(scratchbird::core::Status::OK, compileSql(adapter, "SELECT 1 FROM RDB$DATABASE", error))
        << error;

    error.clear();
    EXPECT_NE(scratchbird::core::Status::OK, compileSql(adapter, "SET TERM ^", error))
        << "Unexpected acceptance: " << error;

    error.clear();
    EXPECT_NE(scratchbird::core::Status::OK, compileSql(adapter, "DECLARE VARIABLE X INTEGER", error))
        << "Unexpected acceptance: " << error;

    error.clear();
    EXPECT_NE(scratchbird::core::Status::OK, compileSql(adapter, "BEGIN END", error))
        << "Unexpected acceptance: " << error;
}

TEST(FirebirdParserBoundaryContractTest, IsolationContextQueryUsesRealCatalogAndExecutorPath) {
    cleanupDb("fb_boundary_context.sbdb");
    CompileHarness<scratchbird::protocol::FirebirdAdapter> adapter(
        makeAdapterConfig("fb_boundary_context.sbdb"));

    scratchbird::core::ErrorContext schema_ctx;
    adapter.applyFirebirdSessionSchemaContextForTest(&schema_ctx);
    ASSERT_TRUE(schema_ctx.message.empty()) << schema_ctx.message;

    std::vector<uint8_t> bytecode;
    std::string error;
    ASSERT_EQ(
        scratchbird::core::Status::OK,
        adapter.runCompile(
            "SELECT TRIM(RDB$GET_CONTEXT('SYSTEM', 'ISOLATION_LEVEL')) FROM RDB$DATABASE",
            bytecode,
            error))
        << error;

    auto* db = adapter.engineDatabaseForTest();
    ASSERT_NE(db, nullptr);
    auto* conn = adapter.connectionContextForTest();
    ASSERT_NE(conn, nullptr);

    auto* catalog = db->catalog_manager();
    ASSERT_NE(catalog, nullptr);
    scratchbird::core::CatalogManager::ViewInfo view_info;
    scratchbird::core::ErrorContext view_ctx;
    EXPECT_EQ(catalog->getView(conn->getCurrentSchemaId(), "RDB$DATABASE", view_info, &view_ctx),
              scratchbird::core::Status::OK)
        << view_ctx.message;

    scratchbird::core::CatalogManager::TableInfo table_info;
    scratchbird::core::ErrorContext table_ctx;
    EXPECT_NE(catalog->getTable(conn->getCurrentSchemaId(), "RDB$DATABASE", table_info, &table_ctx),
              scratchbird::core::Status::OK)
        << "RDB$DATABASE should resolve through the Firebird catalog overlay view, not a heap table";

    scratchbird::sblr::Executor executor(db);
    executor.setConnectionContext(conn);
    executor.setCurrentSchema(conn->getCurrentSchemaId());
    auto result = executor.execute(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    ASSERT_EQ(rs->rowCount(), 1u);
    ASSERT_EQ(rs->columnCount(), 1u);
    const auto isolation_text = [&]() -> std::string {
        switch (conn->getIsolationLevel()) {
            case scratchbird::core::IsolationLevel::SNAPSHOT_TABLE_STABILITY:
                return "SNAPSHOT TABLE STABILITY";
            case scratchbird::core::IsolationLevel::SNAPSHOT:
                return "SNAPSHOT";
            case scratchbird::core::IsolationLevel::READ_COMMITTED_READ_CONSISTENCY:
                return "READ COMMITTED READ CONSISTENCY";
            case scratchbird::core::IsolationLevel::READ_COMMITTED:
            default:
                if (conn->getReadCommittedMode() ==
                    scratchbird::core::ReadCommittedMode::READ_CONSISTENCY) {
                    return "READ COMMITTED READ CONSISTENCY";
                }
                return "READ COMMITTED";
        }
    }();
    EXPECT_EQ(rs->getValue(0, 0).toString(), isolation_text);

    bytecode.clear();
    error.clear();
    ASSERT_EQ(scratchbird::core::Status::OK,
              adapter.runCompile("SELECT 1 FROM RDB$DATABASE", bytecode, error))
        << error;
    auto singleton_result = executor.execute(bytecode);
    ASSERT_TRUE(singleton_result.success()) << singleton_result.error();
    ASSERT_TRUE(singleton_result.hasResultSet());
    ASSERT_EQ(singleton_result.resultSet()->rowCount(), 1u);
    ASSERT_EQ(singleton_result.resultSet()->columnCount(), 1u);
    EXPECT_EQ(singleton_result.resultSet()->getValue(0, 0).toInt64(), 1);

    bytecode.clear();
    error.clear();
    ASSERT_EQ(scratchbird::core::Status::OK,
              adapter.runCompile("SELECT COUNT(*) FROM RDB$DATABASE", bytecode, error))
        << error;
    auto count_result = executor.execute(bytecode);
    ASSERT_TRUE(count_result.success()) << count_result.error();
    ASSERT_TRUE(count_result.hasResultSet());
    ASSERT_EQ(count_result.resultSet()->rowCount(), 1u);
    EXPECT_EQ(count_result.resultSet()->getValue(0, 0).toString(), "1");
}

TEST(FirebirdParserBoundaryContractTest, TrimCastIsolationLiteralCompilesThroughFirebirdDialect) {
    cleanupDb("fb_boundary_trim_cast.sbdb");
    CompileHarness<scratchbird::protocol::FirebirdAdapter> adapter(
        makeAdapterConfig("fb_boundary_trim_cast.sbdb"));

    scratchbird::core::ErrorContext schema_ctx;
    adapter.applyFirebirdSessionSchemaContextForTest(&schema_ctx);
    ASSERT_TRUE(schema_ctx.message.empty()) << schema_ctx.message;

    std::string error;
    EXPECT_EQ(
        scratchbird::core::Status::OK,
        compileSql(adapter,
                   "SELECT TRIM(CAST('READ COMMITTED' AS CHAR(64))) FROM RDB$DATABASE",
                   error))
        << error;
}

TEST(FirebirdParserBoundaryContractTest, RelationFieldSourceIdentitiesPreservePerRelationTypes) {
    cleanupDb("fb_boundary_field_sources.sbdb");
    CompileHarness<scratchbird::protocol::FirebirdAdapter> adapter(
        makeAdapterConfig("fb_boundary_field_sources.sbdb"));

    scratchbird::core::ErrorContext schema_ctx;
    adapter.applyFirebirdSessionSchemaContextForTest(&schema_ctx);
    ASSERT_TRUE(schema_ctx.message.empty()) << schema_ctx.message;

    auto* db = adapter.engineDatabaseForTest();
    auto* conn = adapter.connectionContextForTest();
    ASSERT_NE(db, nullptr);
    ASSERT_NE(conn, nullptr);

    scratchbird::sblr::Executor executor(db);
    executor.setConnectionContext(conn);
    executor.setCurrentSchema(conn->getCurrentSchemaId());

    auto execute_sql = [&](const std::string& sql) {
        std::vector<uint8_t> bytecode;
        std::string error;
        ASSERT_EQ(adapter.runCompile(sql, bytecode, error), scratchbird::core::Status::OK)
            << error;
        auto result = executor.execute(bytecode);
        ASSERT_TRUE(result.success()) << result.error();
    };

    execute_sql("CREATE TABLE fb_metric_int(id INT PRIMARY KEY, metric_value INT NOT NULL)");
    execute_sql(
        "CREATE TABLE fb_metric_text(id INT PRIMARY KEY, metric_value VARCHAR(20) NOT NULL)");

    scratchbird::catalog::FirebirdCatalogHandler catalog_handler(db->catalog_manager());
    scratchbird::core::ErrorContext catalog_ctx;

    scratchbird::catalog::VirtualResultSet field_result;
    ASSERT_EQ(catalog_handler.queryTable("", "RDB$FIELDS", "", field_result, &catalog_ctx),
              scratchbird::core::Status::OK)
        << catalog_ctx.message;

    scratchbird::catalog::VirtualResultSet relation_field_result;
    ASSERT_EQ(catalog_handler.queryTable("",
                                         "RDB$RELATION_FIELDS",
                                         "",
                                         relation_field_result,
                                         &catalog_ctx),
              scratchbird::core::Status::OK)
        << catalog_ctx.message;

    auto find_field_row =
        [&](const std::string& field_source_name) -> const scratchbird::catalog::VirtualRow* {
            for (const auto& row : field_result.rows) {
                const auto* field_name = row.getColumn("RDB$FIELD_NAME");
                if (field_name != nullptr && field_name->toString() == field_source_name) {
                    return &row;
                }
            }
            return nullptr;
        };

    auto find_relation_field_source =
        [&](const std::string& relation_name) -> std::string {
            const std::string stored_relation_name = firebirdStoredIdentifier(relation_name);
            const std::string stored_field_name = firebirdStoredIdentifier("metric_value");
            for (const auto& row : relation_field_result.rows) {
                const auto* rel = row.getColumn("RDB$RELATION_NAME");
                const auto* field = row.getColumn("RDB$FIELD_NAME");
                const auto* source = row.getColumn("RDB$FIELD_SOURCE");
                if (rel == nullptr || field == nullptr || source == nullptr) {
                    continue;
                }
                if (rel->toString() == stored_relation_name &&
                    field->toString() == stored_field_name) {
                    return source->toString();
                }
            }
            return {};
        };

    const std::string int_field_source = find_relation_field_source("fb_metric_int");
    const std::string text_field_source = find_relation_field_source("fb_metric_text");
    ASSERT_FALSE(int_field_source.empty());
    ASSERT_FALSE(text_field_source.empty());
    EXPECT_NE(int_field_source, text_field_source);

    const auto* int_field_row = find_field_row(int_field_source);
    const auto* text_field_row = find_field_row(text_field_source);
    ASSERT_NE(int_field_row, nullptr);
    ASSERT_NE(text_field_row, nullptr);

    const auto* int_field_type = int_field_row->getColumn("RDB$FIELD_TYPE");
    const auto* int_field_length = int_field_row->getColumn("RDB$FIELD_LENGTH");
    const auto* text_field_type = text_field_row->getColumn("RDB$FIELD_TYPE");
    const auto* text_field_length = text_field_row->getColumn("RDB$FIELD_LENGTH");
    ASSERT_NE(int_field_type, nullptr);
    ASSERT_NE(int_field_length, nullptr);
    ASSERT_NE(text_field_type, nullptr);
    ASSERT_NE(text_field_length, nullptr);

    EXPECT_EQ(int_field_type->getInt64(), 8);
    EXPECT_EQ(int_field_length->getInt64(), 4);
    EXPECT_EQ(text_field_type->getInt64(), 37);
    EXPECT_EQ(text_field_length->getInt64(), 20);
}

TEST(FirebirdParserBoundaryContractTest, ErrorSqlStateTranslationDeterminism) {
    FirebirdParserAgentHarness agent(makeParserAgentConfig());

    EXPECT_EQ("335544472", agent.mapSQLStateToProtocol("28000"));

    std::array<char, 6> out{};
    const std::vector<uint8_t> wire = buildFirebirdErrorResponsePacket(335544472u, "28000");
    agent.mapProtocolErrorToSQLState(wire, out.data());
    EXPECT_STREQ("28000", out.data());
}
