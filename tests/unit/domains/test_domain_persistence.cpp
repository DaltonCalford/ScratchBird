/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/ondisk.h"
#include "unit/test_user_helpers.h"
#include "gtest/gtest.h"
#include <cstdio>

using namespace scratchbird::core;

namespace {
auto countCatalogHeapRecords(Database& db, uint32_t page_id, uint32_t& total_records,
                             ErrorContext* ctx) -> Status
{
    if (page_id == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "catalog heap root page is zero");
        return Status::PAGE_CORRUPT;
    }

    BufferPool* bp = db.buffer_pool();
    if (bp == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "buffer pool unavailable");
        return Status::INVALID_ARGUMENT;
    }

    total_records = 0;
    uint32_t current_page_id = page_id;
    while (current_page_id != 0)
    {
        void* page_buffer = nullptr;
        Status status = bp->pinPage(current_page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        const auto* heap = reinterpret_cast<const CatalogHeapPage*>(page_buffer);
        total_records += heap->record_count;
        uint32_t next_page = heap->next_page;

        status = bp->unpinPage(current_page_id, false, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        current_page_id = next_page;
    }

    return Status::OK;
}
} // namespace

TEST(DomainPersistenceTest, ReloadsDomainMetadata)
{
    const char* test_db = "test_domain_persistence.sbdb";
    std::remove(test_db);

    ErrorContext ctx;
    Status status = Database::create(test_db, 16384, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    Database db;
    status = db.open(test_db, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    auto* catalog = db.catalog_manager();
    ASSERT_NE(catalog, nullptr);
    EnsureUser(catalog, "test_user");

    std::string raw_schema_heap_layout_before_create;
    status = catalog->rawSchemaHeapLayoutForTesting(raw_schema_heap_layout_before_create, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    const uint32_t schema_catalog_page_before_create = catalog->schemaCatalogPageForTesting();
    const uint32_t recovery_run_catalog_page_before_create =
        catalog->recoveryRunCatalogPageForTesting();

    uint32_t schema_records_before = 0;
    status = countCatalogHeapRecords(db, catalog->schemaCatalogPageForTesting(),
                                     schema_records_before, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    ID schema_id;
    status = catalog->createSchema("test_schema", "test_user", schema_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    uint32_t schema_records_after_create = 0;
    status = countCatalogHeapRecords(db, catalog->schemaCatalogPageForTesting(),
                                     schema_records_after_create, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_EQ(schema_records_after_create, schema_records_before + 1);

    CatalogManager::SchemaInfo sys_schema;
    status = catalog->getSchema("sys", sys_schema, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    bool raw_sys_schema_present = false;
    status = catalog->rawSchemaRecordExistsForTesting(sys_schema.schema_id,
                                                      raw_sys_schema_present, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_TRUE(raw_sys_schema_present);

    bool raw_created_schema_present = false;
    status = catalog->rawSchemaRecordExistsForTesting(schema_id, raw_created_schema_present, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    bool raw_schema_name_found = false;
    ID raw_schema_name_id;
    uint32_t raw_schema_name_valid = 0;
    uint32_t raw_schema_name_page_id = 0;
    uint32_t raw_schema_name_slot_index = 0;
    status = catalog->rawSchemaRecordByNameForTesting("test_schema",
                                                      raw_schema_name_found,
                                                      raw_schema_name_id,
                                                      raw_schema_name_valid,
                                                      raw_schema_name_page_id,
                                                      raw_schema_name_slot_index,
                                                      &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    std::string raw_schema_heap_layout;
    status = catalog->rawSchemaHeapLayoutForTesting(raw_schema_heap_layout, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ID last_raw_schema_id;
    std::string last_raw_schema_name;
    uint32_t last_raw_schema_valid = 0;
    status = catalog->lastRawSchemaRecordForTesting(last_raw_schema_id,
                                                    last_raw_schema_name,
                                                    last_raw_schema_valid,
                                                    &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_TRUE(raw_created_schema_present)
        << " schema_catalog_page_before_create=" << schema_catalog_page_before_create
        << " recovery_run_catalog_page_before_create="
        << recovery_run_catalog_page_before_create
        << " raw_schema_heap_layout_before_create=" << raw_schema_heap_layout_before_create
        << "last_raw_schema_id=" << last_raw_schema_id.toString()
        << " last_raw_schema_name=" << last_raw_schema_name
        << " last_raw_schema_valid=" << last_raw_schema_valid
        << " raw_schema_name_found=" << raw_schema_name_found
        << " raw_schema_name_id=" << raw_schema_name_id.toString()
        << " raw_schema_name_valid=" << raw_schema_name_valid
        << " raw_schema_name_page_id=" << raw_schema_name_page_id
        << " raw_schema_name_slot_index=" << raw_schema_name_slot_index
        << " raw_schema_heap_layout=" << raw_schema_heap_layout;

    auto* dm = db.domain_manager();
    ASSERT_NE(dm, nullptr);

    std::vector<DomainConstraint> constraints;
    constraints.emplace_back(ConstraintType::CHECK, "value > 0", "positive_value");

    ID basic_id;
    DomainManager::DomainCreateOptions options;
    options.nullable = false;
    options.default_value = "";
    options.constraints = constraints;
    options.dialect_tag = "postgresql";
    options.compat_name = "int4";
    status = dm->createBasicDomain(schema_id, "positive_int", DataType::INT32, 0, 0,
                                   options, basic_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    std::vector<RecordField> fields;
    RecordField name_field("name", DataType::TEXT, false);
    fields.push_back(name_field);
    RecordField age_field("age", DataType::INT32, true);
    fields.push_back(age_field);
    RecordField score_field("score", DataType::INT32, true);
    score_field.domain_id = basic_id;
    fields.push_back(score_field);

    ID record_id;
    status = dm->createRecordDomain(schema_id, "person_record", fields, record_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    std::vector<EnumValue> enum_values;
    enum_values.emplace_back("small", 1);
    enum_values.emplace_back("medium", 2);
    enum_values.emplace_back("large", 3);

    ID enum_id;
    status = dm->createEnumDomain(schema_id, "size_enum", enum_values, enum_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    std::vector<DataType> variant_types = {DataType::INT32, DataType::TEXT};

    ID variant_id;
    status = dm->createVariantDomain(schema_id, "flexible_value", variant_types, variant_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    ID domain_table_id;
    std::vector<CatalogManager::ColumnInfo> domain_columns;
    CatalogManager::ColumnInfo domain_col;
    domain_col.column_name = "value";
    domain_col.data_type = static_cast<uint16_t>(DataType::INT32);
    domain_col.domain_id = basic_id;
    domain_col.nullable = false;
    domain_columns.push_back(domain_col);
    status = catalog->createTable(schema_id, "domain_table", domain_columns, domain_table_id, 0, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    bool raw_schema_present = false;
    status = catalog->rawSchemaRecordExistsForTesting(schema_id, raw_schema_present, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_TRUE(raw_schema_present);

    db.close();

    Database db_reopen;
    status = db_reopen.open(test_db, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    auto* dm_reopen = db_reopen.domain_manager();
    ASSERT_NE(dm_reopen, nullptr);

    DomainInfo basic_info;
    status = dm_reopen->getDomain(schema_id, "positive_int", basic_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(basic_info.domain_id, basic_id);
    ASSERT_EQ(basic_info.constraints.size(), 1u);
    EXPECT_EQ(basic_info.constraints[0].type, ConstraintType::CHECK);
    EXPECT_EQ(basic_info.constraints[0].expression, "value > 0");
    EXPECT_EQ(basic_info.constraints[0].name, "positive_value");
    EXPECT_EQ(basic_info.dialect_tag, "postgresql");
    EXPECT_EQ(basic_info.compat_name, "int4");

    DomainInfo record_info;
    status = dm_reopen->getDomain(schema_id, "person_record", record_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(record_info.domain_id, record_id);
    ASSERT_EQ(record_info.fields.size(), 3u);
    EXPECT_EQ(record_info.fields[0].name, "name");
    EXPECT_EQ(record_info.fields[0].type, DataType::TEXT);
    EXPECT_FALSE(record_info.fields[0].nullable);
    EXPECT_EQ(record_info.fields[2].domain_id, basic_id);

    DomainInfo enum_info;
    status = dm_reopen->getDomain(schema_id, "size_enum", enum_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(enum_info.domain_id, enum_id);
    ASSERT_EQ(enum_info.enum_values.size(), 3u);
    EXPECT_EQ(enum_info.enum_values[1].label, "medium");
    EXPECT_EQ(enum_info.enum_values[1].position, 2);

    DomainInfo variant_info;
    status = dm_reopen->getDomain(schema_id, "flexible_value", variant_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(variant_info.domain_id, variant_id);
    ASSERT_EQ(variant_info.variant_allowed_types.size(), 2u);
    EXPECT_EQ(variant_info.variant_allowed_types[0].type, DataType::INT32);
    EXPECT_EQ(variant_info.variant_allowed_types[1].type, DataType::TEXT);

    CatalogManager::ColumnInfo persisted_col;
    status = db_reopen.catalog_manager()->getColumn(domain_table_id, "value", persisted_col, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(persisted_col.domain_id, basic_id);

    db_reopen.close();
    std::remove(test_db);
}
