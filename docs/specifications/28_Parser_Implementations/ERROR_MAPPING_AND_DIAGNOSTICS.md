# Error Mapping and Diagnostics Contract

## Purpose
Define exact parser-side transformation rules from canonical ScratchBird error
UUID envelopes into native ScratchBird or donor-dialect client errors, with no
text guessing and no engine-owned client prose.

## Owning Inputs
This contract depends on:

- section `20` error registry and render ownership rules
- section `22` canonical UUID/detail transport envelope
- section `23` engine emission and registry binding rules

## Canonical Internal Error Envelope
Every parser shall normalize inbound failures to:

- `correlation_uuid`
- `source_layer`
- `error_ref_uuid`
- `stable_symbol`
- `status`
- `sqlstate`
- `severity`
- `detail_schema_id`
- `detail_items`
- `cause_chain`
- `legacy_vnext_code` or null
- `original_client_payload`
- `engine_visible_payload`
- `source_span` or null
- `sblr_checksum` or null

`sb_error_code` text is no longer the primary mapping key. `error_ref_uuid` is.

## Render Packs

### Native ScratchBird
The native V3 parser owns the canonical ScratchBird client render pack. The
native pack is authoritative for:

- `message`
- `detail`
- `hint`
- client field population order

### Donor Families
Every donor parser owns a donor map pack keyed by `error_ref_uuid`. The donor
pack is authoritative for:

- donor-visible primary error code
- donor-visible secondary error code or SQLSTATE
- donor-visible message text
- donor-visible severity or status fields
- donor-visible detail or hint fields
- slot remap and slot suppression policy

## Supported Donor Families
This contract must admit donor map packs for:

- Apache Ignite
- Cassandra
- Citus
- ClickHouse
- CockroachDB
- Dolt
- DuckDB
- FirebirdSQL
- FoundationDB
- immudb
- InfluxDB
- MariaDB
- Milvus
- MongoDB
- MySQL
- Neo4j
- OpenSearch
- PostgreSQL
- Redis
- SQLite
- TiDB
- Vitess
- XTDB
- YugabyteDB

## Mapping Targets

### Native ScratchBird
- ScratchBird native code or symbolic name if defined by the client contract
- SQLSTATE
- message
- detail
- hint

### PostgreSQL-Family
- SQLSTATE
- severity
- message
- detail
- hint
- position when parser-origin source span exists

Families:

- PostgreSQL
- Citus
- CockroachDB
- YugabyteDB
- XTDB when using PostgreSQL wire compatibility

### MySQL-Family
- numeric error code
- SQLSTATE
- message

Families:

- MySQL
- MariaDB
- TiDB
- Vitess
- Dolt

### Firebird
- SQLSTATE where the donor surface expects it
- engine-specific numeric code
- message fields expected by Firebird clients

### MongoDB
- numeric code
- `codeName`
- `errmsg`

### Cassandra
- protocol exception code
- message

### Neo4j
- status code string
- message

### Redis
- RESP error prefix
- message

### Milvus
- RPC or API status code
- reason

### Other Donor Families
For Apache Ignite, ClickHouse, DuckDB, FoundationDB, immudb, InfluxDB,
OpenSearch, and SQLite, the parser-local donor map pack shall emit the exact
code and text shapes admitted by that donor family’s reference packet. No
family may be reduced to generic SQLSTATE-only output if the donor client model
expects a richer or different code identity.

## Deterministic Mapping Algorithm
1. Decode the canonical UUID/detail envelope.
2. Validate `error_ref_uuid` against the parser’s admitted registry snapshot.
3. If the parser target is native ScratchBird:
   - lookup native render pack by `error_ref_uuid`
   - render client text from typed detail slots
   - emit native result frame
4. If the parser target is a donor family:
   - lookup donor map row by `error_ref_uuid`
   - if found, apply slot remap policy and render donor output
   - if not found, lookup the donor family generic internal-error row
5. Populate diagnostics fields from canonical envelope metadata.
6. Persist trace record with render-pack id, map-pack id, and selected row id.

Production mapping logic shall not use:

- substring matching on engine text
- regex matching on engine text
- parser-family-specific heuristics based on English message fragments

## Fallback Rules

### Unmapped UUID
If a donor family lacks a row for a received `error_ref_uuid`, the parser shall:

1. emit the donor family generic internal error code
2. render the donor family generic internal error text
3. preserve `correlation_uuid`
4. record `unmapped_uuid_fallback=true` in diagnostics

### Uncataloged Internal Failure
If the engine emits the reserved uncataloged internal UUID, the parser shall:

1. emit the donor or native generic internal error row
2. never expose the raw legacy engine text
3. preserve audit fields and operator trace id

## Parser-Origin Errors
Parser-origin errors still use the canonical envelope. They shall:

- use parser-owned `error_ref_uuid` rows
- include source span where available
- render through the same native or donor render pack mechanism

This keeps parser and engine errors on one identity plane.

## Donor Map Row Schema
Every donor map row shall define:

- `error_ref_uuid`
- `donor_family`
- `donor_code_primary`
- `donor_code_secondary` or null
- `donor_severity`
- `message_template`
- `detail_template` or null
- `hint_template` or null
- `slot_remap_policy`
- `slot_drop_policy`
- `fallback_class`
- `reference_packet_row_id`

## Slot Mapping Rules
- slot remap is by stable slot name or ordinal, never by rendered text
- donor-specific inserted literals are allowed only in parser templates
- donor text templates may omit unsupported ScratchBird slots
- donor text templates may not introduce semantic claims absent from the
  canonical detail vector

## Diagnostics Rendering Rules
1. All client-visible errors must carry `correlation_uuid`.
2. Parser-origin errors may include input snippet and source span.
3. Engine-origin errors may include engine-visible normalized payload ids and
   SBLR checksum when the client model admits them.
4. Sanitization shall be applied before any client-visible rendering.
5. Cause chain exposure is donor-family specific, but full cause-chain identity
   must remain in audit output.

## Reference Packet Requirement
Each donor parser family shall have one source-backed donor error-code reference
packet entry in the reference library. The donor map pack is not complete until
its reference packet exists and identifies the donor authority paths.

## Required Validation Tests
- mapping completeness test:
  - no enabled parser target is missing required UUID rows
- determinism test:
  - same canonical envelope maps to byte-identical donor output
- fallback test:
  - unknown UUID maps to the deterministic donor generic internal error row
- sanitization test:
  - secret-bearing detail slots do not appear in client-visible render output
- no-text-key test:
  - mapping code cannot branch on engine message text
- reference-packet completeness test:
  - every enabled donor parser family has a current donor error-code packet

## Audit normalization note (2026-04-02)
- Current code-backed shipped parser-agent proof still exists only for native
  V3 plus the dedicated Firebird, PostgreSQL, and MySQL parser agents.
- This error UUID contract is wider than the current shipped parser set and is
  the target-state canonical requirement for all enabled donor families.
- Donor families without shipped parser agents are still required to complete
  the donor error-code packet and donor map pack before they can claim error
  parity.
