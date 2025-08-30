### PSQL runtime

Runtime for PSQL: execute block, procedures/functions, variables, control flow, exceptions, cursors, security context, packages, dev tools, and debugging.

## Execute block
- Executes `EXECUTE BLOCK` with parameter binding, declarations, and sequential statement execution; returns rows for functions or execution status for procedures.
- Implementation references:
  - `scratchbird::engine::PsqlExecutor::execute_statement`
  - `scratchbird::engine::PsqlExecutor::execute_declare`
  - `scratchbird::engine::PsqlExecutor::execute_sql_statement`
  - `scratchbird::engine::PsqlTypeManager::parse_type`
  - `scratchbird::engine::parse_psql_block`

## Variables and typing
- Scoped variable storage with type parsing, default initialization, assignment with validation/coercion, and parameter IN/OUT/INOUT support.
- Implementation references:
  - `scratchbird::engine::PsqlExecutionContext::declare_variable`
  - `scratchbird::engine::PsqlExecutionContext::assign_variable`
  - `scratchbird::engine::PsqlExecutionContext::get_variable_value`
  - `scratchbird::engine::PsqlTypeManager::validate_assignment`
  - `scratchbird::engine::PsqlTypeManager::coerce_value`

## Control flow
- IF/THEN/ELSE branching, WHILE loops with break/continue, basic FOR loop placeholder and cursor FOR loop support.
- Implementation references:
  - `scratchbird::engine::PsqlExecutor::execute_if_statement`
  - `scratchbird::engine::PsqlExecutor::execute_while_loop`
  - `scratchbird::engine::PsqlExecutor::execute_for_loop`
  - `scratchbird::engine::PsqlExecutor::execute_leave_statement`
  - `scratchbird::engine::PsqlExecutor::execute_continue_statement`
  - `scratchbird::engine::PsqlExecutionContext::ControlFlowState`

## Exceptions
- RAISE statements, system and user exceptions, WHEN handlers with condition matching, propagation, and cleanup.
- Implementation references:
  - `scratchbird::engine::PsqlExecutor::execute_raise_statement`
  - `scratchbird::engine::PsqlExecutor::execute_exception_handler`
  - `scratchbird::engine::PsqlExecutionContext::set_exception`
  - `scratchbird::engine::PsqlExecutionContext::clear_exception`
  - `scratchbird::engine::PsqlExecutionContext::get_system_exceptions`

## Cursors
- DECLARE/OPEN/FETCH/CLOSE lifecycle, scrollable navigation (NEXT/PRIOR/FIRST/LAST/ABSOLUTE/RELATIVE), CURSOR FOR loops, bulk operations infra.
- Implementation references:
  - `scratchbird::engine::PsqlExecutor::execute_declare_cursor`
  - `scratchbird::engine::PsqlExecutor::execute_open_cursor`
  - `scratchbird::engine::PsqlExecutor::execute_fetch_cursor`
  - `scratchbird::engine::PsqlExecutor::execute_close_cursor`
  - `scratchbird::engine::PsqlExecutor::fetch_cursor_direction`
  - `scratchbird::engine::PsqlExecutor::fetch_cursor_absolute`
  - `scratchbird::engine::PsqlExecutor::fetch_cursor_relative`
  - `scratchbird::engine::PsqlExecutor::execute_cursor_for_loop`

## Security context
- SECURITY DEFINER/INVOKER execution with context switch, inheritance, and restoration.
- Implementation references:
  - `scratchbird::engine::PsqlExecutionContext::set_security_context`
  - `scratchbird::engine::PsqlExecutionContext::restore_security_context`
  - `scratchbird::engine::PsqlExecutionContext::get_current_security_context`
  - `scratchbird::engine::PsqlExecutionContext::has_definer_rights`

## Packages
- Package spec/body infrastructure with visibility, initialization blocks, state, and execution of packaged routines.
- Implementation references:
  - `scratchbird::engine::PsqlExecutor::compile_package_specification`
  - `scratchbird::engine::PsqlExecutor::compile_package_body`
  - `scratchbird::engine::PsqlExecutor::initialize_package`
  - `scratchbird::engine::PsqlExecutor::execute_package_procedure`
  - `scratchbird::engine::PsqlExecutor::execute_package_function`
  - `scratchbird::engine::PsqlExecutor::get_package_variable`
  - `scratchbird::engine::PsqlExecutor::set_package_variable`

## Development tools
- Dependency analyzer, formatter, profiler, syntax validator, and IDE helper for definitions/references/completions.
- Implementation references:
  - `scratchbird::engine::PsqlDevEnvironment::analyze_code`
  - `scratchbird::engine::PsqlDevEnvironment::get_code_completion`
  - `scratchbird::engine::PsqlDevEnvironment::find_definition`
  - `scratchbird::engine::PsqlDevEnvironment::find_references`
  - `scratchbird::engine::PsqlCodeFormatter::format_code`
  - `scratchbird::engine::PsqlPerformanceProfiler::generate_report`
  - `scratchbird::engine::PsqlSyntaxValidator::validate_syntax`

## Debugging
- Breakpoints (including conditional), step controls, variable and call stack inspection, error reporting with locations.
- Implementation references:
  - `scratchbird::engine::PsqlExecutor::enable_debugging`
  - `scratchbird::engine::PsqlExecutor::add_breakpoint`
  - `scratchbird::engine::PsqlExecutor::get_breakpoints`
  - `scratchbird::engine::PsqlExecutor::enable_step_mode`
  - `scratchbird::engine::PsqlExecutor::step_over`
  - `scratchbird::engine::PsqlExecutor::step_into`
  - `scratchbird::engine::PsqlExecutor::continue_execution`
  - `scratchbird::engine::PsqlExecutor::get_current_variables`
  - `scratchbird::engine::PsqlExecutor::get_call_stack`
  - `scratchbird::engine::PsqlExecutor::get_last_error_with_location`

## Parser and AST
- PSQL parsing entry points and AST nodes for blocks, routines, calls, packages, triggers.
- Implementation references:
  - `scratchbird::engine::parse_psql_block`
  - `scratchbird::engine::parse_psql_routine`
  - `scratchbird::engine::parse_psql_call`
  - `scratchbird::engine::parse_psql_package`
  - `scratchbird::engine::parse_psql_execstmt`
  - `scratchbird::engine::parse_psql_trigger`

## Implementation References
- `ScratchBird/include/scratchbird/engine/psql_executor.h`
- `ScratchBird/src/engine/psql_executor.cpp`
- `ScratchBird/include/scratchbird/engine/psql_dev_tools.h`
- `ScratchBird/src/engine/psql_dev_tools.cpp`
- `ScratchBird/include/scratchbird/engine/parser_psql.h`
- `ScratchBird/include/scratchbird/engine/ast.h`

## Spec Trace
- [REQ-PSQL-RUNTIME-EXECUTE-BLOCK](../../traceability/spec/requirements.md#req-psql-runtime-execute-block)
- [REQ-PSQL-RUNTIME-VARS](../../traceability/spec/requirements.md#req-psql-runtime-vars)
- [REQ-PSQL-RUNTIME-CONTROL-FLOW](../../traceability/spec/requirements.md#req-psql-runtime-control-flow)
- [REQ-PSQL-RUNTIME-PROCEDURE](../../traceability/spec/requirements.md#req-psql-runtime-procedure)
- [REQ-PSQL-RUNTIME-FUNCTION](../../traceability/spec/requirements.md#req-psql-runtime-function)
- [REQ-PSQL-RUNTIME-EXCEPTION](../../traceability/spec/requirements.md#req-psql-runtime-exception)
- [REQ-PSQL-RUNTIME-CURSOR](../../traceability/spec/requirements.md#req-psql-runtime-cursor)
- [REQ-PSQL-RUNTIME-SECURITY-CONTEXT](../../traceability/spec/requirements.md#req-psql-runtime-security-context)
- [REQ-PSQL-RUNTIME-PACKAGES](../../traceability/spec/requirements.md#req-psql-runtime-packages)
- [REQ-PSQL-RUNTIME-DEV-TOOLS](../../traceability/spec/requirements.md#req-psql-runtime-dev-tools)
- [REQ-PSQL-RUNTIME-DEBUG](../../traceability/spec/requirements.md#req-psql-runtime-debug)
