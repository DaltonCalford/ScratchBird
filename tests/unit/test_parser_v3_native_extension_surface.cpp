/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include <gtest/gtest.h>

#include <string>
#include <set>
#include <utility>

#include "scratchbird/parser/ast_v3.h"
#include "scratchbird/parser/parser_v3.h"

namespace {

using namespace scratchbird::parser::v3;

bool hasErrorCode(const ParseResult& result, const std::string& code) {
    for (const auto& err : result.errors()) {
        if (err.message.find(code) != std::string::npos) {
            return true;
        }
    }
    return false;
}

ParseResult parseWithDisabledFeatures(std::string_view sql, std::initializer_list<const char*> disabled) {
    ParserOptions options;
    for (const char* key : disabled) {
        options.disabled_feature_keys.insert(std::string(key));
    }
    Parser parser(sql, std::move(options));
    return parser.parseStatement();
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesCreateSearchAndVectorIndexCanonicalForms) {
    {
        Parser parser("CREATE SEARCH INDEX idx_search ON docs (title, body)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::CreateIndexStmt);
        auto* stmt = static_cast<CreateIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->index_type, IndexType::FULLTEXT);
        EXPECT_EQ(stmt->columns.size(), 2u);
    }

    {
        Parser parser("CREATE VECTOR INDEX idx_vec ON docs (embedding) METRIC COSINE TOPK_DEFAULT 25");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::CreateIndexStmt);
        auto* stmt = static_cast<CreateIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->index_type, IndexType::HNSW);
        EXPECT_EQ(stmt->columns.size(), 1u);
        EXPECT_EQ(stmt->option_assignments.size(), 2u);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsVectorMetricAndTopkDeterministically) {
    {
        Parser parser("CREATE VECTOR INDEX idx_vec ON docs (embedding) METRIC BAD TOPK_DEFAULT 10");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }

    {
        Parser parser("CREATE VECTOR INDEX idx_vec ON docs (embedding) METRIC L2 TOPK_DEFAULT 0");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesAlterDropSearchAndVectorIndexCanonicalForms) {
    {
        Parser parser("ALTER SEARCH INDEX idx_search REBUILD ONLINE");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterIndexStmt);
        auto* stmt = static_cast<AlterIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->action, AlterIndexAction::REBUILD);
        EXPECT_EQ(stmt->mode, IndexMaintenanceMode::ONLINE);
    }

    {
        Parser parser("ALTER VECTOR INDEX idx_vec REBUILD OFFLINE");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterIndexStmt);
        auto* stmt = static_cast<AlterIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->action, AlterIndexAction::REBUILD);
        EXPECT_EQ(stmt->mode, IndexMaintenanceMode::OFFLINE);
    }

    {
        Parser parser("DROP SEARCH INDEX idx_search");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::DropIndexStmt);
    }

    {
        Parser parser("DROP VECTOR INDEX idx_vec");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::DropIndexStmt);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesMeasurementSurface) {
    {
        Parser parser("CREATE MEASUREMENT cpu (host STRING, usage_user DOUBLE)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::CreateTableStmt);
        auto* stmt = static_cast<CreateTableStmt*>(result.statement());
        EXPECT_EQ(stmt->columns.size(), 2u);
    }

    {
        Parser parser("ALTER MEASUREMENT cpu RETENTION '30d'");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesScheduleRruleAndRruleSet) {
    {
        Parser parser(
            "CREATE SCHEDULE sch_daily RRULE 'FREQ=DAILY;INTERVAL=1' "
            "DTSTART '2026-02-17T00:00:00' TZ 'UTC'");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::CreateJobStmt);
        auto* stmt = static_cast<CreateJobStmt*>(result.statement());
        EXPECT_EQ(stmt->schedule_kind, JobScheduleKind::CRON);
    }

    {
        Parser parser(
            "ALTER SCHEDULE sch_union SET RRULE_SET "
            "('FREQ=DAILY;BYDAY=MO', 'FREQ=DAILY;BYDAY=TU') "
            "DTSTART '2026-02-17T00:00:00' TZ 'UTC'");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterJobStmt);
    }

    {
        Parser parser(
            "ALTER SCHEDULE sch_union SET RRULE_SET "
            "('FREQ=DAILY;BYDAY=MO', 'FREQ=DAILY;BYDAY=TU') "
            "DTSTART '2026-02-17T00:00:00' TZ 'UTC' "
            "RDATE ('2026-02-18T00:00:00') EXDATE ('2026-02-19T00:00:00')");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterJobStmt);
    }

    {
        Parser parser("DROP SCHEDULE sch_daily");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::DropJobStmt);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsInvalidScheduleRruleDeterministically) {
    {
        Parser parser(
            "CREATE SCHEDULE sch_bad RRULE 'FREQ=DAILY;FREQ=MONTHLY' "
            "DTSTART '2026-02-17T00:00:00' TZ 'UTC'");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0507"));
    }

    {
        Parser parser("CREATE SCHEDULE sch_bad RRULE 'FREQ=DAILY' TZ 'UTC'");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0507"));
    }

    {
        Parser parser(
            "ALTER SCHEDULE sch_bad SET RRULE_SET ('FREQ=DAILY', 'FREQ=DAILY') "
            "DTSTART '2026-02-17T00:00:00' TZ 'UTC'");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0507"));
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesFetchClauseWithTiesAndDefaultRowCount) {
    {
        Parser parser("SELECT id FROM docs ORDER BY id FETCH FIRST 10 ROWS WITH TIES");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
        auto* stmt = static_cast<SelectStmt*>(result.statement());
        EXPECT_EQ(stmt->fetch_mode, FetchMode::FIRST);
        EXPECT_TRUE(stmt->fetch_with_ties);
        ASSERT_NE(stmt->fetch_row_count, nullptr);
        ASSERT_EQ(stmt->fetch_row_count->kind(), ASTKind::LiteralExpr);
        auto* row_count = static_cast<LiteralExpr*>(stmt->fetch_row_count);
        EXPECT_EQ(row_count->literal_type, LiteralType::INTEGER);
        EXPECT_EQ(row_count->int_value, 10);
    }

    {
        Parser parser("SELECT id FROM docs ORDER BY id FETCH NEXT ROW ONLY");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
        auto* stmt = static_cast<SelectStmt*>(result.statement());
        EXPECT_EQ(stmt->fetch_mode, FetchMode::NEXT);
        EXPECT_FALSE(stmt->fetch_with_ties);
        ASSERT_NE(stmt->fetch_row_count, nullptr);
        ASSERT_EQ(stmt->fetch_row_count->kind(), ASTKind::LiteralExpr);
        auto* row_count = static_cast<LiteralExpr*>(stmt->fetch_row_count);
        EXPECT_EQ(row_count->literal_type, LiteralType::INTEGER);
        EXPECT_EQ(row_count->int_value, 1);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsFetchWithTiesWithoutOrderBy) {
    Parser parser("SELECT id FROM docs FETCH FIRST 10 ROWS WITH TIES");
    auto result = parser.parseStatement();
    EXPECT_FALSE(result.success());
    EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesPostgreSqlRowLockStrengthsDeterministically) {
    {
        Parser parser("SELECT id FROM docs FOR UPDATE");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
        auto* stmt = static_cast<SelectStmt*>(result.statement());
        EXPECT_EQ(stmt->lock_strength, SelectLockStrength::UPDATE);
        EXPECT_TRUE(stmt->for_update);
        EXPECT_FALSE(stmt->for_share);
    }

    {
        Parser parser("SELECT id FROM docs FOR NO KEY UPDATE");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
        auto* stmt = static_cast<SelectStmt*>(result.statement());
        EXPECT_EQ(stmt->lock_strength, SelectLockStrength::NO_KEY_UPDATE);
        EXPECT_TRUE(stmt->for_update);
        EXPECT_FALSE(stmt->for_share);
    }

    {
        Parser parser("SELECT id FROM docs FOR SHARE");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
        auto* stmt = static_cast<SelectStmt*>(result.statement());
        EXPECT_EQ(stmt->lock_strength, SelectLockStrength::SHARE);
        EXPECT_FALSE(stmt->for_update);
        EXPECT_TRUE(stmt->for_share);
    }

    {
        Parser parser("SELECT id FROM docs FOR KEY SHARE NOWAIT");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
        auto* stmt = static_cast<SelectStmt*>(result.statement());
        EXPECT_EQ(stmt->lock_strength, SelectLockStrength::KEY_SHARE);
        EXPECT_FALSE(stmt->for_update);
        EXPECT_TRUE(stmt->for_share);
        EXPECT_TRUE(stmt->nowait);
        EXPECT_FALSE(stmt->skip_locked);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsFetchWithTiesCombinedWithSkipLocked) {
    Parser parser("SELECT id FROM docs ORDER BY id FETCH FIRST 1 ROW WITH TIES FOR UPDATE SKIP LOCKED");
    auto result = parser.parseStatement();
    EXPECT_FALSE(result.success());
    EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesDistinctOnWithDeterministicOrderBy) {
    Parser parser(
        "SELECT DISTINCT ON (user_id) user_id, created_at "
        "FROM logs ORDER BY user_id, created_at DESC");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());
    ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
    auto* stmt = static_cast<SelectStmt*>(result.statement());
    EXPECT_TRUE(stmt->distinct);
    ASSERT_EQ(stmt->distinct_on.size(), 1u);
    ASSERT_EQ(stmt->order_by.size(), 2u);
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsDistinctOnWithoutOrderBy) {
    Parser parser("SELECT DISTINCT ON (user_id) user_id FROM logs");
    auto result = parser.parseStatement();
    EXPECT_FALSE(result.success());
    EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesLateralWithOrdinalitySources) {
    {
        Parser parser(
            "SELECT g.val, g.ord "
            "FROM LATERAL generate_series(1, 3) WITH ORDINALITY AS g(val, ord)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
        auto* stmt = static_cast<SelectStmt*>(result.statement());
        ASSERT_NE(stmt->from, nullptr);
        EXPECT_EQ(stmt->from->ref_type, TableRefNode::Type::FUNCTION);
        EXPECT_TRUE(stmt->from->lateral);
        EXPECT_TRUE(stmt->from->with_ordinality);
    }

    {
        Parser parser("SELECT v FROM generate_series(1, 3) WITH ORDINALITY AS g(v, ord)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
        auto* stmt = static_cast<SelectStmt*>(result.statement());
        ASSERT_NE(stmt->from, nullptr);
        EXPECT_EQ(stmt->from->ref_type, TableRefNode::Type::FUNCTION);
        EXPECT_FALSE(stmt->from->lateral);
        EXPECT_TRUE(stmt->from->with_ordinality);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesTableSampleWithRepeatableSeed) {
    Parser parser("SELECT id FROM docs TABLESAMPLE BERNOULLI (10) REPEATABLE (42)");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());
    ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
    auto* stmt = static_cast<SelectStmt*>(result.statement());
    ASSERT_NE(stmt->from, nullptr);
    EXPECT_EQ(stmt->from->sample_method, TableSampleMethod::BERNOULLI);
    ASSERT_NE(stmt->from->sample_percent, nullptr);
    ASSERT_NE(stmt->from->sample_repeatable_seed, nullptr);
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsInvalidTableSampleForms) {
    {
        Parser parser("SELECT id FROM docs TABLESAMPLE INVALID (10)");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }

    {
        Parser parser("SELECT id FROM docs TABLESAMPLE SYSTEM ()");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }

    {
        Parser parser("SELECT v FROM generate_series(1, 3) TABLESAMPLE BERNOULLI (10)");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesGroupingSetsRollupAndCube) {
    {
        Parser parser("SELECT region, sum(amount) FROM sales GROUP BY ROLLUP(region)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
        auto* stmt = static_cast<SelectStmt*>(result.statement());
        EXPECT_EQ(stmt->grouping_type, GroupingType::ROLLUP);
        ASSERT_EQ(stmt->group_by.size(), 1u);
    }

    {
        Parser parser("SELECT region, product, sum(amount) FROM sales GROUP BY CUBE(region, product)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
        auto* stmt = static_cast<SelectStmt*>(result.statement());
        EXPECT_EQ(stmt->grouping_type, GroupingType::CUBE);
        ASSERT_EQ(stmt->group_by.size(), 2u);
    }

    {
        Parser parser("SELECT sum(amount) FROM sales GROUP BY GROUPING SETS ((region), (product), ())");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
        auto* stmt = static_cast<SelectStmt*>(result.statement());
        EXPECT_EQ(stmt->grouping_type, GroupingType::GROUPING_SETS);
        ASSERT_EQ(stmt->grouping_sets.size(), 3u);
        EXPECT_EQ(stmt->grouping_sets[2].size(), 0u);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesAggregateFilterClause) {
    Parser parser("SELECT count(*) FILTER (WHERE status = 'ok') FROM logs");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());
    ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
    auto* stmt = static_cast<SelectStmt*>(result.statement());
    ASSERT_EQ(stmt->items.size(), 1u);
    ASSERT_NE(stmt->items[0], nullptr);
    ASSERT_NE(stmt->items[0]->expr, nullptr);
    ASSERT_EQ(stmt->items[0]->expr->kind(), ASTKind::FunctionCallExpr);
    auto* fn = static_cast<FunctionCallExpr*>(stmt->items[0]->expr);
    ASSERT_NE(fn->filter, nullptr);
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesWindowExcludeClauseAndRejectsInvalidForm) {
    {
        Parser parser(
            "SELECT sum(val) OVER (ORDER BY ts ROWS BETWEEN 1 PRECEDING AND CURRENT ROW EXCLUDE TIES) "
            "FROM metrics");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
        auto* stmt = static_cast<SelectStmt*>(result.statement());
        ASSERT_EQ(stmt->items.size(), 1u);
        ASSERT_NE(stmt->items[0], nullptr);
        ASSERT_NE(stmt->items[0]->expr, nullptr);
        ASSERT_EQ(stmt->items[0]->expr->kind(), ASTKind::FunctionCallExpr);
        auto* fn = static_cast<FunctionCallExpr*>(stmt->items[0]->expr);
        ASSERT_TRUE(fn->is_window);
        ASSERT_NE(fn->window, nullptr);
        EXPECT_TRUE(fn->window->has_frame);
        EXPECT_TRUE(fn->window->has_frame_exclusion);
        EXPECT_EQ(fn->window->frame_exclusion, FrameExclusion::TIES);
    }

    {
        Parser parser(
            "SELECT sum(val) OVER (ORDER BY ts ROWS BETWEEN 1 PRECEDING AND CURRENT ROW EXCLUDE BAD) "
            "FROM metrics");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesWithRecursiveSearchAndCycleClauses) {
    Parser parser(
        "WITH RECURSIVE t(id, parent_id) AS ("
        "  SELECT id, parent_id FROM nodes "
        "  UNION ALL "
        "  SELECT n.id, n.parent_id FROM nodes n JOIN t ON n.parent_id = t.id"
        ") "
        "SEARCH DEPTH FIRST BY id SET walk_order "
        "CYCLE id SET is_cycle TO true DEFAULT false USING cycle_path "
        "SELECT id FROM t");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());
    ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
    auto* stmt = static_cast<SelectStmt*>(result.statement());
    ASSERT_NE(stmt->with, nullptr);
    ASSERT_EQ(stmt->with->ctes.size(), 1u);
    const auto& cte = stmt->with->ctes[0];
    EXPECT_TRUE(cte.recursive);
    EXPECT_TRUE(cte.has_search);
    EXPECT_EQ(cte.search_order, CTE::SearchOrder::DEPTH_FIRST);
    ASSERT_EQ(cte.search_by_columns.size(), 1u);
    EXPECT_TRUE(cte.has_cycle);
    ASSERT_EQ(cte.cycle_columns.size(), 1u);
    EXPECT_TRUE(cte.has_cycle_mark_value);
    EXPECT_TRUE(cte.has_cycle_default_value);
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsInvalidWithRecursiveSearchCycleForms) {
    {
        Parser parser(
            "WITH RECURSIVE t(n) AS (SELECT 1) "
            "SEARCH INVALID FIRST BY n SET ord "
            "SELECT n FROM t");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }

    {
        Parser parser(
            "WITH RECURSIVE t(n) AS (SELECT 1) "
            "CYCLE SET is_cycle USING cycle_path "
            "SELECT n FROM t");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesDeferrableConstraintSurfaces) {
    {
        Parser parser(
            "CREATE TABLE child_def ("
            "  id INT, "
            "  parent_id INT, "
            "  CONSTRAINT child_def_pk PRIMARY KEY (id) DEFERRABLE INITIALLY DEFERRED, "
            "  CONSTRAINT child_def_fk FOREIGN KEY (parent_id) REFERENCES parent_def(id) DEFERRABLE INITIALLY IMMEDIATE, "
            "  CONSTRAINT child_def_uq UNIQUE (parent_id) DEFERRABLE INITIALLY DEFERRED"
            ")");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::CreateTableStmt);
        auto* stmt = static_cast<CreateTableStmt*>(result.statement());
        ASSERT_EQ(stmt->constraints.size(), 3u);
        EXPECT_TRUE(stmt->constraints[0]->deferrable);
        EXPECT_TRUE(stmt->constraints[0]->initially_deferred);
        EXPECT_TRUE(stmt->constraints[1]->deferrable);
        EXPECT_TRUE(stmt->constraints[1]->initially_immediate);
        EXPECT_TRUE(stmt->constraints[2]->deferrable);
        EXPECT_TRUE(stmt->constraints[2]->initially_deferred);
    }

    {
        Parser parser("SET CONSTRAINTS ALL DEFERRED");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::SetStmt);
        auto* stmt = static_cast<SetStmt*>(result.statement());
        EXPECT_EQ(stmt->set_type, SetStmt::SetType::CONSTRAINTS);
        EXPECT_TRUE(stmt->constraints_all);
        EXPECT_TRUE(stmt->constraints_deferred);
    }

    {
        Parser parser("SET CONSTRAINTS child_def_fk, child_def_uq IMMEDIATE");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::SetStmt);
        auto* stmt = static_cast<SetStmt*>(result.statement());
        EXPECT_EQ(stmt->set_type, SetStmt::SetType::CONSTRAINTS);
        EXPECT_FALSE(stmt->constraints_all);
        EXPECT_EQ(stmt->constraint_names.size(), 2u);
        EXPECT_FALSE(stmt->constraints_deferred);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsInvalidDeferrableConstraintSurfaces) {
    {
        Parser parser("SET CONSTRAINTS ALL");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }

    {
        Parser parser("CREATE TABLE bad_def_1 (id INT, CONSTRAINT bad_ck CHECK (id > 0) DEFERRABLE)");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }

    {
        Parser parser("CREATE TABLE bad_def_2 (id INT, CONSTRAINT bad_pk PRIMARY KEY (id) INITIALLY DEFERRED)");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }

    {
        Parser parser(
            "CREATE TABLE bad_def_3 (id INT, CONSTRAINT bad_pk PRIMARY KEY (id) NOT DEFERRABLE INITIALLY DEFERRED)");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesExcludeUsingConstraintSurfaces) {
    Parser parser(
        "CREATE TABLE room_booking ("
        "  room_id INT, "
        "  during TSRANGE, "
        "  CONSTRAINT room_booking_excl "
        "    EXCLUDE USING gist (room_id WITH =, during WITH &&) "
        "    WHERE (room_id > 0) "
        "    DEFERRABLE INITIALLY IMMEDIATE"
        ")");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success()) << "expected parse success";
    ASSERT_EQ(result.statement()->kind(), ASTKind::CreateTableStmt);
    auto* stmt = static_cast<CreateTableStmt*>(result.statement());
    ASSERT_EQ(stmt->constraints.size(), 1u);
    auto* c = stmt->constraints[0];
    EXPECT_EQ(c->type, TableConstraintType::EXCLUDE);
    EXPECT_EQ(c->exclude_expressions.size(), 2u);
    EXPECT_EQ(c->exclude_operators.size(), 2u);
    EXPECT_NE(c->exclude_where, nullptr);
    EXPECT_TRUE(c->deferrable);
    EXPECT_TRUE(c->initially_immediate);
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsInvalidExcludeUsingConstraintSurfaces) {
    {
        Parser parser(
            "CREATE TABLE room_booking_bad_1 ("
            "  room_id INT, "
            "  CONSTRAINT room_booking_excl EXCLUDE (room_id WITH =)"
            ")");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }

    {
        Parser parser(
            "CREATE TABLE room_booking_bad_2 ("
            "  room_id INT, "
            "  CONSTRAINT room_booking_excl EXCLUDE USING gist (room_id)"
            ")");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesGeneratedStoredColumnSurface) {
    Parser parser(
        "CREATE TABLE gen_docs ("
        "  id INT, "
        "  title TEXT, "
        "  normalized_title TEXT GENERATED ALWAYS AS (lower(title)) STORED"
        ")");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success()) << "expected parse success";
    ASSERT_EQ(result.statement()->kind(), ASTKind::CreateTableStmt);
    auto* stmt = static_cast<CreateTableStmt*>(result.statement());
    ASSERT_EQ(stmt->columns.size(), 3u);

    const auto* col = stmt->columns[2];
    ASSERT_NE(col, nullptr);
    ASSERT_EQ(col->constraints.size(), 1u);
    const auto& generated = col->constraints[0];
    EXPECT_EQ(generated.type, ConstraintType::GENERATED);
    EXPECT_TRUE(generated.generated_always);
    EXPECT_TRUE(generated.generated_stored);
    EXPECT_NE(generated.generated_expr, nullptr);
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsInvalidGeneratedStoredColumnSurface) {
    {
        Parser parser(
            "CREATE TABLE gen_docs_bad_1 ("
            "  id INT, "
            "  title TEXT, "
            "  normalized_title TEXT GENERATED ALWAYS AS (lower(title))"
            ")");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }

    {
        Parser parser(
            "CREATE TABLE gen_docs_bad_2 ("
            "  id INT, "
            "  title TEXT, "
            "  normalized_title TEXT GENERATED BY DEFAULT AS (lower(title)) STORED"
            ")");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesTableInheritanceSurface) {
    {
        Parser parser("CREATE TABLE child_tbl (id INT) INHERITS (base_tbl)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::CreateTableStmt);
        auto* stmt = static_cast<CreateTableStmt*>(result.statement());
        ASSERT_EQ(stmt->inherits.size(), 1u);
    }

    {
        Parser parser("ALTER TABLE child_tbl INHERIT base_tbl");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterTableStmt);
        auto* stmt = static_cast<AlterTableStmt*>(result.statement());
        EXPECT_EQ(stmt->action, AlterTableAction::INHERIT);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesExtensionLifecycleSurfaces) {
    {
        Parser parser("CREATE EXTENSION IF NOT EXISTS pg_trgm WITH SCHEMA ext");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
    }
    {
        Parser parser("ALTER EXTENSION pg_trgm UPDATE TO '1.1'");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
    }
    {
        Parser parser("DROP EXTENSION IF EXISTS pg_trgm CASCADE");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesInstallLoadExtensionSurfaces) {
    {
        Parser parser("INSTALL EXTENSION httpfs");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
        auto* stmt = static_cast<AlterSystemStmt*>(result.statement());
        EXPECT_NE(stmt->name, StringPool::INVALID_ID);
    }
    {
        Parser parser("LOAD httpfs");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
        auto* stmt = static_cast<AlterSystemStmt*>(result.statement());
        EXPECT_NE(stmt->name, StringPool::INVALID_ID);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesCassandraConditionalWriteAndConsistencySurfaces) {
    {
        Parser parser(
            "INSERT INTO docs (id, title) VALUES (1, 'x') "
            "USING CONSISTENCY QUORUM AND SERIAL CONSISTENCY LOCAL_SERIAL "
            "IF NOT EXISTS");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::InsertStmt);
        auto* stmt = static_cast<InsertStmt*>(result.statement());
        EXPECT_TRUE(stmt->conditional_if_not_exists);
        EXPECT_EQ(stmt->consistency_level, parser.stringPool().intern("QUORUM"));
        EXPECT_EQ(stmt->serial_consistency_level, parser.stringPool().intern("LOCAL_SERIAL"));
    }

    {
        Parser parser("UPDATE docs SET title = 'y' WHERE id = 1 WITH CONSISTENCY ONE IF EXISTS");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::UpdateStmt);
        auto* stmt = static_cast<UpdateStmt*>(result.statement());
        EXPECT_TRUE(stmt->conditional_if_exists);
        EXPECT_EQ(stmt->consistency_level, parser.stringPool().intern("ONE"));
    }

    {
        Parser parser("DELETE FROM docs WHERE id = 1 IF title = 'x'");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::DeleteStmt);
        auto* stmt = static_cast<DeleteStmt*>(result.statement());
        EXPECT_NE(stmt->conditional_if, nullptr);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesClickHouseAndDuckDbTypeAliases) {
    Parser parser(
        "CREATE TABLE t_types ("
        "  agg AGGREGATEFUNCTION(sum, UInt64), "
        "  sagg SIMPLEAGGREGATEFUNCTION(sum, UInt64), "
        "  dyn DYNAMIC(JSON), "
        "  bits QBIT(128), "
        "  big BIGNUM(256)"
        ")");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());
    ASSERT_EQ(result.statement()->kind(), ASTKind::CreateTableStmt);
    auto* stmt = static_cast<CreateTableStmt*>(result.statement());
    ASSERT_EQ(stmt->columns.size(), 5u);
    EXPECT_FALSE(stmt->columns[0]->type.type_arguments.empty());
    EXPECT_FALSE(stmt->columns[1]->type.type_arguments.empty());
    EXPECT_FALSE(stmt->columns[2]->type.type_arguments.empty());
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesRuntimeConsistencyAndSingleWriterSetSurfaces) {
    {
        Parser parser("SET CONSISTENCY QUORUM");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SetStmt);
    }
    {
        Parser parser("SET SERIAL CONSISTENCY LOCAL_SERIAL");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SetStmt);
    }
    {
        Parser parser("SET CONCURRENCY MODE SINGLE_WRITER");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SetStmt);
    }
    {
        Parser parser("SET SINGLE_WRITER ON");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SetStmt);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesPublicationSubscriptionSurfaces) {
    {
        Parser parser("CREATE PUBLICATION pub_all FOR ALL TABLES");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
    }
    {
        Parser parser(
            "CREATE SUBSCRIPTION sub_main CONNECTION 'host=127.0.0.1 dbname=main' PUBLICATION pub_all");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesAccessMethodStatisticsAndTransformSurfaces) {
    {
        Parser parser("CREATE ACCESS METHOD am_btree TYPE INDEX HANDLER amhandler_btree");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
    }
    {
        Parser parser("CREATE STATISTICS st_sales (ndistinct, dependencies) ON region, product FROM sales");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
    }
    {
        Parser parser(
            "CREATE TRANSFORM FOR jsonb LANGUAGE plpython3u "
            "(FROM SQL WITH FUNCTION jsonb_from_sql, TO SQL WITH FUNCTION jsonb_to_sql)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesCopyProgramAndExplainWalSurfaces) {
    {
        Parser parser("COPY docs TO PROGRAM 'gzip > /tmp/docs.gz' WITH (FORMAT CSV, HEADER)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::CopyStmt);
        auto* stmt = static_cast<CopyStmt*>(result.statement());
        EXPECT_TRUE(stmt->target_is_program);
    }

    {
        Parser parser("EXPLAIN (ANALYZE, BUFFERS, WAL, FORMAT JSON) SELECT id FROM docs");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::ExplainStmt);
        auto* stmt = static_cast<ExplainStmt*>(result.statement());
        EXPECT_TRUE(stmt->analyze);
        EXPECT_TRUE(stmt->buffers);
        EXPECT_TRUE(stmt->wal);
        EXPECT_TRUE(stmt->format_json);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesSecurityLabelAndIdentitySwitchSurfaces) {
    {
        Parser parser("SECURITY LABEL FOR sec_provider ON TABLE docs IS 'classified'");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
    }
    {
        Parser parser("SET SESSION AUTHORIZATION app_user");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SetStmt);
        auto* stmt = static_cast<SetStmt*>(result.statement());
        EXPECT_EQ(stmt->set_type, SetStmt::SetType::SESSION_AUTHORIZATION);
    }
    {
        Parser parser("SET ROLE app_readonly");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SetStmt);
        auto* stmt = static_cast<SetStmt*>(result.statement());
        EXPECT_EQ(stmt->set_type, SetStmt::SetType::ROLE);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesRlsControlSurfaces) {
    {
        Parser parser("ALTER TABLE docs ENABLE ROW LEVEL SECURITY");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterTableStmt);
        auto* stmt = static_cast<AlterTableStmt*>(result.statement());
        EXPECT_EQ(stmt->action, AlterTableAction::ENABLE_RLS);
    }
    {
        Parser parser("ALTER TABLE docs FORCE ROW LEVEL SECURITY");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterTableStmt);
        auto* stmt = static_cast<AlterTableStmt*>(result.statement());
        EXPECT_EQ(stmt->action, AlterTableAction::FORCE_RLS);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesFirebirdSelectAliasSurfacesIntoCanonicalFields) {
    Parser parser(
        "SELECT FIRST 10 SKIP 5 id "
        "FROM docs "
        "PLAN NATURAL "
        "OPTIMIZE FOR 100 ROWS "
        "FOR UPDATE WITH LOCK");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());
    ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
    auto* stmt = static_cast<SelectStmt*>(result.statement());
    ASSERT_NE(stmt->limit, nullptr);
    ASSERT_NE(stmt->offset, nullptr);
    ASSERT_NE(stmt->firebird_plan, nullptr);
    ASSERT_NE(stmt->optimize_for_rows, nullptr);
    EXPECT_TRUE(stmt->for_update);
    EXPECT_TRUE(stmt->with_lock);
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesFirebirdRowsToAlias) {
    Parser parser("SELECT id FROM docs ORDER BY id ROWS 3 TO 7");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());
    ASSERT_EQ(result.statement()->kind(), ASTKind::SelectStmt);
    auto* stmt = static_cast<SelectStmt*>(result.statement());
    ASSERT_NE(stmt->limit, nullptr);
    ASSERT_NE(stmt->offset, nullptr);
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesFirebirdUpsertAndOverridingAliases) {
    {
        Parser parser(
            "UPDATE OR INSERT INTO docs (id, name) VALUES (1, 'alpha') MATCHING (id) RETURNING NEW.*");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::InsertStmt);
        auto* stmt = static_cast<InsertStmt*>(result.statement());
        ASSERT_NE(stmt->on_conflict, nullptr);
        EXPECT_EQ(stmt->on_conflict->action, ConflictAction::UPDATE);
        ASSERT_EQ(stmt->on_conflict->columns.size(), 1u);
        ASSERT_EQ(stmt->returning.size(), 1u);
    }

    {
        Parser parser("INSERT OVERRIDING SYSTEM VALUE INTO docs (id) VALUES (1)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::InsertStmt);
        auto* stmt = static_cast<InsertStmt*>(result.statement());
        EXPECT_EQ(stmt->overriding, InsertStmt::OverridingMode::SYSTEM);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesTypeOfAndDeclareExternalFunctionAliases) {
    {
        Parser parser("CREATE TABLE docs_t (id TYPE OF COLUMN docs.id, code TYPE OF doc_code_dom)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::CreateTableStmt);
        auto* stmt = static_cast<CreateTableStmt*>(result.statement());
        ASSERT_EQ(stmt->columns.size(), 2u);
        EXPECT_TRUE(stmt->columns[0]->type.is_type_of);
        EXPECT_TRUE(stmt->columns[0]->type.is_type_of_column);
        EXPECT_TRUE(stmt->columns[1]->type.is_type_of);
        EXPECT_FALSE(stmt->columns[1]->type.is_type_of_column);
    }

    {
        Parser parser(
            "DECLARE EXTERNAL FUNCTION udf_add INTEGER, INTEGER "
            "RETURNS INTEGER ENTRY_POINT 'udf_add' MODULE_NAME 'udf_lib'");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::CreateUdrStmt);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesFirebirdCreateOrAlterAndRecreateAliases) {
    {
        Parser parser("CREATE OR ALTER PROCEDURE p_demo AS BEGIN END");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::CreateProcedureStmt);
        auto* stmt = static_cast<CreateProcedureStmt*>(result.statement());
        EXPECT_TRUE(stmt->or_replace);
    }

    {
        Parser parser("RECREATE VIEW v_demo AS SELECT 1");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::CreateViewStmt);
        auto* stmt = static_cast<CreateViewStmt*>(result.statement());
        EXPECT_TRUE(stmt->or_replace);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesFirebirdDatabaseTriggerAndExecuteStatementOptions) {
    {
        Parser parser(
            "CREATE TRIGGER trg_connect ON CONNECT SQL SECURITY DEFINER AS BEGIN POST_EVENT 'evt'; END");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::CreateTriggerStmt);
        auto* stmt = static_cast<CreateTriggerStmt*>(result.statement());
        EXPECT_TRUE(stmt->is_database_trigger);
        EXPECT_TRUE(stmt->has_sql_security);
        EXPECT_EQ(stmt->sql_security, RoutineSqlSecurity::DEFINER);
        EXPECT_TRUE((stmt->event_mask &
                    (1u << static_cast<uint8_t>(TriggerEvent::CONNECT))) != 0u);
    }

    {
        Parser parser(
            "EXECUTE BLOCK AS BEGIN "
            "EXECUTE STATEMENT 'SELECT 1' "
            "ON EXTERNAL DATA SOURCE 'srv' "
            "AS USER 'alice' PASSWORD 'secret' ROLE 'r1' "
            "WITH AUTONOMOUS TRANSACTION WITH CALLER PRIVILEGES; "
            "END");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::ExecuteBlockStmt);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesSetGeneratorAlias) {
    {
        Parser parser("SET SEQUENCE seq_docs TO 42");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SetStmt);
        auto* stmt = static_cast<SetStmt*>(result.statement());
        EXPECT_EQ(stmt->set_type, SetStmt::SetType::GENERATOR);
        ASSERT_NE(stmt->value, nullptr);
    }

    {
        Parser parser("SET GENERATOR seq_docs TO 42");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::SetStmt);
        auto* stmt = static_cast<SetStmt*>(result.statement());
        EXPECT_EQ(stmt->set_type, SetStmt::SetType::GENERATOR);
        ASSERT_NE(stmt->value, nullptr);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesUserLifecycleSurfaces) {
    {
        Parser parser("CREATE USER app_user WITH PASSWORD 'pw' NOSUPERUSER");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::CreateUserStmt);
    }

    {
        Parser parser("ALTER USER app_user WITH PASSWORD 'pw2' SUPERUSER");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
    }

    {
        Parser parser("DROP USER IF EXISTS app_user CASCADE");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::DropUserStmt);
        auto* stmt = static_cast<DropUserStmt*>(result.statement());
        EXPECT_TRUE(stmt->if_exists);
        EXPECT_TRUE(stmt->cascade);
        ASSERT_EQ(stmt->users.size(), 1u);
    }

    {
        Parser parser("DROP USER MAPPING IF EXISTS FOR PUBLIC SERVER ext_srv");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::DropUserMappingStmt);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsInvalidAlterUserDeterministically) {
    Parser parser("ALTER USER app_user");
    auto result = parser.parseStatement();
    EXPECT_FALSE(result.success());
}

TEST(ParserV3NativeExtensionSurfaceTest, ValidatesEmulatedProfileIdentifier) {
    {
        Parser parser("CREATE DATABASE EMULATED postgresql localhost:db_main");
        auto result = parser.parseStatement();
        EXPECT_TRUE(result.success()) << "expected known profile to parse";
    }

    {
        Parser parser("CREATE DATABASE EMULATED unknown_profile localhost:db_main");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0503"));
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ValidatesSection05BuiltinFunctionArityAndUnknownSymbols) {
    {
        Parser parser("SELECT DOC_PATH_EXISTS('{}', '$.a')");
        auto result = parser.parseStatement();
        EXPECT_TRUE(result.success());
    }
    {
        Parser parser("SELECT DOC_PATH_EXISTS('{}')");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }
    {
        Parser parser("SELECT COMPILE_EMBEDDED_PAYLOAD('p','fmt','bytes','sig')");
        auto result = parser.parseStatement();
        EXPECT_TRUE(result.success());
    }
    {
        Parser parser("SELECT COMPILE_EMBEDDED_PAYLOAD('p','fmt','bytes')");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }
    {
        Parser parser("SELECT VECTOR_UNKNOWN_FUNC(1, 2)");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0506"));
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesSecurityNativeSurfaces) {
    {
        Parser parser(
            "CREATE CONNECTION RULE ch_src "
            "ORDER 5 "
            "MATCH (TRANSPORT=TLS, SOURCE='10.0.0.0/8', PRINCIPAL='ch_%') "
            "REQUIRE (TLS=TLS, PROVIDER=INTERNAL) "
            "ACTION ALLOW EXPECT VERSION 1");
        auto result = parser.parseStatement();
        EXPECT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
    }
    {
        Parser parser("CREATE TOKEN ifx_reader WITH SCOPE (ALLOW BUCKET 'cpu_metrics' ACTION READ)");
        auto result = parser.parseStatement();
        EXPECT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
    }
    {
        Parser parser("CREATE QUOTA PROFILE q1 (MAX_REQUESTS_PER_SEC=1000, WINDOW_MS=1000)");
        auto result = parser.parseStatement();
        EXPECT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
    }
    {
        Parser parser("REVOKE TOKEN ifx_reader");
        auto result = parser.parseStatement();
        EXPECT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsInvalidTokenScopeModelDeterministically) {
    Parser parser("CREATE TOKEN bad WITH SCOPE_MODEL CUSTOM_FOO SCOPE (ALLOW BUCKET 'cpu' ACTION READ)");
    auto result = parser.parseStatement();
    EXPECT_FALSE(result.success());
    EXPECT_TRUE(hasErrorCode(result, "SEC_1256"));
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesDocPathFilterStatementAndClauseForms) {
    {
        Parser parser("DOC PATH FILTER PATH_ID 17 OP EQ VALUE_REF 42");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AST_DOC_PATH_FILTER);
        auto* stmt = static_cast<DocPathFilterStmt*>(result.statement());
        EXPECT_EQ(stmt->path_expr, 17u);
        EXPECT_EQ(stmt->compare_op, 0u);
        EXPECT_EQ(stmt->value_expr, 42u);
    }
    {
        Parser parser("FILTER DOC PATH 11 >= 9");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AST_DOC_PATH_FILTER);
        auto* stmt = static_cast<DocPathFilterStmt*>(result.statement());
        EXPECT_EQ(stmt->path_expr, 11u);
        EXPECT_EQ(stmt->compare_op, 5u);
        EXPECT_EQ(stmt->value_expr, 9u);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsInvalidDocPathFilterComparator) {
    Parser parser("DOC PATH FILTER PATH_ID 1 OP BAD VALUE_REF 2");
    auto result = parser.parseStatement();
    EXPECT_FALSE(result.success());
    EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesTimeBucketAggregateStatementAndClauseForms) {
    {
        Parser parser("TS BUCKET AGG TIME_EXPR 91 BUCKET_NS 60000000000 AGG_REFS (7, 8, 9)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AST_TS_BUCKET_AGG);
        auto* stmt = static_cast<TsBucketAggStmt*>(result.statement());
        EXPECT_EQ(stmt->time_expr, 91u);
        EXPECT_EQ(stmt->bucket_size, 60000000000ULL);
        ASSERT_EQ(stmt->agg_refs.size(), 3u);
    }
    {
        Parser parser("AGGREGATE TIME BUCKET 60000000000 BY 91 USING (7, 8)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AST_TS_BUCKET_AGG);
        auto* stmt = static_cast<TsBucketAggStmt*>(result.statement());
        EXPECT_EQ(stmt->time_expr, 91u);
        EXPECT_EQ(stmt->bucket_size, 60000000000ULL);
        ASSERT_EQ(stmt->agg_refs.size(), 2u);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsInvalidTimeBucketAggregateList) {
    Parser parser("TS BUCKET AGG TIME_EXPR 91 BUCKET_NS 10 AGG_REFS ()");
    auto result = parser.parseStatement();
    EXPECT_FALSE(result.success());
    EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesSearchDslStatementAndClauseForms) {
    {
        Parser parser("SEARCH QUERY DSL TARGET_INDEX 22 PAYLOAD '{\"q\":\"bird\"}' SCORER BM25");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AST_SEARCH_QUERY_DSL);
        auto* stmt = static_cast<SearchQueryDslStmt*>(result.statement());
        EXPECT_EQ(stmt->target_index, 22u);
        EXPECT_EQ(stmt->scorer_id, 1u);
    }
    {
        Parser parser("SEARCH DSL '{\"q\":\"bird\"}' ON INDEX 22 SCORER DFR");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AST_SEARCH_QUERY_DSL);
        auto* stmt = static_cast<SearchQueryDslStmt*>(result.statement());
        EXPECT_EQ(stmt->target_index, 22u);
        EXPECT_EQ(stmt->scorer_id, 3u);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsInvalidSearchDslScorer) {
    Parser parser("SEARCH QUERY DSL TARGET_INDEX 22 PAYLOAD '{\"q\":\"bird\"}' SCORER BAD");
    auto result = parser.parseStatement();
    EXPECT_FALSE(result.success());
    EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesVectorAnnStatementAndClauseForms) {
    {
        Parser parser("VECTOR ANN QUERY INDEX 33 METRIC COSINE TOPK 15 EF_SEARCH 64");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AST_VECTOR_ANN_QUERY);
        auto* stmt = static_cast<VectorAnnQueryStmt*>(result.statement());
        EXPECT_EQ(stmt->vector_expr, 33u);
        EXPECT_EQ(stmt->metric, 2u);
        EXPECT_EQ(stmt->k, 15u);
        EXPECT_EQ(stmt->ef_search, 64u);
    }
    {
        Parser parser("ANN INDEX 33 WITH METRIC DOT TOPK 8 EF 40");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AST_VECTOR_ANN_QUERY);
        auto* stmt = static_cast<VectorAnnQueryStmt*>(result.statement());
        EXPECT_EQ(stmt->metric, 3u);
        EXPECT_EQ(stmt->k, 8u);
        EXPECT_EQ(stmt->ef_search, 40u);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsInvalidVectorAnnMetric) {
    Parser parser("VECTOR ANN QUERY INDEX 33 METRIC BAD TOPK 10 EF_SEARCH 20");
    auto result = parser.parseStatement();
    EXPECT_FALSE(result.success());
    EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesHybridBridgeStatementAndClauseForms) {
    {
        Parser parser("HYBRID BRIDGE EXCHANGE SOURCE_TRACK 1 TARGET_TRACK 2 MODE HASH_SHUFFLE");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AST_HYBRID_BRIDGE);
        auto* stmt = static_cast<HybridBridgeStmt*>(result.statement());
        EXPECT_EQ(stmt->source_track, 1u);
        EXPECT_EQ(stmt->target_track, 2u);
        EXPECT_EQ(stmt->bridge_mode, 1u);
    }
    {
        Parser parser("BRIDGE SOURCE 3 TARGET 4 MODE BROADCAST");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AST_HYBRID_BRIDGE);
        auto* stmt = static_cast<HybridBridgeStmt*>(result.statement());
        EXPECT_EQ(stmt->source_track, 3u);
        EXPECT_EQ(stmt->target_track, 4u);
        EXPECT_EQ(stmt->bridge_mode, 3u);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsInvalidHybridBridgeMode) {
    Parser parser("HYBRID BRIDGE EXCHANGE SOURCE_TRACK 1 TARGET_TRACK 2 MODE BAD");
    auto result = parser.parseStatement();
    EXPECT_FALSE(result.success());
    EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesUdrCompileDispatchStatementAndClauseForms) {
    {
        Parser parser(
            "UDR COMPILE EMBEDDED PAYLOAD "
            "PROFILE native FORMAT SQL_TEXT BYTES 'SELECT 1' SESSION_SIGNATURE sig_a");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AST_UDR_COMPILE_DISPATCH);
        auto* stmt = static_cast<UdrCompileDispatchStmt*>(result.statement());
        EXPECT_FALSE(stmt->validate_only);
    }
    {
        Parser parser("VALIDATE EMBEDDED PAYLOAD 'native','SQL_TEXT','SELECT 1','sig_b'");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AST_UDR_COMPILE_DISPATCH);
        auto* stmt = static_cast<UdrCompileDispatchStmt*>(result.statement());
        EXPECT_TRUE(stmt->validate_only);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsInvalidUdrCompileDispatchSurface) {
    Parser parser("UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES 'SELECT 1'");
    auto result = parser.parseStatement();
    EXPECT_FALSE(result.success());
    EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesEmbeddedSqlTemplateStatementAndClauseForms) {
    {
        Parser parser(
            "UDR VALIDATE SQL TEMPLATE "
            "TEMPLATE_ID tpl_001 SQL_TEXT 'SELECT 42' PROFILE native SESSION_SIGNATURE sig_c");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AST_UDR_EMBEDDED_SQL_COMPILE);
        auto* stmt = static_cast<UdrEmbeddedSqlCompileStmt*>(result.statement());
        EXPECT_TRUE(stmt->validate_only);
    }
    {
        Parser parser("COMPILE SQL TEMPLATE tpl_002 USING 'SELECT 7' PROFILE native SIGNATURE sig_d");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AST_UDR_EMBEDDED_SQL_COMPILE);
        auto* stmt = static_cast<UdrEmbeddedSqlCompileStmt*>(result.statement());
        EXPECT_FALSE(stmt->validate_only);
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsInvalidEmbeddedSqlTemplateSurface) {
    Parser parser("VALIDATE SQL TEMPLATE tpl_003 USING 'SELECT 9' PROFILE native");
    auto result = parser.parseStatement();
    EXPECT_FALSE(result.success());
    EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesMilvusAnnIndexFamiliesInCreateIndexSurface) {
    Parser parser(
        "CREATE INDEX idx_vectors_pq ON vectors USING IVF_PQ (embedding) "
        "WITH (NLIST = 1024, M = 16, NPROBE = 32)");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success());
    ASSERT_EQ(result.statement()->kind(), ASTKind::CreateIndexStmt);
    auto* stmt = static_cast<CreateIndexStmt*>(result.statement());
    EXPECT_EQ(stmt->index_type, IndexType::IVF_PQ);
    EXPECT_EQ(stmt->option_assignments.size(), 3u);
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesOpenSearchJoinFieldStatementAndClauseForms) {
    auto assert_join_surface = [](const ParseResult& result, StringPool& pool) {
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
        auto* stmt = static_cast<AlterSystemStmt*>(result.statement());
        EXPECT_EQ(std::string(pool.get(stmt->name)), "search.join_field.mapping");
        ASSERT_NE(stmt->value, nullptr);
        ASSERT_EQ(stmt->value->kind(), ASTKind::LiteralExpr);
        auto* lit = static_cast<LiteralExpr*>(stmt->value);
        EXPECT_EQ(lit->literal_type, LiteralType::STRING);
        EXPECT_EQ(std::string(pool.get(lit->string_value)),
                  "SEARCH JOIN FIELD MAPPING INDEX 17 FIELD rel_join PARENT parent_doc CHILD child_doc ROUTING REQUIRED");
    };

    {
        Parser parser(
            "SEARCH JOIN FIELD MAPPING INDEX 17 FIELD rel_join "
            "PARENT parent_doc CHILD child_doc ROUTING REQUIRED");
        auto result = parser.parseStatement();
        assert_join_surface(result, parser.stringPool());
    }
    {
        Parser parser(
            "JOIN FIELD rel_join ON INDEX 17 "
            "PARENT parent_doc CHILD child_doc ROUTING REQUIRED");
        auto result = parser.parseStatement();
        assert_join_surface(result, parser.stringPool());
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsInvalidOpenSearchJoinFieldRouting) {
    Parser parser(
        "SEARCH JOIN FIELD MAPPING INDEX 17 FIELD rel_join "
        "PARENT parent_doc CHILD child_doc ROUTING BAD");
    auto result = parser.parseStatement();
    EXPECT_FALSE(result.success());
    EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesOpenSearchPercolatorStatementAndClauseForms) {
    auto assert_percolator_surface = [](const ParseResult& result, StringPool& pool) {
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
        auto* stmt = static_cast<AlterSystemStmt*>(result.statement());
        EXPECT_EQ(std::string(pool.get(stmt->name)), "search.percolator.field");
        ASSERT_NE(stmt->value, nullptr);
        ASSERT_EQ(stmt->value->kind(), ASTKind::LiteralExpr);
        auto* lit = static_cast<LiteralExpr*>(stmt->value);
        EXPECT_EQ(lit->literal_type, LiteralType::STRING);
        EXPECT_EQ(std::string(pool.get(lit->string_value)),
                  "SEARCH PERCOLATOR FIELD INDEX 41 FIELD query_match QUERY_PARSER SIMPLE");
    };

    {
        Parser parser("SEARCH PERCOLATOR FIELD INDEX 41 FIELD query_match QUERY_PARSER SIMPLE");
        auto result = parser.parseStatement();
        assert_percolator_surface(result, parser.stringPool());
    }
    {
        Parser parser("PERCOLATOR FIELD query_match ON INDEX 41 PARSER SIMPLE");
        auto result = parser.parseStatement();
        assert_percolator_surface(result, parser.stringPool());
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesNeo4jQuantifiedPathStatementAndClauseForms) {
    auto assert_path_surface = [](const ParseResult& result, StringPool& pool) {
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
        auto* stmt = static_cast<AlterSystemStmt*>(result.statement());
        EXPECT_EQ(std::string(pool.get(stmt->name)), "graph.path.quantified");
        ASSERT_NE(stmt->value, nullptr);
        ASSERT_EQ(stmt->value->kind(), ASTKind::LiteralExpr);
        auto* lit = static_cast<LiteralExpr*>(stmt->value);
        EXPECT_EQ(std::string(pool.get(lit->string_value)),
                  "GRAPH PATH MATCH PATTERN rel_path MIN_HOPS 1 MAX_HOPS 4 CYCLE_POLICY NO_REPEAT");
    };

    {
        Parser parser(
            "GRAPH PATH MATCH PATTERN rel_path MIN_HOPS 1 MAX_HOPS 4 CYCLE_POLICY NO_REPEAT");
        auto result = parser.parseStatement();
        assert_path_surface(result, parser.stringPool());
    }
    {
        Parser parser("MATCH GRAPH PATH rel_path HOPS 1..4 NO CYCLES");
        auto result = parser.parseStatement();
        assert_path_surface(result, parser.stringPool());
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsInvalidNeo4jQuantifiedPathRange) {
    Parser parser("MATCH GRAPH PATH rel_path HOPS 5..1 NO CYCLES");
    auto result = parser.parseStatement();
    EXPECT_FALSE(result.success());
    EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesRedisLuaEvalStatementAndClauseForms) {
    auto assert_lua_surface = [](const ParseResult& result, StringPool& pool) {
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
        auto* stmt = static_cast<AlterSystemStmt*>(result.statement());
        EXPECT_EQ(std::string(pool.get(stmt->name)), "redis.lua.eval");
        ASSERT_NE(stmt->value, nullptr);
        ASSERT_EQ(stmt->value->kind(), ASTKind::LiteralExpr);
        auto* lit = static_cast<LiteralExpr*>(stmt->value);
        EXPECT_EQ(std::string(pool.get(lit->string_value)),
                  "REDIS LUA EVAL SCRIPT return_1 KEYS (k1,k2) ARGS (a1,a2)");
    };

    {
        Parser parser("REDIS LUA EVAL SCRIPT return_1 KEYS (k1, k2) ARGS (a1, a2)");
        auto result = parser.parseStatement();
        assert_lua_surface(result, parser.stringPool());
    }
    {
        Parser parser("EVAL LUA return_1 KEYS (k1, k2) ARGS (a1, a2)");
        auto result = parser.parseStatement();
        assert_lua_surface(result, parser.stringPool());
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, ParsesRedisStreamGroupStatementAndClauseForms) {
    auto assert_stream_create = [](const ParseResult& result, StringPool& pool) {
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
        auto* stmt = static_cast<AlterSystemStmt*>(result.statement());
        EXPECT_EQ(std::string(pool.get(stmt->name)), "redis.stream.group.create");
        auto* lit = static_cast<LiteralExpr*>(stmt->value);
        EXPECT_EQ(std::string(pool.get(lit->string_value)),
                  "REDIS STREAM GROUP CREATE STREAM orders GROUP grp_a START_ID 0-0");
    };
    auto assert_stream_read = [](const ParseResult& result, StringPool& pool) {
        ASSERT_TRUE(result.success());
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterSystemStmt);
        auto* stmt = static_cast<AlterSystemStmt*>(result.statement());
        EXPECT_EQ(std::string(pool.get(stmt->name)), "redis.stream.group.read");
        auto* lit = static_cast<LiteralExpr*>(stmt->value);
        EXPECT_EQ(std::string(pool.get(lit->string_value)),
                  "REDIS STREAM GROUP READ STREAM orders GROUP grp_a CONSUMER c1 COUNT 32 BLOCK_MS 5000");
    };

    {
        Parser parser("REDIS STREAM GROUP CREATE STREAM orders GROUP grp_a START_ID 0-0");
        auto result = parser.parseStatement();
        assert_stream_create(result, parser.stringPool());
    }
    {
        Parser parser("XGROUP CREATE orders grp_a 0-0");
        auto result = parser.parseStatement();
        assert_stream_create(result, parser.stringPool());
    }
    {
        Parser parser("REDIS STREAM GROUP READ STREAM orders GROUP grp_a CONSUMER c1 COUNT 32 BLOCK_MS 5000");
        auto result = parser.parseStatement();
        assert_stream_read(result, parser.stringPool());
    }
    {
        Parser parser("XREADGROUP STREAM orders GROUP grp_a CONSUMER c1 COUNT 32 BLOCK 5000");
        auto result = parser.parseStatement();
        assert_stream_read(result, parser.stringPool());
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsInvalidRedisStreamGroupSurfaces) {
    {
        Parser parser("REDIS STREAM GROUP READ STREAM orders GROUP grp_a CONSUMER c1 BLOCK_MS 5000");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }
    {
        Parser parser("XCLAIM STREAM orders GROUP grp_a CONSUMER c1 MINIDLE -1 IDS (0-1)");
        auto result = parser.parseStatement();
        EXPECT_FALSE(result.success());
        EXPECT_TRUE(hasErrorCode(result, "PRS_0504"));
    }
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsDisabledFeatureStatementsDeterministically) {
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures("DOC PATH FILTER PATH_ID 1 OP EQ VALUE_REF 2", {"F_DOC_PATH_FILTER"}),
        "PRS_0503"));
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures("TS BUCKET AGG TIME_EXPR 91 BUCKET_NS 10 AGG_REFS (1)", {"F_TS_BUCKET_AGG"}),
        "PRS_0503"));
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures("SEARCH QUERY DSL TARGET_INDEX 22 PAYLOAD '{\"q\":\"bird\"}'",
                                  {"F_SEARCH_QUERY_DSL"}),
        "PRS_0503"));
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures("VECTOR ANN QUERY INDEX 33 METRIC COSINE TOPK 10 EF_SEARCH 40",
                                  {"F_VECTOR_ANN"}),
        "PRS_0503"));
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures("HYBRID BRIDGE EXCHANGE SOURCE_TRACK 1 TARGET_TRACK 2 MODE HASH_SHUFFLE",
                                  {"F_HYBRID_BRIDGE_HINT"}),
        "PRS_0503"));
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures(
            "UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES 'SELECT 1' "
            "SESSION_SIGNATURE sig_a",
            {"F_LANGUAGE_UDR_COMPILE_BRIDGE"}),
        "PRS_0503"));
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures(
            "UDR VALIDATE SQL TEMPLATE TEMPLATE_ID tpl_001 SQL_TEXT 'SELECT 42' PROFILE native "
            "SESSION_SIGNATURE sig_c",
            {"F_EMBEDDED_SQL_TEMPLATE_COMPILE"}),
        "PRS_0503"));
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures("CREATE DATABASE EMULATED postgresql localhost:db_main",
                                  {"F_ENGINE_PROFILE_CREATE"}),
        "PRS_0503"));
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures(
            "CREATE SCHEDULE sch_daily RRULE 'FREQ=DAILY;INTERVAL=1' "
            "DTSTART '2026-02-17T00:00:00' TZ 'UTC'",
            {"F_RRULE_SCHEDULE_SURFACE"}),
        "PRS_0503"));
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures(
            "CREATE CONNECTION RULE ch_src ORDER 5 "
            "MATCH (TRANSPORT=TLS, SOURCE='10.0.0.0/8', PRINCIPAL='ch_%') "
            "REQUIRE (TLS=TLS, PROVIDER=INTERNAL) ACTION ALLOW EXPECT VERSION 1",
            {"F_SECURITY_CONNECTION_RULE_DDL"}),
        "PRS_0503"));
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures("CREATE TOKEN ifx_reader WITH SCOPE (ALLOW BUCKET 'cpu_metrics' ACTION READ)",
                                  {"F_SECURITY_TOKEN_DDL"}),
        "PRS_0503"));
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures("CREATE QUOTA PROFILE q1 (MAX_REQUESTS_PER_SEC=1000, WINDOW_MS=1000)",
                                  {"F_SECURITY_QUOTA_PROFILE_DDL"}),
        "PRS_0503"));
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures("CREATE USER app_user WITH PASSWORD 'pw' NOSUPERUSER",
                                  {"F_SECURITY_USER_ACCOUNT_DDL"}),
        "PRS_0503"));
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures("CREATE POLICY p1 ON t1 USING (1 = 1)", {"F_SECURITY_MODEL_POLICY_DDL"}),
        "PRS_0503"));
}

TEST(ParserV3NativeExtensionSurfaceTest, RejectsDisabledFeatureFunctionsDeterministically) {
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures("SELECT DOC_PATH_EXISTS('{}', '$.a')", {"F_DOC_PATH_FILTER"}),
        "PRS_0503"));
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures("SELECT TIME_BUCKET(1000, 60)", {"F_TS_BUCKET_AGG"}),
        "PRS_0503"));
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures("SELECT SEARCH_BM25('idx', 'field', 'q', 1, 10, 0.2)",
                                  {"F_SEARCH_QUERY_DSL"}),
        "PRS_0503"));
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures("SELECT VECTOR_L2_DISTANCE('[1,2]', '[3,4]')", {"F_VECTOR_ANN"}),
        "PRS_0503"));
    EXPECT_TRUE(hasErrorCode(
        parseWithDisabledFeatures("SELECT COMPILE_EMBEDDED_PAYLOAD('p','fmt','bytes','sig')",
                                  {"F_LANGUAGE_UDR_COMPILE_BRIDGE"}),
        "PRS_0503"));
}

}  // namespace
