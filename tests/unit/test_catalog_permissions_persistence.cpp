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
#include <vector>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/uuidv7.h"
#include "test_helpers.h"

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestDbPath;

TEST(CatalogPermissionsPersistenceTest, ColumnObjectPolicyPersistAcrossRestart)
{
    std::string db_path = uniqueTestDbPath("test_catalog_permissions_persist");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    Status status = Database::create(db_path.c_str(), 8192, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    {
        Database db;
        status = db.open(db_path.c_str(), &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        auto *catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        CatalogManager::SchemaInfo schema;
        status = catalog->getSchema("PUBLIC", schema, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        std::vector<CatalogManager::ColumnInfo> columns;
        CatalogManager::ColumnInfo col;
        col.column_id = generateUuidV7();
        col.column_name = "payload";
        col.data_type = static_cast<uint16_t>(DataType::VARCHAR);
        col.type_precision = 64;
        col.max_length = 64;
        col.nullable = false;
        col.ordinal = 0;
        columns.push_back(col);

        ID table_id;
        status = catalog->createTable(schema.schema_id, "perm_table", columns, table_id, 0, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        ID system_user = catalog->getSystemUserId(&ctx);
        ASSERT_NE(system_user, ID{});

        status = catalog->grantColumnPermission(
            table_id, "payload", system_user, CatalogManager::GranteeType::USER,
            static_cast<uint32_t>(CatalogManager::Privilege::SELECT),
            false, system_user, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        ID object_perm_id;
        status = catalog->grantObjectPermission(
            table_id, CatalogManager::ObjectType::TABLE, system_user,
            CatalogManager::GranteeType::USER, CatalogManager::PERM_SELECT,
            false, object_perm_id, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        ID policy_id;
        status = catalog->createPolicy(
            table_id, "policy_test", CatalogManager::PolicyType::SELECT,
            {}, "tenant_id = 1", "", policy_id, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        db.close();
    }

    {
        Database db;
        status = db.open(db_path.c_str(), &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        auto *catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        CatalogManager::SchemaInfo schema;
        status = catalog->getSchema("PUBLIC", schema, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        CatalogManager::TableInfo table;
        status = catalog->getTable(schema.schema_id, "perm_table", table, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        std::vector<CatalogManager::ColumnPermissionInfo> column_perms;
        status = catalog->getColumnPermissions(table.table_id, column_perms, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;
        ASSERT_EQ(column_perms.size(), 1u);
        EXPECT_EQ(column_perms[0].column_name, "payload");
        EXPECT_EQ(column_perms[0].privileges,
                  static_cast<uint32_t>(CatalogManager::Privilege::SELECT));

        std::vector<CatalogManager::ObjectPermissionInfo> object_perms;
        status = catalog->getObjectPermissions(table.table_id, object_perms, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;
        ASSERT_EQ(object_perms.size(), 1u);
        EXPECT_EQ(object_perms[0].object_type, CatalogManager::ObjectType::TABLE);
        EXPECT_EQ(object_perms[0].permissions, CatalogManager::PERM_SELECT);

        CatalogManager::PolicyInfo policy_info;
        status = catalog->getPolicy(table.table_id, "policy_test", policy_info, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;
        EXPECT_EQ(policy_info.policy_name, "policy_test");
        EXPECT_EQ(policy_info.using_expr, "tenant_id = 1");
        EXPECT_EQ(policy_info.policy_type, CatalogManager::PolicyType::SELECT);

        db.close();
    }

    std::remove(db_path.c_str());
}
