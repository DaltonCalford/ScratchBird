[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# PostgreSQL Emulation SQL Surface

## Emulation behavior

SQL in this dialect is parsed by the emulation parser, translated to SBLR,
executed by the ScratchBird engine, and results are formatted back to the
client protocol. Emulated databases are metadata-only schemas and do not
create physical database files.


Parser: PostgreSQL emulation parser (direct SBLR bytecode emission; no V2 semantic stage).

Sources:
- Parser core: `ScratchBird/src/parser/postgresql/pg_parser.cpp`
- DDL: `ScratchBird/src/parser/postgresql/pg_parser_ddl.cpp`
- DML: `ScratchBird/src/parser/postgresql/pg_parser_dml.cpp`
- Expressions: `ScratchBird/src/parser/postgresql/pg_parser_expr.cpp`
- Misc: `ScratchBird/src/parser/postgresql/pg_parser_misc.cpp`
- Executor: `ScratchBird/src/sblr/executor.cpp`
- Audit refs: `ScratchBird/docs/audit/17_postgresql_parser_statement_reference_actual.md`,
  `ScratchBird/docs/audit/19_postgresql_parser_correction_plan_checklist.md`,
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
- Many PostgreSQL statements parse but emit bytecode layouts that do not match
  the executor; these are marked as Stubbed.
- Transaction control, CREATE DATABASE/SCHEMA/DOMAIN, and ANALYZE are the main
  end-to-end implementations.
