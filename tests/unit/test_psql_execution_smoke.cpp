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

#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/parser/parser_v2.h"
#include "scratchbird/sblr/bytecode_generator_v2.h"
#include "scratchbird/sblr/semantic_analyzer_v2.h"
#include "scratchbird/sblr/executor.h"
#include "test_helpers.h"

using namespace scratchbird;
using namespace scratchbird::core;
using namespace scratchbird::sblr;
using scratchbird::testing::TestDatabaseFile;

TEST(PSQLExecutionSmoke, ExecuteBlockSuspendAndExecuteStatement)
{
    TestDatabaseFile db_file("test_psql_smoke");
    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_file.path(), 16384, &ctx), Status::OK) << ctx.message;

    Database db;
    ASSERT_EQ(db.open(db_file.path(), &ctx), Status::OK) << ctx.message;

    const char* sql =
        "EXECUTE BLOCK RETURNS (x INT) AS "
        "DECLARE VARIABLE y INT; "
        "BEGIN "
        "EXECUTE STATEMENT 'SELECT 7' INTO y; "
        "x = y + 1; "
        "SUSPEND; "
        "END;";

    parser::v2::Parser parser(sql);
    auto parse_result = parser.parseStatement();

    ASSERT_TRUE(parse_result.success()) << (parse_result.errors().empty()
        ? std::string()
        : parse_result.errors().front().message);

    auto& pool = parser.stringPool();
    parser::v2::SemanticAnalyzerV2 analyzer(*db.catalog_manager(), pool);
    auto sem_result = analyzer.analyze(parse_result.statement());

    ASSERT_TRUE(sem_result.success()) << (sem_result.errors().empty()
        ? std::string()
        : sem_result.errors().front().message);

    parser::v2::BytecodeGeneratorV2 generator(pool);
    generator.setSourceSql(sql);
    auto bytecode = generator.generate(sem_result.statement());

    ASSERT_TRUE(bytecode.success()) << (bytecode.errors().empty()
        ? std::string()
        : bytecode.errors().front());

    Executor executor(&db);
    auto result = executor.execute(bytecode.bytecode());
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());

    auto* result_set = result.resultSet();
    ASSERT_NE(result_set, nullptr);
    ASSERT_EQ(result_set->rowCount(), 1u);
    ASSERT_EQ(result_set->columnCount(), 1u);
    EXPECT_EQ(result_set->getValue(0, 0).toInt64(), 8);
}
