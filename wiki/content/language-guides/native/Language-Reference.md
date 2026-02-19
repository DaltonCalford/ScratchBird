# Native Parser Language Reference

- Version: `0.1.0`
- Baseline date: `2026-02-19`

## 1. Parsing Contract

The native parser compiles SQL to AST and then to SBLR for execution by the
core engine runtime.

## 2. Supported Statement Families

### 2.1 Core SQL

- DDL: create/alter/drop families
- DML: select/insert/update/delete/merge
- transaction/session control statements

### 2.2 Index Management Surface

Supported forms include:

- `ALTER INDEX ... SET (...)`
- `ALTER INDEX ... RESET (...)`
- `ALTER INDEX ... REBUILD [ONLINE|OFFLINE]`
- `ALTER INDEX ... REBALANCE [ONLINE|OFFLINE]`
- `ALTER INDEX ... RELOCATE TO FILESPACE ...`
- `VALIDATE INDEX ...`
- `ANALYZE INDEX ...`
- `SHOW INDEX HEALTH ...`
- `SHOW INDEX OPTIONS ...`

### 2.3 Native Extension Surface

Supported forms include:

- `CREATE SEARCH INDEX ...`
- `CREATE VECTOR INDEX ... METRIC ... TOPK_DEFAULT ...`
- `ALTER SEARCH|VECTOR INDEX ...`
- `DROP SEARCH|VECTOR INDEX ...`
- `CREATE MEASUREMENT ...`
- `ALTER MEASUREMENT ...`
- `CREATE/ALTER/DROP SCHEDULE ...` with `RRULE` and `RRULE_SET`

### 2.4 Locking and Fetch Clauses

- `FOR UPDATE`
- `FOR NO KEY UPDATE`
- `FOR SHARE`
- `FOR KEY SHARE`
- `FETCH FIRST|NEXT ... [ONLY|WITH TIES]`

## 3. Deterministic Rejection Behavior

The parser intentionally rejects unsupported/invalid forms with deterministic
error codes in covered scenarios, including:

- invalid vector metric and top-k configuration
- invalid schedule RRULE forms
- fetch-with-ties without required ordering

## 4. Out-of-Scope for 0.1.0

- Final normalization pass for dialect-style consistency (0.2.0 track)
- Full emulated parser parity closure against source-engine suites (0.2.0 track)

## 5. Validation Tests

- `tests/unit/test_parser_v3_native_extension_surface.cpp`
- `tests/unit/test_parser_v3_index_management.cpp`
- `tests/unit/test_parser_v3_udr_compile_emitter_contract.cpp`
- `tests/unit/test_sblr_vnext_executor_dispatch_contract.cpp`
- `tests/unit/test_sblr_vnext_payload_schema_mapping_contract.cpp`
