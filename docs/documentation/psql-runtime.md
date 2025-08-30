### PSQL Runtime

Parsing: `src/engine/parser_psql.cpp`. Runtime: `src/engine/psql_executor.cpp`.

Features:
- EXECUTE BLOCK with optional `(params)` and `RETURNS(...)`, body `AS BEGIN ... END`
- Statement kinds captured: IF, FOR SELECT ... INTO vars, DECLARE (variables or cursors), EXCEPTION/WHEN [DO], SUSPEND, RETURN, EXECUTE STATEMENT (WITH CALLER PRIVILEGES, AS USER/PASSWORD/ROLE, ON EXTERNAL, WITH BIND, TIMEOUT, INTO), WHILE, LEAVE/EXIT, CONTINUE, OPEN/FETCH/CLOSE cursor
- Scope and variables: declaration, assignment with type validation/coercion; default values
- Cursors: declare/open/fetch/close; scrollable cursors with bulk limits
- Exceptions: system exception map (e.g., ZERO_DIVIDE, NUMERIC_OVERFLOW), set/propagate
- Security context: DEFINER/INVOKER switching; caller privileges
- Parameters: IN/OUT/INOUT binding and retrieval of output parameters
- Utilities: dependency analyzer, code formatter, performance profiler, syntax validator

Examples:
```sql
EXECUTE BLOCK (input_val INTEGER = 10) RETURNS (output_val INTEGER) AS
BEGIN
  output_val = input_val * 2;
END;
```

Code anchors: `src/engine/parser_psql.cpp`, `src/engine/psql_executor.cpp`

