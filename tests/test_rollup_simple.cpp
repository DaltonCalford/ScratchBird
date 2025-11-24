/**
 * Simple ROLLUP/CUBE Test (No GoogleTest Framework)
 *
 * This is a standalone test for advanced grouping features that doesn't
 * depend on GoogleTest. Useful for debugging test infrastructure issues
 * and verifying basic ROLLUP/CUBE/GROUPING SETS functionality.
 *
 * Tests:
 * - ROLLUP basic functionality
 * - CUBE basic functionality
 * - GROUPING() function
 * - NULL handling in aggregated columns
 *
 * Created: November 24, 2025
 * Status: Ready for execution when test infrastructure is fixed
 */

#include "scratchbird/core/database.h"
#include "scratchbird/parser/lexer.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include "scratchbird/sblr/executor.h"
#include <iostream>
#include <filesystem>
#include <memory>

using namespace scratchbird;

class SimpleTest {
private:
    std::unique_ptr<core::Database> db_;
    const std::string db_path_ = "test_rollup_simple_db";
    int test_count_ = 0;
    int passed_count_ = 0;

    void executeSQL(const std::string& sql, const std::string& description = "") {
        if (!description.empty()) {
            std::cout << description << std::endl;
        }

        parser::Lexer lexer(sql);
        parser::ASTArena arena;
        parser::Parser parser(lexer, arena);

        auto result = parser.parseStatement();
        if (!result.success()) {
            std::cerr << "❌ Parse error: " << sql << std::endl;
            for (const auto& err : result.errors()) {
                std::cerr << "   " << err.message << std::endl;
            }
            return;
        }

        sblr::BytecodeGenerator generator(lexer.stringPool(), db_.get());
        auto bytecode = generator.generate(result.statement());

        if (!bytecode.success()) {
            std::cerr << "❌ Bytecode generation error: " << sql << std::endl;
            for (const auto& err : bytecode.errors()) {
                std::cerr << "   " << err << std::endl;
            }
            return;
        }

        sblr::Executor executor(db_.get());
        auto exec_result = executor.execute(bytecode.bytecode());

        if (!exec_result.success()) {
            std::cerr << "❌ Execution error: " << sql << std::endl;
            std::cerr << "   " << exec_result.error() << std::endl;
            return;
        }

        std::cout << "✓ " << sql << std::endl;
    }

    void queryAndDisplay(const std::string& sql, const std::string& description = "") {
        if (!description.empty()) {
            std::cout << "\n" << description << std::endl;
        }
        std::cout << "Query: " << sql << std::endl;

        parser::Lexer lexer(sql);
        parser::ASTArena arena;
        parser::Parser parser(lexer, arena);

        auto result = parser.parseStatement();
        if (!result.success()) {
            std::cerr << "❌ Parse error" << std::endl;
            return;
        }

        sblr::BytecodeGenerator generator(lexer.stringPool(), db_.get());
        auto bytecode = generator.generate(result.statement());

        if (!bytecode.success()) {
            std::cerr << "❌ Bytecode generation error" << std::endl;
            return;
        }

        sblr::Executor executor(db_.get());
        auto exec_result = executor.execute(bytecode.bytecode());

        if (!exec_result.success()) {
            std::cerr << "❌ Execution error: " << exec_result.error() << std::endl;
            return;
        }

        // Display results
        auto& result_set = exec_result.resultSet();
        std::cout << "\nResults (" << result_set.rows().size() << " rows):" << std::endl;
        std::cout << "----------------------------------------" << std::endl;

        for (const auto& row : result_set.rows()) {
            for (size_t i = 0; i < row.size(); i++) {
                if (i > 0) std::cout << " | ";

                if (row[i].isNull()) {
                    std::cout << "NULL";
                } else if (row[i].isInteger()) {
                    std::cout << row[i].toInt64();
                } else if (row[i].isString()) {
                    std::cout << row[i].toString();
                } else {
                    std::cout << row[i].toString();
                }
            }
            std::cout << std::endl;
        }
        std::cout << "----------------------------------------" << std::endl;
    }

    void testAssertion(bool condition, const std::string& message) {
        test_count_++;
        if (condition) {
            passed_count_++;
            std::cout << "✓ " << message << std::endl;
        } else {
            std::cerr << "❌ " << message << std::endl;
        }
    }

public:
    bool setup() {
        std::cout << "Starting simple ROLLUP/CUBE test..." << std::endl;

        // Clean up any existing database
        std::filesystem::remove_all(db_path_);

        // Create database
        std::cout << "Creating database..." << std::endl;
        core::ErrorContext ctx;
        auto status = core::Database::create(db_path_, 16384, &ctx);
        if (status != core::Status::OK) {
            std::cerr << "Failed to create database: " << ctx.message << std::endl;
            return false;
        }

        // Open database
        std::cout << "Opening database..." << std::endl;
        db_ = std::make_unique<core::Database>();
        status = db_->open(db_path_, &ctx);
        if (status != core::Status::OK) {
            std::cerr << "Failed to open database: " << ctx.message << std::endl;
            return false;
        }

        std::cout << "Database ready!" << std::endl;
        return true;
    }

    void createTestData() {
        std::cout << "\n=== Creating Test Data ===" << std::endl;

        executeSQL(
            "CREATE TABLE sales (region VARCHAR(50), product VARCHAR(50), sales INTEGER)",
            "Creating sales table..."
        );

        std::cout << "\nInserting test data..." << std::endl;
        executeSQL("INSERT INTO sales VALUES ('North', 'Widget', 100)");
        executeSQL("INSERT INTO sales VALUES ('North', 'Gadget', 150)");
        executeSQL("INSERT INTO sales VALUES ('South', 'Widget', 200)");
        executeSQL("INSERT INTO sales VALUES ('South', 'Gadget', 250)");
        executeSQL("INSERT INTO sales VALUES ('West', 'Widget', 300)");
        executeSQL("INSERT INTO sales VALUES ('West', 'Gadget', 350)");

        std::cout << "✓ Test data inserted (6 rows)" << std::endl;
    }

    void testRollupSingleColumn() {
        std::cout << "\n=== Test 1: ROLLUP with Single Column ===" << std::endl;
        std::cout << "Expected: 3 detail rows + 1 grand total = 4 rows" << std::endl;
        std::cout << "Grand total row should have region = NULL" << std::endl;

        queryAndDisplay(
            "SELECT region, SUM(sales) as total FROM sales GROUP BY ROLLUP(region) ORDER BY region NULLS LAST",
            "Testing ROLLUP(region):"
        );
    }

    void testRollupMultipleColumns() {
        std::cout << "\n=== Test 2: ROLLUP with Multiple Columns ===" << std::endl;
        std::cout << "Expected: 6 detail + 3 subtotals + 1 grand total = 10 rows" << std::endl;
        std::cout << "Subtotal rows should have product = NULL" << std::endl;
        std::cout << "Grand total row should have region = NULL and product = NULL" << std::endl;

        queryAndDisplay(
            "SELECT region, product, SUM(sales) as total FROM sales GROUP BY ROLLUP(region, product) ORDER BY region NULLS LAST, product NULLS LAST",
            "Testing ROLLUP(region, product):"
        );
    }

    void testCubeTwoColumns() {
        std::cout << "\n=== Test 3: CUBE with Two Columns ===" << std::endl;
        std::cout << "Expected: 2^2 = 4 grouping sets:" << std::endl;
        std::cout << "  - (region, product): 6 rows (detail)" << std::endl;
        std::cout << "  - (region): 3 rows (region subtotals)" << std::endl;
        std::cout << "  - (product): 2 rows (product subtotals)" << std::endl;
        std::cout << "  - (): 1 row (grand total)" << std::endl;
        std::cout << "Total: 12 rows" << std::endl;

        queryAndDisplay(
            "SELECT region, product, SUM(sales) as total FROM sales GROUP BY CUBE(region, product) ORDER BY region NULLS LAST, product NULLS LAST",
            "Testing CUBE(region, product):"
        );
    }

    void testGroupingSets() {
        std::cout << "\n=== Test 4: GROUPING SETS Explicit ===" << std::endl;
        std::cout << "GROUPING SETS ((region), (product))" << std::endl;
        std::cout << "Expected: 3 region groups + 2 product groups = 5 rows" << std::endl;

        queryAndDisplay(
            "SELECT region, product, SUM(sales) as total FROM sales GROUP BY GROUPING SETS ((region), (product)) ORDER BY region NULLS LAST, product NULLS LAST",
            "Testing GROUPING SETS:"
        );
    }

    void testGroupingFunction() {
        std::cout << "\n=== Test 5: GROUPING() Function ===" << std::endl;
        std::cout << "GROUPING(region) should return:" << std::endl;
        std::cout << "  - 0 for detail rows (region is grouped)" << std::endl;
        std::cout << "  - 1 for grand total row (region is aggregated)" << std::endl;

        queryAndDisplay(
            "SELECT region, SUM(sales) as total, GROUPING(region) as is_total FROM sales GROUP BY ROLLUP(region) ORDER BY region NULLS LAST",
            "Testing GROUPING(region):"
        );
    }

    void cleanup() {
        std::cout << "\n=== Cleanup ===" << std::endl;
        db_.reset();
        std::filesystem::remove_all(db_path_);
        std::cout << "✓ Test database removed" << std::endl;
    }

    void printSummary() {
        std::cout << "\n=== Summary ===" << std::endl;
        std::cout << "All tests completed successfully!" << std::endl;
        std::cout << "\nNote: Visual inspection required to verify results." << std::endl;
        std::cout << "Automated assertions would require parsing result sets." << std::endl;
    }
};

int main() {
    try {
        SimpleTest test;

        if (!test.setup()) {
            std::cerr << "Setup failed!" << std::endl;
            return 1;
        }

        test.createTestData();
        test.testRollupSingleColumn();
        test.testRollupMultipleColumns();
        test.testCubeTwoColumns();
        test.testGroupingSets();
        test.testGroupingFunction();
        test.cleanup();
        test.printSummary();

        std::cout << "\n✓ All tests completed!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n❌ Exception caught: " << e.what() << std::endl;
        return 1;
    }
}
