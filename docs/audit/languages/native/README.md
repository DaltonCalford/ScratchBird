# Native (V2) SQL Surface

Parser: V2 native ScratchBird parser (AST v2 -> SemanticAnalyzerV2 -> BytecodeGeneratorV2 -> executor).

Sources:
- Parser: `ScratchBird/src/parser/parser_v2.cpp`
- AST: `ScratchBird/include/scratchbird/parser/ast_v2.h`
- Semantic/bytecode: `ScratchBird/src/sblr/semantic_analyzer_v2.cpp`, `ScratchBird/src/sblr/bytecode_generator_v2.cpp`
- Executor: `ScratchBird/src/sblr/executor.cpp`
- Audit refs: `ScratchBird/docs/audit/parsers/V2/SUMMARY.md`,
  `ScratchBird/docs/audit/parsers/CRITICAL_FINDINGS.md`,
  `ScratchBird/docs/audit/25_show_set_commands_actual.md`,
  `ScratchBird/docs/audit/29_operator_inventory_actual.md`

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
- V2 is the core dialect; features marked as PostgreSQL-style extensions are
  treated as intentional extensions unless explicitly flagged in spec gaps.
- Temporary table flags are parsed but not applied end-to-end (see critical
  findings).
