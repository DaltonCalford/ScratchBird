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
#include <cstdio>
#include <sys/stat.h>
#include <fstream>
#include "scratchbird/core/database.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/uuidv7.h"
#include "test_helpers.h"

using namespace scratchbird::core;

class Alpha101Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Generate unique database paths for this test
        test_db_8k_ = std::make_unique<scratchbird::testing::TestDatabaseFile>("test_8k");
        test_db_16k_ = std::make_unique<scratchbird::testing::TestDatabaseFile>("test_16k");
        test_db_32k_ = std::make_unique<scratchbird::testing::TestDatabaseFile>("test_32k");
        test_db_ = std::make_unique<scratchbird::testing::TestDatabaseFile>("test");
    }

    void TearDown() override
    {
        // TestDatabaseFile automatically cleans up
        test_db_8k_.reset();
        test_db_16k_.reset();
        test_db_32k_.reset();
        test_db_.reset();
    }

    bool file_exists(const std::string& path)
    {
        struct stat st;
        return stat(path.c_str(), &st) == 0;
    }

    size_t get_file_size(const std::string& path)
    {
        struct stat st;
        if (stat(path.c_str(), &st) == 0)
        {
            return st.st_size;
        }
        return 0;
    }

    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_db_8k_;
    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_db_16k_;
    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_db_32k_;
    std::unique_ptr<scratchbird::testing::TestDatabaseFile> test_db_;
};

// Test database creation with different page sizes
TEST_F(Alpha101Test, CreateDatabase_8K)
{
    ASSERT_EQ(Database::create(test_db_8k_->path(), 8192), Status::OK);
    ASSERT_TRUE(file_exists(test_db_8k_->path()));
    ASSERT_EQ(get_file_size(test_db_8k_->path()),
              8192 * BOOTSTRAP_FIXED_PAGE_COUNT); // fixed bootstrap pages 0..5
}

TEST_F(Alpha101Test, CreateDatabase_16K)
{
    ASSERT_EQ(Database::create(test_db_16k_->path(), 16384), Status::OK);
    ASSERT_TRUE(file_exists(test_db_16k_->path()));
    ASSERT_EQ(get_file_size(test_db_16k_->path()),
              16384 * BOOTSTRAP_FIXED_PAGE_COUNT); // fixed bootstrap pages 0..5
}

TEST_F(Alpha101Test, CreateDatabase_32K)
{
    ASSERT_EQ(Database::create(test_db_32k_->path(), 32768), Status::OK);
    ASSERT_TRUE(file_exists(test_db_32k_->path()));
    ASSERT_EQ(get_file_size(test_db_32k_->path()),
              32768 * BOOTSTRAP_FIXED_PAGE_COUNT); // fixed bootstrap pages 0..5
}

// Test header validation for each page size
TEST_F(Alpha101Test, ValidateHeader_8K)
{
    Database::create(test_db_->path(), 8192);

    // Read header directly
    std::ifstream file(test_db_->path(), std::ios::binary);
    ASSERT_TRUE(file.is_open());

    uint8_t buffer[8192];
    file.read(reinterpret_cast<char *>(buffer), 8192);
    file.close();

    PageHeader *header = reinterpret_cast<PageHeader *>(buffer);
    ASSERT_EQ(header->magic, 0x53425244); // 'SBRD'
    ASSERT_EQ(header->version, 1);
    ASSERT_EQ(header->page_type, PAGE_TYPE_DATABASE_HEADER);
    ASSERT_EQ(header->page_size, 8192);
    ASSERT_EQ(header->page_id, 0);
    ASSERT_TRUE(validatePageChecksum(buffer, 8192));
}

TEST_F(Alpha101Test, ValidateHeader_16K)
{
    Database::create(test_db_->path(), 16384);

    std::ifstream file(test_db_->path(), std::ios::binary);
    ASSERT_TRUE(file.is_open());

    uint8_t buffer[16384];
    file.read(reinterpret_cast<char *>(buffer), 16384);
    file.close();

    PageHeader *header = reinterpret_cast<PageHeader *>(buffer);
    ASSERT_EQ(header->magic, 0x53425244); // 'SBRD'
    ASSERT_EQ(header->version, 1);
    ASSERT_EQ(header->page_type, PAGE_TYPE_DATABASE_HEADER);
    ASSERT_EQ(header->page_size, 16384);
    ASSERT_EQ(header->page_id, 0);
    ASSERT_TRUE(validatePageChecksum(buffer, 16384));
}

TEST_F(Alpha101Test, ValidateHeader_32K)
{
    Database::create(test_db_->path(), 32768);

    std::ifstream file(test_db_->path(), std::ios::binary);
    ASSERT_TRUE(file.is_open());

    uint8_t buffer[32768];
    file.read(reinterpret_cast<char *>(buffer), 32768);
    file.close();

    PageHeader *header = reinterpret_cast<PageHeader *>(buffer);
    ASSERT_EQ(header->magic, 0x53425244); // 'SBRD'
    ASSERT_EQ(header->version, 1);
    ASSERT_EQ(header->page_type, PAGE_TYPE_DATABASE_HEADER);
    ASSERT_EQ(header->page_size, 32768);
    ASSERT_EQ(header->page_id, 0);
    ASSERT_TRUE(validatePageChecksum(buffer, 32768));
}

// Test canonical bootstrap page map
TEST_F(Alpha101Test, SystemCatalogInitialization)
{
    Database::create(test_db_->path(), 16384);

    // Read system state page (Page 1)
    std::ifstream file(test_db_->path(), std::ios::binary);
    ASSERT_TRUE(file.is_open());

    // Skip header page
    file.seekg(16384);

    uint8_t buffer[16384];
    file.read(reinterpret_cast<char *>(buffer), 16384);
    PageHeader *header = reinterpret_cast<PageHeader *>(buffer);
    ASSERT_EQ(header->magic, 0x53425244);
    ASSERT_EQ(header->page_type, PAGE_TYPE_SYSTEM_STATE);
    ASSERT_EQ(header->page_id, 1);
    auto *system_state = reinterpret_cast<BootstrapSystemStatePage *>(buffer);
    ASSERT_EQ(system_state->clean_shutdown, 1);
    ASSERT_TRUE(validatePageChecksum(buffer, 16384));

    // Read catalog root page (Page 2)
    file.seekg(16384 * 2);
    file.read(reinterpret_cast<char *>(buffer), 16384);
    header = reinterpret_cast<PageHeader *>(buffer);
    ASSERT_EQ(header->magic, 0x53425244);
    ASSERT_EQ(header->page_type, PAGE_TYPE_CATALOG_ROOT);
    ASSERT_EQ(header->page_id, 2);
    ASSERT_TRUE(validatePageChecksum(buffer, 16384));

    // Read FSM root page (Page 3)
    file.seekg(16384 * 3);
    file.read(reinterpret_cast<char *>(buffer), 16384);
    header = reinterpret_cast<PageHeader *>(buffer);
    ASSERT_EQ(header->magic, 0x53425244);
    ASSERT_EQ(header->page_type, PAGE_TYPE_FSM_ROOT);
    ASSERT_EQ(header->page_id, 3);
    ASSERT_TRUE(validatePageChecksum(buffer, 16384));

    // Read transaction map root page (Page 4)
    file.seekg(16384 * 4);
    file.read(reinterpret_cast<char *>(buffer), 16384);
    header = reinterpret_cast<PageHeader *>(buffer);
    ASSERT_EQ(header->magic, 0x53425244);
    ASSERT_EQ(header->page_type, PAGE_TYPE_TRANSACTION_MAP);
    ASSERT_EQ(header->page_id, 4);
    ASSERT_TRUE(validatePageChecksum(buffer, 16384));

    // Read reserved bootstrap page (Page 5)
    file.seekg(16384 * 5);
    file.read(reinterpret_cast<char *>(buffer), 16384);
    file.close();
    header = reinterpret_cast<PageHeader *>(buffer);
    ASSERT_EQ(header->magic, 0x53425244);
    ASSERT_EQ(header->page_type, PAGE_TYPE_BOOTSTRAP_RESERVED);
    ASSERT_EQ(header->page_id, 5);
    ASSERT_TRUE(validatePageChecksum(buffer, 16384));
}

// Test UUID v7 generation
TEST_F(Alpha101Test, UUIDv7Generation)
{
    Database::create(test_db_->path(), 16384);

    std::ifstream file(test_db_->path(), std::ios::binary);
    ASSERT_TRUE(file.is_open());

    uint8_t buffer[16384];
    file.read(reinterpret_cast<char *>(buffer), 16384);
    file.close();

    DatabaseHeader *header = reinterpret_cast<DatabaseHeader *>(buffer);

    // Check UUID version bits (version 7)
    // Byte 6 should have 0x70 in high nibble
    ASSERT_EQ((header->database_uuid.bytes[6] & 0xF0), 0x70);

    // Check variant bits
    // Byte 8 should have 0x80 or higher in high nibble
    ASSERT_GE((header->database_uuid.bytes[8] & 0xC0), 0x80);
}

// Test error paths
TEST_F(Alpha101Test, CreateDatabase_FileExists)
{
    ASSERT_EQ(Database::create(test_db_->path(), 16384), Status::OK);
    ASSERT_EQ(Database::create(test_db_->path(), 16384), Status::FILE_EXISTS);
}

TEST_F(Alpha101Test, CreateDatabase_InvalidPageSize)
{
    // Only 8192, 16384, 32768, 65536, and 131072 are valid page sizes
    // Test invalid page sizes
    ASSERT_NE(Database::create(test_db_->path(), 4096), Status::OK);   // Too small
    ASSERT_NE(Database::create(test_db_->path(), 1024), Status::OK);   // Too small
    ASSERT_NE(Database::create(test_db_->path(), 262144), Status::OK); // Too large (256K)
    ASSERT_NE(Database::create(test_db_->path(), 12345), Status::OK);  // Not a power of 2
}

TEST_F(Alpha101Test, OpenDatabase_FileNotFound)
{
    Database db;
    ASSERT_EQ(db.open("nonexistent.db"), Status::FILE_NOT_FOUND);
}

TEST_F(Alpha101Test, OpenDatabase_Success)
{
    ASSERT_EQ(Database::create(test_db_->path(), 16384), Status::OK);

    Database db;
    ASSERT_EQ(db.open(test_db_->path()), Status::OK);
    ASSERT_TRUE(db.is_open());
    ASSERT_EQ(db.page_size(), 16384);
}

// Test checksum validation
TEST_F(Alpha101Test, ChecksumValidation)
{
    Database::create(test_db_->path(), 16384);

    // Corrupt the file
    std::fstream file(test_db_->path(), std::ios::binary | std::ios::in | std::ios::out);
    ASSERT_TRUE(file.is_open());

    // Corrupt payload bytes outside the canonical page header so payload CRC
    // validation must fail on reopen.
    file.seekp(CANONICAL_PAGE_HEADER_BYTES + 16);
    char corrupt = 0xFF;
    file.write(&corrupt, 1);
    file.close();

    // Try to open - should fail with checksum mismatch
    Database db;
    ASSERT_EQ(db.open(test_db_->path()), Status::CHECKSUM_MISMATCH);
}

// Test page read/write
TEST_F(Alpha101Test, PageReadWrite)
{
    Database::create(test_db_->path(), 16384);

    Database db;
    ASSERT_EQ(db.open(test_db_->path()), Status::OK);

    // Read page 0
    uint8_t buffer[16384];
    ASSERT_EQ(db.read_page(0, buffer), Status::OK);

    PageHeader *header = reinterpret_cast<PageHeader *>(buffer);
    ASSERT_EQ(header->magic, 0x53425244);
    ASSERT_EQ(header->page_id, 0);

    // Read page 1
    ASSERT_EQ(db.read_page(1, buffer), Status::OK);
    header = reinterpret_cast<PageHeader *>(buffer);
    ASSERT_EQ(header->magic, 0x53425244);
    ASSERT_EQ(header->page_id, 1);
    ASSERT_EQ(header->page_type, PAGE_TYPE_SYSTEM_STATE);
}

// Test CRC32C implementation
TEST_F(Alpha101Test, CRC32CComputation)
{
    // Test with known values
    const char *test_data = "123456789";
    uint32_t crc = crc32cCompute(reinterpret_cast<const uint8_t *>(test_data), 9, 0xFFFFFFFF);
    crc ^= 0xFFFFFFFF;

    // CRC32C of "123456789" should be 0xE3069283
    ASSERT_EQ(crc, 0xE3069283);
}
