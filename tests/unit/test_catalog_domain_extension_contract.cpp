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
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird::core;

class CatalogDomainExtensionContractTest : public ::testing::Test
{
protected:
    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    DomainManager* domain_mgr_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;

    void SetUp() override
    {
        db_path_ = "/tmp/test_catalog_domain_extension_contract_" + std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);
        domain_mgr_ = db_->domain_manager();
        ASSERT_NE(domain_mgr_, nullptr);

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
            domain_mgr_ = nullptr;
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

    ID createBasicDomain(const ID& schema_id, const std::string& domain_name)
    {
        ErrorContext ctx;
        ID domain_id{};
        Status status = domain_mgr_->createBasicDomain(
            schema_id,
            domain_name,
            DataType::INT32,
            0,
            0,
            true,
            "",
            {},
            domain_id,
            &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return domain_id;
    }
};

TEST_F(CatalogDomainExtensionContractTest, DomainParamKeyAndParameterContracts)
{
    ID schema_id = createSchemaPath("users.public.cat011_domain_params");
    ID domain_id = createBasicDomain(schema_id, "cat011_domain");

    CatalogManager::DomainParamKeyCatalogInfo key{};
    key.param_key_id = 1;
    key.param_name = "length_chars";
    key.param_type = CatalogManager::DomainParamType::U32;

    ErrorContext ctx;
    ASSERT_EQ(catalog_->upsertDomainParamKey(key, &ctx), Status::OK) << ctx.message;

    CatalogManager::DomainParamKeyCatalogInfo duplicate_name = key;
    duplicate_name.param_key_id = 2;
    EXPECT_EQ(catalog_->upsertDomainParamKey(duplicate_name, &ctx), Status::CONSTRAINT_VIOLATION);

    CatalogManager::DomainParameterCatalogInfo mismatch{};
    mismatch.domain_id = domain_id;
    mismatch.param_key_id = 1;
    mismatch.param_type = CatalogManager::DomainParamType::STRING;
    mismatch.val_string = std::string("32");
    EXPECT_EQ(catalog_->upsertDomainParameter(mismatch, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::DomainParameterCatalogInfo too_many{};
    too_many.domain_id = domain_id;
    too_many.param_key_id = 1;
    too_many.param_type = CatalogManager::DomainParamType::U32;
    too_many.val_u32 = 32;
    too_many.val_i32 = 7;
    EXPECT_EQ(catalog_->upsertDomainParameter(too_many, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::DomainParameterCatalogInfo ok{};
    ok.domain_id = domain_id;
    ok.param_key_id = 1;
    ok.param_type = CatalogManager::DomainParamType::U32;
    ok.val_u32 = 128;
    ASSERT_EQ(catalog_->upsertDomainParameter(ok, &ctx), Status::OK) << ctx.message;

    CatalogManager::DomainParameterCatalogInfo fetched{};
    ASSERT_EQ(catalog_->getDomainParameter(domain_id, 1, fetched, &ctx), Status::OK) << ctx.message;
    ASSERT_TRUE(fetched.val_u32.has_value());
    EXPECT_EQ(fetched.val_u32.value(), 128u);

    std::vector<CatalogManager::DomainParameterCatalogInfo> rows;
    ASSERT_EQ(catalog_->listDomainParameters(domain_id, rows, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(rows.size(), 1u);

    ASSERT_EQ(catalog_->deleteDomainParameter(domain_id, 1, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(catalog_->getDomainParameter(domain_id, 1, fetched, &ctx), Status::NOT_FOUND);
}

TEST_F(CatalogDomainExtensionContractTest, DomainConstraintCatalogContracts)
{
    ID schema_id = createSchemaPath("users.public.cat011_constraints");
    ID domain_id = createBasicDomain(schema_id, "constraint_domain");

    ErrorContext ctx;

    CatalogManager::DomainConstraintCatalogInfo invalid{};
    invalid.domain_id = domain_id;
    invalid.constraint_kind = static_cast<CatalogManager::DomainConstraintKind>(99);
    invalid.constraint_expr_sblr = "expr";
    ID invalid_id{};
    EXPECT_EQ(catalog_->upsertDomainConstraintCatalogEntry(invalid, invalid_id, &ctx),
              Status::INVALID_ARGUMENT);

    CatalogManager::DomainConstraintCatalogInfo row{};
    row.domain_id = domain_id;
    row.constraint_kind = CatalogManager::DomainConstraintKind::CHECK;
    row.constraint_expr_sblr = "check_expr_sblr";

    ID constraint_id{};
    ASSERT_EQ(catalog_->upsertDomainConstraintCatalogEntry(row, constraint_id, &ctx), Status::OK)
        << ctx.message;
    ASSERT_NE(constraint_id, ID{});

    CatalogManager::DomainConstraintCatalogInfo fetched{};
    ASSERT_EQ(catalog_->getDomainConstraintCatalogEntry(constraint_id, fetched, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(fetched.constraint_kind, CatalogManager::DomainConstraintKind::CHECK);
    EXPECT_EQ(fetched.constraint_expr_sblr, "check_expr_sblr");

    std::vector<CatalogManager::DomainConstraintCatalogInfo> rows;
    ASSERT_EQ(catalog_->listDomainConstraintCatalogEntries(domain_id, rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(rows.size(), 1u);

    ASSERT_EQ(catalog_->deleteDomainConstraintCatalogEntry(constraint_id, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(catalog_->getDomainConstraintCatalogEntry(constraint_id, fetched, &ctx), Status::NOT_FOUND);
}

TEST_F(CatalogDomainExtensionContractTest, DomainSecurityValidationIntegrityContracts)
{
    ID schema_id = createSchemaPath("users.public.cat011_security_validation_integrity");
    ID domain_id = createBasicDomain(schema_id, "quality_domain");

    ErrorContext ctx;

    CatalogManager::DomainSecurityCatalogInfo security{};
    security.domain_id = domain_id;
    security.security_kind = CatalogManager::DomainSecurityKind::REQUIRE_PERMISSION;
    security.security_expr_sblr = "security_expr";
    ID security_id{};
    ASSERT_EQ(catalog_->upsertDomainSecurityCatalogEntry(security, security_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::DomainValidationCatalogInfo validation{};
    validation.domain_id = domain_id;
    validation.validation_kind = CatalogManager::DomainValidationKind::VALIDATE_FUNCTION;
    validation.validation_expr_sblr = "validation_expr";
    ID validation_id{};
    ASSERT_EQ(catalog_->upsertDomainValidationCatalogEntry(validation, validation_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::DomainIntegrityCatalogInfo integrity{};
    integrity.domain_id = domain_id;
    integrity.integrity_kind = CatalogManager::DomainIntegrityKind::NORMALIZE_FUNCTION;
    integrity.integrity_expr_sblr = "integrity_expr";
    ID integrity_id{};
    ASSERT_EQ(catalog_->upsertDomainIntegrityCatalogEntry(integrity, integrity_id, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::DomainSecurityCatalogInfo fetched_security{};
    ASSERT_EQ(catalog_->getDomainSecurityCatalogEntry(security_id, fetched_security, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(fetched_security.security_expr_sblr, "security_expr");

    CatalogManager::DomainValidationCatalogInfo fetched_validation{};
    ASSERT_EQ(catalog_->getDomainValidationCatalogEntry(validation_id, fetched_validation, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(fetched_validation.validation_expr_sblr, "validation_expr");

    CatalogManager::DomainIntegrityCatalogInfo fetched_integrity{};
    ASSERT_EQ(catalog_->getDomainIntegrityCatalogEntry(integrity_id, fetched_integrity, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(fetched_integrity.integrity_expr_sblr, "integrity_expr");

    std::vector<CatalogManager::DomainSecurityCatalogInfo> security_rows;
    std::vector<CatalogManager::DomainValidationCatalogInfo> validation_rows;
    std::vector<CatalogManager::DomainIntegrityCatalogInfo> integrity_rows;

    ASSERT_EQ(catalog_->listDomainSecurityCatalogEntries(domain_id, security_rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->listDomainValidationCatalogEntries(domain_id, validation_rows, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(catalog_->listDomainIntegrityCatalogEntries(domain_id, integrity_rows, &ctx), Status::OK)
        << ctx.message;

    ASSERT_EQ(security_rows.size(), 1u);
    ASSERT_EQ(validation_rows.size(), 1u);
    ASSERT_EQ(integrity_rows.size(), 1u);
}
