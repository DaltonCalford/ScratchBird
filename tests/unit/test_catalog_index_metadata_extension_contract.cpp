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

class CatalogIndexMetadataExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
    ID schema_id_{};

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_index_metadata_extension_contract_" +
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

        ASSERT_EQ(catalog_->createSchema("cat016_schema", "system", schema_id_, &ctx), Status::OK)
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

    ID createType(const std::string& type_name)
    {
        CatalogManager::TypeCatalogInfo info{};
        info.schema_id = schema_id_;
        info.type_name = type_name;
        info.type_kind = CatalogManager::TypeKind::SCALAR;

        ErrorContext ctx;
        ID type_id{};
        EXPECT_EQ(catalog_->upsertTypeCatalogEntry(info, type_id, &ctx), Status::OK) << ctx.message;
        return type_id;
    }

    void createTableAndIndex(ID& table_id_out, ID& index_id_out, ID& id_column_id_out, ID& value_column_id_out)
    {
        CatalogManager::ColumnInfo id_col{};
        id_col.column_name = "id";
        id_col.data_type = static_cast<uint16_t>(DataType::INT64);
        id_col.nullable = false;

        CatalogManager::ColumnInfo value_col{};
        value_col.column_name = "value";
        value_col.data_type = static_cast<uint16_t>(DataType::INT64);
        value_col.nullable = true;

        std::vector<CatalogManager::ColumnInfo> columns{id_col, value_col};

        ErrorContext ctx;
        ASSERT_EQ(catalog_->createTable(schema_id_, "cat016_table", columns, table_id_out, 0, &ctx),
                  Status::OK)
            << ctx.message;

        ASSERT_EQ(catalog_->createIndex(table_id_out,
                                        "cat016_idx",
                                        std::vector<std::string>{"id"},
                                        index_id_out,
                                        false,
                                        CatalogManager::IndexType::BTREE,
                                        0,
                                        &ctx),
                  Status::OK)
            << ctx.message;

        std::vector<CatalogManager::ColumnInfo> fetched_columns;
        ASSERT_EQ(catalog_->getColumns(table_id_out, fetched_columns, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(fetched_columns.size(), 2u);
        for (const auto& col : fetched_columns)
        {
            if (col.column_name == "id")
            {
                id_column_id_out = col.column_id;
            }
            else if (col.column_name == "value")
            {
                value_column_id_out = col.column_id;
            }
        }
        ASSERT_NE(id_column_id_out, ID{});
        ASSERT_NE(value_column_id_out, ID{});
    }
};

TEST_F(CatalogIndexMetadataExtensionContractTest, AccessMethodOpclassAndColumnContracts)
{
    ErrorContext ctx;
    ID table_id{};
    ID index_id{};
    ID id_column_id{};
    ID value_column_id{};
    createTableAndIndex(table_id, index_id, id_column_id, value_column_id);

    CatalogManager::IndexAccessMethodCatalogInfo invalid_access_method{};
    ID access_method_id{};
    EXPECT_EQ(catalog_->upsertIndexAccessMethodCatalogEntry(invalid_access_method, access_method_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::IndexAccessMethodCatalogInfo access_method{};
    access_method.method_name = "am_btree_native";
    access_method.index_type_name = "BTREE";
    access_method.supports_unique = true;
    access_method.supports_multicolumn = true;
    access_method.supports_order = true;
    access_method.supports_nulls_order = true;
    access_method.default_fillfactor = 90;
    ASSERT_EQ(catalog_->upsertIndexAccessMethodCatalogEntry(access_method, access_method_id, &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::IndexAccessMethodCatalogInfo duplicate_access_method = access_method;
    duplicate_access_method.access_method_id = generateUuidV7();
    ID duplicate_access_method_id{};
    EXPECT_EQ(catalog_->upsertIndexAccessMethodCatalogEntry(
                  duplicate_access_method, duplicate_access_method_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    ID input_type_id = createType("cat016_numeric");

    CatalogManager::IndexOpclassCatalogInfo invalid_opclass{};
    invalid_opclass.opclass_name = "btree_int8_ops";
    invalid_opclass.index_type_name = "BTREE";
    invalid_opclass.owner_schema_id = schema_id_;
    ID opclass_id{};
    EXPECT_EQ(catalog_->upsertIndexOpclassCatalogEntry(invalid_opclass, opclass_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::IndexOpclassCatalogInfo opclass{};
    opclass.opclass_name = "btree_int8_ops";
    opclass.index_type_name = "BTREE";
    opclass.input_type_id = input_type_id;
    opclass.owner_schema_id = schema_id_;
    ASSERT_EQ(catalog_->upsertIndexOpclassCatalogEntry(opclass, opclass_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::IndexOpclassFunctionCatalogInfo invalid_fn{};
    invalid_fn.opclass_id = opclass_id;
    invalid_fn.fn_kind = CatalogManager::IndexOpclassFunctionKind::COMPARE;
    ID opclass_fn_id{};
    EXPECT_EQ(catalog_->upsertIndexOpclassFunctionCatalogEntry(invalid_fn, opclass_fn_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::IndexOpclassFunctionCatalogInfo fn{};
    fn.opclass_id = opclass_id;
    fn.fn_kind = CatalogManager::IndexOpclassFunctionKind::COMPARE;
    fn.function_id = generateUuidV7();
    fn.support_number = 1;
    ASSERT_EQ(catalog_->upsertIndexOpclassFunctionCatalogEntry(fn, opclass_fn_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::IndexOpclassFunctionCatalogInfo duplicate_fn = fn;
    duplicate_fn.opclass_fn_id = generateUuidV7();
    ID duplicate_fn_id{};
    EXPECT_EQ(catalog_->upsertIndexOpclassFunctionCatalogEntry(duplicate_fn, duplicate_fn_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::IndexColumnCatalogInfo key_col{};
    key_col.index_id = index_id;
    key_col.position = 1;
    key_col.column_id = id_column_id;
    key_col.opclass_id = opclass_id;
    key_col.sort_order = CatalogManager::IndexSortOrder::ASC;
    key_col.null_order = CatalogManager::IndexNullOrder::LAST;
    ID key_col_id{};
    ASSERT_EQ(catalog_->upsertIndexColumnCatalogEntry(key_col, key_col_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::IndexColumnCatalogInfo include_col{};
    include_col.index_id = index_id;
    include_col.position = 2;
    include_col.column_id = value_column_id;
    include_col.is_include = true;
    include_col.sort_order = CatalogManager::IndexSortOrder::ASC;
    include_col.null_order = CatalogManager::IndexNullOrder::LAST;
    ID include_col_id{};
    ASSERT_EQ(catalog_->upsertIndexColumnCatalogEntry(include_col, include_col_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::IndexColumnCatalogInfo bad_mixed{};
    bad_mixed.index_id = index_id;
    bad_mixed.position = 3;
    bad_mixed.column_id = id_column_id;
    bad_mixed.expression_sblr_id = generateUuidV7();
    bad_mixed.sort_order = CatalogManager::IndexSortOrder::ASC;
    bad_mixed.null_order = CatalogManager::IndexNullOrder::LAST;
    ID bad_mixed_id{};
    EXPECT_EQ(catalog_->upsertIndexColumnCatalogEntry(bad_mixed, bad_mixed_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::IndexColumnCatalogInfo late_key{};
    late_key.index_id = index_id;
    late_key.position = 3;
    late_key.column_id = id_column_id;
    late_key.sort_order = CatalogManager::IndexSortOrder::ASC;
    late_key.null_order = CatalogManager::IndexNullOrder::LAST;
    ID late_key_id{};
    EXPECT_EQ(catalog_->upsertIndexColumnCatalogEntry(late_key, late_key_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    std::vector<CatalogManager::IndexColumnCatalogInfo> columns;
    ASSERT_EQ(catalog_->listIndexColumnCatalogEntries(index_id, columns, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(columns.size(), 2u);
    EXPECT_EQ(columns[0].position, 1u);
    EXPECT_EQ(columns[1].position, 2u);
}

TEST_F(CatalogIndexMetadataExtensionContractTest, OptionAndMaintenanceDeltaContracts)
{
    ErrorContext ctx;
    ID table_id{};
    ID index_id{};
    ID id_column_id{};
    ID value_column_id{};
    createTableAndIndex(table_id, index_id, id_column_id, value_column_id);

    CatalogManager::IndexOptionCatalogInfo invalid_option{};
    invalid_option.index_id = index_id;
    ID option_id{};
    EXPECT_EQ(catalog_->upsertIndexOptionCatalogEntry(invalid_option, option_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::IndexOptionCatalogInfo option{};
    option.index_id = index_id;
    option.option_key = "m";
    option.option_value = "16";
    option.option_type = CatalogManager::IndexOptionValueType::INT;
    ASSERT_EQ(catalog_->upsertIndexOptionCatalogEntry(option, option_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::IndexOptionCatalogInfo duplicate_option = option;
    duplicate_option.option_id = generateUuidV7();
    ID duplicate_option_id{};
    EXPECT_EQ(catalog_->upsertIndexOptionCatalogEntry(duplicate_option, duplicate_option_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::IndexMaintenanceCatalogInfo maintenance{};
    maintenance.index_id = index_id;
    maintenance.maintenance_kind = CatalogManager::IndexMaintenanceKind::REBUILD;
    maintenance.maintenance_mode = CatalogManager::IndexMaintenanceMode::OFFLINE;
    maintenance.maintenance_state = CatalogManager::IndexMaintenanceState::BUILDING_SHADOW;
    maintenance.started_txid = 42;
    ID maintenance_id{};
    ASSERT_EQ(catalog_->upsertIndexMaintenanceCatalogEntry(maintenance, maintenance_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::IndexMaintenanceCatalogInfo second_active = maintenance;
    second_active.maintenance_id = generateUuidV7();
    second_active.started_txid = 43;
    ID second_active_id{};
    EXPECT_EQ(catalog_->upsertIndexMaintenanceCatalogEntry(second_active, second_active_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::IndexMaintenanceCatalogInfo finished = maintenance;
    finished.maintenance_id = generateUuidV7();
    finished.maintenance_state = CatalogManager::IndexMaintenanceState::COMPLETE;
    finished.started_txid = 44;
    ID finished_id{};
    ASSERT_EQ(catalog_->upsertIndexMaintenanceCatalogEntry(finished, finished_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::IndexMaintenanceDeltaCatalogInfo maintenance_delta{};
    maintenance_delta.maintenance_id = maintenance_id;
    maintenance_delta.delta_id = 1;
    maintenance_delta.delta_op = CatalogManager::IndexDeltaOp::INSERT;
    maintenance_delta.tid_gpid = 101;
    maintenance_delta.tid_slot = 3;
    maintenance_delta.commit_txid = 1001;
    ID maintenance_delta_id{};
    ASSERT_EQ(catalog_->upsertIndexMaintenanceDeltaCatalogEntry(
                  maintenance_delta, maintenance_delta_id, &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::IndexMaintenanceDeltaCatalogInfo duplicate_delta = maintenance_delta;
    duplicate_delta.maintenance_delta_id = generateUuidV7();
    ID duplicate_delta_id{};
    EXPECT_EQ(catalog_->upsertIndexMaintenanceDeltaCatalogEntry(duplicate_delta, duplicate_delta_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::IndexBuildDeltaCatalogInfo invalid_build_delta{};
    invalid_build_delta.index_id = index_id;
    invalid_build_delta.delta_id = 1;
    invalid_build_delta.delta_op = CatalogManager::IndexDeltaOp::INSERT;
    ID build_delta_id{};
    EXPECT_EQ(catalog_->upsertIndexBuildDeltaCatalogEntry(invalid_build_delta, build_delta_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::IndexBuildDeltaCatalogInfo build_delta = invalid_build_delta;
    build_delta.key_bytes_id = generateUuidV7();
    build_delta.created_txid = 2001;
    ASSERT_EQ(catalog_->upsertIndexBuildDeltaCatalogEntry(build_delta, build_delta_id, &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::IndexBuildDeltaCatalogInfo duplicate_build_delta = build_delta;
    duplicate_build_delta.build_delta_id = generateUuidV7();
    ID duplicate_build_delta_id{};
    EXPECT_EQ(catalog_->upsertIndexBuildDeltaCatalogEntry(
                  duplicate_build_delta, duplicate_build_delta_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    std::vector<CatalogManager::IndexMaintenanceCatalogInfo> maintenance_rows;
    ASSERT_EQ(catalog_->listIndexMaintenanceCatalogEntries(index_id, maintenance_rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(maintenance_rows.size(), 2u);

    std::vector<CatalogManager::IndexBuildDeltaCatalogInfo> build_rows;
    ASSERT_EQ(catalog_->listIndexBuildDeltaCatalogEntries(index_id, build_rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(build_rows.size(), 1u);
}
