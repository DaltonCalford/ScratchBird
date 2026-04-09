# Parser Normalization to SBLR Validation Matrix

Status: current_authority

## Required parser-side sequence

1. Start from the parser session's current committed catalog baseline.
2. If the transaction boundary advanced, refresh committed metadata through sb_catalog_delta_since_anchor or rebuild the baseline through sb_catalog_snapshot_begin.
3. Resolve durable object paths through sb_catalog_resolve_name_to_uuid when emitting UUID-bound references.
4. Normalize dialect syntax into canonical operator, type, statement, and clause forms.
5. Emit SBLR only after identifier binding, coercion normalization, and unsupported-feature refusal have completed.
6. Use sb_catalog_resolve_uuid_to_path_name only for renderer fidelity and client-facing naming, not as a substitute for execution identity.

## Overlay rule

Uncommitted local schema work may exist in the parser session as an overlay, but it must remain explicitly session-local until transaction publication. Remote uncommitted schema state must never be merged into the committed baseline.

## Autocommit rule

When autocommit is enabled, a successful statement is followed by commit and immediate entry into the next transaction. The next parser transaction may consume a delta refresh after that commit to reduce catalog snapshot traffic.
