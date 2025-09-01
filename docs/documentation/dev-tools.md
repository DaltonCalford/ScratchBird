### Development Tools

**What it is**

ScratchBird provides a comprehensive set of development tools for debugging, profiling, testing, and analyzing database applications. These tools help developers understand query behavior, optimize performance, debug stored procedures, and ensure code quality. The toolkit includes interactive debuggers, profilers, test frameworks, and code analysis utilities.

**Why it matters**

- **Debugging**: Step through stored procedures and identify issues
- **Performance**: Profile queries and identify bottlenecks  
- **Quality**: Automated testing ensures reliability
- **Productivity**: Integrated tools speed up development
- **Maintenance**: Analysis tools help maintain code quality

**How to use it**

Use the interactive debugger to step through PSQL code, set breakpoints, and inspect variables. Profile queries to understand resource usage and optimization opportunities. Write unit tests for database objects and run them automatically. Use static analysis to identify potential issues before deployment.

## Bytecode Tools

**Source**: `src/engine/sblr_tools.cpp`

- **Disassembler**: Convert SBLR bytecode back to readable assembly format for debugging
- **Bytecode Inspector**: Analyze bytecode modules for optimization opportunities, hot paths, and type patterns
- **Profile Analyzer**: Process runtime profiling data to identify JIT compilation candidates
- **Bytecode Verifier**: Validate bytecode integrity, type safety, and stack balance
- **Optimization Reporter**: Generate reports on applied optimizations and their effectiveness

These utilities are meant for developer workflows and PSQL code hygiene.

## PSQL Debugger

### Enabling Debug Mode

```sql
-- Enable debugging for session
SET debug_mode = on;
SET debug_output = 'verbose';

-- Enable for specific routine
ALTER FUNCTION calculate_discount DEBUG ENABLED;
ALTER PROCEDURE process_order DEBUG ENABLED;
```

### Interactive Debugging

```sql
-- Start debug session
DEBUG PROCEDURE process_payment(order_id => 12345);

-- Debugger commands:
-- STEP    - Execute next statement
-- NEXT    - Step over function calls
-- CONT    - Continue execution
-- BREAK   - Set breakpoint
-- WATCH   - Watch variable
-- PRINT   - Print variable value
-- STACK   - Show call stack
```

## Query Profiler

### Basic Profiling

```sql
-- Enable profiling
SET profiling = on;

-- Profile query
PROFILE SELECT * FROM large_table WHERE status = 'active';

-- View results
SHOW PROFILE;
```

## Code Analysis

### Dependency Analysis

```sql
-- Find dependencies
SELECT * FROM find_dependencies('function', 'calculate_total');

-- Dependency tree
SELECT * FROM dependency_tree('table', 'orders');
```

### Static Analysis

```sql
-- Analyze function
ANALYZE FUNCTION calculate_tax;

-- Analyze schema
ANALYZE SCHEMA public;
```

## Test Framework

### Unit Testing

```sql
-- Create test
CREATE TEST CASE test_calculate_discount
AS
BEGIN
    ASSERT calculate_discount(1, 100.00) = 90.00,
           'Gold customer should get 10% discount';
END;

-- Run test
RUN TEST test_calculate_discount;
```

## Implementation Details

**PSQL Dev Tools** (`src/engine/psql_dev_tools.cpp`):
- Dependency analyzer: Extracts function calls, table references
- Code formatter: Normalizes whitespace, keywords, indentation
- Performance profiler: Records execution times and counts
- Syntax validator: Parses code and reports issues

**Code Anchors**:
- Dev tools: `src/engine/psql_dev_tools.cpp`
- Debugger: `src/engine/debugger.cpp`
- Profiler: `src/engine/profiler.cpp`
- Test framework: `src/engine/test_framework.cpp`

## See also

- [PSQL Runtime](./psql-runtime.md) - Procedural SQL features
- [Routines & Triggers](./psql-routines-and-triggers.md) - Stored procedures
- [EXPLAIN ANALYZE](./explain-analyze.md) - Query analysis
- [Configuration](./configuration.md) - Debug settings
- [CLI Tools](./cli-tools.md) - Command-line utilities

