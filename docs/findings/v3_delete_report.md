# DELETE - V3 Findings

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/DELETE.md`

Status: **Partially implemented**. DELETE parsing and emission exist, but opcode name differs from spec and there is no explicit error mapping for required error codes.

## Checklist

### Parsing
- [*] Parses `DELETE FROM <table_ref>` with optional alias.
  - Evidence: `src/parser/parser_v3.cpp:7089-7112`.
- [*] Parses `USING <table_ref_list>` with joins.
  - Evidence: `src/parser/parser_v3.cpp:7114-7130`.
- [*] Parses WHERE clause.
  - Evidence: `src/parser/parser_v3.cpp:7132-7136`.
- [*] Parses RETURNING clause.
  - Evidence: `src/parser/parser_v3.cpp:7138-7141`.

### Emission
- [~] Emits opcode `SBLR3_DELETE` with payload containing target, alias, using, joins, where, returning.
  - Evidence: `src/parser/v3_emitter.cpp:538-567`.
- [ ] Spec requires `SBLR3_DML_DELETE` with typed payload `DML_DELETE` and child nodes; opcode name mismatch.
  - Evidence: no `SBLR3_DML_DELETE` in code (`src/sblr/v3_opcodes.generated.cpp`).

### Errors
- [ ] No explicit `ERR_PARSE_EXPECTED_TABLE` mapping for missing target table.
- [ ] No `ERR_FEATURE_NOT_SUPPORTED` for RETURNING on unsupported dialects.
- [ ] No `ERR_PERMISSION_DENIED` mapping for system table delete in parser.

