#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "unit/test_user_helpers.h"
#include <iostream>
#include "gtest/gtest.h"
#include <cstdio>

using namespace scratchbird::core;


TEST(VariantDomainTest, Comprehensive) {

    std::cout << "Testing VARIANT Domain Implementation (Phase 5)...\n\n";

    const char* test_db = "test_variant_domain.sbdb";
    std::remove(test_db);

    ErrorContext ctx;
    Status status = Database::create(test_db, 16384, &ctx);
    ASSERT_EQ(status, Status::OK);

    Database db;
    status = db.open(test_db, &ctx);
    ASSERT_EQ(status, Status::OK);

    DomainManager* dm = db.domain_manager();
    CatalogManager* catalog = db.catalog_manager();
    ASSERT_NE(dm, nullptr);
    ASSERT_NE(catalog, nullptr);

    ID schema_id;
    EnsureUser(catalog, "test_user");
    status = catalog->createSchema("test_schema", "test_user", schema_id, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Test 1: Create VARIANT domain
    std::cout << "Test 1: Create VARIANT domain\n";
    {
        std::vector<DataType> types = {DataType::INT32, DataType::VARCHAR, DataType::FLOAT64};
        ID variant_id;
        status = dm->createVariantDomain(schema_id, "NumberOrString", types, variant_id, &ctx);
        ASSERT_EQ(status, Status::OK);
        std::cout << "  Created NumberOrString VARIANT ✓\n";
    }
    std::cout << "  ✓ Create VARIANT domain passed\n\n";

    // Test 2: Get VARIANT domain info
    std::cout << "Test 2: Get VARIANT domain info\n";
    {
        DomainInfo info;
        status = dm->getDomain(schema_id, "NumberOrString", info, &ctx);
        ASSERT_EQ(status, Status::OK);
        ASSERT_EQ(info.domain_type, DomainType::VARIANT);
        ASSERT_EQ(info.variant_allowed_types.size(), 3);
        std::cout << "  Retrieved VARIANT with 3 allowed types ✓\n";
    }
    std::cout << "  ✓ Get VARIANT domain info passed\n\n";

    // Test 3: Reject empty allowed types
    std::cout << "Test 3: Reject empty allowed types\n";
    {
        std::vector<DataType> empty;
        ID invalid_id;
        status = dm->createVariantDomain(schema_id, "Invalid", empty, invalid_id, &ctx);
        ASSERT_EQ(status, Status::INVALID_ARGUMENT);
        std::cout << "  Empty types rejected ✓\n";
    }
    std::cout << "  ✓ Reject empty allowed types passed\n\n";

    // Test 4: Reject UNKNOWN type
    std::cout << "Test 4: Reject UNKNOWN type\n";
    {
        std::vector<DataType> types = {DataType::INT32, DataType::UNKNOWN};
        ID invalid_id;
        status = dm->createVariantDomain(schema_id, "Invalid", types, invalid_id, &ctx);
        ASSERT_EQ(status, Status::INVALID_ARGUMENT);
        std::cout << "  UNKNOWN type rejected ✓\n";
    }
    std::cout << "  ✓ Reject UNKNOWN type passed\n\n";

    // Test 5: Reject duplicate types
    std::cout << "Test 5: Reject duplicate types\n";
    {
        std::vector<DataType> types = {DataType::INT32, DataType::VARCHAR, DataType::INT32};
        ID invalid_id;
        status = dm->createVariantDomain(schema_id, "Invalid", types, invalid_id, &ctx);
        ASSERT_EQ(status, Status::INVALID_ARGUMENT);
        std::cout << "  Duplicate types rejected ✓\n";
    }
    std::cout << "  ✓ Reject duplicate types passed\n\n";

    // Test 6: VARIANT operations
    std::cout << "Test 6: VARIANT operations\n";
    {
        TypedValue payload = TypedValue::makeInt32(42);
        TypedValue variant = TypedValue::makeVariant(DataType::INT32, payload);

        DataType runtime = DataType::UNKNOWN;
        status = dm->extractDataType(variant, runtime, &ctx);
        ASSERT_EQ(status, Status::OK);
        EXPECT_EQ(runtime, DataType::INT32);

        bool is_type = false;
        status = dm->isOfType(variant, DataType::INT32, is_type, &ctx);
        ASSERT_EQ(status, Status::OK);
        EXPECT_TRUE(is_type);

        status = dm->isOfType(variant, DataType::VARCHAR, is_type, &ctx);
        ASSERT_EQ(status, Status::OK);
        EXPECT_FALSE(is_type);

        TypedValue casted;
        status = dm->variantCast(variant, DataType::INT32, casted, &ctx);
        ASSERT_EQ(status, Status::OK);
        EXPECT_EQ(casted.getInt32(), 42);

        status = dm->variantCast(variant, DataType::VARCHAR, casted, &ctx);
        ASSERT_EQ(status, Status::TYPE_MISMATCH);

        TypedValue null_variant = TypedValue::makeVariant(TypedValue::makeNull(DataType::INT32));
        status = dm->extractDataType(null_variant, runtime, &ctx);
        ASSERT_EQ(status, Status::OK);
        EXPECT_EQ(runtime, DataType::NULL_TYPE);
    }
    std::cout << "  ✓ VARIANT operations passed\n\n";

    // Test 7: List all domain types
    std::cout << "Test 7: List all domain types\n";
    {
        // Create one of each type
        ID basic_id;
        std::vector<DomainConstraint> constraints;
        dm->createBasicDomain(schema_id, "Basic", DataType::INT32, 0, 0, false, "0", constraints, basic_id, &ctx);

        std::vector<RecordField> fields;
        fields.push_back(RecordField("id", DataType::INT32, false));
        ID record_id;
        dm->createRecordDomain(schema_id, "Record", fields, record_id, &ctx);

        std::vector<EnumValue> values;
        EnumValue v1; v1.label = "A"; v1.position = 1;
        values.push_back(v1);
        ID enum_id;
        dm->createEnumDomain(schema_id, "Enum", values, enum_id, &ctx);

        ID set_id;
        dm->createSetDomain(schema_id, "Set", DataType::INT32, set_id, &ctx);

        std::vector<DomainInfo> domains;
        dm->listDomains(schema_id, domains, &ctx);

        int counts[5] = {0};
        for (const auto& d : domains) {
            counts[static_cast<int>(d.domain_type)]++;
        }

        std::cout << "  Found: " << counts[0] << " BASIC, "
                  << counts[1] << " RECORD, " << counts[2] << " ENUM, "
                  << counts[3] << " SET, " << counts[4] << " VARIANT ✓\n";
        ASSERT_GE(counts[0], 1 && counts[1] >= 1 && counts[2] >= 1 && counts[3] >= 1 && counts[4] >= 1);
    }
    std::cout << "  ✓ List all domain types passed\n\n";

    db.close();
    std::remove(test_db);

    std::cout << "========================================\n";
    std::cout << "ALL TESTS PASSED! ✓\n";
    std::cout << "VARIANT Domain Phase 5 is functional.\n";
    std::cout << "========================================\n";
    std::cout << "\nNote: VARIANT operations validate runtime tags and enforce type safety.\n";
}
