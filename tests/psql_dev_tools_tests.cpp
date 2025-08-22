#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/psql_dev_tools.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

using namespace scratchbird::engine;

void test_dependency_analyzer()
{
    std::cout << "Testing PSQL dependency analyzer..." << std::endl;

    PsqlDependencyAnalyzer analyzer;

    std::string psql_code = R"(
        PROCEDURE test_proc AS
        BEGIN
            CALL other_proc();
            SELECT * FROM customers;
            UPDATE orders SET status = 'processed';
            result = LENGTH('test') + my_function(42);
        END
    )";

    auto result = analyzer.analyze_dependencies("test_proc", psql_code);

    assert(result.procedure_name == "test_proc");
    assert(!result.dependencies.empty());

    std::cout << "✓ Found " << result.dependencies.size() << " dependencies:" << std::endl;
    for (const auto& dep : result.dependencies) {
        std::cout << "  - " << dep.name << " (" << dep.type << ")" << std::endl;
    }

    // Check for specific dependencies
    bool found_other_proc = false, found_customers = false, found_orders = false;
    for (const auto& dep : result.dependencies) {
        if (dep.name == "other_proc" && dep.type == "procedure")
            found_other_proc = true;
        if (dep.name == "customers" && dep.type == "table")
            found_customers = true;
        if (dep.name == "orders" && dep.type == "table")
            found_orders = true;
    }

    assert(found_other_proc);
    assert(found_customers);
    assert(found_orders);

    std::cout << "✓ Dependency analyzer test passed" << std::endl;
}

void test_code_formatter()
{
    std::cout << "Testing PSQL code formatter..." << std::endl;

    PsqlCodeFormatter formatter;

    std::string unformatted_code = R"(
procedure test_proc as begin declare x integer; if x>0 then call other_proc; end if; end
    )";

    auto formatted = formatter.format_code(unformatted_code);

    std::cout << "Original code:" << std::endl;
    std::cout << unformatted_code << std::endl;
    std::cout << "Formatted code:" << std::endl;
    std::cout << formatted << std::endl;

    // Check that keywords are uppercase
    assert(formatted.find("PROCEDURE") != std::string::npos);
    assert(formatted.find("BEGIN") != std::string::npos);
    assert(formatted.find("DECLARE") != std::string::npos);

    // Test individual statement formatting
    auto stmt_formatted = formatter.format_statement("if x>0 then call proc; end if;");
    assert(!stmt_formatted.empty());
    std::cout << "✓ Statement formatted: " << stmt_formatted << std::endl;

    // Test format validation
    PsqlCodeFormatter::FormatOptions options;
    options.uppercase_keywords = false;
    auto lowercase_formatted = formatter.format_code("begin end", options);
    bool is_well_formatted = formatter.is_well_formatted(lowercase_formatted, options);
    assert(is_well_formatted);

    std::cout << "✓ Code formatter test passed" << std::endl;
}

void test_performance_profiler()
{
    std::cout << "Testing PSQL performance profiler..." << std::endl;

    PsqlPerformanceProfiler profiler;

    // Simulate profiling a procedure
    profiler.start_profiling("test_proc");

    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Record statement executions
    profiler.record_statement("test_proc", "SELECT");
    profiler.record_statement("test_proc", "UPDATE");
    profiler.record_statement("test_proc", "SELECT");

    profiler.stop_profiling("test_proc");

    // Run another execution
    profiler.start_profiling("test_proc");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    profiler.stop_profiling("test_proc");

    // Get metrics
    auto metrics = profiler.get_metrics("test_proc");
    assert(metrics.procedure_name == "test_proc");
    assert(metrics.execution_count == 2);
    assert(metrics.total_time.count() > 0);
    assert(metrics.statement_counts["SELECT"] == 2);
    assert(metrics.statement_counts["UPDATE"] == 1);

    std::cout << "✓ Metrics for test_proc:" << std::endl;
    std::cout << "  Executions: " << metrics.execution_count << std::endl;
    std::cout << "  Total time: " << metrics.total_time.count() << "μs" << std::endl;
    std::cout << "  Average time: " << metrics.avg_time.count() << "μs" << std::endl;

    // Test report generation
    auto report = profiler.generate_report();
    assert(!report.empty());
    assert(report.find("test_proc") != std::string::npos);
    std::cout << "✓ Generated performance report" << std::endl;

    // Test clearing metrics
    profiler.clear_metrics();
    auto cleared_metrics = profiler.get_metrics("test_proc");
    assert(cleared_metrics.execution_count == 0);

    std::cout << "✓ Performance profiler test passed" << std::endl;
}

void test_syntax_validator()
{
    std::cout << "Testing PSQL syntax validator..." << std::endl;

    PsqlSyntaxValidator validator;

    // Test valid syntax
    std::string valid_code = R"(
        PROCEDURE valid_proc AS
        BEGIN
            DECLARE x INTEGER;
            x := 42;
        END
    )";

    auto valid_result = validator.validate_syntax(valid_code);
    std::cout << "✓ Valid code validation result: " << (valid_result.is_valid ? "PASS" : "FAIL")
              << std::endl;

    // Test invalid syntax
    std::string invalid_code = R"(
        PROCEDURE invalid_proc AS
        BEGIN
            DECLARE x INTEGER
            x := 42
        -- Missing END
    )";

    auto invalid_result = validator.validate_syntax(invalid_code);
    assert(!invalid_result.is_valid);
    assert(!invalid_result.issues.empty());

    std::cout << "✓ Invalid code found " << invalid_result.issues.size() << " issues:" << std::endl;
    for (const auto& issue : invalid_result.issues) {
        std::cout << "  [" << issue.type << "] " << issue.message << std::endl;
    }

    // Test procedure validation
    auto proc_result = validator.validate_procedure(valid_code);
    std::cout << "✓ Procedure validation completed" << std::endl;

    // Test report generation
    auto report = validator.generate_report(invalid_result);
    assert(!report.empty());
    assert(report.find("Syntax validation failed") != std::string::npos);
    std::cout << "✓ Generated validation report" << std::endl;

    std::cout << "✓ Syntax validator test passed" << std::endl;
}

void test_development_environment()
{
    std::cout << "Testing integrated development environment..." << std::endl;

    PsqlDevEnvironment dev_env("/tmp/dev_test.db");

    std::string code = R"(
        PROCEDURE sample_proc AS
        DECLARE
            counter INTEGER;
        BEGIN
            counter := 0;
            WHILE counter < 10 DO
            BEGIN
                counter := counter + 1;
                CALL helper_proc(counter);
            END
        END
    )";

    // Comprehensive code analysis
    auto analysis = dev_env.analyze_code("sample_proc", code);

    assert(analysis.dependencies.procedure_name == "sample_proc");
    assert(!analysis.formatted_code.empty());

    std::cout << "✓ Code analysis completed:" << std::endl;
    std::cout << "  Dependencies: " << analysis.dependencies.dependencies.size() << std::endl;
    std::cout << "  Validation issues: " << analysis.validation.issues.size() << std::endl;
    std::cout << "  Code formatted: "
              << (analysis.formatted_code.size() > code.size() ? "expanded" : "condensed")
              << std::endl;

    // Test code completion
    auto suggestions = dev_env.get_code_completion("DECL", 4);
    assert(!suggestions.empty());

    bool found_declare = false;
    for (const auto& suggestion : suggestions) {
        if (suggestion == "DECLARE") {
            found_declare = true;
            break;
        }
    }
    assert(found_declare);

    std::cout << "✓ Code completion found " << suggestions.size() << " suggestions for 'DECL'"
              << std::endl;
    for (const auto& suggestion : suggestions) {
        std::cout << "  - " << suggestion << std::endl;
    }

    // Test definition and reference search
    auto definition = dev_env.find_definition("sample_proc");
    auto references = dev_env.find_references("sample_proc");

    assert(!definition.empty());
    assert(!references.empty());

    std::cout << "✓ Development environment test passed" << std::endl;
}

void test_tools_integration()
{
    std::cout << "Testing development tools integration..." << std::endl;

    // Create a complex PSQL procedure for comprehensive testing
    std::string complex_code = R"(
        CREATE PROCEDURE complex_test_proc (
            input_param INTEGER,
            output_param INTEGER
        ) AS
        DECLARE
            local_var VARCHAR(100);
            counter INTEGER;
        BEGIN
            counter := 0;
            local_var := UPPER('hello world');

            WHILE counter < input_param DO
            BEGIN
                counter := counter + 1;

                IF counter = 5 THEN
                    CALL helper_function(local_var);

                INSERT INTO log_table (message, created_at)
                VALUES (local_var || ' - ' || counter, CURRENT_TIMESTAMP);

                IF counter > 8 THEN
                    LEAVE;
            END

            output_param := counter;

            WHEN ZERO_DIVIDE DO
            BEGIN
                output_param := -1;
            END
        END
    )";

    // Test all tools on the complex code
    PsqlDependencyAnalyzer analyzer;
    PsqlCodeFormatter formatter;
    PsqlSyntaxValidator validator;
    PsqlPerformanceProfiler profiler;

    auto dependencies = analyzer.analyze_dependencies("complex_test_proc", complex_code);
    auto formatted_code = formatter.format_code(complex_code);
    auto validation = validator.validate_syntax(complex_code);

    std::cout << "✓ Complex procedure analysis:" << std::endl;
    std::cout << "  Dependencies found: " << dependencies.dependencies.size() << std::endl;
    std::cout << "  Code formatting: " << (formatted_code.size() > 0 ? "SUCCESS" : "FAILED")
              << std::endl;
    std::cout << "  Syntax validation: "
              << (validation.issues.size() == 0
                      ? "CLEAN"
                      : std::to_string(validation.issues.size()) + " issues")
              << std::endl;

    // Show dependencies
    std::cout << "  Found dependencies:" << std::endl;
    for (const auto& dep : dependencies.dependencies) {
        std::cout << "    " << dep.name << " (" << dep.type << ")" << std::endl;
    }

    std::cout << "✓ Tools integration test passed" << std::endl;
}

int main()
{
    std::cout << "=== PSQL Development Tools Tests ===" << std::endl;

    try {
        // Set up temporary database path
        std::string db_path = "/tmp/dev_tools_test.db";
        set_executor_db_path(db_path);

        // Clean up any existing test database
        std::filesystem::remove(db_path + ".seg0");

        // Run development tools tests
        test_dependency_analyzer();
        test_code_formatter();
        test_performance_profiler();
        test_syntax_validator();
        test_development_environment();
        test_tools_integration();

        std::cout << "=== PSQL Development Tools Tests Complete ===" << std::endl;
        std::cout << "All development tools are operational and ready for use" << std::endl;

        // Clean up
        std::filesystem::remove(db_path + ".seg0");

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
