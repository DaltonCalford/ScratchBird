# FirebirdSQL Emulation SQL Surface

Parser: Firebird emulation parser (AST v2 -> V2 pipeline).

Sources:
- Parser: `ScratchBird/src/parser/firebird/firebird_parser.cpp`
- Lexer: `ScratchBird/src/parser/firebird/firebird_lexer.cpp`
- V2 pipeline: `ScratchBird/src/sblr/semantic_analyzer_v2.cpp`, `ScratchBird/src/sblr/bytecode_generator_v2.cpp`
- Executor: `ScratchBird/src/sblr/executor.cpp`
- Audit refs: `ScratchBird/docs/audit/16_firebird_parser_statement_reference_actual.md`,
  `ScratchBird/docs/audit/22_firebird_parser_correction_plan_checklist.md`,
  `ScratchBird/docs/audit/30_operator_matrix_by_dialect_actual.md`

## Files
- `01_databases_and_schemas.md`
- `02_tables_and_constraints.md`
- `03_indexes_views_sequences.md`
- `04_types_and_domains.md`
- `05_programmable_sql.md`
- `06_dml_select.md`
- `07_dml_modification.md`
- `08_transactions.md`
- `09_security_dcl.md`
- `10_session_show_set.md`
- `11_utilities.md`
- `12_operators.md`
- `13_system_catalog.md`
- `14_functions.md`

Notes:
- Firebird parser accepts many Firebird-specific statements but a number of DDL
  and PSQL constructs are stubbed or blocked by the V2 pipeline.
- Bytecode/executor mismatches are common for CREATE/DROP VIEW/INDEX and some
  table constraint payloads.
