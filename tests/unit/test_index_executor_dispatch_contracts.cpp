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

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "scratchbird/core/database.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "test_helpers.h"

using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::sblr::ExecutionResult;
using scratchbird::sblr::Executor;
using scratchbird::sblr::QueryCompilerV3;
using scratchbird::sblr::v3::Container;
using scratchbird::sblr::v3::DecodeError;
using scratchbird::sblr::v3::Instruction;
using scratchbird::sblr::v3::Opcode;
using scratchbird::sblr::v3::Value;
using scratchbird::testing::TestDatabaseFile;

namespace {
namespace core = scratchbird::core;

class IndexExecutorDispatchContractsTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("index_executor_dispatch_contracts");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
        executor_ = std::make_unique<Executor>(db_.get());

        core::CatalogManager::SchemaInfo public_schema_info;
        ASSERT_EQ(db_->catalog_manager()->getSchema("public", public_schema_info, &ctx), Status::OK)
            << ctx.message;
        compiler_->setCurrentSchema(public_schema_info.schema_id);
        executor_->setCurrentSchema(public_schema_info.schema_id);

        auto create_users = executeSql("CREATE TABLE users (id INT, name TEXT)");
        ASSERT_TRUE(create_users.success()) << create_users.error();
        auto create_idx = executeSql("CREATE INDEX idx_users_id ON users USING BTREE (id)");
        ASSERT_TRUE(create_idx.success()) << create_idx.error();
        auto create_vectors = executeSql("CREATE TABLE vectors (id INT, embedding VECTOR(3))");
        ASSERT_TRUE(create_vectors.success()) << create_vectors.error();
    }

    std::vector<uint8_t> compileSql(const std::string& sql)
    {
        auto compile_result = compiler_->compile(sql);
        EXPECT_TRUE(compile_result.success()) << sql;
        if (!compile_result.success())
        {
            return {};
        }
        return compile_result.bytecode();
    }

    ExecutionResult executeSql(const std::string& sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            std::string compile_error = "Compilation failed: " + sql;
            const auto& errors = compile_result.errors();
            if (!errors.empty())
            {
                compile_error += " :: " + errors.front();
            }
            return ExecutionResult(std::move(compile_error));
        }
        return executor_->execute(compile_result.bytecode());
    }

    ExecutionResult executeBytecode(const std::vector<uint8_t>& bytecode)
    {
        return executor_->execute(bytecode);
    }

    static std::vector<uint8_t> mutateV3InstructionPayload(
        const std::vector<uint8_t>& bytecode,
        Opcode opcode,
        const std::function<void(Value::Object&)>& mutator)
    {
        Container container;
        std::string container_err;
        if (!scratchbird::sblr::v3::decodeContainer(
                bytecode.data(), bytecode.size(), container, container_err))
        {
            return {};
        }

        std::vector<Instruction> instructions;
        size_t offset = 0;
        DecodeError decode_err;
        while (offset < container.bytecode_stream.size())
        {
            Instruction inst;
            if (!scratchbird::sblr::v3::decodeInstructionWithSchema(
                    container.bytecode_stream.data(),
                    container.bytecode_stream.size(),
                    offset,
                    inst,
                    decode_err))
            {
                return {};
            }
            instructions.push_back(std::move(inst));
        }

        bool mutated = false;
        for (auto& inst : instructions)
        {
            if (inst.opcode != static_cast<uint16_t>(opcode))
            {
                continue;
            }
            auto* payload_obj = std::get_if<Value::Object>(&inst.payload.data);
            if (!payload_obj)
            {
                return {};
            }
            mutator(*payload_obj);
            mutated = true;
            break;
        }
        if (!mutated)
        {
            return {};
        }

        scratchbird::sblr::v3::Buffer stream;
        for (const auto& inst : instructions)
        {
            if (!scratchbird::sblr::v3::encodeInstructionWithSchema(inst, stream, decode_err))
            {
                return {};
            }
        }
        container.bytecode_stream = std::move(stream);

        std::vector<uint8_t> mutated_out;
        if (!scratchbird::sblr::v3::encodeContainer(container, mutated_out, container_err))
        {
            return {};
        }
        return mutated_out;
    }

    static void appendLegacyUVarint(std::vector<uint8_t>& bytecode, uint64_t value)
    {
        size_t offset = bytecode.size();
        bytecode.resize(offset + 10);
        size_t count = scratchbird::sblr::writeUVarint(&bytecode[offset], value);
        bytecode.resize(offset + count);
    }

    static void appendLegacyInt32(std::vector<uint8_t>& bytecode, uint32_t value)
    {
        size_t offset = bytecode.size();
        bytecode.resize(offset + 4);
        scratchbird::sblr::writeInt32(&bytecode[offset], value);
    }

    static void appendLegacyString(std::vector<uint8_t>& bytecode, const std::string& value)
    {
        appendLegacyUVarint(bytecode, static_cast<uint64_t>(value.size()));
        bytecode.insert(bytecode.end(), value.begin(), value.end());
    }

    static std::vector<uint8_t> buildLegacyCreateIndexBytecode(uint8_t index_type_byte)
    {
        std::vector<uint8_t> bytecode;
        bytecode.push_back(static_cast<uint8_t>(scratchbird::sblr::Opcode::VERSION));
        bytecode.push_back(static_cast<uint8_t>(scratchbird::sblr::SBLR_VERSION));
        bytecode.push_back(static_cast<uint8_t>(scratchbird::sblr::Opcode::CREATE_INDEX));
        appendLegacyString(bytecode, "idx_legacy_invalid");
        appendLegacyString(bytecode, "users");
        bytecode.push_back(0); // is_unique
        appendLegacyInt32(bytecode, 1); // column_count
        appendLegacyString(bytecode, "id");
        appendLegacyInt32(bytecode, 0); // include_count
        appendLegacyString(bytecode, ""); // tablespace_name
        bytecode.push_back(index_type_byte);
        appendLegacyInt32(bytecode, 0); // options_flags
        bytecode.push_back(0); // has_expressions
        bytecode.push_back(0); // has_predicate
        return bytecode;
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<Executor> executor_;
};

TEST_F(IndexExecutorDispatchContractsTest, AlterIndexSetOptionsRoutesThroughV3Dispatch)
{
    ExecutionResult result =
        executeSql("ALTER INDEX public.idx_users_id SET (bloom_filter = true, bloom_fpr = 0.05)");
    EXPECT_EQ(result.error().find("V3 opcode not implemented in executor"), std::string::npos)
        << result.error();
    EXPECT_EQ(result.error().find("Unsupported DDL mutation opcode"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, AnalyzeTableExecutesThroughV3Dispatch)
{
    ExecutionResult result = executeSql("ANALYZE users");
    ASSERT_TRUE(result.success()) << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, AnalyzeIndexExecutesThroughV3DispatchAndUpdatesStatsCatalog)
{
    ExecutionResult result = executeSql("ANALYZE INDEX public.idx_users_id WITH (sample_rate = 0.25)");
    ASSERT_TRUE(result.success()) << result.error();

    core::ErrorContext ctx;
    core::CatalogManager::SchemaInfo schema_info;
    ASSERT_EQ(db_->catalog_manager()->getSchema("public", schema_info, &ctx), Status::OK)
        << ctx.message;

    core::CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(schema_info.schema_id, "users", table_info, &ctx), Status::OK)
        << ctx.message;

    core::CatalogManager::IndexInfo index_info;
    ASSERT_EQ(db_->catalog_manager()->getIndex(table_info.table_id, "idx_users_id", index_info, &ctx), Status::OK)
        << ctx.message;

    core::CatalogManager::IndexStatsCatalogInfo stats_info;
    ASSERT_EQ(db_->catalog_manager()->getIndexStatsCatalogEntry(index_info.index_id, stats_info, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_GE(stats_info.stats_version, 1u);
    EXPECT_TRUE(stats_info.is_valid);
}

TEST_F(IndexExecutorDispatchContractsTest, AlterIndexRebuildTransitionsMaintenanceStateToComplete)
{
    ExecutionResult result = executeSql(
        "ALTER INDEX idx_users_id REBUILD ONLINE "
        "WITH (target_fillfactor = 90, throttle_ms = 5)");
    ASSERT_TRUE(result.success()) << result.error();

    core::ErrorContext ctx;
    core::CatalogManager::SchemaInfo schema_info;
    ASSERT_EQ(db_->catalog_manager()->getSchema("public", schema_info, &ctx), Status::OK)
        << ctx.message;

    core::CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(schema_info.schema_id, "users", table_info, &ctx), Status::OK)
        << ctx.message;

    core::CatalogManager::IndexInfo index_info;
    ASSERT_EQ(db_->catalog_manager()->getIndex(table_info.table_id, "idx_users_id", index_info, &ctx), Status::OK)
        << ctx.message;

    std::vector<core::CatalogManager::IndexMaintenanceCatalogInfo> maintenance_rows;
    ASSERT_EQ(db_->catalog_manager()->listIndexMaintenanceCatalogEntries(
                  index_info.index_id, maintenance_rows, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(maintenance_rows.empty());

    const core::CatalogManager::IndexMaintenanceCatalogInfo* latest = &maintenance_rows.front();
    for (const auto& row : maintenance_rows)
    {
        if (row.started_time >= latest->started_time)
        {
            latest = &row;
        }
    }

    EXPECT_EQ(latest->maintenance_kind, core::CatalogManager::IndexMaintenanceKind::REBUILD);
    EXPECT_EQ(latest->maintenance_mode, core::CatalogManager::IndexMaintenanceMode::ONLINE);
    EXPECT_EQ(latest->maintenance_state, core::CatalogManager::IndexMaintenanceState::COMPLETE);
    EXPECT_TRUE(latest->has_target_fillfactor);
    EXPECT_EQ(latest->target_fillfactor, 90);
}

TEST_F(IndexExecutorDispatchContractsTest, OnlineMaintenanceCapturesInsertDelta)
{
    core::ErrorContext ctx;
    core::CatalogManager::SchemaInfo schema_info;
    ASSERT_EQ(db_->catalog_manager()->getSchema("public", schema_info, &ctx), Status::OK)
        << ctx.message;

    core::CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(schema_info.schema_id, "users", table_info, &ctx), Status::OK)
        << ctx.message;

    core::CatalogManager::IndexInfo index_info;
    ASSERT_EQ(db_->catalog_manager()->getIndex(table_info.table_id, "idx_users_id", index_info, &ctx), Status::OK)
        << ctx.message;

    core::CatalogManager::IndexMaintenanceCatalogInfo maintenance{};
    maintenance.index_id = index_info.index_id;
    maintenance.maintenance_kind = core::CatalogManager::IndexMaintenanceKind::REBUILD;
    maintenance.maintenance_mode = core::CatalogManager::IndexMaintenanceMode::ONLINE;
    maintenance.maintenance_state = core::CatalogManager::IndexMaintenanceState::BUILDING_SHADOW;
    maintenance.started_txid = 1;
    core::ID maintenance_id{};
    ASSERT_EQ(db_->catalog_manager()->upsertIndexMaintenanceCatalogEntry(maintenance, maintenance_id, &ctx),
              Status::OK)
        << ctx.message;

    ExecutionResult result = executeSql("INSERT INTO users (id, name) VALUES (1, 'alice')");
    ASSERT_TRUE(result.success()) << result.error();

    std::vector<core::CatalogManager::IndexMaintenanceDeltaCatalogInfo> deltas;
    ASSERT_EQ(db_->catalog_manager()->listIndexMaintenanceDeltaCatalogEntries(maintenance_id, deltas, &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(deltas.empty());

    bool found_insert = false;
    for (const auto& delta : deltas)
    {
        if (delta.delta_op == core::CatalogManager::IndexDeltaOp::INSERT)
        {
            found_insert = true;
            break;
        }
    }
    EXPECT_TRUE(found_insert);
    EXPECT_GT(deltas.front().commit_txid, 0u);
}

TEST_F(IndexExecutorDispatchContractsTest, AlterIndexRebalanceRoutesThroughMaintenanceStateMachine)
{
    ExecutionResult result = executeSql(
        "ALTER INDEX idx_users_id REBALANCE OFFLINE WITH (target_fillfactor = 85)");
    ASSERT_TRUE(result.success())
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, AlterIndexLightScanPopulatesHealthCatalog)
{
    ExecutionResult result = executeSql("ALTER INDEX idx_users_id LIGHT SCAN");
    ASSERT_TRUE(result.success()) << result.error();

    core::ErrorContext ctx;
    core::CatalogManager::SchemaInfo schema_info;
    ASSERT_EQ(db_->catalog_manager()->getSchema("public", schema_info, &ctx), Status::OK)
        << ctx.message;

    core::CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(schema_info.schema_id, "users", table_info, &ctx), Status::OK)
        << ctx.message;

    core::CatalogManager::IndexInfo index_info;
    ASSERT_EQ(db_->catalog_manager()->getIndex(table_info.table_id, "idx_users_id", index_info, &ctx), Status::OK)
        << ctx.message;

    core::CatalogManager::IndexHealthCatalogInfo health_info;
    ASSERT_EQ(db_->catalog_manager()->getIndexHealthCatalogEntry(index_info.index_id, health_info, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_GT(health_info.last_light_scan_time, 0u);
    EXPECT_EQ(health_info.light_status, core::CatalogManager::IndexHealthStatus::WARNING);
    EXPECT_EQ(health_info.light_error_count, 1u);
    EXPECT_GE(health_info.pages_scanned, 1u);
}

TEST_F(IndexExecutorDispatchContractsTest, AlterIndexDiagnosticScanDetectsChecksumCorruption)
{
    core::ErrorContext ctx;
    core::CatalogManager::SchemaInfo schema_info;
    ASSERT_EQ(db_->catalog_manager()->getSchema("public", schema_info, &ctx), Status::OK)
        << ctx.message;

    core::CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(schema_info.schema_id, "users", table_info, &ctx), Status::OK)
        << ctx.message;

    core::CatalogManager::IndexInfo index_info;
    ASSERT_EQ(db_->catalog_manager()->getIndex(table_info.table_id, "idx_users_id", index_info, &ctx), Status::OK)
        << ctx.message;

    void* page_buffer = nullptr;
    ASSERT_EQ(db_->buffer_pool()->pinPageGlobal(index_info.root_gpid, &page_buffer, &ctx), Status::OK)
        << ctx.message;
    auto* page_bytes = static_cast<uint8_t*>(page_buffer);
    auto* header = reinterpret_cast<core::PageHeader*>(page_bytes);
    header->flags |= core::PAGE_FLAG_CHECKSUM_VALID;
    header->checksum = core::calculatePageChecksum(page_bytes, db_->page_size()) ^ 0xFFFFFFFFu;
    page_bytes[512] ^= 0x5Au;
    ASSERT_EQ(db_->buffer_pool()->unpinPageGlobal(index_info.root_gpid, true, &ctx), Status::OK)
        << ctx.message;

    ExecutionResult result = executeSql("ALTER INDEX idx_users_id DIAGNOSTIC SCAN");
    ASSERT_TRUE(result.success()) << result.error();

    core::CatalogManager::IndexHealthCatalogInfo health_info;
    ASSERT_EQ(db_->catalog_manager()->getIndexHealthCatalogEntry(index_info.index_id, health_info, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_GT(health_info.last_diag_scan_time, 0u);
    EXPECT_EQ(health_info.diagnostic_status, core::CatalogManager::IndexHealthStatus::CORRUPT);
    EXPECT_GT(health_info.diagnostic_error_count, 0u);
    EXPECT_GT(health_info.checksum_errors, 0u);
}

TEST_F(IndexExecutorDispatchContractsTest, AlterIndexLightScanUpdatesUsageMetrics)
{
    core::ErrorContext ctx;
    core::CatalogManager::SchemaInfo schema_info;
    ASSERT_EQ(db_->catalog_manager()->getSchema("public", schema_info, &ctx), Status::OK)
        << ctx.message;

    core::CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(schema_info.schema_id, "users", table_info, &ctx), Status::OK)
        << ctx.message;

    core::CatalogManager::IndexInfo index_info;
    ASSERT_EQ(db_->catalog_manager()->getIndex(table_info.table_id, "idx_users_id", index_info, &ctx), Status::OK)
        << ctx.message;

    ExecutionResult result = executeSql("ALTER INDEX idx_users_id LIGHT SCAN");
    ASSERT_TRUE(result.success()) << result.error();

    core::CatalogManager::IndexUsageCatalogInfo usage_info;
    ASSERT_EQ(db_->catalog_manager()->getIndexUsageCatalogEntry(index_info.index_id, usage_info, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_GE(usage_info.scan_count, 1u);
    EXPECT_GE(usage_info.blocks_read, 1u);
    EXPECT_GT(usage_info.total_time_ns, 0u);
    EXPECT_GT(usage_info.last_used_time, 0u);
}

TEST_F(IndexExecutorDispatchContractsTest, AlterIndexMaintenanceUpdatesStorageMetrics)
{
    core::ErrorContext ctx;
    core::CatalogManager::SchemaInfo schema_info;
    ASSERT_EQ(db_->catalog_manager()->getSchema("public", schema_info, &ctx), Status::OK)
        << ctx.message;

    core::CatalogManager::TableInfo table_info;
    ASSERT_EQ(db_->catalog_manager()->getTable(schema_info.schema_id, "users", table_info, &ctx), Status::OK)
        << ctx.message;

    core::CatalogManager::IndexInfo index_info;
    ASSERT_EQ(db_->catalog_manager()->getIndex(table_info.table_id, "idx_users_id", index_info, &ctx), Status::OK)
        << ctx.message;

    ExecutionResult result = executeSql("ALTER INDEX idx_users_id REBUILD OFFLINE");
    ASSERT_TRUE(result.success()) << result.error();

    core::CatalogManager::IndexStorageCatalogInfo storage_info;
    ASSERT_EQ(db_->catalog_manager()->getIndexStorageCatalogEntry(index_info.index_id, storage_info, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_GE(storage_info.page_count, 1u);
    EXPECT_GE(storage_info.bytes_allocated, static_cast<uint64_t>(db_->page_size()));
    EXPECT_GT(storage_info.bytes_used, 0u);
    EXPECT_LE(storage_info.fragmentation_ratio, 1.0f);
    EXPECT_GE(storage_info.fragmentation_ratio, 0.0f);
}

TEST_F(IndexExecutorDispatchContractsTest, ShowIndexReportingProfilesReturnDeterministicShapes)
{
    ASSERT_TRUE(executeSql("ALTER INDEX idx_users_id LIGHT SCAN").success());
    ASSERT_TRUE(executeSql("ALTER INDEX idx_users_id REBUILD OFFLINE").success());
    ASSERT_TRUE(executeSql("ANALYZE INDEX idx_users_id WITH (sample_rate = 0.5)").success());

    struct ShowContract {
        const char* sql;
        std::vector<std::string> columns;
        bool expect_multiple_rows = false;
    };

    const std::vector<ShowContract> contracts = {
        {"SHOW INDEX HEALTH idx_users_id",
         {"Index_Name",
          "Index_ID",
          "Light_Status",
          "Light_Error_Count",
          "Last_Light_Scan_Txid",
          "Last_Light_Scan_Time",
          "Diagnostic_Status",
          "Diagnostic_Error_Count",
          "Last_Diagnostic_Scan_Txid",
          "Last_Diagnostic_Scan_Time",
          "Checksum_Errors",
          "Order_Errors",
          "Pointer_Errors",
          "Orphan_Pages",
          "Duplicate_Keys",
          "In_Memory_Errors",
          "Pages_Scanned",
          "Bytes_Scanned"}},
        {"SHOW INDEX USAGE idx_users_id",
         {"Index_Name",
          "Index_ID",
          "Scan_Count",
          "Tuple_Read",
          "Tuple_Returned",
          "Index_Only_Hits",
          "Blocks_Read",
          "Blocks_Hit",
          "Total_Time_Ns",
          "Last_Used_Time"}},
        {"SHOW INDEX STORAGE idx_users_id",
         {"Index_Name",
          "Index_ID",
          "Page_Count",
          "Bytes_Used",
          "Bytes_Allocated",
          "Fragmentation_Ratio",
          "Filespace_ID"}},
        {"SHOW INDEX CONTENTION idx_users_id",
         {"Index_Name",
          "Index_ID",
          "Lock_Wait_Count",
          "Lock_Wait_Time_Ns",
          "Deadlock_Count",
          "Latch_Wait_Count",
          "Latch_Wait_Time_Ns",
          "Unique_Key_Conflict_Count",
          "Hot_Key_Count"}},
        {"SHOW INDEX OPTIONS idx_users_id",
         {"Index_Name",
          "Index_ID",
          "Option_Key",
          "Option_Value",
          "Option_Type",
          "Option_Source"},
         true},
    };

    for (const auto& contract : contracts)
    {
        ExecutionResult show_result = executeSql(contract.sql);
        ASSERT_TRUE(show_result.success()) << contract.sql << ": " << show_result.error();
        ASSERT_TRUE(show_result.hasResultSet()) << contract.sql;

        auto* rs = show_result.resultSet();
        ASSERT_NE(rs, nullptr);
        ASSERT_EQ(rs->columnCount(), contract.columns.size()) << contract.sql;
        for (size_t col = 0; col < contract.columns.size(); ++col)
        {
            EXPECT_EQ(rs->columnName(col), contract.columns[col]) << contract.sql;
        }
        if (contract.expect_multiple_rows)
        {
            EXPECT_GE(rs->rowCount(), 2u) << contract.sql;
        }
        else
        {
            EXPECT_EQ(rs->rowCount(), 1u) << contract.sql;
        }
        ASSERT_GE(rs->rowCount(), 1u) << contract.sql;
        EXPECT_EQ(rs->getValue(0, 0).toString(), "idx_users_id") << contract.sql;
    }
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsUnknownIndexTypeFromPayload)
{
    auto bytecode = compileSql("CREATE INDEX idx_users_runtime_bad ON users USING BTREE (id)");
    ASSERT_FALSE(bytecode.empty());

    auto mutated = mutateV3InstructionPayload(
        bytecode,
        Opcode::SBLR3_CREATE_INDEX,
        [](Value::Object& payload) {
            payload["index_type"] = Value(std::string("NOT_A_REAL_TYPE"));
        });
    ASSERT_FALSE(mutated.empty());

    ExecutionResult result = executeBytecode(mutated);
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("CREATE INDEX unsupported index_type: NOT_A_REAL_TYPE"),
              std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRoutesIvfSq8HybridThroughVectorFamilyPath)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_vectors_hybrid ON vectors USING IVF_SQ8_HYBRID (embedding) "
        "WITH (metric = 'l2', nlist = 8, nprobe = 4, sq_bits = 8, gpu_search_threshold = 32)");
    ASSERT_FALSE(result.success());
    EXPECT_EQ(result.error().find("CREATE INDEX unsupported index_type"), std::string::npos)
        << result.error();
    EXPECT_NE(result.error().find("Vector column has no dimensions specified"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsInvalidPqBitsDeterministically)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_vectors_pq_bad ON vectors USING IVF_PQ (embedding) "
        "WITH (pq_m = 8, pq_bits = 6)");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("PQ_BITS/BITS_PER_CODE supports only values 4 or 8"),
              std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsMissingPqMDeterministically)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_vectors_pq_missing_m ON vectors USING IVF_PQ (embedding) "
        "WITH (pq_bits = 8)");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("missing required option: M"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsInvalidBinaryVectorMetric)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_vectors_bin_bad ON vectors USING VECTOR_BIN_FLAT (embedding) "
        "WITH (metric = 'cosine')");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("METRIC invalid for binary vector family"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsUnsupportedVectorFamilyOption)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_vectors_bad_opt ON vectors USING IVF_SQ8_HYBRID (embedding) "
        "WITH (foo = 1)");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("unsupported option for IVF_SQ8_HYBRID: FOO"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRoutesAnnoyThroughAdvancedAnnPath)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_vectors_annoy ON vectors USING ANNOY (embedding) "
        "WITH (n_trees = 32, leaf_size = 64, search_k = 2048, metric = 'cosine')");
    ASSERT_FALSE(result.success());
    EXPECT_EQ(result.error().find("CREATE INDEX unsupported index_type"), std::string::npos)
        << result.error();
    EXPECT_EQ(result.error().find("unsupported option for ANNOY"), std::string::npos)
        << result.error();
    EXPECT_NE(result.error().find("Vector column has no dimensions specified"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsScannInvalidQuantizerDeterministically)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_vectors_scann_bad ON vectors USING SCANN (embedding) "
        "WITH (quantizer = 'invalid')");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("CREATE INDEX option QUANTIZER invalid for SCANN"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsDiskannInvalidEntrypointStrategy)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_vectors_diskann_bad ON vectors USING DISKANN (embedding) "
        "WITH (entrypoint_strategy = 'random')");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("CREATE INDEX option ENTRYPOINT_STRATEGY invalid for DISKANN"),
              std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsGpuCagraNonBooleanFallbackCpu)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_vectors_cagra_bad ON vectors USING GPU_CAGRA (embedding) "
        "WITH (fallback_cpu = 1)");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("CREATE INDEX option FALLBACK_CPU expects boolean value"),
              std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexAcceptsArtTypeWithoutUnsupportedTypeError)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_users_name_art ON users USING ART (name)");
    EXPECT_EQ(result.error().find("CREATE INDEX unsupported index_type"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexAcceptsInvertedTypeWithoutUnsupportedTypeError)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_users_name_inverted ON users USING INVERTED (name)");
    EXPECT_EQ(result.error().find("CREATE INDEX unsupported index_type"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexAcceptsStlSortTypeWithoutUnsupportedTypeError)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_users_id_stlsort ON users USING STL_SORT (id)");
    EXPECT_EQ(result.error().find("CREATE INDEX unsupported index_type"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsNgramInvalidAnalyzer)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_users_name_ngram_bad ON users USING NGRAM (name) "
        "WITH (analyzer = 'broken')");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("CREATE INDEX option ANALYZER invalid for NGRAM"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsSparseInvertedInvalidNorm)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_users_name_sparse_bad ON users USING SPARSE_INVERTED (name) "
        "WITH (norm = 'l3')");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("CREATE INDEX option NORM invalid for SPARSE_INVERTED"),
              std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsMinHashBandProductMismatch)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_users_name_minhash_bad ON users USING MINHASH_LSH (name) "
        "WITH (num_perm = 64, band_count = 8, rows_per_band = 7)");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("CREATE INDEX option NUM_PERM must equal BAND_COUNT * ROWS_PER_BAND"),
              std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsTrieCaseFoldNonBoolean)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_users_name_trie_bad ON users USING TRIE (name) "
        "WITH (case_fold = 1)");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("CREATE INDEX option CASE_FOLD expects boolean value"),
              std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsMongo2dUnknownOptionDeterministically)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_users_mongo2d_bad ON users USING MONGODB_2D (id) "
        "WITH (foo = 1)");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("unsupported option for MONGODB_2D: FOO"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsMongoGeoHaystackMissingSecondaryKey)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_users_mongo_hay_bad ON users USING MONGODB_GEO_HAYSTACK (id) "
        "WITH (bucket_size = 1.0)");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("missing required option: SECONDARY_KEY"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsMongoEncryptedRangeInvalidTokenHmac)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_users_mongo_enc_bad ON users USING MONGODB_ENCRYPTED_RANGE (id) "
        "WITH (range_bits = 32, token_levels = 2, token_hmac = 'sha1')");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("TOKEN_HMAC invalid for MONGODB_ENCRYPTED_RANGE"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsNeo4jLookupInvalidTokenKind)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_users_neo_lookup_bad ON users USING NEO4J_LOOKUP (id) "
        "WITH (token_kind = 'broken')");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("TOKEN_KIND invalid for NEO4J_LOOKUP"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsNeo4jTextNgramBounds)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_users_neo_text_bad ON users USING NEO4J_TEXT (name) "
        "WITH (min_n = 4, max_n = 2)");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("MIN_N must be <= MAX_N"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsNeo4jPointInvalidCellSemantics)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_users_neo_point_bad ON users USING NEO4J_POINT (id) "
        "WITH (cell_semantics = 'grid')");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("CELL_SEMANTICS invalid for NEO4J_POINT"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsNeo4jVectorInvalidIndexImpl)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_vectors_neo_vec_bad ON vectors USING NEO4J_VECTOR (embedding) "
        "WITH (index_impl = 'flat')");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("INDEX_IMPL invalid for NEO4J_VECTOR"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsCassandraSasiInvalidMode)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_users_sasi_bad ON users USING CASSANDRA_SASI (name) "
        "WITH (mode = 'regex')");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("MODE invalid for CASSANDRA_SASI"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsCassandraSaiInvalidIndexKind)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_users_sai_bad ON users USING CASSANDRA_SAI (name) "
        "WITH (index_kind = 'geo')");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("INDEX_KIND invalid for CASSANDRA_SAI"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3CreateIndexRejectsRedisInvalidPersistenceMode)
{
    ExecutionResult result = executeSql(
        "CREATE INDEX idx_users_redis_bad ON users USING REDIS_HASH (name) "
        "WITH (\"redis.persistence\" = 'later')");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("REDIS.PERSISTENCE invalid for REDIS family"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, LegacyCreateIndexIsRejectedByV3OnlyExecutor)
{
    std::vector<uint8_t> bytecode = buildLegacyCreateIndexBytecode(254);
    ExecutionResult result = executeBytecode(bytecode);
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("SBLR3_REQUIRED"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3AnalyzeRejectsInvalidTargetDeterministically)
{
    auto bytecode = compileSql("ANALYZE users");
    ASSERT_FALSE(bytecode.empty());

    auto mutated = mutateV3InstructionPayload(
        bytecode,
        Opcode::SBLR3_ANALYZE,
        [](Value::Object& payload) {
            payload["target"] = Value(static_cast<uint64_t>(99));
        });
    ASSERT_FALSE(mutated.empty());

    ExecutionResult result = executeBytecode(mutated);
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("V3 ANALYZE target is invalid"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3AlterIndexRejectsUnknownActionDeterministically)
{
    auto bytecode = compileSql("ALTER INDEX public.idx_users_id REBUILD ONLINE");
    ASSERT_FALSE(bytecode.empty());

    auto mutated = mutateV3InstructionPayload(
        bytecode,
        Opcode::SBLR3_ALTER_INDEX,
        [](Value::Object& payload) {
            payload["action"] = Value(static_cast<uint64_t>(999));
        });
    ASSERT_FALSE(mutated.empty());

    ExecutionResult result = executeBytecode(mutated);
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("ALTER INDEX action not implemented: UNKNOWN"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3AlterIndexRejectsUnsupportedOptionKeyDeterministically)
{
    auto bytecode = compileSql("ALTER INDEX public.idx_users_id SET (bloom_filter = true)");
    ASSERT_FALSE(bytecode.empty());

    auto mutated = mutateV3InstructionPayload(
        bytecode,
        Opcode::SBLR3_ALTER_INDEX,
        [](Value::Object& payload) {
            auto it = payload.find("options");
            ASSERT_TRUE(it != payload.end());
            auto* options = std::get_if<Value::List>(&it->second.data);
            ASSERT_TRUE(options != nullptr);
            ASSERT_FALSE(options->empty());
            auto* opt_obj = std::get_if<Value::Object>(&(*options)[0].data);
            ASSERT_TRUE(opt_obj != nullptr);
            (*opt_obj)["key"] = Value(std::string("FILLFACTOR"));
        });
    ASSERT_FALSE(mutated.empty());

    ExecutionResult result = executeBytecode(mutated);
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("ALTER INDEX option not implemented: FILLFACTOR"), std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3AlterIndexResetRequiresSupportedOption)
{
    auto bytecode = compileSql("ALTER INDEX public.idx_users_id RESET (bloom_filter)");
    ASSERT_FALSE(bytecode.empty());

    auto mutated = mutateV3InstructionPayload(
        bytecode,
        Opcode::SBLR3_ALTER_INDEX,
        [](Value::Object& payload) {
            payload["options"] = Value(Value::List{});
        });
    ASSERT_FALSE(mutated.empty());

    ExecutionResult result = executeBytecode(mutated);
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find(
                  "ALTER INDEX RESET_OPTIONS requires at least one supported option"),
              std::string::npos)
        << result.error();
}

TEST_F(IndexExecutorDispatchContractsTest, V3AlterIndexDefaultsScopeRejectsUntilImplemented)
{
    ExecutionResult result =
        executeSql("ALTER INDEX DEFAULTS FOR HASH SET (bloom_filter = true)");
    ASSERT_FALSE(result.success());
    EXPECT_NE(result.error().find("BRG_0406"),
              std::string::npos)
        << result.error();
    EXPECT_NE(result.error().find("SBLR3_ALTER_INDEX_DEFAULTS"),
              std::string::npos)
        << result.error();
}

} // namespace
