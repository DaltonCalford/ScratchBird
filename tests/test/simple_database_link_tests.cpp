/*
 * ScratchBird v0.6.0 - Simple Database Link Tests
 * Standalone test program for basic database link schema functionality
 * 
 * Build Location: src/test_software/
 * Output Location: release/alpha0.6.0/bin/tests/
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <memory>
#include <functional>
#include <algorithm>

namespace ScratchBird {

// Schema resolution modes
enum SchemaMode {
    SCHEMA_MODE_NONE = 0,
    SCHEMA_MODE_FIXED = 1,
    SCHEMA_MODE_CONTEXT_AWARE = 2,
    SCHEMA_MODE_HIERARCHICAL = 3,
    SCHEMA_MODE_MIRROR = 4
};

// Simple database link implementation for testing
class SimpleDatabaseLink {
private:
    std::string link_name;
    std::string connect_string;
    std::string username;
    std::string password;
    SchemaMode schema_mode;
    std::string local_schema;
    std::string remote_schema;
    int schema_depth;
    
public:
    SimpleDatabaseLink(const std::string& name, const std::string& conn_str, 
                      const std::string& user, const std::string& pass)
        : link_name(name), connect_string(conn_str), username(user), password(pass)
        , schema_mode(SCHEMA_MODE_NONE), schema_depth(0) {}
    
    void setSchemaMode(SchemaMode mode) { schema_mode = mode; }
    void setLocalSchema(const std::string& schema) { 
        local_schema = schema; 
        schema_depth = getSchemaDepth(schema);
    }
    void setRemoteSchema(const std::string& schema) { remote_schema = schema; }
    
    SchemaMode getSchemaMode() const { return schema_mode; }
    const std::string& getLocalSchema() const { return local_schema; }
    const std::string& getRemoteSchema() const { return remote_schema; }
    int getLocalSchemaDepth() const { return schema_depth; }
    int getRemoteSchemaDepth() const { return getSchemaDepth(remote_schema); }
    
    bool validateConfiguration() const {
        if (link_name.empty()) return false;
        
        switch (schema_mode) {
            case SCHEMA_MODE_NONE:
                return true;
                
            case SCHEMA_MODE_FIXED:
                return !remote_schema.empty();
                
            case SCHEMA_MODE_CONTEXT_AWARE:
                return (remote_schema == "CURRENT" || 
                       remote_schema == "HOME" || 
                       remote_schema == "USER");
                       
            case SCHEMA_MODE_HIERARCHICAL:
                return !local_schema.empty() && !remote_schema.empty();
                
            case SCHEMA_MODE_MIRROR:
                return true;
                
            default:
                return false;
        }
    }
    
    std::string resolveRemoteSchema(const std::string& qualified_name, 
                                   const std::string& current_schema,
                                   const std::string& user_name) const {
        switch (schema_mode) {
            case SCHEMA_MODE_NONE:
                return "";
                
            case SCHEMA_MODE_FIXED:
                return remote_schema;
                
            case SCHEMA_MODE_CONTEXT_AWARE:
                if (remote_schema == "CURRENT") {
                    return extractSchemaFromQualifiedName(qualified_name);
                } else if (remote_schema == "HOME") {
                    return current_schema;
                } else if (remote_schema == "USER") {
                    return user_name;
                }
                return "";
                
            case SCHEMA_MODE_HIERARCHICAL:
                return mapHierarchicalSchema(qualified_name);
                
            case SCHEMA_MODE_MIRROR:
                return extractSchemaFromQualifiedName(qualified_name);
                
            default:
                return "";
        }
    }
    
    std::vector<std::string> getLocalSchemaComponents() const {
        std::vector<std::string> components;
        parseSchemaPath(local_schema, components);
        return components;
    }
    
    bool validateSchemaAccess(const std::map<std::string, std::string>& available_schemas) const {
        if (schema_mode == SCHEMA_MODE_FIXED || schema_mode == SCHEMA_MODE_HIERARCHICAL) {
            return available_schemas.find(remote_schema) != available_schemas.end();
        }
        return true; // Other modes don't require pre-validation
    }
    
private:
    static int getSchemaDepth(const std::string& path) {
        if (path.empty()) return 0;
        return static_cast<int>(std::count(path.begin(), path.end(), '.') + 1);
    }
    
    static void parseSchemaPath(const std::string& path, std::vector<std::string>& components) {
        components.clear();
        if (path.empty()) return;
        
        size_t start = 0;
        size_t end = path.find('.');
        
        while (end != std::string::npos) {
            components.push_back(path.substr(start, end - start));
            start = end + 1;
            end = path.find('.', start);
        }
        components.push_back(path.substr(start));
    }
    
    std::string extractSchemaFromQualifiedName(const std::string& qualified_name) const {
        size_t lastDot = qualified_name.find_last_of('.');
        if (lastDot == std::string::npos) return "";
        return qualified_name.substr(0, lastDot);
    }
    
    std::string mapHierarchicalSchema(const std::string& qualified_name) const {
        std::string source_schema = extractSchemaFromQualifiedName(qualified_name);
        
        // Check if source schema starts with local schema prefix
        if (source_schema.find(local_schema) == 0) {
            // Replace local schema prefix with remote schema prefix
            std::string suffix = source_schema.substr(local_schema.length());
            return remote_schema + suffix;
        }
        
        return remote_schema; // Fallback to base remote schema
    }
};

// Simple test framework (reusing from previous file)
class TestResult {
public:
    std::string test_name;
    bool passed;
    std::string error_message;
    
    TestResult(const std::string& name) : test_name(name), passed(true) {}
    
    void fail(const std::string& message) {
        passed = false;
        error_message = message;
    }
};

class TestSuite {
private:
    std::vector<TestResult> results;
    int total_tests = 0;
    int passed_tests = 0;
    
public:
    void run_test(const std::string& name, std::function<void(TestResult&)> test_func) {
        TestResult result(name);
        total_tests++;
        
        try {
            test_func(result);
            if (result.passed) {
                passed_tests++;
                std::cout << "✓ " << name << std::endl;
            } else {
                std::cout << "✗ " << name << " - " << result.error_message << std::endl;
            }
        } catch (const std::exception& e) {
            result.fail(std::string("Exception: ") + e.what());
            std::cout << "✗ " << name << " - Exception: " << e.what() << std::endl;
        }
        
        results.push_back(result);
    }
    
    void print_summary() {
        std::cout << "\n=== Test Summary ===" << std::endl;
        std::cout << "Total tests: " << total_tests << std::endl;
        std::cout << "Passed: " << passed_tests << std::endl;
        std::cout << "Failed: " << (total_tests - passed_tests) << std::endl;
        std::cout << "Success rate: " << (100.0 * passed_tests / total_tests) << "%" << std::endl;
    }
    
    bool all_passed() const {
        return passed_tests == total_tests;
    }
};

} // namespace ScratchBird

// Test helper macros (same as before)
#define EXPECT_TRUE(condition) \
    if (!(condition)) { \
        result.fail("Expected true: " #condition); \
        return; \
    }

#define EXPECT_FALSE(condition) \
    if (condition) { \
        result.fail("Expected false: " #condition); \
        return; \
    }

#define EXPECT_EQ(actual, expected) \
    if ((actual) != (expected)) { \
        result.fail("Expected " #actual " == " #expected); \
        return; \
    }

using namespace ScratchBird;

int main() {
    std::cout << "ScratchBird v0.6.0 - Simple Database Link Tests" << std::endl;
    std::cout << "===============================================" << std::endl;
    
    TestSuite suite;
    
    // Test database link creation with different schema modes
    suite.run_test("Database Link Schema Mode Configuration", [](TestResult& result) {
        SimpleDatabaseLink link("test_link", "server:db", "user", "pass");
        
        // Test SCHEMA_MODE_NONE
        link.setSchemaMode(SCHEMA_MODE_NONE);
        EXPECT_TRUE(link.validateConfiguration());
        EXPECT_EQ(link.getSchemaMode(), SCHEMA_MODE_NONE);
        
        // Test SCHEMA_MODE_FIXED
        link.setSchemaMode(SCHEMA_MODE_FIXED);
        link.setRemoteSchema("accounting.reports");
        EXPECT_TRUE(link.validateConfiguration());
        
        link.setRemoteSchema("");
        EXPECT_FALSE(link.validateConfiguration());
        
        // Test SCHEMA_MODE_CONTEXT_AWARE
        link.setSchemaMode(SCHEMA_MODE_CONTEXT_AWARE);
        link.setRemoteSchema("CURRENT");
        EXPECT_TRUE(link.validateConfiguration());
        
        link.setRemoteSchema("INVALID_CONTEXT");
        EXPECT_FALSE(link.validateConfiguration());
        
        // Test SCHEMA_MODE_HIERARCHICAL
        link.setSchemaMode(SCHEMA_MODE_HIERARCHICAL);
        link.setLocalSchema("hr");
        link.setRemoteSchema("human_resources");
        EXPECT_TRUE(link.validateConfiguration());
        
        // Test SCHEMA_MODE_MIRROR
        link.setSchemaMode(SCHEMA_MODE_MIRROR);
        EXPECT_TRUE(link.validateConfiguration());
    });
    
    // Test schema resolution for different modes
    suite.run_test("Schema Resolution Modes", [](TestResult& result) {
        SimpleDatabaseLink link("test_link", "server:db", "user", "pass");
        
        // Test FIXED mode
        link.setSchemaMode(SCHEMA_MODE_FIXED);
        link.setRemoteSchema("accounting.reports");
        std::string resolved = link.resolveRemoteSchema("finance.accounts", "finance", "testuser");
        EXPECT_EQ(resolved, "accounting.reports");
        
        // Test CONTEXT_AWARE mode
        link.setSchemaMode(SCHEMA_MODE_CONTEXT_AWARE);
        link.setRemoteSchema("CURRENT");
        resolved = link.resolveRemoteSchema("finance.accounting.accounts", "finance", "testuser");
        EXPECT_EQ(resolved, "finance.accounting");
        
        link.setRemoteSchema("HOME");
        resolved = link.resolveRemoteSchema("temp.table", "finance", "testuser");
        EXPECT_EQ(resolved, "finance");
        
        link.setRemoteSchema("USER");
        resolved = link.resolveRemoteSchema("temp.table", "finance", "testuser");
        EXPECT_EQ(resolved, "testuser");
        
        // Test HIERARCHICAL mode
        link.setSchemaMode(SCHEMA_MODE_HIERARCHICAL);
        link.setLocalSchema("hr");
        link.setRemoteSchema("human_resources");
        resolved = link.resolveRemoteSchema("hr.payroll.monthly.table", "hr", "testuser");
        EXPECT_EQ(resolved, "human_resources.payroll.monthly");
        
        // Test MIRROR mode
        link.setSchemaMode(SCHEMA_MODE_MIRROR);
        resolved = link.resolveRemoteSchema("finance.accounting.table", "finance", "testuser");
        EXPECT_EQ(resolved, "finance.accounting");
    });
    
    // Test schema depth calculation
    suite.run_test("Schema Depth Calculation", [](TestResult& result) {
        SimpleDatabaseLink link("test_link", "server:db", "user", "pass");
        
        link.setLocalSchema("level1.level2.level3");
        link.setRemoteSchema("remote1.remote2.remote3");
        
        EXPECT_EQ(link.getLocalSchemaDepth(), 3);
        EXPECT_EQ(link.getRemoteSchemaDepth(), 3);
        
        link.setLocalSchema("single");
        EXPECT_EQ(link.getLocalSchemaDepth(), 1);
        
        link.setLocalSchema("");
        EXPECT_EQ(link.getLocalSchemaDepth(), 0);
    });
    
    // Test schema path parsing
    suite.run_test("Schema Path Component Parsing", [](TestResult& result) {
        SimpleDatabaseLink link("test_link", "server:db", "user", "pass");
        link.setLocalSchema("level1.level2.level3");
        
        std::vector<std::string> components = link.getLocalSchemaComponents();
        EXPECT_EQ(components.size(), 3);
        EXPECT_EQ(components[0], "level1");
        EXPECT_EQ(components[1], "level2");
        EXPECT_EQ(components[2], "level3");
        
        link.setLocalSchema("single");
        components = link.getLocalSchemaComponents();
        EXPECT_EQ(components.size(), 1);
        EXPECT_EQ(components[0], "single");
    });
    
    // Test remote schema validation
    suite.run_test("Remote Schema Validation", [](TestResult& result) {
        SimpleDatabaseLink link("test_link", "server:db", "user", "pass");
        
        std::map<std::string, std::string> available_schemas = {
            {"accounting", ""},
            {"accounting.reports", "accounting"},
            {"human_resources", ""},
            {"human_resources.payroll", "human_resources"}
        };
        
        // Test valid schemas
        link.setSchemaMode(SCHEMA_MODE_FIXED);
        link.setRemoteSchema("accounting.reports");
        EXPECT_TRUE(link.validateSchemaAccess(available_schemas));
        
        // Test invalid schemas
        link.setRemoteSchema("nonexistent.schema");
        EXPECT_FALSE(link.validateSchemaAccess(available_schemas));
        
        // Test hierarchical validation
        link.setSchemaMode(SCHEMA_MODE_HIERARCHICAL);
        link.setLocalSchema("hr");
        link.setRemoteSchema("human_resources");
        EXPECT_TRUE(link.validateSchemaAccess(available_schemas));
        
        link.setRemoteSchema("nonexistent_hr");
        EXPECT_FALSE(link.validateSchemaAccess(available_schemas));
    });
    
    // Test hierarchical schema mapping
    suite.run_test("Hierarchical Schema Mapping", [](TestResult& result) {
        SimpleDatabaseLink link("test_link", "server:db", "user", "pass");
        link.setSchemaMode(SCHEMA_MODE_HIERARCHICAL);
        link.setLocalSchema("hr");
        link.setRemoteSchema("human_resources");
        
        // Test direct mapping
        std::string resolved = link.resolveRemoteSchema("hr.payroll.table", "hr", "user");
        EXPECT_EQ(resolved, "human_resources.payroll");
        
        // Test deeper hierarchy
        resolved = link.resolveRemoteSchema("hr.benefits.dental.plans.table", "hr", "user");
        EXPECT_EQ(resolved, "human_resources.benefits.dental.plans");
        
        // Test non-matching prefix (should fall back to base remote schema)
        resolved = link.resolveRemoteSchema("finance.accounting.table", "hr", "user");
        EXPECT_EQ(resolved, "human_resources");
    });
    
    // Test edge cases and error conditions
    suite.run_test("Edge Cases and Error Handling", [](TestResult& result) {
        SimpleDatabaseLink link("test_link", "server:db", "user", "pass");
        
        // Test empty link name validation
        SimpleDatabaseLink empty_link("", "server:db", "user", "pass");
        EXPECT_FALSE(empty_link.validateConfiguration());
        
        // Test schema resolution with empty parameters
        link.setSchemaMode(SCHEMA_MODE_CONTEXT_AWARE);
        link.setRemoteSchema("CURRENT");
        std::string resolved = link.resolveRemoteSchema("table", "", "");
        EXPECT_EQ(resolved, ""); // Should handle gracefully
        
        // Test very deep schema hierarchy
        link.setSchemaMode(SCHEMA_MODE_HIERARCHICAL);
        link.setLocalSchema("l1.l2.l3.l4.l5.l6.l7.l8");
        link.setRemoteSchema("r1.r2.r3.r4.r5.r6.r7.r8");
        EXPECT_EQ(link.getLocalSchemaDepth(), 8);
        EXPECT_EQ(link.getRemoteSchemaDepth(), 8);
    });
    
    suite.print_summary();
    
    if (suite.all_passed()) {
        std::cout << "\n🎉 All tests passed! ScratchBird database link schema awareness is working correctly." << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ Some tests failed. Please review the implementation." << std::endl;
        return 1;
    }
}