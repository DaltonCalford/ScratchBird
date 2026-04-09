Status: current_authority_beta2

# Beta 2 Scalar, Temporal, Text, and Identifier Type Expansion Model

## Purpose

Define the canonical Beta 2 datatype expansion required to support the next
emulation wave for Apache Ignite, Cassandra, Citus, ClickHouse,
CockroachDB, Dolt, DuckDB, FirebirdSQL, FoundationDB, immudb, InfluxDB,
MariaDB, Milvus, MongoDB, MySQL, Neo4j, OpenSearch, PostgreSQL, Redis,
SQLite, TiDB, Vitess, XTDB, and YugabyteDB.

This file covers the scalar, scalar-adjacent, temporal, textual, binary,
network, identifier, and catalog-reference surfaces that are missing from
the current ScratchBird scalar runtime but are present in the donor-source
comparison inventory.

## Hard invariants

1. Beta 2 shall not add one new core `DataType` enum member per donor label
   when an existing SB carrier plus deterministic system-domain metadata is
   sufficient.
2. Every datatype admitted by this file shall support: persistence,
   deserialization, `TypedValue` carriage, executor comparison or
   arithmetic where semantically legal, default cast to and from string,
   `EXTRACT(...)` or `SET_FIELD(...)` style access where the type has
   addressable subfields, index-key encoding, v3 parser recognition, and
   SBLR type transport.
3. Donor-facing type names remain first-class catalog and parser identities
   even when they lower to an existing SB carrier.
4. String rendering must be deterministic and donor-profile aware. Binary
   or protocol adapters may not guess at textual forms.
5. Every system-domain-backed donor type must obtain its id from the
   existing deterministic system-domain mechanism in section `15`.

## Delivery lanes

| Delivery lane | Meaning |
| --- | --- |
| `NEW_NATIVE` | ScratchBird adds a new core runtime carrier with dedicated `DataType`, serializer lane, `TypedValue` storage, and executor support. |
| `SYSTEM_DOMAIN` | ScratchBird persists the value on an existing core carrier and binds donor semantics through a deterministic system domain plus parameter contract. |
| `TRANSLATED_ALIAS` | ScratchBird uses an existing core carrier directly. Parser, AST, catalog, wire, and bridge layers preserve the donor name and donor render rules. |

## Shared implementation contract

For every row in the matrices below, Beta 2 shall implement the following:

1. `types.h`
   - add a new core type only for `NEW_NATIVE` rows
   - add emulation mapping rows for every donor name
   - bind domain hints and parser-rule hints for `SYSTEM_DOMAIN` rows
2. `TypeInfo`
   - carry width, precision, scale, timezone, interval-unit, enum-width,
     codec id, vector layout, and donor-profile modifiers as needed
3. `TypedValue`
   - store the canonical carrier value plus optional donor metadata handle
   - expose deterministic comparison, hashing, and string conversion
4. `TypeSerializer`
   - persist canonical carrier payload plus any required subtype or domain
     metadata reference
5. `TypeSystem`
   - resolve donor names to the correct carrier or system domain
   - expose full cast, extract, update, and whole-value-mutation policy
6. Extractors and setters
   - types with internal fields must expose selectors through the existing
     `extract_element_catalog` and `extract_element_ops` path
7. Parser and SBLR
   - v3 `TypeRef` shall preserve donor name, modifiers, subtype metadata,
     and system-domain binding
   - SBLR shall carry a canonical type payload sufficient for parser-free
     execution

## System-domain naming rule

Every `SYSTEM_DOMAIN` row in this file shall use the deterministic system-domain
name:

`[sb_dom]beta2_scalar_<normalized_type_name>`

`normalized_type_name` is the donor datatype name lower-cased with every
non-alphanumeric run replaced by one underscore and with leading or trailing
underscores removed.

Examples:

- `KEYWORD (XTDB)` -> `[sb_dom]beta2_scalar_keyword_xtdb`
- `KEY_BYTES (FoundationDB)` -> `[sb_dom]beta2_scalar_key_bytes_foundationdb`
- `REGPROCEDURE` -> `[sb_dom]beta2_scalar_regprocedure`

## Family rules

### Numeric and integer families

- `TRANSLATED_ALIAS` numeric rows lower to the corresponding existing fixed-
  width carrier and preserve donor width and signedness in the type
  descriptor.
- `DECIMAL32`, `DECIMAL64`, and `SCALED_FLOAT` persist on the existing
  decimal carrier and add compact-width or scale-lock metadata in the
  system-domain parameter vector.
- `NUMBERINT`, `NUMBERLONG`, `NUMBERDOUBLE`, and `NUMBERDECIMAL` preserve
  BSON numeric subtype identity even when the physical carrier is an
  existing integer, float, or decimal value.
- `ENUM8` and `ENUM16` use the existing enum family but fix the storage
  width and label-table contract.
- `VARBIT` and `QBIT` must preserve bit length in the type descriptor and
  admit ordered exact comparison only on canonical bitstring bytes.

### Temporal and interval families

- `DATE_NANOS` lowers to `TIMESTAMP_NS` and uses UTC normalization.
- `DATETIME32`, `DATETIME64`, `TIMESTAMP_MS`, `TIMESTAMP_SEC`, and
  `TIME64` lower to existing temporal carriers with fixed precision rules.
- `LOCAL_*` and `ZONED_*` families distinguish wall-clock interpretation
  from instant interpretation in the type descriptor rather than in
  formatter-side heuristics.
- unit-locked ClickHouse intervals are persisted on the shared `INTERVAL`
  carrier and enforce a single legal unit per system domain.

### Text, identifier, and keyword families

- keyword-like rows persist as normal text payloads but enforce donor
  normalization and comparison rules, including exact-match-only or
  analyzer-bypassed semantics where required.
- `JSONPATH` persists as canonical text plus validated parse-tree cache.
  The stored truth is the canonical string form, not an ephemeral compiled
  pointer.
- PostgreSQL-style `REG*` rows persist the underlying reference value plus
  donor catalog binding rules. Rendering back to client text is a bridge and
  catalog operation, not a parser-only rewrite.

### Binary and network families

- `FIXED_SIZE_BINARY`, `KEY_BYTES (FoundationDB)`, and `VALUE_BYTES
  (FoundationDB)` shall preserve exact byte sequences without character-set
  reinterpretation.
- `VERSIONSTAMP (FoundationDB)` is a new native scalar with deterministic
  tuple-pack, unpack, comparison, and textual hex rendering rules.
- `IP`, `IPV4`, and `IPV6` shall lower to the existing network family and
  preserve donor render policy for host-only versus CIDR-like text.

## Scalar field extraction and update rules

The following selectors are mandatory when the type has addressable
subfields:

- temporals: `year`, `month`, `day`, `hour`, `minute`, `second`,
  `millisecond`, `microsecond`, `nanosecond`, `timezone_offset_seconds`
- intervals: `unit`, `sign`, `months`, `days`, `seconds`, `nanos`
- network: `family`, `address`, `prefix_length`
- versionstamp: `transaction_version`, `user_version`
- reg and ref wrappers: `raw_id`, `catalog_binding`

Setter support is required for temporals, intervals, network values, and
`VERSIONSTAMP`. `REG*`, `CID`, and `REFCURSOR` rows are extract-only and may
only be changed through explicit constructor or cast functions.

## Default string casts

- every row must support `CAST(value AS TEXT)` and `CAST(text AS type)`
- casts must use canonical donor-facing text, not debug dumps
- lossy casts must fail under strict coercion unless a row-specific rule in
  section `13` explicitly permits them
- `JSONPATH`, `KEYWORD`, `TAG`, and `URI` must round-trip through canonical
  text without reformatting drift

## Index admissibility baseline

Unless a stricter rule exists in section `18`, the rows in this file are
admitted to `BTREE`, `HASH`, `LSM`, and `BRIN` when a canonical ordering or
hash exists. Text-semantic rows additionally admit inverted or fulltext
families where the donor surface requires them. Temporal and numeric rows
admit exact ordered families by default.

## Sample registration flow

```cpp
Beta2ScalarTypeDescriptor registerBeta2ScalarType(const DonorTypeRow& row) {
  Beta2ScalarTypeDescriptor desc;
  desc.donor_name = row.donor_name;
  desc.delivery_lane = row.delivery_lane;
  desc.system_domain_id = row.delivery_lane == DeliveryLane::SystemDomain
      ? deterministicSystemDomainId("beta2.scalar", row.donor_name)
      : Uuid{};
  desc.base_type = resolveBaseCarrier(row);
  desc.type_modifiers = buildTypeModifierVector(row);
  desc.extractor_profile = buildExtractorProfile(row);
  desc.index_profile = buildIndexProfile(row);
  return desc;
}
```

## Normative matrices

## New native scalar carriers

These donor types require a dedicated runtime carrier rather than a wrapper over an existing SB scalar.

| Datatype | Donor databases | Delivery lane | SB carrier or substrate |
| --- | --- | --- | --- |
| `BFLOAT16` | ClickHouse | `NEW_NATIVE` | new `DataType` and `TypedValue` carrier |
| `BIGNUM` | DuckDB | `NEW_NATIVE` | new `DataType` and `TypedValue` carrier |
| `VERSIONSTAMP (FoundationDB)` | FoundationDB | `NEW_NATIVE` | new `DataType` and `TypedValue` carrier |
## Translated fixed-width numeric aliases

These donor names lower to existing SB numeric storage with donor-name preservation in parser, catalog, and wire layers.

| Datatype | Donor databases | Delivery lane | SB carrier or substrate |
| --- | --- | --- | --- |
| `F32` | XTDB | `TRANSLATED_ALIAS` | existing fixed-width signed, unsigned, and float carriers |
| `F64` | XTDB | `TRANSLATED_ALIAS` | existing fixed-width signed, unsigned, and float carriers |
| `HUGEINT` | DuckDB | `TRANSLATED_ALIAS` | existing fixed-width signed, unsigned, and float carriers |
| `I16` | XTDB | `TRANSLATED_ALIAS` | existing fixed-width signed, unsigned, and float carriers |
| `I32` | XTDB | `TRANSLATED_ALIAS` | existing fixed-width signed, unsigned, and float carriers |
| `I64` | XTDB | `TRANSLATED_ALIAS` | existing fixed-width signed, unsigned, and float carriers |
| `I8` | XTDB | `TRANSLATED_ALIAS` | existing fixed-width signed, unsigned, and float carriers |
| `U16` | XTDB | `TRANSLATED_ALIAS` | existing fixed-width signed, unsigned, and float carriers |
| `U32` | XTDB | `TRANSLATED_ALIAS` | existing fixed-width signed, unsigned, and float carriers |
| `U64` | XTDB | `TRANSLATED_ALIAS` | existing fixed-width signed, unsigned, and float carriers |
| `U8` | XTDB | `TRANSLATED_ALIAS` | existing fixed-width signed, unsigned, and float carriers |
| `UBIGINT` | DuckDB | `TRANSLATED_ALIAS` | existing fixed-width signed, unsigned, and float carriers |
| `UHUGEINT` | DuckDB | `TRANSLATED_ALIAS` | existing fixed-width signed, unsigned, and float carriers |
| `UINTEGER` | DuckDB, InfluxDB | `TRANSLATED_ALIAS` | existing fixed-width signed, unsigned, and float carriers |
| `USMALLINT` | DuckDB | `TRANSLATED_ALIAS` | existing fixed-width signed, unsigned, and float carriers |
| `UTINYINT` | DuckDB | `TRANSLATED_ALIAS` | existing fixed-width signed, unsigned, and float carriers |
## Compact and tagged numeric domains

These types require donor-specific precision, scaling, tag retention, or comparison rules, but not a separate core page format.

| Datatype | Donor databases | Delivery lane | SB carrier or substrate |
| --- | --- | --- | --- |
| `CID` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `DECIMAL`, `FLOAT64`, `INT32`, `INT64`, `UINT32`, `UINT64`, and `BIT` carriers plus domain metadata |
| `DECIMAL32` | ClickHouse | `SYSTEM_DOMAIN` | existing `DECIMAL`, `FLOAT64`, `INT32`, `INT64`, `UINT32`, `UINT64`, and `BIT` carriers plus domain metadata |
| `DECIMAL64` | ClickHouse | `SYSTEM_DOMAIN` | existing `DECIMAL`, `FLOAT64`, `INT32`, `INT64`, `UINT32`, `UINT64`, and `BIT` carriers plus domain metadata |
| `ENUM16` | ClickHouse | `SYSTEM_DOMAIN` | existing `DECIMAL`, `FLOAT64`, `INT32`, `INT64`, `UINT32`, `UINT64`, and `BIT` carriers plus domain metadata |
| `ENUM8` | ClickHouse | `SYSTEM_DOMAIN` | existing `DECIMAL`, `FLOAT64`, `INT32`, `INT64`, `UINT32`, `UINT64`, and `BIT` carriers plus domain metadata |
| `MURMUR3` | OpenSearch | `SYSTEM_DOMAIN` | existing `DECIMAL`, `FLOAT64`, `INT32`, `INT64`, `UINT32`, `UINT64`, and `BIT` carriers plus domain metadata |
| `NUMBERDECIMAL` | MongoDB | `SYSTEM_DOMAIN` | existing `DECIMAL`, `FLOAT64`, `INT32`, `INT64`, `UINT32`, `UINT64`, and `BIT` carriers plus domain metadata |
| `NUMBERDOUBLE` | MongoDB | `SYSTEM_DOMAIN` | existing `DECIMAL`, `FLOAT64`, `INT32`, `INT64`, `UINT32`, `UINT64`, and `BIT` carriers plus domain metadata |
| `NUMBERINT` | MongoDB | `SYSTEM_DOMAIN` | existing `DECIMAL`, `FLOAT64`, `INT32`, `INT64`, `UINT32`, `UINT64`, and `BIT` carriers plus domain metadata |
| `NUMBERLONG` | MongoDB | `SYSTEM_DOMAIN` | existing `DECIMAL`, `FLOAT64`, `INT32`, `INT64`, `UINT32`, `UINT64`, and `BIT` carriers plus domain metadata |
| `OID8` | PostgreSQL | `SYSTEM_DOMAIN` | existing `DECIMAL`, `FLOAT64`, `INT32`, `INT64`, `UINT32`, `UINT64`, and `BIT` carriers plus domain metadata |
| `QBIT` | ClickHouse | `SYSTEM_DOMAIN` | existing `DECIMAL`, `FLOAT64`, `INT32`, `INT64`, `UINT32`, `UINT64`, and `BIT` carriers plus domain metadata |
| `SCALED_FLOAT` | OpenSearch | `SYSTEM_DOMAIN` | existing `DECIMAL`, `FLOAT64`, `INT32`, `INT64`, `UINT32`, `UINT64`, and `BIT` carriers plus domain metadata |
| `TOKEN_COUNT` | OpenSearch | `SYSTEM_DOMAIN` | existing `DECIMAL`, `FLOAT64`, `INT32`, `INT64`, `UINT32`, `UINT64`, and `BIT` carriers plus domain metadata |
| `VARBIT` | PostgreSQL, CockroachDB, YugabyteDB | `SYSTEM_DOMAIN` | existing `DECIMAL`, `FLOAT64`, `INT32`, `INT64`, `UINT32`, `UINT64`, and `BIT` carriers plus domain metadata |
## Temporal precision and timezone aliases

These donor types reuse SB temporal carriers and impose donor-specific precision, rounding, and rendering policies.

| Datatype | Donor databases | Delivery lane | SB carrier or substrate |
| --- | --- | --- | --- |
| `DATETIME32` | ClickHouse | `TRANSLATED_ALIAS` | existing `DATE`, `TIME`, `TIMESTAMP`, `TIMESTAMP_WITH_ZONE`, `TIME_WITH_ZONE`, `INTERVAL`, and `TIMESTAMP_NS` carriers |
| `DATETIME64` | ClickHouse | `TRANSLATED_ALIAS` | existing `DATE`, `TIME`, `TIMESTAMP`, `TIMESTAMP_WITH_ZONE`, `TIME_WITH_ZONE`, `INTERVAL`, and `TIMESTAMP_NS` carriers |
| `DATE_NANOS` | OpenSearch | `TRANSLATED_ALIAS` | existing `DATE`, `TIME`, `TIMESTAMP`, `TIMESTAMP_WITH_ZONE`, `TIME_WITH_ZONE`, `INTERVAL`, and `TIMESTAMP_NS` carriers |
| `INSTANT` | XTDB | `TRANSLATED_ALIAS` | existing `DATE`, `TIME`, `TIMESTAMP`, `TIMESTAMP_WITH_ZONE`, `TIME_WITH_ZONE`, `INTERVAL`, and `TIMESTAMP_NS` carriers |
| `LOCAL_DATETIME` | Neo4j | `TRANSLATED_ALIAS` | existing `DATE`, `TIME`, `TIMESTAMP`, `TIMESTAMP_WITH_ZONE`, `TIME_WITH_ZONE`, `INTERVAL`, and `TIMESTAMP_NS` carriers |
| `LOCAL_TIME` | Neo4j | `TRANSLATED_ALIAS` | existing `DATE`, `TIME`, `TIMESTAMP`, `TIMESTAMP_WITH_ZONE`, `TIME_WITH_ZONE`, `INTERVAL`, and `TIMESTAMP_NS` carriers |
| `TIMESTAMP_LOCAL` | XTDB | `TRANSLATED_ALIAS` | existing `DATE`, `TIME`, `TIMESTAMP`, `TIMESTAMP_WITH_ZONE`, `TIME_WITH_ZONE`, `INTERVAL`, and `TIMESTAMP_NS` carriers |
| `TIMESTAMP_MS` | DuckDB | `TRANSLATED_ALIAS` | existing `DATE`, `TIME`, `TIMESTAMP`, `TIMESTAMP_WITH_ZONE`, `TIME_WITH_ZONE`, `INTERVAL`, and `TIMESTAMP_NS` carriers |
| `TIMESTAMP_SEC` | DuckDB | `TRANSLATED_ALIAS` | existing `DATE`, `TIME`, `TIMESTAMP`, `TIMESTAMP_WITH_ZONE`, `TIME_WITH_ZONE`, `INTERVAL`, and `TIMESTAMP_NS` carriers |
| `TIMESTAMP_TZ` | DuckDB, XTDB | `TRANSLATED_ALIAS` | existing `DATE`, `TIME`, `TIMESTAMP`, `TIMESTAMP_WITH_ZONE`, `TIME_WITH_ZONE`, `INTERVAL`, and `TIMESTAMP_NS` carriers |
| `TIME64` | ClickHouse | `TRANSLATED_ALIAS` | existing `DATE`, `TIME`, `TIMESTAMP`, `TIMESTAMP_WITH_ZONE`, `TIME_WITH_ZONE`, `INTERVAL`, and `TIMESTAMP_NS` carriers |
| `TIME_LOCAL` | XTDB | `TRANSLATED_ALIAS` | existing `DATE`, `TIME`, `TIMESTAMP`, `TIMESTAMP_WITH_ZONE`, `TIME_WITH_ZONE`, `INTERVAL`, and `TIMESTAMP_NS` carriers |
| `TIME_NS` | DuckDB | `TRANSLATED_ALIAS` | existing `DATE`, `TIME`, `TIMESTAMP`, `TIMESTAMP_WITH_ZONE`, `TIME_WITH_ZONE`, `INTERVAL`, and `TIMESTAMP_NS` carriers |
| `TIME_TZ` | DuckDB | `TRANSLATED_ALIAS` | existing `DATE`, `TIME`, `TIMESTAMP`, `TIMESTAMP_WITH_ZONE`, `TIME_WITH_ZONE`, `INTERVAL`, and `TIMESTAMP_NS` carriers |
| `ZONED_DATETIME` | Neo4j | `TRANSLATED_ALIAS` | existing `DATE`, `TIME`, `TIMESTAMP`, `TIMESTAMP_WITH_ZONE`, `TIME_WITH_ZONE`, `INTERVAL`, and `TIMESTAMP_NS` carriers |
| `ZONED_TIME` | Neo4j | `TRANSLATED_ALIAS` | existing `DATE`, `TIME`, `TIMESTAMP`, `TIMESTAMP_WITH_ZONE`, `TIME_WITH_ZONE`, `INTERVAL`, and `TIMESTAMP_NS` carriers |
## Unit-locked interval domains

The stored value is one canonical interval payload. The domain fixes the legal unit and string grammar.

| Datatype | Donor databases | Delivery lane | SB carrier or substrate |
| --- | --- | --- | --- |
| `INTERVALDAY` | ClickHouse | `SYSTEM_DOMAIN` | existing `INTERVAL` carrier plus unit-lock domain parameter |
| `INTERVALHOUR` | ClickHouse | `SYSTEM_DOMAIN` | existing `INTERVAL` carrier plus unit-lock domain parameter |
| `INTERVALMICROSECOND` | ClickHouse | `SYSTEM_DOMAIN` | existing `INTERVAL` carrier plus unit-lock domain parameter |
| `INTERVALMILLISECOND` | ClickHouse | `SYSTEM_DOMAIN` | existing `INTERVAL` carrier plus unit-lock domain parameter |
| `INTERVALMINUTE` | ClickHouse | `SYSTEM_DOMAIN` | existing `INTERVAL` carrier plus unit-lock domain parameter |
| `INTERVALMONTH` | ClickHouse | `SYSTEM_DOMAIN` | existing `INTERVAL` carrier plus unit-lock domain parameter |
| `INTERVALNANOSECOND` | ClickHouse | `SYSTEM_DOMAIN` | existing `INTERVAL` carrier plus unit-lock domain parameter |
| `INTERVALQUARTER` | ClickHouse | `SYSTEM_DOMAIN` | existing `INTERVAL` carrier plus unit-lock domain parameter |
| `INTERVALSECOND` | ClickHouse | `SYSTEM_DOMAIN` | existing `INTERVAL` carrier plus unit-lock domain parameter |
| `INTERVALWEEK` | ClickHouse | `SYSTEM_DOMAIN` | existing `INTERVAL` carrier plus unit-lock domain parameter |
| `INTERVALYEAR` | ClickHouse | `SYSTEM_DOMAIN` | existing `INTERVAL` carrier plus unit-lock domain parameter |
## Text, keyword, and identifier semantic domains

These types need donor-specific normalization, collation, analyzer, or identifier behavior while still persisting as normal SB text payloads.

| Datatype | Donor databases | Delivery lane | SB carrier or substrate |
| --- | --- | --- | --- |
| `ANNOTATED_TEXT` | OpenSearch | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `FIXEDSTRING` | ClickHouse | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `ICU_COLLATION_KEYWORD` | OpenSearch | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `JSONPATH` | PostgreSQL, CockroachDB, YugabyteDB | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `KEYWORD` | OpenSearch | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `KEYWORD (XTDB)` | XTDB | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `MATCH_ONLY_TEXT` | OpenSearch | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `RANK_FEATURE` | OpenSearch | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `REFCURSOR` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `REGCLASS` | PostgreSQL, XTDB, YugabyteDB | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `REGCOLLATION` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `REGCONFIG` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `REGDATABASE` | PostgreSQL | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `REGDICTIONARY` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `REGNAMESPACE` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `REGOPER` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `REGOPERATOR` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `REGPROC` | PostgreSQL, XTDB, YugabyteDB | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `REGPROCEDURE` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `REGROLE` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `REGTYPE` | PostgreSQL, YugabyteDB | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `TAG` | InfluxDB | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `URI` | XTDB | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `UTF8` | XTDB | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `VERSION` | OpenSearch | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
| `WILDCARD` | OpenSearch | `SYSTEM_DOMAIN` | existing `CHAR`, `VARCHAR`, and `TEXT` carriers plus donor profile metadata |
## Binary and network semantic domains

These types require exact donor byte-layout, textual rendering, or network-family rules without a distinct page-format family.

| Datatype | Donor databases | Delivery lane | SB carrier or substrate |
| --- | --- | --- | --- |
| `FIXED_SIZE_BINARY` | XTDB | `SYSTEM_DOMAIN` | existing `BINARY`, `VARBINARY`, `BLOB`, `INET`, and `CIDR` carriers plus donor codec parameters |
| `IP` | OpenSearch | `SYSTEM_DOMAIN` | existing `BINARY`, `VARBINARY`, `BLOB`, `INET`, and `CIDR` carriers plus donor codec parameters |
| `IPV4` | ClickHouse | `SYSTEM_DOMAIN` | existing `BINARY`, `VARBINARY`, `BLOB`, `INET`, and `CIDR` carriers plus donor codec parameters |
| `IPV6` | ClickHouse | `SYSTEM_DOMAIN` | existing `BINARY`, `VARBINARY`, `BLOB`, `INET`, and `CIDR` carriers plus donor codec parameters |
| `KEY_BYTES (FoundationDB)` | FoundationDB | `SYSTEM_DOMAIN` | existing `BINARY`, `VARBINARY`, `BLOB`, `INET`, and `CIDR` carriers plus donor codec parameters |
| `VALUE_BYTES (FoundationDB)` | FoundationDB | `SYSTEM_DOMAIN` | existing `BINARY`, `VARBINARY`, `BLOB`, `INET`, and `CIDR` carriers plus donor codec parameters |

## Required donor coverage statement

The donor databases named in the matrices above are the reason each row is
admitted to Beta 2. Removal of a row requires demonstrating that the donor
source no longer exposes that datatype or that another canonical row covers
it byte-for-byte without donor-visible behavior loss.
