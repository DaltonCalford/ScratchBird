# Native Parser Language Reference (Beta 0.1.0)

## 1. Purpose

This reference describes the currently implemented native parser surface in the
beta `0.1.0` baseline.

## 2. Parsing Pipeline

- Input SQL text
- Parser (`parser_v3`)
- AST
- SBLR container emission
- Executor dispatch

The parser is validated through parser and executor contract tests under
`tests/unit/`.

## 3. Statement Families (Implemented Surface)

### 3.1 DDL/DML Core

- `CREATE`, `ALTER`, `DROP` families for schema objects
- `SELECT`, `INSERT`, `UPDATE`, `DELETE`, `MERGE`
- transaction control and session utilities

### 3.2 Index Management Extensions

Supported statement patterns include:

- `ALTER INDEX ... SET (...)`
- `ALTER INDEX ... RESET (...)`
- `ALTER INDEX ... REBUILD [ONLINE|OFFLINE]`
- `ALTER INDEX ... REBALANCE [ONLINE|OFFLINE]`
- `ALTER INDEX ... RELOCATE TO FILESPACE ...`
- `VALIDATE INDEX ...`
- `ANALYZE INDEX ...`
- `SHOW INDEX HEALTH ...`
- `SHOW INDEX OPTIONS ...`

### 3.3 Native Extension Surface

Implemented parser coverage includes:

- `CREATE SEARCH INDEX ...`
- `CREATE VECTOR INDEX ... METRIC ... TOPK_DEFAULT ...`
- `ALTER SEARCH|VECTOR INDEX ...`
- `DROP SEARCH|VECTOR INDEX ...`
- `CREATE MEASUREMENT ...`
- `ALTER MEASUREMENT ...`
- schedule grammar (`RRULE`, `RRULE_SET`, `RDATE`, `EXDATE`)

### 3.4 Row Lock and Fetch Clauses

Implemented parse coverage includes:

- `FOR UPDATE`
- `FOR NO KEY UPDATE`
- `FOR SHARE`
- `FOR KEY SHARE`
- `FETCH FIRST|NEXT ... ROW[S] [ONLY|WITH TIES]`

## 4. Deterministic Rejection Contracts

The parser intentionally rejects invalid forms with deterministic codes in
covered scenarios (examples in tests include `PRS_0504`, `PRS_0507`).

Examples:

- invalid vector metric / invalid top-k configuration
- `WITH TIES` without `ORDER BY`
- invalid/duplicate schedule RRULE compositions

## 5. Dialect Consistency Boundary

Native parser normalization for style consistency remains an explicit 0.2.0
workstream. The 0.1.0 baseline focuses on correctness and deterministic
contract behavior.

## 6. Validation Sources

Primary tests:

- `tests/unit/test_parser_v3_native_extension_surface.cpp`
- `tests/unit/test_parser_v3_index_management.cpp`
- `tests/unit/test_parser_v3_udr_compile_emitter_contract.cpp`
- `tests/unit/test_sblr_vnext_executor_dispatch_contract.cpp`
- `tests/unit/test_sblr_vnext_payload_schema_mapping_contract.cpp`

## 7. Out of Scope for 0.1.0

- Final emulation parser parity closure across all target engines
- Post-normalization driver conformance reruns
- Performance parity validation vs source engines

These are explicitly tracked in the 0.2.0 planning artifacts.
