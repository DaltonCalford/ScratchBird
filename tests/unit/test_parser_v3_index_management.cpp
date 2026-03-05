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

#include "scratchbird/parser/ast_v3.h"
#include "scratchbird/parser/parser_v3.h"

namespace {

using namespace scratchbird::parser::v3;

TEST(ParserV3IndexManagementTest, ParsesAlterIndexSetAndReset) {
    {
        Parser parser("ALTER INDEX orders.idx_orders SET (bloom_filter = true, bloom_fpr = 0.05)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_NE(result.statement(), nullptr);
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterIndexStmt);

        auto* stmt = static_cast<AlterIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->action, AlterIndexAction::SET_OPTIONS);
        EXPECT_EQ(stmt->option_assignments.size(), 2u);
    }

    {
        Parser parser("ALTER INDEX orders.idx_orders RESET (bloom_filter, bloom_fpr)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_NE(result.statement(), nullptr);
        ASSERT_EQ(result.statement()->kind(), ASTKind::AlterIndexStmt);

        auto* stmt = static_cast<AlterIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->action, AlterIndexAction::RESET_OPTIONS);
        EXPECT_EQ(stmt->reset_options.size(), 2u);
    }
}

TEST(ParserV3IndexManagementTest, ParsesAlterIndexMaintenanceActions) {
    {
        Parser parser("ALTER INDEX orders.idx_orders REBUILD ONLINE WITH (target_fillfactor = 90, throttle_ms = 5)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        auto* stmt = static_cast<AlterIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->action, AlterIndexAction::REBUILD);
        EXPECT_EQ(stmt->mode, IndexMaintenanceMode::ONLINE);
        EXPECT_EQ(stmt->option_assignments.size(), 2u);
    }

    {
        Parser parser("ALTER INDEX orders.idx_orders REBALANCE OFFLINE");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        auto* stmt = static_cast<AlterIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->action, AlterIndexAction::REBALANCE);
        EXPECT_EQ(stmt->mode, IndexMaintenanceMode::OFFLINE);
    }

    {
        Parser parser("ALTER INDEX orders.idx_orders RELOCATE TO FILESPACE fs_hot ONLINE WITH (max_bytes_per_txn = 8192)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        auto* stmt = static_cast<AlterIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->action, AlterIndexAction::RELOCATE);
        EXPECT_TRUE(stmt->has_target_filespace);
        EXPECT_EQ(stmt->mode, IndexMaintenanceMode::ONLINE);
        EXPECT_EQ(stmt->option_assignments.size(), 1u);
    }

    {
        Parser parser("ALTER INDEX orders.idx_orders LIGHT SCAN WITH (sample_pages = 128)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        auto* stmt = static_cast<AlterIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->action, AlterIndexAction::LIGHT_SCAN);
        EXPECT_EQ(stmt->option_assignments.size(), 1u);
    }

    {
        Parser parser("ALTER INDEX orders.idx_orders DIAGNOSTIC SCAN WITH (throttle_ms = 5)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        auto* stmt = static_cast<AlterIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->action, AlterIndexAction::DIAGNOSTIC_SCAN);
        EXPECT_EQ(stmt->option_assignments.size(), 1u);
    }

    {
        Parser parser("VALIDATE INDEX orders.idx_orders WITH (throttle_ms = 5)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        auto* stmt = static_cast<AlterIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->action, AlterIndexAction::DIAGNOSTIC_SCAN);
        EXPECT_EQ(stmt->option_assignments.size(), 1u);
    }
}

TEST(ParserV3IndexManagementTest, ParsesAlterIndexDefaultsSurface) {
    {
        Parser parser("ALTER INDEX DEFAULTS FOR IVF_SQ8_HYBRID SET (nprobe = 16, metric = 'l2')");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        auto* stmt = static_cast<AlterIndexStmt*>(result.statement());
        EXPECT_TRUE(stmt->defaults_scope);
        EXPECT_EQ(stmt->defaults_index_type, IndexType::IVF_SQ8_HYBRID);
        EXPECT_EQ(stmt->action, AlterIndexAction::SET_OPTIONS);
        EXPECT_EQ(stmt->option_assignments.size(), 2u);
    }

    {
        Parser parser("ALTER INDEX DEFAULTS FOR HASH RESET (fillfactor, bloom_filter)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        auto* stmt = static_cast<AlterIndexStmt*>(result.statement());
        EXPECT_TRUE(stmt->defaults_scope);
        EXPECT_EQ(stmt->defaults_index_type, IndexType::HASH);
        EXPECT_EQ(stmt->action, AlterIndexAction::RESET_OPTIONS);
        EXPECT_EQ(stmt->reset_options.size(), 2u);
    }
}

TEST(ParserV3IndexManagementTest, ParsesAnalyzeIndexAndShowIndexReporting) {
    {
        Parser parser("ANALYZE INDEX orders.idx_orders WITH (sample_rate = 0.25)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::AnalyzeStmt);
        auto* stmt = static_cast<AnalyzeStmt*>(result.statement());
        EXPECT_EQ(stmt->target, AnalyzeStmt::AnalyzeTarget::INDEX);
        EXPECT_TRUE(stmt->has_sample);
    }

    {
        Parser parser("SHOW INDEX HEALTH orders.idx_orders");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::ShowStmt);
        auto* stmt = static_cast<ShowStmt*>(result.statement());
        EXPECT_EQ(stmt->show_type, ShowStmt::ShowType::INDEX_HEALTH);
    }

    {
        Parser parser("SHOW INDEX OPTIONS orders.idx_orders");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::ShowStmt);
        auto* stmt = static_cast<ShowStmt*>(result.statement());
        EXPECT_EQ(stmt->show_type, ShowStmt::ShowType::INDEX_OPTIONS);
    }
}

TEST(ParserV3IndexManagementTest, ParsesDropTablespaceSurface) {
    Parser parser("DROP TABLESPACE IF EXISTS ts_hot FORCE");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success()) << "expected parse success";
    ASSERT_EQ(result.statement()->kind(), ASTKind::DropTablespaceStmt);
    auto* stmt = static_cast<DropTablespaceStmt*>(result.statement());
    EXPECT_TRUE(stmt->if_exists);
    EXPECT_TRUE(stmt->force);
}

TEST(ParserV3IndexManagementTest, ParsesAlterTablespaceActions) {
    Parser parser(
        "ALTER TABLESPACE ts_hot AUTOEXTEND ON AUTOEXTEND_SIZE 128 MAXSIZE UNLIMITED RENAME TO ts_cold");
    auto result = parser.parseStatement();
    ASSERT_TRUE(result.success()) << "expected parse success";
    ASSERT_EQ(result.statement()->kind(), ASTKind::AlterTablespaceStmt);
    auto* stmt = static_cast<AlterTablespaceStmt*>(result.statement());
    ASSERT_EQ(stmt->alterations.size(), 4u);

    EXPECT_EQ(stmt->alterations[0].action, TablespaceAlterAction::SET_AUTOEXTEND);
    EXPECT_TRUE(stmt->alterations[0].autoextend_enabled);

    EXPECT_EQ(stmt->alterations[1].action,
              TablespaceAlterAction::SET_AUTOEXTEND_SIZE);
    EXPECT_EQ(stmt->alterations[1].size_mb, 128u);

    EXPECT_EQ(stmt->alterations[2].action, TablespaceAlterAction::SET_MAXSIZE);
    EXPECT_EQ(stmt->alterations[2].size_mb, 0u);

    EXPECT_EQ(stmt->alterations[3].action, TablespaceAlterAction::RENAME_TO);
    EXPECT_NE(stmt->alterations[3].new_name, StringPool::INVALID_ID);
}

TEST(ParserV3IndexManagementTest, ParsesExtendedCreateIndexTypes) {
    {
        Parser parser("CREATE INDEX idx_a ON t USING IVF_SQ8_HYBRID (c)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        ASSERT_EQ(result.statement()->kind(), ASTKind::CreateIndexStmt);
        auto* stmt = static_cast<CreateIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->index_type, IndexType::IVF_SQ8_HYBRID);
        EXPECT_NE(stmt->index_method_name, StringPool::INVALID_ID);
    }

    {
        Parser parser("CREATE INDEX idx_a ON t USING MONGODB_2D (c)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        auto* stmt = static_cast<CreateIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->index_type, IndexType::MONGODB_2D);
    }

    {
        Parser parser("CREATE INDEX idx_a ON t USING REDIS_HASH (c)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        auto* stmt = static_cast<CreateIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->index_type, IndexType::REDIS_HASH);
    }

    {
        Parser parser("CREATE INDEX idx_a ON t USING NEO4J_POINT (c)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        auto* stmt = static_cast<CreateIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->index_type, IndexType::NEO4J_POINT);
    }

    {
        Parser parser("CREATE INDEX idx_a ON t USING CASSANDRA_SAI (c)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        auto* stmt = static_cast<CreateIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->index_type, IndexType::CASSANDRA_SAI);
    }

    {
        Parser parser("CREATE INDEX idx_a ON t USING ANNOY (c)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        auto* stmt = static_cast<CreateIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->index_type, IndexType::ANNOY);
    }

    {
        Parser parser("CREATE INDEX idx_a ON t USING GPU_CAGRA (c)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        auto* stmt = static_cast<CreateIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->index_type, IndexType::GPU_CAGRA);
    }

    {
        Parser parser("CREATE INDEX idx_a ON t USING ART (c)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        auto* stmt = static_cast<CreateIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->index_type, IndexType::ART);
    }

    {
        Parser parser("CREATE INDEX idx_a ON t USING SPARSE_WAND (c)");
        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "expected parse success";
        auto* stmt = static_cast<CreateIndexStmt*>(result.statement());
        EXPECT_EQ(stmt->index_type, IndexType::SPARSE_WAND);
    }
}

TEST(ParserV3IndexManagementTest, RejectsUnknownCreateIndexTypeDeterministically) {
    Parser parser("CREATE INDEX idx_a ON t USING NOT_A_REAL_TYPE (c)");
    auto result = parser.parseStatement();
    EXPECT_FALSE(result.success());
    EXPECT_FALSE(result.errors().empty());
}

}  // namespace
