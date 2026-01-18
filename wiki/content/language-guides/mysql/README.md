[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# MySQL Emulation SQL Surface

## Emulation behavior

SQL in this dialect is parsed by the emulation parser, translated to SBLR,
executed by the ScratchBird engine, and results are formatted back to the
client protocol. Emulated databases are metadata-only schemas and do not
create physical database files.


Parser: MySQL emulation parser (direct SBLR bytecode emission; no V2 semantic stage).

Sources:
- Parser: `ScratchBird/src/parser/mysql/mysql_parser.cpp`
- Lexer: `ScratchBird/src/parser/mysql/mysql_lexer.cpp`
- Executor: `ScratchBird/src/sblr/executor.cpp`
- Audit refs: `ScratchBird/docs/audit/18_mysql_parser_statement_reference_actual.md`,
  `ScratchBird/docs/audit/20_mysql_parser_correction_plan_checklist.md`,
  `ScratchBird/docs/audit/30_operator_matrix_by_dialect_actual.md`

## Files
- [01_databases_and_schemas](01_databases_and_schemas.md)
- [02_tables_and_constraints](02_tables_and_constraints.md)
- [03_indexes_views_sequences](03_indexes_views_sequences.md)
- [04_types_and_domains](04_types_and_domains.md)
- [05_programmable_sql](05_programmable_sql.md)
- [06_dml_select](06_dml_select.md)
- [07_dml_modification](07_dml_modification.md)
- [08_transactions](08_transactions.md)
- [09_security_dcl](09_security_dcl.md)
- [10_session_show_set](10_session_show_set.md)
- [11_utilities](11_utilities.md)
- [12_operators](12_operators.md)
- [13_system_catalog](13_system_catalog.md)
- [14_functions](14_functions.md)

Notes:
- DDL coverage is limited; CREATE TABLE and CREATE/DROP DATABASE are implemented,
  but most other DDL statements are missing or stubbed.
- Many DML statements parse but do not execute due to bytecode format mismatch.
