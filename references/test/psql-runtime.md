### PSQL Runtime

What it is
- The procedural engine: variables, cursors, control flow, dynamic SQL, exceptions, and security context.

Why it matters
- Encapsulates business logic close to data; enables robust error handling and controlled privileges.

How to use it
- Start with EXECUTE BLOCK for quick scripts; evolve into PROCEDUREs/FUNCTIONs; use cursors for multi-row logic and EXECUTE STATEMENT for dynamic needs.

Parsing: `src/engine/parser_psql.cpp`. Runtime: `src/engine/psql_executor.cpp`. Bytecode: See [Complete SBLR/BLR Specification](/workspace/docs/scratchbird-bytecode-complete-specification.md).

Features:
- EXECUTE BLOCK with optional `(params)` and `RETURNS(...)`, body `AS BEGIN ... END`
- Statement kinds captured: IF, FOR SELECT ... INTO vars, DECLARE (variables or cursors), EXCEPTION/WHEN [DO], SUSPEND, RETURN, EXECUTE STATEMENT (WITH CALLER PRIVILEGES, AS USER/PASSWORD/ROLE, ON EXTERNAL, WITH BIND, TIMEOUT, INTO), WHILE, LEAVE/EXIT, CONTINUE, OPEN/FETCH/CLOSE cursor
- Scope and variables: declaration, assignment with type validation/coercion; default values
- Cursors: declare/open/fetch/close; scrollable cursors with bulk limits
- Exceptions: system exception map (e.g., ZERO_DIVIDE, NUMERIC_OVERFLOW), set/propagate
- Security context: DEFINER/INVOKER switching; caller privileges
- Parameters: IN/OUT/INOUT binding and retrieval of output parameters
- Utilities: dependency analyzer, code formatter, performance profiler, syntax validator
- **Bytecode compilation**: PSQL procedures are compiled to SBLR bytecode for efficient execution with adaptive optimization and optional JIT compilation for hot code paths

Examples:
```sql
EXECUTE BLOCK (input_val INTEGER = 10) RETURNS (output_val INTEGER) AS
BEGIN
  output_val = input_val * 2;
END;
```

Code anchors: `src/engine/parser_psql.cpp`, `src/engine/psql_executor.cpp`, `src/engine/sblr_compiler.cpp` (bytecode generation), `src/engine/sblr_vm.cpp` (bytecode execution)

See also
- [Routines & triggers](./psql-routines-and-triggers.md) · [DML](./sql-dml.md)

