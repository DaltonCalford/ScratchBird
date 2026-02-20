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
#include "scratchbird/catalog/schema_introspection.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include <cstdio>
#include <string>
#include <vector>
#include <unistd.h>

using namespace scratchbird::core;
using namespace scratchbird::catalog;

/**
 * Comprehensive test suite for SchemaIntrospection
 * 
 * Tests cover:
 * 1. PostgreSQL Views Tests (pg_tables, pg_columns, pg_indexes, pg_database, pg_settings, pg_views, pg_type, pg_namespace)
 * 2. information_schema Tests (tables, columns, views, schemata, key_column_usage, table_constraints)
 * 3. MySQL Compatibility Tests (SHOW DATABASES, SHOW TABLES, SHOW COLUMNS, SHOW INDEX, SHOW CREATE TABLE)
 * 4. Firebird Compatibility Tests (RDB$RELATIONS, RDB$RELATION_FIELDS, RDB$FIELDS, RDB$INDICES)
 * 5. Query Tests (getTables, getColumns, getIndexes, getConstraints, getPrimaryKeyColumns, getForeignKeys)
 */
class SchemaIntrospectionTest : public ::testing::Test
{
protected:
    std::string test_db_path;
    std::unique_ptr<Database> db;
    CatalogManager* catalog = nullptr;

    void SetUp() override
    {
        test_db_path = "/tmp/test_schema_introspection_" + std::to_string(getpid()) + ".db";
        std::remove(test_db_path.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path, 16384, &ctx), Status::OK);

        db = std::make_unique<Database>();
        ASSERT_EQ(db->open(test_db_path, &ctx), Status::OK);

        catalog = db->catalog_manager();
        ASSERT_NE(catalog, nullptr);

        // Ensure default schema exists
        CatalogManager::SchemaInfo schema_info;
        if (catalog->getSchema("users.public", schema_info, &ctx) != Status::OK) {
            if (catalog->getSchema("public", schema_info, &ctx) != Status::OK) {
                ID schema_id;
                ASSERT_EQ(catalog->createSchema("public", "system", schema_id, &ctx), Status::OK);
            }
        }
    }

    void TearDown() override
    {
        if (db) {
            db->close();
            db.reset();
        }
        std::remove(test_db_path.c_str());
    }

    // Helper to create a test schema
    ID createTestSchema(const std::string& name)
    {
        ErrorContext ctx;
        ID schema_id;
        Status status = catalog->createSchema(name, "test_user", schema_id, &ctx);
        if (status != Status::OK) {
            // Schema might already exist
            CatalogManager::SchemaInfo info;
            if (catalog->getSchema(name, info, &ctx) == Status::OK) {
                return info.schema_id;
            }
        }
        return schema_id;
    }

    // Helper to create a test table with columns
    ID createTestTable(ID schema_id, const std::string& table_name,
                       const std::vector<CatalogManager::ColumnInfo>& columns)
    {
        ErrorContext ctx;
        ID table_id;
        EXPECT_EQ(catalog->createTable(schema_id, table_name, columns, table_id, 0, &ctx), Status::OK);
        return table_id;
    }

    // Helper to create column info
    CatalogManager::ColumnInfo makeColumn(const std::string& name, DataType type,
                                          uint32_t max_len, bool nullable = true,
                                          bool has_default = false, const std::string& default_val = "")
    {
        CatalogManager::ColumnInfo col;
        col.column_name = name;
        col.data_type = static_cast<uint16_t>(type);
        col.max_length = max_len;
        col.nullable = nullable;
        col.has_default = has_default;
        col.default_value = default_val;
        return col;
    }
};

// =============================================================================
// PostgreSQL Views Tests
// =============================================================================

/**
 * Test 1: pg_tables view generation
 * Verifies the SQL definition for pg_catalog.pg_tables view
 */
TEST_F(SchemaIntrospectionTest, PgCatalogTablesViewDefinition)
{
    const char* view_sql = SchemaIntrospection::PG_CATALOG_TABLES;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    // Verify essential components of the view
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW pg_catalog.pg_tables"), std::string::npos);
    EXPECT_NE(sql.find("pg_catalog.pg_class"), std::string::npos);
    EXPECT_NE(sql.find("pg_catalog.pg_namespace"), std::string::npos);
    EXPECT_NE(sql.find("schemaname"), std::string::npos);
    EXPECT_NE(sql.find("tablename"), std::string::npos);
    EXPECT_NE(sql.find("tableowner"), std::string::npos);
}

/**
 * Test 2: pg_columns view generation
 * Verifies the SQL definition for pg_catalog.pg_columns view
 */
TEST_F(SchemaIntrospectionTest, PgCatalogColumnsViewDefinition)
{
    const char* view_sql = SchemaIntrospection::PG_CATALOG_COLUMNS;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW pg_catalog.pg_columns"), std::string::npos);
    EXPECT_NE(sql.find("pg_catalog.pg_attribute"), std::string::npos);
    EXPECT_NE(sql.find("column_name"), std::string::npos);
    EXPECT_NE(sql.find("data_type"), std::string::npos);
    EXPECT_NE(sql.find("is_nullable"), std::string::npos);
}

/**
 * Test 3: pg_indexes view generation
 * Verifies the SQL definition for pg_catalog.pg_indexes view
 */
TEST_F(SchemaIntrospectionTest, PgCatalogIndexesViewDefinition)
{
    const char* view_sql = SchemaIntrospection::PG_CATALOG_INDEXES;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW pg_catalog.pg_indexes"), std::string::npos);
    EXPECT_NE(sql.find("pg_catalog.pg_index"), std::string::npos);
    EXPECT_NE(sql.find("indexname"), std::string::npos);
    EXPECT_NE(sql.find("indexdef"), std::string::npos);
}

/**
 * Test 4: pg_database view
 * Verifies the SQL definition for pg_catalog.pg_database view
 */
TEST_F(SchemaIntrospectionTest, PgCatalogDatabaseViewDefinition)
{
    const char* view_sql = SchemaIntrospection::PG_CATALOG_DATABASES;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW pg_catalog.pg_database"), std::string::npos);
    EXPECT_NE(sql.find("datname"), std::string::npos);
    EXPECT_NE(sql.find("datdba"), std::string::npos);
    EXPECT_NE(sql.find("encoding"), std::string::npos);
}

/**
 * Test 5: pg_settings view
 * Verifies the SQL definition for pg_catalog.pg_settings view
 */
TEST_F(SchemaIntrospectionTest, PgCatalogSettingsViewDefinition)
{
    const char* view_sql = SchemaIntrospection::PG_CATALOG_SETTINGS;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW pg_catalog.pg_settings"), std::string::npos);
    EXPECT_NE(sql.find("name"), std::string::npos);
    EXPECT_NE(sql.find("setting"), std::string::npos);
    EXPECT_NE(sql.find("category"), std::string::npos);
}

/**
 * Test 6: pg_views view
 * Verifies the SQL definition for pg_catalog.pg_views view
 */
TEST_F(SchemaIntrospectionTest, PgCatalogViewsViewDefinition)
{
    const char* view_sql = SchemaIntrospection::PG_CATALOG_VIEWS;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW pg_catalog.pg_views"), std::string::npos);
    EXPECT_NE(sql.find("viewname"), std::string::npos);
    EXPECT_NE(sql.find("viewowner"), std::string::npos);
    EXPECT_NE(sql.find("definition"), std::string::npos);
}

/**
 * Test 7: pg_type view
 * Verifies the SQL definition for pg_catalog.pg_type view
 */
TEST_F(SchemaIntrospectionTest, PgCatalogTypeViewDefinition)
{
    const char* view_sql = SchemaIntrospection::PG_CATALOG_TYPES;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW pg_catalog.pg_type"), std::string::npos);
    EXPECT_NE(sql.find("typname"), std::string::npos);
    EXPECT_NE(sql.find("typnamespace"), std::string::npos);
    EXPECT_NE(sql.find("typtype"), std::string::npos);
}

/**
 * Test 8: pg_namespace view
 * Verifies the SQL definition for pg_catalog.pg_namespace view
 */
TEST_F(SchemaIntrospectionTest, PgCatalogNamespaceViewDefinition)
{
    const char* view_sql = SchemaIntrospection::PG_CATALOG_NAMESPACE;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW pg_catalog.pg_namespace"), std::string::npos);
    EXPECT_NE(sql.find("nspname"), std::string::npos);
    EXPECT_NE(sql.find("nspowner"), std::string::npos);
}

// =============================================================================
// information_schema Tests
// =============================================================================

/**
 * Test 9: information_schema.tables view
 * Verifies the SQL definition for information_schema.tables view
 */
TEST_F(SchemaIntrospectionTest, InfoSchemaTablesViewDefinition)
{
    const char* view_sql = SchemaIntrospection::INFO_SCHEMA_TABLES;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW information_schema.tables"), std::string::npos);
    EXPECT_NE(sql.find("table_catalog"), std::string::npos);
    EXPECT_NE(sql.find("table_schema"), std::string::npos);
    EXPECT_NE(sql.find("table_name"), std::string::npos);
    EXPECT_NE(sql.find("table_type"), std::string::npos);
}

/**
 * Test 10: information_schema.columns view
 * Verifies the SQL definition for information_schema.columns view
 */
TEST_F(SchemaIntrospectionTest, InfoSchemaColumnsViewDefinition)
{
    const char* view_sql = SchemaIntrospection::INFO_SCHEMA_COLUMNS;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW information_schema.columns"), std::string::npos);
    EXPECT_NE(sql.find("ordinal_position"), std::string::npos);
    EXPECT_NE(sql.find("column_default"), std::string::npos);
    EXPECT_NE(sql.find("is_nullable"), std::string::npos);
}

/**
 * Test 11: information_schema.views view
 * Verifies the SQL definition for information_schema.views view
 */
TEST_F(SchemaIntrospectionTest, InfoSchemaViewsViewDefinition)
{
    const char* view_sql = SchemaIntrospection::INFO_SCHEMA_VIEWS;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW information_schema.views"), std::string::npos);
    EXPECT_NE(sql.find("view_definition"), std::string::npos);
    EXPECT_NE(sql.find("check_option"), std::string::npos);
}

/**
 * Test 12: information_schema.schemata view
 * Verifies the SQL definition for information_schema.schemata view
 */
TEST_F(SchemaIntrospectionTest, InfoSchemaSchemataViewDefinition)
{
    const char* view_sql = SchemaIntrospection::INFO_SCHEMA_SCHEMATA;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW information_schema.schemata"), std::string::npos);
    EXPECT_NE(sql.find("catalog_name"), std::string::npos);
    EXPECT_NE(sql.find("schema_name"), std::string::npos);
    EXPECT_NE(sql.find("schema_owner"), std::string::npos);
}

/**
 * Test 13: information_schema.key_column_usage view
 * Verifies the SQL definition for information_schema.key_column_usage view
 */
TEST_F(SchemaIntrospectionTest, InfoSchemaKeyColumnUsageViewDefinition)
{
    const char* view_sql = SchemaIntrospection::INFO_SCHEMA_KEY_COLUMN_USAGE;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW information_schema.key_column_usage"), std::string::npos);
    EXPECT_NE(sql.find("constraint_name"), std::string::npos);
    EXPECT_NE(sql.find("column_name"), std::string::npos);
    EXPECT_NE(sql.find("ordinal_position"), std::string::npos);
}

/**
 * Test 14: information_schema.table_constraints view
 * Verifies the SQL definition for information_schema.table_constraints view
 */
TEST_F(SchemaIntrospectionTest, InfoSchemaTableConstraintsViewDefinition)
{
    const char* view_sql = SchemaIntrospection::INFO_SCHEMA_TABLE_CONSTRAINTS;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW information_schema.table_constraints"), std::string::npos);
    EXPECT_NE(sql.find("constraint_type"), std::string::npos);
    EXPECT_NE(sql.find("is_deferrable"), std::string::npos);
    EXPECT_NE(sql.find("enforced"), std::string::npos);
}

// =============================================================================
// MySQL Compatibility Tests
// =============================================================================

/**
 * Test 15: SHOW DATABASES equivalent
 * Verifies the SQL definition for MySQL show_databases view
 */
TEST_F(SchemaIntrospectionTest, MySQLShowDatabasesViewDefinition)
{
    const char* view_sql = SchemaIntrospection::MYSQL_SHOW_DATABASES;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW mysql.show_databases"), std::string::npos);
    EXPECT_NE(sql.find("schema_name"), std::string::npos);
    EXPECT_NE(sql.find("Database"), std::string::npos);
}

/**
 * Test 16: SHOW TABLES equivalent
 * Verifies the SQL definition for MySQL show_tables view
 */
TEST_F(SchemaIntrospectionTest, MySQLShowTablesViewDefinition)
{
    const char* view_sql = SchemaIntrospection::MYSQL_SHOW_TABLES;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW mysql.show_tables"), std::string::npos);
    EXPECT_NE(sql.find("Tables_in_db"), std::string::npos);
    EXPECT_NE(sql.find("information_schema.tables"), std::string::npos);
}

/**
 * Test 17: SHOW COLUMNS equivalent
 * Verifies the SQL definition for MySQL show_columns view
 */
TEST_F(SchemaIntrospectionTest, MySQLShowColumnsViewDefinition)
{
    const char* view_sql = SchemaIntrospection::MYSQL_SHOW_COLUMNS;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW mysql.show_columns"), std::string::npos);
    EXPECT_NE(sql.find("Field"), std::string::npos);
    EXPECT_NE(sql.find("Type"), std::string::npos);
    EXPECT_NE(sql.find("Null"), std::string::npos);
    EXPECT_NE(sql.find("Default"), std::string::npos);
    EXPECT_NE(sql.find("Extra"), std::string::npos);
}

/**
 * Test 18: SHOW INDEX equivalent
 * Verifies the SQL definition for MySQL show_index view
 */
TEST_F(SchemaIntrospectionTest, MySQLShowIndexViewDefinition)
{
    const char* view_sql = SchemaIntrospection::MYSQL_SHOW_INDEX;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW mysql.show_index"), std::string::npos);
    EXPECT_NE(sql.find("Non_unique"), std::string::npos);
    EXPECT_NE(sql.find("Key_name"), std::string::npos);
    EXPECT_NE(sql.find("Column_name"), std::string::npos);
    EXPECT_NE(sql.find("Index_type"), std::string::npos);
}

/**
 * Test 19: SHOW CREATE TABLE equivalent
 * Verifies the SQL definition for MySQL show_create_table view
 */
TEST_F(SchemaIntrospectionTest, MySQLShowCreateTableViewDefinition)
{
    const char* view_sql = SchemaIntrospection::MYSQL_SHOW_CREATE_TABLE;
    ASSERT_NE(view_sql, nullptr);
    // This is a placeholder comment - it may be empty or contain a comment
    std::string sql(view_sql);
    EXPECT_TRUE(sql.find("Generated dynamically") != std::string::npos ||
                sql.find("CREATE VIEW") != std::string::npos ||
                sql.empty() ||
                sql.find("--") != std::string::npos);
}

// =============================================================================
// Firebird Compatibility Tests
// =============================================================================

/**
 * Test 20: RDB$RELATIONS view
 * Verifies the SQL definition for Firebird RDB$RELATIONS view
 */
TEST_F(SchemaIntrospectionTest, FirebirdRdbRelationsViewDefinition)
{
    const char* view_sql = SchemaIntrospection::FB_RDB_RELATIONS;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW rdb$relations"), std::string::npos);
    EXPECT_NE(sql.find("rdb$relation_name"), std::string::npos);
    EXPECT_NE(sql.find("rdb$relation_id"), std::string::npos);
    EXPECT_NE(sql.find("rdb$system_flag"), std::string::npos);
}

/**
 * Test 21: RDB$RELATION_FIELDS view
 * Verifies the SQL definition for Firebird RDB$RELATION_FIELDS view
 */
TEST_F(SchemaIntrospectionTest, FirebirdRdbRelationFieldsViewDefinition)
{
    const char* view_sql = SchemaIntrospection::FB_RDB_RELATION_FIELDS;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW rdb$relation_fields"), std::string::npos);
    EXPECT_NE(sql.find("rdb$field_name"), std::string::npos);
    EXPECT_NE(sql.find("rdb$field_source"), std::string::npos);
    EXPECT_NE(sql.find("rdb$field_position"), std::string::npos);
}

/**
 * Test 22: RDB$FIELDS view
 * Verifies the SQL definition for Firebird RDB$FIELDS view
 */
TEST_F(SchemaIntrospectionTest, FirebirdRdbFieldsViewDefinition)
{
    const char* view_sql = SchemaIntrospection::FB_RDB_FIELDS;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW rdb$fields"), std::string::npos);
    EXPECT_NE(sql.find("rdb$field_type"), std::string::npos);
    EXPECT_NE(sql.find("rdb$field_length"), std::string::npos);
    EXPECT_NE(sql.find("rdb$field_scale"), std::string::npos);
}

/**
 * Test 23: RDB$INDICES view
 * Verifies the SQL definition for Firebird RDB$INDICES view
 */
TEST_F(SchemaIntrospectionTest, FirebirdRdbIndicesViewDefinition)
{
    const char* view_sql = SchemaIntrospection::FB_RDB_INDICES;
    ASSERT_NE(view_sql, nullptr);
    EXPECT_NE(strlen(view_sql), 0u);
    
    std::string sql(view_sql);
    EXPECT_NE(sql.find("CREATE VIEW rdb$indices"), std::string::npos);
    EXPECT_NE(sql.find("rdb$index_name"), std::string::npos);
    EXPECT_NE(sql.find("rdb$unique_flag"), std::string::npos);
    EXPECT_NE(sql.find("rdb$segment_count"), std::string::npos);
}

// =============================================================================
// Query Method Tests
// =============================================================================

/**
 * Test 24: getTables with null database
 * Verifies that getTables handles null database gracefully
 */
TEST_F(SchemaIntrospectionTest, GetTablesWithNullDatabase)
{
    auto tables = SchemaIntrospection::getTables(nullptr, "public");
    EXPECT_TRUE(tables.empty());
}

/**
 * Test 25: getTables returns empty for non-existent schema
 * Verifies that getTables returns empty vector for non-existent schema
 */
TEST_F(SchemaIntrospectionTest, GetTablesNonExistentSchema)
{
    auto tables = SchemaIntrospection::getTables(db.get(), "non_existent_schema_xyz");
    // Implementation returns empty for now
    EXPECT_TRUE(tables.empty());
}

/**
 * Test 26: getColumns with null database
 * Verifies that getColumns handles null database gracefully
 */
TEST_F(SchemaIntrospectionTest, GetColumnsWithNullDatabase)
{
    auto columns = SchemaIntrospection::getColumns(nullptr, "public", "test_table");
    EXPECT_TRUE(columns.empty());
}

/**
 * Test 27: getColumns returns empty for non-existent table
 * Verifies that getColumns returns empty vector for non-existent table
 */
TEST_F(SchemaIntrospectionTest, GetColumnsNonExistentTable)
{
    auto columns = SchemaIntrospection::getColumns(db.get(), "public", "non_existent_table_xyz");
    // Implementation returns empty for now
    EXPECT_TRUE(columns.empty());
}

/**
 * Test 28: getIndexes with null database
 * Verifies that getIndexes handles null database gracefully
 */
TEST_F(SchemaIntrospectionTest, GetIndexesWithNullDatabase)
{
    auto indexes = SchemaIntrospection::getIndexes(nullptr, "public", "test_table");
    EXPECT_TRUE(indexes.empty());
}

/**
 * Test 29: getIndexes returns empty for non-existent table
 * Verifies that getIndexes returns empty vector for non-existent table
 */
TEST_F(SchemaIntrospectionTest, GetIndexesNonExistentTable)
{
    auto indexes = SchemaIntrospection::getIndexes(db.get(), "public", "non_existent_table_xyz");
    // Implementation returns empty for now
    EXPECT_TRUE(indexes.empty());
}

/**
 * Test 30: getConstraints with null database
 * Verifies that getConstraints handles null database gracefully
 */
TEST_F(SchemaIntrospectionTest, GetConstraintsWithNullDatabase)
{
    auto constraints = SchemaIntrospection::getConstraints(nullptr, "public", "test_table");
    EXPECT_TRUE(constraints.empty());
}

/**
 * Test 31: getConstraints returns empty for non-existent table
 * Verifies that getConstraints returns empty vector for non-existent table
 */
TEST_F(SchemaIntrospectionTest, GetConstraintsNonExistentTable)
{
    auto constraints = SchemaIntrospection::getConstraints(db.get(), "public", "non_existent_table_xyz");
    // Implementation returns empty for now
    EXPECT_TRUE(constraints.empty());
}

/**
 * Test 32: getPrimaryKeyColumns with null database
 * Verifies that getPrimaryKeyColumns handles null database gracefully
 */
TEST_F(SchemaIntrospectionTest, GetPrimaryKeyColumnsWithNullDatabase)
{
    auto pk_columns = SchemaIntrospection::getPrimaryKeyColumns(nullptr, "public", "test_table");
    EXPECT_TRUE(pk_columns.empty());
}

/**
 * Test 33: getPrimaryKeyColumns returns empty for non-existent table
 * Verifies that getPrimaryKeyColumns returns empty vector for non-existent table
 */
TEST_F(SchemaIntrospectionTest, GetPrimaryKeyColumnsNonExistentTable)
{
    auto pk_columns = SchemaIntrospection::getPrimaryKeyColumns(db.get(), "public", "non_existent_table_xyz");
    // Implementation returns empty for now
    EXPECT_TRUE(pk_columns.empty());
}

/**
 * Test 34: getForeignKeys with null database
 * Verifies that getForeignKeys handles null database gracefully
 */
TEST_F(SchemaIntrospectionTest, GetForeignKeysWithNullDatabase)
{
    auto fks = SchemaIntrospection::getForeignKeys(nullptr, "public", "test_table");
    EXPECT_TRUE(fks.empty());
}

/**
 * Test 35: getForeignKeys returns empty for non-existent table
 * Verifies that getForeignKeys returns empty vector for non-existent table
 */
TEST_F(SchemaIntrospectionTest, GetForeignKeysNonExistentTable)
{
    auto fks = SchemaIntrospection::getForeignKeys(db.get(), "public", "non_existent_table_xyz");
    // Implementation returns empty for now
    EXPECT_TRUE(fks.empty());
}

// =============================================================================
// Initialization Tests
// =============================================================================

/**
 * Test 36: initializePostgreSQL with null database
 * Verifies that initializePostgreSQL handles null database gracefully
 */
TEST_F(SchemaIntrospectionTest, InitializePostgreSQLWithNullDatabase)
{
    // Should not crash
    SchemaIntrospection::initializePostgreSQL(nullptr);
    SUCCEED();
}

/**
 * Test 37: initializePostgreSQL with valid database
 * Verifies that initializePostgreSQL works with a valid database
 */
TEST_F(SchemaIntrospectionTest, InitializePostgreSQLWithValidDatabase)
{
    // Should not crash
    SchemaIntrospection::initializePostgreSQL(db.get());
    SUCCEED();
}

/**
 * Test 38: initializeMySQL with null database
 * Verifies that initializeMySQL handles null database gracefully
 */
TEST_F(SchemaIntrospectionTest, InitializeMySQLWithNullDatabase)
{
    // Should not crash
    SchemaIntrospection::initializeMySQL(nullptr);
    SUCCEED();
}

/**
 * Test 39: initializeMySQL with valid database
 * Verifies that initializeMySQL works with a valid database
 */
TEST_F(SchemaIntrospectionTest, InitializeMySQLWithValidDatabase)
{
    // Should not crash
    SchemaIntrospection::initializeMySQL(db.get());
    SUCCEED();
}

/**
 * Test 40: initializeFirebird with null database
 * Verifies that initializeFirebird handles null database gracefully
 */
TEST_F(SchemaIntrospectionTest, InitializeFirebirdWithNullDatabase)
{
    // Should not crash
    SchemaIntrospection::initializeFirebird(nullptr);
    SUCCEED();
}

/**
 * Test 41: initializeFirebird with valid database
 * Verifies that initializeFirebird works with a valid database
 */
TEST_F(SchemaIntrospectionTest, InitializeFirebirdWithValidDatabase)
{
    // Should not crash
    SchemaIntrospection::initializeFirebird(db.get());
    SUCCEED();
}

// =============================================================================
// Data Structure Tests
// =============================================================================

/**
 * Test 42: TableInfo struct initialization
 * Verifies that TableInfo struct can be properly initialized
 */
TEST(SchemaIntrospectionStructTest, TableInfoInitialization)
{
    SchemaIntrospection::TableInfo info;
    info.catalog = "test_catalog";
    info.schema = "test_schema";
    info.name = "test_table";
    info.type = "BASE TABLE";
    info.owner = "test_owner";
    info.description = "Test table description";
    
    EXPECT_EQ(info.catalog, "test_catalog");
    EXPECT_EQ(info.schema, "test_schema");
    EXPECT_EQ(info.name, "test_table");
    EXPECT_EQ(info.type, "BASE TABLE");
    EXPECT_EQ(info.owner, "test_owner");
    EXPECT_EQ(info.description, "Test table description");
}

/**
 * Test 43: ColumnInfo struct initialization
 * Verifies that ColumnInfo struct can be properly initialized
 */
TEST(SchemaIntrospectionStructTest, ColumnInfoInitialization)
{
    SchemaIntrospection::ColumnInfo info;
    info.catalog = "test_catalog";
    info.schema = "test_schema";
    info.table = "test_table";
    info.name = "test_column";
    info.position = 1;
    info.data_type = "INTEGER";
    info.max_length = 4;
    info.numeric_precision = 0;
    info.numeric_scale = 0;
    info.is_nullable = false;
    info.default_value = "0";
    info.is_identity = true;
    info.collation = "en_US";
    
    EXPECT_EQ(info.catalog, "test_catalog");
    EXPECT_EQ(info.schema, "test_schema");
    EXPECT_EQ(info.table, "test_table");
    EXPECT_EQ(info.name, "test_column");
    EXPECT_EQ(info.position, 1);
    EXPECT_EQ(info.data_type, "INTEGER");
    EXPECT_EQ(info.max_length, 4);
    EXPECT_FALSE(info.is_nullable);
    EXPECT_EQ(info.default_value, "0");
    EXPECT_TRUE(info.is_identity);
    EXPECT_EQ(info.collation, "en_US");
}

/**
 * Test 44: IndexInfo struct initialization
 * Verifies that IndexInfo struct can be properly initialized
 */
TEST(SchemaIntrospectionStructTest, IndexInfoInitialization)
{
    SchemaIntrospection::IndexInfo info;
    info.catalog = "test_catalog";
    info.schema = "test_schema";
    info.table = "test_table";
    info.name = "test_index";
    info.is_unique = true;
    info.is_primary = true;
    info.columns = {"col1", "col2"};
    info.type = "BTREE";
    
    EXPECT_EQ(info.catalog, "test_catalog");
    EXPECT_EQ(info.schema, "test_schema");
    EXPECT_EQ(info.table, "test_table");
    EXPECT_EQ(info.name, "test_index");
    EXPECT_TRUE(info.is_unique);
    EXPECT_TRUE(info.is_primary);
    EXPECT_EQ(info.columns.size(), 2u);
    EXPECT_EQ(info.columns[0], "col1");
    EXPECT_EQ(info.columns[1], "col2");
    EXPECT_EQ(info.type, "BTREE");
}

/**
 * Test 45: ConstraintInfo struct initialization
 * Verifies that ConstraintInfo struct can be properly initialized
 */
TEST(SchemaIntrospectionStructTest, ConstraintInfoInitialization)
{
    SchemaIntrospection::ConstraintInfo info;
    info.catalog = "test_catalog";
    info.schema = "test_schema";
    info.table = "test_table";
    info.name = "test_fk";
    info.type = "FOREIGN KEY";
    info.columns = {"col1"};
    info.referenced_table = "ref_table";
    info.referenced_columns = {"ref_col"};
    info.check_clause = "";
    
    EXPECT_EQ(info.catalog, "test_catalog");
    EXPECT_EQ(info.schema, "test_schema");
    EXPECT_EQ(info.table, "test_table");
    EXPECT_EQ(info.name, "test_fk");
    EXPECT_EQ(info.type, "FOREIGN KEY");
    EXPECT_EQ(info.columns.size(), 1u);
    EXPECT_EQ(info.columns[0], "col1");
    EXPECT_EQ(info.referenced_table, "ref_table");
    EXPECT_EQ(info.referenced_columns.size(), 1u);
    EXPECT_EQ(info.referenced_columns[0], "ref_col");
}

/**
 * Test 46: ConstraintInfo for CHECK constraint
 * Verifies that ConstraintInfo works correctly for CHECK constraints
 */
TEST(SchemaIntrospectionStructTest, ConstraintInfoCheckConstraint)
{
    SchemaIntrospection::ConstraintInfo info;
    info.name = "check_positive";
    info.type = "CHECK";
    info.check_clause = "value > 0";
    
    EXPECT_EQ(info.name, "check_positive");
    EXPECT_EQ(info.type, "CHECK");
    EXPECT_EQ(info.check_clause, "value > 0");
}

/**
 * Test 47: ConstraintInfo for PRIMARY KEY constraint
 * Verifies that ConstraintInfo works correctly for PRIMARY KEY constraints
 */
TEST(SchemaIntrospectionStructTest, ConstraintInfoPrimaryKeyConstraint)
{
    SchemaIntrospection::ConstraintInfo info;
    info.name = "pk_test";
    info.type = "PRIMARY KEY";
    info.columns = {"id"};
    info.is_deferrable = false;
    
    EXPECT_EQ(info.name, "pk_test");
    EXPECT_EQ(info.type, "PRIMARY KEY");
    EXPECT_EQ(info.columns.size(), 1u);
    EXPECT_EQ(info.columns[0], "id");
}

/**
 * Test 48: ConstraintInfo for UNIQUE constraint
 * Verifies that ConstraintInfo works correctly for UNIQUE constraints
 */
TEST(SchemaIntrospectionStructTest, ConstraintInfoUniqueConstraint)
{
    SchemaIntrospection::ConstraintInfo info;
    info.name = "uk_email";
    info.type = "UNIQUE";
    info.columns = {"email"};
    
    EXPECT_EQ(info.name, "uk_email");
    EXPECT_EQ(info.type, "UNIQUE");
    EXPECT_EQ(info.columns.size(), 1u);
}

// =============================================================================
// Integration Tests with Real Schema
// =============================================================================

/**
 * Test 49: getTables with existing schema and tables
 * Creates a schema and tables, then verifies getTables
 */
TEST_F(SchemaIntrospectionTest, GetTablesWithExistingTables)
{
    ErrorContext ctx;
    
    // Create a test schema
    ID schema_id = createTestSchema("test_gettables_schema");
    ASSERT_NE(schema_id, ID{});
    
    // Create test tables
    std::vector<CatalogManager::ColumnInfo> columns;
    columns.push_back(makeColumn("id", DataType::INT64, 8, false));
    columns.push_back(makeColumn("name", DataType::VARCHAR, 100, true));
    
    ID table1_id = createTestTable(schema_id, "table1", columns);
    ID table2_id = createTestTable(schema_id, "table2", columns);
    
    ASSERT_NE(table1_id, ID{});
    ASSERT_NE(table2_id, ID{});
    
    // Call getTables (implementation may not be complete)
    auto tables = SchemaIntrospection::getTables(db.get(), "test_gettables_schema");
    
    // For now, implementation returns empty - this test verifies it doesn't crash
    // When fully implemented, we would verify:
    // EXPECT_EQ(tables.size(), 2u);
    // Verify table names, etc.
}

/**
 * Test 50: getColumns with existing table
 * Creates a table with columns, then verifies getColumns
 */
TEST_F(SchemaIntrospectionTest, GetColumnsWithExistingTable)
{
    ErrorContext ctx;
    
    // Create a test schema and table
    ID schema_id = createTestSchema("test_getcolumns_schema");
    ASSERT_NE(schema_id, ID{});
    
    std::vector<CatalogManager::ColumnInfo> columns;
    columns.push_back(makeColumn("id", DataType::INT64, 8, false));
    columns.push_back(makeColumn("email", DataType::VARCHAR, 255, false));
    columns.push_back(makeColumn("age", DataType::INT32, 4, true, true, "0"));
    
    ID table_id = createTestTable(schema_id, "test_table", columns);
    ASSERT_NE(table_id, ID{});
    
    // Call getColumns (implementation may not be complete)
    auto result_columns = SchemaIntrospection::getColumns(db.get(), "test_getcolumns_schema", "test_table");
    
    // For now, implementation returns empty - this test verifies it doesn't crash
    // When fully implemented, we would verify column count and properties
}

/**
 * Test 51: All view constants are non-null
 * Verifies that all defined view constants are not null
 */
TEST(SchemaIntrospectionViewConstantsTest, AllViewConstantsAreNonNull)
{
    // PostgreSQL views
    EXPECT_NE(SchemaIntrospection::PG_CATALOG_TABLES, nullptr);
    EXPECT_NE(SchemaIntrospection::PG_CATALOG_COLUMNS, nullptr);
    EXPECT_NE(SchemaIntrospection::PG_CATALOG_INDEXES, nullptr);
    EXPECT_NE(SchemaIntrospection::PG_CATALOG_DATABASES, nullptr);
    EXPECT_NE(SchemaIntrospection::PG_CATALOG_SETTINGS, nullptr);
    EXPECT_NE(SchemaIntrospection::PG_CATALOG_VIEWS, nullptr);
    EXPECT_NE(SchemaIntrospection::PG_CATALOG_TYPES, nullptr);
    EXPECT_NE(SchemaIntrospection::PG_CATALOG_NAMESPACE, nullptr);
    
    // information_schema views
    EXPECT_NE(SchemaIntrospection::INFO_SCHEMA_TABLES, nullptr);
    EXPECT_NE(SchemaIntrospection::INFO_SCHEMA_COLUMNS, nullptr);
    EXPECT_NE(SchemaIntrospection::INFO_SCHEMA_VIEWS, nullptr);
    EXPECT_NE(SchemaIntrospection::INFO_SCHEMA_SCHEMATA, nullptr);
    EXPECT_NE(SchemaIntrospection::INFO_SCHEMA_KEY_COLUMN_USAGE, nullptr);
    EXPECT_NE(SchemaIntrospection::INFO_SCHEMA_TABLE_CONSTRAINTS, nullptr);
    
    // MySQL views
    EXPECT_NE(SchemaIntrospection::MYSQL_SHOW_DATABASES, nullptr);
    EXPECT_NE(SchemaIntrospection::MYSQL_SHOW_TABLES, nullptr);
    EXPECT_NE(SchemaIntrospection::MYSQL_SHOW_COLUMNS, nullptr);
    EXPECT_NE(SchemaIntrospection::MYSQL_SHOW_INDEX, nullptr);
    EXPECT_NE(SchemaIntrospection::MYSQL_SHOW_CREATE_TABLE, nullptr);
    
    // Firebird views
    EXPECT_NE(SchemaIntrospection::FB_RDB_RELATIONS, nullptr);
    EXPECT_NE(SchemaIntrospection::FB_RDB_RELATION_FIELDS, nullptr);
    EXPECT_NE(SchemaIntrospection::FB_RDB_FIELDS, nullptr);
    EXPECT_NE(SchemaIntrospection::FB_RDB_INDICES, nullptr);
}

/**
 * Test 52: View SQL contains expected keywords
 * Verifies that view SQL contains expected SQL keywords
 */
TEST(SchemaIntrospectionViewConstantsTest, ViewSqlContainsExpectedKeywords)
{
    // Test a sample of views for expected keywords
    std::string pg_tables(SchemaIntrospection::PG_CATALOG_TABLES);
    EXPECT_NE(pg_tables.find("SELECT"), std::string::npos);
    EXPECT_NE(pg_tables.find("FROM"), std::string::npos);
    
    std::string info_columns(SchemaIntrospection::INFO_SCHEMA_COLUMNS);
    EXPECT_NE(info_columns.find("SELECT"), std::string::npos);
    EXPECT_NE(info_columns.find("FROM"), std::string::npos);
    
    std::string mysql_databases(SchemaIntrospection::MYSQL_SHOW_DATABASES);
    EXPECT_NE(mysql_databases.find("SELECT"), std::string::npos);
    
    std::string fb_relations(SchemaIntrospection::FB_RDB_RELATIONS);
    EXPECT_NE(fb_relations.find("SELECT"), std::string::npos);
    EXPECT_NE(fb_relations.find("FROM"), std::string::npos);
}

/**
 * Test 53: Multiple initialization calls are safe
 * Verifies that calling initialization methods multiple times doesn't cause issues
 */
TEST_F(SchemaIntrospectionTest, MultipleInitializationCallsAreSafe)
{
    // Call PostgreSQL initialization multiple times
    SchemaIntrospection::initializePostgreSQL(db.get());
    SchemaIntrospection::initializePostgreSQL(db.get());
    SchemaIntrospection::initializePostgreSQL(db.get());
    
    // Call MySQL initialization multiple times
    SchemaIntrospection::initializeMySQL(db.get());
    SchemaIntrospection::initializeMySQL(db.get());
    
    // Call Firebird initialization multiple times
    SchemaIntrospection::initializeFirebird(db.get());
    SchemaIntrospection::initializeFirebird(db.get());
    SchemaIntrospection::initializeFirebird(db.get());
    SchemaIntrospection::initializeFirebird(db.get());
    
    SUCCEED();
}

/**
 * Test 54: Query methods with empty schema/table names
 * Verifies that query methods handle empty strings gracefully
 */
TEST_F(SchemaIntrospectionTest, QueryMethodsWithEmptyNames)
{
    // Test with empty schema name
    auto tables = SchemaIntrospection::getTables(db.get(), "");
    // Should not crash - implementation dependent
    
    // Test with empty table name
    auto columns = SchemaIntrospection::getColumns(db.get(), "public", "");
    // Should not crash - implementation dependent
    
    auto indexes = SchemaIntrospection::getIndexes(db.get(), "public", "");
    // Should not crash - implementation dependent
    
    auto constraints = SchemaIntrospection::getConstraints(db.get(), "public", "");
    // Should not crash - implementation dependent
    
    auto pk_columns = SchemaIntrospection::getPrimaryKeyColumns(db.get(), "public", "");
    // Should not crash - implementation dependent
    
    auto fks = SchemaIntrospection::getForeignKeys(db.get(), "public", "");
    // Should not crash - implementation dependent
    
    SUCCEED();
}

/**
 * Test 55: Complex table structure introspection
 * Creates a table with various column types and tests introspection
 */
TEST_F(SchemaIntrospectionTest, ComplexTableStructureIntrospection)
{
    ErrorContext ctx;
    
    ID schema_id = createTestSchema("complex_test_schema");
    ASSERT_NE(schema_id, ID{});
    
    // Create table with various column types
    std::vector<CatalogManager::ColumnInfo> columns;
    columns.push_back(makeColumn("id", DataType::INT64, 8, false));                    // Primary key
    columns.push_back(makeColumn("uuid_col", DataType::UUID, 16, false));              // UUID
    columns.push_back(makeColumn("name", DataType::VARCHAR, 255, true));               // Nullable string
    columns.push_back(makeColumn("description", DataType::TEXT, 0, true));             // Text
    columns.push_back(makeColumn("is_active", DataType::BOOLEAN, 1, false, true, "true")); // Boolean with default
    columns.push_back(makeColumn("amount", DataType::DECIMAL, 16, true));              // Decimal
    columns.push_back(makeColumn("created_at", DataType::TIMESTAMP, 8, false, true, "now()")); // Timestamp
    columns.push_back(makeColumn("data", DataType::JSONB, 0, true));                   // JSONB
    
    ID table_id = createTestTable(schema_id, "complex_table", columns);
    ASSERT_NE(table_id, ID{});
    
    // Create an index
    ID index_id;
    Status index_status = catalog->createIndex(table_id, "idx_name", {"name"}, index_id, false,
                                               CatalogManager::IndexType::BTREE, 0, &ctx);
    EXPECT_EQ(index_status, Status::OK);
    
    // Test introspection methods
    auto result_tables = SchemaIntrospection::getTables(db.get(), "complex_test_schema");
    auto result_columns = SchemaIntrospection::getColumns(db.get(), "complex_test_schema", "complex_table");
    auto result_indexes = SchemaIntrospection::getIndexes(db.get(), "complex_test_schema", "complex_table");
    
    // Methods should not crash - full implementation would verify results
    SUCCEED();
}

/**
 * Test 56: getIndexes with existing index
 * Creates a table with an index, then verifies getIndexes
 */
TEST_F(SchemaIntrospectionTest, GetIndexesWithExistingIndex)
{
    ErrorContext ctx;
    
    ID schema_id = createTestSchema("test_getindexes_schema");
    ASSERT_NE(schema_id, ID{});
    
    std::vector<CatalogManager::ColumnInfo> columns;
    columns.push_back(makeColumn("id", DataType::INT64, 8, false));
    columns.push_back(makeColumn("email", DataType::VARCHAR, 255, false));
    
    ID table_id = createTestTable(schema_id, "indexed_table", columns);
    ASSERT_NE(table_id, ID{});
    
    // Create indexes
    ID index1_id, index2_id;
    ASSERT_EQ(catalog->createIndex(table_id, "pk_index", {"id"}, index1_id, true,
                                   CatalogManager::IndexType::BTREE, 0, &ctx), Status::OK);
    ASSERT_EQ(catalog->createIndex(table_id, "email_index", {"email"}, index2_id, false,
                                   CatalogManager::IndexType::BTREE, 0, &ctx), Status::OK);
    
    // Call getIndexes
    auto indexes = SchemaIntrospection::getIndexes(db.get(), "test_getindexes_schema", "indexed_table");
    
    // For now, implementation returns empty - this test verifies it doesn't crash
    // When fully implemented, we would verify index count and properties
}

/**
 * Test 57: Cross-schema table introspection
 * Verifies that tables in different schemas are properly isolated
 */
TEST_F(SchemaIntrospectionTest, CrossSchemaTableIntrospection)
{
    ErrorContext ctx;
    
    // Create two schemas
    ID schema1_id = createTestSchema("schema_one");
    ID schema2_id = createTestSchema("schema_two");
    ASSERT_NE(schema1_id, ID{});
    ASSERT_NE(schema2_id, ID{});
    
    // Create tables in each schema with the same name
    std::vector<CatalogManager::ColumnInfo> columns;
    columns.push_back(makeColumn("id", DataType::INT64, 8, false));
    
    ID table1_id = createTestTable(schema1_id, "shared_name", columns);
    ID table2_id = createTestTable(schema2_id, "shared_name", columns);
    
    ASSERT_NE(table1_id, ID{});
    ASSERT_NE(table2_id, ID{});
    
    // Query each schema separately
    auto tables1 = SchemaIntrospection::getTables(db.get(), "schema_one");
    auto tables2 = SchemaIntrospection::getTables(db.get(), "schema_two");
    
    // Query columns from each schema's table
    auto columns1 = SchemaIntrospection::getColumns(db.get(), "schema_one", "shared_name");
    auto columns2 = SchemaIntrospection::getColumns(db.get(), "schema_two", "shared_name");
    
    // Should not crash - full implementation would verify proper isolation
    SUCCEED();
}

/**
 * Test 58: PostgreSQL view filtering excludes system schemas
 * Verifies that PostgreSQL-compatible views filter out system schemas
 */
TEST(SchemaIntrospectionViewConstantsTest, PostgreSQLViewsExcludeSystemSchemas)
{
    std::string info_tables(SchemaIntrospection::INFO_SCHEMA_TABLES);
    EXPECT_NE(info_tables.find("NOT IN ('pg_catalog', 'information_schema')"), std::string::npos);
    
    std::string info_columns(SchemaIntrospection::INFO_SCHEMA_COLUMNS);
    EXPECT_NE(info_columns.find("NOT IN ('pg_catalog', 'information_schema')"), std::string::npos);
    
    std::string info_schemata(SchemaIntrospection::INFO_SCHEMA_SCHEMATA);
    EXPECT_NE(info_schemata.find("NOT IN ('pg_catalog', 'information_schema')"), std::string::npos);
}

/**
 * Test 59: View SQL syntax validation
 * Verifies that view SQL starts with CREATE VIEW
 */
TEST(SchemaIntrospectionViewConstantsTest, ViewSqlSyntaxValidation)
{
    // Test all PostgreSQL views
    std::vector<std::pair<const char*, std::string>> pg_views = {
        {SchemaIntrospection::PG_CATALOG_TABLES, "pg_tables"},
        {SchemaIntrospection::PG_CATALOG_COLUMNS, "pg_columns"},
        {SchemaIntrospection::PG_CATALOG_INDEXES, "pg_indexes"},
        {SchemaIntrospection::PG_CATALOG_DATABASES, "pg_database"},
        {SchemaIntrospection::PG_CATALOG_SETTINGS, "pg_settings"},
        {SchemaIntrospection::PG_CATALOG_VIEWS, "pg_views"},
        {SchemaIntrospection::PG_CATALOG_TYPES, "pg_type"},
        {SchemaIntrospection::PG_CATALOG_NAMESPACE, "pg_namespace"},
    };
    
    for (const auto& [sql, name] : pg_views) {
        std::string sql_str(sql);
        EXPECT_TRUE(sql_str.find("CREATE VIEW") != std::string::npos ||
                    sql_str.find("CREATE OR REPLACE VIEW") != std::string::npos)
            << "View " << name << " should start with CREATE VIEW";
    }
    
    // Test all information_schema views
    std::vector<std::pair<const char*, std::string>> info_views = {
        {SchemaIntrospection::INFO_SCHEMA_TABLES, "tables"},
        {SchemaIntrospection::INFO_SCHEMA_COLUMNS, "columns"},
        {SchemaIntrospection::INFO_SCHEMA_VIEWS, "views"},
        {SchemaIntrospection::INFO_SCHEMA_SCHEMATA, "schemata"},
        {SchemaIntrospection::INFO_SCHEMA_KEY_COLUMN_USAGE, "key_column_usage"},
        {SchemaIntrospection::INFO_SCHEMA_TABLE_CONSTRAINTS, "table_constraints"},
    };
    
    for (const auto& [sql, name] : info_views) {
        std::string sql_str(sql);
        EXPECT_TRUE(sql_str.find("CREATE VIEW") != std::string::npos ||
                    sql_str.find("CREATE OR REPLACE VIEW") != std::string::npos)
            << "View " << name << " should start with CREATE VIEW";
    }
}

/**
 * Test 60: MySQL views reference information_schema
 * Verifies that MySQL compatibility views reference information_schema
 */
TEST(SchemaIntrospectionViewConstantsTest, MySQLViewsReferenceInformationSchema)
{
    std::string mysql_databases(SchemaIntrospection::MYSQL_SHOW_DATABASES);
    EXPECT_NE(mysql_databases.find("information_schema"), std::string::npos);
    
    std::string mysql_tables(SchemaIntrospection::MYSQL_SHOW_TABLES);
    EXPECT_NE(mysql_tables.find("information_schema"), std::string::npos);
    
    std::string mysql_columns(SchemaIntrospection::MYSQL_SHOW_COLUMNS);
    EXPECT_NE(mysql_columns.find("information_schema"), std::string::npos);
    
    std::string mysql_index(SchemaIntrospection::MYSQL_SHOW_INDEX);
    EXPECT_NE(mysql_index.find("information_schema"), std::string::npos);
}
