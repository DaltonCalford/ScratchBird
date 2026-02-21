/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "scratchbird/core/database.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::sblr::QueryCompilerV3;
using scratchbird::testing::TestDatabaseFile;

class SqlToSblrTraceDeterminismTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_file_ = std::make_unique<TestDatabaseFile>("sql_to_sblr_trace_determinism");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
    }

    QueryCompilerV3::TraceResult trace(const std::string& sql) {
        auto result = compiler_->compileTrace(sql);
        if (!result.success()) {
            for (const auto& error : result.errors()) {
                ADD_FAILURE() << "compileTrace error: " << error;
            }
        }
        return result;
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<QueryCompilerV3> compiler_;
};

namespace {

bool traceHasCode(const QueryCompilerV3::TraceResult& result, std::string_view code) {
    for (const auto& error : result.errors()) {
        if (error.find(code) != std::string::npos) {
            return true;
        }
    }
    return false;
}

struct GoldenSqlVector {
    const char* case_id;
    const char* sql;
};

}  // namespace

TEST_F(SqlToSblrTraceDeterminismTest, NormalizationMakesEquivalentSqlStable) {
    const std::string canonical =
        "CREATE INDEX idx_vec ON docs USING HNSW (embedding) WITH (metric='COSINE', topk_default=25)";
    const std::string variant =
        "  CREATE   INDEX idx_vec   ON docs USING HNSW (embedding) WITH (metric='COSINE', topk_default=25)  ";

    auto trace_a = trace(canonical);
    auto trace_b = trace(variant);
    ASSERT_TRUE(trace_a.success());
    ASSERT_TRUE(trace_b.success());

    EXPECT_EQ(trace_a.digest().normalized_sql, trace_b.digest().normalized_sql);
    EXPECT_EQ(trace_a.digest().sql_hash, trace_b.digest().sql_hash);
    EXPECT_EQ(trace_a.digest().ast_hash, trace_b.digest().ast_hash);
    EXPECT_EQ(trace_a.digest().sblr_hash, trace_b.digest().sblr_hash);
    EXPECT_EQ(trace_a.digest().root_opcode_symbol, "OP_STMT_DDL_CREATE_INDEX");
}

TEST_F(SqlToSblrTraceDeterminismTest, RepeatCompileHasIdenticalDigests) {
    const std::string sql =
        "CREATE SCHEDULE sch_daily RRULE 'FREQ=DAILY;INTERVAL=1' "
        "DTSTART '2026-02-17T00:00:00' TZ 'UTC'";

    auto trace_a = trace(sql);
    auto trace_b = trace(sql);
    ASSERT_TRUE(trace_a.success());
    ASSERT_TRUE(trace_b.success());

    EXPECT_EQ(trace_a.digest().sql_hash, trace_b.digest().sql_hash);
    EXPECT_EQ(trace_a.digest().ast_hash, trace_b.digest().ast_hash);
    EXPECT_EQ(trace_a.digest().sblr_hash, trace_b.digest().sblr_hash);
    EXPECT_EQ(trace_a.digest().root_opcode_symbol, trace_b.digest().root_opcode_symbol);
}

TEST_F(SqlToSblrTraceDeterminismTest, DistinctStatementsProduceDistinctSblrDigests) {
    auto trace_index = trace("CREATE INDEX idx_search ON docs USING FULLTEXT (title, body)");
    auto trace_token =
        trace("CREATE TOKEN ifx_reader WITH SCOPE (ALLOW BUCKET 'cpu_metrics' ACTION READ)");
    ASSERT_TRUE(trace_index.success());
    ASSERT_TRUE(trace_token.success());

    EXPECT_NE(trace_index.digest().sblr_hash, trace_token.digest().sblr_hash);
    EXPECT_NE(trace_index.digest().ast_hash, trace_token.digest().ast_hash);
}

TEST_F(SqlToSblrTraceDeterminismTest, UdrCompileAliasFormsNormalizeToCanonicalTrace) {
    const std::string statement_form =
        "UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES payload_trace SESSION_SIGNATURE sig_trace";
    const std::string clause_form =
        "COMPILE EMBEDDED PAYLOAD native, SQL_TEXT, payload_trace, sig_trace";

    auto trace_statement = trace(statement_form);
    auto trace_clause = trace(clause_form);
    ASSERT_TRUE(trace_statement.success());
    ASSERT_TRUE(trace_clause.success());

    EXPECT_EQ(trace_statement.digest().normalized_sql, trace_clause.digest().normalized_sql);
    EXPECT_EQ(trace_statement.digest().sql_hash, trace_clause.digest().sql_hash);
    EXPECT_EQ(trace_statement.digest().ast_hash, trace_clause.digest().ast_hash);
    EXPECT_EQ(trace_statement.digest().sblr_hash, trace_clause.digest().sblr_hash);
}

TEST_F(SqlToSblrTraceDeterminismTest, UdrCompileFunctionFormNormalizesToCanonicalTrace) {
    const std::string statement_form =
        "UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES payload_trace SESSION_SIGNATURE sig_trace";
    const std::string function_form =
        "SELECT COMPILE_EMBEDDED_PAYLOAD('native','SQL_TEXT','payload_trace','sig_trace')";

    auto trace_statement = trace(statement_form);
    auto trace_function = trace(function_form);
    ASSERT_TRUE(trace_statement.success());
    ASSERT_TRUE(trace_function.success());

    EXPECT_EQ(trace_statement.digest().normalized_sql, trace_function.digest().normalized_sql);
    EXPECT_EQ(trace_statement.digest().sql_hash, trace_function.digest().sql_hash);
    EXPECT_EQ(trace_statement.digest().ast_hash, trace_function.digest().ast_hash);
    EXPECT_EQ(trace_statement.digest().sblr_hash, trace_function.digest().sblr_hash);
}

TEST_F(SqlToSblrTraceDeterminismTest, UdrTemplateAliasFormsNormalizeToCanonicalTrace) {
    const std::string statement_form =
        "UDR VALIDATE SQL TEMPLATE TEMPLATE_ID tpl_trace SQL_TEXT 'SELECT 1' PROFILE native SESSION_SIGNATURE sig_trace";
    const std::string clause_form =
        "VALIDATE SQL TEMPLATE tpl_trace USING 'SELECT 1' PROFILE native SIGNATURE sig_trace";

    auto trace_statement = trace(statement_form);
    auto trace_clause = trace(clause_form);
    ASSERT_TRUE(trace_statement.success());
    ASSERT_TRUE(trace_clause.success());

    EXPECT_EQ(trace_statement.digest().normalized_sql, trace_clause.digest().normalized_sql);
    EXPECT_EQ(trace_statement.digest().sql_hash, trace_clause.digest().sql_hash);
    EXPECT_EQ(trace_statement.digest().ast_hash, trace_clause.digest().ast_hash);
    EXPECT_EQ(trace_statement.digest().sblr_hash, trace_clause.digest().sblr_hash);
}

TEST_F(SqlToSblrTraceDeterminismTest, ConsistencyClauseAliasFormsShareAstAndSblrDigests) {
    const std::string canonical =
        "UPDATE docs SET title = 'y' WHERE id = 1 "
        "WITH CONSISTENCY QUORUM SERIAL CONSISTENCY LOCAL_SERIAL IF EXISTS";
    const std::string alias =
        "UPDATE docs SET title = 'y' WHERE id = 1 "
        "USING CONSISTENCY QUORUM AND SERIAL CONSISTENCY LOCAL_SERIAL IF EXISTS";

    auto trace_canonical = trace(canonical);
    auto trace_alias = trace(alias);
    ASSERT_TRUE(trace_canonical.success());
    ASSERT_TRUE(trace_alias.success());

    EXPECT_EQ(trace_canonical.digest().ast_hash, trace_alias.digest().ast_hash);
    EXPECT_EQ(trace_canonical.digest().sblr_hash, trace_alias.digest().sblr_hash);
}

TEST_F(SqlToSblrTraceDeterminismTest, ExtensionInstallAliasFormsShareAstAndSblrDigests) {
    const std::string statement_form = "INSTALL EXTENSION httpfs";
    const std::string clause_form = "INSTALL httpfs";

    auto trace_statement = trace(statement_form);
    auto trace_clause = trace(clause_form);
    ASSERT_TRUE(trace_statement.success());
    ASSERT_TRUE(trace_clause.success());

    EXPECT_EQ(trace_statement.digest().ast_hash, trace_clause.digest().ast_hash);
    EXPECT_EQ(trace_statement.digest().sblr_hash, trace_clause.digest().sblr_hash);
}

TEST_F(SqlToSblrTraceDeterminismTest, InvalidUdrAliasRejectsBeforeSblrEmission) {
    auto bad_trace =
        compiler_->compileTrace("COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES payload_trace SESSION_SIGNATURE sig_trace");
    EXPECT_FALSE(bad_trace.success());
    ASSERT_FALSE(bad_trace.errors().empty());
    EXPECT_NE(bad_trace.errors().front().find("Parse error"), std::string::npos);
    EXPECT_NE(bad_trace.errors().front().find("SQL_CONTEXT:"), std::string::npos);
    EXPECT_FALSE(bad_trace.diagnostic_sql_context().empty());
}

TEST_F(SqlToSblrTraceDeterminismTest, NativeExtensionCorpusCompilesToDeterministicDigests) {
    const bool dump_trace_hashes = std::getenv("SB_TRACE_DUMP") != nullptr;
    const std::vector<std::string> corpus = {
        "CREATE INDEX idx_search ON docs USING FULLTEXT (title, body)",
        "CREATE INDEX idx_vec ON docs USING HNSW (embedding) WITH (metric='COSINE', topk_default=25)",
        "ALTER INDEX idx_search REBUILD ONLINE",
        "CREATE MEASUREMENT cpu (host STRING, usage_user DOUBLE)",
        "ALTER MEASUREMENT cpu RETENTION '30d'",
        "CREATE SCHEDULE sch_daily RRULE 'FREQ=DAILY;INTERVAL=1' DTSTART '2026-02-17T00:00:00' TZ 'UTC'",
        "SELECT id FROM docs ORDER BY id FETCH FIRST 10 ROWS WITH TIES",
        "SELECT DISTINCT ON (user_id) user_id, created_at FROM logs ORDER BY user_id, created_at DESC",
        "SELECT id FROM docs FOR NO KEY UPDATE",
        "SELECT g.val, g.ord FROM LATERAL generate_series(1, 3) WITH ORDINALITY AS g(val, ord)",
        "SELECT id FROM docs TABLESAMPLE SYSTEM (10) REPEATABLE (42)",
        "SELECT region, sum(amount) FROM sales GROUP BY ROLLUP(region)",
        "SELECT count(*) FILTER (WHERE status = 'ok') FROM logs",
        "SELECT sum(val) OVER (ORDER BY ts ROWS BETWEEN 1 PRECEDING AND CURRENT ROW EXCLUDE TIES) FROM metrics",
        "WITH RECURSIVE t(id, parent_id) AS (SELECT id, parent_id FROM nodes UNION ALL SELECT n.id, n.parent_id FROM nodes n JOIN t ON n.parent_id = t.id) "
        "SEARCH DEPTH FIRST BY id SET walk_order CYCLE id SET is_cycle USING cycle_path SELECT id FROM t",
        "CREATE TABLE child_tbl (id INT) INHERITS (base_tbl)",
        "CREATE EXTENSION IF NOT EXISTS pg_trgm WITH SCHEMA ext",
        "ALTER EXTENSION pg_trgm UPDATE TO '1.1'",
        "DROP EXTENSION IF EXISTS pg_trgm CASCADE",
        "INSTALL EXTENSION httpfs",
        "LOAD EXTENSION httpfs",
        "SET CONCURRENCY MODE SINGLE_WRITER",
        "SET SINGLE_WRITER ON",
        "SET CONSISTENCY QUORUM",
        "SET SERIAL CONSISTENCY LOCAL_SERIAL",
        "INSERT INTO docs (id, title) VALUES (1, 'x') USING CONSISTENCY QUORUM AND SERIAL CONSISTENCY LOCAL_SERIAL IF NOT EXISTS",
        "CREATE PUBLICATION pub_all FOR ALL TABLES",
        "CREATE SUBSCRIPTION sub_main CONNECTION 'host=127.0.0.1 dbname=main' PUBLICATION pub_all",
        "CREATE ACCESS METHOD am_btree TYPE INDEX HANDLER amhandler_btree",
        "CREATE STATISTICS st_sales (ndistinct, dependencies) ON region, product FROM sales",
        "CREATE TRANSFORM FOR jsonb LANGUAGE plpython3u (FROM SQL WITH FUNCTION jsonb_from_sql, TO SQL WITH FUNCTION jsonb_to_sql)",
        "COPY docs TO PROGRAM 'gzip > /tmp/docs.gz' WITH (FORMAT CSV, HEADER)",
        "SECURITY LABEL FOR sec_provider ON TABLE docs IS 'classified'",
        "SET SESSION AUTHORIZATION app_user",
        "EXPLAIN (ANALYZE, BUFFERS, WAL, FORMAT JSON) SELECT id FROM docs",
        "ALTER TABLE docs ENABLE ROW LEVEL SECURITY",
        "CREATE CONNECTION RULE ch_src ORDER 5 MATCH (TRANSPORT=TLS, SOURCE='10.0.0.0/8', PRINCIPAL='ch_%') "
        "REQUIRE (TLS=TLS, PROVIDER=INTERNAL) ACTION ALLOW EXPECT VERSION 1",
        "CREATE TOKEN ifx_reader WITH SCOPE (ALLOW BUCKET 'cpu_metrics' ACTION READ)",
        "CREATE QUOTA PROFILE q1 (MAX_REQUESTS_PER_SEC=1000, WINDOW_MS=1000)",
        "REVOKE TOKEN ifx_reader",
        "COMPILE EMBEDDED PAYLOAD native, SQL_TEXT, payload_trace, sig_trace",
        "COMPILE SQL TEMPLATE tpl_trace USING 'SELECT 1' PROFILE native SIGNATURE sig_trace",
        "SEARCH JOIN FIELD MAPPING INDEX 17 FIELD rel_join PARENT parent_doc CHILD child_doc ROUTING REQUIRED",
        "SEARCH PERCOLATOR FIELD INDEX 41 FIELD query_match QUERY_PARSER SIMPLE",
        "GRAPH PATH MATCH PATTERN rel_path MIN_HOPS 1 MAX_HOPS 4 CYCLE_POLICY NO_REPEAT",
        "REDIS LUA EVAL SCRIPT return_1 KEYS (k1, k2) ARGS (a1, a2)",
        "REDIS STREAM GROUP CREATE STREAM orders GROUP grp_a START_ID 0-0",
        "REDIS STREAM GROUP READ STREAM orders GROUP grp_a CONSUMER c1 COUNT 32 BLOCK_MS 5000"};

    for (const auto& sql : corpus) {
        auto first = trace(sql);
        auto second = trace(sql);
        ASSERT_TRUE(first.success()) << sql;
        ASSERT_TRUE(second.success()) << sql;

        EXPECT_FALSE(first.digest().sql_hash.empty()) << sql;
        EXPECT_FALSE(first.digest().ast_hash.empty()) << sql;
        EXPECT_FALSE(first.digest().sblr_hash.empty()) << sql;
        EXPECT_FALSE(first.digest().root_opcode_symbol.empty()) << sql;

        EXPECT_EQ(first.digest().sql_hash, second.digest().sql_hash) << sql;
        EXPECT_EQ(first.digest().ast_hash, second.digest().ast_hash) << sql;
        EXPECT_EQ(first.digest().sblr_hash, second.digest().sblr_hash) << sql;

        if (dump_trace_hashes) {
            std::cout << first.digest().normalized_sql << ","
                      << first.digest().sql_hash << ","
                      << first.digest().ast_hash << ","
                      << first.digest().sblr_hash << ","
                      << first.digest().root_opcode_symbol << "\n";
        }
    }
}

TEST_F(SqlToSblrTraceDeterminismTest, GoldenConformanceVectorsReplayDeterministically) {
    const std::vector<GoldenSqlVector> vectors = {
        {"NP025-GOLD-001", "DOC PATH FILTER PATH_ID 17 OP EQ VALUE_REF 42"},
        {"NP025-GOLD-002", "TS BUCKET AGG TIME_EXPR 91 BUCKET_NS 60000000000 AGG_REFS (7, 8, 9)"},
        {"NP025-GOLD-003", "SEARCH QUERY DSL TARGET_INDEX 22 PAYLOAD '{\"q\":\"bird\"}' SCORER BM25"},
        {"NP025-GOLD-004", "VECTOR ANN QUERY INDEX 33 METRIC COSINE TOPK 15 EF_SEARCH 64"},
        {"NP025-GOLD-005", "HYBRID BRIDGE EXCHANGE SOURCE_TRACK 1 TARGET_TRACK 2 MODE HASH_SHUFFLE"},
        {"NP025-GOLD-006", "CREATE DATABASE EMULATED postgresql localhost:db_main"},
        {"NP025-GOLD-007", "CREATE USER app_user WITH PASSWORD 'pw' NOSUPERUSER"},
        {"NP025-GOLD-008", "ALTER USER app_user WITH PASSWORD 'pw2' SUPERUSER"},
        {"NP025-GOLD-009", "DROP USER IF EXISTS app_user CASCADE"},
        {"NP025-GOLD-010", "CREATE CONNECTION RULE ch_src ORDER 5 MATCH (TRANSPORT=TLS, SOURCE='10.0.0.0/8', PRINCIPAL='ch_%') REQUIRE (TLS=TLS, PROVIDER=INTERNAL) ACTION ALLOW EXPECT VERSION 1"},
        {"NP025-GOLD-011", "ALTER CONNECTION RULE ch_src SET (ACTION='ALLOW') EXPECT VERSION 2"},
        {"NP025-GOLD-012", "DROP CONNECTION RULE ch_src EXPECT VERSION 2"},
        {"NP025-GOLD-013", "CREATE TOKEN ifx_reader WITH SCOPE (ALLOW BUCKET 'cpu_metrics' ACTION READ)"},
        {"NP025-GOLD-014", "ALTER TOKEN ifx_reader SET (TTL_HOURS=24)"},
        {"NP025-GOLD-015", "REVOKE TOKEN ifx_reader"},
        {"NP025-GOLD-016", "DROP TOKEN ifx_reader"},
        {"NP025-GOLD-017", "CREATE QUOTA PROFILE q1 (MAX_REQUESTS_PER_SEC=1000, WINDOW_MS=1000)"},
        {"NP025-GOLD-018", "ALTER QUOTA PROFILE q1 SET (MAX_REQUESTS_PER_SEC=2000)"},
        {"NP025-GOLD-019", "DROP QUOTA PROFILE q1"},
        {"NP025-GOLD-020", "CREATE POLICY p1 ON t1 USING (1 = 1)"},
        {"NP025-GOLD-021", "ALTER POLICY p1 ON t1 USING (2 = 2)"},
        {"NP025-GOLD-022", "DROP POLICY IF EXISTS p1 ON t1"},
        {"NP025-GOLD-023", "CREATE SCHEDULE sch_daily RRULE 'FREQ=DAILY;INTERVAL=1' DTSTART '2026-02-17T00:00:00' TZ 'UTC'"},
        {"NP025-GOLD-024", "ALTER SCHEDULE sch_daily SET RRULE_SET ('FREQ=DAILY;BYDAY=MO', 'FREQ=DAILY;BYDAY=TU') DTSTART '2026-02-17T00:00:00' TZ 'UTC'"},
        {"NP025-GOLD-025", "DROP SCHEDULE sch_daily"},
        {"NP025-GOLD-026", "UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES payload_trace SESSION_SIGNATURE sig_trace"},
        {"NP025-GOLD-027", "UDR VALIDATE SQL TEMPLATE TEMPLATE_ID tpl_trace SQL_TEXT 'SELECT 1' PROFILE native SESSION_SIGNATURE sig_trace"},
        {"NP040-GOLD-001", "SELECT id FROM docs ORDER BY id FETCH FIRST 10 ROWS WITH TIES"},
        {"NP040-GOLD-002", "SELECT id FROM docs FOR NO KEY UPDATE"},
        {"NP040-GOLD-003", "SELECT DISTINCT ON (user_id) user_id, created_at FROM logs ORDER BY user_id, created_at DESC"},
        {"NP040-GOLD-004", "SELECT g.val, g.ord FROM LATERAL generate_series(1, 3) WITH ORDINALITY AS g(val, ord)"},
        {"NP040-GOLD-005", "SELECT id FROM docs TABLESAMPLE SYSTEM (10) REPEATABLE (42)"},
        {"NP040-GOLD-006", "SELECT region, sum(amount) FROM sales GROUP BY ROLLUP(region)"},
        {"NP040-GOLD-007", "SELECT count(*) FILTER (WHERE status = 'ok') FROM logs"},
        {"NP040-GOLD-008", "SELECT sum(val) OVER (ORDER BY ts ROWS BETWEEN 1 PRECEDING AND CURRENT ROW EXCLUDE TIES) FROM metrics"},
        {"NP040-GOLD-009", "WITH RECURSIVE t(id, parent_id) AS (SELECT id, parent_id FROM nodes UNION ALL SELECT n.id, n.parent_id FROM nodes n JOIN t ON n.parent_id = t.id) SEARCH DEPTH FIRST BY id SET walk_order CYCLE id SET is_cycle USING cycle_path SELECT id FROM t"},
        {"NP040-GOLD-010", "CREATE TABLE child_def (id INT, parent_id INT, CONSTRAINT child_def_pk PRIMARY KEY (id) DEFERRABLE INITIALLY DEFERRED, CONSTRAINT child_def_fk FOREIGN KEY (parent_id) REFERENCES parent_def(id) DEFERRABLE INITIALLY IMMEDIATE)"},
        {"NP040-GOLD-011", "SET CONSTRAINTS child_def_fk, child_def_pk IMMEDIATE"},
        {"NP040-GOLD-012", "CREATE TABLE room_booking (room_id INT, during TSRANGE, CONSTRAINT room_booking_excl EXCLUDE USING gist (room_id WITH =, during WITH &&) WHERE (room_id > 0) DEFERRABLE INITIALLY IMMEDIATE)"},
        {"NP040-GOLD-013", "CREATE TABLE gen_docs (id INT, title TEXT, normalized_title TEXT GENERATED ALWAYS AS (lower(title)) STORED)"},
        {"NP040-GOLD-014", "CREATE TABLE child_tbl (id INT) INHERITS (base_tbl)"},
        {"NP040-GOLD-015", "CREATE EXTENSION IF NOT EXISTS pg_trgm WITH SCHEMA ext"},
        {"NP040-GOLD-016", "ALTER EXTENSION pg_trgm UPDATE TO '1.1'"},
        {"NP040-GOLD-017", "DROP EXTENSION IF EXISTS pg_trgm CASCADE"},
        {"NP040-GOLD-018", "CREATE PUBLICATION pub_all FOR ALL TABLES"},
        {"NP040-GOLD-019", "CREATE SUBSCRIPTION sub_main CONNECTION 'host=127.0.0.1 dbname=main' PUBLICATION pub_all"},
        {"NP040-GOLD-020", "CREATE ACCESS METHOD am_btree TYPE INDEX HANDLER amhandler_btree"},
        {"NP040-GOLD-021", "CREATE STATISTICS st_sales (ndistinct, dependencies) ON region, product FROM sales"},
        {"NP040-GOLD-022", "CREATE TRANSFORM FOR jsonb LANGUAGE plpython3u (FROM SQL WITH FUNCTION jsonb_from_sql, TO SQL WITH FUNCTION jsonb_to_sql)"},
        {"NP040-GOLD-023", "COPY docs TO PROGRAM 'gzip > /tmp/docs.gz' WITH (FORMAT CSV, HEADER)"},
        {"NP040-GOLD-024", "SECURITY LABEL FOR sec_provider ON TABLE docs IS 'classified'"},
        {"NP040-GOLD-025", "SET SESSION AUTHORIZATION app_user"},
        {"NP040-GOLD-026", "SET ROLE app_readonly"},
        {"NP040-GOLD-027", "EXPLAIN (ANALYZE, BUFFERS, WAL, FORMAT JSON) SELECT id FROM docs"},
        {"NP040-GOLD-028", "ALTER TABLE docs ENABLE ROW LEVEL SECURITY"},
        {"NP043-GOLD-001", "SEARCH JOIN FIELD MAPPING INDEX 17 FIELD rel_join PARENT parent_doc CHILD child_doc ROUTING REQUIRED"},
        {"NP043-GOLD-002", "SEARCH PERCOLATOR FIELD INDEX 41 FIELD query_match QUERY_PARSER SIMPLE"},
        {"NP043-GOLD-003", "GRAPH PATH MATCH PATTERN rel_path MIN_HOPS 1 MAX_HOPS 4 CYCLE_POLICY NO_REPEAT"},
        {"NP043-GOLD-004", "REDIS LUA EVAL SCRIPT return_1 KEYS (k1,k2) ARGS (a1,a2)"},
        {"NP043-GOLD-005", "REDIS STREAM GROUP CREATE STREAM orders GROUP grp_a START_ID 0-0"},
        {"NP043-GOLD-006", "REDIS STREAM GROUP READ STREAM orders GROUP grp_a CONSUMER c1 COUNT 32 BLOCK_MS 5000"}
    };

    for (const auto& vector : vectors) {
        auto first = trace(vector.sql);
        auto second = trace(vector.sql);
        ASSERT_TRUE(first.success()) << vector.case_id << " sql=" << vector.sql;
        ASSERT_TRUE(second.success()) << vector.case_id << " sql=" << vector.sql;

        EXPECT_FALSE(first.digest().normalized_sql.empty()) << vector.case_id;
        EXPECT_FALSE(first.digest().sql_hash.empty()) << vector.case_id;
        EXPECT_FALSE(first.digest().ast_hash.empty()) << vector.case_id;
        EXPECT_FALSE(first.digest().sblr_hash.empty()) << vector.case_id;
        EXPECT_FALSE(first.digest().root_opcode_symbol.empty()) << vector.case_id;

        EXPECT_EQ(first.digest().normalized_sql, second.digest().normalized_sql) << vector.case_id;
        EXPECT_EQ(first.digest().sql_hash, second.digest().sql_hash) << vector.case_id;
        EXPECT_EQ(first.digest().ast_hash, second.digest().ast_hash) << vector.case_id;
        EXPECT_EQ(first.digest().sblr_hash, second.digest().sblr_hash) << vector.case_id;
        EXPECT_EQ(first.digest().root_opcode_symbol, second.digest().root_opcode_symbol) << vector.case_id;
    }
}

TEST_F(SqlToSblrTraceDeterminismTest, GoldenAliasPairsNormalizeToEquivalentDigests) {
    struct AliasPair {
        const char* case_id;
        const char* a;
        const char* b;
    };
    const std::vector<AliasPair> pairs = {
        {"NP025-ALIAS-001", "DOC PATH FILTER PATH_ID 17 OP EQ VALUE_REF 42", "FILTER DOC PATH 17 = 42"},
        {"NP025-ALIAS-002", "TS BUCKET AGG TIME_EXPR 91 BUCKET_NS 60000000000 AGG_REFS (7, 8, 9)", "AGGREGATE TIME BUCKET 60000000000 BY 91 USING (7, 8, 9)"},
        {"NP025-ALIAS-003", "SEARCH QUERY DSL TARGET_INDEX 22 PAYLOAD '{\"q\":\"bird\"}' SCORER BM25", "SEARCH DSL '{\"q\":\"bird\"}' ON INDEX 22 SCORER BM25"},
        {"NP025-ALIAS-004", "VECTOR ANN QUERY INDEX 33 METRIC DOT TOPK 8 EF_SEARCH 40", "ANN INDEX 33 WITH METRIC DOT TOPK 8 EF 40"},
        {"NP025-ALIAS-005", "HYBRID BRIDGE EXCHANGE SOURCE_TRACK 1 TARGET_TRACK 2 MODE BROADCAST", "BRIDGE SOURCE 1 TARGET 2 MODE BROADCAST"},
        {"NP025-ALIAS-006", "UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES payload_trace SESSION_SIGNATURE sig_trace", "COMPILE EMBEDDED PAYLOAD native, SQL_TEXT, payload_trace, sig_trace"},
        {"NP025-ALIAS-007", "UDR VALIDATE SQL TEMPLATE TEMPLATE_ID tpl_trace SQL_TEXT 'SELECT 1' PROFILE native SESSION_SIGNATURE sig_trace", "VALIDATE SQL TEMPLATE tpl_trace USING 'SELECT 1' PROFILE native SIGNATURE sig_trace"},
        {"NP041-ALIAS-001", "SET SEQUENCE seq_docs TO 42", "SET GENERATOR seq_docs TO 42"},
        {"NP043-ALIAS-001", "SEARCH JOIN FIELD MAPPING INDEX 17 FIELD rel_join PARENT parent_doc CHILD child_doc ROUTING REQUIRED", "JOIN FIELD rel_join ON INDEX 17 PARENT parent_doc CHILD child_doc ROUTING REQUIRED"},
        {"NP043-ALIAS-002", "SEARCH PERCOLATOR FIELD INDEX 41 FIELD query_match QUERY_PARSER SIMPLE", "PERCOLATOR FIELD query_match ON INDEX 41 PARSER SIMPLE"},
        {"NP043-ALIAS-003", "GRAPH PATH MATCH PATTERN rel_path MIN_HOPS 1 MAX_HOPS 4 CYCLE_POLICY NO_REPEAT", "MATCH GRAPH PATH rel_path HOPS 1..4 NO CYCLES"},
        {"NP043-ALIAS-004", "REDIS LUA EVAL SCRIPT return_1 KEYS (k1,k2) ARGS (a1,a2)", "EVAL LUA return_1 KEYS (k1,k2) ARGS (a1,a2)"},
        {"NP043-ALIAS-005", "REDIS STREAM GROUP CREATE STREAM orders GROUP grp_a START_ID 0-0", "XGROUP CREATE orders grp_a 0-0"},
        {"NP043-ALIAS-006", "REDIS STREAM GROUP READ STREAM orders GROUP grp_a CONSUMER c1 COUNT 32 BLOCK_MS 5000", "XREADGROUP STREAM orders GROUP grp_a CONSUMER c1 COUNT 32 BLOCK 5000"}
    };

    for (const auto& pair : pairs) {
        auto a = trace(pair.a);
        auto b = trace(pair.b);
        ASSERT_TRUE(a.success()) << pair.case_id;
        ASSERT_TRUE(b.success()) << pair.case_id;

        EXPECT_EQ(a.digest().normalized_sql, b.digest().normalized_sql) << pair.case_id;
        EXPECT_EQ(a.digest().sql_hash, b.digest().sql_hash) << pair.case_id;
        EXPECT_EQ(a.digest().ast_hash, b.digest().ast_hash) << pair.case_id;
        EXPECT_EQ(a.digest().sblr_hash, b.digest().sblr_hash) << pair.case_id;
        EXPECT_EQ(a.digest().root_opcode_symbol, b.digest().root_opcode_symbol) << pair.case_id;
    }
}

TEST_F(SqlToSblrTraceDeterminismTest, GoldenRejectVectorsRemainDeterministic) {
    struct RejectVector {
        const char* case_id;
        const char* sql;
        const char* code;
    };

    const std::vector<RejectVector> rejects = {
        {"NP025-REJ-001", "DOC PATH FILTER PATH_ID 1 OP BAD VALUE_REF 2", "PRS_0504"},
        {"NP025-REJ-002", "TS BUCKET AGG TIME_EXPR 91 BUCKET_NS 10 AGG_REFS ()", "PRS_0504"},
        {"NP025-REJ-003", "SEARCH QUERY DSL TARGET_INDEX 22 PAYLOAD '{\"q\":\"bird\"}' SCORER BAD", "PRS_0504"},
        {"NP025-REJ-004", "VECTOR ANN QUERY INDEX 33 METRIC BAD TOPK 10 EF_SEARCH 20", "PRS_0504"},
        {"NP025-REJ-005", "HYBRID BRIDGE EXCHANGE SOURCE_TRACK 1 TARGET_TRACK 2 MODE BAD", "PRS_0504"},
        {"NP025-REJ-006", "UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES 'SELECT 1'", "PRS_0504"},
        {"NP025-REJ-007", "VALIDATE SQL TEMPLATE tpl_003 USING 'SELECT 9' PROFILE native", "PRS_0504"},
        {"NP025-REJ-008", "CREATE SCHEDULE sch_bad RRULE 'FREQ=DAILY;FREQ=MONTHLY' DTSTART '2026-02-17T00:00:00' TZ 'UTC'", "PRS_0507"},
        {"NP025-REJ-009", "CREATE DATABASE EMULATED unknown_profile localhost:db_main", "PRS_0503"},
        {"NP040-REJ-001", "SELECT id FROM docs FETCH FIRST 10 ROWS WITH TIES", "PRS_0504"},
        {"NP040-REJ-002", "SELECT id FROM docs ORDER BY id FETCH FIRST 1 ROW WITH TIES FOR UPDATE SKIP LOCKED", "PRS_0504"},
        {"NP040-REJ-003", "SELECT DISTINCT ON (user_id) user_id FROM logs", "PRS_0504"},
        {"NP040-REJ-004", "SELECT id FROM docs TABLESAMPLE INVALID (10)", "PRS_0504"},
        {"NP040-REJ-005", "SELECT sum(val) OVER (ORDER BY ts ROWS BETWEEN 1 PRECEDING AND CURRENT ROW EXCLUDE BAD) FROM metrics", "PRS_0504"},
        {"NP040-REJ-006", "WITH RECURSIVE t(n) AS (SELECT 1) SEARCH INVALID FIRST BY n SET ord SELECT n FROM t", "PRS_0504"},
        {"NP040-REJ-007", "SET CONSTRAINTS ALL", "PRS_0504"},
        {"NP040-REJ-008", "CREATE TABLE bad_def (id INT, CONSTRAINT bad_pk PRIMARY KEY (id) INITIALLY DEFERRED)", "PRS_0504"},
        {"NP040-REJ-009", "CREATE TABLE room_booking_bad (room_id INT, CONSTRAINT room_booking_excl EXCLUDE USING gist (room_id))", "PRS_0504"},
        {"NP040-REJ-010", "CREATE TABLE gen_docs_bad_1 (id INT, title TEXT, normalized_title TEXT GENERATED ALWAYS AS (lower(title)))", "PRS_0504"},
        {"NP040-REJ-011", "CREATE TABLE gen_docs_bad_2 (id INT, title TEXT, normalized_title TEXT GENERATED BY DEFAULT AS (lower(title)) STORED)", "PRS_0504"},
        {"NP040-REJ-012", "ALTER EXTENSION pg_trgm", "PRS_0504"},
        {"NP040-REJ-013", "CREATE PUBLICATION pub_bad", "PRS_0504"},
        {"NP040-REJ-014", "CREATE SUBSCRIPTION sub_bad", "PRS_0504"},
        {"NP040-REJ-015", "CREATE ACCESS METHOD am_bad TYPE BAD HANDLER h", "PRS_0504"},
        {"NP043-REJ-001", "SEARCH JOIN FIELD MAPPING INDEX 17 FIELD rel_join PARENT parent_doc CHILD child_doc ROUTING BAD", "PRS_0504"},
        {"NP043-REJ-002", "MATCH GRAPH PATH rel_path HOPS 9..4 NO CYCLES", "PRS_0504"},
        {"NP043-REJ-003", "REDIS STREAM GROUP READ STREAM orders GROUP grp_a CONSUMER c1 BLOCK_MS 5000", "PRS_0504"},
        {"NP043-REJ-004", "XCLAIM STREAM orders GROUP grp_a CONSUMER c1 MINIDLE -1 IDS (0-1)", "PRS_0504"}
    };

    for (const auto& reject : rejects) {
        auto first = compiler_->compileTrace(reject.sql);
        auto second = compiler_->compileTrace(reject.sql);
        EXPECT_FALSE(first.success()) << reject.case_id;
        EXPECT_FALSE(second.success()) << reject.case_id;
        EXPECT_TRUE(traceHasCode(first, reject.code)) << reject.case_id;
        EXPECT_TRUE(traceHasCode(second, reject.code)) << reject.case_id;
        EXPECT_FALSE(first.diagnostic_sql_context().empty()) << reject.case_id;
        EXPECT_EQ(first.diagnostic_sql_context(), second.diagnostic_sql_context()) << reject.case_id;
        ASSERT_FALSE(first.errors().empty()) << reject.case_id;
        ASSERT_FALSE(second.errors().empty()) << reject.case_id;
        EXPECT_EQ(first.errors().front(), second.errors().front()) << reject.case_id;
    }
}
