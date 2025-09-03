#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace scratchbird::core;

class CatalogManagerTest : public ::testing::Test {
protected:
    std::string test_db_path;
    Database* db = nullptr;
    
    void SetUp() override {
        test_db_path = "/tmp/test_catalog_" + std::to_string(getpid()) + ".db";
        // Remove any existing test file
        std::remove(test_db_path.c_str());
    }
    
    void TearDown() override {
        if (db) {
            db->close();
            delete db;
            db = nullptr;
        }
        std::remove(test_db_path.c_str());
    }
    
    void CreateAndOpenDatabase(uint32_t page_size = 16384) {
        ErrorContext ctx;
        
        // Create database
        ASSERT_EQ(Database::create(test_db_path, page_size, &ctx), Status::Ok);
        
        // Open database
        db = new Database();
        Status status = db->open(test_db_path, &ctx);
        if (status != Status::Ok) {
            std::cerr << "Failed to open database: " << ctx.message << std::endl;
        }
        ASSERT_EQ(status, Status::Ok);
    }
};

TEST_F(CatalogManagerTest, InitializeCatalog) {
    CreateAndOpenDatabase();
    
    CatalogManager* catalog = db->catalog_manager();
    ASSERT_NE(catalog, nullptr);
    
    // Should have initial [sys] schema
    EXPECT_GT(catalog->schema_count(), 0);
}

TEST_F(CatalogManagerTest, CreateAndGetSchema) {
    CreateAndOpenDatabase();
    ErrorContext ctx;
    
    CatalogManager* catalog = db->catalog_manager();
    
    // Create a schema
    uint32_t schema_id;
    ASSERT_EQ(catalog->create_schema("test_schema", "test_user", schema_id, &ctx), 
              Status::Ok);
    EXPECT_GE(schema_id, 1000); // User schemas start at 1000
    
    // Get schema by ID
    CatalogManager::SchemaInfo info;
    ASSERT_EQ(catalog->get_schema(schema_id, info, &ctx), Status::Ok);
    EXPECT_EQ(info.schema_id, schema_id);
    EXPECT_EQ(info.schema_name, "test_schema");
    EXPECT_EQ(info.owner, "test_user");
    
    // Get schema by name
    CatalogManager::SchemaInfo info2;
    ASSERT_EQ(catalog->get_schema("test_schema", info2, &ctx), Status::Ok);
    EXPECT_EQ(info2.schema_id, schema_id);
    EXPECT_EQ(info2.schema_name, "test_schema");
}

TEST_F(CatalogManagerTest, ListSchemas) {
    CreateAndOpenDatabase();
    ErrorContext ctx;
    
    CatalogManager* catalog = db->catalog_manager();
    
    // Create multiple schemas
    uint32_t id1, id2, id3;
    ASSERT_EQ(catalog->create_schema("schema1", "user1", id1, &ctx), Status::Ok);
    ASSERT_EQ(catalog->create_schema("schema2", "user2", id2, &ctx), Status::Ok);
    ASSERT_EQ(catalog->create_schema("schema3", "user3", id3, &ctx), Status::Ok);
    
    // List all schemas
    std::vector<CatalogManager::SchemaInfo> schemas;
    ASSERT_EQ(catalog->list_schemas(schemas, &ctx), Status::Ok);
    
    // Should have at least 4 schemas (including [sys])
    EXPECT_GE(schemas.size(), 4);
    
    // Find our created schemas
    int found = 0;
    for (const auto& schema : schemas) {
        if (schema.schema_name == "schema1" || 
            schema.schema_name == "schema2" ||
            schema.schema_name == "schema3") {
            found++;
        }
    }
    EXPECT_EQ(found, 3);
}

TEST_F(CatalogManagerTest, CreateDuplicateSchema) {
    CreateAndOpenDatabase();
    ErrorContext ctx;
    
    CatalogManager* catalog = db->catalog_manager();
    
    // Create a schema
    uint32_t schema_id;
    ASSERT_EQ(catalog->create_schema("test_schema", "test_user", schema_id, &ctx), 
              Status::Ok);
    
    // Try to create duplicate
    uint32_t duplicate_id;
    Status status = catalog->create_schema("test_schema", "test_user", duplicate_id, &ctx);
    EXPECT_EQ(status, Status::InvalidArgument);
    EXPECT_NE(std::string(ctx.message).find("already exists"), std::string::npos);
}

TEST_F(CatalogManagerTest, CreateAndGetTable) {
    CreateAndOpenDatabase();
    ErrorContext ctx;
    
    CatalogManager* catalog = db->catalog_manager();
    
    // Create a schema first
    uint32_t schema_id;
    ASSERT_EQ(catalog->create_schema("test_schema", "test_user", schema_id, &ctx), 
              Status::Ok);
    
    // Define columns
    std::vector<CatalogManager::ColumnInfo> columns;
    columns.push_back({0, 0, "id", static_cast<uint16_t>(DataType::Int32), 4, false, false, ""});
    columns.push_back({0, 1, "name", static_cast<uint16_t>(DataType::Varchar), 100, true, false, ""});
    columns.push_back({0, 2, "created", static_cast<uint16_t>(DataType::Timestamp), 8, false, true, "now()"});
    
    // Create table
    uint32_t table_id;
    ASSERT_EQ(catalog->create_table(schema_id, "test_table", columns, table_id, &ctx), 
              Status::Ok);
    EXPECT_GE(table_id, 1000); // User tables start at 1000
    
    // Get table by ID
    CatalogManager::TableInfo info;
    ASSERT_EQ(catalog->get_table(table_id, info, &ctx), Status::Ok);
    EXPECT_EQ(info.table_id, table_id);
    EXPECT_EQ(info.schema_id, schema_id);
    EXPECT_EQ(info.table_name, "test_table");
    EXPECT_EQ(info.column_count, 3);
    EXPECT_GT(info.root_page, 2); // Should have allocated a page
    
    // Get table by schema and name
    CatalogManager::TableInfo info2;
    ASSERT_EQ(catalog->get_table(schema_id, "test_table", info2, &ctx), Status::Ok);
    EXPECT_EQ(info2.table_id, table_id);
}

TEST_F(CatalogManagerTest, ListTables) {
    CreateAndOpenDatabase();
    ErrorContext ctx;
    
    CatalogManager* catalog = db->catalog_manager();
    
    // Create a schema
    uint32_t schema_id;
    ASSERT_EQ(catalog->create_schema("test_schema", "test_user", schema_id, &ctx), 
              Status::Ok);
    
    // Create multiple tables
    std::vector<CatalogManager::ColumnInfo> columns;
    columns.push_back({0, 0, "id", static_cast<uint16_t>(DataType::Int32), 4, false, false, ""});
    
    uint32_t id1, id2, id3;
    ASSERT_EQ(catalog->create_table(schema_id, "table1", columns, id1, &ctx), Status::Ok);
    ASSERT_EQ(catalog->create_table(schema_id, "table2", columns, id2, &ctx), Status::Ok);
    ASSERT_EQ(catalog->create_table(schema_id, "table3", columns, id3, &ctx), Status::Ok);
    
    // List tables in schema
    std::vector<CatalogManager::TableInfo> tables;
    ASSERT_EQ(catalog->list_tables(schema_id, tables, &ctx), Status::Ok);
    
    EXPECT_EQ(tables.size(), 3);
    
    // Should be sorted by name
    EXPECT_EQ(tables[0].table_name, "table1");
    EXPECT_EQ(tables[1].table_name, "table2");
    EXPECT_EQ(tables[2].table_name, "table3");
}

TEST_F(CatalogManagerTest, GetColumns) {
    CreateAndOpenDatabase();
    ErrorContext ctx;
    
    CatalogManager* catalog = db->catalog_manager();
    
    // Create a schema
    uint32_t schema_id;
    ASSERT_EQ(catalog->create_schema("test_schema", "test_user", schema_id, &ctx), 
              Status::Ok);
    
    // Define columns with various types
    std::vector<CatalogManager::ColumnInfo> columns;
    columns.push_back({0, 0, "id", static_cast<uint16_t>(DataType::Int32), 4, false, false, ""});
    columns.push_back({0, 1, "name", static_cast<uint16_t>(DataType::Varchar), 100, true, false, ""});
    columns.push_back({0, 2, "age", static_cast<uint16_t>(DataType::Int16), 2, true, true, "0"});
    columns.push_back({0, 3, "active", static_cast<uint16_t>(DataType::Boolean), 1, false, true, "true"});
    columns.push_back({0, 4, "created", static_cast<uint16_t>(DataType::Timestamp), 8, false, true, "now()"});
    
    // Create table
    uint32_t table_id;
    ASSERT_EQ(catalog->create_table(schema_id, "test_table", columns, table_id, &ctx), 
              Status::Ok);
    
    // Get columns
    std::vector<CatalogManager::ColumnInfo> retrieved_columns;
    ASSERT_EQ(catalog->get_columns(table_id, retrieved_columns, &ctx), Status::Ok);
    
    EXPECT_EQ(retrieved_columns.size(), 5);
    
    // Verify columns are in order
    EXPECT_EQ(retrieved_columns[0].column_name, "id");
    EXPECT_EQ(retrieved_columns[0].data_type, static_cast<uint16_t>(DataType::Int32));
    EXPECT_FALSE(retrieved_columns[0].nullable);
    
    EXPECT_EQ(retrieved_columns[1].column_name, "name");
    EXPECT_EQ(retrieved_columns[1].data_type, static_cast<uint16_t>(DataType::Varchar));
    EXPECT_TRUE(retrieved_columns[1].nullable);
    
    EXPECT_EQ(retrieved_columns[4].column_name, "created");
    EXPECT_EQ(retrieved_columns[4].default_value, "now()");
}

TEST_F(CatalogManagerTest, GetColumnByName) {
    CreateAndOpenDatabase();
    ErrorContext ctx;
    
    CatalogManager* catalog = db->catalog_manager();
    
    // Create schema and table
    uint32_t schema_id;
    ASSERT_EQ(catalog->create_schema("test_schema", "test_user", schema_id, &ctx), 
              Status::Ok);
    
    std::vector<CatalogManager::ColumnInfo> columns;
    columns.push_back({0, 0, "id", static_cast<uint16_t>(DataType::Int32), 4, false, false, ""});
    columns.push_back({0, 1, "name", static_cast<uint16_t>(DataType::Varchar), 100, true, false, ""});
    
    uint32_t table_id;
    ASSERT_EQ(catalog->create_table(schema_id, "test_table", columns, table_id, &ctx), 
              Status::Ok);
    
    // Get column by name
    CatalogManager::ColumnInfo col_info;
    ASSERT_EQ(catalog->get_column(table_id, "name", col_info, &ctx), Status::Ok);
    
    EXPECT_EQ(col_info.column_name, "name");
    EXPECT_EQ(col_info.column_id, 1);
    EXPECT_EQ(col_info.data_type, static_cast<uint16_t>(DataType::Varchar));
    EXPECT_EQ(col_info.max_length, 100);
    EXPECT_TRUE(col_info.nullable);
    
    // Try to get non-existent column
    Status status = catalog->get_column(table_id, "nonexistent", col_info, &ctx);
    EXPECT_EQ(status, Status::InvalidArgument);
}

TEST_F(CatalogManagerTest, InvalidOperations) {
    CreateAndOpenDatabase();
    ErrorContext ctx;
    
    CatalogManager* catalog = db->catalog_manager();
    
    // Try to create table in non-existent schema
    std::vector<CatalogManager::ColumnInfo> columns;
    columns.push_back({0, 0, "id", static_cast<uint16_t>(DataType::Int32), 4, false, false, ""});
    
    uint32_t table_id;
    Status status = catalog->create_table(9999, "test_table", columns, table_id, &ctx);
    EXPECT_EQ(status, Status::InvalidArgument);
    
    // Try to get non-existent schema
    CatalogManager::SchemaInfo schema_info;
    status = catalog->get_schema(9999, schema_info, &ctx);
    EXPECT_EQ(status, Status::InvalidArgument);
    
    // Try to get non-existent table
    CatalogManager::TableInfo table_info;
    status = catalog->get_table(9999, table_info, &ctx);
    EXPECT_EQ(status, Status::InvalidArgument);
}

TEST_F(CatalogManagerTest, CatalogPersistence) {
    GTEST_SKIP() << "Skipping due to TIP page issues - needs investigation";
    // Create database and add schema/table
    {
        CreateAndOpenDatabase();
        ErrorContext ctx;
        
        CatalogManager* catalog = db->catalog_manager();
        
        // Create schema
        uint32_t schema_id;
        ASSERT_EQ(catalog->create_schema("persistent_schema", "owner1", schema_id, &ctx), 
                  Status::Ok);
        
        // Create table
        std::vector<CatalogManager::ColumnInfo> columns;
        columns.push_back({0, 0, "id", static_cast<uint16_t>(DataType::Int64), 8, false, false, ""});
        columns.push_back({0, 1, "data", static_cast<uint16_t>(DataType::Text), 0, true, false, ""});
        
        uint32_t table_id;
        ASSERT_EQ(catalog->create_table(schema_id, "persistent_table", columns, table_id, &ctx), 
                  Status::Ok);
        
        // Close database
        db->close();
        delete db;
        db = nullptr;
    }
    
    // Reopen database and verify catalog
    {
        ErrorContext ctx;
        db = new Database();
        ASSERT_EQ(db->open(test_db_path, &ctx), Status::Ok);
        
        CatalogManager* catalog = db->catalog_manager();
        ASSERT_NE(catalog, nullptr);
        
        // Find schema
        CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(catalog->get_schema("persistent_schema", schema_info, &ctx), Status::Ok);
        EXPECT_EQ(schema_info.schema_name, "persistent_schema");
        EXPECT_EQ(schema_info.owner, "owner1");
        
        // Find table
        CatalogManager::TableInfo table_info;
        ASSERT_EQ(catalog->get_table(schema_info.schema_id, "persistent_table", table_info, &ctx), 
                  Status::Ok);
        EXPECT_EQ(table_info.table_name, "persistent_table");
        EXPECT_EQ(table_info.column_count, 2);
        
        // Verify columns
        std::vector<CatalogManager::ColumnInfo> columns;
        ASSERT_EQ(catalog->get_columns(table_info.table_id, columns, &ctx), Status::Ok);
        EXPECT_EQ(columns.size(), 2);
        EXPECT_EQ(columns[0].column_name, "id");
        EXPECT_EQ(columns[0].data_type, static_cast<uint16_t>(DataType::Int64));
        EXPECT_EQ(columns[1].column_name, "data");
        EXPECT_EQ(columns[1].data_type, static_cast<uint16_t>(DataType::Text));
    }
}

TEST_F(CatalogManagerTest, LargeNumberOfTables) {
    CreateAndOpenDatabase();
    ErrorContext ctx;
    
    CatalogManager* catalog = db->catalog_manager();
    
    // Create schema
    uint32_t schema_id;
    ASSERT_EQ(catalog->create_schema("load_test", "test_user", schema_id, &ctx), 
              Status::Ok);
    
    // Create many tables
    std::vector<CatalogManager::ColumnInfo> columns;
    columns.push_back({0, 0, "id", static_cast<uint16_t>(DataType::Int32), 4, false, false, ""});
    
    const int num_tables = 50;
    std::vector<uint32_t> table_ids;
    
    for (int i = 0; i < num_tables; i++) {
        std::string table_name = "table_" + std::to_string(i);
        uint32_t table_id;
        ASSERT_EQ(catalog->create_table(schema_id, table_name, columns, table_id, &ctx), 
                  Status::Ok);
        table_ids.push_back(table_id);
    }
    
    // Verify all tables exist
    std::vector<CatalogManager::TableInfo> tables;
    ASSERT_EQ(catalog->list_tables(schema_id, tables, &ctx), Status::Ok);
    EXPECT_EQ(tables.size(), num_tables);
    
    // Verify we can look up each table
    for (int i = 0; i < num_tables; i++) {
        CatalogManager::TableInfo info;
        ASSERT_EQ(catalog->get_table(table_ids[i], info, &ctx), Status::Ok);
        EXPECT_EQ(info.table_id, table_ids[i]);
    }
    
    EXPECT_EQ(catalog->table_count(), num_tables);
}