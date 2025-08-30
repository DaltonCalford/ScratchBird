### PSQL Routines, Packages, and Triggers

Routines:
- CREATE/ALTER/RECREATE PROCEDURE/FUNCTION parsed into `ast.psqlRoutine` (kind, name, params, returns, param modes, attributes, body)
- CALL and EXECUTE PROCEDURE supported in DML parser and PSQL

Packages:
- Package header/body captured in `ast.psqlPackage` (public/private procedures/functions, bodies)

Triggers:
- CREATE/ALTER/RECREATE TRIGGER captured in `ast.psqlTrigger` (name, table, timing BEFORE/AFTER, events, FOR EACH ROW/STATEMENT, ACTIVE/INACTIVE, position, body)

Examples:
```sql
CREATE FUNCTION add2(IN x INT, IN y INT) RETURNS INT AS
BEGIN
  RETURN x + y;
END;

CREATE TRIGGER tbi BEFORE INSERT ON t AS BEGIN /* body */ END;
```

Code anchors: `src/engine/parser.cpp` (routing), `src/engine/parser_psql.cpp` (structure)

