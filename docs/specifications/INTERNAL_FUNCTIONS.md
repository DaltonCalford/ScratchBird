# Internal Functions Specification (Alpha)

Status: Draft for Alpha alignment. Scope: V2 parser/SBLR emission, executor behavior, and type contracts
for internal functions that already exist in the executor.

## Goals
- Make V2 emit bytecode for all internal functions already implemented in the executor.
- Define return types and nullability for temporal, JSON/JSONB, and array functions.
- Establish JSONB binary semantics and JSON array string semantics.
- Allow expression-level calls to stored PSQL functions and UDR functions.

## Function Resolution (V2)
1. Built-in functions: resolved by name in the semantic analyzer and emitted as opcodes.
2. Stored PSQL functions: resolved from catalog and emitted as EXT_EXPR_FUNCTION_CALL.
3. UDR functions: resolved from catalog and emitted as EXT_EXPR_FUNCTION_CALL.
4. Procedures are not legal in expressions.

## Temporal Functions
All temporal results are typed (DATE/TIME/TIMESTAMP) with UTC storage plus a timezone offset.
Offsets are stored in seconds, and values are normalized to UTC on disk.

Return types:
- NOW(), CURRENT_TIMESTAMP -> TIMESTAMP WITH TIME ZONE
- CURRENT_DATE -> DATE (with configured default time in display/casts)
- CURRENT_TIME -> TIME WITH TIME ZONE
- DATE_ADD(date_or_ts, days) -> same type as first argument
- DATE_SUB(date_or_ts, days) -> same type as first argument
- DATE_DIFF(date1, date2) -> INT64 days
- AT TIME ZONE -> TIMESTAMP WITH TIME ZONE

Default date time:
- Use server.time.date_default_time (sb_config.ini). Default 00:00:00.

## Function Matrix (core subset)
- LTRIM -> EXT_FUNC_LTRIM -> VARCHAR
- RTRIM -> EXT_FUNC_RTRIM -> VARCHAR
- CONCAT -> EXT_FUNC_CONCAT -> VARCHAR (NULL if any arg is NULL)
- CONCAT_WS -> EXT_FUNC_CONCAT_WS -> VARCHAR (NULL if separator is NULL; skip NULL args)
- NOW/CURRENT_TIMESTAMP -> FUNC_NOW -> TIMESTAMP WITH TIME ZONE
- CURRENT_DATE -> FUNC_CURRENT_DATE -> DATE
- CURRENT_TIME -> EXT_FUNC_CURRENT_TIME -> TIME WITH TIME ZONE
- DATE_ADD/DATE_SUB -> FUNC_DATE_ADD/FUNC_DATE_SUB -> same type as first argument
- DATE_DIFF -> FUNC_DATE_DIFF -> INT64 days
- JSON_* -> JSON opcodes -> JSON (text)
- JSONB_* -> JSONB opcodes -> JSONB (binary)
- ST_POINT -> EXT_ST_POINT -> POINT
- ST_MAKELINE -> EXT_ST_MAKELINE -> LINESTRING
- ST_MAKEPOLYGON -> EXT_ST_MAKEPOLYGON -> POLYGON
- ST_ASTEXT -> EXT_ST_ASTEXT -> TEXT (WKT)
- ST_ASBINARY -> EXT_ST_ASBINARY -> TEXT (WKB bytes, alpha placeholder)
- ST_GEOMETRYTYPE -> EXT_ST_GEOMETRYTYPE -> TEXT
- ST_ISVALID -> EXT_ST_ISVALID -> BOOLEAN

## JSON and JSONB
JSON:
- Stored as text.
- JSON_* functions return JSON (text) unless explicitly documented otherwise.

JSONB:
- Stored as canonical binary JSON (CBOR) using sorted keys and minimal encoding.
- JSONB_* functions return JSONB.
- JSONB toString returns canonical JSON text.

### JSONB Binary Format Options (Pros/Cons)
CBOR (chosen):
- Pros: canonical form available, compact, widely supported in nlohmann::json.
- Cons: not identical to PostgreSQL JSONB, requires canonicalization rules.

MessagePack:
- Pros: compact, fast, simple encoder/decoder.
- Cons: lacks standard canonical ordering; not PostgreSQL JSONB compatible.

Custom binary:
- Pros: full control, can mimic PostgreSQL JSONB layout.
- Cons: expensive to design/test, higher maintenance.

## JSON Array String Semantics
Array functions and ARRAY_AGG operate on JSON array strings (not DataType::ARRAY).
Array stats accept JSON array strings containing numeric values or numeric strings.

Return types:
- ARRAY_AGG -> JSON array string
- ARRAY_* helpers -> JSON/TEXT/BOOLEAN/INT64 depending on function semantics

## Expression-Level Stored/UDR Calls
EXT_EXPR_FUNCTION_CALL payload:
- function kind (builtin/function/udr)
- function UUID
- argument count

Behavior:
- Stored PSQL functions execute with SQL SECURITY (DEFINER/INVOKER).
- UDR functions must be callable and subject to EXECUTE permission.
- Legacy binary UDFs remain removed.

## Error Semantics
- Unsupported or unresolved functions are compile-time errors.
- Expression-level function execution errors are runtime errors.
