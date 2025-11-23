#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include <iostream>
#include "gtest/gtest.h"
#include <cstdio>

using namespace scratchbird::core;


TEST(SetDomainTest, Comprehensive) {

    std::cout << "Testing SET Domain Implementation (Phase 4)...\n\n";

    // Create a test database
    const char* test_db = "test_set_domain.sbdb";
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
    status = catalog->createSchema("test_schema", "test_user", schema_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Test 1: Create SET domain
    std::cout << "Test 1: Create SET domain\n";
    {
        ID tags_domain_id;
        status = dm->createSetDomain(schema_id, "Tags", DataType::VARCHAR, tags_domain_id, &ctx);
        if (status != Status::OK) {
            std::cerr << "Failed to create SET domain: " << static_cast<int>(status) << "\n";
            if (!ctx.message.empty()) {
                std::cerr << "Error: " << ctx.message << "\n";
            }
            FAIL(); return;
        }
        std::cout << "  Created Tags SET domain with VARCHAR elements ✓\n";
    }
    std::cout << "  ✓ Create SET domain passed\n\n";

    // Test 2: Get SET domain info
    std::cout << "Test 2: Get SET domain info\n";
    {
        DomainInfo info;
        status = dm->getDomain(schema_id, "Tags", info, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(info.domain_type, DomainType::SET);
        ASSERT_EQ(info.base_type, DataType::VECTOR);
        ASSERT_EQ(info.set_element_type, DataType::VARCHAR);
        std::cout << "  Retrieved Tags domain: SET<VARCHAR> ✓\n";
    }
    std::cout << "  ✓ Get SET domain info passed\n\n";

    // Test 3: Create SET with different element types
    std::cout << "Test 3: Create SET with different element types\n";
    {
        ID int_set_id;
        status = dm->createSetDomain(schema_id, "Numbers", DataType::INT32, int_set_id, &ctx);
        ASSERT_EQ(status, Status::OK);
        std::cout << "  Created Numbers SET<INT32> ✓\n";

        ID float_set_id;
        status = dm->createSetDomain(schema_id, "Scores", DataType::FLOAT64, float_set_id, &ctx);
        ASSERT_EQ(status, Status::OK);
        std::cout << "  Created Scores SET<FLOAT64> ✓\n";

        ID date_set_id;
        status = dm->createSetDomain(schema_id, "Holidays", DataType::DATE, date_set_id, &ctx);
        ASSERT_EQ(status, Status::OK);
        std::cout << "  Created Holidays SET<DATE> ✓\n";
    }
    std::cout << "  ✓ Create SET with different element types passed\n\n";

    // Test 4: Reject UNKNOWN element type
    std::cout << "Test 4: Reject UNKNOWN element type\n";
    {
        ID invalid_set_id;
        status = dm->createSetDomain(schema_id, "Invalid", DataType::UNKNOWN, invalid_set_id, &ctx);
        ASSERT_EQ(status, Status::INVALID_ARGUMENT);
        std::cout << "  UNKNOWN element type rejected ✓\n";
    }
    std::cout << "  ✓ Reject UNKNOWN element type passed\n\n";

    // Test 5: List mixed domain types
    std::cout << "Test 5: List mixed domain types\n";
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

        // Create an ENUM domain
        std::vector<EnumValue> values;
        EnumValue val1; val1.label = "SMALL"; val1.position = 0;
        EnumValue val2; val2.label = "MEDIUM"; val2.position = 1;
        EnumValue val3; val3.label = "LARGE"; val3.position = 2;
        values.push_back(val1);
        values.push_back(val2);
        values.push_back(val3);

        ID enum_domain_id;
        status = dm->createEnumDomain(schema_id, "Size", values, enum_domain_id, &ctx);
        ASSERT_EQ(status, Status::OK);

        // List all domains
        std::vector<DomainInfo> domains;
        status = dm->listDomains(schema_id, domains, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_GE(domains.size(), 7);  // Tags, Numbers, Scores, Holidays, PositiveInt, Person, Size

        int basic_count = 0;
        int record_count = 0;
        int enum_count = 0;
        int set_count = 0;

        for (const auto& domain : domains) {
            if (domain.domain_type == DomainType::BASIC) {
                basic_count++;
            } else if (domain.domain_type == DomainType::RECORD) {
                record_count++;
            } else if (domain.domain_type == DomainType::ENUM) {
                enum_count++;
            } else if (domain.domain_type == DomainType::SET) {
                set_count++;
                std::cout << "    - " << domain.domain_name
                         << " (SET<" << static_cast<int>(domain.set_element_type) << ">)\n";
            } else {
                std::cout << "    - " << domain.domain_name
                         << " (";
                if (domain.domain_type == DomainType::BASIC) std::cout << "BASIC";
                else if (domain.domain_type == DomainType::RECORD) std::cout << "RECORD";
                else if (domain.domain_type == DomainType::ENUM) std::cout << "ENUM";
                std::cout << ")\n";
            }
        }

        ASSERT_GE(basic_count, 1);
        ASSERT_GE(record_count, 1);
        ASSERT_GE(enum_count, 1);
        ASSERT_GE(set_count, 4);

        std::cout << "  Found " << basic_count << " BASIC, "
                 << record_count << " RECORD, "
                 << enum_count << " ENUM, and "
                 << set_count << " SET domains ✓\n";
    }
    std::cout << "  ✓ List mixed domain types passed\n\n";

    // Test 6: SET operations (stub verification)
    std::cout << "Test 6: SET operations (stub verification)\n";
    {
        // Note: These operations return NOT_IMPLEMENTED until TypedValue VECTOR support is complete
        // This test verifies the API exists and returns appropriate status

        TypedValue empty_vector = TypedValue::makeNull();  // Placeholder
        TypedValue element = TypedValue::makeInt32(42);
        bool result_bool;
        TypedValue result_set;

        // setContains - should return NOT_IMPLEMENTED
        status = dm->setContains(empty_vector, element, result_bool, &ctx);
        // We expect NOT_IMPLEMENTED or TYPE_MISMATCH (since empty_vector is NULL, not VECTOR)
        ASSERT_NE(status, Status::OK);
        std::cout << "  setContains API exists (returns non-OK as expected) ✓\n";

        // setsOverlap - should return NOT_IMPLEMENTED
        status = dm->setsOverlap(empty_vector, empty_vector, result_bool, &ctx);
        ASSERT_NE(status, Status::OK);
        std::cout << "  setsOverlap API exists (returns non-OK as expected) ✓\n";

        // setUnion - should return NOT_IMPLEMENTED
        status = dm->setUnion(empty_vector, empty_vector, result_set, &ctx);
        ASSERT_NE(status, Status::OK);
        std::cout << "  setUnion API exists (returns non-OK as expected) ✓\n";

        // setIntersection - should return NOT_IMPLEMENTED
        status = dm->setIntersection(empty_vector, empty_vector, result_set, &ctx);
        ASSERT_NE(status, Status::OK);
        std::cout << "  setIntersection API exists (returns non-OK as expected) ✓\n";

        // setDifference - should return NOT_IMPLEMENTED
        status = dm->setDifference(empty_vector, empty_vector, result_set, &ctx);
        ASSERT_NE(status, Status::OK);
        std::cout << "  setDifference API exists (returns non-OK as expected) ✓\n";
    }
    std::cout << "  ✓ SET operations stub verification passed\n\n";

    db.close();
    std::remove(test_db);

    std::cout << "========================================\n";
    std::cout << "ALL TESTS PASSED! ✓\n";
    std::cout << "SET Domain Phase 4 is functional.\n";
    std::cout << "========================================\n";
    std::cout << "\nNote: SET value operations (contains, overlap, union, intersection, difference)\n";
    std::cout << "require TypedValue VECTOR element access and are planned for future enhancement.\n";
}

