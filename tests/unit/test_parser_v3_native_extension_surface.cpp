/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include <gtest/gtest.h>

#include <string>

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

}  // namespace

