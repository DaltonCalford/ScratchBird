Status: current_authority_beta2

# Beta 2 Complex, Document, Vector, and Catalog Type Expansion Model

## Purpose

Define the canonical Beta 2 datatype expansion for donor complex,
document-shaped, vector, multirange, spatial, and catalog-payload types.
This file covers the rows that cannot be treated as simple scalar aliases,
but that are required for full emulation of the Beta 2 donor engines.

## Hard invariants

1. Donor complex types shall persist as real engine values, not as parser-
   only annotations.
2. Beta 2 shall prefer one reusable carrier per semantic family and bind
   donor names through deterministic system domains where that preserves
   exact client-visible behavior.
3. Complex values with subfields or elements shall expose extractor and
   setter semantics through the shared extract-element runtime.
4. Binary envelopes used for donor catalog or system payloads must be
   versioned and self-describing. Anonymous blobs are non-conforming.
5. Vector, spatial, and search-structure types must declare their allowed
   index families in section `18` and may not rely on implicit planner-side
   assumptions.

## Shared carrier model

Beta 2 uses the following carrier strategy:

- one new native `MULTIRANGE` carrier for PostgreSQL-lineage multiranges
- existing `LIST`, `ARRAY`, and range carriers for typed lists and
  single-range wrappers
- existing `BSON`, `JSONB`, `VARIANT`, and `COMPOSITE` carriers for
  document and structured payloads
- existing `VECTOR` carrier plus layout descriptors for the Milvus family
- existing `POINT` and `GEOMETRY` carriers for geo wrappers
- versioned binary envelopes for catalog payload types whose donor engines
  expose opaque internal structures as SQL-visible values

## System-domain naming rule

Every `SYSTEM_DOMAIN` row in this file shall use the deterministic system-domain
name:

`[sb_dom]beta2_complex_<normalized_type_name>`

`normalized_type_name` is the donor datatype name lower-cased with every
non-alphanumeric run replaced by one underscore and with leading or trailing
underscores removed.

Examples:

- `TUPLE (FoundationDB layer)` -> `[sb_dom]beta2_complex_tuple_foundationdb_layer`
- `VECTOR_SPARSE_U32_F32` -> `[sb_dom]beta2_complex_vector_sparse_u32_f32`
- `PG_NODE_TREE` -> `[sb_dom]beta2_complex_pg_node_tree`

## Persistence and mutation rules

1. `MULTIRANGE` persists as an ordered, non-overlapping array of canonical
   range payloads with base-type identity stored in the type descriptor.
2. typed-list wrappers persist on `LIST` and enforce element type and
   element nullability at write time.
3. document wrappers persist on `BSON`, `JSONB`, or `COMPOSITE` and keep
   donor field-tag semantics intact.
4. opaque catalog payload wrappers persist on versioned binary envelopes and
   expose only the selectors explicitly named in this file and section `13`.
5. vectors persist on `VECTOR` with a layout descriptor that includes
   element format, sparsity flag, dimension semantics, and donor-profile
   codec choice.

## Extract and set contract

The following families require both extract and set support:

- multiranges: `range_count`, `range[n]`, `isempty`
- typed lists: `length`, `element[n]`
- document wrappers: named-field selectors, path selectors, and update-by-
  path when the donor surface supports it
- geo wrappers: `x`/`y` or `lat`/`lon`, plus CRS selectors where required
- vectors: `dimension`, `element_format`, `element[n]`, and sparse entry
  selectors for sparse layouts

The following families are extract-only:

- PostgreSQL opaque catalog payload wrappers such as `PG_MCV_LIST`
- `MODULE`
- `AGGREGATEFUNCTION`

## String casts

- structured and document rows must round-trip through canonical donor text
  where the donor has one (`CODEWSCOPE`, `DBREF`, `OBJECT`, tuple text, and
  multirange text)
- opaque catalog payload wrappers may cast to text only through a stable,
  donor-compatible textual renderer
- vectors cast to text through a deterministic bracketed representation
  owned by section `13`

## Index admissibility baseline

- multiranges admit exact families for equality and ordering only where a
  total ordering is defined, and admit generalized or range families for
  containment and overlap
- typed lists are not admitted to ordered exact families unless a donor row
  defines lexicographic semantics
- document wrappers admit exact, inverted, and path-oriented families as
  allowed by section `18`
- vectors admit vector and ANN families only
- geo wrappers admit spatial families only
- opaque catalog payload wrappers are exact-hash only unless section `18`
  explicitly widens admission

## Sample complex registration flow

```cpp
Beta2ComplexTypeDescriptor registerBeta2ComplexType(const DonorTypeRow& row) {
  Beta2ComplexTypeDescriptor desc;
  desc.donor_name = row.donor_name;
  desc.delivery_lane = row.delivery_lane;
  desc.base_type = resolveComplexCarrier(row);
  desc.system_domain_id = deterministicSystemDomainId("beta2.complex", row.donor_name);
  desc.codec_id = resolveCodecId(row);
  desc.selector_profile = buildSelectorProfile(row);
  desc.index_profile = buildIndexProfile(row);
  return desc;
}
```

## Normative matrices

## New native multirange carrier and wrappers

Beta 2 adds one multirange runtime family, then binds donor names to base-type-specific system domains.

| Datatype | Donor databases | Delivery lane | SB carrier or substrate |
| --- | --- | --- | --- |
| `DATEMULTIRANGE` | PostgreSQL, YugabyteDB | `NEW_NATIVE and SYSTEM_DOMAIN` | new `MULTIRANGE` carrier plus donor-named system domains over the base-range descriptor |
| `INT4MULTIRANGE` | PostgreSQL, YugabyteDB | `NEW_NATIVE and SYSTEM_DOMAIN` | new `MULTIRANGE` carrier plus donor-named system domains over the base-range descriptor |
| `INT8MULTIRANGE` | PostgreSQL, YugabyteDB | `NEW_NATIVE and SYSTEM_DOMAIN` | new `MULTIRANGE` carrier plus donor-named system domains over the base-range descriptor |
| `NUMMULTIRANGE` | PostgreSQL, YugabyteDB | `NEW_NATIVE and SYSTEM_DOMAIN` | new `MULTIRANGE` carrier plus donor-named system domains over the base-range descriptor |
| `TSMULTIRANGE` | PostgreSQL, YugabyteDB | `NEW_NATIVE and SYSTEM_DOMAIN` | new `MULTIRANGE` carrier plus donor-named system domains over the base-range descriptor |
| `TSTZMULTIRANGE` | PostgreSQL, YugabyteDB | `NEW_NATIVE and SYSTEM_DOMAIN` | new `MULTIRANGE` carrier plus donor-named system domains over the base-range descriptor |
## Range, vector-list, and typed-list wrappers

These types reuse SB container carriers and add donor-specific element, nullability, and ordering rules.

| Datatype | Donor databases | Delivery lane | SB carrier or substrate |
| --- | --- | --- | --- |
| `LIST_BOOLEAN_NOT_NULL` | Neo4j | `SYSTEM_DOMAIN` | existing `LIST`, `ARRAY`, `COMPOSITE`, and range carriers with deterministic element metadata |
| `LIST_DATE_NOT_NULL` | Neo4j | `SYSTEM_DOMAIN` | existing `LIST`, `ARRAY`, `COMPOSITE`, and range carriers with deterministic element metadata |
| `LIST_DURATION_NOT_NULL` | Neo4j | `SYSTEM_DOMAIN` | existing `LIST`, `ARRAY`, `COMPOSITE`, and range carriers with deterministic element metadata |
| `LIST_FLOAT_NOT_NULL` | Neo4j | `SYSTEM_DOMAIN` | existing `LIST`, `ARRAY`, `COMPOSITE`, and range carriers with deterministic element metadata |
| `LIST_INTEGER_NOT_NULL` | Neo4j | `SYSTEM_DOMAIN` | existing `LIST`, `ARRAY`, `COMPOSITE`, and range carriers with deterministic element metadata |
| `LIST_LOCAL_DATETIME_NOT_NULL` | Neo4j | `SYSTEM_DOMAIN` | existing `LIST`, `ARRAY`, `COMPOSITE`, and range carriers with deterministic element metadata |
| `LIST_LOCAL_TIME_NOT_NULL` | Neo4j | `SYSTEM_DOMAIN` | existing `LIST`, `ARRAY`, `COMPOSITE`, and range carriers with deterministic element metadata |
| `LIST_POINT_NOT_NULL` | Neo4j | `SYSTEM_DOMAIN` | existing `LIST`, `ARRAY`, `COMPOSITE`, and range carriers with deterministic element metadata |
| `LIST_STRING_NOT_NULL` | Neo4j | `SYSTEM_DOMAIN` | existing `LIST`, `ARRAY`, `COMPOSITE`, and range carriers with deterministic element metadata |
| `LIST_ZONED_DATETIME_NOT_NULL` | Neo4j | `SYSTEM_DOMAIN` | existing `LIST`, `ARRAY`, `COMPOSITE`, and range carriers with deterministic element metadata |
| `LIST_ZONED_TIME_NOT_NULL` | Neo4j | `SYSTEM_DOMAIN` | existing `LIST`, `ARRAY`, `COMPOSITE`, and range carriers with deterministic element metadata |
| `TSTZ_RANGE` | XTDB | `SYSTEM_DOMAIN` | existing `LIST`, `ARRAY`, `COMPOSITE`, and range carriers with deterministic element metadata |
## PostgreSQL and Yugabyte catalog payload domains

These types are required for catalog, plan, error, and bootstrap truth in the PostgreSQL-lineage emulation surface.

| Datatype | Donor databases | Delivery lane | SB carrier or substrate |
| --- | --- | --- | --- |
| `ACLITEM` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `COMPOSITE`, `ARRAY`, `BINARY`, `TEXT`, and integer carriers plus donor codec ids |
| `GTSVECTOR` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `COMPOSITE`, `ARRAY`, `BINARY`, `TEXT`, and integer carriers plus donor codec ids |
| `INT2VECTOR` | PostgreSQL, CockroachDB, YugabyteDB | `SYSTEM_DOMAIN` | existing `COMPOSITE`, `ARRAY`, `BINARY`, `TEXT`, and integer carriers plus donor codec ids |
| `OIDVECTOR` | PostgreSQL, CockroachDB, YugabyteDB | `SYSTEM_DOMAIN` | existing `COMPOSITE`, `ARRAY`, `BINARY`, `TEXT`, and integer carriers plus donor codec ids |
| `PG_BRIN_BLOOM_SUMMARY` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `COMPOSITE`, `ARRAY`, `BINARY`, `TEXT`, and integer carriers plus donor codec ids |
| `PG_BRIN_MINMAX_MULTI_SUMMARY` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `COMPOSITE`, `ARRAY`, `BINARY`, `TEXT`, and integer carriers plus donor codec ids |
| `PG_DEPENDENCIES` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `COMPOSITE`, `ARRAY`, `BINARY`, `TEXT`, and integer carriers plus donor codec ids |
| `PG_MCV_LIST` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `COMPOSITE`, `ARRAY`, `BINARY`, `TEXT`, and integer carriers plus donor codec ids |
| `PG_NDISTINCT` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `COMPOSITE`, `ARRAY`, `BINARY`, `TEXT`, and integer carriers plus donor codec ids |
| `PG_NODE_TREE` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `COMPOSITE`, `ARRAY`, `BINARY`, `TEXT`, and integer carriers plus donor codec ids |
| `PG_SNAPSHOT` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `COMPOSITE`, `ARRAY`, `BINARY`, `TEXT`, and integer carriers plus donor codec ids |
| `TID` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `COMPOSITE`, `ARRAY`, `BINARY`, `TEXT`, and integer carriers plus donor codec ids |
| `TXID_SNAPSHOT` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `COMPOSITE`, `ARRAY`, `BINARY`, `TEXT`, and integer carriers plus donor codec ids |
## Document and object wrappers

These donor types must preserve their source-engine structural tags and exact string rendering rules.

| Datatype | Donor databases | Delivery lane | SB carrier or substrate |
| --- | --- | --- | --- |
| `CODE` | MongoDB | `SYSTEM_DOMAIN` | existing `BSON`, `JSONB`, `COMPOSITE`, and `VARIANT` carriers with donor codec and field-policy metadata |
| `CODEWSCOPE` | MongoDB | `SYSTEM_DOMAIN` | existing `BSON`, `JSONB`, `COMPOSITE`, and `VARIANT` carriers with donor codec and field-policy metadata |
| `DBREF` | MongoDB | `SYSTEM_DOMAIN` | existing `BSON`, `JSONB`, `COMPOSITE`, and `VARIANT` carriers with donor codec and field-policy metadata |
| `DYNAMIC` | ClickHouse | `SYSTEM_DOMAIN` | existing `BSON`, `JSONB`, `COMPOSITE`, and `VARIANT` carriers with donor codec and field-policy metadata |
| `OBJECT` | MongoDB | `SYSTEM_DOMAIN` | existing `BSON`, `JSONB`, `COMPOSITE`, and `VARIANT` carriers with donor codec and field-policy metadata |
| `TRANSIT` | XTDB | `SYSTEM_DOMAIN` | existing `BSON`, `JSONB`, `COMPOSITE`, and `VARIANT` carriers with donor codec and field-policy metadata |
## Structured and opaque payload families

These types are persisted as typed envelopes that the executor, bridge layer, and wire adapters can decode without parser-only knowledge.

| Datatype | Donor databases | Delivery lane | SB carrier or substrate |
| --- | --- | --- | --- |
| `AGGREGATEFUNCTION` | ClickHouse | `SYSTEM_DOMAIN` | existing `COMPOSITE`, `BLOB`, `VARIANT`, and `BINARY` carriers plus deterministic system-domain ids |
| `MODULE` | Redis | `SYSTEM_DOMAIN` | existing `COMPOSITE`, `BLOB`, `VARIANT`, and `BINARY` carriers plus deterministic system-domain ids |
| `STRUCT` | DuckDB, XTDB | `SYSTEM_DOMAIN` | existing `COMPOSITE`, `BLOB`, `VARIANT`, and `BINARY` carriers plus deterministic system-domain ids |
| `TUPLE (FoundationDB layer)` | FoundationDB | `SYSTEM_DOMAIN` | existing `COMPOSITE`, `BLOB`, `VARIANT`, and `BINARY` carriers plus deterministic system-domain ids |
## Spatial, ranking, and search structure domains

These types depend on specialized index and plan rendering rules even though their persisted payload rides on existing SB geometric or composite storage.

| Datatype | Donor databases | Delivery lane | SB carrier or substrate |
| --- | --- | --- | --- |
| `GEO_POINT` | OpenSearch | `SYSTEM_DOMAIN` | existing `POINT`, `GEOMETRY`, `MAP`, and `COMPOSITE` carriers with donor search metadata |
| `GEO_SHAPE` | OpenSearch | `SYSTEM_DOMAIN` | existing `POINT`, `GEOMETRY`, `MAP`, and `COMPOSITE` carriers with donor search metadata |
| `RANK_FEATURES` | OpenSearch | `SYSTEM_DOMAIN` | existing `POINT`, `GEOMETRY`, `MAP`, and `COMPOSITE` carriers with donor search metadata |
## Typed vector domains

One vector runtime is retained. Beta 2 adds donor-specific layout descriptors, distance semantics, and bridge codecs.

| Datatype | Donor databases | Delivery lane | SB carrier or substrate |
| --- | --- | --- | --- |
| `VECTOR_ARRAY` | Milvus | `SYSTEM_DOMAIN` | existing `VECTOR` carrier plus layout descriptor (`float`, `float16`, `bfloat16`, `int8`, `binary`, `sparse`, or nested-array`) and donor profile metadata |
| `VECTOR_BFLOAT16` | Milvus | `SYSTEM_DOMAIN` | existing `VECTOR` carrier plus layout descriptor (`float`, `float16`, `bfloat16`, `int8`, `binary`, `sparse`, or nested-array`) and donor profile metadata |
| `VECTOR_BINARY` | Milvus | `SYSTEM_DOMAIN` | existing `VECTOR` carrier plus layout descriptor (`float`, `float16`, `bfloat16`, `int8`, `binary`, `sparse`, or nested-array`) and donor profile metadata |
| `VECTOR_FLOAT` | Milvus | `SYSTEM_DOMAIN` | existing `VECTOR` carrier plus layout descriptor (`float`, `float16`, `bfloat16`, `int8`, `binary`, `sparse`, or nested-array`) and donor profile metadata |
| `VECTOR_FLOAT16` | Milvus | `SYSTEM_DOMAIN` | existing `VECTOR` carrier plus layout descriptor (`float`, `float16`, `bfloat16`, `int8`, `binary`, `sparse`, or nested-array`) and donor profile metadata |
| `VECTOR_INT8` | Milvus | `SYSTEM_DOMAIN` | existing `VECTOR` carrier plus layout descriptor (`float`, `float16`, `bfloat16`, `int8`, `binary`, `sparse`, or nested-array`) and donor profile metadata |
| `VECTOR_SPARSE_U32_F32` | Milvus | `SYSTEM_DOMAIN` | existing `VECTOR` carrier plus layout descriptor (`float`, `float16`, `bfloat16`, `int8`, `binary`, `sparse`, or nested-array`) and donor profile metadata |

## Required donor coverage statement

The donor databases named in the matrices below are the source-backed reason
each complex row is admitted to Beta 2. Complex rows may not be removed,
renamed, or silently folded into parser-only behavior while those donor
engines remain in scope for full emulation.
