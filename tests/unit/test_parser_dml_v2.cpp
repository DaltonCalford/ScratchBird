/**
 * Unit Tests for ScratchBird Parser v2.0 - DML Statements
 *
 * Tests parsing of SELECT, INSERT, UPDATE, DELETE statements.
 */

#include <gtest/gtest.h>
#include "scratchbird/parser/parser_v2.h"

using namespace scratchbird::parser::v2;

class ParserDMLV2Test : public ::testing::Test {
protected:
    void SetUp() override {
        parser_.reset();
    }

    // Creates a parser and keeps it alive for the duration of the test
    Parser& createParser(const std::string& sql) {
        input_ = sql;
        parser_ = std::make_unique<Parser>(input_);
        return *parser_;
    }

    ParseResult parse(const std::string& sql) {
        return createParser(sql).parseStatement();
    }

    template<typename T>
    T* parseAs(const std::string& sql) {
        auto result = parse(sql);
        EXPECT_TRUE(result.success()) << "Parse failed for: " << sql;
        if (!result.success()) {
            for (const auto& err : result.errors()) {
                std::cerr << "Error: " << err.message << "\n";
            }
            return nullptr;
        }
        Statement* stmt = result.statement();
        if (!stmt) {
            std::cerr << "Null statement for: " << sql << "\n";
            return nullptr;
        }
        T* typedStmt = dynamic_cast<T*>(stmt);
        if (!typedStmt) {
            std::cerr << "Wrong statement type for: " << sql << ", got kind=" << static_cast<int>(stmt->kind()) << "\n";
        }
        return typedStmt;
    }

private:
    std::string input_;
    std::unique_ptr<Parser> parser_;
};

// =============================================================================
// SELECT Statement Tests
// =============================================================================

TEST_F(ParserDMLV2Test, BasicSelect) {
    auto* stmt = parseAs<SelectStmt>("SELECT 1");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->items.size(), 1);
    EXPECT_FALSE(stmt->distinct);
    EXPECT_EQ(stmt->from, nullptr);  // No FROM clause
}

TEST_F(ParserDMLV2Test, SelectDistinct) {
    auto* stmt = parseAs<SelectStmt>("SELECT DISTINCT name FROM users");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->distinct);
    EXPECT_EQ(stmt->items.size(), 1);
    EXPECT_NE(stmt->from, nullptr);
}

TEST_F(ParserDMLV2Test, SelectStar) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->items.size(), 1);
    EXPECT_EQ(stmt->items[0]->item_type, SelectItem::Type::STAR);
}

TEST_F(ParserDMLV2Test, SelectTableStar) {
    auto* stmt = parseAs<SelectStmt>("SELECT users.* FROM users");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->items.size(), 1);
    EXPECT_EQ(stmt->items[0]->item_type, SelectItem::Type::TABLE_STAR);
}

TEST_F(ParserDMLV2Test, SelectMultipleColumns) {
    auto* stmt = parseAs<SelectStmt>("SELECT id, name, email FROM users");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->items.size(), 3);
}

TEST_F(ParserDMLV2Test, SelectWithAlias) {
    auto* stmt = parseAs<SelectStmt>("SELECT name AS user_name FROM users");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->items.size(), 1);
    EXPECT_TRUE(stmt->items[0]->has_alias);
}

TEST_F(ParserDMLV2Test, SelectWithWhere) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users WHERE id = 1");
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->where, nullptr);
}

TEST_F(ParserDMLV2Test, SelectWithGroupBy) {
    auto* stmt = parseAs<SelectStmt>("SELECT count, name FROM users GROUP BY name");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->group_by.empty());
}

TEST_F(ParserDMLV2Test, SelectWithHaving) {
    auto* stmt = parseAs<SelectStmt>("SELECT name FROM users GROUP BY name HAVING count > 1");
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->having, nullptr);
}

TEST_F(ParserDMLV2Test, SelectWithOrderBy) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users ORDER BY name ASC");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->order_by.empty());
    EXPECT_TRUE(stmt->order_by[0]->ascending);
}

TEST_F(ParserDMLV2Test, SelectWithOrderByDesc) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users ORDER BY name DESC");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->order_by.empty());
    EXPECT_FALSE(stmt->order_by[0]->ascending);
}

TEST_F(ParserDMLV2Test, SelectWithLimit) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users LIMIT 10");
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->limit, nullptr);
}

TEST_F(ParserDMLV2Test, SelectWithLimitOffset) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users LIMIT 10 OFFSET 5");
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->limit, nullptr);
    EXPECT_NE(stmt->offset, nullptr);
}

TEST_F(ParserDMLV2Test, SelectUnion) {
    auto* stmt = parseAs<SelectStmt>("SELECT 1 UNION SELECT 2");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->set_op, SetOpType::UNION);
    EXPECT_NE(stmt->set_op_right, nullptr);
}

TEST_F(ParserDMLV2Test, SelectUnionAll) {
    auto* stmt = parseAs<SelectStmt>("SELECT 1 UNION ALL SELECT 2");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->set_op, SetOpType::UNION);
    EXPECT_TRUE(stmt->set_op_all);
}

TEST_F(ParserDMLV2Test, SelectForUpdate) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users FOR UPDATE");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->for_update);
}

TEST_F(ParserDMLV2Test, SelectWithCTE) {
    auto* stmt = parseAs<SelectStmt>(
        "WITH cte AS (SELECT 1 AS id) SELECT id FROM cte");
    ASSERT_NE(stmt, nullptr);
    ASSERT_NE(stmt->with, nullptr);
    EXPECT_FALSE(stmt->with->recursive);
    ASSERT_EQ(stmt->with->ctes.size(), 1u);
    EXPECT_NE(stmt->with->ctes[0].query, nullptr);
}

TEST_F(ParserDMLV2Test, SelectWithRecursiveCTE) {
    auto* stmt = parseAs<SelectStmt>(
        "WITH RECURSIVE t(n) AS (SELECT 1 UNION ALL SELECT n + 1 FROM t) "
        "SELECT n FROM t");
    ASSERT_NE(stmt, nullptr);
    ASSERT_NE(stmt->with, nullptr);
    EXPECT_TRUE(stmt->with->recursive);
    ASSERT_EQ(stmt->with->ctes.size(), 1u);
    EXPECT_TRUE(stmt->with->ctes[0].recursive);
}

// =============================================================================
// JOIN Tests
// =============================================================================

TEST_F(ParserDMLV2Test, SelectInnerJoin) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users JOIN orders ON users.id = orders.user_id");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->joins.empty());
    EXPECT_EQ(stmt->joins[0]->join_type, JoinType::INNER);
}

TEST_F(ParserDMLV2Test, SelectLeftJoin) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users LEFT JOIN orders ON users.id = orders.user_id");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->joins.empty());
    EXPECT_EQ(stmt->joins[0]->join_type, JoinType::LEFT);
}

TEST_F(ParserDMLV2Test, SelectRightJoin) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users RIGHT JOIN orders ON users.id = orders.user_id");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->joins.empty());
    EXPECT_EQ(stmt->joins[0]->join_type, JoinType::RIGHT);
}

TEST_F(ParserDMLV2Test, SelectFullJoin) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users FULL JOIN orders ON users.id = orders.user_id");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->joins.empty());
    EXPECT_EQ(stmt->joins[0]->join_type, JoinType::FULL);
}

TEST_F(ParserDMLV2Test, SelectCrossJoin) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users CROSS JOIN orders");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->joins.empty());
    EXPECT_EQ(stmt->joins[0]->join_type, JoinType::CROSS);
}

TEST_F(ParserDMLV2Test, SelectJoinUsing) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users JOIN orders USING (user_id)");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->joins.empty());
    EXPECT_TRUE(stmt->joins[0]->has_using);
}

// =============================================================================
// INSERT Statement Tests
// =============================================================================

TEST_F(ParserDMLV2Test, InsertValues) {
    auto* stmt = parseAs<InsertStmt>("INSERT INTO users VALUES (1, 'name')");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->source, InsertStmt::Source::VALUES);
    EXPECT_FALSE(stmt->values_rows.empty());
}

TEST_F(ParserDMLV2Test, InsertWithColumns) {
    auto* stmt = parseAs<InsertStmt>("INSERT INTO users (id, name) VALUES (1, 'test')");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->columns.empty());
    EXPECT_EQ(stmt->columns.size(), 2);
}

TEST_F(ParserDMLV2Test, InsertMultipleRows) {
    auto* stmt = parseAs<InsertStmt>("INSERT INTO users VALUES (1, 'a'), (2, 'b')");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->values_rows.size(), 2);
}

TEST_F(ParserDMLV2Test, InsertSelect) {
    auto* stmt = parseAs<InsertStmt>("INSERT INTO users SELECT * FROM other_users");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->source, InsertStmt::Source::SELECT);
    EXPECT_NE(stmt->select_source, nullptr);
}

TEST_F(ParserDMLV2Test, InsertDefaultValues) {
    auto* stmt = parseAs<InsertStmt>("INSERT INTO users DEFAULT VALUES");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->source, InsertStmt::Source::DEFAULT);
}

TEST_F(ParserDMLV2Test, InsertOnConflictDoNothing) {
    auto* stmt = parseAs<InsertStmt>("INSERT INTO users VALUES (1) ON CONFLICT DO NOTHING");
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->on_conflict, nullptr);
    EXPECT_EQ(stmt->on_conflict->action, ConflictAction::NOTHING);
}

TEST_F(ParserDMLV2Test, InsertOnConflictDoUpdate) {
    auto* stmt = parseAs<InsertStmt>("INSERT INTO users VALUES (1) ON CONFLICT (id) DO UPDATE SET name = 'new'");
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->on_conflict, nullptr);
    EXPECT_EQ(stmt->on_conflict->action, ConflictAction::UPDATE);
}

TEST_F(ParserDMLV2Test, InsertReturning) {
    auto* stmt = parseAs<InsertStmt>("INSERT INTO users VALUES (1) RETURNING id");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->returning.empty());
}

TEST_F(ParserDMLV2Test, InsertWithCTE) {
    auto* stmt = parseAs<InsertStmt>(
        "WITH src AS (SELECT 1 AS id) INSERT INTO users (id) SELECT id FROM src");
    ASSERT_NE(stmt, nullptr);
    ASSERT_NE(stmt->with, nullptr);
    EXPECT_EQ(stmt->with->ctes.size(), 1u);
}

// =============================================================================
// UPDATE Statement Tests
// =============================================================================

TEST_F(ParserDMLV2Test, BasicUpdate) {
    auto* stmt = parseAs<UpdateStmt>("UPDATE users SET name = 'test'");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->set_items.empty());
}

TEST_F(ParserDMLV2Test, UpdateWithWhere) {
    auto* stmt = parseAs<UpdateStmt>("UPDATE users SET name = 'test' WHERE id = 1");
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->where, nullptr);
}

TEST_F(ParserDMLV2Test, UpdateMultipleColumns) {
    auto* stmt = parseAs<UpdateStmt>("UPDATE users SET name = 'test', email = 'a@b.com' WHERE id = 1");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->set_items.size(), 2);
}

TEST_F(ParserDMLV2Test, UpdateReturning) {
    auto* stmt = parseAs<UpdateStmt>("UPDATE users SET name = 'test' RETURNING *");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->returning.empty());
}

TEST_F(ParserDMLV2Test, UpdateWithCTE) {
    auto* stmt = parseAs<UpdateStmt>(
        "WITH src AS (SELECT 1 AS id) UPDATE users SET name = 'x' WHERE id IN (SELECT id FROM src)");
    ASSERT_NE(stmt, nullptr);
    ASSERT_NE(stmt->with, nullptr);
    EXPECT_EQ(stmt->with->ctes.size(), 1u);
}

// =============================================================================
// DELETE Statement Tests
// =============================================================================

TEST_F(ParserDMLV2Test, BasicDelete) {
    auto* stmt = parseAs<DeleteStmt>("DELETE FROM users");
    ASSERT_NE(stmt, nullptr);
}

TEST_F(ParserDMLV2Test, DeleteWithWhere) {
    auto* stmt = parseAs<DeleteStmt>("DELETE FROM users WHERE id = 1");
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->where, nullptr);
}

TEST_F(ParserDMLV2Test, DeleteReturning) {
    auto* stmt = parseAs<DeleteStmt>("DELETE FROM users RETURNING *");
    ASSERT_NE(stmt, nullptr);
    EXPECT_FALSE(stmt->returning.empty());
}

TEST_F(ParserDMLV2Test, DeleteWithCTE) {
    auto* stmt = parseAs<DeleteStmt>(
        "WITH src AS (SELECT id FROM users WHERE id < 10) "
        "DELETE FROM users WHERE id IN (SELECT id FROM src)");
    ASSERT_NE(stmt, nullptr);
    ASSERT_NE(stmt->with, nullptr);
    EXPECT_EQ(stmt->with->ctes.size(), 1u);
}

// =============================================================================
// Expression Tests in DML Context
// =============================================================================

TEST_F(ParserDMLV2Test, SelectWithCaseExpression) {
    auto* stmt = parseAs<SelectStmt>("SELECT CASE WHEN x = 1 THEN 'a' ELSE 'b' END FROM t");
    ASSERT_NE(stmt, nullptr);
    EXPECT_EQ(stmt->items.size(), 1);
}

TEST_F(ParserDMLV2Test, SelectWithInExpression) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users WHERE id IN (1, 2, 3)");
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->where, nullptr);
}

TEST_F(ParserDMLV2Test, SelectWithBetweenExpression) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users WHERE id BETWEEN 1 AND 10");
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->where, nullptr);
}

TEST_F(ParserDMLV2Test, SelectWithLikeExpression) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users WHERE name LIKE 'test%'");
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->where, nullptr);
}

TEST_F(ParserDMLV2Test, SelectWithIsNull) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users WHERE name IS NULL");
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->where, nullptr);
}

TEST_F(ParserDMLV2Test, SelectWithIsNotNull) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users WHERE name IS NOT NULL");
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->where, nullptr);
}

TEST_F(ParserDMLV2Test, SelectWithSubquery) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users WHERE id IN (SELECT user_id FROM orders)");
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->where, nullptr);
}

TEST_F(ParserDMLV2Test, SelectWithExistsExpression) {
    auto* stmt = parseAs<SelectStmt>("SELECT * FROM users WHERE EXISTS (SELECT 1 FROM orders WHERE orders.user_id = users.id)");
    ASSERT_NE(stmt, nullptr);
    EXPECT_NE(stmt->where, nullptr);
}
