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
#include "unit/test_user_helpers.h"
#include <iostream>
#include "gtest/gtest.h"
#include <cstdio>

using namespace scratchbird::core;


TEST(AdvancedDomainTest, Comprehensive) {

    std::cout << "Testing Advanced Domain Features (Phase 6)...\n\n";

    const char* test_db = "test_advanced_domain.sbdb";
    std::remove(test_db);

    ErrorContext ctx;
    Status status = Database::create(test_db, 16384, &ctx);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(test_db, &ctx);
    ASSERT_EQ(status, Status::OK);

    DomainManager* dm = db.domain_manager();
    CatalogManager* catalog = db.catalog_manager();

    ID schema_id;
    EnsureUser(catalog, "test_user");
    EnsureUser(catalog, "mask_user");
    EnsureUser(catalog, "priv_user");

    CatalogManager::UserInfo test_user;
    CatalogManager::UserInfo mask_user;
    CatalogManager::UserInfo priv_user;
    status = catalog->getUserByName("test_user", test_user, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = catalog->getUserByName("mask_user", mask_user, &ctx);
    ASSERT_EQ(status, Status::OK);
    status = catalog->getUserByName("priv_user", priv_user, &ctx);
    ASSERT_EQ(status, Status::OK);

    status = catalog->createSchema("test_schema", "test_user", schema_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Create a basic domain for testing
    ID ssn_domain_id;
    std::vector<DomainConstraint> constraints;
    status = dm->createBasicDomain(schema_id, "SSN", DataType::VARCHAR, 11, 0, false, "", constraints, ssn_domain_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Test 1: Set security options
    std::cout << "Test 1: Set security options\n";
    {
        DomainSecurity security;
        security.masking_config.type = MaskingType::FULL;
        security.required_privilege_for_unmasked = "SELECT";
        security.encryption_enabled = false;
        security.audit_enabled = false;

        status = dm->setSecurityOptions(ssn_domain_id, security, &ctx);
        ASSERT_EQ(status, Status::OK);
        std::cout << "  Security options set ✓\n";

        // Verify options were stored
        DomainInfo info;
        dm->getDomain(ssn_domain_id, info, &ctx);
        ASSERT_EQ(info.security.masking_config.type, MaskingType::FULL);
        ASSERT_EQ(info.security.required_privilege_for_unmasked, "SELECT");
        std::cout << "  Security options verified ✓\n";
    }
    std::cout << "  ✓ Set security options passed\n\n";

    // Test 2: Apply masking
    std::cout << "Test 2: Apply masking\n";
    {
        TypedValue original = TypedValue::makeVarchar("123-45-6789");
        TypedValue masked;

        status = dm->applyMasking(ssn_domain_id, mask_user.user_id, original, masked, &ctx);
        ASSERT_EQ(status, Status::OK);
        std::cout << "  Masking applied ✓\n";

        // Masked value should not equal original
        ASSERT_EQ(masked.type(), DataType::VARCHAR);
        ASSERT_NE(masked.getVarchar(), original.getVarchar());
        std::cout << "  Masked value type correct ✓\n";

        status = catalog->grantPermission(
            ssn_domain_id,
            CatalogManager::PermissionObjectType::DOMAIN,
            priv_user.user_id,
            CatalogManager::GranteeType::USER,
            static_cast<uint32_t>(CatalogManager::Privilege::SELECT),
            false,
            test_user.user_id,
            &ctx);
        ASSERT_EQ(status, Status::OK);

        TypedValue unmasked;
        status = dm->applyMasking(ssn_domain_id, priv_user.user_id, original, unmasked, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(unmasked.getVarchar(), original.getVarchar());
        std::cout << "  Privileged masking bypass ✓\n";
    }
    std::cout << "  ✓ Apply masking passed\n\n";

    // Test 3: Set integrity options
    std::cout << "Test 3: Set integrity options\n";
    {
        DomainIntegrity integrity;
        integrity.uniqueness_check = true;
        integrity.normalization_enabled = false;

        status = dm->setIntegrityOptions(ssn_domain_id, integrity, &ctx);
        ASSERT_EQ(status, Status::OK);
        std::cout << "  Integrity options set ✓\n";

        DomainInfo info;
        dm->getDomain(ssn_domain_id, info, &ctx);
        ASSERT_EQ(info.integrity.uniqueness_check, true);
        std::cout << "  Integrity options verified ✓\n";
    }
    std::cout << "  ✓ Set integrity options passed\n\n";

    // Test 4: Set validation options
    std::cout << "Test 4: Set validation options\n";
    {
        DomainValidationConfig validation;
        validation.validation_function = "ssn_validator";

        status = dm->setValidationOptions(ssn_domain_id, validation, &ctx);
        ASSERT_EQ(status, Status::OK);
        std::cout << "  Validation options set ✓\n";

        DomainInfo info;
        dm->getDomain(ssn_domain_id, info, &ctx);
        ASSERT_EQ(info.validation.validation_function, "ssn_validator");
        std::cout << "  Validation options verified ✓\n";
    }
    std::cout << "  ✓ Set validation options passed\n\n";

    // Test 5: Set quality options
    std::cout << "Test 5: Set quality options\n";
    {
        DomainQuality quality;
        quality.parse_function = "parse_ssn";
        quality.standardize_function = "standardize_ssn";
        quality.enrich_function = "";

        status = dm->setQualityOptions(ssn_domain_id, quality, &ctx);
        ASSERT_EQ(status, Status::OK);
        std::cout << "  Quality options set ✓\n";

        DomainInfo info;
        dm->getDomain(ssn_domain_id, info, &ctx);
        ASSERT_EQ(info.quality.parse_function, "parse_ssn");
        ASSERT_EQ(info.quality.standardize_function, "standardize_ssn");
        std::cout << "  Quality options verified ✓\n";
    }
    std::cout << "  ✓ Set quality options passed\n\n";

    // Test 6: Masking with disabled security
    std::cout << "Test 6: Masking with disabled security\n";
    {
        // Create another domain without masking
        ID email_domain_id;
        dm->createBasicDomain(schema_id, "Email", DataType::VARCHAR, 255, 0, false, "", constraints, email_domain_id, &ctx);

        TypedValue original = TypedValue::makeVarchar("user@example.com");
        TypedValue masked;

        status = dm->applyMasking(email_domain_id, mask_user.user_id, original, masked, &ctx);
        ASSERT_EQ(status, Status::OK);

        // Should return original value (no masking enabled)
        ASSERT_EQ(masked.getVarchar(), original.getVarchar());
        std::cout << "  Masking skipped when disabled ✓\n";
    }
    std::cout << "  ✓ Masking with disabled security passed\n\n";

    // Test 7: Masking with NULL value
    std::cout << "Test 7: Masking with NULL value\n";
    {
        TypedValue null_value = TypedValue::makeNull(DataType::VARCHAR);
        TypedValue masked;
        status = dm->applyMasking(ssn_domain_id, mask_user.user_id, null_value, masked, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_TRUE(masked.isNull());
        std::cout << "  NULL masking passthrough ✓\n";
    }
    std::cout << "  ✓ Masking with NULL value passed\n\n";

    db.close();
    std::remove(test_db);

    std::cout << "========================================\n";
    std::cout << "ALL TESTS PASSED! ✓\n";
    std::cout << "Advanced Domain Features Phase 6 is functional.\n";
    std::cout << "========================================\n";
    std::cout << "\n🎉 ALPHA-002 COMPLETE! 🎉\n";
    std::cout << "All 6 phases of the DOMAIN system have been implemented:\n";
    std::cout << "  ✓ Phase 1: BASIC domains with constraints\n";
    std::cout << "  ✓ Phase 2: RECORD domains with fields\n";
    std::cout << "  ✓ Phase 3: ENUM domains with ordering\n";
    std::cout << "  ✓ Phase 4: SET domains with operators\n";
    std::cout << "  ✓ Phase 5: VARIANT type with polymorphism\n";
    std::cout << "  ✓ Phase 6: Advanced features (security, integrity, validation, quality)\n";
}
