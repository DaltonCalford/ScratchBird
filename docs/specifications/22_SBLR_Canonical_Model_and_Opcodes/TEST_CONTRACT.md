# Section 22 Test Contract

Status: current_authority

## Certification lanes

1. Container decode and encode round-trip for valid SBL3 artifacts, including
   retained-symbol sections when present.
2. Deterministic serialization for logically identical parser output.
3. Verifier rejection of malformed headers, duplicate singleton sections,
   invalid pool references, unknown required sections, opcode and payload
   mismatches, and malformed retained-symbol registries.
4. Statement payload validation for all currently shipped statement classes.
5. Domain payload validation for all current scalar and complex domain families carried by v3.
6. Parser helper usage certification:
   - committed baseline loaded through sb_catalog_snapshot_begin
   - incremental refresh through sb_catalog_delta_since_anchor
   - point lookups through sb_catalog_resolve_name_to_uuid and sb_catalog_resolve_uuid_to_path_name
7. Rendering fidelity checks proving canonical SBLR can be surfaced back to supported client expectations without losing durable object identity.

## Refusal rules

- An implementation that accepts unverifiable SBLR fails this contract.
- An implementation that emits name-only durable object references where UUID binding is required fails this contract.
- An implementation that bypasses committed-baseline helper synchronization fails this contract.

## Beta 2 required proof additions

1. every Beta 2 type reference shall round-trip through SBLR encode and decode
2. modifier blobs, donor-name refs, and system-domain ids shall be verified
3. selector and setter payloads shall round-trip for every setter-capable Beta 2 family
4. every Beta 2 donor-dialect payload from sections `21` and `28` shall
   round-trip through SBLR encode and decode
5. generic window-function payloads shall preserve canonical function identity
   and donor-facing render symbols
6. ordered-set payloads, structured argument items, temporal bindings, select
   modifier blocks, and insert-surface flavors shall verify deterministically
7. multi-model command envelopes shall reject invalid family or verb pairings
   and shall round-trip with deterministic payload ordering
8. function payload v3 shall preserve parameter items, aggregate-local option
   payloads, SQL/JSON metadata, SQL/XML metadata, and special-function syntax
   payloads deterministically
9. insert-source value, lambda, `XMLTABLE`, and `ROWS_FROM_V2` payloads shall
   round-trip and reject invalid scope or shape combinations
10. error payload encode and decode shall preserve `error_ref_uuid`,
    `stable_symbol`, `status`, `sqlstate`, detail-slot ordering, and
    cause-chain ordering deterministically
11. verifier rejection shall fail on unknown error UUIDs, mismatched stable
    symbols, invalid detail-slot types, repeated slots, and invalid
    `payload_version`
