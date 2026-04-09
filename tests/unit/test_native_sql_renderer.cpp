/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "scratchbird/core/database.h"
#include "scratchbird/sblr/native_sql_renderer.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/v3_container.h"
#include "test_helpers.h"

using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::sblr::NativeSqlNameResolver;
using scratchbird::sblr::NativeSqlObjectTypeHint;
using scratchbird::sblr::NativeSqlRenderResult;
using scratchbird::sblr::QueryCompilerV3;
using scratchbird::sblr::renderNativeSqlInstruction;
using scratchbird::testing::TestDatabaseFile;

namespace {

bool startsWith(const std::string& text, const std::string& prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

class StubNameResolver final : public NativeSqlNameResolver {
public:
    void add(const std::string& uuid,
             NativeSqlObjectTypeHint hint,
             const std::string& resolved_name) {
        map_[key(uuid, hint)] = resolved_name;
    }

    bool resolveNameByUuid(const std::string& uuid_text,
                           NativeSqlObjectTypeHint hint,
                           std::string& resolved_name) override {
        auto it = map_.find(key(uuid_text, hint));
        if (it == map_.end()) {
            it = map_.find(key(uuid_text, NativeSqlObjectTypeHint::UNKNOWN));
            if (it == map_.end()) {
                resolved_name.clear();
                return false;
            }
        }
        resolved_name = it->second;
        return true;
    }

private:
    static std::string key(const std::string& uuid, NativeSqlObjectTypeHint hint) {
        return std::to_string(static_cast<unsigned>(hint)) + ":" + uuid;
    }

    std::unordered_map<std::string, std::string> map_;
};

}  // namespace

class NativeSqlRendererTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_file_ = std::make_unique<TestDatabaseFile>("native_sql_renderer");

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

TEST_F(NativeSqlRendererTest, NP025VectorsRenderDeterministicNativeSql) {
    struct RenderVector {
        const char* case_id;
        const char* sql;
        const char* contract_id;
        const char* prefix;
    };

    const std::vector<RenderVector> vectors = {
        {"NP025-GOLD-001", "DOC PATH FILTER PATH_ID 17 OP EQ VALUE_REF 42", "NRSQL-001-DOC-PATH-FILTER", "DOC PATH FILTER PATH_ID "},
        {"NP025-GOLD-002", "TS BUCKET AGG TIME_EXPR 91 BUCKET_NS 60000000000 AGG_REFS (7, 8, 9)", "NRSQL-002-TS-BUCKET-AGG", "TS BUCKET AGG "},
        {"NP025-GOLD-003", "SEARCH QUERY DSL TARGET_INDEX 22 PAYLOAD '{\"q\":\"bird\"}' SCORER BM25", "NRSQL-003-SEARCH-QUERY-DSL", "SEARCH QUERY DSL "},
        {"NP025-GOLD-004", "VECTOR ANN QUERY INDEX 33 METRIC COSINE TOPK 15 EF_SEARCH 64", "NRSQL-004-VECTOR-ANN-QUERY", "VECTOR ANN QUERY INDEX "},
        {"NP025-GOLD-005", "HYBRID BRIDGE EXCHANGE SOURCE_TRACK 1 TARGET_TRACK 2 MODE HASH_SHUFFLE", "NRSQL-005-HYBRID-BRIDGE", "HYBRID BRIDGE EXCHANGE SOURCE_TRACK "},
        {"NP025-GOLD-006", "CREATE DATABASE EMULATED postgresql localhost:db_main", "NRSQL-010-CREATE-DATABASE-EMULATED", "CREATE DATABASE "},
        {"NP025-GOLD-007", "CREATE USER app_user WITH PASSWORD 'pw' NOSUPERUSER", "NRSQL-011-CREATE-USER", "CREATE USER app_user"},
        {"NP025-GOLD-008", "ALTER USER app_user WITH PASSWORD 'pw2' SUPERUSER", "NRSQL-012-ALTER-USER", "ALTER USER app_user WITH PASSWORD 'pw2' SUPERUSER"},
        {"NP025-GOLD-009", "DROP USER IF EXISTS app_user CASCADE", "NRSQL-013-DROP-USER", "DROP USER "},
        {"NP025-GOLD-010", "CREATE CONNECTION RULE ch_src ORDER 5 MATCH (TRANSPORT=TLS, SOURCE='10.0.0.0/8', PRINCIPAL='ch_%') REQUIRE (TLS=TLS, PROVIDER=INTERNAL) ACTION ALLOW EXPECT VERSION 1", "NRSQL-020-CONNECTION-RULE-CREATE", "CREATE CONNECTION RULE ch_src"},
        {"NP025-GOLD-011", "ALTER CONNECTION RULE ch_src SET (ACTION='ALLOW') EXPECT VERSION 2", "NRSQL-021-CONNECTION-RULE-ALTER", "ALTER CONNECTION RULE ch_src SET (ACTION='ALLOW')"},
        {"NP025-GOLD-012", "DROP CONNECTION RULE ch_src EXPECT VERSION 2", "NRSQL-022-CONNECTION-RULE-DROP", "DROP CONNECTION RULE ch_src"},
        {"NP025-GOLD-013", "CREATE TOKEN ifx_reader WITH SCOPE (ALLOW BUCKET 'cpu_metrics' ACTION READ)", "NRSQL-023-TOKEN-CREATE", "CREATE TOKEN ifx_reader WITH SCOPE_MODEL=GENERIC SCOPE(ALLOW BUCKET 'cpu_metrics' ACTION READ)"},
        {"NP025-GOLD-014", "ALTER TOKEN ifx_reader SET (TTL_HOURS=24)", "NRSQL-024-TOKEN-ALTER", "ALTER TOKEN ifx_reader SET (TTL_HOURS=24)"},
        {"NP025-GOLD-015", "REVOKE TOKEN ifx_reader", "NRSQL-025-TOKEN-REVOKE", "REVOKE TOKEN ifx_reader"},
        {"NP025-GOLD-016", "DROP TOKEN ifx_reader", "NRSQL-026-TOKEN-DROP", "DROP TOKEN ifx_reader"},
        {"NP025-GOLD-017", "CREATE QUOTA PROFILE q1 (MAX_REQUESTS_PER_SEC=1000, WINDOW_MS=1000)", "NRSQL-027-QUOTA-PROFILE-CREATE", "CREATE QUOTA PROFILE q1 (MAX_REQUESTS_PER_SEC=1000, WINDOW_MS=1000)"},
        {"NP025-GOLD-018", "ALTER QUOTA PROFILE q1 SET (MAX_REQUESTS_PER_SEC=2000)", "NRSQL-028-QUOTA-PROFILE-ALTER", "ALTER QUOTA PROFILE q1 SET (MAX_REQUESTS_PER_SEC=2000)"},
        {"NP025-GOLD-019", "DROP QUOTA PROFILE q1", "NRSQL-029-QUOTA-PROFILE-DROP", "DROP QUOTA PROFILE q1"},
        {"NP025-GOLD-020", "CREATE POLICY p1 ON t1 USING (1 = 1)", "NRSQL-014-CREATE-POLICY", "CREATE POLICY p1 ON t1 USING (<expr>)"},
        {"NP025-GOLD-021", "ALTER POLICY p1 ON t1 USING (2 = 2)", "NRSQL-015-ALTER-POLICY", "ALTER POLICY "},
        {"NP025-GOLD-022", "DROP POLICY IF EXISTS p1 ON t1", "NRSQL-016-DROP-POLICY", "DROP POLICY "},
        {"NP025-GOLD-023", "CREATE JOB sch_daily SCHEDULE = CRON 'FREQ=DAILY;INTERVAL=1' AS SQL 'SELECT 1'", "NRSQL-017-CREATE-SCHEDULE", "CREATE JOB sch_daily SCHEDULE = "},
        {"NP025-GOLD-024", "ALTER JOB sch_daily SCHEDULE = CRON 'FREQ=DAILY;BYDAY=MO,TU'", "NRSQL-018-ALTER-SCHEDULE", "ALTER JOB sch_daily SET SCHEDULE = CRON "},
        {"NP025-GOLD-025", "DROP JOB sch_daily", "NRSQL-019-DROP-SCHEDULE", "DROP JOB sch_daily"},
        {"NP025-GOLD-026", "UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES payload_trace SESSION_SIGNATURE sig_trace", "NRSQL-006-UDR-COMPILE", "UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES payload_trace SESSION_SIGNATURE sig_trace"},
        {"NP025-GOLD-027", "UDR VALIDATE SQL TEMPLATE TEMPLATE_ID tpl_trace SQL_TEXT 'SELECT 1' PROFILE native SESSION_SIGNATURE sig_trace", "NRSQL-007-UDR-TEMPLATE", "UDR VALIDATE SQL TEMPLATE TEMPLATE_ID tpl_trace SQL_TEXT 'SELECT 1' PROFILE native SESSION_SIGNATURE sig_trace"},
    };

    for (const auto& vector : vectors) {
        auto root = compileRootInstruction(vector.sql);

        NativeSqlRenderResult first;
        std::string error;
        ASSERT_TRUE(renderNativeSqlInstruction(root, first, error))
            << vector.case_id << " error=" << error;
        EXPECT_EQ(first.contract_id, std::string(vector.contract_id))
            << vector.case_id;
        EXPECT_FALSE(first.sql.empty()) << vector.case_id;
        EXPECT_TRUE(startsWith(first.sql, vector.prefix))
            << vector.case_id << " sql=" << first.sql;

        NativeSqlRenderResult second;
        ASSERT_TRUE(renderNativeSqlInstruction(root, second, error))
            << vector.case_id << " second pass error=" << error;
        EXPECT_EQ(first.sql, second.sql) << vector.case_id;
        EXPECT_EQ(first.contract_id, second.contract_id) << vector.case_id;
    }
}

TEST_F(NativeSqlRendererTest, GenericAlterSystemRendersWithLiteralValue) {
    auto root = compileRootInstruction("ALTER SYSTEM SET custom.knob = 'enabled'");
    NativeSqlRenderResult rendered;
    std::string error;
    ASSERT_TRUE(renderNativeSqlInstruction(root, rendered, error)) << error;
    EXPECT_EQ(rendered.contract_id, std::string("NRSQL-100-ALTER-SYSTEM-GENERIC"));
    EXPECT_EQ(rendered.sql, std::string("ALTER SYSTEM SET custom.knob = 'enabled'"));
}

TEST_F(NativeSqlRendererTest, AlterSystemResetRendersClassifierStatement) {
    auto root = compileRootInstruction("ALTER SYSTEM RESET scheduler.enabled");
    NativeSqlRenderResult rendered;
    std::string error;
    ASSERT_TRUE(renderNativeSqlInstruction(root, rendered, error)) << error;
    EXPECT_EQ(rendered.contract_id, std::string("NRSQL-100-ALTER-SYSTEM-GENERIC"));
    EXPECT_EQ(rendered.sql, std::string("ALTER SYSTEM RESET scheduler.enabled"));
}

TEST_F(NativeSqlRendererTest, ConfigHistoryRendersClassifierStatement) {
    auto root = compileRootInstruction("CONFIG HISTORY");
    NativeSqlRenderResult rendered;
    std::string error;
    ASSERT_TRUE(renderNativeSqlInstruction(root, rendered, error)) << error;
    EXPECT_EQ(rendered.contract_id, std::string("NRSQL-100-ALTER-SYSTEM-GENERIC"));
    EXPECT_EQ(rendered.sql, std::string("CONFIG HISTORY"));
}

TEST_F(NativeSqlRendererTest, ConfigReloadRendersClassifierStatement) {
    auto root = compileRootInstruction("CONFIG RELOAD");
    NativeSqlRenderResult rendered;
    std::string error;
    ASSERT_TRUE(renderNativeSqlInstruction(root, rendered, error)) << error;
    EXPECT_EQ(rendered.contract_id, std::string("NRSQL-100-ALTER-SYSTEM-GENERIC"));
    EXPECT_EQ(rendered.sql, std::string("CONFIG RELOAD"));
}

TEST_F(NativeSqlRendererTest, UuidNameResolutionAdapterResolvesWhenResolverProvided) {
    auto root = compileRootInstruction("CREATE USER app_user WITH PASSWORD 'pw' NOSUPERUSER");
    auto* payload = std::get_if<scratchbird::sblr::v3::Value::Object>(&root.payload.data);
    ASSERT_NE(payload, nullptr);

    constexpr const char* kUserUuid = "01234567-89ab-cdef-0123-456789abcdef";
    (*payload)["name"] = scratchbird::sblr::v3::Value(std::string(kUserUuid));

    StubNameResolver resolver;
    resolver.add(kUserUuid, NativeSqlObjectTypeHint::USER, "resolved_user");

    NativeSqlRenderResult resolved_render;
    std::string error;
    ASSERT_TRUE(renderNativeSqlInstruction(root, &resolver, resolved_render, error)) << error;
    EXPECT_EQ(resolved_render.sql, std::string("CREATE USER resolved_user"));

    StubNameResolver empty_resolver;
    NativeSqlRenderResult unresolved_render;
    ASSERT_TRUE(renderNativeSqlInstruction(root, &empty_resolver, unresolved_render, error)) << error;
    EXPECT_EQ(unresolved_render.sql, std::string("CREATE USER ") + kUserUuid);
}
