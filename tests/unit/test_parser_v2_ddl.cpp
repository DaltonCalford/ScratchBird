/**
 * Unit Tests for ScratchBird Parser v2.0 - DDL Statements
 *
 * Tests CREATE TABLE, CREATE INDEX, CREATE VIEW, CREATE SEQUENCE,
 * ALTER TABLE, DROP statements, and TRUNCATE.
 */

#include <gtest/gtest.h>
#include "scratchbird/parser/parser_v2.h"

using namespace scratchbird::parser::v2;

class ParserV2DDLTest : public ::testing::Test {
protected:
    std::string getString(Parser& parser, StringPool::StringId id) {
        return std::string(parser.stringPool().get(id));
    }

    std::string getPathString(Parser& parser, const SchemaPath& path) {
        return schemaPathToString(path, parser.stringPool());
    }
};

// =============================================================================
// CREATE TABLE - Basic Tests
// =============================================================================

TEST_F(ParserV2DDLTest, CreateTable_Simple) {
    Parser parser("CREATE TABLE users (id INT, name VARCHAR(100))");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success()) << "Errors: " << (result.errors().empty() ? "none" : result.errors()[0].message);

    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());
    ASSERT_NE(stmt, nullptr);

    EXPECT_EQ(getPathString(parser, stmt->table_path), "users");
    EXPECT_EQ(stmt->columns.size(), 2);

    EXPECT_EQ(getString(parser, stmt->columns[0]->name), "id");
    EXPECT_EQ(getString(parser, stmt->columns[0]->type.name), "INT");

    EXPECT_EQ(getString(parser, stmt->columns[1]->name), "name");
    EXPECT_EQ(getString(parser, stmt->columns[1]->type.name), "VARCHAR");
    EXPECT_EQ(stmt->columns[1]->type.length.value(), 100);
}

TEST_F(ParserV2DDLTest, CreateTable_SchemaQualified) {
    Parser parser("CREATE TABLE public.orders (id INT)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    EXPECT_EQ(stmt->table_path.type, PathType::ABSOLUTE);
    EXPECT_EQ(getPathString(parser, stmt->table_path), "public.orders");
}

TEST_F(ParserV2DDLTest, CreateTable_CurrentSchema) {
    Parser parser("CREATE TABLE .orders (id INT)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    EXPECT_EQ(stmt->table_path.type, PathType::CURRENT);
    EXPECT_EQ(getPathString(parser, stmt->table_path), ".orders");
}

TEST_F(ParserV2DDLTest, CreateTable_IfNotExists) {
    Parser parser("CREATE TABLE IF NOT EXISTS users (id INT)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    EXPECT_TRUE(stmt->if_not_exists);
}

TEST_F(ParserV2DDLTest, CreateTable_Temporary) {
    Parser parser("CREATE TEMPORARY TABLE temp_data (id INT)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    EXPECT_TRUE(stmt->temporary);
}

// =============================================================================
// CREATE TABLE - Column Types
// =============================================================================

TEST_F(ParserV2DDLTest, CreateTable_NumericTypes) {
    Parser parser("CREATE TABLE t (a SMALLINT, b INTEGER, c BIGINT, d DECIMAL(10,2), e FLOAT)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    EXPECT_EQ(stmt->columns.size(), 5);
    EXPECT_EQ(getString(parser, stmt->columns[3]->type.name), "DECIMAL");
    EXPECT_EQ(stmt->columns[3]->type.precision.value(), 10);
    EXPECT_EQ(stmt->columns[3]->type.scale.value(), 2);
}

TEST_F(ParserV2DDLTest, CreateTable_StringTypes) {
    Parser parser("CREATE TABLE t (a CHAR(10), b VARCHAR(255), c TEXT)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    EXPECT_EQ(stmt->columns.size(), 3);
    EXPECT_EQ(stmt->columns[0]->type.length.value(), 10);
    EXPECT_EQ(stmt->columns[1]->type.length.value(), 255);
}

TEST_F(ParserV2DDLTest, CreateTable_ArrayType) {
    Parser parser("CREATE TABLE t (tags VARCHAR[])");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    EXPECT_TRUE(stmt->columns[0]->type.is_array);
}

TEST_F(ParserV2DDLTest, CreateTable_TimestampWithTimeZone) {
    Parser parser("CREATE TABLE t (created TIMESTAMP WITH TIME ZONE)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    EXPECT_TRUE(stmt->columns[0]->type.with_time_zone);
}

// =============================================================================
// CREATE TABLE - Column Constraints
// =============================================================================

TEST_F(ParserV2DDLTest, CreateTable_NotNull) {
    Parser parser("CREATE TABLE t (id INT NOT NULL)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    ASSERT_GE(stmt->columns[0]->constraints.size(), 1);
    EXPECT_EQ(stmt->columns[0]->constraints[0].type, ConstraintType::NOT_NULL);
}

TEST_F(ParserV2DDLTest, CreateTable_PrimaryKey) {
    Parser parser("CREATE TABLE t (id INT PRIMARY KEY)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    ASSERT_GE(stmt->columns[0]->constraints.size(), 1);
    EXPECT_EQ(stmt->columns[0]->constraints[0].type, ConstraintType::PRIMARY_KEY);
}

TEST_F(ParserV2DDLTest, CreateTable_Unique) {
    Parser parser("CREATE TABLE t (email VARCHAR(100) UNIQUE)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    ASSERT_GE(stmt->columns[0]->constraints.size(), 1);
    EXPECT_EQ(stmt->columns[0]->constraints[0].type, ConstraintType::UNIQUE);
}

TEST_F(ParserV2DDLTest, CreateTable_Default) {
    Parser parser("CREATE TABLE t (status INT DEFAULT 0)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    ASSERT_GE(stmt->columns[0]->constraints.size(), 1);
    EXPECT_EQ(stmt->columns[0]->constraints[0].type, ConstraintType::DEFAULT);
    EXPECT_NE(stmt->columns[0]->constraints[0].default_expr, nullptr);
}

TEST_F(ParserV2DDLTest, CreateTable_References) {
    Parser parser("CREATE TABLE orders (user_id INT REFERENCES users(id))");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    ASSERT_GE(stmt->columns[0]->constraints.size(), 1);
    auto& constraint = stmt->columns[0]->constraints[0];
    EXPECT_EQ(constraint.type, ConstraintType::REFERENCES);
    EXPECT_EQ(getPathString(parser, constraint.ref_table), "users");
    ASSERT_EQ(constraint.ref_columns.size(), 1);
    EXPECT_EQ(getString(parser, constraint.ref_columns[0]), "id");
}

TEST_F(ParserV2DDLTest, CreateTable_ReferencesOnDelete) {
    Parser parser("CREATE TABLE orders (user_id INT REFERENCES users ON DELETE CASCADE)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    auto& constraint = stmt->columns[0]->constraints[0];
    EXPECT_EQ(constraint.on_delete, ForeignKeyAction::CASCADE);
}

TEST_F(ParserV2DDLTest, CreateTable_MultipleColumnConstraints) {
    Parser parser("CREATE TABLE t (id INT NOT NULL PRIMARY KEY DEFAULT 1)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    EXPECT_GE(stmt->columns[0]->constraints.size(), 3);
}

// =============================================================================
// CREATE TABLE - Table Constraints
// =============================================================================

TEST_F(ParserV2DDLTest, CreateTable_TablePrimaryKey) {
    Parser parser("CREATE TABLE t (id INT, name VARCHAR(50), PRIMARY KEY (id))");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    ASSERT_EQ(stmt->constraints.size(), 1);
    EXPECT_EQ(stmt->constraints[0]->type, TableConstraintType::PRIMARY_KEY);
    ASSERT_EQ(stmt->constraints[0]->columns.size(), 1);
    EXPECT_EQ(getString(parser, stmt->constraints[0]->columns[0]), "id");
}

TEST_F(ParserV2DDLTest, CreateTable_CompositePrimaryKey) {
    Parser parser("CREATE TABLE t (a INT, b INT, PRIMARY KEY (a, b))");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    ASSERT_EQ(stmt->constraints.size(), 1);
    EXPECT_EQ(stmt->constraints[0]->columns.size(), 2);
}

TEST_F(ParserV2DDLTest, CreateTable_NamedConstraint) {
    Parser parser("CREATE TABLE t (id INT, CONSTRAINT pk_t PRIMARY KEY (id))");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    ASSERT_EQ(stmt->constraints.size(), 1);
    EXPECT_EQ(getString(parser, stmt->constraints[0]->name), "pk_t");
}

TEST_F(ParserV2DDLTest, CreateTable_ForeignKey) {
    Parser parser("CREATE TABLE orders (id INT, user_id INT, FOREIGN KEY (user_id) REFERENCES users(id))");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    ASSERT_EQ(stmt->constraints.size(), 1);
    EXPECT_EQ(stmt->constraints[0]->type, TableConstraintType::FOREIGN_KEY);
}

TEST_F(ParserV2DDLTest, CreateTable_CheckConstraint) {
    Parser parser("CREATE TABLE t (age INT, CHECK (age > 0))");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    ASSERT_EQ(stmt->constraints.size(), 1);
    EXPECT_EQ(stmt->constraints[0]->type, TableConstraintType::CHECK);
    EXPECT_NE(stmt->constraints[0]->check_expr, nullptr);
}

// =============================================================================
// CREATE TABLE - Using Contextual Keywords as Identifiers
// =============================================================================

TEST_F(ParserV2DDLTest, CreateTable_ContextualKeywordsAsNames) {
    // The key test from the design: TABLE, procedure, value, type, name should all work as identifiers
    Parser parser("CREATE TABLE procedure (value INT, type VARCHAR(50), name TEXT)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success()) << "Errors: " << (result.errors().empty() ? "none" : result.errors()[0].message);
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    EXPECT_EQ(getPathString(parser, stmt->table_path), "procedure");
    EXPECT_EQ(stmt->columns.size(), 3);
    EXPECT_EQ(getString(parser, stmt->columns[0]->name), "value");
    EXPECT_EQ(getString(parser, stmt->columns[1]->name), "type");
    EXPECT_EQ(getString(parser, stmt->columns[2]->name), "name");
}

TEST_F(ParserV2DDLTest, CreateTable_IndexAsTableName) {
    Parser parser("CREATE TABLE index (data TEXT)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateTableStmt*>(result.statement());

    EXPECT_EQ(getPathString(parser, stmt->table_path), "index");
}

// =============================================================================
// CREATE INDEX Tests
// =============================================================================

TEST_F(ParserV2DDLTest, CreateIndex_Simple) {
    Parser parser("CREATE INDEX idx_users_email ON users (email)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateIndexStmt*>(result.statement());
    ASSERT_NE(stmt, nullptr);

    EXPECT_EQ(getString(parser, stmt->index_name), "idx_users_email");
    EXPECT_EQ(getPathString(parser, stmt->table_path), "users");
    EXPECT_EQ(stmt->columns.size(), 1);
}

TEST_F(ParserV2DDLTest, CreateIndex_Unique) {
    Parser parser("CREATE UNIQUE INDEX idx ON users (email)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateIndexStmt*>(result.statement());

    EXPECT_TRUE(stmt->unique);
}

TEST_F(ParserV2DDLTest, CreateIndex_MultiColumn) {
    Parser parser("CREATE INDEX idx ON t (a, b, c)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateIndexStmt*>(result.statement());

    EXPECT_EQ(stmt->columns.size(), 3);
}

TEST_F(ParserV2DDLTest, CreateIndex_UsingBtree) {
    Parser parser("CREATE INDEX idx ON t USING BTREE (id)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateIndexStmt*>(result.statement());

    EXPECT_EQ(stmt->index_type, IndexType::BTREE);
}

TEST_F(ParserV2DDLTest, CreateIndex_UsingGin) {
    Parser parser("CREATE INDEX idx ON t USING GIN (tags)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateIndexStmt*>(result.statement());

    EXPECT_EQ(stmt->index_type, IndexType::GIN);
}

TEST_F(ParserV2DDLTest, CreateIndex_DescNullsFirst) {
    Parser parser("CREATE INDEX idx ON t (created DESC NULLS FIRST)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateIndexStmt*>(result.statement());

    EXPECT_FALSE(stmt->columns[0].ascending);
    EXPECT_TRUE(stmt->columns[0].nulls_first);
}

TEST_F(ParserV2DDLTest, CreateIndex_PartialWhere) {
    Parser parser("CREATE INDEX idx ON t (status) WHERE status = 1");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateIndexStmt*>(result.statement());

    EXPECT_NE(stmt->where_clause, nullptr);
}

TEST_F(ParserV2DDLTest, CreateIndex_Include) {
    Parser parser("CREATE INDEX idx ON t (id) INCLUDE (name, email)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateIndexStmt*>(result.statement());

    EXPECT_EQ(stmt->include_columns.size(), 2);
}

// =============================================================================
// CREATE SEQUENCE Tests
// =============================================================================

TEST_F(ParserV2DDLTest, CreateSequence_Simple) {
    Parser parser("CREATE SEQUENCE my_seq");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateSequenceStmt*>(result.statement());
    ASSERT_NE(stmt, nullptr);

    EXPECT_EQ(getPathString(parser, stmt->sequence_path), "my_seq");
}

TEST_F(ParserV2DDLTest, CreateSequence_AllOptions) {
    Parser parser("CREATE SEQUENCE my_seq START WITH 100 INCREMENT BY 10 MINVALUE 1 MAXVALUE 10000 CACHE 20 CYCLE");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<CreateSequenceStmt*>(result.statement());

    EXPECT_EQ(stmt->start_with.value(), 100);
    EXPECT_EQ(stmt->increment_by.value(), 10);
    EXPECT_EQ(stmt->min_value.value(), 1);
    EXPECT_EQ(stmt->max_value.value(), 10000);
    EXPECT_EQ(stmt->cache.value(), 20);
    EXPECT_TRUE(stmt->cycle);
}

// =============================================================================
// CREATE DOMAIN Tests
// =============================================================================

TEST_F(ParserV2DDLTest, CreateDomain_WithIntegrity) {
    Parser parser(
        "CREATE DOMAIN IF NOT EXISTS public.user_name AS TEXT "
        "NOT NULL DEFAULT 'x' CHECK (VALUE <> '') "
        "WITH INTEGRITY (UNIQUENESS = TRUE)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success()) << "Errors: "
                                  << (result.errors().empty() ? "none" : result.errors()[0].message);

    auto* stmt = dynamic_cast<CreateDomainStmt*>(result.statement());
    ASSERT_NE(stmt, nullptr);

    EXPECT_TRUE(stmt->if_not_exists);
    EXPECT_EQ(getPathString(parser, stmt->domain_path), "public.user_name");
    EXPECT_EQ(getString(parser, stmt->base_type.name), "TEXT");

    bool saw_not_null = false;
    bool saw_default = false;
    bool saw_check = false;
    for (const auto& constraint : stmt->constraints) {
        if (constraint.type == DomainConstraintType::NOT_NULL) {
            saw_not_null = true;
        } else if (constraint.type == DomainConstraintType::DEFAULT) {
            saw_default = true;
            EXPECT_EQ(constraint.expression, "'x'");
        } else if (constraint.type == DomainConstraintType::CHECK) {
            saw_check = true;
            EXPECT_EQ(constraint.expression, "VALUE <> ''");
        }
    }

    EXPECT_TRUE(saw_not_null);
    EXPECT_TRUE(saw_default);
    EXPECT_TRUE(saw_check);

    EXPECT_TRUE(stmt->has_integrity);
    EXPECT_TRUE(stmt->integrity.uniqueness);
}

TEST_F(ParserV2DDLTest, CreateDomain_WithSecurityValidationQuality) {
    Parser parser(
        "CREATE DOMAIN public.email AS TEXT "
        "WITH SECURITY (MASKING = PARTIAL, MASK_PATTERN = '***', ENCRYPTION = AES256, "
        "AUDIT_ACCESS = TRUE, REQUIRE_PRIVILEGE = 'unmask') "
        "WITH VALIDATION (FUNCTION = validate_email, ERROR_MESSAGE = 'bad email') "
        "WITH QUALITY (PARSE_FUNCTION = parse_email, STANDARDIZE_FUNCTION = std_email, "
        "ENRICH_FUNCTION = enrich_email)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success()) << "Errors: "
                                  << (result.errors().empty() ? "none" : result.errors()[0].message);

    auto* stmt = dynamic_cast<CreateDomainStmt*>(result.statement());
    ASSERT_NE(stmt, nullptr);

    EXPECT_EQ(getPathString(parser, stmt->domain_path), "public.email");
    EXPECT_TRUE(stmt->has_security);
    EXPECT_TRUE(stmt->security.has_masking);
    EXPECT_EQ(stmt->security.masking, "PARTIAL");
    EXPECT_TRUE(stmt->security.has_mask_pattern);
    EXPECT_EQ(stmt->security.mask_pattern, "***");
    EXPECT_TRUE(stmt->security.has_encryption);
    EXPECT_EQ(stmt->security.encryption, "AES256");
    EXPECT_TRUE(stmt->security.has_audit_access);
    EXPECT_TRUE(stmt->security.audit_access);
    EXPECT_TRUE(stmt->security.has_required_privilege);
    EXPECT_EQ(stmt->security.required_privilege, "unmask");

    EXPECT_TRUE(stmt->has_validation);
    EXPECT_TRUE(stmt->validation.has_function);
    EXPECT_EQ(stmt->validation.function, "validate_email");
    EXPECT_TRUE(stmt->validation.has_error_message);
    EXPECT_EQ(stmt->validation.error_message, "bad email");

    EXPECT_TRUE(stmt->has_quality);
    EXPECT_TRUE(stmt->quality.has_parse_function);
    EXPECT_EQ(stmt->quality.parse_function, "parse_email");
    EXPECT_TRUE(stmt->quality.has_standardize_function);
    EXPECT_EQ(stmt->quality.standardize_function, "std_email");
    EXPECT_TRUE(stmt->quality.has_enrich_function);
    EXPECT_EQ(stmt->quality.enrich_function, "enrich_email");
}

TEST_F(ParserV2DDLTest, AlterDomain_SetDefault) {
    Parser parser("ALTER DOMAIN public.user_name SET DEFAULT 42");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<AlterDomainStmt*>(result.statement());
    ASSERT_NE(stmt, nullptr);

    EXPECT_EQ(getPathString(parser, stmt->domain_path), "public.user_name");
    EXPECT_EQ(stmt->action, AlterDomainAction::SET_DEFAULT);
    EXPECT_EQ(stmt->value, "42");
}

TEST_F(ParserV2DDLTest, AlterDomain_AddCheck) {
    Parser parser("ALTER DOMAIN public.user_name ADD CHECK (VALUE <> '')");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<AlterDomainStmt*>(result.statement());
    ASSERT_NE(stmt, nullptr);

    EXPECT_EQ(getPathString(parser, stmt->domain_path), "public.user_name");
    EXPECT_EQ(stmt->action, AlterDomainAction::ADD_CHECK);
    EXPECT_EQ(stmt->value, "VALUE <> ''");
}

TEST_F(ParserV2DDLTest, AlterDomain_Rename) {
    Parser parser("ALTER DOMAIN public.user_name RENAME TO user_name2");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<AlterDomainStmt*>(result.statement());
    ASSERT_NE(stmt, nullptr);

    EXPECT_EQ(getPathString(parser, stmt->domain_path), "public.user_name");
    EXPECT_EQ(stmt->action, AlterDomainAction::RENAME);
    EXPECT_EQ(getString(parser, stmt->new_name), "user_name2");
}

TEST_F(ParserV2DDLTest, AlterDomain_SetCompat) {
    Parser parser("ALTER DOMAIN public.user_name SET COMPAT 'VARCHAR'");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<AlterDomainStmt*>(result.statement());
    ASSERT_NE(stmt, nullptr);

    EXPECT_EQ(stmt->action, AlterDomainAction::SET_COMPAT);
    EXPECT_EQ(stmt->value, "VARCHAR");
}

TEST_F(ParserV2DDLTest, DropDomain_IfExistsRestrict) {
    Parser parser("DROP DOMAIN IF EXISTS public.user_name RESTRICT");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<DropDomainStmt*>(result.statement());
    ASSERT_NE(stmt, nullptr);

    EXPECT_TRUE(stmt->if_exists);
    EXPECT_TRUE(stmt->restrict);
    ASSERT_EQ(stmt->domains.size(), 1u);
    EXPECT_EQ(getPathString(parser, stmt->domains[0]), "public.user_name");
}

// =============================================================================
// ALTER TABLE Tests
// =============================================================================

TEST_F(ParserV2DDLTest, AlterTable_AddColumn) {
    Parser parser("ALTER TABLE users ADD COLUMN age INT");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<AlterTableStmt*>(result.statement());
    ASSERT_NE(stmt, nullptr);

    EXPECT_EQ(stmt->action, AlterTableAction::ADD_COLUMN);
    EXPECT_NE(stmt->column, nullptr);
    EXPECT_EQ(getString(parser, stmt->column->name), "age");
}

TEST_F(ParserV2DDLTest, AlterTable_DropColumn) {
    Parser parser("ALTER TABLE users DROP COLUMN age");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<AlterTableStmt*>(result.statement());

    EXPECT_EQ(stmt->action, AlterTableAction::DROP_COLUMN);
    EXPECT_EQ(getString(parser, stmt->column_name), "age");
}

TEST_F(ParserV2DDLTest, AlterTable_RenameColumn) {
    Parser parser("ALTER TABLE users RENAME COLUMN name TO full_name");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<AlterTableStmt*>(result.statement());

    EXPECT_EQ(stmt->action, AlterTableAction::RENAME_COLUMN);
    EXPECT_EQ(getString(parser, stmt->column_name), "name");
    EXPECT_EQ(getString(parser, stmt->new_name), "full_name");
}

TEST_F(ParserV2DDLTest, AlterTable_AddConstraint) {
    Parser parser("ALTER TABLE users ADD CONSTRAINT pk_users PRIMARY KEY (id)");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<AlterTableStmt*>(result.statement());

    EXPECT_EQ(stmt->action, AlterTableAction::ADD_CONSTRAINT);
    EXPECT_NE(stmt->constraint, nullptr);
}

TEST_F(ParserV2DDLTest, AlterTable_EnableRLS) {
    Parser parser("ALTER TABLE users ENABLE ROW LEVEL SECURITY");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<AlterTableStmt*>(result.statement());

    EXPECT_EQ(stmt->action, AlterTableAction::ENABLE_RLS);
}

// =============================================================================
// DROP Statement Tests
// =============================================================================

TEST_F(ParserV2DDLTest, DropTable_Simple) {
    Parser parser("DROP TABLE users");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<DropTableStmt*>(result.statement());
    ASSERT_NE(stmt, nullptr);

    EXPECT_EQ(stmt->tables.size(), 1);
    EXPECT_EQ(getPathString(parser, stmt->tables[0]), "users");
}

TEST_F(ParserV2DDLTest, DropTable_IfExists) {
    Parser parser("DROP TABLE IF EXISTS users");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<DropTableStmt*>(result.statement());

    EXPECT_TRUE(stmt->if_exists);
}

TEST_F(ParserV2DDLTest, DropTable_MultipleTables) {
    Parser parser("DROP TABLE t1, t2, t3");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<DropTableStmt*>(result.statement());

    EXPECT_EQ(stmt->tables.size(), 3);
}

TEST_F(ParserV2DDLTest, DropTable_Cascade) {
    Parser parser("DROP TABLE users CASCADE");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<DropTableStmt*>(result.statement());

    EXPECT_TRUE(stmt->cascade);
}

TEST_F(ParserV2DDLTest, DropIndex_Simple) {
    Parser parser("DROP INDEX idx_users_email");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<DropIndexStmt*>(result.statement());
    ASSERT_NE(stmt, nullptr);
}

TEST_F(ParserV2DDLTest, DropView_Simple) {
    Parser parser("DROP VIEW my_view");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<DropViewStmt*>(result.statement());
    ASSERT_NE(stmt, nullptr);
}

TEST_F(ParserV2DDLTest, DropMaterializedView) {
    Parser parser("DROP MATERIALIZED VIEW mv_summary");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<DropViewStmt*>(result.statement());

    EXPECT_TRUE(stmt->materialized);
}

// =============================================================================
// TRUNCATE Statement Tests
// =============================================================================

TEST_F(ParserV2DDLTest, Truncate_Simple) {
    Parser parser("TRUNCATE TABLE users");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<TruncateTableStmt*>(result.statement());
    ASSERT_NE(stmt, nullptr);

    EXPECT_EQ(stmt->tables.size(), 1);
}

TEST_F(ParserV2DDLTest, Truncate_MultipleTables) {
    Parser parser("TRUNCATE t1, t2");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<TruncateTableStmt*>(result.statement());

    EXPECT_EQ(stmt->tables.size(), 2);
}

TEST_F(ParserV2DDLTest, Truncate_RestartIdentity) {
    Parser parser("TRUNCATE users RESTART IDENTITY");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<TruncateTableStmt*>(result.statement());

    EXPECT_TRUE(stmt->restart_identity);
}

TEST_F(ParserV2DDLTest, Truncate_Cascade) {
    Parser parser("TRUNCATE users CASCADE");
    auto result = parser.parseStatement();

    ASSERT_TRUE(result.success());
    auto* stmt = dynamic_cast<TruncateTableStmt*>(result.statement());

    EXPECT_TRUE(stmt->cascade);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(ParserV2DDLTest, Error_MissingTableName) {
    Parser parser("CREATE TABLE ()");
    auto result = parser.parseStatement();

    EXPECT_FALSE(result.success());
    EXPECT_FALSE(result.errors().empty());
}

TEST_F(ParserV2DDLTest, Error_MissingParenthesis) {
    Parser parser("CREATE TABLE users id INT");
    auto result = parser.parseStatement();

    EXPECT_FALSE(result.success());
}

TEST_F(ParserV2DDLTest, Error_InvalidColumnType) {
    Parser parser("CREATE TABLE users (id INVALID_TYPE)");
    auto result = parser.parseStatement();

    // This should still parse (type is just an identifier)
    EXPECT_TRUE(result.success());
}
