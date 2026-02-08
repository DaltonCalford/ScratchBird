# Transaction System Specifications

Status: Authoritative (V3)
Last Updated: 2026-02-08

This directory contains transaction management specifications for ScratchBird's
Firebird-style MGA system.

## Specifications in this Directory

- `TRANSACTION_MAIN.md` - Core transaction architecture
- `TRANSACTION_MGA_CORE.md` - MGA visibility and versioning
- `TRANSACTION_LOCK_MANAGER.md` - Lock manager and ordering
- `TRANSACTION_DISTRIBUTED.md` - Distributed transactions (if enabled)
- `07_TRANSACTION_AND_SESSION_CONTROL.md` - TCL + session control
- `FIREBIRD_CONSTANTS_REFERENCE.md` - Firebird constants
- `FIREBIRD_GC_SWEEP_GLOSSARY.md` - GC/sweep terminology

## Core Rules

- ScratchBird uses Firebird-style MGA (no undo/WAL).
- WAL is forbidden in V3; any WAL configuration MUST fail with `ERR_FEATURE_DISABLED`.
- Visibility and GC rules are defined in `TRANSACTION_MGA_CORE.md`.

## Related Specs

- `docs/specifications/parser/v3/storage/MGA_IMPLEMENTATION.md`
- `docs/specifications/parser/v3/parser/TRANSACTION_CONTROL.md`
