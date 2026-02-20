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
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

class CatalogTypeSchemaContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_type_schema_contract_" + std::to_string(getpid()) + ".db";
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

    ID upsertType(const ID& schema_id, const std::string& type_name)
    {
        CatalogManager::TypeCatalogInfo info{};
        info.schema_id = schema_id;
        info.type_name = type_name;
        info.type_kind = CatalogManager::TypeKind::SCALAR;

        ErrorContext ctx;
        ID type_id{};
        Status status = catalog_->upsertTypeCatalogEntry(info, type_id, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return type_id;
    }
};

TEST_F(CatalogTypeSchemaContractTest, TypeNameUniquenessIsSchemaScoped)
{
    ID schema_a = createSchemaPath("users.public.cat010_schema_a");
    ID schema_b = createSchemaPath("users.public.cat010_schema_b");

    ID type_a = upsertType(schema_a, "Amount");
    ASSERT_NE(type_a, ID{});

    CatalogManager::TypeCatalogInfo duplicate{};
    duplicate.schema_id = schema_a;
    duplicate.type_name = "amount";
    duplicate.type_kind = CatalogManager::TypeKind::SCALAR;

    ErrorContext ctx;
    ID duplicate_id{};
    Status duplicate_status = catalog_->upsertTypeCatalogEntry(duplicate, duplicate_id, &ctx);
    EXPECT_EQ(duplicate_status, Status::CONSTRAINT_VIOLATION);

    CatalogManager::TypeCatalogInfo cross_schema{};
    cross_schema.schema_id = schema_b;
    cross_schema.type_name = "amount";
    cross_schema.type_kind = CatalogManager::TypeKind::SCALAR;

    ID schema_b_type{};
    ASSERT_EQ(catalog_->upsertTypeCatalogEntry(cross_schema, schema_b_type, &ctx), Status::OK)
        << ctx.message;
}

TEST_F(CatalogTypeSchemaContractTest, TypeModifierEnforcesSingleValueAndKindMatch)
{
    ID schema_id = createSchemaPath("users.public.cat010_modifier");
    ID type_id = upsertType(schema_id, "varchar_like");

    CatalogManager::TypeModifierInfo mismatch{};
    mismatch.type_id = type_id;
    mismatch.modifier_key = static_cast<uint16_t>(CatalogManager::TypeModifierKey::LENGTH_CHARS);
    mismatch.value_kind = CatalogManager::TypeModifierValueKind::TEXT;
    mismatch.val_text = std::string("bad");

    ErrorContext ctx;
    EXPECT_EQ(catalog_->upsertTypeModifier(mismatch, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::TypeModifierInfo too_many{};
    too_many.type_id = type_id;
    too_many.modifier_key = static_cast<uint16_t>(CatalogManager::TypeModifierKey::LENGTH_CHARS);
    too_many.value_kind = CatalogManager::TypeModifierValueKind::U64;
    too_many.val_u64 = 32;
    too_many.val_i64 = 4;
    EXPECT_EQ(catalog_->upsertTypeModifier(too_many, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::TypeModifierInfo ok{};
    ok.type_id = type_id;
    ok.modifier_key = static_cast<uint16_t>(CatalogManager::TypeModifierKey::LENGTH_CHARS);
    ok.value_kind = CatalogManager::TypeModifierValueKind::U64;
    ok.val_u64 = 128;
    ASSERT_EQ(catalog_->upsertTypeModifier(ok, &ctx), Status::OK) << ctx.message;

    CatalogManager::TypeModifierInfo fetched{};
    ASSERT_EQ(catalog_->getTypeModifier(type_id, ok.modifier_key, fetched, &ctx), Status::OK)
        << ctx.message;
    ASSERT_TRUE(fetched.val_u64.has_value());
    EXPECT_EQ(fetched.val_u64.value(), 128u);
}

TEST_F(CatalogTypeSchemaContractTest, TypeIoIsUniquePerTypeAndUpsertedInPlace)
{
    ID schema_id = createSchemaPath("users.public.cat010_type_io");
    ID type_id = upsertType(schema_id, "bytea_like");

    CatalogManager::TypeIoInfo invalid{};
    invalid.type_id = type_id;
    invalid.input_fn_id = generateUuidV7();

    ErrorContext ctx;
    EXPECT_EQ(catalog_->upsertTypeIo(invalid, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::TypeIoInfo first{};
    first.type_id = type_id;
    first.input_fn_id = generateUuidV7();
    first.output_fn_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertTypeIo(first, &ctx), Status::OK) << ctx.message;

    CatalogManager::TypeIoInfo updated = first;
    updated.output_fn_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertTypeIo(updated, &ctx), Status::OK) << ctx.message;

    CatalogManager::TypeIoInfo fetched{};
    ASSERT_EQ(catalog_->getTypeIo(type_id, fetched, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(fetched.output_fn_id, updated.output_fn_id);

    std::vector<CatalogManager::TypeIoInfo> rows;
    ASSERT_EQ(catalog_->listTypeIo(rows, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].type_id, type_id);
}

TEST_F(CatalogTypeSchemaContractTest, TypeCastIsUniqueBySourceTargetAndKind)
{
    ID schema_id = createSchemaPath("users.public.cat010_type_cast");
    ID source_type = upsertType(schema_id, "src_type");
    ID target_type = upsertType(schema_id, "dst_type");

    CatalogManager::TypeCastInfo cast_info{};
    cast_info.source_type_id = source_type;
    cast_info.target_type_id = target_type;
    cast_info.cast_kind = CatalogManager::TypeCastKind::EXPLICIT;
    cast_info.cast_fn_id = generateUuidV7();

    ErrorContext ctx;
    ASSERT_EQ(catalog_->upsertTypeCast(cast_info, &ctx), Status::OK) << ctx.message;

    cast_info.is_lossy = true;
    ASSERT_EQ(catalog_->upsertTypeCast(cast_info, &ctx), Status::OK) << ctx.message;

    std::vector<CatalogManager::TypeCastInfo> casts;
    ASSERT_EQ(catalog_->listTypeCasts(casts, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(casts.size(), 1u);
    EXPECT_TRUE(casts[0].is_lossy);

    ASSERT_EQ(catalog_->deleteTypeCast(source_type, target_type,
                                       CatalogManager::TypeCastKind::EXPLICIT, &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::TypeCastInfo missing{};
    EXPECT_EQ(catalog_->getTypeCast(source_type, target_type,
                                    CatalogManager::TypeCastKind::EXPLICIT,
                                    missing, &ctx),
              Status::NOT_FOUND);
}

TEST_F(CatalogTypeSchemaContractTest, TypeTransformRequiresProcAndUniqueTypeLanguagePair)
{
    ID schema_id = createSchemaPath("users.public.cat010_transform");
    ID type_id = upsertType(schema_id, "json_like");

    CatalogManager::TypeTransformInfo invalid{};
    invalid.type_id = type_id;
    invalid.language_id = generateUuidV7();

    ErrorContext ctx;
    ID ignored{};
    EXPECT_EQ(catalog_->upsertTypeTransform(invalid, ignored, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::TypeTransformInfo first{};
    first.type_id = type_id;
    first.language_id = generateUuidV7();
    first.from_sql_proc_id = generateUuidV7();
    ID first_id{};
    ASSERT_EQ(catalog_->upsertTypeTransform(first, first_id, &ctx), Status::OK) << ctx.message;

    CatalogManager::TypeTransformInfo duplicate{};
    duplicate.type_id = type_id;
    duplicate.language_id = first.language_id;
    duplicate.to_sql_proc_id = generateUuidV7();
    duplicate.transform_id = generateUuidV7();
    ID duplicate_id{};
    EXPECT_EQ(catalog_->upsertTypeTransform(duplicate, duplicate_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::TypeTransformInfo resolved{};
    ASSERT_EQ(catalog_->getTypeTransformByTypeLanguage(type_id, first.language_id, resolved, &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(resolved.transform_id, first_id);
}

TEST_F(CatalogTypeSchemaContractTest, EncodingConversionEnforcesUniquenessRules)
{
    ErrorContext ctx;

    CatalogManager::EncodingConversionInfo first{};
    first.conversion_name = "utf8_to_latin1_default";
    first.source_charset_id = generateUuidV7();
    first.target_charset_id = generateUuidV7();
    first.conversion_proc_id = generateUuidV7();
    first.is_default = true;

    ID first_id{};
    ASSERT_EQ(catalog_->upsertEncodingConversion(first, first_id, &ctx), Status::OK) << ctx.message;

    CatalogManager::EncodingConversionInfo duplicate_name{};
    duplicate_name.conversion_name = "UTF8_TO_LATIN1_DEFAULT";
    duplicate_name.source_charset_id = generateUuidV7();
    duplicate_name.target_charset_id = generateUuidV7();
    duplicate_name.conversion_proc_id = generateUuidV7();
    duplicate_name.is_default = false;

    ID duplicate_name_id{};
    EXPECT_EQ(catalog_->upsertEncodingConversion(duplicate_name, duplicate_name_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::EncodingConversionInfo duplicate_default_pair{};
    duplicate_default_pair.conversion_name = "utf8_to_latin1_default_2";
    duplicate_default_pair.source_charset_id = first.source_charset_id;
    duplicate_default_pair.target_charset_id = first.target_charset_id;
    duplicate_default_pair.conversion_proc_id = generateUuidV7();
    duplicate_default_pair.is_default = true;

    ID duplicate_default_id{};
    EXPECT_EQ(catalog_->upsertEncodingConversion(duplicate_default_pair, duplicate_default_id, &ctx),
              Status::CONSTRAINT_VIOLATION);

    CatalogManager::EncodingConversionInfo non_default_pair{};
    non_default_pair.conversion_name = "utf8_to_latin1_non_default";
    non_default_pair.source_charset_id = first.source_charset_id;
    non_default_pair.target_charset_id = first.target_charset_id;
    non_default_pair.conversion_proc_id = generateUuidV7();
    non_default_pair.is_default = false;

    ID non_default_id{};
    ASSERT_EQ(catalog_->upsertEncodingConversion(non_default_pair, non_default_id, &ctx), Status::OK)
        << ctx.message;
}
