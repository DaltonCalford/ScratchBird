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

#include <cstring>
#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

class CatalogCharsetCollationExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_charset_collation_extension_contract_" +
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

    ID createCharset(uint16_t charset_id, const std::string& charset_name)
    {
        CatalogManager::CharsetInfo cs{};
        cs.charset_id = charset_id;
        cs.name = charset_name;
        cs.description = "CAT-012 test charset";
        cs.min_bytes = 1;
        cs.max_bytes = 4;
        cs.variable_width = 1;
        cs.default_collation_id = 0;

        ErrorContext ctx;
        EXPECT_EQ(catalog_->createCharset(cs, &ctx), Status::OK) << ctx.message;

        CatalogManager::CharsetInfo fetched{};
        EXPECT_EQ(catalog_->getCharset(charset_id, fetched, &ctx), Status::OK) << ctx.message;
        return fetched.charset_uuid;
    }

    void createCollation(uint32_t collation_id,
                         const std::string& collation_name,
                         uint16_t charset_id,
                         const ID& charset_uuid)
    {
        CatalogManager::CollationCatalogInfo col{};
        col.collation_id = collation_id;
        col.name = collation_name;
        col.charset_id = charset_id;
        col.charset_uuid = charset_uuid;
        col.collation_type = 0;
        col.strength = 0;
        col.pad_space = 1;
        col.is_default = 0;
        std::strncpy(col.locale, "en_US", sizeof(col.locale) - 1);

        ErrorContext ctx;
        EXPECT_EQ(catalog_->createCollation(col, &ctx), Status::OK) << ctx.message;
    }
};

TEST_F(CatalogCharsetCollationExtensionContractTest, CharsetAliasContracts)
{
    ID charset_uuid = createCharset(250, "cat012_utf8");

    ErrorContext ctx;
    ID alias_id{};

    CatalogManager::CharsetAliasCatalogInfo invalid_missing{};
    EXPECT_EQ(catalog_->upsertCharsetAliasCatalogEntry(invalid_missing, alias_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::CharsetAliasCatalogInfo invalid_charset{};
    invalid_charset.charset_id = generateUuidV7();
    invalid_charset.bundle_id = generateUuidV7();
    invalid_charset.alias_name = "utf8";
    invalid_charset.normalized_name = "utf8";
    EXPECT_EQ(catalog_->upsertCharsetAliasCatalogEntry(invalid_charset, alias_id, &ctx),
              Status::NOT_FOUND);

    CatalogManager::CharsetAliasCatalogInfo row{};
    row.charset_id = charset_uuid;
    row.bundle_id = generateUuidV7();
    row.alias_name = "UTF-8";
    row.normalized_name = "utf8";
    ASSERT_EQ(catalog_->upsertCharsetAliasCatalogEntry(row, alias_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_NE(alias_id, ID{});

    CatalogManager::CharsetAliasCatalogInfo duplicate = row;
    duplicate.alias_id = generateUuidV7();
    duplicate.alias_name = "utf8";
    duplicate.normalized_name = "utf8";
    ID duplicate_id{};
    EXPECT_EQ(catalog_->upsertCharsetAliasCatalogEntry(duplicate, duplicate_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::CharsetAliasCatalogInfo fetched{};
    ASSERT_EQ(catalog_->getCharsetAliasCatalogEntry(alias_id, fetched, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(fetched.charset_id, charset_uuid);
    EXPECT_EQ(fetched.normalized_name, "UTF8");

    CatalogManager::CharsetAliasCatalogInfo fetched_by_name{};
    ASSERT_EQ(catalog_->getCharsetAliasCatalogEntryByNormalizedName("UTF8", fetched_by_name, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(fetched_by_name.alias_id, alias_id);

    std::vector<CatalogManager::CharsetAliasCatalogInfo> rows;
    ASSERT_EQ(catalog_->listCharsetAliasCatalogEntries(charset_uuid, rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].alias_id, alias_id);

    ASSERT_EQ(catalog_->deleteCharsetAliasCatalogEntry(alias_id, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(catalog_->getCharsetAliasCatalogEntry(alias_id, fetched, &ctx), Status::NOT_FOUND);
}

TEST_F(CatalogCharsetCollationExtensionContractTest, CollationTailoringContracts)
{
    ID charset_uuid = createCharset(251, "cat012_utf8_coll");
    createCollation(2601, "cat012_collation", 251, charset_uuid);

    ErrorContext ctx;
    ID tailoring_id{};

    CatalogManager::CollationTailoringCatalogInfo invalid_kind{};
    invalid_kind.collation_id = 2601;
    invalid_kind.bundle_id = generateUuidV7();
    invalid_kind.tailoring_kind = static_cast<CatalogManager::CollationTailoringKind>(99);
    invalid_kind.tailoring_json = std::string("{\"k\":\"v\"}");
    invalid_kind.tailoring_hash = "abc";
    EXPECT_EQ(catalog_->upsertCollationTailoringCatalogEntry(invalid_kind, tailoring_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::CollationTailoringCatalogInfo missing_payload{};
    missing_payload.collation_id = 2601;
    missing_payload.bundle_id = generateUuidV7();
    missing_payload.tailoring_kind = CatalogManager::CollationTailoringKind::LOCALE;
    missing_payload.tailoring_hash = "hash-missing-payload";
    EXPECT_EQ(catalog_->upsertCollationTailoringCatalogEntry(missing_payload, tailoring_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::CollationTailoringCatalogInfo missing_collation{};
    missing_collation.collation_id = 999999;
    missing_collation.bundle_id = generateUuidV7();
    missing_collation.tailoring_kind = CatalogManager::CollationTailoringKind::UCA;
    missing_collation.tailoring_json = std::string("{\"uca\":\"v14\"}");
    missing_collation.tailoring_hash = "hash-missing-collation";
    EXPECT_EQ(catalog_->upsertCollationTailoringCatalogEntry(missing_collation, tailoring_id, &ctx),
              Status::NOT_FOUND);

    CatalogManager::CollationTailoringCatalogInfo row{};
    row.collation_id = 2601;
    row.bundle_id = generateUuidV7();
    row.tailoring_kind = CatalogManager::CollationTailoringKind::UCA;
    row.tailoring_json = std::string("{\"uca\":\"v14\"}");
    row.tailoring_hash = "hash-uca-v14";
    ASSERT_EQ(catalog_->upsertCollationTailoringCatalogEntry(row, tailoring_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_NE(tailoring_id, ID{});

    CatalogManager::CollationTailoringCatalogInfo duplicate = row;
    duplicate.tailoring_id = generateUuidV7();
    duplicate.tailoring_json = std::string("{\"uca\":\"v14-custom\"}");
    ID duplicate_id{};
    EXPECT_EQ(catalog_->upsertCollationTailoringCatalogEntry(duplicate, duplicate_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::CollationTailoringCatalogInfo fetched{};
    ASSERT_EQ(catalog_->getCollationTailoringCatalogEntry(tailoring_id, fetched, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(fetched.collation_id, 2601u);
    ASSERT_TRUE(fetched.tailoring_json.has_value());
    EXPECT_EQ(fetched.tailoring_json.value(), "{\"uca\":\"v14\"}");

    std::vector<CatalogManager::CollationTailoringCatalogInfo> rows;
    ASSERT_EQ(catalog_->listCollationTailoringCatalogEntries(2601, rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].tailoring_id, tailoring_id);

    ASSERT_EQ(catalog_->deleteCollationTailoringCatalogEntry(tailoring_id, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(catalog_->getCollationTailoringCatalogEntry(tailoring_id, fetched, &ctx),
              Status::NOT_FOUND);
}
