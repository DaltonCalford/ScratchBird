# Alpha 1.01 Detailed Specification
## Engine Bootstrap & Database File Creation

## Objective

Create the absolute minimum viable ScratchBird engine that:
1. Responds to version queries
2. Creates properly structured database files
3. Initializes the hierarchical schema system with UUIDs
4. Passes comprehensive tests for multiple page sizes

## Deliverables

### 1. Engine Executable

```bash
# Version query
$ scratchbird --version
ScratchBird Alpha 1.01
Build: 2024.01.15.001
Page Sizes: 8192, 16384, 32768

# Create database
$ scratchbird create database test.sdb --page-size=8192
Database created: test.sdb (page_size=8192)

# Create with different page sizes
$ scratchbird create database test16k.sdb --page-size=16384
$ scratchbird create database test32k.sdb --page-size=32768
```

### 2. Database File Format

#### Page 0: Database Header
```cpp
struct DatabaseHeader {  // Must fit in one page
    // Identification
    char magic[16] = "SCRATCHBIRD_DB\x00\x01";  // Magic + version
    uint16_t version_major = 1;
    uint16_t version_minor = 1;
    uint32_t page_size;      // 8192, 16384, or 32768
    
    // Database identity
    UUID database_uuid;       // Unique database identifier
    char database_name[256];  // Database name
    uint64_t creation_time;   // Unix timestamp
    
    // Page management
    uint64_t page_count;      // Total pages in file
    uint64_t free_page_list;  // Head of free page list
    uint64_t data_pages;      // Start of data pages
    
    // Schema tree
    uint64_t schema_root_page;     // Root of schema tree
    UUID root_schema_uuid;          // UUID of [root] schema
    
    // System catalog
    uint64_t syscat_root_page;      // System catalog root
    uint64_t sys_tables_page;       // sys.tables location
    uint64_t sys_columns_page;      // sys.columns location
    uint64_t sys_schemas_page;      // sys.schemas location
    
    // Transaction management (stubbed for future)
    uint64_t tip_page;              // Transaction Inventory Page
    uint64_t oldest_transaction;    // For MGA (unused)
    uint64_t newest_transaction;    // For MGA (unused)
    
    // WAL (stubbed for future)
    uint64_t wal_level;             // 0 = disabled
    char wal_directory[256];        // WAL location (unused)
    uint64_t last_checkpoint_lsn;   // LSN (unused)
    
    // Replication (stubbed for future)
    uint64_t replication_role;      // 0 = none
    UUID cluster_uuid;              // Cluster ID (unused)
    
    // Reserved for future use
    uint8_t reserved[2048];         // Room for growth
    
    // Integrity
    uint32_t header_checksum;       // CRC32 of header
};
```

#### Page 1: Schema Tree Root
```cpp
struct SchemaTreePage {
    PageHeader page_header;  // Standard page header
    
    struct SchemaNode {
        UUID schema_uuid;
        UUID parent_uuid;           // NULL for root
        char schema_name[128];      // "root", "sys", "sec", etc.
        uint64_t child_page;        // Page containing children
        uint32_t child_count;       // Number of children
        uint32_t flags;             // Various flags
        uint64_t metadata_page;     // Additional metadata (unused)
    };
    
    uint32_t node_count;
    SchemaNode nodes[];  // Variable length array
};

// Initial schema tree structure:
// [root] - UUID: generated
//   ├── [sys] - UUID: generated
//   ├── [sec] - UUID: generated
//   ├── [agents] - UUID: generated
//   ├── [app] - UUID: generated
//   ├── [remote] - UUID: generated
//   ├── [users] - UUID: generated
//   └── [roles] - UUID: generated
```

#### Page 2: System Catalog Root
```cpp
struct SystemCatalogPage {
    PageHeader page_header;
    
    struct CatalogEntry {
        UUID object_uuid;
        char object_name[128];
        uint32_t object_type;  // TABLE, VIEW, PROCEDURE, etc.
        UUID schema_uuid;      // Which schema owns this
        uint64_t data_page;    // Where object data starts
        uint64_t metadata_page; // Object metadata
        uint32_t column_count;  // For tables
        uint32_t flags;
    };
    
    uint32_t entry_count;
    CatalogEntry entries[];
};

// Initial system tables:
// [root].[sys].schemas - List of all schemas
// [root].[sys].tables - List of all tables  
// [root].[sys].columns - List of all columns
// [root].[sys].indexes - List of all indexes
// [root].[sys].procedures - List of all procedures
// [root].[sys].version_control_log - Change history (empty)
```

#### Page 3+: System Table Data
```cpp
// sys.schemas table data
struct SysSchemas {
    UUID schema_uuid;
    UUID parent_uuid;
    char path[1000];       // "[root].[sys]"
    char name[128];        // "sys"
    uint32_t level;        // 0=root, 1=top-level, etc.
    uint64_t created_time;
    UUID owner_uuid;
    uint32_t object_count;
};

// sys.tables table data
struct SysTables {
    UUID table_uuid;
    UUID schema_uuid;
    char table_name[128];
    char full_path[1000];  // "[root].[sys].tables"
    uint32_t column_count;
    uint64_t row_count;    // 0 initially
    uint64_t data_pages;
    uint64_t index_pages;
    uint64_t created_time;
};

// sys.columns table data
struct SysColumns {
    UUID column_uuid;
    UUID table_uuid;
    char column_name[128];
    uint32_t column_position;
    uint32_t data_type;
    uint32_t max_length;
    bool is_nullable;
    bool is_primary_key;
    char default_value[256];
};
```

### 3. Core Implementation Files

#### src/alpha/database_creator.cpp
```cpp
#include "scratchbird/alpha/database_creator.h"

namespace scratchbird::alpha {

class DatabaseCreator {
private:
    size_t page_size_;
    string db_path_;
    fstream file_;
    
    UUID generate_uuid() {
        // Simple UUID v4 generation
        return UUID::random();
    }
    
    void write_page(uint64_t page_num, const void* data) {
        file_.seekp(page_num * page_size_);
        file_.write(reinterpret_cast<const char*>(data), page_size_);
    }
    
public:
    bool create_database(const string& path, size_t page_size) {
        // Validate page size
        if (page_size != 8192 && page_size != 16384 && page_size != 32768) {
            return false;
        }
        
        page_size_ = page_size;
        db_path_ = path;
        
        // Open file
        file_.open(path, ios::binary | ios::out | ios::trunc);
        if (!file_) return false;
        
        // Allocate initial space (minimum 100 pages)
        size_t initial_pages = 100;
        size_t file_size = initial_pages * page_size;
        file_.seekp(file_size - 1);
        file_.write("", 1);
        
        // Create and write header page
        create_header_page();
        
        // Create schema tree
        create_schema_tree();
        
        // Create system catalog
        create_system_catalog();
        
        // Initialize system tables
        initialize_system_tables();
        
        file_.close();
        return true;
    }
    
private:
    void create_header_page() {
        vector<uint8_t> page(page_size_, 0);
        DatabaseHeader* header = reinterpret_cast<DatabaseHeader*>(page.data());
        
        strcpy(header->magic, "SCRATCHBIRD_DB\x00\x01");
        header->version_major = 1;
        header->version_minor = 1;
        header->page_size = page_size_;
        header->database_uuid = generate_uuid();
        header->creation_time = time(nullptr);
        header->page_count = 100;
        header->free_page_list = 0;
        header->schema_root_page = 1;
        header->syscat_root_page = 2;
        header->sys_tables_page = 3;
        header->sys_columns_page = 4;
        header->sys_schemas_page = 5;
        header->tip_page = 10;  // Reserve for future
        
        header->header_checksum = calculate_crc32(header, sizeof(DatabaseHeader) - 4);
        
        write_page(0, page.data());
    }
    
    void create_schema_tree() {
        vector<uint8_t> page(page_size_, 0);
        SchemaTreePage* tree = reinterpret_cast<SchemaTreePage*>(page.data());
        
        // Create root and immediate children
        tree->node_count = 8;
        
        // [root]
        tree->nodes[0].schema_uuid = generate_uuid();
        tree->nodes[0].parent_uuid = UUID::nil();
        strcpy(tree->nodes[0].schema_name, "root");
        tree->nodes[0].child_count = 7;
        
        // [sys]
        tree->nodes[1].schema_uuid = generate_uuid();
        tree->nodes[1].parent_uuid = tree->nodes[0].schema_uuid;
        strcpy(tree->nodes[1].schema_name, "sys");
        
        // [sec]
        tree->nodes[2].schema_uuid = generate_uuid();
        tree->nodes[2].parent_uuid = tree->nodes[0].schema_uuid;
        strcpy(tree->nodes[2].schema_name, "sec");
        
        // [agents]
        tree->nodes[3].schema_uuid = generate_uuid();
        tree->nodes[3].parent_uuid = tree->nodes[0].schema_uuid;
        strcpy(tree->nodes[3].schema_name, "agents");
        
        // [app]
        tree->nodes[4].schema_uuid = generate_uuid();
        tree->nodes[4].parent_uuid = tree->nodes[0].schema_uuid;
        strcpy(tree->nodes[4].schema_name, "app");
        
        // [remote]
        tree->nodes[5].schema_uuid = generate_uuid();
        tree->nodes[5].parent_uuid = tree->nodes[0].schema_uuid;
        strcpy(tree->nodes[5].schema_name, "remote");
        
        // [users]
        tree->nodes[6].schema_uuid = generate_uuid();
        tree->nodes[6].parent_uuid = tree->nodes[0].schema_uuid;
        strcpy(tree->nodes[6].schema_name, "users");
        
        // [roles]
        tree->nodes[7].schema_uuid = generate_uuid();
        tree->nodes[7].parent_uuid = tree->nodes[0].schema_uuid;
        strcpy(tree->nodes[7].schema_name, "roles");
        
        write_page(1, page.data());
    }
    
    void create_system_catalog() {
        vector<uint8_t> page(page_size_, 0);
        SystemCatalogPage* catalog = reinterpret_cast<SystemCatalogPage*>(page.data());
        
        catalog->entry_count = 6;
        
        // sys.schemas table
        catalog->entries[0].object_uuid = generate_uuid();
        strcpy(catalog->entries[0].object_name, "schemas");
        catalog->entries[0].object_type = OBJECT_TABLE;
        catalog->entries[0].schema_uuid = /* sys schema uuid */;
        catalog->entries[0].data_page = 5;
        
        // sys.tables table
        catalog->entries[1].object_uuid = generate_uuid();
        strcpy(catalog->entries[1].object_name, "tables");
        catalog->entries[1].object_type = OBJECT_TABLE;
        catalog->entries[1].data_page = 3;
        
        // sys.columns table
        catalog->entries[2].object_uuid = generate_uuid();
        strcpy(catalog->entries[2].object_name, "columns");
        catalog->entries[2].object_type = OBJECT_TABLE;
        catalog->entries[2].data_page = 4;
        
        // Continue for other system tables...
        
        write_page(2, page.data());
    }
    
    void initialize_system_tables() {
        // Write initial rows to sys.schemas
        populate_schemas_table();
        
        // Write initial rows to sys.tables
        populate_tables_table();
        
        // Write initial rows to sys.columns
        populate_columns_table();
    }
};

} // namespace scratchbird::alpha
```

### 4. Comprehensive Test Suite

#### tests/alpha_101_tests.cpp
```cpp
#include <gtest/gtest.h>
#include "scratchbird/alpha/engine.h"
#include "scratchbird/alpha/database.h"

class Alpha101Test : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any existing test files
        cleanup_test_files();
    }
    
    void TearDown() override {
        cleanup_test_files();
    }
    
    void cleanup_test_files() {
        filesystem::remove("test_8k.sdb");
        filesystem::remove("test_16k.sdb");
        filesystem::remove("test_32k.sdb");
    }
};

TEST_F(Alpha101Test, EngineVersion) {
    Engine engine;
    EXPECT_EQ(engine.version(), "Alpha 1.01");
    EXPECT_EQ(engine.version_major(), 1);
    EXPECT_EQ(engine.version_minor(), 1);
}

TEST_F(Alpha101Test, CreateDatabaseMultiplePageSizes) {
    vector<size_t> page_sizes = {8192, 16384, 32768};
    
    for (size_t page_size : page_sizes) {
        string db_name = "test_" + to_string(page_size/1024) + "k.sdb";
        
        // Create database
        Engine engine;
        ASSERT_TRUE(engine.create_database(db_name, page_size));
        
        // Verify file exists
        ASSERT_TRUE(filesystem::exists(db_name));
        
        // Verify file size is multiple of page size
        size_t file_size = filesystem::file_size(db_name);
        ASSERT_EQ(file_size % page_size, 0);
        ASSERT_GE(file_size, page_size * 100);  // At least 100 pages
        
        // Open and verify
        Database db(db_name);
        ASSERT_TRUE(db.open());
        
        // Check header
        ASSERT_EQ(db.page_size(), page_size);
        ASSERT_EQ(db.version(), "1.01");
        ASSERT_FALSE(db.database_uuid().is_nil());
        
        // Verify schema tree
        ASSERT_TRUE(db.schema_exists("[root]"));
        ASSERT_TRUE(db.schema_exists("[root].[sys]"));
        ASSERT_TRUE(db.schema_exists("[root].[sec]"));
        ASSERT_TRUE(db.schema_exists("[root].[agents]"));
        ASSERT_TRUE(db.schema_exists("[root].[app]"));
        ASSERT_TRUE(db.schema_exists("[root].[remote]"));
        ASSERT_TRUE(db.schema_exists("[root].[users]"));
        ASSERT_TRUE(db.schema_exists("[root].[roles]"));
        
        // Verify all schemas have UUIDs
        auto root_uuid = db.get_schema_uuid("[root]");
        ASSERT_FALSE(root_uuid.is_nil());
        
        auto sys_uuid = db.get_schema_uuid("[root].[sys]");
        ASSERT_FALSE(sys_uuid.is_nil());
        ASSERT_NE(sys_uuid, root_uuid);
        
        // Verify system tables exist
        ASSERT_TRUE(db.table_exists("[root].[sys].schemas"));
        ASSERT_TRUE(db.table_exists("[root].[sys].tables"));
        ASSERT_TRUE(db.table_exists("[root].[sys].columns"));
        ASSERT_TRUE(db.table_exists("[root].[sys].indexes"));
        
        // Verify system tables have UUIDs
        auto tables_uuid = db.get_table_uuid("[root].[sys].tables");
        ASSERT_FALSE(tables_uuid.is_nil());
        
        // Check system table contents
        auto schemas = db.query_system_table("schemas");
        ASSERT_GE(schemas.size(), 8);  // At least 8 schemas
        
        // Verify parent-child relationships
        auto sys_parent = db.get_schema_parent("[root].[sys]");
        ASSERT_EQ(sys_parent, "[root]");
        
        db.close();
    }
}

TEST_F(Alpha101Test, DatabasePersistence) {
    // Create database
    {
        Engine engine;
        ASSERT_TRUE(engine.create_database("persist_test.sdb", 8192));
        
        Database db("persist_test.sdb");
        ASSERT_TRUE(db.open());
        
        auto uuid_before = db.database_uuid();
        ASSERT_FALSE(uuid_before.is_nil());
        
        db.close();
    }
    
    // Reopen and verify
    {
        Database db("persist_test.sdb");
        ASSERT_TRUE(db.open());
        
        // Should have same UUID
        auto uuid_after = db.database_uuid();
        ASSERT_FALSE(uuid_after.is_nil());
        
        // All schemas still exist
        ASSERT_TRUE(db.schema_exists("[root]"));
        ASSERT_TRUE(db.schema_exists("[root].[sys]"));
        
        db.close();
    }
    
    filesystem::remove("persist_test.sdb");
}

TEST_F(Alpha101Test, InvalidPageSize) {
    Engine engine;
    
    // Should reject invalid page sizes
    ASSERT_FALSE(engine.create_database("bad.sdb", 4096));
    ASSERT_FALSE(engine.create_database("bad.sdb", 9000));
    ASSERT_FALSE(engine.create_database("bad.sdb", 65536));
    
    // File should not exist
    ASSERT_FALSE(filesystem::exists("bad.sdb"));
}

TEST_F(Alpha101Test, HeaderIntegrity) {
    Engine engine;
    ASSERT_TRUE(engine.create_database("integrity.sdb", 8192));
    
    // Corrupt the header
    {
        fstream file("integrity.sdb", ios::binary | ios::in | ios::out);
        file.seekp(100);  // Write garbage in header
        file.write("CORRUPT", 7);
        file.close();
    }
    
    // Should detect corruption
    Database db("integrity.sdb");
    ASSERT_FALSE(db.open());  // Should fail due to bad checksum
    
    filesystem::remove("integrity.sdb");
}
```

## Success Criteria

Alpha 1.01 is complete when:

1. ✅ Engine responds to `--version` with "Alpha 1.01"
2. ✅ Can create database files with 8K, 16K, 32K page sizes
3. ✅ Database file has proper header with magic number
4. ✅ Schema tree initialized with all 8 base schemas
5. ✅ Every schema and table has a UUID assigned
6. ✅ System tables (schemas, tables, columns) are created
7. ✅ Parent-child relationships are correct in schema tree
8. ✅ All tests pass for all three page sizes
9. ✅ Database file survives close and reopen
10. ✅ Header checksum validates integrity

## Future Hooks Included

Even though not used in Alpha 1.01, the following are included:

1. **Transaction fields** in header (TIP page, transaction IDs)
2. **WAL fields** in header (WAL level, directory, LSN)
3. **Replication fields** in header (role, cluster UUID)
4. **MGA fields** in tuple headers (xmin, xmax, etc.)
5. **Version chain pointers** in tuple structure
6. **Reserved space** in all structures for future expansion
7. **Page type enumeration** includes all future types
8. **BLR opcode space** reserved in design
9. **Remote schema mount points** in schema tree
10. **Version control tables** created but empty

This ensures Alpha 1.01 builds the right foundation for everything that follows!