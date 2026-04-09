# Error Reference UUID Registry and Text Externalization Model

## Purpose
Define the canonical Beta 2 project that removes static client-visible error
text from the ScratchBird engine, assigns every engine error an immutable UUID
reference identity, and makes parser-side render packs authoritative for
human-readable error output.

## Problem Statement
Current engine and executor paths still raise errors through `Status`,
`SQLSTATE`, and inline human-readable message text. That keeps client text in
engine code, makes donor-dialect error mapping text-driven, and prevents
deterministic cross-dialect error identity.

This model replaces that shape with:

- immutable `error_ref_uuid` identity
- typed detail slots
- parser-owned render packs for native ScratchBird text
- donor-parser-owned map packs for donor codes and donor text

## Canonical Invariants

1. Every client-visible ScratchBird engine error shall have one immutable
   `error_ref_uuid`.
2. `error_ref_uuid` is the primary identity for mapping, rendering, telemetry,
   and audit.
3. `Status` remains the coarse runtime status class and must not be used as the
   primary mapping key.
4. `SQLSTATE` remains a machine-readable interoperability field and must not be
   treated as the primary identity for donor mapping.
5. Engine and SBLR runtime paths shall not concatenate client-visible prose for
   cataloged errors.
6. Human-readable text for native ScratchBird output shall live in the V3
   parser render pack.
7. Human-readable donor text and donor error-code selection shall live in the
   donor parser map pack for that donor family.
8. Unknown or uncataloged failures shall collapse to one reserved internal
   error UUID rather than leaking raw engine strings to clients.
9. Reference reports for donor error codes shall live in the reference library
   and must be kept source-backed.

## Registry Identity Rules

### `error_ref_uuid`
- Type: UUID.
- Stability rule: permanent once admitted.
- Generation rule: UUIDv5 over the canonical namespace
  `scratchbird:error-registry` and the stable symbol
  `SBERR.<symbol_name>`.
- Collision rule: build fails if two entries generate the same UUID from
  different symbols or if one symbol resolves to different metadata across
  registries.

### Stable Symbol
- Type: uppercase ASCII token.
- Format: `SBERR_<subsystem>_<condition>`.
- Example: `SBERR_CATALOG_UNDEFINED_TABLE`.
- Stability rule: never reused for another condition.

### Legacy Deterministic Codes
- Existing `vnext_code` strings remain allowed as secondary search keys and
  migration aids.
- `vnext_code` shall not be the primary client mapping key once
  `error_ref_uuid` is present.

## Registry Row Schema
Every registry row shall define:

- `error_ref_uuid`
- `stable_symbol`
- `status`
- `default_sqlstate`
- `default_severity`
- `diagnostic_class`
- `redaction_class`
- `retry_class`
- `detail_schema_id`
- `detail_slot_count`
- `legacy_vnext_code` or null
- `owner_subsystem`
- `native_render_key`
- `supported_donor_families`

## Detail Schema Contract
Each error row shall bind to one detail schema. A detail schema defines:

- ordered slot ids beginning at `0`
- stable slot name
- stable slot type
- nullability
- redaction behavior
- formatting hints for parser-side rendering

Admitted slot types:

- `STRING`
- `UUID`
- `INT64`
- `UINT64`
- `BOOL`
- `DECIMAL128`
- `TIMESTAMP`
- `IDENTIFIER`
- `OBJECT_KIND`
- `SQLSTATE`
- `ENUM_SYMBOL`
- `BYTES_HEX`

Forbidden detail payloads:

- pre-rendered full message text
- pre-rendered full detail text
- locale-specific text
- donor-native error text

## Render Ownership Model

### Native ScratchBird Render Pack
The native V3 parser shall own the canonical ScratchBird client render pack.
Each native render row shall define:

- `error_ref_uuid`
- `locale`
- `message_template`
- `detail_template` or null
- `hint_template` or null
- `field_population_policy`

### Donor Render and Map Pack
Each donor parser shall own a donor map pack keyed by `error_ref_uuid`. Every
map row shall define:

- `error_ref_uuid`
- `donor_family`
- `donor_primary_code`
- `donor_secondary_code` or null
- `donor_severity`
- `donor_message_template`
- `donor_detail_template` or null
- `donor_hint_template` or null
- `slot_remap_policy`
- `fallback_class`

The donor map pack is authoritative for donor-visible code and text. No donor
parser may guess from free-form engine text.

## Required Deliverables
This project is complete only when all of the following exist:

1. Engine error string inventory covering core, executor, storage, server, IPC,
   and security paths.
2. Canonical generated error registry with UUID rows and detail schemas.
3. Native ScratchBird render pack in the V3 parser.
4. Donor render and map packs for every enabled donor parser family.
5. Engine and SBLR runtime APIs that emit `error_ref_uuid` plus typed details.
6. IPC and parser envelopes that carry UUID, status, SQLSTATE, and detail
   vector without raw client prose.
7. Reference-library donor error-code packet set with one engine section per
   donor family.
8. Static-text eradication gates proving cataloged engine errors no longer keep
   client prose inline.

## Inventory and Migration Flow

### Phase 1: Inventory
1. Scan engine and executor code for:
   - `ctx->set(`
   - `SET_ERROR_CONTEXT(`
   - `SET_ERROR_CONTEXT_VNEXT(`
   - `ExecutionResult("`
   - `ExecutionStatusException(`
   - `sendError(`
   - string-literal throws used for client-visible failures
2. Emit an inventory row for every cataloged failure site.
3. Group duplicate semantic conditions into one canonical row.
4. Refuse to merge rows that carry different detail semantics.

### Phase 2: Registry Freeze
1. Assign one stable symbol per semantic condition.
2. Generate one deterministic UUID per symbol.
3. Define the detail schema and donor applicability.
4. Generate compile-time descriptor artifacts.

### Phase 3: Engine Cutover
1. Replace inline client text with registry descriptor references.
2. Replace free-form concatenation with typed slot population.
3. Keep operator-only internal trace fields separate from client rendering.
4. Preserve `Status` and `SQLSTATE` as machine metadata.

### Phase 4: Parser Cutover
1. Native V3 parser renders ScratchBird text from the native render pack.
2. Donor parsers map UUID rows to donor code and donor text from donor map
   packs.
3. Unmapped UUIDs fall back to one donor-family generic internal error row.
4. Client output is formed only after parser-side rendering.

### Phase 5: Enforcement
1. Build scan rejects new inline client error prose in engine paths.
2. Mapping completeness tests reject missing donor rows for admitted parsers.
3. Reference packet completeness tests reject missing donor error-code reports.

## Engine-Side API Shape
Every engine-visible rejection path shall resolve through a generated error
descriptor:

```cpp
struct EngineErrorDescriptor {
    UUID error_ref_uuid;
    Status status;
    const char* stable_symbol;
    const char* default_sqlstate;
    uint16_t detail_schema_id;
    uint8_t detail_slot_count;
};

enum class ErrorSlotId : uint16_t {
    table_name = 0,
    column_name = 1,
    offending_value = 2,
};
```

Sample use:

```cpp
core::ErrorBuilder err(core::kErrCatalogUndefinedColumn);
err.set(ErrorSlotId::table_name, "sales.orders");
err.set(ErrorSlotId::column_name, "legacy_code");
return err.finish(ctx);
```

Forbidden engine pattern:

```cpp
ctx->set(Status::UNDEFINED_COLUMN,
         "Column 'legacy_code' not found in table 'sales.orders'",
         __FILE__, __LINE__, __func__);
```

## Reserved Internal Error
One reserved registry row shall exist for uncataloged failures:

- stable symbol: `SBERR_INTERNAL_UNREGISTERED_FAILURE`
- detail slots:
  - `source_subsystem`
  - `source_file`
  - `source_line`
  - `legacy_text_digest`

This row is the only production path allowed to carry a legacy text digest
originating from uncataloged failures. The raw legacy text shall not be sent to
the client.

## Operator and Audit Rendering
Operator tooling, support bundles, structured logging, and audit export shall
render from the same parser-side or shared render-pack substrate used for
client-visible text. The engine core shall not regain prose ownership for
observability convenience.

## Reference Library Requirement
The reference library shall carry a dated donor error-code packet set. That
packet set shall contain:

- a manifest covering every donor engine targeted for emulation
- one engine section or report per donor family
- the donor authority paths used for error-code extraction
- the observed donor error identity shape
- extraction notes and local evidence gaps

## Donor Coverage Requirement
The donor error packet set shall cover:

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

## Refusal Rules
- Engine code shall not rely on raw human-readable text to select donor error
  codes.
- Parser code shall not map donor errors by substring matching against engine
  text.
- Registry rows shall not be deleted or reused once released.
- Detail schemas shall not be changed incompatibly for an existing
  `error_ref_uuid`.
- New cataloged engine errors shall not be admitted without a registry row.

## Sample Native Render Row

```yaml
error_ref_uuid: "3ed9d2df-6d57-58e4-9ec7-2e0d8ce6d8c1"
stable_symbol: "SBERR_CATALOG_UNDEFINED_COLUMN"
locale: "en"
message_template: "column \"{column_name}\" does not exist"
detail_template: "table: {table_name}"
hint_template: "Verify the column name or refresh the emulated catalog overlay."
```

## Sample Donor Map Row

```yaml
error_ref_uuid: "3ed9d2df-6d57-58e4-9ec7-2e0d8ce6d8c1"
donor_family: "postgresql"
donor_primary_code: "42703"
donor_severity: "ERROR"
donor_message_template: "column \"{column_name}\" does not exist"
donor_detail_template: null
donor_hint_template: null
slot_remap_policy: "identity"
fallback_class: "GENERIC_UNDEFINED_COLUMN"
```

## Implementation Outcome
When complete, every parser family will consume the same canonical error UUID
and detail payload from the engine while presenting dialect-correct codes and
text to its clients, and ScratchBird will have one stable cross-dialect error
identity plane.
