#include <gtest/gtest.h>
#include <cstdio>
#include <sys/stat.h>
#include <fstream>
#include "scratchbird/core/database.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

class Alpha101Test : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any test files from previous runs
        remove("test_8k.db");
        remove("test_16k.db");
        remove("test_32k.db");
        remove("test.db");
    }
    
    void TearDown() override {
        // Clean up test files
        remove("test_8k.db");
        remove("test_16k.db");
        remove("test_32k.db");
        remove("test.db");
    }
    
    bool file_exists(const char* path) {
        struct stat st;
        return stat(path, &st) == 0;
    }
    
    size_t get_file_size(const char* path) {
        struct stat st;
        if (stat(path, &st) == 0) {
            return st.st_size;
        }
        return 0;
    }
};

// Test database creation with different page sizes
TEST_F(Alpha101Test, CreateDatabase_8K) {
    ASSERT_EQ(Database::create("test_8k.db", 8192), Status::Ok);
    ASSERT_TRUE(file_exists("test_8k.db"));
    ASSERT_EQ(get_file_size("test_8k.db"), 8192 * 3);  // 3 pages: header, catalog, FSM
}

TEST_F(Alpha101Test, CreateDatabase_16K) {
    ASSERT_EQ(Database::create("test_16k.db", 16384), Status::Ok);
    ASSERT_TRUE(file_exists("test_16k.db"));
    ASSERT_EQ(get_file_size("test_16k.db"), 16384 * 3);  // 3 pages: header, catalog, FSM
}

TEST_F(Alpha101Test, CreateDatabase_32K) {
    ASSERT_EQ(Database::create("test_32k.db", 32768), Status::Ok);
    ASSERT_TRUE(file_exists("test_32k.db"));
    ASSERT_EQ(get_file_size("test_32k.db"), 32768 * 3);  // 3 pages: header, catalog, FSM
}

// Test header validation for each page size
TEST_F(Alpha101Test, ValidateHeader_8K) {
    Database::create("test.db", 8192);
    
    // Read header directly
    std::ifstream file("test.db", std::ios::binary);
    ASSERT_TRUE(file.is_open());
    
    uint8_t buffer[8192];
    file.read(reinterpret_cast<char*>(buffer), 8192);
    file.close();
    
    PageHeader* header = reinterpret_cast<PageHeader*>(buffer);
    ASSERT_EQ(header->magic, 0x53425244);  // 'SBRD'
    ASSERT_EQ(header->version, 1);
    ASSERT_EQ(header->page_type, PAGE_TYPE_DATABASE_HEADER);
    ASSERT_EQ(header->page_size, 8192);
    ASSERT_EQ(header->page_id, 0);
    ASSERT_TRUE(validate_page_checksum(buffer, 8192));
}

TEST_F(Alpha101Test, ValidateHeader_16K) {
    Database::create("test.db", 16384);
    
    std::ifstream file("test.db", std::ios::binary);
    ASSERT_TRUE(file.is_open());
    
    uint8_t buffer[16384];
    file.read(reinterpret_cast<char*>(buffer), 16384);
    file.close();
    
    PageHeader* header = reinterpret_cast<PageHeader*>(buffer);
    ASSERT_EQ(header->magic, 0x53425244);  // 'SBRD'
    ASSERT_EQ(header->version, 1);
    ASSERT_EQ(header->page_type, PAGE_TYPE_DATABASE_HEADER);
    ASSERT_EQ(header->page_size, 16384);
    ASSERT_EQ(header->page_id, 0);
    ASSERT_TRUE(validate_page_checksum(buffer, 16384));
}

TEST_F(Alpha101Test, ValidateHeader_32K) {
    Database::create("test.db", 32768);
    
    std::ifstream file("test.db", std::ios::binary);
    ASSERT_TRUE(file.is_open());
    
    uint8_t buffer[32768];
    file.read(reinterpret_cast<char*>(buffer), 32768);
    file.close();
    
    PageHeader* header = reinterpret_cast<PageHeader*>(buffer);
    ASSERT_EQ(header->magic, 0x53425244);  // 'SBRD'
    ASSERT_EQ(header->version, 1);
    ASSERT_EQ(header->page_type, PAGE_TYPE_DATABASE_HEADER);
    ASSERT_EQ(header->page_size, 32768);
    ASSERT_EQ(header->page_id, 0);
    ASSERT_TRUE(validate_page_checksum(buffer, 32768));
}

// Test system catalog initialization
TEST_F(Alpha101Test, SystemCatalogInitialization) {
    Database::create("test.db", 16384);
    
    // Read catalog page (Page 1)
    std::ifstream file("test.db", std::ios::binary);
    ASSERT_TRUE(file.is_open());
    
    // Skip header page
    file.seekg(16384);
    
    uint8_t buffer[16384];
    file.read(reinterpret_cast<char*>(buffer), 16384);
    file.close();
    
    PageHeader* header = reinterpret_cast<PageHeader*>(buffer);
    ASSERT_EQ(header->magic, 0x53425244);
    ASSERT_EQ(header->page_type, PAGE_TYPE_SYSTEM_CATALOG);
    ASSERT_EQ(header->page_id, 1);
    ASSERT_EQ(header->item_count, 8);  // 8 base schemas
    ASSERT_TRUE(validate_page_checksum(buffer, 16384));
    
    // Check catalog entries
    SystemCatalogEntry* entries = reinterpret_cast<SystemCatalogEntry*>(
        buffer + sizeof(PageHeader));
    
    // Verify schema names
    const char* expected_names[] = {
        "[root]", "[sys]", "[sec]", "[agents]", 
        "[app]", "[remote]", "[users]", "[roles]"
    };
    
    for (int i = 0; i < 8; i++) {
        ASSERT_STREQ(entries[i].name, expected_names[i]);
        ASSERT_EQ(entries[i].object_type, 0);  // Schema type
        
        // Root has no parent
        if (i == 0) {
            uint8_t zero_uuid[16] = {0};
            ASSERT_EQ(memcmp(entries[i].parent_uuid, zero_uuid, 16), 0);
        } else {
            // Others have root as parent
            ASSERT_EQ(memcmp(entries[i].parent_uuid, entries[0].schema_uuid, 16), 0);
        }
    }
}

// Test UUID v7 generation
TEST_F(Alpha101Test, UUIDv7Generation) {
    Database::create("test.db", 16384);
    
    std::ifstream file("test.db", std::ios::binary);
    ASSERT_TRUE(file.is_open());
    
    uint8_t buffer[16384];
    file.read(reinterpret_cast<char*>(buffer), 16384);
    file.close();
    
    PageHeader* header = reinterpret_cast<PageHeader*>(buffer);
    
    // Check UUID version bits (version 7)
    // Byte 6 should have 0x70 in high nibble
    ASSERT_EQ((header->database_uuid[6] & 0xF0), 0x70);
    
    // Check variant bits
    // Byte 8 should have 0x80 or higher in high nibble
    ASSERT_GE((header->database_uuid[8] & 0xC0), 0x80);
}

// Test error paths
TEST_F(Alpha101Test, CreateDatabase_FileExists) {
    ASSERT_EQ(Database::create("test.db", 16384), Status::Ok);
    ASSERT_EQ(Database::create("test.db", 16384), Status::FileExists);
}

TEST_F(Alpha101Test, CreateDatabase_InvalidPageSize) {
    // 64K and 128K are not supported in Alpha
    ASSERT_NE(Database::create("test.db", 65536), Status::Ok);
    ASSERT_NE(Database::create("test.db", 131072), Status::Ok);
    ASSERT_NE(Database::create("test.db", 4096), Status::Ok);
}

TEST_F(Alpha101Test, OpenDatabase_FileNotFound) {
    Database db;
    ASSERT_EQ(db.open("nonexistent.db"), Status::FileNotFound);
}

TEST_F(Alpha101Test, OpenDatabase_Success) {
    ASSERT_EQ(Database::create("test.db", 16384), Status::Ok);
    
    Database db;
    ASSERT_EQ(db.open("test.db"), Status::Ok);
    ASSERT_TRUE(db.is_open());
    ASSERT_EQ(db.page_size(), 16384);
}

// Test checksum validation
TEST_F(Alpha101Test, ChecksumValidation) {
    Database::create("test.db", 16384);
    
    // Corrupt the file
    std::fstream file("test.db", std::ios::binary | std::ios::in | std::ios::out);
    ASSERT_TRUE(file.is_open());
    
    // Corrupt a byte after the checksum field
    file.seekp(100);
    char corrupt = 0xFF;
    file.write(&corrupt, 1);
    file.close();
    
    // Try to open - should fail with checksum mismatch
    Database db;
    ASSERT_EQ(db.open("test.db"), Status::ChecksumMismatch);
}

// Test page read/write
TEST_F(Alpha101Test, PageReadWrite) {
    Database::create("test.db", 16384);
    
    Database db;
    ASSERT_EQ(db.open("test.db"), Status::Ok);
    
    // Read page 0
    uint8_t buffer[16384];
    ASSERT_EQ(db.read_page(0, buffer), Status::Ok);
    
    PageHeader* header = reinterpret_cast<PageHeader*>(buffer);
    ASSERT_EQ(header->magic, 0x53425244);
    ASSERT_EQ(header->page_id, 0);
    
    // Read page 1
    ASSERT_EQ(db.read_page(1, buffer), Status::Ok);
    header = reinterpret_cast<PageHeader*>(buffer);
    ASSERT_EQ(header->magic, 0x53425244);
    ASSERT_EQ(header->page_id, 1);
}

// Test CRC32C implementation
TEST_F(Alpha101Test, CRC32CComputation) {
    // Test with known values
    const char* test_data = "123456789";
    uint32_t crc = crc32c_compute(reinterpret_cast<const uint8_t*>(test_data), 9, 0xFFFFFFFF);
    crc ^= 0xFFFFFFFF;
    
    // CRC32C of "123456789" should be 0xE3069283
    ASSERT_EQ(crc, 0xE3069283);
}