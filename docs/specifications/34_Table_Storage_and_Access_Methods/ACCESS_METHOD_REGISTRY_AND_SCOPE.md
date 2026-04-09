# Access Method Registry and Scope

Status: current_authority

## Purpose

Define the current access-method registry, ownership boundary, and non-plugin rule for ScratchBird table and index access paths.

## Current Registry

### Primary storage path

Current primary-storage authority is:
- heap-backed row storage through `StorageEngine`
- stable `TID` and `GPID` row addressing
- MGA version-chain aware tuple access

### Secondary access paths

Current secondary and analytical access families are:
- B-tree
- BRIN
- GIN
- GiST
- hash
- HNSW
- LSM tree
- R-tree
- SP-GiST
- bitmap
- columnstore
- inverted index

Deep family semantics remain owned by section `18`, but section `34` owns the fact that these are real current access families in the runtime and not merely planning placeholders.

## Current Runtime Binding

Current code-backed storage runtime already binds access families through:
- `StorageEngine`
- heap scan iterator
- index scan iterator
- family-specific helper paths for insert, delete, update, cleanup, and candidate filtering

This is a concrete runtime dispatch surface, not a generic dynamically pluggable table-access-method API.

## Registry Rules

1. Access methods must be named explicitly rather than inferred from donor-engine taxonomies.
2. Section `34` owns the top-level registry boundary, while section `18` retains deep family semantics for indexes.
3. Primary row storage and secondary access families are different classes and must not be conflated.
4. If a method is not explicitly listed as current, it remains fail closed here.

## Non-Pluggable TableAM Rule

ScratchBird does not currently expose a PostgreSQL-style generic pluggable table-access-method contract for primary storage.

That means:
- heap primary storage is the current authoritative row-store implementation
- specialized families are admitted through explicit runtime and catalog support
- no caller may assume a general-purpose runtime table-AM registration ABI

## Explicit Non-Guarantees

- no generic pluggable table-access API guarantee
- no universal parity across heap and specialized methods
- no claim that every index family is a first-class table-access strategy
