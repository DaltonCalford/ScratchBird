/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "scratchbird/core/database.h"
#include "scratchbird/sblr/native_sql_renderer.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/v3_container.h"
#include "test_helpers.h"

using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::sblr::NativeSqlRenderResult;
using scratchbird::sblr::QueryCompilerV3;
using scratchbird::sblr::renderNativeSqlInstruction;
using scratchbird::testing::TestDatabaseFile;

namespace {

enum class RoundtripMode : uint8_t {
    EXACT_DIGEST = 0,
    OPCODE_ONLY = 1,
    EXPECT_REPARSE_REJECT = 2,
};

struct RoundtripCase {
    const char* case_id;
    const char* sql;
    RoundtripMode mode;
};

}  // namespace

class NativeSqlRoundtripFidelityTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_file_ = std::make_unique<TestDatabaseFile>("native_sql_roundtrip_fidelity");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
    }

    QueryCompilerV3::TraceResult trace(const std::string& sql) {
        return compiler_->compileTrace(sql);
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

TEST_F(NativeSqlRoundtripFidelityTest, RoundtripMatrixHonorsDeterministicContracts) {
    const std::vector<RoundtripCase> vectors = {
        {"NP034-RT-001", "DOC PATH FILTER PATH_ID 17 OP EQ VALUE_REF 42", RoundtripMode::EXACT_DIGEST},
        {"NP034-RT-002", "TS BUCKET AGG TIME_EXPR 91 BUCKET_NS 60000000000 AGG_REFS (7, 8, 9)", RoundtripMode::EXPECT_REPARSE_REJECT},
        {"NP034-RT-003", "SEARCH QUERY DSL TARGET_INDEX 22 PAYLOAD '{\"q\":\"bird\"}' SCORER BM25", RoundtripMode::OPCODE_ONLY},
        {"NP034-RT-004", "VECTOR ANN QUERY INDEX 33 METRIC COSINE TOPK 15 EF_SEARCH 64", RoundtripMode::EXACT_DIGEST},
        {"NP034-RT-005", "HYBRID BRIDGE EXCHANGE SOURCE_TRACK 1 TARGET_TRACK 2 MODE HASH_SHUFFLE", RoundtripMode::EXACT_DIGEST},
        {"NP034-RT-006", "UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES payload_trace SESSION_SIGNATURE sig_trace", RoundtripMode::EXACT_DIGEST},
        {"NP034-RT-007", "UDR VALIDATE SQL TEMPLATE TEMPLATE_ID tpl_trace SQL_TEXT 'SELECT 1' PROFILE native SESSION_SIGNATURE sig_trace", RoundtripMode::EXACT_DIGEST},
        {"NP034-RT-008", "CREATE USER app_user WITH PASSWORD 'pw' NOSUPERUSER", RoundtripMode::OPCODE_ONLY},
        {"NP034-RT-009", "CREATE POLICY p1 ON t1 USING (1 = 1)", RoundtripMode::EXPECT_REPARSE_REJECT},
        {"NP034-RT-010", "ALTER POLICY p1 ON t1 USING (2 = 2)", RoundtripMode::OPCODE_ONLY},
    };

    for (const auto& vector : vectors) {
        auto first_trace = trace(vector.sql);
        ASSERT_TRUE(first_trace.success()) << vector.case_id;

        auto root = compileRootInstruction(vector.sql);
        NativeSqlRenderResult rendered;
        std::string render_error;
        ASSERT_TRUE(renderNativeSqlInstruction(root, rendered, render_error))
            << vector.case_id << " render_error=" << render_error;
        ASSERT_FALSE(rendered.sql.empty()) << vector.case_id;

        auto second_trace = trace(rendered.sql);
        if (vector.mode == RoundtripMode::EXPECT_REPARSE_REJECT) {
            EXPECT_FALSE(second_trace.success()) << vector.case_id << " rendered=" << rendered.sql;
            EXPECT_FALSE(second_trace.errors().empty()) << vector.case_id;
            continue;
        }

        ASSERT_TRUE(second_trace.success()) << vector.case_id << " rendered=" << rendered.sql;
        EXPECT_EQ(first_trace.digest().root_opcode_symbol, second_trace.digest().root_opcode_symbol)
            << vector.case_id << " rendered=" << rendered.sql;

        if (vector.mode == RoundtripMode::EXACT_DIGEST) {
            EXPECT_EQ(first_trace.digest().sblr_hash, second_trace.digest().sblr_hash)
                << vector.case_id << " rendered=" << rendered.sql;
        } else {
            EXPECT_EQ(first_trace.digest().root_opcode_symbol, second_trace.digest().root_opcode_symbol)
                << vector.case_id << " rendered=" << rendered.sql;
        }
    }
}
