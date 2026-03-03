/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

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

void expectRejectedWithCanonicalCode(const std::string& sql) {
    Parser parser(sql);
    auto result = parser.parseStatement();
    EXPECT_FALSE(result.success()) << sql;
    EXPECT_TRUE(hasErrorCode(result, "PRS_0505")) << sql;
}

TEST(ParserV3CanonicalRejectionsTest, RejectsRemovedLegacySelectAndJoinForms) {
    expectRejectedWithCanonicalCode("SELECT TOP (10) id FROM users");
    expectRejectedWithCanonicalCode("SELECT id FROM users MINUS SELECT id FROM archived_users");
    expectRejectedWithCanonicalCode("SELECT 1 FROM DUAL");
    expectRejectedWithCanonicalCode("SELECT * FROM t CROSS APPLY lateral_fn(t.id)");
    expectRejectedWithCanonicalCode("SELECT * FROM t OUTER APPLY lateral_fn(t.id)");
    expectRejectedWithCanonicalCode("SELECT * FROM t PIVOT (SUM(v) FOR k IN (1))");
    expectRejectedWithCanonicalCode(
        "SELECT id FROM tree START WITH parent_id IS NULL CONNECT BY PRIOR id = parent_id");
    expectRejectedWithCanonicalCode("SELECT {fn UCASE('alpha')}");
}

TEST(ParserV3CanonicalRejectionsTest, RejectsRemovedLegacyMutationForms) {
    expectRejectedWithCanonicalCode("INSERT IGNORE INTO t (id) VALUES (1)");
    expectRejectedWithCanonicalCode("REPLACE INTO t (id) VALUES (1)");
    expectRejectedWithCanonicalCode("INSERT INTO t (id) VALUES (1) ON DUPLICATE KEY UPDATE id = 2");
    expectRejectedWithCanonicalCode("UPDATE t SET id = 1 ORDER BY id LIMIT 1");
    expectRejectedWithCanonicalCode("DELETE FROM t ORDER BY id LIMIT 1");
    expectRejectedWithCanonicalCode("UPDATE t SET id = 1 OUTPUT id");
    expectRejectedWithCanonicalCode("DELETE FROM t OUTPUT id");
    expectRejectedWithCanonicalCode("INSERT INTO t (id) VALUES (1) OUTPUT id");
}

}  // namespace
