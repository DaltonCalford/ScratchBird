# Dialect Conformance Assertions (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

Purpose: define mandatory assertions the conformance harness MUST check for
all dialects.

## 1) ScratchBird (Native)

Minimum assertions:
- CREATE/ALTER/DROP TABLE round-trip via sys.catalog.
- INSERT/UPDATE/DELETE correctness with MGA visibility.
- Transaction isolation modes (READ COMMITTED + SNAPSHOT).
- Parser → SBLR → executor → storage round-trip.
- No SQL parsing occurs inside the engine.

## 2) PostgreSQL Emulation

Minimum assertions:
- SQL parser accepts PostgreSQL 16+ grammar for core DDL/DML.
- pg_catalog and pg_stat_* views map to sys.* correctly.
- Array/JSONB/INET types parse and emit correct SBLR types.
- ON CONFLICT behavior matches INSERT/UPDATE semantics.
- OID mapping is deterministic per `OID_MAPPING_STRATEGY.md`.

## 3) MySQL Emulation

Minimum assertions:
- SQL parser accepts MySQL 8.x grammar for core DDL/DML.
- INFORMATION_SCHEMA/mysql/performance_schema views map to sys.* correctly.
- ON DUPLICATE KEY UPDATE emits correct SBLR on_conflict encoding.
- UNSIGNED/ZEROFILL semantics enforced at type level.

## 4) Firebird Emulation

Minimum assertions:
- Firebird dialect grammar parses for DDL/DML/PSQL.
- RDB$/MON$/SEC$ views map correctly to sys.*.
- MGA visibility preserved (Firebird-style behavior).

## 5) Protocol Rejection

- TDS/MSSQL protocol is rejected with `ERR_FEATURE_DISABLED`.

## Related Specs

- `docs/specifications/parser/v3/operations/MONITORING_DIALECT_MAPPINGS.md`
- `docs/specifications/parser/v3/operations/OID_MAPPING_STRATEGY.md`
