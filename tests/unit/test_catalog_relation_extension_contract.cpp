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

class CatalogRelationExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    ID schema_id_{};

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_relation_extension_contract_" + std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());

        ASSERT_EQ(catalog_->createSchema("cat015_schema", "system", schema_id_, &ctx), Status::OK)
            << ctx.message;
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

    ID createPackage(const std::string& name)
    {
        ID package_id{};
        ErrorContext ctx;
        EXPECT_EQ(catalog_->createPackage(
                      schema_id_,
                      name,
                      "package header",
                      "package body",
                      package_id,
                      &ctx),
                  Status::OK)
            << ctx.message;
        return package_id;
    }
};

TEST_F(CatalogRelationExtensionContractTest, PartitionAndInheritanceContracts)
{
    ErrorContext ctx;
    ID parent_table_id = createSimpleTable("orders");
    ID child_table_id = createSimpleTable("orders_archive");
    ID partition_table_id = createSimpleTable("orders_2026");

    CatalogManager::PartitionedTableCatalogInfo invalid_partitioned{};
    invalid_partitioned.table_id = parent_table_id;
    invalid_partitioned.strategy = CatalogManager::PartitionStrategy::RANGE;
    ID partitioned_table_id{};
    EXPECT_EQ(catalog_->upsertPartitionedTableCatalogEntry(
                  invalid_partitioned, partitioned_table_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::PartitionedTableCatalogInfo partitioned{};
    partitioned.table_id = parent_table_id;
    partitioned.strategy = CatalogManager::PartitionStrategy::RANGE;
    partitioned.key_columns_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertPartitionedTableCatalogEntry(
                  partitioned, partitioned_table_id, &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::PartitionedTableCatalogInfo duplicate_partitioned = partitioned;
    duplicate_partitioned.partitioned_table_id = generateUuidV7();
    ID duplicate_partitioned_id{};
    EXPECT_EQ(catalog_->upsertPartitionedTableCatalogEntry(
                  duplicate_partitioned, duplicate_partitioned_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::PartitionCatalogInfo invalid_partition{};
    invalid_partition.parent_table_id = parent_table_id;
    invalid_partition.partition_table_id = partition_table_id;
    invalid_partition.partition_name = "p_bad_range";
    invalid_partition.bound_kind = CatalogManager::PartitionBoundKind::RANGE;
    invalid_partition.range_min_bytes = std::string("2026-01-01");
    ID partition_id{};
    EXPECT_EQ(catalog_->upsertPartitionCatalogEntry(invalid_partition, partition_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::PartitionCatalogInfo range_partition = invalid_partition;
    range_partition.partition_name = "p_2026";
    range_partition.range_max_bytes = std::string("2027-01-01");
    ASSERT_EQ(catalog_->upsertPartitionCatalogEntry(range_partition, partition_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::PartitionCatalogInfo duplicate_partition = range_partition;
    duplicate_partition.partition_id = generateUuidV7();
    ID duplicate_partition_id{};
    EXPECT_EQ(catalog_->upsertPartitionCatalogEntry(duplicate_partition, duplicate_partition_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::TableInheritanceCatalogInfo inheritance{};
    inheritance.parent_table_id = parent_table_id;
    inheritance.child_table_id = child_table_id;
    inheritance.inheritance_kind = CatalogManager::InheritanceKind::INHERITS;
    ID inheritance_id{};
    ASSERT_EQ(catalog_->upsertTableInheritanceCatalogEntry(inheritance, inheritance_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::TableInheritanceCatalogInfo duplicate_inheritance = inheritance;
    duplicate_inheritance.inheritance_id = generateUuidV7();
    ID duplicate_inheritance_id{};
    EXPECT_EQ(catalog_->upsertTableInheritanceCatalogEntry(
                  duplicate_inheritance, duplicate_inheritance_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::PartitionedTableCatalogInfo fetched_partitioned{};
    ASSERT_EQ(catalog_->getPartitionedTableCatalogEntryByTable(
                  parent_table_id, fetched_partitioned, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(fetched_partitioned.partitioned_table_id, partitioned_table_id);

    std::vector<CatalogManager::PartitionCatalogInfo> partitions;
    ASSERT_EQ(catalog_->listPartitionCatalogEntries(parent_table_id, partitions, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(partitions.size(), 1u);
}

TEST_F(CatalogRelationExtensionContractTest, LanguageEventAndPackageMemberContracts)
{
    ErrorContext ctx;
    ID package_id = createPackage("pkg_alpha");

    CatalogManager::LanguageCatalogInfo invalid_language{};
    ID language_id{};
    EXPECT_EQ(catalog_->upsertLanguageCatalogEntry(invalid_language, language_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::LanguageCatalogInfo language{};
    language.language_name = "plpgsql";
    language.language_kind = CatalogManager::LanguageKind::PLPGSQL;
    language.owner_id = generateUuidV7();
    language.is_trusted = true;
    ASSERT_EQ(catalog_->upsertLanguageCatalogEntry(language, language_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::LanguageCatalogInfo duplicate_language = language;
    duplicate_language.language_id = generateUuidV7();
    ID duplicate_language_id{};
    EXPECT_EQ(catalog_->upsertLanguageCatalogEntry(duplicate_language, duplicate_language_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::EventCatalogInfo invalid_event{};
    invalid_event.schema_id = schema_id_;
    invalid_event.event_name = "nightly_event";
    invalid_event.definer_id = generateUuidV7();
    invalid_event.status = CatalogManager::EventStatus::ENABLED;
    invalid_event.on_completion = CatalogManager::EventOnCompletion::PRESERVE;
    invalid_event.schedule_kind = CatalogManager::ScheduleKind::CRON;
    invalid_event.body_sblr_id = generateUuidV7();
    ID event_id{};
    EXPECT_EQ(catalog_->upsertEventCatalogEntry(invalid_event, event_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::EventCatalogInfo event = invalid_event;
    event.cron_expr = std::string("0 0 * * *");
    event.comment = std::string("nightly maintenance");
    ASSERT_EQ(catalog_->upsertEventCatalogEntry(event, event_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::EventCatalogInfo duplicate_event = event;
    duplicate_event.event_id = generateUuidV7();
    ID duplicate_event_id{};
    EXPECT_EQ(catalog_->upsertEventCatalogEntry(duplicate_event, duplicate_event_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::PackageMemberCatalogInfo invalid_member{};
    invalid_member.package_id = package_id;
    invalid_member.procedure_id = generateUuidV7();
    ID member_id{};
    EXPECT_EQ(catalog_->upsertPackageMemberCatalogEntry(invalid_member, member_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::PackageMemberCatalogInfo member = invalid_member;
    member.member_name = "run_task";
    member.member_kind = CatalogManager::PackageMemberKind::PROCEDURE;
    member.position = 1;
    member.is_public = true;
    ASSERT_EQ(catalog_->upsertPackageMemberCatalogEntry(member, member_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::PackageMemberCatalogInfo duplicate_member = member;
    duplicate_member.member_id = generateUuidV7();
    ID duplicate_member_id{};
    EXPECT_EQ(catalog_->upsertPackageMemberCatalogEntry(duplicate_member, duplicate_member_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::LanguageCatalogInfo fetched_language{};
    ASSERT_EQ(catalog_->getLanguageCatalogEntryByName("PLPGSQL", fetched_language, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(fetched_language.language_id, language_id);

    std::vector<CatalogManager::EventCatalogInfo> events;
    ASSERT_EQ(catalog_->listEventCatalogEntries(schema_id_, events, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(events.size(), 1u);

    std::vector<CatalogManager::PackageMemberCatalogInfo> members;
    ASSERT_EQ(catalog_->listPackageMemberCatalogEntries(package_id, members, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(members.size(), 1u);
}
