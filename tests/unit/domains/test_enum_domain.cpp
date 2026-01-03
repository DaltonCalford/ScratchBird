#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "unit/test_user_helpers.h"
#include <iostream>
#include "gtest/gtest.h"
#include <cstdio>

using namespace scratchbird::core;


TEST(EnumDomainTest, Comprehensive) {

    std::cout << "Testing ENUM Domain Implementation (Phase 3)...\n\n";

    // Create a test database
    const char* test_db = "test_enum_domain.sbdb";
    std::remove(test_db);

    ErrorContext ctx;
    Status status = Database::create(test_db, 16384, &ctx);
    if (status != Status::OK) {
        std::cerr << "Failed to create database: " << static_cast<int>(status) << "\n";
        FAIL(); return;
    }

    Database db;
    status = db.open(test_db, &ctx);
    if (status != Status::OK) {
        std::cerr << "Failed to open database: " << static_cast<int>(status) << "\n";
        FAIL(); return;
    }

    DomainManager* dm = db.domain_manager();
    CatalogManager* catalog = db.catalog_manager();
    ASSERT_NE(dm, nullptr);
    ASSERT_NE(catalog, nullptr);

    // Create schema
    ID schema_id;
    EnsureUser(catalog, "test_user");
    status = catalog->createSchema("test_schema", "test_user", schema_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Test 1: Create ENUM domain
    std::cout << "Test 1: Create ENUM domain\n";
    {
        std::vector<EnumValue> values;
        EnumValue val1; val1.label = "SMALL"; val1.position = 1;
        EnumValue val2; val2.label = "MEDIUM"; val2.position = 2;
        EnumValue val3; val3.label = "LARGE"; val3.position = 3;
        values.push_back(val1);
        values.push_back(val2);
        values.push_back(val3);

        ID size_domain_id;
        status = dm->createEnumDomain(schema_id, "Size", values, size_domain_id, &ctx);
        ASSERT_EQ(status, Status::OK);
        std::cout << "  Created Size ENUM domain with 3 values ✓\n";
    }
    std::cout << "  ✓ Create ENUM domain passed\n\n";

    // Test 2: Get ENUM domain info
    std::cout << "Test 2: Get ENUM domain info\n";
    {
        DomainInfo info;
        status = dm->getDomain(schema_id, "Size", info, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(info.domain_type, DomainType::ENUM);
        ASSERT_EQ(info.base_type, DataType::VARCHAR);
        ASSERT_EQ(info.enum_values.size(), 3);
        std::cout << "  Retrieved Size domain: " << info.enum_values.size() << " values ✓\n";

        // Verify enum values
        ASSERT_EQ(info.enum_values[0].label, "SMALL");
        ASSERT_EQ(info.enum_values[0].position, 1);
        ASSERT_EQ(info.enum_values[1].label, "MEDIUM");
        ASSERT_EQ(info.enum_values[1].position, 2);
        ASSERT_EQ(info.enum_values[2].label, "LARGE");
        ASSERT_EQ(info.enum_values[2].position, 3);

        std::cout << "  All enum values verified ✓\n";
    }
    std::cout << "  ✓ Get ENUM domain info passed\n\n";

    // Test 3: Get value for position
    std::cout << "Test 3: Get value for position\n";
    {
        DomainInfo info;
        status = dm->getDomain(schema_id, "Size", info, &ctx);
        ASSERT_EQ(status, Status::OK);

        std::string label;
        status = dm->getEnumValueForPosition(info.domain_id, 2, label, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(label, "MEDIUM");
        std::cout << "  Position 2 = 'MEDIUM' ✓\n";

        // Try invalid position
        status = dm->getEnumValueForPosition(info.domain_id, 10, label, &ctx);
        ASSERT_EQ(status, Status::OUT_OF_RANGE);
        std::cout << "  Out of range position rejected ✓\n";
    }
    std::cout << "  ✓ Get value for position passed\n\n";

    // Test 4: Get position for value
    std::cout << "Test 4: Get position for value\n";
    {
        DomainInfo info;
        status = dm->getDomain(schema_id, "Size", info, &ctx);
        ASSERT_EQ(status, Status::OK);

        int32_t position;
        status = dm->getPositionForEnumValue(info.domain_id, "LARGE", position, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(position, 3);
        std::cout << "  'LARGE' = position 3 ✓\n";

        // Try non-existent value
        status = dm->getPositionForEnumValue(info.domain_id, "XLARGE", position, &ctx);
        ASSERT_EQ(status, Status::NOT_FOUND);
        std::cout << "  Non-existent value rejected ✓\n";
    }
    std::cout << "  ✓ Get position for value passed\n\n";

    // Test 5: Set next value
    std::cout << "Test 5: Set next value\n";
    {
        DomainInfo info;
        status = dm->getDomain(schema_id, "Size", info, &ctx);
        ASSERT_EQ(status, Status::OK);

        std::string next_label;
        status = dm->setNextEnumValue(info.domain_id, "SMALL", next_label, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(next_label, "MEDIUM");
        std::cout << "  Next after 'SMALL' is 'MEDIUM' ✓\n";

        status = dm->setNextEnumValue(info.domain_id, "MEDIUM", next_label, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(next_label, "LARGE");
        std::cout << "  Next after 'MEDIUM' is 'LARGE' ✓\n";

        // Try to get next after last value
        status = dm->setNextEnumValue(info.domain_id, "LARGE", next_label, &ctx);
        ASSERT_EQ(status, Status::OUT_OF_RANGE);
        std::cout << "  No next after 'LARGE' (last value) ✓\n";
    }
    std::cout << "  ✓ Set next value passed\n\n";

    // Test 6: Compare enum values
    std::cout << "Test 6: Compare enum values\n";
    {
        DomainInfo info;
        status = dm->getDomain(schema_id, "Size", info, &ctx);
        ASSERT_EQ(status, Status::OK);

        int result;
        status = dm->compareEnumValues(info.domain_id, "SMALL", "LARGE", result, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(result, -1);  // SMALL < LARGE
        std::cout << "  'SMALL' < 'LARGE' ✓\n";

        status = dm->compareEnumValues(info.domain_id, "LARGE", "SMALL", result, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(result, 1);   // LARGE > SMALL
        std::cout << "  'LARGE' > 'SMALL' ✓\n";

        status = dm->compareEnumValues(info.domain_id, "MEDIUM", "MEDIUM", result, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(result, 0);   // MEDIUM == MEDIUM
        std::cout << "  'MEDIUM' == 'MEDIUM' ✓\n";
    }
    std::cout << "  ✓ Compare enum values passed\n\n";

    // Test 7: Reject duplicate values
    std::cout << "Test 7: Reject duplicate values\n";
    {
        std::vector<EnumValue> values;
        EnumValue v1; v1.label = "RED"; v1.position = 1;
        EnumValue v2; v2.label = "GREEN"; v2.position = 2;
        EnumValue v3; v3.label = "RED"; v3.position = 3;  // Duplicate!
        values.push_back(v1);
        values.push_back(v2);
        values.push_back(v3);

        ID dup_domain_id;
        status = dm->createEnumDomain(schema_id, "InvalidColor", values, dup_domain_id, &ctx);
        ASSERT_EQ(status, Status::INVALID_ARGUMENT);
        std::cout << "  Duplicate enum value rejected ✓\n";
    }
    std::cout << "  ✓ Reject duplicate values passed\n\n";

    // Test 8: Reject empty value list
    std::cout << "Test 8: Reject empty value list\n";
    {
        std::vector<EnumValue> values;  // Empty!

        ID empty_domain_id;
        status = dm->createEnumDomain(schema_id, "Empty", values, empty_domain_id, &ctx);
        ASSERT_EQ(status, Status::INVALID_ARGUMENT);
        std::cout << "  Empty value list rejected ✓\n";
    }
    std::cout << "  ✓ Reject empty value list passed\n\n";

    // Test 9: Reject invalid positions
    std::cout << "Test 9: Reject invalid positions\n";
    {
        std::vector<EnumValue> values;
        EnumValue v1; v1.label = "FIRST"; v1.position = 1;
        EnumValue v2; v2.label = "SECOND"; v2.position = 3;  // Position not sequential!
        values.push_back(v1);
        values.push_back(v2);

        ID invalid_domain_id;
        status = dm->createEnumDomain(schema_id, "InvalidOrder", values, invalid_domain_id, &ctx);
        ASSERT_EQ(status, Status::INVALID_ARGUMENT);
        std::cout << "  Non-sequential positions rejected ✓\n";
    }
    std::cout << "  ✓ Reject invalid positions passed\n\n";

    // Test 10: Complex ENUM domain
    std::cout << "Test 10: Complex ENUM domain with many values\n";
    {
        std::vector<EnumValue> values;
        const char* days[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
        for (int i = 0; i < 7; i++) {
            EnumValue val;
            val.label = days[i];
            val.position = i + 1;
            values.push_back(val);
        }

        ID day_domain_id;
        status = dm->createEnumDomain(schema_id, "DayOfWeek", values, day_domain_id, &ctx);
        ASSERT_EQ(status, Status::OK);

        // Verify all values
        DomainInfo info;
        status = dm->getDomain(schema_id, "DayOfWeek", info, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(info.enum_values.size(), 7);
        std::cout << "  Created DayOfWeek ENUM with 7 values ✓\n";

        // Test value access for each day
        for (const auto& expected_value : values) {
            std::string label;
            status = dm->getEnumValueForPosition(info.domain_id, expected_value.position, label, &ctx);
            ASSERT_EQ(status, Status::OK);
            ASSERT_EQ(label, expected_value.label);

            int32_t position;
            status = dm->getPositionForEnumValue(info.domain_id, expected_value.label, position, &ctx);
            ASSERT_EQ(status, Status::OK);
            ASSERT_EQ(position, static_cast<int32_t>(expected_value.position));
        }
        std::cout << "  All 7 values accessible ✓\n";
    }
    std::cout << "  ✓ Complex ENUM domain passed\n\n";

    // Test 11: List mixed domain types
    std::cout << "Test 11: List mixed domain types\n";
    {
        // Create a basic domain
        ID basic_domain_id;
        std::vector<DomainConstraint> constraints;
        status = dm->createBasicDomain(
            schema_id, "PositiveInt", DataType::INT32,
            0, 0, false, "0", constraints,
            basic_domain_id, &ctx
        );
        ASSERT_EQ(status, Status::OK);

        // Create a RECORD domain
        std::vector<RecordField> fields;
        fields.push_back(RecordField("id", DataType::INT32, false));
        fields.push_back(RecordField("name", DataType::VARCHAR, false));

        ID record_domain_id;
        status = dm->createRecordDomain(schema_id, "Person", fields, record_domain_id, &ctx);
        ASSERT_EQ(status, Status::OK);

        // List all domains
        std::vector<DomainInfo> domains;
        status = dm->listDomains(schema_id, domains, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_GE(domains.size(), 4);  // At least Size, DayOfWeek, PositiveInt, Person

        int basic_count = 0;
        int record_count = 0;
        int enum_count = 0;
        for (const auto& domain : domains) {
            if (domain.domain_type == DomainType::BASIC) {
                basic_count++;
            } else if (domain.domain_type == DomainType::RECORD) {
                record_count++;
            } else if (domain.domain_type == DomainType::ENUM) {
                enum_count++;
            }
            std::cout << "    - " << domain.domain_name
                     << " (";
            if (domain.domain_type == DomainType::BASIC) std::cout << "BASIC";
            else if (domain.domain_type == DomainType::RECORD) std::cout << "RECORD";
            else if (domain.domain_type == DomainType::ENUM) std::cout << "ENUM";
            std::cout << ")\n";
        }

        ASSERT_GE(basic_count, 1);
        ASSERT_GE(record_count, 1);
        ASSERT_GE(enum_count, 2);
        std::cout << "  Found " << basic_count << " BASIC, "
                 << record_count << " RECORD, and "
                 << enum_count << " ENUM domains ✓\n";
    }
    std::cout << "  ✓ List mixed domain types passed\n\n";

    db.close();
    std::remove(test_db);

    std::cout << "========================================\n";
    std::cout << "ALL TESTS PASSED! ✓\n";
    std::cout << "ENUM Domain Phase 3 is functional.\n";
    std::cout << "========================================\n";
}
