# Test Contract - 21_V3_Dialect_Surface

## Current code-backed required proof surfaces
- `tests/conformance/v3_native_inet`
- `tests/conformance/public_beta/run_required_public_beta_gate.sh`
- `tests/unit/test_parser_v3_canonical_rejections.cpp`
- `tests/unit/test_parser_v3_udr_compile_emitter_contract.cpp`
- `tests/unit/test_query_compiler_v3.cpp`
- parser family source and header proof in `parser_v3`, `lexer_v3`, `v3_emitter`, and emulated parser trees

## Current required interpretation
- `supported_listener_path` proof is authoritative where present
- parser-header or source proof is authoritative for statement-family presence
- parser-and-lowering proof is stronger than parser-only proof but weaker than end-to-end listener-path proof
- prose-only checklist rows are not enough for implementation proof

## Explicit open gaps
- full per-family clause matrix proof
- exact JDBC promotion closure
- exact listener-control SQL statement matrix
- exact branch-changeset surface closure
- exact storage, connector, cluster, and diagnostics statement-family runtime parity

## Beta 2 required proof additions
- each Beta 2 datatype name parses in the owning donor parser
- native v3 accepts the corresponding system-domain or native type-definition
  surface
- AST round-trip preserves donor name, modifiers, delivery lane, and domain
  hint for every Beta 2 row
- native v3 accepts and canonicalizes `APPLY`, `QUALIFY`, `JSON_TABLE`,
  `PIVOT`, `UNPIVOT`, `MATCH_RECOGNIZE`, `ROWS FROM`, temporal clauses,
  ClickHouse select modifiers, ordered-set aggregate syntax, structured
  function arguments, session snapshot surfaces, and multi-model command entry
  points
- AST round-trip preserves argument mode, argument name, ordered-set metadata,
  window null-treatment, `FROM FIRST/LAST`, select-stage modifier placement,
  temporal clause kind, insert-surface flavor, and multi-model family or verb
- parser hard-refusal coverage proves the former native rejections for
  `APPLY`, `CREATE MATERIALIZED VIEW`, and MySQL-family insert aliases are
  replaced by deterministic canonical lowering
- native v3 accepts and canonicalizes PostgreSQL-family SQL/XML functions,
  `XMLTABLE`, clause-rich SQL/JSON functions, MySQL-family special-function
  forms, insert-source `VALUES(col)`, ClickHouse parametric functions, and
  ClickHouse or DuckDB lambda forms
- AST round-trip preserves SQL/XML clause metadata, SQL/JSON clause metadata,
  aggregate-local separator or limit payloads, typed `ROWS FROM` column
  definitions, insert-source column identity, parametric-function parameters,
  and lambda parameter lists

## Active contradiction owner
- `CCAW-015`
