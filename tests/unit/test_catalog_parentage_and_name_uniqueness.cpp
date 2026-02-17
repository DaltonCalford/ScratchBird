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

class CatalogParentageAndNameUniquenessTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_parentage_" + std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());
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

    ID createSchemaPath(const std::string& path)
    {
        ErrorContext ctx;
        ID schema_id{};
        Status status = catalog_->createSchemaPath(path,
                                                   CatalogManager::SchemaType::APPLICATION,
                                                   schema_id,
                                                   &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return schema_id;
    }

    ID createTable(const ID& schema_id, const std::string& table_name)
    {
        ErrorContext ctx;
        CatalogManager::ColumnInfo id_column{};
        id_column.column_name = "id";
        id_column.data_type = static_cast<uint16_t>(DataType::INT32);
        id_column.max_length = 4;
        id_column.nullable = false;

        ID table_id{};
        Status status = catalog_->createTable(schema_id, table_name, {id_column}, table_id, 0, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return table_id;
    }

    Status createTrigger(const ID& table_id,
                         const std::string& table_name,
                         const std::string& trigger_name,
                         ID& trigger_id_out,
                         ErrorContext* ctx)
    {
        CatalogManager::TriggerInfo trigger{};
        trigger.trigger_id = generateUuidV7();
        trigger.trigger_name = trigger_name;
        trigger.table_id = table_id;
        trigger.table_name = table_name;
        trigger.timing = CatalogManager::TriggerTiming::BEFORE;
        trigger.event_mask = 1u << static_cast<uint8_t>(CatalogManager::TriggerEvent::INSERT);
        trigger.granularity = CatalogManager::TriggerGranularity::FOR_EACH_ROW;
        trigger_id_out = trigger.trigger_id;
        return catalog_->createTrigger(trigger, ctx);
    }
};

TEST_F(CatalogParentageAndNameUniquenessTest, TriggerNameCollisionIsParentScoped)
{
    ID schema_id = createSchemaPath("root.users.public.catalog_parentage");
    ID table_id = createTable(schema_id, "orders");

    ErrorContext ctx;
    ID trigger_a{};
    ASSERT_EQ(createTrigger(table_id, "orders", "orders_bi", trigger_a, &ctx), Status::OK)
        << ctx.message;

    ID trigger_b{};
    Status status = createTrigger(table_id, "orders", "orders_bi", trigger_b, &ctx);
    EXPECT_EQ(status, Status::DUPLICATE_OBJECT);
    EXPECT_NE(ctx.message.find("NAME_COLLISION"), std::string::npos);
}

TEST_F(CatalogParentageAndNameUniquenessTest, SameTriggerNameOnDifferentTablesIsAllowed)
{
    ID schema_id = createSchemaPath("root.users.public.catalog_parentage");
    ID table_a = createTable(schema_id, "events_a");
    ID table_b = createTable(schema_id, "events_b");

    ErrorContext ctx;
    ID trigger_a{};
    ASSERT_EQ(createTrigger(table_a, "events_a", "events_trg", trigger_a, &ctx), Status::OK)
        << ctx.message;

    ID trigger_b{};
    ASSERT_EQ(createTrigger(table_b, "events_b", "events_trg", trigger_b, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::TriggerInfo resolved{};
    Status lookup = catalog_->getTriggerByName("events_trg", resolved, &ctx);
    EXPECT_EQ(lookup, Status::DUPLICATE_OBJECT);
    EXPECT_NE(ctx.message.find("use table scope"), std::string::npos);

    std::vector<CatalogManager::TriggerInfo> table_triggers;
    ASSERT_EQ(catalog_->listAllTriggersForTable(table_a, table_triggers, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(table_triggers.size(), 1u);
    EXPECT_EQ(table_triggers[0].trigger_id, trigger_a);
}

TEST_F(CatalogParentageAndNameUniquenessTest, TriggerListingOrderIsDeterministic)
{
    ID schema_id = createSchemaPath("root.users.public.catalog_parentage");
    ID table_id = createTable(schema_id, "events_det");

    ErrorContext ctx;
    ID trigger_a{};
    ASSERT_EQ(createTrigger(table_id, "events_det", "events_trg_a", trigger_a, &ctx), Status::OK)
        << ctx.message;
    ID trigger_b{};
    ASSERT_EQ(createTrigger(table_id, "events_det", "events_trg_b", trigger_b, &ctx), Status::OK)
        << ctx.message;
    ID trigger_c{};
    ASSERT_EQ(createTrigger(table_id, "events_det", "events_trg_c", trigger_c, &ctx), Status::OK)
        << ctx.message;

    std::vector<CatalogManager::TriggerInfo> triggers;
    ASSERT_EQ(catalog_->listAllTriggersForTable(table_id, triggers, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(triggers.size(), 3u);

    auto is_ordered = [](const CatalogManager::TriggerInfo& lhs,
                         const CatalogManager::TriggerInfo& rhs) {
        if (lhs.created_time != rhs.created_time)
        {
            return lhs.created_time <= rhs.created_time;
        }
        if (lhs.trigger_name != rhs.trigger_name)
        {
            return lhs.trigger_name <= rhs.trigger_name;
        }
        return !(rhs.trigger_id < lhs.trigger_id);
    };

    for (size_t i = 1; i < triggers.size(); ++i)
    {
        EXPECT_TRUE(is_ordered(triggers[i - 1], triggers[i]));
    }

    std::vector<ID> first_order;
    first_order.reserve(triggers.size());
    for (const auto& trigger : triggers)
    {
        first_order.push_back(trigger.trigger_id);
    }

    for (int pass = 0; pass < 5; ++pass)
    {
        std::vector<CatalogManager::TriggerInfo> pass_triggers;
        ASSERT_EQ(catalog_->listTriggersForTable(table_id,
                                                 CatalogManager::TriggerEvent::INSERT,
                                                 CatalogManager::TriggerTiming::BEFORE,
                                                 pass_triggers,
                                                 &ctx),
                  Status::OK) << ctx.message;
        ASSERT_EQ(pass_triggers.size(), first_order.size());
        for (size_t i = 0; i < pass_triggers.size(); ++i)
        {
            EXPECT_EQ(pass_triggers[i].trigger_id, first_order[i]);
        }
    }
}

TEST_F(CatalogParentageAndNameUniquenessTest, IndexNameCollisionIsParentScoped)
{
    ID schema_id = createSchemaPath("root.users.public.catalog_parentage");
    ID table_a = createTable(schema_id, "customer_a");
    ID table_b = createTable(schema_id, "customer_b");

    ErrorContext ctx;
    ID idx_a{};
    ASSERT_EQ(catalog_->createIndex(table_a, "idx_shared", {"id"}, idx_a, false,
                                    CatalogManager::IndexType::BTREE, 0, &ctx),
              Status::OK)
        << ctx.message;

    ID idx_dup{};
    Status duplicate = catalog_->createIndex(table_a, "idx_shared", {"id"}, idx_dup, false,
                                             CatalogManager::IndexType::BTREE, 0, &ctx);
    EXPECT_EQ(duplicate, Status::INVALID_ARGUMENT);

    ID idx_b{};
    ASSERT_EQ(catalog_->createIndex(table_b, "idx_shared", {"id"}, idx_b, false,
                                    CatalogManager::IndexType::BTREE, 0, &ctx),
              Status::OK)
        << ctx.message;
}
