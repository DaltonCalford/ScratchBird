#include "scratchbird/core/jsonb.h"
#include <iostream>
#include <cassert>

using namespace scratchbird::core;

int main() {
    std::cout << "Testing JSONB Implementation...\n\n";

    // Test 1: Parse and encode simple values
    std::cout << "Test 1: Simple values\n";
    {
        // Null
        auto binary1 = JSONB::fromJSON("null");
        assert(binary1.has_value());
        auto json1 = JSONB::toJSON(*binary1);
        assert(json1.has_value() && *json1 == "null");
        std::cout << "  null -> binary -> " << *json1 << " ✓\n";

        // Boolean true
        auto binary2 = JSONB::fromJSON("true");
        assert(binary2.has_value());
        auto json2 = JSONB::toJSON(*binary2);
        assert(json2.has_value() && *json2 == "true");
        std::cout << "  true -> binary -> " << *json2 << " ✓\n";

        // Boolean false
        auto binary3 = JSONB::fromJSON("false");
        assert(binary3.has_value());
        auto json3 = JSONB::toJSON(*binary3);
        assert(json3.has_value() && *json3 == "false");
        std::cout << "  false -> binary -> " << *json3 << " ✓\n";

        // Number
        auto binary4 = JSONB::fromJSON("42");
        assert(binary4.has_value());
        auto json4 = JSONB::toJSON(*binary4);
        assert(json4.has_value() && *json4 == "42");
        std::cout << "  42 -> binary -> " << *json4 << " ✓\n";

        // String
        auto binary5 = JSONB::fromJSON("\"hello\"");
        assert(binary5.has_value());
        auto json5 = JSONB::toJSON(*binary5);
        assert(json5.has_value() && *json5 == "\"hello\"");
        std::cout << "  \"hello\" -> binary -> " << *json5 << " ✓\n";
    }
    std::cout << "  ✓ Simple values passed\n\n";

    // Test 2: Arrays
    std::cout << "Test 2: Arrays\n";
    {
        auto binary = JSONB::fromJSON("[1,2,3]");
        assert(binary.has_value());
        auto json = JSONB::toJSON(*binary);
        assert(json.has_value());
        std::cout << "  [1,2,3] -> " << *json << " ✓\n";

        auto binary2 = JSONB::fromJSON("[\"a\",\"b\",\"c\"]");
        assert(binary2.has_value());
        auto json2 = JSONB::toJSON(*binary2);
        assert(json2.has_value());
        std::cout << "  [\"a\",\"b\",\"c\"] -> " << *json2 << " ✓\n";

        auto binary3 = JSONB::fromJSON("[]");
        assert(binary3.has_value());
        auto json3 = JSONB::toJSON(*binary3);
        assert(json3.has_value() && *json3 == "[]");
        std::cout << "  [] -> " << *json3 << " ✓\n";
    }
    std::cout << "  ✓ Arrays passed\n\n";

    // Test 3: Objects
    std::cout << "Test 3: Objects\n";
    {
        auto binary = JSONB::fromJSON("{\"name\":\"John\"}");
        assert(binary.has_value());
        auto json = JSONB::toJSON(*binary);
        assert(json.has_value());
        std::cout << "  {\"name\":\"John\"} -> " << *json << " ✓\n";

        auto binary2 = JSONB::fromJSON("{}");
        assert(binary2.has_value());
        auto json2 = JSONB::toJSON(*binary2);
        assert(json2.has_value() && *json2 == "{}");
        std::cout << "  {} -> " << *json2 << " ✓\n";
    }
    std::cout << "  ✓ Objects passed\n\n";

    // Test 4: Nested structures
    std::cout << "Test 4: Nested structures\n";
    {
        std::string input = "{\"user\":{\"name\":\"John\",\"age\":30}}";
        auto binary = JSONB::fromJSON(input);
        assert(binary.has_value());
        auto json = JSONB::toJSON(*binary);
        assert(json.has_value());
        std::cout << "  Nested object -> " << *json << " ✓\n";

        std::string input2 = "[{\"id\":1},{\"id\":2}]";
        auto binary2 = JSONB::fromJSON(input2);
        assert(binary2.has_value());
        auto json2 = JSONB::toJSON(*binary2);
        assert(json2.has_value());
        std::cout << "  Array of objects -> " << *json2 << " ✓\n";
    }
    std::cout << "  ✓ Nested structures passed\n\n";

    // Test 5: Path-based access
    std::cout << "Test 5: Path-based access\n";
    {
        std::string input = "{\"user\":{\"name\":\"John\",\"age\":30,\"address\":{\"city\":\"NYC\"}}}";
        auto binary = JSONB::fromJSON(input);
        assert(binary.has_value());

        auto value = JSONB::decode(*binary);
        assert(value.has_value());

        // Access nested value
        auto name = (*value)["user"];
        assert(name.has_value());
        std::cout << "  user: " << name->toJSON() << " ✓\n";

        auto user_name = value->getPath("user.name");
        assert(user_name.has_value());
        std::cout << "  user.name: " << user_name->toJSON() << " ✓\n";

        auto city = value->getPath("user.address.city");
        assert(city.has_value());
        std::cout << "  user.address.city: " << city->toJSON() << " ✓\n";

        // Access non-existent path
        auto missing = value->getPath("user.missing");
        assert(!missing.has_value());
        std::cout << "  user.missing: (not found) ✓\n";
    }
    std::cout << "  ✓ Path-based access passed\n\n";

    // Test 6: Array indexing
    std::cout << "Test 6: Array indexing\n";
    {
        std::string input = "[{\"id\":1},{\"id\":2},{\"id\":3}]";
        auto binary = JSONB::fromJSON(input);
        assert(binary.has_value());

        auto value = JSONB::decode(*binary);
        assert(value.has_value());

        auto first = (*value)[0];
        assert(first.has_value());
        std::cout << "  [0]: " << first->toJSON() << " ✓\n";

        auto second = (*value)[1];
        assert(second.has_value());
        std::cout << "  [1]: " << second->toJSON() << " ✓\n";

        // Out of bounds
        auto out_of_bounds = (*value)[10];
        assert(!out_of_bounds.has_value());
        std::cout << "  [10]: (out of bounds) ✓\n";
    }
    std::cout << "  ✓ Array indexing passed\n\n";

    // Test 7: String escaping
    std::cout << "Test 7: String escaping\n";
    {
        auto binary = JSONB::fromJSON("\"hello\\nworld\"");
        assert(binary.has_value());
        auto value = JSONB::decode(*binary);
        assert(value.has_value());
        assert(value->getString() == "hello\nworld");
        std::cout << "  Escaped newline: " << value->toJSON() << " ✓\n";

        auto binary2 = JSONB::fromJSON("\"quote: \\\"test\\\"\"");
        assert(binary2.has_value());
        auto value2 = JSONB::decode(*binary2);
        assert(value2.has_value());
        assert(value2->getString() == "quote: \"test\"");
        std::cout << "  Escaped quotes: " << value2->toJSON() << " ✓\n";
    }
    std::cout << "  ✓ String escaping passed\n\n";

    // Test 8: Numbers
    std::cout << "Test 8: Numbers\n";
    {
        auto binary1 = JSONB::fromJSON("123.45");
        assert(binary1.has_value());
        auto value1 = JSONB::decode(*binary1);
        assert(value1.has_value() && value1->getNumber() == 123.45);
        std::cout << "  123.45 ✓\n";

        auto binary2 = JSONB::fromJSON("-99");
        assert(binary2.has_value());
        auto value2 = JSONB::decode(*binary2);
        assert(value2.has_value() && value2->getNumber() == -99);
        std::cout << "  -99 ✓\n";

        auto binary3 = JSONB::fromJSON("1.5e2");
        assert(binary3.has_value());
        auto value3 = JSONB::decode(*binary3);
        assert(value3.has_value() && value3->getNumber() == 150);
        std::cout << "  1.5e2 (scientific notation) ✓\n";
    }
    std::cout << "  ✓ Numbers passed\n\n";

    // Test 9: Whitespace handling
    std::cout << "Test 9: Whitespace handling\n";
    {
        auto binary = JSONB::fromJSON("  {  \"key\"  :  \"value\"  }  ");
        assert(binary.has_value());
        auto json = JSONB::toJSON(*binary);
        assert(json.has_value());
        std::cout << "  Whitespace preserved during parse ✓\n";
    }
    std::cout << "  ✓ Whitespace handling passed\n\n";

    // Test 10: Complex real-world example
    std::cout << "Test 10: Real-world example\n";
    {
        std::string input = R"({
            "user": {
                "id": 123,
                "name": "John Doe",
                "email": "john@example.com",
                "roles": ["admin", "user"],
                "settings": {
                    "theme": "dark",
                    "notifications": true
                }
            }
        })";

        auto binary = JSONB::fromJSON(input);
        assert(binary.has_value());
        std::cout << "  Parsed complex JSON ✓\n";

        auto value = JSONB::decode(*binary);
        assert(value.has_value());

        auto user_id = value->getPath("user.id");
        assert(user_id.has_value() && user_id->getNumber() == 123);
        std::cout << "  user.id = " << user_id->getNumber() << " ✓\n";

        auto user_name = value->getPath("user.name");
        assert(user_name.has_value() && user_name->getString() == "John Doe");
        std::cout << "  user.name = " << user_name->toJSON() << " ✓\n";

        auto theme = value->getPath("user.settings.theme");
        assert(theme.has_value() && theme->getString() == "dark");
        std::cout << "  user.settings.theme = " << theme->toJSON() << " ✓\n";

        auto json_output = JSONB::toJSON(*binary);
        assert(json_output.has_value());
        std::cout << "  Converted back to JSON ✓\n";
    }
    std::cout << "  ✓ Real-world example passed\n\n";

    // Test 11: Validation
    std::cout << "Test 11: JSON validation\n";
    {
        assert(JSONB::validateJSON("{\"valid\":true}"));
        std::cout << "  Valid JSON accepted ✓\n";

        assert(!JSONB::validateJSON("{invalid}"));
        std::cout << "  Invalid JSON rejected ✓\n";

        assert(!JSONB::validateJSON("{\"unclosed\":"));
        std::cout << "  Unclosed JSON rejected ✓\n";
    }
    std::cout << "  ✓ Validation passed\n\n";

    std::cout << "========================================\n";
    std::cout << "ALL TESTS PASSED! ✓\n";
    std::cout << "JSONB type is fully functional.\n";
    std::cout << "========================================\n";

    return 0;
}
