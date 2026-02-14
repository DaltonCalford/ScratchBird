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

class CatalogEngineSpecificExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    ID schema_id_{};
    ID table_id_{};
    ID trigger_id_{};
    ID owner_id_{};
    ID type_id_{};

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_engine_specific_extension_contract_" + std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());

        owner_id_ = catalog_->getSystemUserId(&ctx);
        ASSERT_NE(owner_id_, ID{});

        ASSERT_EQ(catalog_->createSchema("cat032_schema", "system", schema_id_, &ctx), Status::OK)
            << ctx.message;

        CatalogManager::ColumnInfo c1{};
        c1.column_name = "id";
        c1.data_type = static_cast<uint16_t>(DataType::INT64);
        c1.nullable = false;
        CatalogManager::ColumnInfo c2{};
        c2.column_name = "payload";
        c2.data_type = static_cast<uint16_t>(DataType::VARCHAR);
        c2.type_precision = 128;
        c2.max_length = 128;
        c2.nullable = true;
        std::vector<CatalogManager::ColumnInfo> columns{c1, c2};
        ASSERT_EQ(catalog_->createTable(schema_id_, "cat032_table", columns, table_id_, 0, &ctx), Status::OK)
            << ctx.message;

        CatalogManager::TriggerInfo trigger{};
        trigger.trigger_name = "cat032_trg";
        trigger.table_id = table_id_;
        trigger.table_name = "cat032_table";
        trigger.timing = CatalogManager::TriggerTiming::BEFORE;
        trigger.event_mask = 1u << static_cast<uint8_t>(CatalogManager::TriggerEvent::INSERT);
        trigger.granularity = CatalogManager::TriggerGranularity::FOR_EACH_ROW;
        trigger.procedure_name = "";
        ASSERT_EQ(catalog_->createTrigger(trigger, &ctx), Status::OK) << ctx.message;

        std::vector<CatalogManager::TriggerInfo> triggers;
        ASSERT_EQ(catalog_->listAllTriggersForTable(table_id_, triggers, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(triggers.size(), 1u);
        trigger_id_ = triggers[0].trigger_id;
        ASSERT_NE(trigger_id_, ID{});

        CatalogManager::TypeCatalogInfo type_info{};
        type_info.schema_id = schema_id_;
        type_info.type_name = "cat032_type";
        type_info.type_kind = CatalogManager::TypeKind::SCALAR;
        ASSERT_EQ(catalog_->upsertTypeCatalogEntry(type_info, type_id_, &ctx), Status::OK) << ctx.message;
        ASSERT_NE(type_id_, ID{});
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
};

TEST_F(CatalogEngineSpecificExtensionContractTest, EngineSpecificCatalogContracts)
{
    ErrorContext ctx;

    CatalogManager::BlobFilterCatalogInfo filter{};
    filter.filter_id = generateUuidV7();
    filter.filter_name = "cat032_blob_filter";
    filter.input_subtype = -2;
    filter.output_subtype = 1;
    filter.entry_point = "entry_fn";
    filter.module_name = "mod_lib";
    filter.owner_id = owner_id_;
    ASSERT_EQ(catalog_->upsertBlobFilterCatalogEntry(filter, &ctx), Status::OK) << ctx.message;

    CatalogManager::BlobFilterCatalogInfo filter_dup = filter;
    filter_dup.filter_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertBlobFilterCatalogEntry(filter_dup, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::BlobFilterCatalogInfo filter_out{};
    ASSERT_EQ(catalog_->getBlobFilterCatalogEntry(filter.filter_id, filter_out, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(filter_out.filter_name, filter.filter_name);
    EXPECT_EQ(filter_out.input_subtype, filter.input_subtype);
    EXPECT_EQ(filter_out.output_subtype, filter.output_subtype);

    CatalogManager::TriggerMessageCatalogInfo message{};
    message.message_id = generateUuidV7();
    message.trigger_id = trigger_id_;
    message.message_number = 10;
    message.message_text = "cat032 trigger message";
    ASSERT_EQ(catalog_->upsertTriggerMessageCatalogEntry(message, &ctx), Status::OK) << ctx.message;

    CatalogManager::TriggerMessageCatalogInfo message_dup = message;
    message_dup.message_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertTriggerMessageCatalogEntry(message_dup, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::TriggerMessageCatalogInfo message_out{};
    ASSERT_EQ(catalog_->getTriggerMessageCatalogEntry(message.message_id, message_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(message_out.message_text, message.message_text);
    EXPECT_NE(message_out.message_text_oid, ID{});

    std::vector<CatalogManager::TriggerMessageCatalogInfo> trigger_messages;
    ASSERT_EQ(catalog_->listTriggerMessageCatalogEntries(trigger_id_, trigger_messages, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(trigger_messages.size(), 1u);
    EXPECT_EQ(trigger_messages[0].message_number, 10);

    CatalogManager::TriggerMessageCatalogInfo missing_trigger{};
    missing_trigger.message_id = generateUuidV7();
    missing_trigger.trigger_id = generateUuidV7();
    missing_trigger.message_number = 11;
    missing_trigger.message_text = "bad";
    EXPECT_EQ(catalog_->upsertTriggerMessageCatalogEntry(missing_trigger, &ctx), Status::NOT_FOUND);

    CatalogManager::ColumnDropHistoryCatalogInfo history{};
    history.history_id = generateUuidV7();
    history.table_id = table_id_;
    history.column_name = "old_column";
    history.column_type_id = type_id_;
    history.dropped_time = 123456789;
    history.has_dropped_by = true;
    history.dropped_by_id = owner_id_;
    ASSERT_EQ(catalog_->upsertColumnDropHistoryCatalogEntry(history, &ctx), Status::OK) << ctx.message;

    CatalogManager::ColumnDropHistoryCatalogInfo history_dup = history;
    history_dup.history_id = generateUuidV7();
    EXPECT_EQ(catalog_->upsertColumnDropHistoryCatalogEntry(history_dup, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::ColumnDropHistoryCatalogInfo history_out{};
    ASSERT_EQ(catalog_->getColumnDropHistoryCatalogEntry(history.history_id, history_out, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(history_out.column_name, history.column_name);
    EXPECT_EQ(history_out.column_type_id, history.column_type_id);
    EXPECT_EQ(history_out.dropped_time, history.dropped_time);
    EXPECT_TRUE(history_out.has_dropped_by);
    EXPECT_EQ(history_out.dropped_by_id, owner_id_);

    std::vector<CatalogManager::ColumnDropHistoryCatalogInfo> history_rows;
    ASSERT_EQ(catalog_->listColumnDropHistoryCatalogEntries(table_id_, history_rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(history_rows.size(), 1u);
    EXPECT_EQ(history_rows[0].column_name, history.column_name);

    CatalogManager::ColumnDropHistoryCatalogInfo bad_type = history;
    bad_type.history_id = generateUuidV7();
    bad_type.column_name = "other_column";
    bad_type.column_type_id = generateUuidV7();
    bad_type.dropped_time = history.dropped_time + 1;
    EXPECT_EQ(catalog_->upsertColumnDropHistoryCatalogEntry(bad_type, &ctx), Status::NOT_FOUND);

    ASSERT_EQ(catalog_->deleteColumnDropHistoryCatalogEntry(history.history_id, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(catalog_->getColumnDropHistoryCatalogEntry(history.history_id, history_out, &ctx), Status::NOT_FOUND);

    ASSERT_EQ(catalog_->deleteTriggerMessageCatalogEntry(message.message_id, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(catalog_->getTriggerMessageCatalogEntry(message.message_id, message_out, &ctx), Status::NOT_FOUND);

    ASSERT_EQ(catalog_->deleteBlobFilterCatalogEntry(filter.filter_id, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(catalog_->getBlobFilterCatalogEntry(filter.filter_id, filter_out, &ctx), Status::NOT_FOUND);
}
