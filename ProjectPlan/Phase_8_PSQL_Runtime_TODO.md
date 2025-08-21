# Phase 8 — PSQL Runtime: Detailed Implementation TODO

**Status**: Not Started
**Priority**: High (Core SQL Programming Language Support)
**Estimated Effort**: 8-12 weeks
**Dependencies**: Phases 1-7 (Complete), Parser foundation

---

## Overview and Goals

Implement a complete PSQL (Procedural SQL) runtime engine supporting EXECUTE BLOCK, stored procedures, functions, variables, control flow, exceptions, cursors, and security contexts. This phase transforms ScratchBird from a relational database into a full application development platform.

### Exit Criteria
- ✅ EXECUTE BLOCK statements execute correctly with variables and control flow
- ✅ CREATE PROCEDURE/FUNCTION with full parameter support
- ✅ Exception handling with custom and system exceptions
- ✅ Cursor operations (OPEN, FETCH, CLOSE) working
- ✅ SECURITY DEFINER/INVOKER semantics properly enforced
- ✅ Deterministic/Nondeterministic flags affect caching and optimization
- ✅ PSQL test suites pass including packages and exception scenarios
- ✅ Performance benchmarks show acceptable execution overhead

---

## Phase 8.1: PSQL Language Infrastructure

### 8.1.1 PSQL Parser Extensions
- [ ] **EXECUTE BLOCK syntax parsing**
  - [ ] Parameter declarations (IN, OUT, INOUT)
  - [ ] Variable declarations with types and defaults
  - [ ] BEGIN/END block structure
  - [ ] Exception handling blocks (WHEN clauses)

- [ ] **Procedure/Function parsing**
  - [ ] CREATE/ALTER/DROP PROCEDURE syntax
  - [ ] CREATE/ALTER/DROP FUNCTION syntax
  - [ ] Parameter lists with optional defaults
  - [ ] RETURNS clause for functions
  - [ ] SECURITY DEFINER/INVOKER clauses
  - [ ] DETERMINISTIC/NOT DETERMINISTIC flags

- [ ] **PSQL statement parsing**
  - [ ] Variable assignment (:var = expr)
  - [ ] IF/THEN/ELSE/END IF control flow
  - [ ] WHILE/DO loops with BREAK/CONTINUE
  - [ ] FOR loops (integer ranges and SELECT cursors)
  - [ ] EXCEPTION/WHEN exception handling
  - [ ] RAISE EXCEPTION statements

### 8.1.2 PSQL AST Nodes
- [ ] **Control flow nodes**
  - [ ] `ExecBlockNode` for EXECUTE BLOCK
  - [ ] `IfStmtNode` for conditional execution
  - [ ] `WhileLoopNode` for iteration
  - [ ] `ForLoopNode` for counted/cursor loops
  - [ ] `BreakNode` and `ContinueNode` for loop control

- [ ] **Variable and assignment nodes**
  - [ ] `VarDeclNode` for variable declarations
  - [ ] `AssignmentNode` for variable assignments
  - [ ] `ParameterNode` for procedure/function parameters

- [ ] **Exception handling nodes**
  - [ ] `ExceptionHandlerNode` for WHEN clauses
  - [ ] `RaiseExceptionNode` for RAISE statements
  - [ ] `TryBlockNode` for exception scoping

### 8.1.3 PSQL Type System Integration
- [ ] **Variable type management**
  - [ ] Type inference from expressions
  - [ ] Type coercion rules for assignments
  - [ ] NULL handling in variable context
  - [ ] Default value evaluation

- [ ] **Parameter type validation**
  - [ ] IN/OUT/INOUT parameter semantics
  - [ ] Type checking for procedure/function calls
  - [ ] Optional parameter default handling
  - [ ] Array and complex type parameters

---

## Phase 8.2: PSQL Runtime Engine

### 8.2.1 Execution Context Management
- [ ] **PSQL execution context**
  - [ ] Variable storage and scoping
  - [ ] Parameter binding for procedure calls
  - [ ] Exception stack management
  - [ ] Security context (DEFINER vs INVOKER)

- [ ] **Call stack implementation**
  - [ ] Procedure/function call frames
  - [ ] Recursion depth limiting
  - [ ] Return value handling
  - [ ] Cleanup on exception unwinding

### 8.2.2 Variable Management
- [ ] **Variable storage**
  - [ ] Typed variable slots in execution context
  - [ ] Variable lifetime management (block scoping)
  - [ ] Variable initialization and cleanup
  - [ ] Memory management for variable values

- [ ] **Variable operations**
  - [ ] Variable assignment with type checking
  - [ ] Variable reference resolution
  - [ ] Variable persistence across procedure calls
  - [ ] Variable debugging/inspection support

### 8.2.3 Control Flow Execution
- [ ] **Conditional execution**
  - [ ] IF/THEN/ELSE evaluation with boolean expressions
  - [ ] Nested conditional support
  - [ ] Short-circuit evaluation for performance

- [ ] **Loop execution**
  - [ ] WHILE loop with condition evaluation
  - [ ] FOR loop with counter variables
  - [ ] Cursor-based FOR loops
  - [ ] BREAK/CONTINUE statement handling
  - [ ] Loop nesting and scope management

---

## Phase 8.3: Procedure and Function Support

### 8.3.1 Stored Procedure Implementation
- [ ] **Procedure lifecycle**
  - [ ] CREATE PROCEDURE catalog integration
  - [ ] Procedure compilation and caching
  - [ ] Procedure parameter binding
  - [ ] Procedure execution and cleanup

- [ ] **Procedure features**
  - [ ] IN/OUT/INOUT parameter handling
  - [ ] Local variable support
  - [ ] Nested procedure calls
  - [ ] Transaction context inheritance

### 8.3.2 User-Defined Functions
- [ ] **Function implementation**
  - [ ] Function compilation and optimization
  - [ ] Return value handling and type checking
  - [ ] Function result caching (DETERMINISTIC)
  - [ ] Function inlining for simple cases

- [ ] **Function integration**
  - [ ] Function calls in SQL expressions
  - [ ] Function calls in WHERE clauses
  - [ ] Recursive function support with depth limits
  - [ ] Function dependency tracking

### 8.3.3 Package Support (Basic)
- [ ] **Package structure**
  - [ ] Package header declarations
  - [ ] Package body implementations
  - [ ] Public/private procedure/function visibility
  - [ ] Package initialization blocks

---

## Phase 8.4: Exception Handling

### 8.4.1 Exception Infrastructure
- [ ] **Exception types**
  - [ ] System exception definitions
  - [ ] User-defined exception support
  - [ ] Exception hierarchy and inheritance
  - [ ] Exception message formatting

- [ ] **Exception context**
  - [ ] Exception stack traces
  - [ ] Exception variable binding
  - [ ] Exception propagation rules
  - [ ] Exception logging and monitoring

### 8.4.2 Exception Operations
- [ ] **RAISE statement**
  - [ ] RAISE with system exceptions
  - [ ] RAISE with custom exceptions
  - [ ] RAISE with formatted messages
  - [ ] RAISE for re-throwing exceptions

- [ ] **Exception handling**
  - [ ] WHEN exception_type clauses
  - [ ] WHEN OTHERS catch-all handling
  - [ ] Exception variable access
  - [ ] Nested exception handling

---

## Phase 8.5: Cursor Support

### 8.5.1 Cursor Infrastructure
- [ ] **Cursor types**
  - [ ] Explicit cursor declarations
  - [ ] Implicit cursors for FOR loops
  - [ ] Parameterized cursors
  - [ ] Scrollable cursor support (basic)

- [ ] **Cursor lifecycle**
  - [ ] Cursor compilation and optimization
  - [ ] Cursor parameter binding
  - [ ] Cursor result set management
  - [ ] Cursor cleanup and resource management

### 8.5.2 Cursor Operations
- [ ] **Basic cursor operations**
  - [ ] OPEN cursor with parameter binding
  - [ ] FETCH cursor into variables
  - [ ] CLOSE cursor with cleanup
  - [ ] Cursor attribute testing (%FOUND, %NOTFOUND, %ROWCOUNT)

- [ ] **Advanced cursor features**
  - [ ] Cursor FOR loops (FOR rec IN cursor)
  - [ ] Cursor expressions (SELECT ... INTO)
  - [ ] Bulk operations (FETCH ... BULK COLLECT)
  - [ ] Cursor exception handling

---

## Phase 8.6: Security and Performance

### 8.6.1 Security Context Management
- [ ] **SECURITY DEFINER/INVOKER**
  - [ ] Security context switching
  - [ ] Permission checking for definer rights
  - [ ] Role inheritance in security context
  - [ ] Security context debugging and auditing

- [ ] **Access control**
  - [ ] Procedure/function permission checking
  - [ ] Variable access security
  - [ ] Database object access within procedures
  - [ ] Cross-schema procedure calls

### 8.6.2 Performance Optimization
- [ ] **Compilation optimization**
  - [ ] PSQL bytecode generation
  - [ ] Expression optimization within PSQL
  - [ ] Dead code elimination
  - [ ] Constant folding in PSQL context

- [ ] **Runtime optimization**
  - [ ] Procedure plan caching
  - [ ] Variable access optimization
  - [ ] Loop optimization
  - [ ] Function inlining for DETERMINISTIC functions

### 8.6.3 Deterministic/Nondeterministic Handling
- [ ] **Function categorization**
  - [ ] DETERMINISTIC function marking and enforcement
  - [ ] Nondeterministic function detection
  - [ ] Side-effect analysis for functions
  - [ ] Caching policies based on determinism

- [ ] **Optimization integration**
  - [ ] Query optimizer integration with function determinism
  - [ ] Function result caching
  - [ ] Function call elimination in appropriate contexts

---

## Phase 8.7: Catalog Integration

### 8.7.1 PSQL Metadata Storage
- [ ] **Catalog extensions**
  - [ ] SDB$PROCEDURE table for procedure metadata
  - [ ] SDB$FUNCTION table for function metadata
  - [ ] SDB$PACKAGE table for package metadata
  - [ ] SDB$PARAMETER table for parameter definitions

- [ ] **Source code storage**
  - [ ] PSQL source code preservation in catalog
  - [ ] Compiled bytecode storage
  - [ ] Dependency tracking between procedures
  - [ ] Version management for procedure changes

### 8.7.2 DDL Integration
- [ ] **Procedure DDL**
  - [ ] CREATE PROCEDURE implementation
  - [ ] ALTER PROCEDURE implementation
  - [ ] DROP PROCEDURE with dependency checking
  - [ ] GRANT/REVOKE EXECUTE permissions

- [ ] **Function DDL**
  - [ ] CREATE FUNCTION implementation
  - [ ] ALTER FUNCTION implementation
  - [ ] DROP FUNCTION with usage checking
  - [ ] Function overloading support (basic)

---

## Phase 8.8: Testing and Validation

### 8.8.1 Unit Tests
- [ ] **PSQL language features**
  - [ ] Variable declaration and assignment tests
  - [ ] Control flow (IF/WHILE/FOR) tests
  - [ ] Exception handling tests
  - [ ] Cursor operation tests

- [ ] **Procedure/Function tests**
  - [ ] Parameter passing tests (IN/OUT/INOUT)
  - [ ] Return value tests
  - [ ] Recursive procedure tests
  - [ ] Security context tests

### 8.8.2 Integration Tests
- [ ] **Complex PSQL scenarios**
  - [ ] Multi-level procedure calls
  - [ ] Exception propagation across procedures
  - [ ] Cursor operations within procedures
  - [ ] Transaction management in procedures

- [ ] **Performance tests**
  - [ ] PSQL execution overhead benchmarks
  - [ ] Large procedure compilation tests
  - [ ] Memory usage under PSQL load
  - [ ] Concurrent procedure execution tests

### 8.8.3 Compatibility Tests
- [ ] **Firebird compatibility**
  - [ ] PSQL syntax compatibility tests
  - [ ] Exception behavior compatibility
  - [ ] Built-in function compatibility
  - [ ] Security model compatibility

---

## Phase 8.9: Tools and Debugging

### 8.9.1 PSQL Debugging Support
- [ ] **Debugger infrastructure**
  - [ ] Breakpoint support in procedures
  - [ ] Variable inspection during execution
  - [ ] Call stack visualization
  - [ ] Step-through execution

- [ ] **Error reporting**
  - [ ] Enhanced error messages with line numbers
  - [ ] Stack trace generation for exceptions
  - [ ] Variable state dumping on errors
  - [ ] Performance profiling integration

### 8.9.2 Development Tools
- [ ] **PSQL utilities**
  - [ ] Procedure dependency analyzer
  - [ ] PSQL code formatter
  - [ ] Performance profiler for procedures
  - [ ] PSQL syntax validator

---

## Implementation Priority

### **Critical Path (Weeks 1-4)**
1. PSQL parser extensions for basic syntax
2. Execution context and variable management
3. Basic control flow (IF/WHILE) implementation
4. Simple procedure CREATE/EXECUTE support

### **Core Features (Weeks 5-8)**
1. Exception handling infrastructure
2. Cursor support implementation
3. Function support with return values
4. Security context management

### **Advanced Features (Weeks 9-12)**
1. Package support framework
2. Performance optimization
3. Comprehensive testing
4. Debugging and tooling support

---

## Success Metrics

- [ ] **Functionality**: All basic PSQL constructs working
- [ ] **Performance**: < 20% overhead vs. direct SQL execution
- [ ] **Compatibility**: 90%+ Firebird PSQL compatibility
- [ ] **Reliability**: Exception handling and cleanup working correctly
- [ ] **Security**: DEFINER/INVOKER semantics properly enforced
- [ ] **Testability**: Comprehensive test coverage for all features

This phase represents a major milestone in ScratchBird development, adding full procedural programming capabilities to the database system.
