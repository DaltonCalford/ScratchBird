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
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

// Test that nlohmann/json library is properly integrated
class JSONLibraryTest : public ::testing::Test
{
};

TEST_F(JSONLibraryTest, BasicParsing)
{
    std::string json_str = R"({"name": "John", "age": 30})";
    json j = json::parse(json_str);

    EXPECT_EQ(j["name"], "John");
    EXPECT_EQ(j["age"], 30);
}

TEST_F(JSONLibraryTest, ObjectConstruction)
{
    json j = {
        {"name", "Alice"},
        {"age", 25},
        {"city", "NYC"}
    };

    EXPECT_EQ(j["name"], "Alice");
    EXPECT_EQ(j["age"], 25);
    EXPECT_EQ(j["city"], "NYC");
}

TEST_F(JSONLibraryTest, ArrayConstruction)
{
    json j = {1, 2, 3, 4, 5};

    EXPECT_EQ(j.size(), 5);
    EXPECT_EQ(j[0], 1);
    EXPECT_EQ(j[4], 5);
}

TEST_F(JSONLibraryTest, NestedAccess)
{
    std::string json_str = R"({
        "user": {
            "name": "Bob",
            "address": {
                "city": "SF",
                "zip": "94102"
            }
        }
    })";

    json j = json::parse(json_str);

    EXPECT_EQ(j["user"]["name"], "Bob");
    EXPECT_EQ(j["user"]["address"]["city"], "SF");
    EXPECT_EQ(j["user"]["address"]["zip"], "94102");
}

TEST_F(JSONLibraryTest, JSONPointer)
{
    std::string json_str = R"({
        "user": {
            "name": "Charlie",
            "age": 35
        }
    })";

    json j = json::parse(json_str);

    // Using JSON Pointer (RFC 6901)
    EXPECT_EQ(j["/user/name"_json_pointer], "Charlie");
    EXPECT_EQ(j["/user/age"_json_pointer], 35);
}

TEST_F(JSONLibraryTest, Modification)
{
    json j = {{"name", "David"}, {"age", 40}};

    // Set value
    j["age"] = 41;
    EXPECT_EQ(j["age"], 41);

    // Insert value
    j["email"] = "david@test.com";
    EXPECT_EQ(j["email"], "david@test.com");

    // Remove value
    j.erase("email");
    EXPECT_FALSE(j.contains("email"));
}

TEST_F(JSONLibraryTest, Serialization)
{
    json j = {
        {"name", "Eve"},
        {"scores", {95, 87, 92}}
    };

    std::string output = j.dump();
    EXPECT_TRUE(output.find("Eve") != std::string::npos);
    EXPECT_TRUE(output.find("95") != std::string::npos);
}

TEST_F(JSONLibraryTest, TypeChecking)
{
    json j = {
        {"string", "hello"},
        {"number", 42},
        {"boolean", true},
        {"null", nullptr},
        {"array", {1, 2, 3}},
        {"object", {{"key", "value"}}}
    };

    EXPECT_TRUE(j["string"].is_string());
    EXPECT_TRUE(j["number"].is_number());
    EXPECT_TRUE(j["boolean"].is_boolean());
    EXPECT_TRUE(j["null"].is_null());
    EXPECT_TRUE(j["array"].is_array());
    EXPECT_TRUE(j["object"].is_object());
}

TEST_F(JSONLibraryTest, ErrorHandling)
{
    // Invalid JSON should throw
    std::string invalid_json = "{invalid}";
    EXPECT_THROW(json::parse(invalid_json), json::parse_error);

    // Accessing non-existent key should not crash (returns null)
    json j = {{"name", "Frank"}};
    EXPECT_TRUE(j["nonexistent"].is_null());
}

TEST_F(JSONLibraryTest, ArrayOperations)
{
    json j = json::array();

    j.push_back(1);
    j.push_back(2);
    j.push_back(3);

    EXPECT_EQ(j.size(), 3);
    EXPECT_EQ(j[0], 1);
    EXPECT_EQ(j[1], 2);
    EXPECT_EQ(j[2], 3);
}
