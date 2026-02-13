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


TEST(DomainManagerTest, Comprehensive) {

    std::cout << "Testing Domain Manager Implementation...\n\n";

    // Create a test database
    const char* test_db = "test_domain.sbdb";
    std::remove(test_db);  // Clean up any previous test

    ErrorContext ctx;
    Status status = Database::create(test_db, 16384, &ctx);
    if (status != Status::OK) {
        std::cerr << "Failed to create database: " << static_cast<int>(status) << "\n";
        if (!ctx.message.empty()) {
            std::cerr << "Error: " << ctx.message << "\n";
        }
        FAIL(); return;
    }
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(test_db, &ctx);
    if (status != Status::OK) {
        std::cerr << "Failed to open database: " << static_cast<int>(status) << "\n";
        if (!ctx.message.empty()) {
            std::cerr << "Error: " << ctx.message << "\n";
        }
    }
    ASSERT_EQ(status, Status::OK);

    DomainManager* dm = db.domain_manager();
    ASSERT_NE(dm, nullptr);

    // Test 1: Create basic domain
    std::cout << "Test 1: Create basic domain\n";
    {
        // Create a schema first (using catalog manager)
        ID schema_id;
        auto* catalog = db.catalog_manager();
        EnsureUser(catalog, "test_user");
        status = catalog->createSchema("test_schema", "test_user", schema_id, &ctx);
        ASSERT_EQ(status, Status::OK);

        // Create a domain for email addresses
        ID domain_id;
        std::vector<DomainConstraint> constraints;
        constraints.push_back(DomainConstraint(ConstraintType::NOT_NULL, "", "email_not_null"));

        status = dm->createBasicDomain(
            schema_id,
            "email_address",
            DataType::VARCHAR,
            255,  // precision
            0,    // scale
            false,  // nullable
            "",   // default_value
            constraints,
            domain_id,
            &ctx
        );
        ASSERT_EQ(status, Status::OK);
        std::cout << "  Created email_address domain ✓\n";
    }
    std::cout << "  ✓ Create basic domain passed\n\n";

    // Test 2: Get domain by ID
    std::cout << "Test 2: Get domain by ID\n";
    {
        DomainInfo info;
        // Get first domain from test 1
        ASSERT_GE(dm->domainCount(), 1u);

        // List all domains to get the ID
        ID schema_id;
        auto* catalog = db.catalog_manager();
        CatalogManager::SchemaInfo schema_info;
        status = catalog->getSchema("test_schema", schema_info, &ctx);
        ASSERT_EQ(status, Status::OK);
        schema_id = schema_info.schema_id;

        std::vector<DomainInfo> domains;
        status = dm->listDomains(schema_id, domains, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(domains.size(), 1);

        status = dm->getDomain(domains[0].domain_id, info, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(info.domain_name, "email_address");
        ASSERT_EQ(info.base_type, DataType::VARCHAR);
        ASSERT_EQ(info.precision, 255);
        ASSERT_EQ(info.nullable, false);
        std::cout << "  Retrieved domain: " << info.domain_name << " ✓\n";
    }
    std::cout << "  ✓ Get domain by ID passed\n\n";

    // Test 3: Get domain by name
    std::cout << "Test 3: Get domain by name\n";
    {
        ID schema_id;
        auto* catalog = db.catalog_manager();
        CatalogManager::SchemaInfo schema_info;
        status = catalog->getSchema("test_schema", schema_info, &ctx);
        ASSERT_EQ(status, Status::OK);
        schema_id = schema_info.schema_id;

        DomainInfo info;
        status = dm->getDomain(schema_id, "email_address", info, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(info.domain_name, "email_address");
        std::cout << "  Found domain by name: " << info.domain_name << " ✓\n";
    }
    std::cout << "  ✓ Get domain by name passed\n\n";

    // Test 4: Validate value against domain
    std::cout << "Test 4: Validate value against domain\n";
    {
        ID schema_id;
        auto* catalog = db.catalog_manager();
        CatalogManager::SchemaInfo schema_info;
        status = catalog->getSchema("test_schema", schema_info, &ctx);
        ASSERT_EQ(status, Status::OK);
        schema_id = schema_info.schema_id;

        DomainInfo info;
        status = dm->getDomain(schema_id, "email_address", info, &ctx);
        ASSERT_EQ(status, Status::OK);

        // Valid value
        TypedValue valid_email = TypedValue::makeVarchar("test@example.com");
        status = dm->validateValue(info.domain_id, valid_email, &ctx);
        ASSERT_EQ(status, Status::OK);
        std::cout << "  Valid email accepted ✓\n";

        // NULL value (should fail because nullable=false)
        TypedValue null_value = TypedValue::makeNull();
        status = dm->validateValue(info.domain_id, null_value, &ctx);
        ASSERT_EQ(status, Status::CONSTRAINT_VIOLATION);
        std::cout << "  NULL value rejected as expected ✓\n";
    }
    std::cout << "  ✓ Validate value passed\n\n";

    // Test 5: Domain inheritance
    std::cout << "Test 5: Domain inheritance\n";
    {
        ID schema_id;
        auto* catalog = db.catalog_manager();
        CatalogManager::SchemaInfo schema_info;
        status = catalog->getSchema("test_schema", schema_info, &ctx);
        ASSERT_EQ(status, Status::OK);
        schema_id = schema_info.schema_id;

        // Create parent domain
        ID parent_domain_id;
        std::vector<DomainConstraint> parent_constraints;
        status = dm->createBasicDomain(
            schema_id,
            "positive_int",
            DataType::INT32,
            0, 0, true, "0",
            parent_constraints,
            parent_domain_id,
            &ctx
        );
        ASSERT_EQ(status, Status::OK);

        // Create child domain
        ID child_domain_id;
        std::vector<DomainConstraint> child_constraints;
        status = dm->createBasicDomain(
            schema_id,
            "age",
            DataType::INT32,
            0, 0, true, "0",
            child_constraints,
            child_domain_id,
            &ctx
        );
        ASSERT_EQ(status, Status::OK);

        // Set parent
        status = dm->setParentDomain(child_domain_id, parent_domain_id, &ctx);
        ASSERT_EQ(status, Status::OK);

        // Verify inheritance
        DomainInfo child_info;
        status = dm->getDomain(child_domain_id, child_info, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(child_info.parent_domain_id, parent_domain_id);
        std::cout << "  Domain inheritance set up correctly ✓\n";
    }
    std::cout << "  ✓ Domain inheritance passed\n\n";

    // Test 6: List domains
    std::cout << "Test 6: List domains\n";
    {
        ID schema_id;
        auto* catalog = db.catalog_manager();
        CatalogManager::SchemaInfo schema_info;
        status = catalog->getSchema("test_schema", schema_info, &ctx);
        ASSERT_EQ(status, Status::OK);
        schema_id = schema_info.schema_id;

        std::vector<DomainInfo> domains;
        status = dm->listDomains(schema_id, domains, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(domains.size(), 3);  // email_address, positive_int, age
        std::cout << "  Found " << domains.size() << " domains ✓\n";
        for (const auto& domain : domains) {
            std::cout << "    - " << domain.domain_name << "\n";
        }
    }
    std::cout << "  ✓ List domains passed\n\n";

    // Test 7: Drop domain
    std::cout << "Test 7: Drop domain\n";
    {
        ID schema_id;
        auto* catalog = db.catalog_manager();
        CatalogManager::SchemaInfo schema_info;
        status = catalog->getSchema("test_schema", schema_info, &ctx);
        ASSERT_EQ(status, Status::OK);
        schema_id = schema_info.schema_id;

        DomainInfo info;
        status = dm->getDomain(schema_id, "age", info, &ctx);
        ASSERT_EQ(status, Status::OK);

        status = dm->dropDomain(info.domain_id, &ctx);
        ASSERT_EQ(status, Status::OK);

        // Verify it's dropped
        status = dm->getDomain(schema_id, "age", info, &ctx);
        ASSERT_EQ(status, Status::NOT_FOUND);
        std::cout << "  Domain dropped successfully ✓\n";
    }
    std::cout << "  ✓ Drop domain passed\n\n";

    db.close();
    std::remove(test_db);

    std::cout << "========================================\n";
    std::cout << "ALL TESTS PASSED! ✓\n";
    std::cout << "Domain Manager Phase 1 is fully functional.\n";
    std::cout << "========================================\n";
}
