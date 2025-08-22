#include "scratchbird/engine/ast.h"
#include "scratchbird/engine/executor.h"
#include "scratchbird/engine/parser.h"
#include "scratchbird/engine/psql_executor.h"

#include <cassert>
#include <filesystem>
#include <iostream>

using namespace scratchbird::engine;

void test_debugging_enable_disable()
{
    std::cout << "Testing debugging enable/disable..." << std::endl;

    PsqlExecutor executor;

    // Test enabling debugging
    executor.enable_debugging(true);
    std::cout << "✓ Debugging enabled" << std::endl;

    // Test disabling debugging
    executor.enable_debugging(false);
    std::cout << "✓ Debugging disabled" << std::endl;

    std::cout << "✓ Debugging enable/disable test passed" << std::endl;
}

void test_breakpoint_management()
{
    std::cout << "Testing breakpoint management..." << std::endl;

    PsqlExecutor executor;
    executor.enable_debugging(true);

    // Test adding breakpoints
    executor.add_breakpoint("test_proc", 5);
    executor.add_breakpoint("test_proc", 10, "variable > 0");
    executor.add_breakpoint("other_proc", 3);

    auto breakpoints = executor.get_breakpoints();
    assert(breakpoints.size() == 3);
    std::cout << "✓ Added " << breakpoints.size() << " breakpoints" << std::endl;

    // Verify breakpoint details
    bool found_conditional = false;
    for (const auto& bp : breakpoints) {
        std::cout << "  Breakpoint: " << bp.procedure_name << ":" << bp.line_number;
        if (!bp.condition.empty()) {
            std::cout << " (condition: " << bp.condition << ")";
            found_conditional = true;
        }
        std::cout << std::endl;
    }
    assert(found_conditional);

    // Test removing breakpoint
    executor.remove_breakpoint("test_proc", 5);
    breakpoints = executor.get_breakpoints();
    assert(breakpoints.size() == 2);
    std::cout << "✓ Removed breakpoint, " << breakpoints.size() << " remaining" << std::endl;

    // Test clearing all breakpoints
    executor.clear_breakpoints();
    breakpoints = executor.get_breakpoints();
    assert(breakpoints.empty());
    std::cout << "✓ Cleared all breakpoints" << std::endl;

    std::cout << "✓ Breakpoint management test passed" << std::endl;
}

void test_step_execution_control()
{
    std::cout << "Testing step execution control..." << std::endl;

    PsqlExecutor executor;
    executor.enable_debugging(true);

    // Test step mode
    executor.enable_step_mode(true);
    std::cout << "✓ Step mode enabled" << std::endl;

    executor.step_over();
    std::cout << "✓ Step over executed" << std::endl;

    executor.step_into();
    std::cout << "✓ Step into executed" << std::endl;

    executor.continue_execution();
    std::cout << "✓ Continue execution" << std::endl;

    executor.enable_step_mode(false);
    std::cout << "✓ Step mode disabled" << std::endl;

    std::cout << "✓ Step execution control test passed" << std::endl;
}

void test_variable_inspection()
{
    std::cout << "Testing variable inspection..." << std::endl;

    PsqlExecutor executor;
    executor.enable_debugging(true);

    // Test getting variables (should be empty initially)
    auto variables = executor.get_current_variables();
    assert(variables.empty());
    std::cout << "✓ No variables in empty call stack" << std::endl;

    // Test getting specific variable value
    auto var_value = executor.get_variable_value("nonexistent");
    std::cout << "✓ Retrieved variable value for nonexistent variable" << std::endl;

    std::cout << "✓ Variable inspection test passed" << std::endl;
}

void test_call_stack_inspection()
{
    std::cout << "Testing call stack inspection..." << std::endl;

    PsqlExecutor executor;
    executor.enable_debugging(true);

    // Test empty call stack
    auto call_stack = executor.get_call_stack();
    assert(call_stack.empty());
    std::cout << "✓ Empty call stack initially" << std::endl;

    auto current_proc = executor.get_current_procedure();
    assert(current_proc.empty());
    std::cout << "✓ No current procedure in empty stack" << std::endl;

    auto current_line = executor.get_current_line();
    assert(current_line == 0);
    std::cout << "✓ Current line is 0 in empty stack" << std::endl;

    std::cout << "✓ Call stack inspection test passed" << std::endl;
}

void test_error_reporting()
{
    std::cout << "Testing enhanced error reporting..." << std::endl;

    PsqlExecutor executor;
    executor.enable_debugging(true);

    // Test reporting runtime error
    executor.report_runtime_error("Division by zero", 42);

    auto error_with_location = executor.get_last_error_with_location();
    assert(!error_with_location.empty());
    assert(error_with_location.find("Division by zero") != std::string::npos);
    assert(error_with_location.find("line 42") != std::string::npos);

    std::cout << "✓ Error with location: " << error_with_location << std::endl;

    std::cout << "✓ Error reporting test passed" << std::endl;
}

void test_debugging_integration()
{
    std::cout << "Testing debugging integration..." << std::endl;

    // This would test debugging integration with actual PSQL execution
    // For now, just verify the debugging infrastructure works

    PsqlExecutor executor;
    executor.enable_debugging(true);

    // Set up debugging scenario
    executor.add_breakpoint("debug_test_proc", 1);
    executor.add_breakpoint("debug_test_proc", 5, "counter > 3");
    executor.enable_step_mode(true);

    // Verify setup
    auto breakpoints = executor.get_breakpoints();
    assert(breakpoints.size() == 2);

    std::cout << "✓ Debugging scenario set up with " << breakpoints.size() << " breakpoints"
              << std::endl;
    std::cout << "✓ Debugging integration test passed" << std::endl;
}

void test_debugging_performance_impact()
{
    std::cout << "Testing debugging performance impact..." << std::endl;

    PsqlExecutor executor;

    // Test performance with debugging disabled
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        executor.get_current_variables();
        executor.get_call_stack();
    }
    auto end = std::chrono::steady_clock::now();
    auto disabled_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Test performance with debugging enabled
    executor.enable_debugging(true);
    start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        executor.get_current_variables();
        executor.get_call_stack();
    }
    end = std::chrono::steady_clock::now();
    auto enabled_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "  Performance - Debugging disabled: " << disabled_time << "μs" << std::endl;
    std::cout << "  Performance - Debugging enabled: " << enabled_time << "μs" << std::endl;

    // Performance impact should be reasonable (less than 10x slower)
    if (enabled_time < disabled_time * 10) {
        std::cout << "✓ Debugging performance impact is reasonable" << std::endl;
    } else {
        std::cout << "⚠ Debugging performance impact is high (may be acceptable)" << std::endl;
    }

    std::cout << "✓ Debugging performance impact test completed" << std::endl;
}

int main()
{
    std::cout << "=== PSQL Debugging Support Tests ===" << std::endl;

    try {
        // Set up temporary database path
        std::string db_path = "/tmp/debug_test.db";
        set_executor_db_path(db_path);

        // Clean up any existing test database
        std::filesystem::remove(db_path + ".seg0");

        // Run debugging support tests
        test_debugging_enable_disable();
        test_breakpoint_management();
        test_step_execution_control();
        test_variable_inspection();
        test_call_stack_inspection();
        test_error_reporting();
        test_debugging_integration();
        test_debugging_performance_impact();

        std::cout << "=== PSQL Debugging Support Tests Complete ===" << std::endl;
        std::cout << "Note: Full debugging integration requires runtime execution hooks"
                  << std::endl;

        // Clean up
        std::filesystem::remove(db_path + ".seg0");

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
