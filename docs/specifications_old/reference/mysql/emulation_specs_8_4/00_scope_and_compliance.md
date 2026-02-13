# MySQL 8.4 Emulator Specification — Scope and Compliance

## Purpose
This specification targets 1:1 compatibility with MySQL 8.4 LTS (server-side) as observed by standard clients and drivers.

## Compliance Rule
If any detail is ambiguous in narrative specs, authoritative truth is the copied MySQL source code and documentation under:
- `emulation_specs/mysql-8.4/source_copies/`
- `emulation_specs/mysql-8.4/mysql_docs/`

An implementation MUST follow those sources exactly.

## Required Surfaces
1. SQL grammar and semantics (DDL/DML, stored programs, SQL modes)
2. Full client/server command API
3. Complete MySQL classic wire protocol
4. Response formatting and data type encoding
