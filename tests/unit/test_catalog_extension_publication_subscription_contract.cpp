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
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

class CatalogExtensionPublicationSubscriptionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    ID schema_id_{};
    ID system_user_id_{};

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_extension_publication_subscription_contract_" +
                   std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());

        ASSERT_EQ(catalog_->createSchema("cat028_schema", "system", schema_id_, &ctx), Status::OK)
            << ctx.message;
        system_user_id_ = catalog_->getSystemUserId(&ctx);
        ASSERT_NE(system_user_id_, ID{});
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        if (db_)
        {
            db_->close();
            db_.reset();
            catalog_ = nullptr;
        }
        std::remove(db_path_.c_str());
    }

    ID createSimpleTable(const std::string& name)
    {
        CatalogManager::ColumnInfo col{};
        col.column_name = "id";
        col.data_type = static_cast<uint16_t>(DataType::INT64);
        col.nullable = false;

        std::vector<CatalogManager::ColumnInfo> columns{col};
        ID table_id{};
        ErrorContext ctx;
        EXPECT_EQ(catalog_->createTable(schema_id_, name, columns, table_id, 0, &ctx), Status::OK)
            << ctx.message;
        return table_id;
    }
};

TEST_F(CatalogExtensionPublicationSubscriptionContractTest, Cat028CatalogContracts)
{
    ErrorContext ctx;

    CatalogManager::ExtensionCatalogInfo extension{};
    extension.extension_id = generateUuidV7();
    extension.extension_name = "sb_ext_metrics";
    extension.schema_id = schema_id_;
    extension.version = "1.0.0";
    extension.owner_id = system_user_id_;
    extension.is_relocatable = true;
    ASSERT_EQ(catalog_->upsertExtensionCatalogEntry(extension, &ctx), Status::OK) << ctx.message;

    CatalogManager::ExtensionCatalogInfo extension_out{};
    ASSERT_EQ(catalog_->getExtensionCatalogEntry(extension.extension_id, extension_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(extension_out.extension_name, extension.extension_name);

    CatalogManager::ExtensionCatalogInfo extension_dup = extension;
    extension_dup.extension_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertExtensionCatalogEntry(extension_dup, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::PublicationCatalogInfo publication{};
    publication.publication_id = generateUuidV7();
    publication.publication_name = "cat028_pub";
    publication.owner_id = system_user_id_;
    publication.publish_insert = true;
    publication.publish_update = true;
    publication.publish_delete = true;
    publication.publish_truncate = true;
    publication.publish_via_partition_root = false;
    ASSERT_EQ(catalog_->upsertPublicationCatalogEntry(publication, &ctx), Status::OK) << ctx.message;

    CatalogManager::PublicationCatalogInfo publication_out{};
    ASSERT_EQ(catalog_->getPublicationCatalogEntry(publication.publication_id, publication_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(publication_out.publication_name, publication.publication_name);

    CatalogManager::PublicationCatalogInfo publication_dup = publication;
    publication_dup.publication_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertPublicationCatalogEntry(publication_dup, &ctx), Status::CONSTRAINT_VIOLATION);

    ID table_a = createSimpleTable("cat028_table_a");
    ID table_b = createSimpleTable("cat028_table_b");

    CatalogManager::PublicationTableCatalogInfo pub_table{};
    pub_table.publication_table_id = generateUuidV7();
    pub_table.publication_id = publication.publication_id;
    pub_table.table_id = table_a;
    pub_table.has_column_list_id = true;
    pub_table.column_list_id = generateUuidV7();
    pub_table.has_where_expr_sblr_id = true;
    pub_table.where_expr_sblr_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertPublicationTableCatalogEntry(pub_table, &ctx), Status::OK) << ctx.message;

    CatalogManager::PublicationTableCatalogInfo pub_table_out{};
    ASSERT_EQ(catalog_->getPublicationTableCatalogEntry(pub_table.publication_table_id, pub_table_out, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(pub_table_out.table_id, table_a);

    CatalogManager::PublicationTableCatalogInfo pub_table_dup = pub_table;
    pub_table_dup.publication_table_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertPublicationTableCatalogEntry(pub_table_dup, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::PublicationSchemaCatalogInfo pub_schema{};
    pub_schema.publication_schema_id = generateUuidV7();
    pub_schema.publication_id = publication.publication_id;
    pub_schema.schema_id = schema_id_;
    ASSERT_EQ(catalog_->upsertPublicationSchemaCatalogEntry(pub_schema, &ctx), Status::OK) << ctx.message;

    CatalogManager::PublicationSchemaCatalogInfo pub_schema_dup = pub_schema;
    pub_schema_dup.publication_schema_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertPublicationSchemaCatalogEntry(pub_schema_dup, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::SubscriptionCatalogInfo subscription{};
    subscription.subscription_id = generateUuidV7();
    subscription.subscription_name = "cat028_sub";
    subscription.owner_id = system_user_id_;
    subscription.has_connection_info_id = true;
    subscription.connection_info_id = generateUuidV7();
    subscription.enabled = true;
    subscription.has_slot_name = true;
    subscription.slot_name = "cat028_slot";
    subscription.sync_commit = true;
    subscription.copy_data = true;
    subscription.create_slot = true;
    subscription.refresh_on_start = true;
    ASSERT_EQ(catalog_->upsertSubscriptionCatalogEntry(subscription, &ctx), Status::OK) << ctx.message;

    CatalogManager::SubscriptionCatalogInfo subscription_out{};
    ASSERT_EQ(catalog_->getSubscriptionCatalogEntry(subscription.subscription_id, subscription_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(subscription_out.subscription_name, subscription.subscription_name);

    CatalogManager::SubscriptionCatalogInfo subscription_dup = subscription;
    subscription_dup.subscription_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertSubscriptionCatalogEntry(subscription_dup, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::SubscriptionTableCatalogInfo sub_table{};
    sub_table.subscription_table_id = generateUuidV7();
    sub_table.subscription_id = subscription.subscription_id;
    sub_table.table_id = table_b;
    sub_table.state = CatalogManager::SubscriptionTableState::READY;
    ASSERT_EQ(catalog_->upsertSubscriptionTableCatalogEntry(sub_table, &ctx), Status::OK) << ctx.message;

    CatalogManager::SubscriptionTableCatalogInfo sub_table_out{};
    ASSERT_EQ(catalog_->getSubscriptionTableCatalogEntry(sub_table.subscription_table_id, sub_table_out, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(sub_table_out.state, CatalogManager::SubscriptionTableState::READY);

    CatalogManager::SubscriptionTableCatalogInfo invalid_sub_table = sub_table;
    invalid_sub_table.subscription_table_id = generateUuidV7();
    invalid_sub_table.state = static_cast<CatalogManager::SubscriptionTableState>(99);
    EXPECT_EQ(catalog_->upsertSubscriptionTableCatalogEntry(invalid_sub_table, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::SubscriptionTableCatalogInfo invalid_error_text = sub_table;
    invalid_error_text.subscription_table_id = generateUuidV7();
    invalid_error_text.has_last_error = true;
    invalid_error_text.last_error.clear();
    EXPECT_EQ(catalog_->upsertSubscriptionTableCatalogEntry(invalid_error_text, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::SubscriptionTableCatalogInfo sub_table_dup = sub_table;
    sub_table_dup.subscription_table_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertSubscriptionTableCatalogEntry(sub_table_dup, &ctx), Status::CONSTRAINT_VIOLATION);

    std::vector<CatalogManager::ExtensionCatalogInfo> extension_rows;
    ASSERT_EQ(catalog_->listExtensionCatalogEntries(extension_rows, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(extension_rows.size(), 1u);

    std::vector<CatalogManager::PublicationCatalogInfo> publication_rows;
    ASSERT_EQ(catalog_->listPublicationCatalogEntries(publication_rows, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(publication_rows.size(), 1u);

    std::vector<CatalogManager::PublicationTableCatalogInfo> pub_table_rows;
    ASSERT_EQ(catalog_->listPublicationTableCatalogEntries(publication.publication_id, pub_table_rows, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(pub_table_rows.size(), 1u);

    std::vector<CatalogManager::PublicationSchemaCatalogInfo> pub_schema_rows;
    ASSERT_EQ(catalog_->listPublicationSchemaCatalogEntries(publication.publication_id, pub_schema_rows, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(pub_schema_rows.size(), 1u);

    std::vector<CatalogManager::SubscriptionCatalogInfo> subscription_rows;
    ASSERT_EQ(catalog_->listSubscriptionCatalogEntries(subscription_rows, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(subscription_rows.size(), 1u);

    std::vector<CatalogManager::SubscriptionTableCatalogInfo> sub_table_rows;
    ASSERT_EQ(catalog_->listSubscriptionTableCatalogEntries(subscription.subscription_id, sub_table_rows, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(sub_table_rows.size(), 1u);
}
