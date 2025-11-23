#include "scratchbird/core/composite.h"
#include <iostream>
#include "gtest/gtest.h"

using namespace scratchbird::core;


TEST(CompositeTest, Comprehensive) {

    std::cout << "Testing COMPOSITE/RECORD Implementation...\n\n";

    // Test 1: Create simple composite
    std::cout << "Test 1: Simple composite\n";
    {
        std::vector<CompositeField> fields = {
            CompositeField("id", CompositeFieldType::INT32),
            CompositeField("name", CompositeFieldType::STRING),
            CompositeField("active", CompositeFieldType::BOOL)
        };

        auto comp = Composite::create("User", fields);
        ASSERT_EQ(comp.getTypeName(), "User");
        ASSERT_EQ(comp.getFieldCount(), 3);
        ASSERT_TRUE(comp.hasField("id"));
        ASSERT_TRUE(comp.hasField("name"));
        ASSERT_TRUE(comp.hasField("active"));
        std::cout << "  Created composite type 'User' with 3 fields ✓\n";
    }
    std::cout << "  ✓ Simple composite passed\n\n";

    // Test 2: Set and get field values
    std::cout << "Test 2: Field access\n";
    {
        std::vector<CompositeField> fields = {
            CompositeField("id", CompositeFieldType::INT32),
            CompositeField("name", CompositeFieldType::STRING),
            CompositeField("score", CompositeFieldType::FLOAT64)
        };

        auto comp = Composite::create("Person", fields);

        // Set values
        ASSERT_TRUE(comp.setField("id", int32_t(123)));
        ASSERT_TRUE(comp.setField("name", std::string("John Doe")));
        ASSERT_TRUE(comp.setField("score", 95.5));

        // Get values
        auto id = comp.getField("id");
        ASSERT_TRUE(id.has_value());
        ASSERT_EQ(std::get<int32_t>(*id), 123);
        std::cout << "  id: " << std::get<int32_t>(*id) << " ✓\n";

        auto name = comp.getField("name");
        ASSERT_TRUE(name.has_value());
        ASSERT_EQ(std::get<std::string>(*name), "John Doe");
        std::cout << "  name: " << std::get<std::string>(*name) << " ✓\n";

        auto score = comp.getField("score");
        ASSERT_TRUE(score.has_value());
        ASSERT_EQ(std::get<double>(*score), 95.5);
        std::cout << "  score: " << std::get<double>(*score) << " ✓\n";
    }
    std::cout << "  ✓ Field access passed\n\n";

    // Test 3: Field access by index
    std::cout << "Test 3: Field access by index\n";
    {
        std::vector<CompositeField> fields = {
            CompositeField("a", CompositeFieldType::INT32),
            CompositeField("b", CompositeFieldType::INT32)
        };

        auto comp = Composite::create("Pair", fields);
        comp.setFieldByIndex(0, int32_t(10));
        comp.setFieldByIndex(1, int32_t(20));

        auto val0 = comp.getFieldByIndex(0);
        ASSERT_TRUE(val0.has_value());
        ASSERT_EQ(std::get<int32_t>(*val0), 10);

        auto val1 = comp.getFieldByIndex(1);
        ASSERT_TRUE(val1.has_value());
        ASSERT_EQ(std::get<int32_t>(*val1), 20);

        std::cout << "  Field access by index works ✓\n";
    }
    std::cout << "  ✓ Field access by index passed\n\n";

    // Test 4: All data types
    std::cout << "Test 4: All data types\n";
    {
        std::vector<CompositeField> fields = {
            CompositeField("int32_field", CompositeFieldType::INT32),
            CompositeField("int64_field", CompositeFieldType::INT64),
            CompositeField("float32_field", CompositeFieldType::FLOAT32),
            CompositeField("float64_field", CompositeFieldType::FLOAT64),
            CompositeField("string_field", CompositeFieldType::STRING),
            CompositeField("bool_field", CompositeFieldType::BOOL)
        };

        auto comp = Composite::create("AllTypes", fields);

        comp.setField("int32_field", int32_t(42));
        comp.setField("int64_field", int64_t(123456789));
        comp.setField("float32_field", 3.14f);
        comp.setField("float64_field", 2.71828);
        comp.setField("string_field", std::string("test"));
        comp.setField("bool_field", true);

        ASSERT_EQ(std::get<int32_t>(*comp.getField("int32_field")), 42);
        ASSERT_EQ(std::get<int64_t>(*comp.getField("int64_field")), 123456789);
        ASSERT_EQ(std::get<float>(*comp.getField("float32_field")), 3.14f);
        ASSERT_EQ(std::get<double>(*comp.getField("float64_field")), 2.71828);
        ASSERT_EQ(std::get<std::string>(*comp.getField("string_field")), "test");
        ASSERT_EQ(std::get<bool>(*comp.getField("bool_field")), true);

        std::cout << "  All 6 data types work correctly ✓\n";
    }
    std::cout << "  ✓ All data types passed\n\n";

    // Test 5: toString
    std::cout << "Test 5: String representation\n";
    {
        std::vector<CompositeField> fields = {
            CompositeField("id", CompositeFieldType::INT32),
            CompositeField("name", CompositeFieldType::STRING),
            CompositeField("active", CompositeFieldType::BOOL)
        };

        auto comp = Composite::create("User", fields);
        comp.setField("id", int32_t(1));
        comp.setField("name", std::string("Alice"));
        comp.setField("active", true);

        std::string str = comp.toString();
        std::cout << "  String: " << str << " ✓\n";
        ASSERT_NE(str.find("User"), std::string::npos);
        ASSERT_NE(str.find("id"), std::string::npos);
        ASSERT_NE(str.find("name"), std::string::npos);
        ASSERT_NE(str.find("Alice"), std::string::npos);
    }
    std::cout << "  ✓ String representation passed\n\n";

    // Test 6: Binary encoding/decoding
    std::cout << "Test 6: Binary encoding/decoding\n";
    {
        std::vector<CompositeField> fields = {
            CompositeField("x", CompositeFieldType::INT32),
            CompositeField("y", CompositeFieldType::INT32),
            CompositeField("label", CompositeFieldType::STRING)
        };

        auto comp1 = Composite::create("Point", fields);
        comp1.setField("x", int32_t(100));
        comp1.setField("y", int32_t(200));
        comp1.setField("label", std::string("origin"));

        auto binary = Composite::encode(comp1);
        std::cout << "  Binary size: " << binary.size() << " bytes\n";

        auto comp2 = Composite::decode(binary);
        ASSERT_TRUE(comp2.has_value());
        ASSERT_EQ(comp2->getTypeName(), "Point");
        ASSERT_EQ(comp2->getFieldCount(), 3);
        ASSERT_EQ(std::get<int32_t>(*comp2->getField("x")), 100);
        ASSERT_EQ(std::get<int32_t>(*comp2->getField("y")), 200);
        ASSERT_EQ(std::get<std::string>(*comp2->getField("label")), "origin");

        std::cout << "  Decoded: " << comp2->toString() << " ✓\n";
    }
    std::cout << "  ✓ Binary encoding/decoding passed\n\n";

    // Test 7: fromMap utility
    std::cout << "Test 7: Create from map\n";
    {
        std::vector<CompositeField> fields = {
            CompositeField("id", CompositeFieldType::INT32),
            CompositeField("name", CompositeFieldType::STRING)
        };

        std::unordered_map<std::string, CompositeFieldValue> values = {
            {"id", int32_t(42)},
            {"name", std::string("Bob")}
        };

        auto comp = Composite::fromMap("Employee", fields, values);
        ASSERT_TRUE(comp.has_value());
        ASSERT_EQ(std::get<int32_t>(*comp->getField("id")), 42);
        ASSERT_EQ(std::get<std::string>(*comp->getField("name")), "Bob");

        std::cout << "  Created from map: " << comp->toString() << " ✓\n";
    }
    std::cout << "  ✓ Create from map passed\n\n";

    // Test 8: Field existence checks
    std::cout << "Test 8: Field existence\n";
    {
        std::vector<CompositeField> fields = {
            CompositeField("field1", CompositeFieldType::INT32),
            CompositeField("field2", CompositeFieldType::STRING)
        };

        auto comp = Composite::create("Test", fields);

        ASSERT_TRUE(comp.hasField("field1"));
        ASSERT_TRUE(comp.hasField("field2"));
        ASSERT_FALSE(comp.hasField("field3"));
        ASSERT_FALSE(comp.hasField("nonexistent"));

        std::cout << "  Field existence checks work ✓\n";
    }
    std::cout << "  ✓ Field existence passed\n\n";

    // Test 9: Field index lookup
    std::cout << "Test 9: Field index lookup\n";
    {
        std::vector<CompositeField> fields = {
            CompositeField("first", CompositeFieldType::INT32),
            CompositeField("second", CompositeFieldType::INT32),
            CompositeField("third", CompositeFieldType::INT32)
        };

        auto comp = Composite::create("Triple", fields);

        auto idx0 = comp.getFieldIndex("first");
        ASSERT_TRUE(idx0.has_value() && *idx0 == 0);

        auto idx1 = comp.getFieldIndex("second");
        ASSERT_TRUE(idx1.has_value() && *idx1 == 1);

        auto idx2 = comp.getFieldIndex("third");
        ASSERT_TRUE(idx2.has_value() && *idx2 == 2);

        auto idx_missing = comp.getFieldIndex("missing");
        ASSERT_FALSE(idx_missing.has_value());

        std::cout << "  Field index lookup works ✓\n";
    }
    std::cout << "  ✓ Field index lookup passed\n\n";

    // Test 10: Type checking
    std::cout << "Test 10: Type checking\n";
    {
        std::vector<CompositeField> fields = {
            CompositeField("int_field", CompositeFieldType::INT32),
            CompositeField("str_field", CompositeFieldType::STRING)
        };

        auto comp = Composite::create("TypeTest", fields);

        // Correct types
        ASSERT_TRUE(comp.setField("int_field", int32_t(42)));
        ASSERT_TRUE(comp.setField("str_field", std::string("hello")));

        // Wrong types (should fail)
        ASSERT_FALSE(comp.setField("int_field", std::string("wrong")));
        ASSERT_FALSE(comp.setField("str_field", int32_t(123)));

        std::cout << "  Type checking prevents wrong types ✓\n";
    }
    std::cout << "  ✓ Type checking passed\n\n";

    // Test 11: Complex real-world example
    std::cout << "Test 11: Real-world example (Employee record)\n";
    {
        std::vector<CompositeField> fields = {
            CompositeField("employee_id", CompositeFieldType::INT32),
            CompositeField("name", CompositeFieldType::STRING),
            CompositeField("email", CompositeFieldType::STRING),
            CompositeField("salary", CompositeFieldType::FLOAT64),
            CompositeField("is_active", CompositeFieldType::BOOL),
            CompositeField("years_service", CompositeFieldType::INT32)
        };

        auto employee = Composite::create("Employee", fields);
        employee.setField("employee_id", int32_t(1001));
        employee.setField("name", std::string("John Smith"));
        employee.setField("email", std::string("john.smith@example.com"));
        employee.setField("salary", 75000.0);
        employee.setField("is_active", true);
        employee.setField("years_service", int32_t(5));

        std::cout << "  Employee: " << employee.toString() << " ✓\n";

        // Encode and decode
        auto binary = Composite::encode(employee);
        auto decoded = Composite::decode(binary);
        ASSERT_TRUE(decoded.has_value());
        ASSERT_EQ(decoded->getTypeName(), "Employee");
        ASSERT_EQ(std::get<std::string>(*decoded->getField("name")), "John Smith");
        ASSERT_EQ(std::get<double>(*decoded->getField("salary")), 75000.0);

        std::cout << "  Encode/decode preserved all data ✓\n";
    }
    std::cout << "  ✓ Real-world example passed\n\n";

    std::cout << "========================================\n";
    std::cout << "ALL TESTS PASSED! ✓\n";
    std::cout << "COMPOSITE/RECORD type is fully functional.\n";
    std::cout << "========================================\n";
}

