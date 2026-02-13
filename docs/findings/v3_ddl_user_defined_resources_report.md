# V3 User-Defined Resources Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_USER_DEFINED_RESOURCES.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- V3 parser/emitter/executor do **not** implement `CREATE LIBRARY`, `DROP LIBRARY`, or `CREATE FUNCTION ... AS EXTERNAL NAME ...` syntax.
- There is a separate `CREATE UDR`/`DROP UDR` path in V3, but it does not match this spec’s library binding semantics.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### CREATE LIBRARY / DROP LIBRARY
[ ] No parser support for `CREATE LIBRARY` or `DROP LIBRARY`.
[ ] No AST structures or V3 opcodes for library registration.

### CREATE FUNCTION (External)
[ ] No parsing of `AS EXTERNAL NAME '<lib>:<symbol>'` or `LANGUAGE <lang>` in V3 CREATE FUNCTION.
[ ] No emitter/executor support for binding external functions to libraries.

### Alternatives
[~] V3 has `CREATE UDR`/`DROP UDR` statements, but they represent a different UDR plugin system and do not map to library registration in this spec.

## Key References
- No matches for `CREATE LIBRARY`/`DROP LIBRARY` in V3 parser.
- `CREATE UDR` exists: `src/parser/parser_v3.cpp:3664-3705`, `include/scratchbird/parser/ast_v3.h:1154-1173`.
