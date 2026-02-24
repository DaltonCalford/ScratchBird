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
#include <cstring>
#include <vector>

#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/uuidv7.h"
#include "test_helpers.h"

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestDbPath;

namespace
{
#pragma pack(push, 1)
struct DatabaseRecordContract
{
    ID database_id;
    char database_name[512];
    ID owner_id;
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};

struct ObjectRecordContract
{
    ID object_id;
    uint8_t object_type;
    uint8_t reserved_1[3];
    ID schema_id;
    ID parent_object_id;
    ID owner_id;
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};

struct ObjectNameRecordContract
{
    ID name_id;
    ID object_id;
    uint8_t object_type;
    uint8_t reserved_1[3];
    ID parent_object_id;
    char schema_path[512];
    char language_code[32];
    char name_text[512];
    char canonical_name_text[512];
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
#pragma pack(pop)

bool isZeroUuid(const ID& id)
{
    for (auto byte : id.bytes)
    {
        if (byte != 0)
        {
            return false;
        }
    }
    return true;
}

struct DatabaseCatalogSnapshot
{
    uint32_t valid_row_count = 0;
    uint32_t matching_database_row_count = 0;
    std::string database_name;
    ID owner_id{};
};

struct ObjectCatalogSnapshot
{
    uint32_t valid_row_count = 0;
    uint32_t database_object_row_count = 0;
    uint32_t schema_object_row_count = 0;
};

struct ObjectNameCatalogSnapshot
{
    uint32_t valid_row_count = 0;
    uint32_t default_language_row_count = 0;
    uint32_t database_default_name_row_count = 0;
    std::string database_name;
};

Status readDatabaseCatalogSnapshot(Database& db,
                                   uint32_t database_page_id,
                                   const ID& database_id,
                                   DatabaseCatalogSnapshot& out,
                                   ErrorContext* ctx)
{
    if (database_page_id == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "database catalog page is zero");
        return Status::PAGE_CORRUPT;
    }

    BufferPool* bp = db.buffer_pool();
    if (bp == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "buffer pool unavailable");
        return Status::INVALID_ARGUMENT;
    }

    uint32_t current_page_id = database_page_id;

    while (current_page_id != 0)
    {
        void* page_buffer = nullptr;
        Status status = bp->pinPage(current_page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        const auto* heap = reinterpret_cast<const CatalogHeapPage*>(page_buffer);
        if (heap->header.page_type != PAGE_TYPE_HEAP)
        {
            bp->unpinPage(current_page_id, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "database catalog page is not heap");
            return Status::PAGE_CORRUPT;
        }

        uint32_t offset = sizeof(CatalogHeapPage);
        for (uint32_t i = 0; i < heap->record_count; ++i)
        {
            const auto* rec =
                reinterpret_cast<const DatabaseRecordContract*>(
                    reinterpret_cast<const uint8_t*>(page_buffer) + offset);
            if (rec->is_valid != 0)
            {
                ++out.valid_row_count;
                if (rec->database_id == database_id)
                {
                    ++out.matching_database_row_count;
                    out.database_name = rec->database_name;
                    out.owner_id = rec->owner_id;
                }
            }
            offset += sizeof(DatabaseRecordContract);
        }

        uint32_t next_page_id = heap->next_page;
        status = bp->unpinPage(current_page_id, false, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        current_page_id = next_page_id;
    }

    return Status::OK;
}

Status readObjectCatalogSnapshot(Database& db,
                                 uint32_t object_page_id,
                                 const ID& database_id,
                                 ObjectCatalogSnapshot& out,
                                 ErrorContext* ctx)
{
    if (object_page_id == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "object catalog page is zero");
        return Status::PAGE_CORRUPT;
    }

    BufferPool* bp = db.buffer_pool();
    if (bp == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "buffer pool unavailable");
        return Status::INVALID_ARGUMENT;
    }

    uint32_t current_page_id = object_page_id;
    const uint8_t database_type = static_cast<uint8_t>(CatalogManager::ObjectType::DATABASE);
    const uint8_t schema_type = static_cast<uint8_t>(CatalogManager::ObjectType::SCHEMA);

    while (current_page_id != 0)
    {
        void* page_buffer = nullptr;
        Status status = bp->pinPage(current_page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        const auto* heap = reinterpret_cast<const CatalogHeapPage*>(page_buffer);
        if (heap->header.page_type != PAGE_TYPE_HEAP)
        {
            bp->unpinPage(current_page_id, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "object catalog page is not heap");
            return Status::PAGE_CORRUPT;
        }

        uint32_t offset = sizeof(CatalogHeapPage);
        for (uint32_t i = 0; i < heap->record_count; ++i)
        {
            const auto* rec =
                reinterpret_cast<const ObjectRecordContract*>(
                    reinterpret_cast<const uint8_t*>(page_buffer) + offset);
            if (rec->is_valid != 0)
            {
                ++out.valid_row_count;
                if (rec->object_id == database_id && rec->object_type == database_type)
                {
                    ++out.database_object_row_count;
                }
                if (rec->object_type == schema_type)
                {
                    ++out.schema_object_row_count;
                }
            }
            offset += sizeof(ObjectRecordContract);
        }

        uint32_t next_page_id = heap->next_page;
        status = bp->unpinPage(current_page_id, false, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        current_page_id = next_page_id;
    }

    return Status::OK;
}

Status readObjectNameCatalogSnapshot(Database& db,
                                     uint32_t object_name_page_id,
                                     const ID& database_id,
                                     ObjectNameCatalogSnapshot& out,
                                     ErrorContext* ctx)
{
    if (object_name_page_id == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "object_name catalog page is zero");
        return Status::PAGE_CORRUPT;
    }

    BufferPool* bp = db.buffer_pool();
    if (bp == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "buffer pool unavailable");
        return Status::INVALID_ARGUMENT;
    }

    uint32_t current_page_id = object_name_page_id;
    const uint8_t database_type = static_cast<uint8_t>(CatalogManager::ObjectType::DATABASE);

    while (current_page_id != 0)
    {
        void* page_buffer = nullptr;
        Status status = bp->pinPage(current_page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        const auto* heap = reinterpret_cast<const CatalogHeapPage*>(page_buffer);
        if (heap->header.page_type != PAGE_TYPE_HEAP)
        {
            bp->unpinPage(current_page_id, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "object_name catalog page is not heap");
            return Status::PAGE_CORRUPT;
        }

        uint32_t offset = sizeof(CatalogHeapPage);
        for (uint32_t i = 0; i < heap->record_count; ++i)
        {
            const auto* rec =
                reinterpret_cast<const ObjectNameRecordContract*>(
                    reinterpret_cast<const uint8_t*>(page_buffer) + offset);
            if (rec->is_valid != 0)
            {
                ++out.valid_row_count;
                size_t language_len = strnlen(rec->language_code, sizeof(rec->language_code));
                std::string language(rec->language_code, language_len);
                if (language == "default")
                {
                    ++out.default_language_row_count;
                    if (rec->object_id == database_id && rec->object_type == database_type)
                    {
                        ++out.database_default_name_row_count;
                        size_t name_len = strnlen(rec->name_text, sizeof(rec->name_text));
                        out.database_name.assign(rec->name_text, name_len);
                    }
                }
            }
            offset += sizeof(ObjectNameRecordContract);
        }

        uint32_t next_page_id = heap->next_page;
        status = bp->unpinPage(current_page_id, false, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        current_page_id = next_page_id;
    }

    return Status::OK;
}

Status assertHeapCatalogPage(Database& db, uint32_t page_id, ErrorContext* ctx)
{
    if (page_id == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "catalog page id is zero");
        return Status::PAGE_CORRUPT;
    }

    BufferPool* bp = db.buffer_pool();
    if (bp == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "buffer pool unavailable");
        return Status::INVALID_ARGUMENT;
    }

    void* page_buffer = nullptr;
    Status status = bp->pinPage(page_id, &page_buffer, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    const auto* heap = reinterpret_cast<const CatalogHeapPage*>(page_buffer);
    const bool is_heap = (heap->header.page_type == PAGE_TYPE_HEAP);

    status = bp->unpinPage(page_id, false, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    if (!is_heap)
    {
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "catalog page is not heap");
        return Status::PAGE_CORRUPT;
    }
    return Status::OK;
}
} // namespace

TEST(CatalogDatabaseBootstrapTest, PersistsDatabaseIdentityRow)
{
    std::string db_path = uniqueTestDbPath("test_catalog_database_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    uint32_t database_page_id = 0;
    uint32_t object_page_id = 0;
    uint32_t object_name_page_id = 0;
    ID database_uuid{};
    ID system_user_id{};

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;

        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        database_page_id = catalog->databaseTablePage();
        ASSERT_NE(database_page_id, 0u);
        object_page_id = catalog->objectTablePage();
        ASSERT_NE(object_page_id, 0u);
        object_name_page_id = catalog->objectNameTablePage();
        ASSERT_NE(object_name_page_id, 0u);

        database_uuid = db.uuid();
        ASSERT_FALSE(isZeroUuid(database_uuid));

        system_user_id = catalog->getSystemUserId(&ctx);
        ASSERT_FALSE(isZeroUuid(system_user_id)) << ctx.message;

        ASSERT_EQ(db.sync(&ctx), Status::OK) << ctx.message;

        DatabaseCatalogSnapshot snapshot;
        ASSERT_EQ(readDatabaseCatalogSnapshot(db, database_page_id, database_uuid, snapshot, &ctx),
                  Status::OK)
            << ctx.message;

        EXPECT_GE(snapshot.valid_row_count, 1u);
        EXPECT_EQ(snapshot.matching_database_row_count, 1u);
        EXPECT_FALSE(snapshot.database_name.empty());
        EXPECT_EQ(snapshot.owner_id, system_user_id);

        ObjectCatalogSnapshot object_snapshot;
        ASSERT_EQ(readObjectCatalogSnapshot(db, object_page_id, database_uuid, object_snapshot, &ctx),
                  Status::OK)
            << ctx.message;
        EXPECT_GE(object_snapshot.valid_row_count, 1u);
        EXPECT_EQ(object_snapshot.database_object_row_count, 1u);

        ObjectNameCatalogSnapshot object_name_snapshot;
        ASSERT_EQ(
            readObjectNameCatalogSnapshot(db, object_name_page_id, database_uuid, object_name_snapshot, &ctx),
            Status::OK)
            << ctx.message;

        db.close();
    }

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;

        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);
        uint32_t reopened_database_page_id = catalog->databaseTablePage();
        uint32_t reopened_object_page_id = catalog->objectTablePage();
        uint32_t reopened_object_name_page_id = catalog->objectNameTablePage();
        ASSERT_NE(reopened_database_page_id, 0u);
        ASSERT_NE(reopened_object_page_id, 0u);
        ASSERT_NE(reopened_object_name_page_id, 0u);
        ASSERT_EQ(db.uuid(), database_uuid);

        DatabaseCatalogSnapshot snapshot;
        ASSERT_EQ(readDatabaseCatalogSnapshot(db, reopened_database_page_id, database_uuid, snapshot, &ctx),
                  Status::OK)
            << ctx.message;

        EXPECT_EQ(snapshot.matching_database_row_count, 1u)
            << "bootstrap must not duplicate database identity row";

        ObjectCatalogSnapshot object_snapshot;
        ASSERT_EQ(readObjectCatalogSnapshot(db, reopened_object_page_id, database_uuid, object_snapshot, &ctx),
                  Status::OK)
            << ctx.message;
        EXPECT_EQ(object_snapshot.database_object_row_count, 1u)
            << "bootstrap must not duplicate canonical database object row";

        ObjectNameCatalogSnapshot object_name_snapshot;
        ASSERT_EQ(
            readObjectNameCatalogSnapshot(
                db, reopened_object_name_page_id, database_uuid, object_name_snapshot, &ctx),
            Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesCanonicalFixedSchemaTree)
{
    std::string db_path = uniqueTestDbPath("test_catalog_schema_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    Database db;
    ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;

    auto* catalog = db.catalog_manager();
    ASSERT_NE(catalog, nullptr);

    static constexpr const char* kCanonicalPaths[] = {
        "sys",
        "connections",
        "users",
        "group",
        "cluster",
        "remote",
        "local",
        "nosql",
        "emulated",
        "sys.information",
        "sys.security",
        "sys.system",
        "sys.schema",
        "sys.cluster",
        "sys.connections",
        "sys.emulation",
        "sys.jobs",
        "users.public",
        "users.app_data",
        "users.roles",
        "users.groups",
        "remote.emulation",
        "remote.fdw",
        "remote.links",
        "local.instances",
        "local.links",
        "nosql.cassandra",
        "nosql.mongodb",
        "nosql.neo4j",
        "nosql.redis",
        "nosql.milvus",
        "sys.security.users",
        "sys.security.roles",
        "sys.security.groups",
        "sys.security.auth",
        "remote.emulation.firebird",
        "remote.emulation.postgresql",
        "remote.emulation.mysql",
        "remote.emulation.cassandra",
        "remote.emulation.mongodb",
        "remote.emulation.neo4j",
        "remote.emulation.redis",
        "remote.emulation.milvus",
    };

    for (const char* path : kCanonicalPaths)
    {
        CatalogManager::SchemaInfo info;
        EXPECT_EQ(catalog->getSchema(path, info, &ctx), Status::OK)
            << "missing canonical schema path: " << path << " err=" << ctx.message;

        CatalogManager::SchemaInfo legacy_info;
        EXPECT_EQ(catalog->getSchema(std::string("root.") + path, legacy_info, &ctx), Status::OK)
            << "missing legacy root-prefixed alias path: root." << path
            << " err=" << ctx.message;
    }

    CatalogManager::SchemaInfo legacy;
    EXPECT_EQ(catalog->getSchema("root", legacy, &ctx), Status::INVALID_ARGUMENT);
    EXPECT_EQ(catalog->getSchema("root.app", legacy, &ctx), Status::INVALID_ARGUMENT);
    EXPECT_EQ(catalog->getSchema("root.sys.sec", legacy, &ctx), Status::INVALID_ARGUMENT);
    EXPECT_EQ(catalog->getSchema("root.sys.config", legacy, &ctx), Status::INVALID_ARGUMENT);
    EXPECT_EQ(catalog->getSchema("root.sys.monitor", legacy, &ctx), Status::INVALID_ARGUMENT);
    EXPECT_EQ(catalog->getSchema("root.remote.emulation.mssql", legacy, &ctx), Status::INVALID_ARGUMENT);

    db.close();
    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesTypeCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_type_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    uint32_t type_page_id = 0;
    uint32_t type_modifiers_page_id = 0;
    uint32_t type_io_page_id = 0;
    uint32_t type_casts_page_id = 0;
    uint32_t type_transforms_page_id = 0;
    uint32_t encoding_conversions_page_id = 0;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        type_page_id = catalog->typeTablePage();
        type_modifiers_page_id = catalog->typeModifiersTablePage();
        type_io_page_id = catalog->typeIoTablePage();
        type_casts_page_id = catalog->typeCastsTablePage();
        type_transforms_page_id = catalog->typeTransformsTablePage();
        encoding_conversions_page_id = catalog->encodingConversionsTablePage();

        ASSERT_NE(type_page_id, 0u);
        ASSERT_NE(type_modifiers_page_id, 0u);
        ASSERT_NE(type_io_page_id, 0u);
        ASSERT_NE(type_casts_page_id, 0u);
        ASSERT_NE(type_transforms_page_id, 0u);
        ASSERT_NE(encoding_conversions_page_id, 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, type_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, type_modifiers_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, type_io_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, type_casts_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, type_transforms_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, encoding_conversions_page_id, &ctx), Status::OK) << ctx.message;

        ASSERT_EQ(db.sync(&ctx), Status::OK) << ctx.message;
        db.close();
    }

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        EXPECT_EQ(catalog->typeTablePage(), type_page_id);
        EXPECT_EQ(catalog->typeModifiersTablePage(), type_modifiers_page_id);
        EXPECT_EQ(catalog->typeIoTablePage(), type_io_page_id);
        EXPECT_EQ(catalog->typeCastsTablePage(), type_casts_page_id);
        EXPECT_EQ(catalog->typeTransformsTablePage(), type_transforms_page_id);
        EXPECT_EQ(catalog->encodingConversionsTablePage(), encoding_conversions_page_id);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->typeTablePage(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->typeModifiersTablePage(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->typeIoTablePage(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->typeCastsTablePage(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->typeTransformsTablePage(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->encodingConversionsTablePage(), &ctx), Status::OK) << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, GroupLifecycleMaterializesDynamicOverlaySchema)
{
    std::string db_path = uniqueTestDbPath("test_catalog_group_overlay_lifecycle");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ID group_id{};
        ASSERT_EQ(catalog->createGroup("qa_team",
                                       CatalogManager::GroupType::LOCAL,
                                       "",
                                       ID{},
                                       group_id,
                                       &ctx),
                  Status::OK)
            << ctx.message;

        CatalogManager::SchemaInfo schema_info{};
        EXPECT_EQ(catalog->getSchema("group.qa_team", schema_info, &ctx), Status::OK)
            << ctx.message;
        EXPECT_EQ(catalog->getSchema("root.group.qa_team", schema_info, &ctx), Status::OK)
            << ctx.message;

        ASSERT_EQ(catalog->deleteGroup(group_id, true, &ctx), Status::OK) << ctx.message;
        EXPECT_NE(catalog->getSchema("group.qa_team", schema_info, &ctx), Status::OK);

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, GroupMemberLifecycleMaterializesDynamicOverlaySchema)
{
    std::string db_path = uniqueTestDbPath("test_catalog_group_member_overlay_lifecycle");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ID group_id{};
        ASSERT_EQ(catalog->createGroup("qa_team",
                                       CatalogManager::GroupType::LOCAL,
                                       "",
                                       ID{},
                                       group_id,
                                       &ctx),
                  Status::OK)
            << ctx.message;

        const ID system_user_id = catalog->getSystemUserId(&ctx);
        ASSERT_FALSE(isZeroUuid(system_user_id)) << ctx.message;

        CatalogManager::BasicUserInfo system_user{};
        ASSERT_EQ(catalog->getUserBasic(system_user_id, system_user, &ctx), Status::OK)
            << ctx.message;

        ASSERT_EQ(catalog->addGroupMember(group_id, system_user_id, false, system_user_id, &ctx),
                  Status::OK)
            << ctx.message;

        const std::string member_schema = "group.qa_team." + system_user.username;
        CatalogManager::SchemaInfo schema_info{};
        EXPECT_EQ(catalog->getSchema(member_schema, schema_info, &ctx), Status::OK)
            << ctx.message;

        ASSERT_EQ(catalog->removeGroupMember(group_id, system_user_id, &ctx), Status::OK)
            << ctx.message;
        EXPECT_NE(catalog->getSchema(member_schema, schema_info, &ctx), Status::OK);

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, ClusterLifecycleMaterializesDynamicOverlaySchema)
{
    std::string db_path = uniqueTestDbPath("test_catalog_cluster_overlay_lifecycle");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ID cluster_id = generateUuidV7();
        CatalogManager::ClusterCatalogInfo cluster{};
        cluster.cluster_id = cluster_id;
        cluster.cluster_name = "alpha";
        cluster.cluster_mode = CatalogManager::ClusterMode::CLUSTER;
        cluster.cluster_state = CatalogManager::ClusterState::ONLINE;
        cluster.consensus_mode = CatalogManager::ConsensusMode::RAFT;
        cluster.config_version = 1;
        cluster.cluster_state_version = 1;

        ASSERT_EQ(catalog->upsertClusterCatalogEntry(cluster, &ctx), Status::OK) << ctx.message;

        CatalogManager::SchemaInfo schema_info{};
        EXPECT_EQ(catalog->getSchema("cluster.alpha", schema_info, &ctx), Status::OK)
            << ctx.message;
        EXPECT_EQ(catalog->getSchema("root.cluster.alpha", schema_info, &ctx), Status::OK)
            << ctx.message;

        ASSERT_EQ(catalog->deleteClusterCatalogEntry(cluster_id, &ctx), Status::OK) << ctx.message;
        EXPECT_NE(catalog->getSchema("cluster.alpha", schema_info, &ctx), Status::OK);

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, GroupRenameMovesDynamicOverlaySchema)
{
    std::string db_path = uniqueTestDbPath("test_catalog_group_overlay_rename");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ID group_id{};
        ASSERT_EQ(catalog->createGroup("qa_group_old",
                                       CatalogManager::GroupType::LOCAL,
                                       "",
                                       ID{},
                                       group_id,
                                       &ctx),
                  Status::OK)
            << ctx.message;

        ASSERT_EQ(catalog->updateGroup(group_id,
                                       std::optional<std::string>("qa_group_new"),
                                       std::nullopt,
                                       std::nullopt,
                                       std::nullopt,
                                       std::nullopt,
                                       &ctx),
                  Status::OK)
            << ctx.message;

        CatalogManager::SchemaInfo schema_info{};
        EXPECT_EQ(catalog->getSchema("group.qa_group_new", schema_info, &ctx), Status::OK)
            << ctx.message;
        EXPECT_NE(catalog->getSchema("group.qa_group_old", schema_info, &ctx), Status::OK);

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, ClusterRenameMovesDynamicOverlaySchema)
{
    std::string db_path = uniqueTestDbPath("test_catalog_cluster_overlay_rename");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ID cluster_id = generateUuidV7();
        CatalogManager::ClusterCatalogInfo cluster{};
        cluster.cluster_id = cluster_id;
        cluster.cluster_name = "alpha_old";
        cluster.cluster_mode = CatalogManager::ClusterMode::CLUSTER;
        cluster.cluster_state = CatalogManager::ClusterState::ONLINE;
        cluster.consensus_mode = CatalogManager::ConsensusMode::RAFT;
        cluster.config_version = 1;
        cluster.cluster_state_version = 1;

        ASSERT_EQ(catalog->upsertClusterCatalogEntry(cluster, &ctx), Status::OK) << ctx.message;

        cluster.cluster_name = "alpha_new";
        cluster.config_version = 2;
        cluster.cluster_state_version = 2;
        ASSERT_EQ(catalog->upsertClusterCatalogEntry(cluster, &ctx), Status::OK) << ctx.message;

        CatalogManager::SchemaInfo schema_info{};
        EXPECT_EQ(catalog->getSchema("cluster.alpha_new", schema_info, &ctx), Status::OK)
            << ctx.message;
        EXPECT_NE(catalog->getSchema("cluster.alpha_old", schema_info, &ctx), Status::OK);

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, ClusterNodeLifecycleMaterializesDynamicOverlaySchema)
{
    std::string db_path = uniqueTestDbPath("test_catalog_cluster_node_overlay_lifecycle");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ID cluster_id = generateUuidV7();
        CatalogManager::ClusterCatalogInfo cluster{};
        cluster.cluster_id = cluster_id;
        cluster.cluster_name = "alpha";
        cluster.cluster_mode = CatalogManager::ClusterMode::CLUSTER;
        cluster.cluster_state = CatalogManager::ClusterState::ONLINE;
        cluster.consensus_mode = CatalogManager::ConsensusMode::RAFT;
        cluster.config_version = 1;
        cluster.cluster_state_version = 1;
        ASSERT_EQ(catalog->upsertClusterCatalogEntry(cluster, &ctx), Status::OK) << ctx.message;

        CatalogManager::NodeCatalogInfo node{};
        node.node_id = generateUuidV7();
        node.cluster_id = cluster_id;
        node.node_name = "node_a";
        node.node_role = CatalogManager::ClusterNodeRole::METADATA;
        node.host = "127.0.0.1";
        node.port = 7101;
        node.transport = CatalogManager::ConnectionTransport::INET;
        node.state = CatalogManager::ClusterNodeState::ONLINE;
        ASSERT_EQ(catalog->upsertNodeCatalogEntry(node, &ctx), Status::OK) << ctx.message;

        CatalogManager::SchemaInfo schema_info{};
        EXPECT_EQ(catalog->getSchema("cluster.alpha.node_a", schema_info, &ctx), Status::OK)
            << ctx.message;
        EXPECT_EQ(catalog->getSchema("root.cluster.alpha.node_a", schema_info, &ctx), Status::OK)
            << ctx.message;

        ASSERT_EQ(catalog->deleteNodeCatalogEntry(node.node_id, &ctx), Status::OK) << ctx.message;
        EXPECT_NE(catalog->getSchema("cluster.alpha.node_a", schema_info, &ctx), Status::OK);

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, ClusterNodeRenameMovesDynamicOverlaySchema)
{
    std::string db_path = uniqueTestDbPath("test_catalog_cluster_node_overlay_rename");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ID cluster_id = generateUuidV7();
        CatalogManager::ClusterCatalogInfo cluster{};
        cluster.cluster_id = cluster_id;
        cluster.cluster_name = "alpha";
        cluster.cluster_mode = CatalogManager::ClusterMode::CLUSTER;
        cluster.cluster_state = CatalogManager::ClusterState::ONLINE;
        cluster.consensus_mode = CatalogManager::ConsensusMode::RAFT;
        cluster.config_version = 1;
        cluster.cluster_state_version = 1;
        ASSERT_EQ(catalog->upsertClusterCatalogEntry(cluster, &ctx), Status::OK) << ctx.message;

        CatalogManager::NodeCatalogInfo node{};
        node.node_id = generateUuidV7();
        node.cluster_id = cluster_id;
        node.node_name = "node_old";
        node.node_role = CatalogManager::ClusterNodeRole::METADATA;
        node.host = "127.0.0.1";
        node.port = 7101;
        node.transport = CatalogManager::ConnectionTransport::INET;
        node.state = CatalogManager::ClusterNodeState::ONLINE;
        ASSERT_EQ(catalog->upsertNodeCatalogEntry(node, &ctx), Status::OK) << ctx.message;

        node.node_name = "node_new";
        ASSERT_EQ(catalog->upsertNodeCatalogEntry(node, &ctx), Status::OK) << ctx.message;

        CatalogManager::SchemaInfo schema_info{};
        EXPECT_EQ(catalog->getSchema("cluster.alpha.node_new", schema_info, &ctx), Status::OK)
            << ctx.message;
        EXPECT_NE(catalog->getSchema("cluster.alpha.node_old", schema_info, &ctx), Status::OK);

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, EmulatedDatabaseLifecycleMaterializesDynamicOverlaySchema)
{
    std::string db_path = uniqueTestDbPath("test_catalog_emulated_overlay_lifecycle");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ID emulation_type_id{};
        ASSERT_EQ(catalog->createEmulationType("firebird", 5, 0, "", emulation_type_id, &ctx),
                  Status::OK)
            << ctx.message;

        ID server_id{};
        ASSERT_EQ(catalog->createEmulationServer("fb_local", emulation_type_id, "", server_id, &ctx),
                  Status::OK)
            << ctx.message;

        ID emulated_db_id{};
        ASSERT_EQ(catalog->createEmulatedDatabase("example_db",
                                                  server_id,
                                                  ID{},
                                                  "",
                                                  emulated_db_id,
                                                  &ctx),
                  Status::OK)
            << ctx.message;

        CatalogManager::SchemaInfo schema_info{};
        EXPECT_EQ(catalog->getSchema("emulated.firebird.example_db", schema_info, &ctx), Status::OK)
            << ctx.message;
        EXPECT_EQ(catalog->getSchema("root.emulated.firebird.example_db", schema_info, &ctx), Status::OK)
            << ctx.message;

        ASSERT_EQ(catalog->dropEmulatedDatabase(emulated_db_id, &ctx), Status::OK) << ctx.message;
        EXPECT_NE(catalog->getSchema("emulated.firebird.example_db", schema_info, &ctx), Status::OK);
        EXPECT_NE(catalog->getSchema("emulated.firebird", schema_info, &ctx), Status::OK);

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, EmulatedDatabaseRenameMovesDynamicOverlaySchema)
{
    std::string db_path = uniqueTestDbPath("test_catalog_emulated_overlay_rename");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ID emulation_type_id{};
        ASSERT_EQ(catalog->createEmulationType("firebird", 5, 0, "", emulation_type_id, &ctx),
                  Status::OK)
            << ctx.message;

        ID server_id{};
        ASSERT_EQ(catalog->createEmulationServer("fb_local", emulation_type_id, "", server_id, &ctx),
                  Status::OK)
            << ctx.message;

        ID emulated_db_id{};
        ASSERT_EQ(catalog->createEmulatedDatabase("old_db",
                                                  server_id,
                                                  ID{},
                                                  "",
                                                  emulated_db_id,
                                                  &ctx),
                  Status::OK)
            << ctx.message;

        ASSERT_EQ(catalog->renameEmulatedDatabase(emulated_db_id, "new_db", &ctx), Status::OK)
            << ctx.message;

        CatalogManager::SchemaInfo schema_info{};
        EXPECT_EQ(catalog->getSchema("emulated.firebird.new_db", schema_info, &ctx), Status::OK)
            << ctx.message;
        EXPECT_NE(catalog->getSchema("emulated.firebird.old_db", schema_info, &ctx), Status::OK);

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, RemoteConnectorLifecycleMaterializesDynamicOverlaySchema)
{
    std::string db_path = uniqueTestDbPath("test_catalog_remote_connector_overlay_lifecycle");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        const ID system_user_id = catalog->getSystemUserId(&ctx);
        ASSERT_FALSE(isZeroUuid(system_user_id)) << ctx.message;

        ID fdw_server_id{};
        ASSERT_EQ(catalog->createForeignServer("rc_fdw_server",
                                               "postgresql",
                                               "127.0.0.1",
                                               5432,
                                               "{}",
                                               fdw_server_id,
                                               &ctx),
                  Status::OK)
            << ctx.message;

        ID mapping_id{};
        ASSERT_EQ(catalog->createUserMapping(system_user_id,
                                             fdw_server_id,
                                             "sb_user",
                                             "sb_secret",
                                             mapping_id,
                                             &ctx),
                  Status::OK)
            << ctx.message;

        CatalogManager::RemoteConnectorCatalogInfo connector{};
        connector.remote_connector_id = generateUuidV7();
        connector.fdw_server_id = fdw_server_id;
        connector.fdw_id = generateUuidV7();
        connector.connector_name = "corp_primary";
        connector.engine_name = "postgresql";
        connector.endpoint_uri = "tcp://127.0.0.1:5432";
        connector.has_default_mapping_id = true;
        connector.default_mapping_id = mapping_id;
        connector.has_engine_version_text = true;
        connector.engine_version_text = "18.0";
        connector.module_checksum = 0xA11CEu;
        connector.state = CatalogManager::RemoteConnectorState::READY;

        ASSERT_EQ(catalog->upsertRemoteConnectorCatalogEntry(connector, &ctx), Status::OK)
            << ctx.message;

        CatalogManager::SchemaInfo schema_info{};
        EXPECT_EQ(catalog->getSchema("connections.corp_primary", schema_info, &ctx), Status::OK)
            << ctx.message;
        EXPECT_EQ(catalog->getSchema("root.connections.corp_primary", schema_info, &ctx), Status::OK)
            << ctx.message;

        ASSERT_EQ(catalog->deleteRemoteConnectorCatalogEntry(connector.remote_connector_id, &ctx),
                  Status::OK)
            << ctx.message;
        EXPECT_NE(catalog->getSchema("connections.corp_primary", schema_info, &ctx), Status::OK);

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, RemoteConnectorRenameMovesDynamicOverlaySchema)
{
    std::string db_path = uniqueTestDbPath("test_catalog_remote_connector_overlay_rename");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        const ID system_user_id = catalog->getSystemUserId(&ctx);
        ASSERT_FALSE(isZeroUuid(system_user_id)) << ctx.message;

        ID fdw_server_id{};
        ASSERT_EQ(catalog->createForeignServer("rc_fdw_server",
                                               "postgresql",
                                               "127.0.0.1",
                                               5432,
                                               "{}",
                                               fdw_server_id,
                                               &ctx),
                  Status::OK)
            << ctx.message;

        ID mapping_id{};
        ASSERT_EQ(catalog->createUserMapping(system_user_id,
                                             fdw_server_id,
                                             "sb_user",
                                             "sb_secret",
                                             mapping_id,
                                             &ctx),
                  Status::OK)
            << ctx.message;

        CatalogManager::RemoteConnectorCatalogInfo connector{};
        connector.remote_connector_id = generateUuidV7();
        connector.fdw_server_id = fdw_server_id;
        connector.fdw_id = generateUuidV7();
        connector.connector_name = "corp_old";
        connector.engine_name = "postgresql";
        connector.endpoint_uri = "tcp://127.0.0.1:5432";
        connector.has_default_mapping_id = true;
        connector.default_mapping_id = mapping_id;
        connector.has_engine_version_text = true;
        connector.engine_version_text = "18.0";
        connector.module_checksum = 0xBEEFu;
        connector.state = CatalogManager::RemoteConnectorState::READY;

        ASSERT_EQ(catalog->upsertRemoteConnectorCatalogEntry(connector, &ctx), Status::OK)
            << ctx.message;

        connector.connector_name = "corp_new";
        ASSERT_EQ(catalog->upsertRemoteConnectorCatalogEntry(connector, &ctx), Status::OK)
            << ctx.message;

        CatalogManager::SchemaInfo schema_info{};
        EXPECT_EQ(catalog->getSchema("connections.corp_new", schema_info, &ctx), Status::OK)
            << ctx.message;
        EXPECT_NE(catalog->getSchema("connections.corp_old", schema_info, &ctx), Status::OK);

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesDomainExtensionCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_domain_extension_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    uint32_t domain_param_keys_page_id = 0;
    uint32_t domain_parameters_page_id = 0;
    uint32_t domain_constraints_page_id = 0;
    uint32_t domain_security_page_id = 0;
    uint32_t domain_validation_page_id = 0;
    uint32_t domain_integrity_page_id = 0;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        domain_param_keys_page_id = catalog->domainParamKeysTablePage();
        domain_parameters_page_id = catalog->domainParametersTablePage();
        domain_constraints_page_id = catalog->domainConstraintsTablePage();
        domain_security_page_id = catalog->domainSecurityTablePage();
        domain_validation_page_id = catalog->domainValidationTablePage();
        domain_integrity_page_id = catalog->domainIntegrityTablePage();

        ASSERT_NE(domain_param_keys_page_id, 0u);
        ASSERT_NE(domain_parameters_page_id, 0u);
        ASSERT_NE(domain_constraints_page_id, 0u);
        ASSERT_NE(domain_security_page_id, 0u);
        ASSERT_NE(domain_validation_page_id, 0u);
        ASSERT_NE(domain_integrity_page_id, 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, domain_param_keys_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, domain_parameters_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, domain_constraints_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, domain_security_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, domain_validation_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, domain_integrity_page_id, &ctx), Status::OK) << ctx.message;

        ASSERT_EQ(db.sync(&ctx), Status::OK) << ctx.message;
        db.close();
    }

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        EXPECT_EQ(catalog->domainParamKeysTablePage(), domain_param_keys_page_id);
        EXPECT_EQ(catalog->domainParametersTablePage(), domain_parameters_page_id);
        EXPECT_EQ(catalog->domainConstraintsTablePage(), domain_constraints_page_id);
        EXPECT_EQ(catalog->domainSecurityTablePage(), domain_security_page_id);
        EXPECT_EQ(catalog->domainValidationTablePage(), domain_validation_page_id);
        EXPECT_EQ(catalog->domainIntegrityTablePage(), domain_integrity_page_id);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->domainParamKeysTablePage(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->domainParametersTablePage(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->domainConstraintsTablePage(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->domainSecurityTablePage(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->domainValidationTablePage(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->domainIntegrityTablePage(), &ctx), Status::OK) << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesCharsetCollationExtensionCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_charset_collation_extension_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    uint32_t charset_aliases_page_id = 0;
    uint32_t collation_tailoring_page_id = 0;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        charset_aliases_page_id = catalog->charsetAliasesTablePage();
        collation_tailoring_page_id = catalog->collationTailoringTablePage();

        ASSERT_NE(charset_aliases_page_id, 0u);
        ASSERT_NE(collation_tailoring_page_id, 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, charset_aliases_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, collation_tailoring_page_id, &ctx), Status::OK) << ctx.message;

        ASSERT_EQ(db.sync(&ctx), Status::OK) << ctx.message;
        db.close();
    }

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        EXPECT_EQ(catalog->charsetAliasesTablePage(), charset_aliases_page_id);
        EXPECT_EQ(catalog->collationTailoringTablePage(), collation_tailoring_page_id);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->charsetAliasesTablePage(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->collationTailoringTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesResourceTimezoneExtensionCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_resource_timezone_extension_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    uint32_t resource_bundles_page_id = 0;
    uint32_t resource_artifacts_page_id = 0;
    uint32_t timezone_transitions_page_id = 0;
    uint32_t timezone_leap_seconds_page_id = 0;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        resource_bundles_page_id = catalog->resourceBundlesTablePage();
        resource_artifacts_page_id = catalog->resourceArtifactsTablePage();
        timezone_transitions_page_id = catalog->timezoneTransitionsTablePage();
        timezone_leap_seconds_page_id = catalog->timezoneLeapSecondsTablePage();

        ASSERT_NE(resource_bundles_page_id, 0u);
        ASSERT_NE(resource_artifacts_page_id, 0u);
        ASSERT_NE(timezone_transitions_page_id, 0u);
        ASSERT_NE(timezone_leap_seconds_page_id, 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, resource_bundles_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, resource_artifacts_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, timezone_transitions_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, timezone_leap_seconds_page_id, &ctx), Status::OK) << ctx.message;

        ASSERT_EQ(db.sync(&ctx), Status::OK) << ctx.message;
        db.close();
    }

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        EXPECT_EQ(catalog->resourceBundlesTablePage(), resource_bundles_page_id);
        EXPECT_EQ(catalog->resourceArtifactsTablePage(), resource_artifacts_page_id);
        EXPECT_EQ(catalog->timezoneTransitionsTablePage(), timezone_transitions_page_id);
        EXPECT_EQ(catalog->timezoneLeapSecondsTablePage(), timezone_leap_seconds_page_id);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->resourceBundlesTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->resourceArtifactsTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->timezoneTransitionsTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->timezoneLeapSecondsTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesParserCapabilityExtensionCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_parser_capability_extension_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    uint32_t reserved_words_page_id = 0;
    uint32_t emulation_profile_page_id = 0;
    uint32_t parser_profiles_page_id = 0;
    uint32_t parser_capability_entries_page_id = 0;
    uint32_t parser_transform_entries_page_id = 0;
    uint32_t parser_error_map_entries_page_id = 0;
    uint32_t parser_feature_precedence_page_id = 0;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        reserved_words_page_id = catalog->reservedWordsTablePage();
        emulation_profile_page_id = catalog->emulationProfileTablePage();
        parser_profiles_page_id = catalog->parserProfilesTablePage();
        parser_capability_entries_page_id = catalog->parserCapabilityEntriesTablePage();
        parser_transform_entries_page_id = catalog->parserTransformEntriesTablePage();
        parser_error_map_entries_page_id = catalog->parserErrorMapEntriesTablePage();
        parser_feature_precedence_page_id = catalog->parserFeaturePrecedenceTablePage();

        ASSERT_NE(reserved_words_page_id, 0u);
        ASSERT_NE(emulation_profile_page_id, 0u);
        ASSERT_NE(parser_profiles_page_id, 0u);
        ASSERT_NE(parser_capability_entries_page_id, 0u);
        ASSERT_NE(parser_transform_entries_page_id, 0u);
        ASSERT_NE(parser_error_map_entries_page_id, 0u);
        ASSERT_NE(parser_feature_precedence_page_id, 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, reserved_words_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, emulation_profile_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, parser_profiles_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, parser_capability_entries_page_id, &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, parser_transform_entries_page_id, &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, parser_error_map_entries_page_id, &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, parser_feature_precedence_page_id, &ctx), Status::OK)
            << ctx.message;

        ASSERT_EQ(db.sync(&ctx), Status::OK) << ctx.message;
        db.close();
    }

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        EXPECT_EQ(catalog->reservedWordsTablePage(), reserved_words_page_id);
        EXPECT_EQ(catalog->emulationProfileTablePage(), emulation_profile_page_id);
        EXPECT_EQ(catalog->parserProfilesTablePage(), parser_profiles_page_id);
        EXPECT_EQ(catalog->parserCapabilityEntriesTablePage(), parser_capability_entries_page_id);
        EXPECT_EQ(catalog->parserTransformEntriesTablePage(), parser_transform_entries_page_id);
        EXPECT_EQ(catalog->parserErrorMapEntriesTablePage(), parser_error_map_entries_page_id);
        EXPECT_EQ(catalog->parserFeaturePrecedenceTablePage(), parser_feature_precedence_page_id);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->reservedWordsTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->emulationProfileTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->parserProfilesTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->parserCapabilityEntriesTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->parserTransformEntriesTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->parserErrorMapEntriesTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->parserFeaturePrecedenceTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesRelationExtensionCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_relation_extension_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    uint32_t partitioned_tables_page_id = 0;
    uint32_t partitions_page_id = 0;
    uint32_t table_inheritance_page_id = 0;
    uint32_t languages_page_id = 0;
    uint32_t events_page_id = 0;
    uint32_t package_members_page_id = 0;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        partitioned_tables_page_id = catalog->partitionedTablesTablePage();
        partitions_page_id = catalog->partitionsTablePage();
        table_inheritance_page_id = catalog->tableInheritanceTablePage();
        languages_page_id = catalog->languagesTablePage();
        events_page_id = catalog->eventsTablePage();
        package_members_page_id = catalog->packageMembersTablePage();

        ASSERT_NE(partitioned_tables_page_id, 0u);
        ASSERT_NE(partitions_page_id, 0u);
        ASSERT_NE(table_inheritance_page_id, 0u);
        ASSERT_NE(languages_page_id, 0u);
        ASSERT_NE(events_page_id, 0u);
        ASSERT_NE(package_members_page_id, 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, partitioned_tables_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, partitions_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, table_inheritance_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, languages_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, events_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, package_members_page_id, &ctx), Status::OK) << ctx.message;

        ASSERT_EQ(db.sync(&ctx), Status::OK) << ctx.message;
        db.close();
    }

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        EXPECT_EQ(catalog->partitionedTablesTablePage(), partitioned_tables_page_id);
        EXPECT_EQ(catalog->partitionsTablePage(), partitions_page_id);
        EXPECT_EQ(catalog->tableInheritanceTablePage(), table_inheritance_page_id);
        EXPECT_EQ(catalog->languagesTablePage(), languages_page_id);
        EXPECT_EQ(catalog->eventsTablePage(), events_page_id);
        EXPECT_EQ(catalog->packageMembersTablePage(), package_members_page_id);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->partitionedTablesTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->partitionsTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->tableInheritanceTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->languagesTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->eventsTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->packageMembersTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesIndexMetadataExtensionCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_index_metadata_extension_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    uint32_t index_columns_page_id = 0;
    uint32_t index_opclass_page_id = 0;
    uint32_t index_opclass_fn_page_id = 0;
    uint32_t index_options_page_id = 0;
    uint32_t index_access_methods_page_id = 0;
    uint32_t index_maintenance_page_id = 0;
    uint32_t index_maintenance_deltas_page_id = 0;
    uint32_t index_build_deltas_page_id = 0;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        index_columns_page_id = catalog->indexColumnsTablePage();
        index_opclass_page_id = catalog->indexOpclassTablePage();
        index_opclass_fn_page_id = catalog->indexOpclassFunctionTablePage();
        index_options_page_id = catalog->indexOptionsTablePage();
        index_access_methods_page_id = catalog->indexAccessMethodsTablePage();
        index_maintenance_page_id = catalog->indexMaintenanceTablePage();
        index_maintenance_deltas_page_id = catalog->indexMaintenanceDeltasTablePage();
        index_build_deltas_page_id = catalog->indexBuildDeltasTablePage();

        ASSERT_NE(index_columns_page_id, 0u);
        ASSERT_NE(index_opclass_page_id, 0u);
        ASSERT_NE(index_opclass_fn_page_id, 0u);
        ASSERT_NE(index_options_page_id, 0u);
        ASSERT_NE(index_access_methods_page_id, 0u);
        ASSERT_NE(index_maintenance_page_id, 0u);
        ASSERT_NE(index_maintenance_deltas_page_id, 0u);
        ASSERT_NE(index_build_deltas_page_id, 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, index_columns_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, index_opclass_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, index_opclass_fn_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, index_options_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, index_access_methods_page_id, &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, index_maintenance_page_id, &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, index_maintenance_deltas_page_id, &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, index_build_deltas_page_id, &ctx), Status::OK)
            << ctx.message;

        ASSERT_EQ(db.sync(&ctx), Status::OK) << ctx.message;
        db.close();
    }

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        EXPECT_EQ(catalog->indexColumnsTablePage(), index_columns_page_id);
        EXPECT_EQ(catalog->indexOpclassTablePage(), index_opclass_page_id);
        EXPECT_EQ(catalog->indexOpclassFunctionTablePage(), index_opclass_fn_page_id);
        EXPECT_EQ(catalog->indexOptionsTablePage(), index_options_page_id);
        EXPECT_EQ(catalog->indexAccessMethodsTablePage(), index_access_methods_page_id);
        EXPECT_EQ(catalog->indexMaintenanceTablePage(), index_maintenance_page_id);
        EXPECT_EQ(catalog->indexMaintenanceDeltasTablePage(), index_maintenance_deltas_page_id);
        EXPECT_EQ(catalog->indexBuildDeltasTablePage(), index_build_deltas_page_id);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->indexColumnsTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->indexOpclassTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->indexOpclassFunctionTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->indexOptionsTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->indexAccessMethodsTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->indexMaintenanceTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->indexMaintenanceDeltasTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->indexBuildDeltasTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesIndexTelemetryExtensionCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_index_telemetry_extension_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    uint32_t index_stats_page_id = 0;
    uint32_t index_usage_page_id = 0;
    uint32_t index_contention_page_id = 0;
    uint32_t index_storage_page_id = 0;
    uint32_t index_health_page_id = 0;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        index_stats_page_id = catalog->indexStatsTablePage();
        index_usage_page_id = catalog->indexUsageTablePage();
        index_contention_page_id = catalog->indexContentionTablePage();
        index_storage_page_id = catalog->indexStorageTablePage();
        index_health_page_id = catalog->indexHealthTablePage();

        ASSERT_NE(index_stats_page_id, 0u);
        ASSERT_NE(index_usage_page_id, 0u);
        ASSERT_NE(index_contention_page_id, 0u);
        ASSERT_NE(index_storage_page_id, 0u);
        ASSERT_NE(index_health_page_id, 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, index_stats_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, index_usage_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, index_contention_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, index_storage_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, index_health_page_id, &ctx), Status::OK) << ctx.message;

        ASSERT_EQ(db.sync(&ctx), Status::OK) << ctx.message;
        db.close();
    }

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        EXPECT_EQ(catalog->indexStatsTablePage(), index_stats_page_id);
        EXPECT_EQ(catalog->indexUsageTablePage(), index_usage_page_id);
        EXPECT_EQ(catalog->indexContentionTablePage(), index_contention_page_id);
        EXPECT_EQ(catalog->indexStorageTablePage(), index_storage_page_id);
        EXPECT_EQ(catalog->indexHealthTablePage(), index_health_page_id);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->indexStatsTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->indexUsageTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->indexContentionTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->indexStorageTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->indexHealthTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesStorageExtensionCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_storage_extension_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    uint32_t filespace_stats_page_id = 0;
    uint32_t lob_page_id = 0;
    uint32_t lob_page_map_id = 0;
    uint32_t backup_history_page_id = 0;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        filespace_stats_page_id = catalog->filespaceStatsTablePage();
        lob_page_id = catalog->lobTablePage();
        lob_page_map_id = catalog->lobPageTablePage();
        backup_history_page_id = catalog->backupHistoryTablePage();

        ASSERT_NE(filespace_stats_page_id, 0u);
        ASSERT_NE(lob_page_id, 0u);
        ASSERT_NE(lob_page_map_id, 0u);
        ASSERT_NE(backup_history_page_id, 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, filespace_stats_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, lob_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, lob_page_map_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, backup_history_page_id, &ctx), Status::OK) << ctx.message;

        ASSERT_EQ(db.sync(&ctx), Status::OK) << ctx.message;
        db.close();
    }

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        EXPECT_EQ(catalog->filespaceStatsTablePage(), filespace_stats_page_id);
        EXPECT_EQ(catalog->lobTablePage(), lob_page_id);
        EXPECT_EQ(catalog->lobPageTablePage(), lob_page_map_id);
        EXPECT_EQ(catalog->backupHistoryTablePage(), backup_history_page_id);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->filespaceStatsTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->lobTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->lobPageTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->backupHistoryTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesRuntimeContextCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_runtime_context_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    uint32_t connection_page_id = 0;
    uint32_t transaction_page_id = 0;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        connection_page_id = catalog->connectionTablePage();
        transaction_page_id = catalog->transactionTablePage();

        ASSERT_NE(connection_page_id, 0u);
        ASSERT_NE(transaction_page_id, 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, connection_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, transaction_page_id, &ctx), Status::OK) << ctx.message;

        ASSERT_EQ(db.sync(&ctx), Status::OK) << ctx.message;
        db.close();
    }

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        EXPECT_EQ(catalog->connectionTablePage(), connection_page_id);
        EXPECT_EQ(catalog->transactionTablePage(), transaction_page_id);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->connectionTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->transactionTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesSecurityExtensionCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_security_extension_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    uint32_t auth_mapping_page_id = 0;
    uint32_t role_setting_page_id = 0;
    uint32_t security_label_page_id = 0;
    uint32_t security_class_page_id = 0;
    uint32_t cert_registry_page_id = 0;
    uint32_t private_key_store_page_id = 0;
    uint32_t trust_anchor_page_id = 0;
    uint32_t channel_cert_binding_page_id = 0;
    uint32_t cert_revocation_page_id = 0;
    uint32_t pki_distribution_state_page_id = 0;
    uint32_t trust_anchor_rollover_page_id = 0;
    uint32_t encryption_profile_page_id = 0;
    uint32_t encryption_key_page_id = 0;
    uint32_t encryption_key_shard_page_id = 0;
    uint32_t encryption_bootstrap_info_page_id = 0;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        auth_mapping_page_id = catalog->authMappingTablePage();
        role_setting_page_id = catalog->roleSettingTablePage();
        security_label_page_id = catalog->securityLabelTablePage();
        security_class_page_id = catalog->securityClassTablePage();
        cert_registry_page_id = catalog->certRegistryTablePage();
        private_key_store_page_id = catalog->privateKeyStoreTablePage();
        trust_anchor_page_id = catalog->trustAnchorTablePage();
        channel_cert_binding_page_id = catalog->channelCertBindingTablePage();
        cert_revocation_page_id = catalog->certRevocationTablePage();
        pki_distribution_state_page_id = catalog->pkiDistributionStateTablePage();
        trust_anchor_rollover_page_id = catalog->trustAnchorRolloverTablePage();
        encryption_profile_page_id = catalog->encryptionProfileTablePage();
        encryption_key_page_id = catalog->encryptionKeyTablePage();
        encryption_key_shard_page_id = catalog->encryptionKeyShardTablePage();
        encryption_bootstrap_info_page_id = catalog->encryptionBootstrapInfoTablePage();

        ASSERT_NE(auth_mapping_page_id, 0u);
        ASSERT_NE(role_setting_page_id, 0u);
        ASSERT_NE(security_label_page_id, 0u);
        ASSERT_NE(security_class_page_id, 0u);
        ASSERT_NE(cert_registry_page_id, 0u);
        ASSERT_NE(private_key_store_page_id, 0u);
        ASSERT_NE(trust_anchor_page_id, 0u);
        ASSERT_NE(channel_cert_binding_page_id, 0u);
        ASSERT_NE(cert_revocation_page_id, 0u);
        ASSERT_NE(pki_distribution_state_page_id, 0u);
        ASSERT_NE(trust_anchor_rollover_page_id, 0u);
        ASSERT_NE(encryption_profile_page_id, 0u);
        ASSERT_NE(encryption_key_page_id, 0u);
        ASSERT_NE(encryption_key_shard_page_id, 0u);
        ASSERT_NE(encryption_bootstrap_info_page_id, 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, auth_mapping_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, role_setting_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, security_label_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, security_class_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, cert_registry_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, private_key_store_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, trust_anchor_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, channel_cert_binding_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, cert_revocation_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, pki_distribution_state_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, trust_anchor_rollover_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, encryption_profile_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, encryption_key_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, encryption_key_shard_page_id, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, encryption_bootstrap_info_page_id, &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesClusterNodeAndClockCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_cluster_node_clock_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ASSERT_NE(catalog->nodeTablePage(), 0u);
        ASSERT_NE(catalog->nodeRoleBindingTablePage(), 0u);
        ASSERT_NE(catalog->nodeServiceTablePage(), 0u);
        ASSERT_NE(catalog->nodeCapabilityTablePage(), 0u);
        ASSERT_NE(catalog->clockPolicyTablePage(), 0u);
        ASSERT_NE(catalog->clockSourceTablePage(), 0u);
        ASSERT_NE(catalog->nodeClockStateTablePage(), 0u);
        ASSERT_NE(catalog->clockViolationEventTablePage(), 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->nodeTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->nodeRoleBindingTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->nodeServiceTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->nodeCapabilityTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->clockPolicyTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->clockSourceTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->nodeClockStateTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->clockViolationEventTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesShardingCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_sharding_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ASSERT_NE(catalog->clusterTablePage(), 0u);
        ASSERT_NE(catalog->shardPolicyTablePage(), 0u);
        ASSERT_NE(catalog->shardPolicyParamTablePage(), 0u);
        ASSERT_NE(catalog->shardKeyTablePage(), 0u);
        ASSERT_NE(catalog->shardTablePage(), 0u);
        ASSERT_NE(catalog->shardScopeTablePage(), 0u);
        ASSERT_NE(catalog->shardRangeTablePage(), 0u);
        ASSERT_NE(catalog->shardReplicaTablePage(), 0u);
        ASSERT_NE(catalog->shardMigrationTablePage(), 0u);
        ASSERT_NE(catalog->shardZoneTablePage(), 0u);
        ASSERT_NE(catalog->shardZoneRangeTablePage(), 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->clusterTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->shardPolicyTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->shardPolicyParamTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->shardKeyTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->shardTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->shardScopeTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->shardRangeTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->shardReplicaTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->shardMigrationTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->shardZoneTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->shardZoneRangeTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesRoutingAdmissionCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_routing_admission_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ASSERT_NE(catalog->workloadClassTablePage(), 0u);
        ASSERT_NE(catalog->workloadRouteTablePage(), 0u);
        ASSERT_NE(catalog->admissionPolicyTablePage(), 0u);
        ASSERT_NE(catalog->admissionBindingTablePage(), 0u);
        ASSERT_NE(catalog->sloProfileTablePage(), 0u);
        ASSERT_NE(catalog->sloBindingTablePage(), 0u);
        ASSERT_NE(catalog->sloWindowTablePage(), 0u);
        ASSERT_NE(catalog->sloBurnEventTablePage(), 0u);
        ASSERT_NE(catalog->autoscalePolicyTablePage(), 0u);
        ASSERT_NE(catalog->autoscaleActionTablePage(), 0u);
        ASSERT_NE(catalog->admissionTuningEventTablePage(), 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->workloadClassTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->workloadRouteTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->admissionPolicyTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->admissionBindingTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->sloProfileTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->sloBindingTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->sloWindowTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->sloBurnEventTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->autoscalePolicyTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->autoscaleActionTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->admissionTuningEventTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesIncidentHealingAlertCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_incident_healing_alert_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ASSERT_NE(catalog->clusterPolicyTablePage(), 0u);
        ASSERT_NE(catalog->failureDetectorTablePage(), 0u);
        ASSERT_NE(catalog->alertRuleTablePage(), 0u);
        ASSERT_NE(catalog->alertTargetTablePage(), 0u);
        ASSERT_NE(catalog->alertRouteTablePage(), 0u);
        ASSERT_NE(catalog->alertEventTablePage(), 0u);
        ASSERT_NE(catalog->alertAckTablePage(), 0u);
        ASSERT_NE(catalog->alertSilenceTablePage(), 0u);
        ASSERT_NE(catalog->networkPartitionEventTablePage(), 0u);
        ASSERT_NE(catalog->networkPartitionMemberTablePage(), 0u);
        ASSERT_NE(catalog->healingPolicyTablePage(), 0u);
        ASSERT_NE(catalog->healingActionTablePage(), 0u);
        ASSERT_NE(catalog->healingActionParamTablePage(), 0u);
        ASSERT_NE(catalog->healingRunTablePage(), 0u);
        ASSERT_NE(catalog->healingStepTablePage(), 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->clusterPolicyTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->failureDetectorTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->alertRuleTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->alertTargetTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->alertRouteTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->alertEventTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->alertAckTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->alertSilenceTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->networkPartitionEventTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->networkPartitionMemberTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->healingPolicyTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->healingActionTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->healingActionParamTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->healingRunTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->healingStepTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesSchedulerExtensionCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_scheduler_extension_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ASSERT_NE(catalog->jobTypeTablePage(), 0u);
        ASSERT_NE(catalog->jobTypeParamTablePage(), 0u);
        ASSERT_NE(catalog->jobParamTablePage(), 0u);
        ASSERT_NE(catalog->jobScheduleTablePage(), 0u);
        ASSERT_NE(catalog->jobTypePolicyTablePage(), 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->jobTypeTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->jobTypeParamTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->jobParamTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->jobScheduleTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->jobTypePolicyTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesRemoteConnectorExtensionCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_remote_connector_extension_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ASSERT_NE(catalog->remoteConnectorTablePage(), 0u);
        ASSERT_NE(catalog->remoteConnectorCapabilityTablePage(), 0u);
        ASSERT_NE(catalog->remoteMetadataSnapshotTablePage(), 0u);
        ASSERT_NE(catalog->remoteMetadataObjectTablePage(), 0u);
        ASSERT_NE(catalog->remoteMetadataColumnTablePage(), 0u);
        ASSERT_NE(catalog->remoteSchemaMappingTablePage(), 0u);
        ASSERT_NE(catalog->remotePassthroughPolicyTablePage(), 0u);
        ASSERT_NE(catalog->remotePreparedStatementTablePage(), 0u);
        ASSERT_NE(catalog->remoteTxnBindingTablePage(), 0u);
        ASSERT_NE(catalog->remoteExecutionAuditTablePage(), 0u);
        ASSERT_NE(catalog->remoteErrorTablePage(), 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->remoteConnectorTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->remoteConnectorCapabilityTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->remoteMetadataSnapshotTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->remoteMetadataObjectTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->remoteMetadataColumnTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->remoteSchemaMappingTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->remotePassthroughPolicyTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->remotePreparedStatementTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->remoteTxnBindingTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->remoteExecutionAuditTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->remoteErrorTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesReplicationRuntimeConflictCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_replication_runtime_conflict_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ASSERT_NE(catalog->replicationChannelTablePage(), 0u);
        ASSERT_NE(catalog->replicationChannelMemberTablePage(), 0u);
        ASSERT_NE(catalog->replicationOriginTablePage(), 0u);
        ASSERT_NE(catalog->replicationCursorTablePage(), 0u);
        ASSERT_NE(catalog->replicationOriginProgressTablePage(), 0u);
        ASSERT_NE(catalog->replicationTxnBatchTablePage(), 0u);
        ASSERT_NE(catalog->replicationApplyLogTablePage(), 0u);
        ASSERT_NE(catalog->replicationRetryQueueTablePage(), 0u);
        ASSERT_NE(catalog->replicationConflictTablePage(), 0u);
        ASSERT_NE(catalog->replicationSplitBrainEventTablePage(), 0u);
        ASSERT_NE(catalog->replicationErrorTablePage(), 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->replicationChannelTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->replicationChannelMemberTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->replicationOriginTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->replicationCursorTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->replicationOriginProgressTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->replicationTxnBatchTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->replicationApplyLogTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->replicationRetryQueueTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->replicationConflictTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->replicationSplitBrainEventTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->replicationErrorTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesExtensionPublicationSubscriptionCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_extension_publication_subscription_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ASSERT_NE(catalog->extensionTablePage(), 0u);
        ASSERT_NE(catalog->publicationTablePage(), 0u);
        ASSERT_NE(catalog->publicationTableLinkTablePage(), 0u);
        ASSERT_NE(catalog->publicationSchemaTablePage(), 0u);
        ASSERT_NE(catalog->subscriptionTablePage(), 0u);
        ASSERT_NE(catalog->subscriptionTableLinkTablePage(), 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->extensionTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->publicationTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->publicationTableLinkTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->publicationSchemaTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->subscriptionTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->subscriptionTableLinkTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesClusterFabricCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_cluster_fabric_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ASSERT_NE(catalog->clusterFabricLinkTablePage(), 0u);
        ASSERT_NE(catalog->clusterFabricSessionTablePage(), 0u);
        ASSERT_NE(catalog->clusterFabricTxnTablePage(), 0u);
        ASSERT_NE(catalog->clusterFabricTaskTablePage(), 0u);
        ASSERT_NE(catalog->clusterFabricTaskChunkTablePage(), 0u);
        ASSERT_NE(catalog->clusterFabricEventTablePage(), 0u);
        ASSERT_NE(catalog->clusterFabricErrorTablePage(), 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->clusterFabricLinkTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->clusterFabricSessionTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->clusterFabricTxnTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->clusterFabricTaskTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->clusterFabricTaskChunkTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->clusterFabricEventTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->clusterFabricErrorTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesOlapCubeCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_olap_cube_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ASSERT_NE(catalog->olapWatermarkTablePage(), 0u);
        ASSERT_NE(catalog->olapPartitionTablePage(), 0u);
        ASSERT_NE(catalog->olapSegmentTablePage(), 0u);
        ASSERT_NE(catalog->olapIngestLogTablePage(), 0u);
        ASSERT_NE(catalog->cubeTablePage(), 0u);
        ASSERT_NE(catalog->cubeDimensionTablePage(), 0u);
        ASSERT_NE(catalog->cubeLevelTablePage(), 0u);
        ASSERT_NE(catalog->cubeHierarchyTablePage(), 0u);
        ASSERT_NE(catalog->cubeHierarchyLevelTablePage(), 0u);
        ASSERT_NE(catalog->cubeMeasureTablePage(), 0u);
        ASSERT_NE(catalog->cubeMaterializationTablePage(), 0u);
        ASSERT_NE(catalog->cubeRefreshPolicyTablePage(), 0u);
        ASSERT_NE(catalog->cubeJobTablePage(), 0u);
        ASSERT_NE(catalog->cubeJobStepTablePage(), 0u);
        ASSERT_NE(catalog->cubeStatsTablePage(), 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->olapWatermarkTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->olapPartitionTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->olapSegmentTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->olapIngestLogTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->cubeTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->cubeDimensionTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->cubeLevelTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->cubeHierarchyTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->cubeHierarchyLevelTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->cubeMeasureTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->cubeMaterializationTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->cubeRefreshPolicyTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->cubeJobTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->cubeJobStepTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->cubeStatsTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesTextSearchCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_text_search_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ASSERT_NE(catalog->tsParserTablePage(), 0u);
        ASSERT_NE(catalog->tsTemplateTablePage(), 0u);
        ASSERT_NE(catalog->tsDictionaryTablePage(), 0u);
        ASSERT_NE(catalog->tsConfigTablePage(), 0u);
        ASSERT_NE(catalog->tsConfigMapTablePage(), 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->tsParserTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->tsTemplateTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->tsDictionaryTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->tsConfigTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->tsConfigMapTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesEngineSpecificCompatibilityCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_engine_specific_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ASSERT_NE(catalog->blobFilterTablePage(), 0u);
        ASSERT_NE(catalog->triggerMessageTablePage(), 0u);
        ASSERT_NE(catalog->columnDropHistoryTablePage(), 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->blobFilterTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->triggerMessageTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->columnDropHistoryTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}

TEST(CatalogDatabaseBootstrapTest, CreatesSblrExecutionArtifactCatalogFamilyPages)
{
    std::string db_path = uniqueTestDbPath("test_catalog_sblr_artifact_pages_bootstrap");
    std::remove(db_path.c_str());

    ErrorContext ctx;
    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;

    {
        Database db;
        ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;
        auto* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ASSERT_NE(catalog->sblrModuleTablePage(), 0u);
        ASSERT_NE(catalog->sblrPlanTablePage(), 0u);
        ASSERT_NE(catalog->sblrPlanDependencyTablePage(), 0u);
        ASSERT_NE(catalog->sblrStatementNormTablePage(), 0u);
        ASSERT_NE(catalog->sblrArtifactTablePage(), 0u);
        ASSERT_NE(catalog->sblrArtifactStatsTablePage(), 0u);
        ASSERT_NE(catalog->sblrCompilerTargetTablePage(), 0u);
        ASSERT_NE(catalog->sblrCompileQueueTablePage(), 0u);

        ASSERT_EQ(assertHeapCatalogPage(db, catalog->sblrModuleTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->sblrPlanTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->sblrPlanDependencyTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->sblrStatementNormTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->sblrArtifactTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->sblrArtifactStatsTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->sblrCompilerTargetTablePage(), &ctx), Status::OK)
            << ctx.message;
        ASSERT_EQ(assertHeapCatalogPage(db, catalog->sblrCompileQueueTablePage(), &ctx), Status::OK)
            << ctx.message;

        db.close();
    }

    std::remove(db_path.c_str());
}
