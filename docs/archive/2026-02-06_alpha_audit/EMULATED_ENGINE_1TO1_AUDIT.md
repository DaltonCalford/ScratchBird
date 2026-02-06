# Emulated Engine 1:1 Parity Audit (PostgreSQL/MySQL/Firebird)

Status: Draft (Audit)
Date: 2026-02-01
Scope: Emulated engine parsers + protocol adapters. The audit checks whether
current code supports 1:1 wire-protocol and SQL behavior for PostgreSQL,
MySQL/MariaDB, and Firebird.

## Summary
The emulated listeners and parsers are not yet 1:1 compatible. Core protocol
adapters exist, but several handshake/auth, TLS, capability, and metadata
behaviors diverge from native engines. SQL parser coverage also contains
explicit unsupported paths.

Key gaps:
- TLS/GSSENC and auth verification are incomplete or not implemented for
  PostgreSQL/MySQL/Firebird protocol adapters.
- Several PostgreSQL/MySQL/Firebird SQL features are explicitly rejected in
  emulated parsers.
- Firebird adapter does not fully parse XDR packet lengths and supports only a
  subset of opcodes/BLR types.

## PostgreSQL

### Protocol Adapter Gaps
- TLS not supported (SSLRequest always returns 'N').
  - `src/protocol/adapters/postgresql_adapter.cpp:586`
- GSSENC not supported (GSSENCRequest returns 'N').
  - `src/protocol/adapters/postgresql_adapter.cpp:597`
- SCRAM-PLUS channel binding not supported.
  - `src/protocol/adapters/postgresql_adapter.cpp:699`
- MD5 computation path marked TODO (needs verification/cleanup).
  - `src/protocol/adapters/postgresql_adapter.cpp:3504`
- Server version string is hardcoded to "15.0.0 ScratchBird" and not aligned
  with emulation target versions or ParameterStatus expectations.
  - `src/protocol/adapters/postgresql_adapter.cpp:38`

### SQL Parser Gaps
- JSONPATH not supported.
  - `src/parser/postgresql/pg_parser.cpp:644`
- Array domains not supported.
  - `src/parser/postgresql/pg_parser.cpp:742`
- Table-level CHECK constraints not supported.
  - `src/parser/postgresql/pg_parser_ddl.cpp:380`
- TABLESPACE clauses are rejected (by design for emulation), but no
  compatibility workaround is documented.
  - `src/parser/postgresql/pg_parser_ddl.cpp:507`, `:1704`
- CREATE DOMAIN base type not supported.
  - `src/parser/postgresql/pg_parser_ddl.cpp:2410`
- ALTER TABLE DROP CONSTRAINT / ALTER COLUMN SET/DROP DEFAULT/NOT NULL
  unsupported.
  - `src/parser/postgresql/pg_parser_ddl.cpp:3119`, `:3136`, `:3141`
- ALTER TABLE ALTER COLUMN ... USING unsupported.
  - `src/parser/postgresql/pg_parser_ddl.cpp:3154`
- TRUNCATE options not supported.
  - `src/parser/postgresql/pg_parser_ddl.cpp:3828`
- JOIN USING not supported.
  - `src/parser/postgresql/pg_parser_dml.cpp:417`
- DEFAULT values in multi-row INSERT not supported.
  - `src/parser/postgresql/pg_parser_dml.cpp:893`
- MERGE USING subqueries not supported.
  - `src/parser/postgresql/pg_parser_dml.cpp:1277`

## MySQL / MariaDB

### Protocol Adapter Gaps
- No TLS handshake support or SSLRequest handling.
  - No SSL/TLS path in `src/protocol/adapters/mysql_adapter.cpp`
- Authentication is accepted without validation (TODO).
  - `src/protocol/adapters/mysql_adapter.cpp:984`
- Database existence validation is missing (TODO).
  - `src/protocol/adapters/mysql_adapter.cpp:1386`
- Capability flags are static and may not match target server versions
  (e.g., mysql_native_password vs caching_sha2_password defaults,
  DEPRECATE_EOF differences between 5.7 and 8.0).
  - `include/scratchbird/protocol/adapters/mysql_adapter.h:407`

### SQL Parser Gaps
- Window frame offsets not supported.
  - `src/parser/mysql/mysql_parser.cpp:2422`
- Named windows not supported.
  - `src/parser/mysql/mysql_parser.cpp:2431`
- DEFAULT values in multi-row INSERT/REPLACE not supported.
  - `src/parser/mysql/mysql_parser.cpp:3075`, `:3792`
- ALTER TABLE CHANGE COLUMN rename not supported.
  - `src/parser/mysql/mysql_parser.cpp:4762`
- ALTER TABLE ALTER COLUMN SET/DROP DEFAULT not supported.
  - `src/parser/mysql/mysql_parser.cpp:4780`, `:4787`, `:4792`
- GRANT/REVOKE ON ALL not supported in bytecode.
  - `src/parser/mysql/mysql_parser.cpp:7962`, `:8111`

## Firebird

### Protocol Adapter Gaps
- Authentication is accepted without validation (SRP/legacy auth not enforced).
  - `src/protocol/adapters/firebird_adapter.cpp:1501`, `:2314`
- XDR packet length handling is approximate (min-size + fixed cap), not
  full XDR framing.
  - `src/protocol/adapters/firebird_adapter.cpp:402-411`
- Supported opcodes are limited (no blob ops, events, batch, services, etc.).
  - Opcode switch at `src/protocol/adapters/firebird_adapter.cpp:420-577`
- BLR parser only supports a minimal set of datatypes (short/long/int64/text/varying).
  - `src/protocol/adapters/firebird_adapter.cpp:120-214`

### SQL Parser Gaps
- ALTER ROLE/USER/MAPPING/SHADOW not supported.
  - `src/parser/firebird/firebird_parser.cpp:2036`, `:2055`, `:2059`, `:2063`
- DROP USER/MAPPING/SHADOW not supported.
  - `src/parser/firebird/firebird_parser.cpp:2152`, `:2156`, `:2160`
- ALTER DATABASE options not supported.
  - `src/parser/firebird/firebird_parser.cpp:2355`
- RECREATE ROLE/USER/MAPPING/SHADOW not supported.
  - `src/parser/firebird/firebird_parser.cpp:2400`, `:2404`, `:2408`, `:2412`
- ALTER TABLE SET not supported.
  - `src/parser/firebird/firebird_parser.cpp:2634`

## Recommended Next Steps
1. Implement TLS/auth verification paths for PostgreSQL/MySQL/Firebird adapters.
2. Align server capability flags and version strings to target engine versions.
3. Expand Firebird adapter opcode coverage and full XDR framing.
4. Close SQL parser unsupported paths per engine specification.

