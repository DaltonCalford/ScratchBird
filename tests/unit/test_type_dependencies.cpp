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
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>

using namespace scratchbird::core;

class TypeDependencyTest : public ::testing::Test
{
protected:
    std::string test_db_path;
    Database *db = nullptr;
    CatalogManager *catalog = nullptr;
    DomainManager *domain_mgr = nullptr;
    ID schema_id;

    void SetUp() override
    {
        test_db_path = "/tmp/test_type_dep_" + std::to_string(getpid()) + ".sbdb";
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

    ID createTableWithDefault(const std::string& table_name, const std::string& default_expr_hex)
    {
        ErrorContext ctx;
        std::vector<CatalogManager::ColumnInfo> columns;

        CatalogManager::ColumnInfo col;
        col.column_id = generateUuidV7();
        col.column_name = "id";
        col.data_type = static_cast<uint16_t>(DataType::INT32);
        col.nullable = false;
        col.ordinal = 0;
        col.has_default = true;
        col.default_expr = default_expr_hex;
        columns.push_back(col);

        ID table_id;
        Status status = catalog->createTable(schema_id, table_name, columns, table_id, 0, &ctx);
        EXPECT_EQ(status, Status::OK) << "createTable failed: " << ctx.message;
        return table_id;
    }
};

TEST_F(TypeDependencyTest, DropSequenceFailsIfDefaultUsesSequence)
{
    ErrorContext ctx;

    Status status = catalog->createSequence(schema_id, "seq_default", 1, 1, 1000000, 1, 1, false, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    std::string default_expr_hex = hexForLiteralString("seq_default");
    createTableWithDefault("t_default_seq", default_expr_hex);

    ID seq_id;
    ASSERT_EQ(catalog->getSequenceIdByName("seq_default", seq_id, &ctx), Status::OK);

    status = catalog->dropSequence(seq_id, false, &ctx);
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);
    EXPECT_NE(std::string(ctx.message).find("column"), std::string::npos) << ctx.message;
}

TEST_F(TypeDependencyTest, DropDomainFailsIfColumnUsesDomain)
{
    ErrorContext ctx;

    ID domain_id;
    // Create domain using DomainManager
    Status status = domain_mgr->createBasicDomain(schema_id, "dom_int", DataType::INT32,
                                                  0, 0, false, "", {}, domain_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

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
    status = catalog->createTable(schema_id, "t_domain", columns, table_id, 0, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    (void)table_id;

    // Drop domain using DomainManager - should fail due to dependency
    status = domain_mgr->dropDomain(domain_id, &ctx);
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);
    EXPECT_NE(std::string(ctx.message).find("column"), std::string::npos) << ctx.message;
}

TEST_F(TypeDependencyTest, DropExceptionFailsIfProcedureReferences)
{
    ErrorContext ctx;

    ID exception_id;
    Status status = catalog->createException(schema_id, "ex_test", "boom", exception_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    CatalogManager::ProcedureInfo proc;
    proc.procedure_id = generateUuidV7();
    proc.schema_id = schema_id;
    proc.name = "p1";
    proc.owner_id = catalog->getSystemUserId(&ctx);
    proc.source_text = "raise ex_test;";
    status = catalog->registerProcedure(proc, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    ID dep_id;
    status = catalog->createDependency(
        proc.procedure_id, CatalogManager::ObjectType::PROCEDURE,
        exception_id, CatalogManager::ObjectType::EXCEPTION,
        CatalogManager::DependencyType::NORMAL, dep_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = catalog->dropException(exception_id, false, &ctx);
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);
    EXPECT_NE(std::string(ctx.message).find("procedure"), std::string::npos) << ctx.message;
}
