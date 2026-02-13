# PostgreSQL 18.1 Emulator Specification — Scope and Compliance

## Purpose
This specification targets 1:1 compatibility with PostgreSQL 18.1 server behavior.

## Compliance Rule
If any detail is ambiguous in narrative specs, authoritative truth is the copied PostgreSQL 18.1 source code and documentation under:
- `emulation_specs/postgresql-18.1/src/`
- `emulation_specs/postgresql-18.1/doc/`

## Required Surfaces
1. SQL grammar and semantics (DDL/DML, PL/pgSQL)
2. Full FE/BE protocol (startup, auth, simple and extended query, COPY)
3. Response formatting and data type encoding
4. Catalog-visible behavior and error codes
