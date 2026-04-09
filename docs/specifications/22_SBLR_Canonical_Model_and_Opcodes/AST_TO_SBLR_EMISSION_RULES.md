# AST To SBLR Emission Rules

## Status
Authoritative current lowering contract for parser-side AST to `SBLR3` emission.

## Current authority

The lowering and emission path is owned by `V3Emitter` and `AstSblrLowerer`.
The emitted container shape is:

- `SBLR3_VERSION`
- one root `SBLR3_*` statement or expression family
- `SBLR3_END`

Persistent object identity must be lowered to UUID-backed operands before emission.
Name strings are parser ingress material only and are not valid persistent identity in emitted `SBLR`.

## Supported capability lanes

- AST lowering and emission for current shipped statement and expression families
- vNext AST extension shim lowering where explicit AST and opcode support already exists
- parser-assist catalog helper integration for UUID binding and name rehydration

## Explicit exclusions from the current contract

The current contract does not require implementation of:

- extra checksum-heavy emission metadata beyond the canonical container fields already defined in section `22`
- speculative normalization-evidence attachment that is not already required by the verified container and payload rules

Those are not open implementation questions. They are outside the current contract.

## Canonical parser-assist helper family

The canonical parser-assist catalog helper family is:

- `sb_catalog_resolve_name_to_uuid`
- `sb_catalog_resolve_uuid_to_path_name`
- `sb_catalog_snapshot_begin`
- `sb_catalog_delta_since_anchor`

Rules:
1. `sb_catalog_resolve_name_to_uuid` and `sb_catalog_resolve_uuid_to_path_name` are authoritative for current transaction-local catalog truth.
2. `sb_catalog_snapshot_begin` and `sb_catalog_delta_since_anchor` are committed-baseline bulk transport helpers for parser cache seeding and refresh.
3. Bulk helpers never outrank point helpers when current transaction-local `DDL` makes pre-commit overlay state relevant.
4. Bulk helper output must be sufficient for a parser to build and maintain a local `(full_path, object_uuid, object_kind)` map without guessing.
5. Snapshot and delta helpers are internal parser-assist surfaces only; they are not public SQL compatibility features.

## Emission rules for helper usage

1. Parser may seed a local object cache from `sb_catalog_snapshot_begin`.
2. Parser may refresh that cache at transaction boundary from `sb_catalog_delta_since_anchor`.
3. During an active transaction, if parser-local cache and current transaction-local catalog state can diverge because of uncommitted `DDL` in that same transaction, the parser must fall back to `sb_catalog_resolve_name_to_uuid`.
4. Lowering must treat the point helper result as authoritative over any cached snapshot or delta row.
5. The emitted `SBLR` payload must contain UUIDs, not cache-path strings, for persistent object operands.

## Implementation rules

1. Emission must operate on canonical AST only.
2. Capability-gate rejection must happen before emission.
3. Missing UUID binding is a hard failure.
4. Parser-side lowering must not synthesize persistent object UUIDs.
5. Same canonical AST plus same catalog state plus same profile version must emit byte-identical `SBLR`.
