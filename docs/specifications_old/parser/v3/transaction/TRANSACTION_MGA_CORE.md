# MGA Core (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define Firebird-style Multi-Generational Architecture (MGA) rules for
visibility, version chains, and garbage collection.

## Core MGA Rules

- Each update creates a new record version; old versions remain on-page.
- Readers never block writers; writers never block readers.
- Visibility is determined by transaction snapshot + TIP state.

## Transaction Inventory Pages (TIP)

- TIP stores 2-bit transaction state for each transaction.
- TIP is the authoritative source of transaction state.

## Visibility Rules

Given a record version with create_xid and delete_xid:
- Visible if create_xid is committed and delete_xid is not visible.
- Snapshot isolation uses oldest snapshot to evaluate visibility.

## Garbage Collection

- Cooperative GC prunes old versions when safe.
- Background sweep advances OIT and reclaims old versions.
- Versions older than OIT are garbage if no active snapshot requires them.

## Related Specs

- `docs/specifications/parser/v3/transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md`
- `docs/specifications/parser/v3/transaction/FIREBIRD_CONSTANTS_REFERENCE.md`
