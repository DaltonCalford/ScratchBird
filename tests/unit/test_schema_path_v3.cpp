/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * Unit Tests for ScratchBird Schema Path v3.0
 *
 * Tests the hierarchical schema path parsing for the unique namespace
 * navigation system in ScratchBird.
 */

#include <gtest/gtest.h>
#include "scratchbird/parser/schema_path_v3.h"
#include "scratchbird/parser/parser_state_v3.h"

using namespace scratchbird::parser::v3;

class SchemaPathV3Test : public ::testing::Test {
protected:
    std::unique_ptr<Lexer> makeLexer(const std::string& input) {
        input_ = input;
        return std::make_unique<Lexer>(input_);
    }

    std::unique_ptr<ParserState> makeState(const std::string& input) {
        lexer_ = makeLexer(input);
        return std::make_unique<ParserState>(*lexer_);
    }

    // Helper to get string from StringId
    std::string getString(ParserState& state, StringPool::StringId id) {
        return std::string(state.stringPool().get(id));
    }

private:
    std::string input_;
    std::unique_ptr<Lexer> lexer_;
};

// =============================================================================
// DOUBLE_DOT Token Tests
// =============================================================================

TEST_F(SchemaPathV3Test, LexerRecognizesDoubleDot) {
    auto lexer = makeLexer("..parent");

    Token t1 = lexer->nextToken();
    EXPECT_EQ(t1.type, TokenType::DOUBLE_DOT);

    Token t2 = lexer->nextToken();
    EXPECT_EQ(t2.type, TokenType::IDENTIFIER);
}

TEST_F(SchemaPathV3Test, LexerDistinguishesDotAndDoubleDot) {
    auto lexer = makeLexer(". .. . ..");

    EXPECT_EQ(lexer->nextToken().type, TokenType::DOT);
    EXPECT_EQ(lexer->nextToken().type, TokenType::DOUBLE_DOT);
    EXPECT_EQ(lexer->nextToken().type, TokenType::DOT);
    EXPECT_EQ(lexer->nextToken().type, TokenType::DOUBLE_DOT);
}

TEST_F(SchemaPathV3Test, LexerHandlesThreeDots) {
    // "..." should be DOUBLE_DOT followed by DOT
    auto lexer = makeLexer("...");

    EXPECT_EQ(lexer->nextToken().type, TokenType::DOUBLE_DOT);
    EXPECT_EQ(lexer->nextToken().type, TokenType::DOT);
}

// =============================================================================
// Unqualified Path Tests
// =============================================================================

TEST_F(SchemaPathV3Test, ParseUnqualifiedSingleName) {
    auto state = makeState("orders");

    SchemaPath path = parseSchemaPath(*state);

    EXPECT_EQ(path.type, PathType::UNQUALIFIED);
    EXPECT_EQ(path.components.size(), 1);
    EXPECT_EQ(getString(*state, path.components[0]), "orders");
}

TEST_F(SchemaPathV3Test, ParseUnqualifiedWithTrailingToken) {
    auto state = makeState("orders WHERE");

    SchemaPath path = parseSchemaPath(*state);

    EXPECT_EQ(path.type, PathType::UNQUALIFIED);
    EXPECT_EQ(path.components.size(), 1);
    // Should stop at WHERE keyword
    EXPECT_EQ(state->current().type, TokenType::KW_WHERE);
}

// =============================================================================
// Current Schema Path Tests (.name)
// =============================================================================

TEST_F(SchemaPathV3Test, ParseCurrentSchemaSingleName) {
    auto state = makeState(".orders");

    SchemaPath path = parseSchemaPath(*state);

    EXPECT_EQ(path.type, PathType::CURRENT);
    EXPECT_EQ(path.components.size(), 1);
    EXPECT_EQ(getString(*state, path.components[0]), "orders");
}

TEST_F(SchemaPathV3Test, ParseCurrentSchemaMultipleParts) {
    auto state = makeState(".sub.schema.orders");

    SchemaPath path = parseSchemaPath(*state);

    EXPECT_EQ(path.type, PathType::CURRENT);
    EXPECT_EQ(path.components.size(), 3);
    EXPECT_EQ(getString(*state, path.components[0]), "sub");
    EXPECT_EQ(getString(*state, path.components[1]), "schema");
    EXPECT_EQ(getString(*state, path.components[2]), "orders");
}

TEST_F(SchemaPathV3Test, ParseCurrentSchemaWithTrailingToken) {
    auto state = makeState(".orders AS o");

    SchemaPath path = parseSchemaPath(*state);

    EXPECT_EQ(path.type, PathType::CURRENT);
    EXPECT_EQ(path.components.size(), 1);
    // Should stop before AS (which is a Gatekeeper keyword KW_AS)
    EXPECT_EQ(state->current().type, TokenType::KW_AS);
}

// =============================================================================
// Parent Schema Path Tests (..name)
// =============================================================================

TEST_F(SchemaPathV3Test, ParseParentSchemaSingleName) {
    auto state = makeState("..orders");

    SchemaPath path = parseSchemaPath(*state);

    EXPECT_EQ(path.type, PathType::PARENT);
    EXPECT_EQ(path.components.size(), 1);
    EXPECT_EQ(getString(*state, path.components[0]), "orders");
}

TEST_F(SchemaPathV3Test, ParseParentSchemaMultipleParts) {
    auto state = makeState("..sibling.orders");

    SchemaPath path = parseSchemaPath(*state);

    EXPECT_EQ(path.type, PathType::PARENT);
    EXPECT_EQ(path.components.size(), 2);
    EXPECT_EQ(getString(*state, path.components[0]), "sibling");
    EXPECT_EQ(getString(*state, path.components[1]), "orders");
}

// =============================================================================
// Absolute Path Tests (schema.name)
// =============================================================================

TEST_F(SchemaPathV3Test, ParseAbsoluteTwoParts) {
    auto state = makeState("public.orders");

    SchemaPath path = parseSchemaPath(*state);

    EXPECT_EQ(path.type, PathType::ABSOLUTE);
    EXPECT_EQ(path.components.size(), 2);
    EXPECT_EQ(getString(*state, path.components[0]), "public");
    EXPECT_EQ(getString(*state, path.components[1]), "orders");
}

TEST_F(SchemaPathV3Test, ParseAbsoluteThreeParts) {
    auto state = makeState("sys.catalog.tables");

    SchemaPath path = parseSchemaPath(*state);

    EXPECT_EQ(path.type, PathType::ABSOLUTE);
    EXPECT_EQ(path.components.size(), 3);
    EXPECT_EQ(getString(*state, path.components[0]), "sys");
    EXPECT_EQ(getString(*state, path.components[1]), "catalog");
    EXPECT_EQ(getString(*state, path.components[2]), "tables");
}

TEST_F(SchemaPathV3Test, ParseAbsoluteManyParts) {
    auto state = makeState("a.b.c.d.e.f");

    SchemaPath path = parseSchemaPath(*state);

    EXPECT_EQ(path.type, PathType::ABSOLUTE);
    EXPECT_EQ(path.components.size(), 6);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(SchemaPathV3Test, ParseEmptyInput) {
    auto state = makeState("");

    SchemaPath path = parseSchemaPath(*state);

    EXPECT_TRUE(path.isEmpty());
}

TEST_F(SchemaPathV3Test, ParseDotWithoutFollowingIdentifier) {
    auto state = makeState(". WHERE");

    SchemaPath path = parseSchemaPath(*state);

    // Should return empty path since WHERE is a keyword, not identifier
    EXPECT_EQ(path.type, PathType::CURRENT);
    EXPECT_TRUE(path.isEmpty());
}

TEST_F(SchemaPathV3Test, ParseDoubleDotWithoutFollowingIdentifier) {
    auto state = makeState(".. SELECT");

    SchemaPath path = parseSchemaPath(*state);

    EXPECT_EQ(path.type, PathType::PARENT);
    EXPECT_TRUE(path.isEmpty());
}

// =============================================================================
// SchemaPath Methods
// =============================================================================

TEST_F(SchemaPathV3Test, IsQualified) {
    auto state = makeState("name");
    SchemaPath unqualified = parseSchemaPath(*state);
    EXPECT_FALSE(unqualified.isQualified());

    state = makeState(".name");
    SchemaPath current = parseSchemaPath(*state);
    EXPECT_TRUE(current.isQualified());

    state = makeState("..name");
    SchemaPath parent = parseSchemaPath(*state);
    EXPECT_TRUE(parent.isQualified());

    state = makeState("schema.name");
    SchemaPath absolute = parseSchemaPath(*state);
    EXPECT_TRUE(absolute.isQualified());
}

TEST_F(SchemaPathV3Test, ObjectName) {
    auto state = makeState("sys.catalog.tables");
    SchemaPath path = parseSchemaPath(*state);

    EXPECT_EQ(getString(*state, path.objectName()), "tables");
}

TEST_F(SchemaPathV3Test, SchemaComponents) {
    auto state = makeState("sys.catalog.tables");
    SchemaPath path = parseSchemaPath(*state);

    auto schema = path.schemaComponents();
    EXPECT_EQ(schema.size(), 2);
    EXPECT_EQ(getString(*state, schema[0]), "sys");
    EXPECT_EQ(getString(*state, schema[1]), "catalog");
}

// =============================================================================
// SchemaPathToString Tests
// =============================================================================

TEST_F(SchemaPathV3Test, SchemaPathToString_Unqualified) {
    auto state = makeState("orders");
    SchemaPath path = parseSchemaPath(*state);

    std::string str = schemaPathToString(path, state->stringPool());
    EXPECT_EQ(str, "orders");
}

TEST_F(SchemaPathV3Test, SchemaPathToString_Current) {
    auto state = makeState(".orders");
    SchemaPath path = parseSchemaPath(*state);

    std::string str = schemaPathToString(path, state->stringPool());
    EXPECT_EQ(str, ".orders");
}

TEST_F(SchemaPathV3Test, SchemaPathToString_CurrentMultiple) {
    auto state = makeState(".sub.orders");
    SchemaPath path = parseSchemaPath(*state);

    std::string str = schemaPathToString(path, state->stringPool());
    EXPECT_EQ(str, ".sub.orders");
}

TEST_F(SchemaPathV3Test, SchemaPathToString_Parent) {
    auto state = makeState("..orders");
    SchemaPath path = parseSchemaPath(*state);

    std::string str = schemaPathToString(path, state->stringPool());
    EXPECT_EQ(str, "..orders");
}

TEST_F(SchemaPathV3Test, SchemaPathToString_Absolute) {
    auto state = makeState("sys.catalog.tables");
    SchemaPath path = parseSchemaPath(*state);

    std::string str = schemaPathToString(path, state->stringPool());
    EXPECT_EQ(str, "sys.catalog.tables");
}

// =============================================================================
// CanStartSchemaPath Tests
// =============================================================================

TEST_F(SchemaPathV3Test, CanStartSchemaPath_Identifier) {
    auto state = makeState("orders");
    EXPECT_TRUE(canStartSchemaPath(*state));
}

TEST_F(SchemaPathV3Test, CanStartSchemaPath_Dot) {
    auto state = makeState(".orders");
    EXPECT_TRUE(canStartSchemaPath(*state));
}

TEST_F(SchemaPathV3Test, CanStartSchemaPath_DoubleDot) {
    auto state = makeState("..orders");
    EXPECT_TRUE(canStartSchemaPath(*state));
}

TEST_F(SchemaPathV3Test, CanStartSchemaPath_Keyword) {
    auto state = makeState("SELECT");
    EXPECT_FALSE(canStartSchemaPath(*state));
}

TEST_F(SchemaPathV3Test, CanStartSchemaPath_Number) {
    auto state = makeState("123");
    EXPECT_FALSE(canStartSchemaPath(*state));
}

// =============================================================================
// Table Reference Tests
// =============================================================================

TEST_F(SchemaPathV3Test, ParseTableRef_Simple) {
    auto state = makeState("orders");

    TableRef ref = parseTableRef(*state);

    EXPECT_EQ(ref.path.type, PathType::UNQUALIFIED);
    EXPECT_EQ(ref.path.components.size(), 1);
    EXPECT_FALSE(ref.has_alias);
}

TEST_F(SchemaPathV3Test, ParseTableRef_WithExplicitAlias) {
    auto state = makeState("orders AS o");

    TableRef ref = parseTableRef(*state);

    EXPECT_EQ(ref.path.type, PathType::UNQUALIFIED);
    EXPECT_EQ(getString(*state, ref.path.components[0]), "orders");
    EXPECT_TRUE(ref.has_alias);
    EXPECT_EQ(getString(*state, ref.alias), "o");
}

TEST_F(SchemaPathV3Test, ParseTableRef_WithImplicitAlias) {
    auto state = makeState("orders o");

    TableRef ref = parseTableRef(*state);

    EXPECT_EQ(getString(*state, ref.path.components[0]), "orders");
    EXPECT_TRUE(ref.has_alias);
    EXPECT_EQ(getString(*state, ref.alias), "o");
}

TEST_F(SchemaPathV3Test, ParseTableRef_QualifiedWithAlias) {
    auto state = makeState("sys.catalog.tables AS t");

    TableRef ref = parseTableRef(*state);

    EXPECT_EQ(ref.path.type, PathType::ABSOLUTE);
    EXPECT_EQ(ref.path.components.size(), 3);
    EXPECT_TRUE(ref.has_alias);
    EXPECT_EQ(getString(*state, ref.alias), "t");
}

TEST_F(SchemaPathV3Test, ParseTableRef_CurrentSchema) {
    auto state = makeState(".orders o");

    TableRef ref = parseTableRef(*state);

    EXPECT_EQ(ref.path.type, PathType::CURRENT);
    EXPECT_TRUE(ref.has_alias);
}

TEST_F(SchemaPathV3Test, ParseTableRef_ParentSchema) {
    auto state = makeState("..customers c");

    TableRef ref = parseTableRef(*state);

    EXPECT_EQ(ref.path.type, PathType::PARENT);
    EXPECT_TRUE(ref.has_alias);
}

// =============================================================================
// Column Reference Tests
// =============================================================================

TEST_F(SchemaPathV3Test, ParseColumnRef_Simple) {
    auto state = makeState("id");

    ColumnRef ref = parseColumnRef(*state);

    EXPECT_EQ(getString(*state, ref.column_name), "id");
    EXPECT_FALSE(ref.has_table_qualifier);
}

TEST_F(SchemaPathV3Test, ParseColumnRef_TableQualified) {
    auto state = makeState("orders.id");

    ColumnRef ref = parseColumnRef(*state);

    EXPECT_EQ(getString(*state, ref.column_name), "id");
    EXPECT_TRUE(ref.has_table_qualifier);
    EXPECT_EQ(ref.table_path.type, PathType::UNQUALIFIED);
    EXPECT_EQ(getString(*state, ref.table_path.components[0]), "orders");
}

TEST_F(SchemaPathV3Test, ParseColumnRef_SchemaTableQualified) {
    auto state = makeState("public.orders.id");

    ColumnRef ref = parseColumnRef(*state);

    EXPECT_EQ(getString(*state, ref.column_name), "id");
    EXPECT_TRUE(ref.has_table_qualifier);
    EXPECT_EQ(ref.table_path.type, PathType::ABSOLUTE);
    EXPECT_EQ(ref.table_path.components.size(), 2);
    EXPECT_EQ(getString(*state, ref.table_path.components[0]), "public");
    EXPECT_EQ(getString(*state, ref.table_path.components[1]), "orders");
}

TEST_F(SchemaPathV3Test, ParseColumnRef_CurrentSchemaQualified) {
    auto state = makeState(".orders.id");

    ColumnRef ref = parseColumnRef(*state);

    EXPECT_EQ(getString(*state, ref.column_name), "id");
    EXPECT_TRUE(ref.has_table_qualifier);
    EXPECT_EQ(ref.table_path.type, PathType::CURRENT);
}

TEST_F(SchemaPathV3Test, ParseColumnRef_ParentSchemaQualified) {
    auto state = makeState("..orders.id");

    ColumnRef ref = parseColumnRef(*state);

    EXPECT_EQ(getString(*state, ref.column_name), "id");
    EXPECT_TRUE(ref.has_table_qualifier);
    EXPECT_EQ(ref.table_path.type, PathType::PARENT);
}

TEST_F(SchemaPathV3Test, ParseColumnRef_DeepPath) {
    auto state = makeState("sys.catalog.columns.name");

    ColumnRef ref = parseColumnRef(*state);

    EXPECT_EQ(getString(*state, ref.column_name), "name");
    EXPECT_TRUE(ref.has_table_qualifier);
    EXPECT_EQ(ref.table_path.components.size(), 3);
}

// =============================================================================
// Integration Tests - Real SQL Patterns
// =============================================================================

TEST_F(SchemaPathV3Test, SelectFromCurrentSchema) {
    // SELECT * FROM .orders
    auto state = makeState("SELECT * FROM .orders WHERE id = 1");

    // Skip to FROM clause
    EXPECT_TRUE(state->match(TokenType::KW_SELECT));
    EXPECT_TRUE(state->match(TokenType::STAR));
    EXPECT_TRUE(state->match(TokenType::KW_FROM));

    // Parse table reference
    TableRef ref = parseTableRef(*state);

    EXPECT_EQ(ref.path.type, PathType::CURRENT);
    EXPECT_EQ(getString(*state, ref.path.components[0]), "orders");

    // Should be at WHERE now
    EXPECT_EQ(state->current().type, TokenType::KW_WHERE);
}

TEST_F(SchemaPathV3Test, SelectFromParentSchema) {
    // SELECT * FROM ..customers
    auto state = makeState("SELECT * FROM ..customers");

    state->match(TokenType::KW_SELECT);
    state->match(TokenType::STAR);
    state->match(TokenType::KW_FROM);

    TableRef ref = parseTableRef(*state);

    EXPECT_EQ(ref.path.type, PathType::PARENT);
    EXPECT_EQ(getString(*state, ref.path.components[0]), "customers");
}

TEST_F(SchemaPathV3Test, SelectFromSystemCatalog) {
    // SELECT * FROM sys.catalog.tables
    auto state = makeState("SELECT name FROM sys.catalog.tables AS t");

    state->match(TokenType::KW_SELECT);
    state->advance();  // name
    state->match(TokenType::KW_FROM);

    TableRef ref = parseTableRef(*state);

    EXPECT_EQ(ref.path.type, PathType::ABSOLUTE);
    EXPECT_EQ(ref.path.components.size(), 3);
    EXPECT_TRUE(ref.has_alias);
    EXPECT_EQ(getString(*state, ref.alias), "t");
}

TEST_F(SchemaPathV3Test, JoinWithMixedPaths) {
    // SELECT * FROM .orders o JOIN ..customers c ON o.customer_id = c.id
    auto state = makeState(".orders o");

    TableRef orders = parseTableRef(*state);
    EXPECT_EQ(orders.path.type, PathType::CURRENT);
    EXPECT_EQ(getString(*state, orders.alias), "o");

    state = makeState("..customers c");
    TableRef customers = parseTableRef(*state);
    EXPECT_EQ(customers.path.type, PathType::PARENT);
    EXPECT_EQ(getString(*state, customers.alias), "c");
}

TEST_F(SchemaPathV3Test, ColumnRefsInWhereClause) {
    // WHERE .orders.total > 100 AND ..customers.active = TRUE
    auto state = makeState(".orders.total");
    ColumnRef col1 = parseColumnRef(*state);
    EXPECT_EQ(col1.table_path.type, PathType::CURRENT);
    EXPECT_EQ(getString(*state, col1.column_name), "total");

    state = makeState("..customers.active");
    ColumnRef col2 = parseColumnRef(*state);
    EXPECT_EQ(col2.table_path.type, PathType::PARENT);
    EXPECT_EQ(getString(*state, col2.column_name), "active");
}

// =============================================================================
// PathType Utility Tests
// =============================================================================

TEST_F(SchemaPathV3Test, PathTypeToString) {
    EXPECT_STREQ(pathTypeToString(PathType::UNQUALIFIED), "UNQUALIFIED");
    EXPECT_STREQ(pathTypeToString(PathType::CURRENT), "CURRENT");
    EXPECT_STREQ(pathTypeToString(PathType::PARENT), "PARENT");
    EXPECT_STREQ(pathTypeToString(PathType::ABSOLUTE), "ABSOLUTE");
}
