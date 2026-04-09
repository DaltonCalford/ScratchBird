# Section 00 Specification Outline

Status: current_authority

## Purpose

Capture the code-backed governance rules that define what ScratchBird executes, what is canonical versus front-door only, how durable identity is represented, and which subsystem boundaries must remain stable.

## Authoritative coverage map

1. execution-path governance
2. durable identity and naming governance
3. transaction and recovery governance
4. dialect-lowering governance
5. catalog artifact provenance governance
6. validation and enforcement obligations

## Current outline

1. Canonical execution path
   - runtime authority is scratchbird_core plus scratchbird_sblr
   - SQL parsers compile to SBLR and do not own execution
2. Durable identity
   - shared ID aliases resolve to UuidV7Bytes across core surfaces
3. Transaction and recovery invariants
   - current code proves MGA-style visibility and horizon management through TIP, OIT, OAT, and OST logic
4. Dialect compilation boundary
   - Firebird, V3, PostgreSQL, and MySQL compiler surfaces lower through the SBLR compilation layer
5. Catalog artifact governance
   - catalog metadata carries source dialect, SBLR bytecode, and native artifact fields
6. Proof obligations
   - governance claims require tests or gates and explicit negative-path refusal where relevant
