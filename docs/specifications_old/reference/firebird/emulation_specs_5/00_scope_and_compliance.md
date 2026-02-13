# Firebird 5 Emulator Specification — Scope and Compliance (Standalone)

## Purpose
This specification is intended for a server-side emulator that must be 1:1 compatible with Firebird 5.0.x behavior as observed by standard clients and drivers.

## Compliance Rule (Standalone)
All authoritative behavior is defined inside this specification set. No external source trees or off-repo documents are required or permitted as normative references. If any ambiguity is detected, resolve it by the explicit rules, tables, and formal schemas included in this directory.

## Required Surfaces
1. SQL/PSQL parser and semantic behavior (DDL/DML/PSQL)
2. Full remote API behavior (attachment, transaction, statement, request, blob, service, events)
3. Complete wire protocol (XDR encoding, handshake, auth, compression, encryption, batch, async channel)
4. Response formatting and data type encoding

## Specification Priority (Internal Only)
1. Formal schemas under `formal/` (protocol state machine, field order, datatypes, catalog, builtins)
2. Concrete tables and enumerations in the narrative specs (`10_*.md`, `20_*.md`, `30_*.md`, `40_*.md`, `50_*.md`, `60_*.md`)
3. Example vectors in `90_test_vectors.md`

## Version Pin
Firebird 5.0.x behavior; protocol versions 10–20.

## Deliverable
An implementation produced from this spec must interoperate with Firebird 5.x clients and drivers without reliance on any external Firebird source code or documentation.
