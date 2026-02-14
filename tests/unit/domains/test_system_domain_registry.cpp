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
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/domain_manager.h"
#include "test_helpers.h"

using namespace scratchbird::core;

namespace {
auto toUpperAscii(std::string value) -> std::string
{
    for (char& ch : value)
    {
        if (ch >= 'a' && ch <= 'z')
        {
            ch = static_cast<char>(ch - ('a' - 'A'));
        }
    }
    return value;
}

auto collectSystemDomainIds(Database& db,
                            std::unordered_map<std::string, std::string>& out,
                            ErrorContext* ctx) -> Status
{
    out.clear();
    auto* catalog = db.catalog_manager();
    auto* domains = db.domain_manager();
    if (!catalog || !domains)
    {
        return Status::INVALID_ARGUMENT;
    }

    CatalogManager::SchemaInfo sys_schema;
    Status status = catalog->getSchema("sys", sys_schema, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::vector<DomainInfo> domain_infos;
    status = domains->listDomains(sys_schema.schema_id, domain_infos, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    for (const auto& info : domain_infos)
    {
        const std::string upper_name = toUpperAscii(info.domain_name);
        if (upper_name.rfind("[SB_", 0) == 0)
        {
            out[upper_name] = info.domain_id.toString();
        }
    }

    return Status::OK;
}

auto collectVisibleDomainNames(Database& db,
                               const DomainManager::DomainListOptions& options,
                               std::unordered_set<std::string>& out,
                               ErrorContext* ctx) -> Status
{
    out.clear();
    auto* catalog = db.catalog_manager();
    auto* domains = db.domain_manager();
    if (!catalog || !domains)
    {
        return Status::INVALID_ARGUMENT;
    }

    CatalogManager::SchemaInfo sys_schema;
    Status status = catalog->getSchema("sys", sys_schema, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::vector<DomainInfo> domain_infos;
    status = domains->listDomainsVisible(sys_schema.schema_id, options, domain_infos, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    for (const auto& info : domain_infos)
    {
        out.insert(toUpperAscii(info.domain_name));
    }
    return Status::OK;
}
} // namespace

TEST(SystemDomainRegistryTest, DeterministicIdsAcrossFreshDatabases)
{
    ErrorContext ctx;

    const std::string db_path_a =
        scratchbird::testing::uniqueTestDbPath("system_domain_registry_a", ".db");
    const std::string db_path_b =
        scratchbird::testing::uniqueTestDbPath("system_domain_registry_b", ".db");
    std::filesystem::remove(db_path_a);
    std::filesystem::remove(db_path_b);

    std::unordered_map<std::string, std::string> ids_a;
    std::unordered_map<std::string, std::string> ids_b;

    {
        ASSERT_EQ(Database::create(db_path_a, 8192, &ctx), Status::OK) << ctx.message;
        Database db_a;
        ASSERT_EQ(db_a.open(db_path_a, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(collectSystemDomainIds(db_a, ids_a, &ctx), Status::OK) << ctx.message;
        db_a.close();
    }

    {
        ASSERT_EQ(Database::create(db_path_b, 8192, &ctx), Status::OK) << ctx.message;
        Database db_b;
        ASSERT_EQ(db_b.open(db_path_b, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(collectSystemDomainIds(db_b, ids_b, &ctx), Status::OK) << ctx.message;
        db_b.close();
    }

    std::filesystem::remove(db_path_a);
    std::filesystem::remove(db_path_b);

    ASSERT_FALSE(ids_a.empty());
    ASSERT_EQ(ids_a.size(), ids_b.size());
    ASSERT_EQ(ids_a, ids_b);

    auto it_uuid_v7 = ids_a.find("[SB_DOM]UUID_V7_INTERNAL");
    ASSERT_NE(it_uuid_v7, ids_a.end());
    EXPECT_FALSE(it_uuid_v7->second.empty());

    auto it_cat_identifier = ids_a.find("[SB_DOM]CAT_IDENTIFIER");
    ASSERT_NE(it_cat_identifier, ids_a.end());
    EXPECT_FALSE(it_cat_identifier->second.empty());
}

TEST(SystemDomainRegistryTest, NoLegacySbdbPrefixInSystemDomains)
{
    ErrorContext ctx;
    const std::string db_path =
        scratchbird::testing::uniqueTestDbPath("system_domain_registry_no_legacy", ".db");
    std::filesystem::remove(db_path);

    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;
    Database db;
    ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;

    std::unordered_map<std::string, std::string> ids;
    ASSERT_EQ(collectSystemDomainIds(db, ids, &ctx), Status::OK) << ctx.message;
    ASSERT_FALSE(ids.empty());

    for (const auto& [name, id] : ids)
    {
        (void)id;
        EXPECT_EQ(name.find("SBDB$"), std::string::npos) << name;
    }

    db.close();
    std::filesystem::remove(db_path);
}

TEST(SystemDomainRegistryTest, CanonicalFixedRegistrySampleParity)
{
    ErrorContext ctx;
    const std::string db_path =
        scratchbird::testing::uniqueTestDbPath("system_domain_registry_fixed_parity", ".db");
    std::filesystem::remove(db_path);

    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;
    Database db;
    ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;

    std::unordered_map<std::string, std::string> ids;
    ASSERT_EQ(collectSystemDomainIds(db, ids, &ctx), Status::OK) << ctx.message;

    const std::vector<std::pair<std::string, std::string>> expected{
        {"[SB_DOM]UUID_V7_INTERNAL", "0ae006a9-a092-5d2b-8df3-f77aefae1fde"},
        {"[SB_DOM]CAT_IDENTIFIER", "e990397d-0a51-58e7-9f77-249152f4f4b2"},
        {"[SB_DOM]CAT_BLOB_TEXT", "4bd1aade-9133-5f27-9eef-ae6f20f59ce9"},
        {"[SB_PG_DOM]SERIAL", "6f57c40a-8267-5ae1-9ff0-1810af81b78a"},
        {"[SB_MY_DOM]BOOL", "cea20fb7-6a32-564d-985e-c0e6b9624e3b"},
        {"[SB_CAS_DOM]VARINT", "8367586f-572d-5290-870c-b6ca5b7912e3"},
        {"[SB_MONGO_DOM]OBJECTID", "74701502-77f0-5a44-865e-3471fdde8195"},
        {"[SB_REDIS_DOM]STREAM", "96ba4d01-d6a0-539f-b22a-d32fe5ae79d5"}
    };

    for (const auto& [name, uuid] : expected)
    {
        auto it = ids.find(name);
        ASSERT_NE(it, ids.end()) << name;
        EXPECT_EQ(it->second, uuid) << name;
    }

    db.close();
    std::filesystem::remove(db_path);
}

TEST(SystemDomainRegistryTest, ReservedSystemDomainNamesRequireSystemBypass)
{
    ErrorContext ctx;
    const std::string db_path =
        scratchbird::testing::uniqueTestDbPath("system_domain_reserved_name_guard", ".db");
    std::filesystem::remove(db_path);

    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;
    Database db;
    ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;

    auto* catalog = db.catalog_manager();
    auto* domains = db.domain_manager();
    ASSERT_NE(catalog, nullptr);
    ASSERT_NE(domains, nullptr);

    CatalogManager::SchemaInfo sys_schema;
    ASSERT_EQ(catalog->getSchema("sys", sys_schema, &ctx), Status::OK) << ctx.message;

    ID created_id{};
    DomainManager::DomainCreateOptions user_options;
    Status status = domains->createBasicDomain(
        sys_schema.schema_id, "[sb_pg_dom]internal_user_attempt",
        DataType::INT32, 0, 0, user_options, created_id, &ctx);
    ASSERT_EQ(status, Status::INVALID_ARGUMENT);

    DomainManager::DomainCreateOptions system_options;
    system_options.allow_system_reserved_name = true;
    system_options.dialect_tag = "postgresql";
    status = domains->createBasicDomain(
        sys_schema.schema_id, "[sb_pg_dom]internal_system_install",
        DataType::INT32, 0, 0, system_options, created_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    db.close();
    std::filesystem::remove(db_path);
}

TEST(SystemDomainRegistryTest, VisibilityGatesSystemDomainsByDialectAndProfile)
{
    ErrorContext ctx;
    const std::string db_path =
        scratchbird::testing::uniqueTestDbPath("system_domain_visibility_gate", ".db");
    std::filesystem::remove(db_path);

    ASSERT_EQ(Database::create(db_path, 8192, &ctx), Status::OK) << ctx.message;
    Database db;
    ASSERT_EQ(db.open(db_path, &ctx), Status::OK) << ctx.message;

    auto* catalog = db.catalog_manager();
    auto* domains = db.domain_manager();
    ASSERT_NE(catalog, nullptr);
    ASSERT_NE(domains, nullptr);

    CatalogManager::SchemaInfo sys_schema;
    ASSERT_EQ(catalog->getSchema("sys", sys_schema, &ctx), Status::OK) << ctx.message;

    ID pg_type_id{};
    ASSERT_EQ(catalog->createEmulationType("postgresql", 18, 0, "", pg_type_id, &ctx), Status::OK)
        << ctx.message;

    DomainManager::DomainCreateOptions install_opts;
    install_opts.allow_system_reserved_name = true;
    install_opts.dialect_tag = "postgresql";
    ID pg_domain_id{};
    ASSERT_EQ(domains->createBasicDomain(
                  sys_schema.schema_id, "[sb_pg_dom]visibility_probe",
                  DataType::INT32, 0, 0, install_opts, pg_domain_id, &ctx),
              Status::OK)
        << ctx.message;

    install_opts.dialect_tag = "mysql";
    ID my_domain_id{};
    ASSERT_EQ(domains->createBasicDomain(
                  sys_schema.schema_id, "[sb_my_dom]visibility_probe",
                  DataType::INT32, 0, 0, install_opts, my_domain_id, &ctx),
              Status::OK)
        << ctx.message;

    DomainManager::DomainCreateOptions user_opts;
    ID user_domain_id{};
    ASSERT_EQ(domains->createBasicDomain(
                  sys_schema.schema_id, "user_visible_domain",
                  DataType::INT32, 0, 0, user_opts, user_domain_id, &ctx),
              Status::OK)
        << ctx.message;

    std::unordered_set<std::string> names;
    DomainManager::DomainListOptions options;
    options.include_system = true;
    options.dialect_tag = "postgresql";
    ASSERT_EQ(collectVisibleDomainNames(db, options, names, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(names.count("[SB_DOM]UUID_V7_INTERNAL") > 0);
    EXPECT_TRUE(names.count("[SB_PG_DOM]VISIBILITY_PROBE") > 0);
    EXPECT_TRUE(names.count("USER_VISIBLE_DOMAIN") > 0);
    EXPECT_EQ(names.count("[SB_MY_DOM]VISIBILITY_PROBE"), 0u);

    options.include_system = false;
    names.clear();
    ASSERT_EQ(collectVisibleDomainNames(db, options, names, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(names.count("USER_VISIBLE_DOMAIN") > 0);
    EXPECT_EQ(names.count("[SB_DOM]UUID_V7_INTERNAL"), 0u);
    EXPECT_EQ(names.count("[SB_PG_DOM]VISIBILITY_PROBE"), 0u);

    db.close();
    std::filesystem::remove(db_path);
}
