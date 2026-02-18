/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "scratchbird/core/database.h"
#include "scratchbird/sblr/native_sql_render_contract.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_opcode_identity.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "test_helpers.h"

using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::sblr::NativeSqlRenderContract;
using scratchbird::sblr::QueryCompilerV3;
using scratchbird::sblr::nativeSqlRenderContractForInstruction;
using scratchbird::sblr::nativeSqlRenderContractTable;
using scratchbird::sblr::nativeSqlResultShapeName;
using scratchbird::testing::TestDatabaseFile;

class NativeSqlRenderContractTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_file_ = std::make_unique<TestDatabaseFile>("native_sql_render_contract");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
    }

    scratchbird::sblr::v3::Instruction compileRootInstruction(const std::string& sql) {
        scratchbird::sblr::v3::Instruction root_inst{};

        auto compiled = compiler_->compile(sql);
        if (!compiled.success()) {
            for (const auto& error : compiled.errors()) {
                ADD_FAILURE() << "compile error: " << error << " sql=" << sql;
            }
            return root_inst;
        }

        scratchbird::sblr::v3::Container container;
        std::string decode_error;
        if (!scratchbird::sblr::v3::decodeContainer(
                compiled.bytecode().data(), compiled.bytecode().size(), container, decode_error)) {
            ADD_FAILURE() << "decodeContainer failed: " << decode_error << " sql=" << sql;
            return root_inst;
        }

        size_t offset = 0;
        scratchbird::sblr::v3::DecodeError err;
        scratchbird::sblr::v3::Instruction version_inst;
        if (!scratchbird::sblr::v3::decodeInstructionWithSchema(
                container.bytecode_stream.data(),
                container.bytecode_stream.size(),
                offset,
                version_inst,
                err)) {
            ADD_FAILURE() << "decode version instruction failed: " << err.message << " sql=" << sql;
            return root_inst;
        }
        if (!scratchbird::sblr::v3::decodeInstructionWithSchema(
                container.bytecode_stream.data(),
                container.bytecode_stream.size(),
                offset,
                root_inst,
                err)) {
            ADD_FAILURE() << "decode root instruction failed: " << err.message << " sql=" << sql;
            return root_inst;
        }
        return root_inst;
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<QueryCompilerV3> compiler_;
};

TEST_F(NativeSqlRenderContractTest, ContractTableHasDeterministicCanonicalBindings) {
    size_t count = 0;
    const NativeSqlRenderContract* table = nativeSqlRenderContractTable(count);
    ASSERT_NE(table, nullptr);
    ASSERT_GT(count, 0u);

    std::set<std::string> seen_ids;
    for (size_t i = 0; i < count; ++i) {
        const NativeSqlRenderContract& contract = table[i];
        ASSERT_NE(contract.contract_id, nullptr);
        ASSERT_NE(contract.canonical_opcode_symbol, nullptr);
        ASSERT_NE(contract.grammar_signature, nullptr);
        EXPECT_NE(std::string(contract.contract_id), std::string());
        EXPECT_NE(std::string(contract.canonical_opcode_symbol), std::string()) << contract.contract_id;
        EXPECT_NE(std::string(contract.grammar_signature), std::string()) << contract.contract_id;

        EXPECT_TRUE(seen_ids.insert(contract.contract_id).second)
            << "duplicate contract id: " << contract.contract_id;

        const std::string canonical_symbol =
            scratchbird::sblr::v3::canonicalOpcodeSymbolForOpcode(contract.opcode);
        EXPECT_EQ(canonical_symbol, contract.canonical_opcode_symbol)
            << "contract=" << contract.contract_id;

        const auto* schema = scratchbird::sblr::v3::schemaForOpcode(contract.opcode);
        if (schema == nullptr) {
            EXPECT_EQ(std::string(contract.contract_id), std::string("NRSQL-012-ALTER-USER"))
                << "contract=" << contract.contract_id;
        }

        const char* shape_name = nativeSqlResultShapeName(contract.result_shape);
        EXPECT_NE(shape_name, nullptr);
        EXPECT_NE(std::string(shape_name), std::string()) << "contract=" << contract.contract_id;
    }
}

TEST_F(NativeSqlRenderContractTest, NP025VectorsResolveToFrozenContracts) {
    struct ContractVector {
        const char* case_id;
        const char* sql;
        const char* contract_id;
    };

    const std::vector<ContractVector> vectors = {
        {"NP025-GOLD-001", "DOC PATH FILTER PATH_ID 17 OP EQ VALUE_REF 42", "NRSQL-001-DOC-PATH-FILTER"},
        {"NP025-GOLD-002", "TS BUCKET AGG TIME_EXPR 91 BUCKET_NS 60000000000 AGG_REFS (7, 8, 9)", "NRSQL-002-TS-BUCKET-AGG"},
        {"NP025-GOLD-003", "SEARCH QUERY DSL TARGET_INDEX 22 PAYLOAD '{\"q\":\"bird\"}' SCORER BM25", "NRSQL-003-SEARCH-QUERY-DSL"},
        {"NP025-GOLD-004", "VECTOR ANN QUERY INDEX 33 METRIC COSINE TOPK 15 EF_SEARCH 64", "NRSQL-004-VECTOR-ANN-QUERY"},
        {"NP025-GOLD-005", "HYBRID BRIDGE EXCHANGE SOURCE_TRACK 1 TARGET_TRACK 2 MODE HASH_SHUFFLE", "NRSQL-005-HYBRID-BRIDGE"},
        {"NP025-GOLD-006", "CREATE DATABASE EMULATED postgresql localhost:db_main", "NRSQL-010-CREATE-DATABASE-EMULATED"},
        {"NP025-GOLD-007", "CREATE USER app_user WITH PASSWORD 'pw' NOSUPERUSER", "NRSQL-011-CREATE-USER"},
        {"NP025-GOLD-008", "ALTER USER app_user WITH PASSWORD 'pw2' SUPERUSER", "NRSQL-012-ALTER-USER"},
        {"NP025-GOLD-009", "DROP USER IF EXISTS app_user CASCADE", "NRSQL-013-DROP-USER"},
        {"NP025-GOLD-010", "CREATE CONNECTION RULE ch_src ORDER 5 MATCH (TRANSPORT=TLS, SOURCE='10.0.0.0/8', PRINCIPAL='ch_%') REQUIRE (TLS=TLS, PROVIDER=INTERNAL) ACTION ALLOW EXPECT VERSION 1", "NRSQL-020-CONNECTION-RULE-CREATE"},
        {"NP025-GOLD-011", "ALTER CONNECTION RULE ch_src SET (ACTION='ALLOW') EXPECT VERSION 2", "NRSQL-021-CONNECTION-RULE-ALTER"},
        {"NP025-GOLD-012", "DROP CONNECTION RULE ch_src EXPECT VERSION 2", "NRSQL-022-CONNECTION-RULE-DROP"},
        {"NP025-GOLD-013", "CREATE TOKEN ifx_reader WITH SCOPE (ALLOW BUCKET 'cpu_metrics' ACTION READ)", "NRSQL-023-TOKEN-CREATE"},
        {"NP025-GOLD-014", "ALTER TOKEN ifx_reader SET (TTL_HOURS=24)", "NRSQL-024-TOKEN-ALTER"},
        {"NP025-GOLD-015", "REVOKE TOKEN ifx_reader", "NRSQL-025-TOKEN-REVOKE"},
        {"NP025-GOLD-016", "DROP TOKEN ifx_reader", "NRSQL-026-TOKEN-DROP"},
        {"NP025-GOLD-017", "CREATE QUOTA PROFILE q1 (MAX_REQUESTS_PER_SEC=1000, WINDOW_MS=1000)", "NRSQL-027-QUOTA-PROFILE-CREATE"},
        {"NP025-GOLD-018", "ALTER QUOTA PROFILE q1 SET (MAX_REQUESTS_PER_SEC=2000)", "NRSQL-028-QUOTA-PROFILE-ALTER"},
        {"NP025-GOLD-019", "DROP QUOTA PROFILE q1", "NRSQL-029-QUOTA-PROFILE-DROP"},
        {"NP025-GOLD-020", "CREATE POLICY p1 ON t1 USING (1 = 1)", "NRSQL-014-CREATE-POLICY"},
        {"NP025-GOLD-021", "ALTER POLICY p1 ON t1 USING (2 = 2)", "NRSQL-015-ALTER-POLICY"},
        {"NP025-GOLD-022", "DROP POLICY IF EXISTS p1 ON t1", "NRSQL-016-DROP-POLICY"},
        {"NP025-GOLD-023", "CREATE SCHEDULE sch_daily RRULE 'FREQ=DAILY;INTERVAL=1' DTSTART '2026-02-17T00:00:00' TZ 'UTC'", "NRSQL-017-CREATE-SCHEDULE"},
        {"NP025-GOLD-024", "ALTER SCHEDULE sch_daily SET RRULE_SET ('FREQ=DAILY;BYDAY=MO', 'FREQ=DAILY;BYDAY=TU') DTSTART '2026-02-17T00:00:00' TZ 'UTC'", "NRSQL-018-ALTER-SCHEDULE"},
        {"NP025-GOLD-025", "DROP SCHEDULE sch_daily", "NRSQL-019-DROP-SCHEDULE"},
        {"NP025-GOLD-026", "UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES payload_trace SESSION_SIGNATURE sig_trace", "NRSQL-006-UDR-COMPILE"},
        {"NP025-GOLD-027", "UDR VALIDATE SQL TEMPLATE TEMPLATE_ID tpl_trace SQL_TEXT 'SELECT 1' PROFILE native SESSION_SIGNATURE sig_trace", "NRSQL-007-UDR-TEMPLATE"},
    };

    for (const auto& vector : vectors) {
        auto root = compileRootInstruction(vector.sql);
        const NativeSqlRenderContract* contract = nativeSqlRenderContractForInstruction(root);
        ASSERT_NE(contract, nullptr) << vector.case_id << " sql=" << vector.sql;
        EXPECT_STREQ(contract->contract_id, vector.contract_id)
            << vector.case_id << " sql=" << vector.sql;
    }
}

TEST_F(NativeSqlRenderContractTest, AlterSystemGenericFallsBackWhenClassifierUnknown) {
    auto root = compileRootInstruction("ALTER SYSTEM SET custom.knob = 'enabled'");
    const NativeSqlRenderContract* contract = nativeSqlRenderContractForInstruction(root);
    ASSERT_NE(contract, nullptr);
    EXPECT_STREQ(contract->contract_id, "NRSQL-100-ALTER-SYSTEM-GENERIC");
}
