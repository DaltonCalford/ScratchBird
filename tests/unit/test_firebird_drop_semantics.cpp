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
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/sblr/opcodes.h"
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

using namespace scratchbird::core;

class FirebirdDropSemanticsTest : public ::testing::Test
{
protected:
    std::string test_db_path;
    Database *db = nullptr;
    CatalogManager *catalog = nullptr;
    DomainManager *domain_mgr = nullptr;
    ID schema_id;

    void SetUp() override
    {
        test_db_path = "/tmp/test_fb_drop_" + std::to_string(getpid()) + ".sbdb";
        std::remove(test_db_path.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path, 16384, &ctx), Status::OK);

        db = new Database();
        ASSERT_EQ(db->open(test_db_path, &ctx), Status::OK);
        catalog = db->catalog_manager();
        ASSERT_NE(catalog, nullptr);
        domain_mgr = db->domain_manager();
        ASSERT_NE(domain_mgr, nullptr);

        CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(catalog->getSchema("PUBLIC", schema_info, &ctx), Status::OK);
        schema_id = schema_info.schema_id;
    }

    void TearDown() override
    {
        if (db)
        {
            db->close();
            delete db;
            db = nullptr;
        }
        std::remove(test_db_path.c_str());
        std::remove((test_db_path + "-lock").c_str());
    }

    static std::string hexForLiteralString(const std::string& value)
    {
        std::vector<uint8_t> bytes;
        bytes.push_back(static_cast<uint8_t>(scratchbird::sblr::Opcode::LITERAL_STRING));
        uint32_t len = static_cast<uint32_t>(value.size());
        bytes.push_back(static_cast<uint8_t>(len & 0xFF));
        bytes.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
        bytes.insert(bytes.end(), value.begin(), value.end());

        std::ostringstream ss;
        for (uint8_t b : bytes)
        {
            ss << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(b);
        }
        return ss.str();
    }
};

TEST_F(FirebirdDropSemanticsTest, DropSequenceBlocksWhenReferenced)
{
    ErrorContext ctx;
    ASSERT_EQ(catalog->createSequence(schema_id, "fb_seq", 1, 1, 1000, 1, 1, false, &ctx), Status::OK);

    std::vector<CatalogManager::ColumnInfo> columns;
    CatalogManager::ColumnInfo col;
    col.column_id = generateUuidV7();
    col.column_name = "id";
    col.data_type = static_cast<uint16_t>(DataType::INT32);
    col.nullable = false;
    col.ordinal = 0;
    col.has_default = true;
    col.default_expr = hexForLiteralString("fb_seq");
    columns.push_back(col);

    ID table_id;
    ASSERT_EQ(catalog->createTable(schema_id, "fb_table", columns, table_id, 0, &ctx), Status::OK);
    (void)table_id;

    ID seq_id;
    ASSERT_EQ(catalog->getSequenceIdByName("fb_seq", seq_id, &ctx), Status::OK);
    EXPECT_EQ(catalog->dropSequence(seq_id, false, &ctx), Status::CONSTRAINT_VIOLATION);
}

TEST_F(FirebirdDropSemanticsTest, DropDomainBlocksWhenReferenced)
{
    ErrorContext ctx;
    ID domain_id;

    // Create domain using DomainManager
    ASSERT_EQ(domain_mgr->createBasicDomain(schema_id, "fb_domain", DataType::INT32,
                                            0, 0, false, "", {}, domain_id, &ctx), Status::OK);

    std::vector<CatalogManager::ColumnInfo> columns;
    CatalogManager::ColumnInfo col;
    col.column_id = generateUuidV7();
    col.column_name = "id";
    col.data_type = static_cast<uint16_t>(DataType::INT32);
    col.nullable = false;
    col.ordinal = 0;
    col.domain_id = domain_id;
    columns.push_back(col);

    ID table_id;
    ASSERT_EQ(catalog->createTable(schema_id, "fb_table2", columns, table_id, 0, &ctx), Status::OK);
    (void)table_id;

    // Drop domain using DomainManager - should fail due to dependency
    EXPECT_EQ(domain_mgr->dropDomain(domain_id, &ctx), Status::CONSTRAINT_VIOLATION);
}
