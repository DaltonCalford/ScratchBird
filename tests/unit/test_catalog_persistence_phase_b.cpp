#include <gtest/gtest.h>
#include <filesystem>
#include <memory>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/types.h"
#include "test_helpers.h"

using namespace scratchbird::core;

namespace {

class CatalogPersistencePhaseBTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_path_ =
            scratchbird::testing::uniqueTestDbPath("test_catalog_persistence_phase_b", ".db");
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }

        db_ = std::make_unique<Database>();
        Status status = db_->create(test_db_path_, 16384, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to create database";

        status = db_->open(test_db_path_, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to open database";
    }

    void TearDown() override
    {
        if (db_)
        {
            db_->close();
            db_.reset();
        }

        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }
    }

    std::unique_ptr<Database> db_;
    std::string test_db_path_;
};

TEST_F(CatalogPersistencePhaseBTest, ConstraintsPersistAcrossRestart)
{
    auto *catalog = db_->catalog_manager();
    ASSERT_NE(catalog, nullptr);
    ID system_user_id = catalog->getSystemUserId(nullptr);

    ID schema_id;
    Status status = catalog->createSchema("persist_schema", "system", schema_id, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to create schema";

    std::vector<CatalogManager::ColumnInfo> columns;
    CatalogManager::ColumnInfo id_col{};
    id_col.column_name = "id";
    id_col.data_type = static_cast<uint16_t>(DataType::INT32);
    id_col.nullable = false;
    columns.push_back(id_col);

    CatalogManager::ColumnInfo name_col{};
    name_col.column_name = "name";
    name_col.data_type = static_cast<uint16_t>(DataType::VARCHAR);
    name_col.type_precision = 64;
    name_col.nullable = true;
    columns.push_back(name_col);

    ID table_id;
    status = catalog->createTable(schema_id, "persist_table", columns, table_id, 0, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to create table";

    CatalogManager::ConstraintInfo constraint{};
    constraint.constraint_name = "pk_persist_table";
    constraint.table_id = table_id;
    constraint.constraint_type = CatalogManager::ConstraintType::PRIMARY_KEY;
    constraint.column_names = {"id"};
    constraint.owner_id = system_user_id;

    ID constraint_id;
    status = catalog->createConstraint(constraint, constraint_id, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to create constraint";

    status = db_->buffer_pool()->flushAll(nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to flush buffer pool";

    db_->close();
    status = db_->open(test_db_path_, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to reopen database";

    catalog = db_->catalog_manager();
    ASSERT_NE(catalog, nullptr);

    CatalogManager::ConstraintInfo loaded{};
    status = catalog->getConstraint(constraint_id, loaded, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to reload constraint";
    EXPECT_EQ(loaded.constraint_name, "pk_persist_table");
    EXPECT_EQ(loaded.table_id, table_id);
    EXPECT_EQ(loaded.constraint_type, CatalogManager::ConstraintType::PRIMARY_KEY);
    ASSERT_EQ(loaded.column_names.size(), 1U);
    EXPECT_EQ(loaded.column_names[0], "id");

    CatalogManager::ConstraintInfo by_name{};
    status = catalog->getConstraintByName(table_id, "pk_persist_table", by_name, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to lookup constraint by name";
    EXPECT_EQ(by_name.constraint_id, constraint_id);
}

TEST_F(CatalogPersistencePhaseBTest, PhaseBCatalogsPersistAcrossRestart)
{
    auto *catalog = db_->catalog_manager();
    ASSERT_NE(catalog, nullptr);
    ID system_user_id = catalog->getSystemUserId(nullptr);

    ID foreign_server_id;
    Status status = catalog->createForeignServer("fdw_server", "postgresql", "127.0.0.1",
                                                 5432, "{}", foreign_server_id, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to create foreign server";

    ID mapping_id;
    status = catalog->createUserMapping(system_user_id, foreign_server_id,
                                        "fdw_user", "secret", mapping_id, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to create user mapping";

    ID registry_id;
    status = catalog->registerServer("cluster_primary", "localhost", 4040,
                                     CatalogManager::ServerRole::PRIMARY, "cluster_a",
                                     registry_id, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to register server";

    ID engine_id;
    status = catalog->registerUDREngine("native", CatalogManager::UDREngineType::NATIVE,
                                        "/opt/scratchbird/udr_native.so", "{}", engine_id, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to register UDR engine";

    ID module_id;
    status = catalog->registerUDRModule("analytics", engine_id,
                                        "/opt/scratchbird/analytics.udr", "entry",
                                        module_id, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to register UDR module";

    status = db_->buffer_pool()->flushAll(nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to flush buffer pool";

    db_->close();
    status = db_->open(test_db_path_, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to reopen database";

    catalog = db_->catalog_manager();
    ASSERT_NE(catalog, nullptr);

    CatalogManager::ForeignServerInfo foreign_server{};
    status = catalog->getForeignServerByName("fdw_server", foreign_server, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to reload foreign server";
    EXPECT_EQ(foreign_server.server_id, foreign_server_id);
    EXPECT_EQ(foreign_server.server_type, "postgresql");

    CatalogManager::UserMappingInfo mapping{};
    status = catalog->getUserMapping(system_user_id, foreign_server_id,
                                     mapping, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to reload user mapping";
    EXPECT_EQ(mapping.mapping_id, mapping_id);
    EXPECT_EQ(mapping.remote_user, "fdw_user");

    CatalogManager::ServerRegistryInfo registry{};
    status = catalog->getRegisteredServerByName("cluster_primary", registry, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to reload server registry";
    EXPECT_EQ(registry.server_id, registry_id);
    EXPECT_EQ(registry.cluster_id, "cluster_a");

    CatalogManager::UDREngineInfo engine{};
    status = catalog->getUDREngineByName("native", engine, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to reload UDR engine";
    EXPECT_EQ(engine.engine_id, engine_id);

    CatalogManager::UDRModuleInfo module{};
    status = catalog->getUDRModuleByName("analytics", module, nullptr);
    ASSERT_EQ(status, Status::OK) << "Failed to reload UDR module";
    EXPECT_EQ(module.module_id, module_id);
    EXPECT_EQ(module.engine_id, engine_id);
}

} // namespace
