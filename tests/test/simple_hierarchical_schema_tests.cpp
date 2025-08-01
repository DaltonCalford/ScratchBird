/*
 * ScratchBird v0.6.0 - Simple Hierarchical Schema Tests
 * Standalone test program for basic hierarchical schema functionality
 * 
 * Build Location: src/test_software/
 * Output Location: release/alpha0.6.0/bin/tests/
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <chrono>
#include <functional>
#include <algorithm>

namespace ScratchBird {

// Simple schema path validation functions (standalone implementation)
class SchemaPathValidator {
public:
    static bool isValidSchemaName(const std::string& name) {
        if (name.empty() || name.length() > 63) return false;
        if (!std::isalpha(name[0]) && name[0] != '_') return false;
        
        for (char c : name) {
            if (!std::isalnum(c) && c != '_') return false;
        }
        return true;
    }
    
    static bool isValidSchemaPath(const std::string& path) {
        if (path.empty()) return false;
        
        std::vector<std::string> components;
        parseSchemaPath(path, components);
        
        if (components.size() > 8) return false; // Max depth limit
        
        for (const auto& component : components) {
            if (!isValidSchemaName(component)) return false;
        }
        
        return true;
    }
    
    static void parseSchemaPath(const std::string& path, std::vector<std::string>& components) {
        components.clear();
        size_t start = 0;
        size_t end = path.find('.');
        
        while (end != std::string::npos) {
            components.push_back(path.substr(start, end - start));
            start = end + 1;
            end = path.find('.', start);
        }
        components.push_back(path.substr(start));
    }
    
    static int getSchemaDepth(const std::string& path) {
        std::vector<std::string> components;
        parseSchemaPath(path, components);
        return static_cast<int>(components.size());
    }
    
    static std::string getParentPath(const std::string& path) {
        size_t lastDot = path.find_last_of('.');
        if (lastDot == std::string::npos) return "";
        return path.substr(0, lastDot);
    }
    
    static std::string getLeafName(const std::string& path) {
        size_t lastDot = path.find_last_of('.');
        if (lastDot == std::string::npos) return path;
        return path.substr(lastDot + 1);
    }
};

// Simple test framework
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

// Test helper macros
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

#define EXPECT_GT(actual, threshold) \
    if ((actual) <= (threshold)) { \
        result.fail("Expected " #actual " > " #threshold); \
        return; \
    }

#define EXPECT_LT(actual, threshold) \
    if ((actual) >= (threshold)) { \
        result.fail("Expected " #actual " < " #threshold); \
        return; \
    }

using namespace ScratchBird;

int main() {
    std::cout << "ScratchBird v0.6.0 - Simple Hierarchical Schema Tests" << std::endl;
    std::cout << "=====================================================" << std::endl;
    
    TestSuite suite;
    
    // Test basic schema name validation
    suite.run_test("Basic Schema Name Validation", [](TestResult& result) {
        EXPECT_TRUE(SchemaPathValidator::isValidSchemaName("finance"));
        EXPECT_TRUE(SchemaPathValidator::isValidSchemaName("finance_dept"));
        EXPECT_TRUE(SchemaPathValidator::isValidSchemaName("FINANCE2024"));
        EXPECT_TRUE(SchemaPathValidator::isValidSchemaName("_underscore"));
        
        EXPECT_FALSE(SchemaPathValidator::isValidSchemaName("2finance"));
        EXPECT_FALSE(SchemaPathValidator::isValidSchemaName("finance-dept"));
        EXPECT_FALSE(SchemaPathValidator::isValidSchemaName("finance dept"));
        EXPECT_FALSE(SchemaPathValidator::isValidSchemaName(""));
        
        std::string long_name(64, 'x');
        EXPECT_FALSE(SchemaPathValidator::isValidSchemaName(long_name));
        
        std::string max_name(63, 'x');
        EXPECT_TRUE(SchemaPathValidator::isValidSchemaName(max_name));
    });
    
    // Test schema path validation
    suite.run_test("Schema Path Validation", [](TestResult& result) {
        EXPECT_TRUE(SchemaPathValidator::isValidSchemaPath("finance"));
        EXPECT_TRUE(SchemaPathValidator::isValidSchemaPath("finance.accounting"));
        EXPECT_TRUE(SchemaPathValidator::isValidSchemaPath("finance.accounting.reports"));
        
        // Test maximum depth (8 levels)
        EXPECT_TRUE(SchemaPathValidator::isValidSchemaPath("l1.l2.l3.l4.l5.l6.l7.l8"));
        EXPECT_FALSE(SchemaPathValidator::isValidSchemaPath("l1.l2.l3.l4.l5.l6.l7.l8.l9"));
        
        EXPECT_FALSE(SchemaPathValidator::isValidSchemaPath("finance.@invalid"));
        EXPECT_FALSE(SchemaPathValidator::isValidSchemaPath("finance..accounting"));
        EXPECT_FALSE(SchemaPathValidator::isValidSchemaPath(".finance"));
        EXPECT_FALSE(SchemaPathValidator::isValidSchemaPath("finance."));
    });
    
    // Test schema depth calculation
    suite.run_test("Schema Depth Calculation", [](TestResult& result) {
        EXPECT_EQ(SchemaPathValidator::getSchemaDepth("finance"), 1);
        EXPECT_EQ(SchemaPathValidator::getSchemaDepth("finance.accounting"), 2);
        EXPECT_EQ(SchemaPathValidator::getSchemaDepth("finance.accounting.reports"), 3);
        EXPECT_EQ(SchemaPathValidator::getSchemaDepth("l1.l2.l3.l4.l5.l6.l7.l8"), 8);
    });
    
    // Test path parsing
    suite.run_test("Schema Path Parsing", [](TestResult& result) {
        std::vector<std::string> components;
        
        SchemaPathValidator::parseSchemaPath("finance.accounting.reports", components);
        EXPECT_EQ(components.size(), 3);
        EXPECT_EQ(components[0], "finance");
        EXPECT_EQ(components[1], "accounting");
        EXPECT_EQ(components[2], "reports");
        
        SchemaPathValidator::parseSchemaPath("finance", components);
        EXPECT_EQ(components.size(), 1);
        EXPECT_EQ(components[0], "finance");
    });
    
    // Test parent path extraction
    suite.run_test("Parent Path Extraction", [](TestResult& result) {
        EXPECT_EQ(SchemaPathValidator::getParentPath("finance.accounting.reports"), "finance.accounting");
        EXPECT_EQ(SchemaPathValidator::getParentPath("finance.accounting"), "finance");
        EXPECT_EQ(SchemaPathValidator::getParentPath("finance"), "");
    });
    
    // Test leaf name extraction
    suite.run_test("Leaf Name Extraction", [](TestResult& result) {
        EXPECT_EQ(SchemaPathValidator::getLeafName("finance.accounting.reports"), "reports");
        EXPECT_EQ(SchemaPathValidator::getLeafName("finance.accounting"), "accounting");
        EXPECT_EQ(SchemaPathValidator::getLeafName("finance"), "finance");
    });
    
    // Test performance with many schema paths
    suite.run_test("Performance Test", [](TestResult& result) {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 10000; i++) {
            std::string schema_path = "schema" + std::to_string(i) + ".subsystem.component";
            EXPECT_TRUE(SchemaPathValidator::isValidSchemaPath(schema_path));
            EXPECT_EQ(SchemaPathValidator::getSchemaDepth(schema_path), 3);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "  Performance: 10,000 validations in " << duration.count() << "ms" << std::endl;
        EXPECT_LT(duration.count(), 100); // Should complete in under 100ms
    });
    
    // Test edge cases
    suite.run_test("Edge Cases", [](TestResult& result) {
        EXPECT_FALSE(SchemaPathValidator::isValidSchemaPath(""));
        
        // Test very long but valid path
        std::string long_path = "a.b.c.d.e.f.g.h"; // 8 levels, should be valid
        EXPECT_TRUE(SchemaPathValidator::isValidSchemaPath(long_path));
        
        // Test path with maximum length components
        std::string max_component(63, 'x');
        std::string max_path = max_component + "." + max_component;
        EXPECT_TRUE(SchemaPathValidator::isValidSchemaPath(max_path));
    });
    
    suite.print_summary();
    
    if (suite.all_passed()) {
        std::cout << "\n🎉 All tests passed! ScratchBird hierarchical schema validation is working correctly." << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ Some tests failed. Please review the implementation." << std::endl;
        return 1;
    }
}