Status: current_authority_beta2

# Beta 2 Cast, String, and Subfield Access Model

## Purpose

Define the Beta 2 cast, default string-rendering, and subfield-access contract
for the datatype families admitted by section `14` and section `15`.

This file is implementation-driving authority for the new donor-facing datatype
surface. Parser-only recognition without the runtime behavior defined here is
non-conforming.

## Hard invariants

1. Every Beta 2 datatype shall support deterministic `CAST(value AS TEXT)` and
   `CAST(text AS type)`.
2. String casts shall use canonical donor-facing text, not debug dumps.
3. Types with addressable subfields or elements shall expose them through the
   shared `EXTRACT(...)` and setter dispatch path.
4. Lossy casts shall fail under strict coercion unless this file explicitly
   permits the conversion.
5. Subfield mutation is legal only for families listed as setter-capable below.

## Default string forms

| Family | Default textual form |
| --- | --- |
| fixed-width numeric aliases | canonical decimal with donor width preserved in the type descriptor, not in the text itself |
| `BFLOAT16` | shortest round-trip decimal; exact binary form available only through explicit binary-format cast |
| `BIGNUM` | canonical base-10 integer text with optional leading `-` and no exponent |
| compact or scaled decimal domains | canonical decimal text with fixed scale where the donor requires it |
| `NUMBER*` BSON numeric wrappers | donor-compatible numeric text plus retained BSON subtype in the value descriptor |
| `VARBIT` and `QBIT` | binary digit text without spacing; length comes from the value |
| temporal aliases | ISO-8601 or RFC-3339 compatible text with donor-required precision and zone rendering |
| unit-locked intervals | `<sign><magnitude> <unit>` with one legal unit per domain |
| keyword and identifier domains | the stored text exactly, subject to donor normalization rules |
| `JSONPATH` | canonical jsonpath source text |
| fixed-size binary, key bytes, value bytes | lower-case hex prefixed with `0x` |
| `VERSIONSTAMP` | lower-case hex prefixed with `0x` |
| multiranges | donor multirange text with canonical member-range rendering |
| typed lists | bracketed list text with deterministic element rendering |
| document wrappers | donor-compatible document text or constructor text |
| vectors | bracketed deterministic element text; sparse vectors render explicit index-value pairs |
| opaque catalog payload wrappers | stable donor-compatible renderer; never pointer or raw memory dumps |

## Cast lanes

### Always-legal explicit casts

1. datatype to text
2. text to datatype
3. exact-width numeric alias to its base numeric carrier
4. base numeric carrier to exact-width alias when in-range
5. temporal alias to its canonical SB temporal carrier
6. canonical SB temporal carrier to donor alias when precision and timezone
   requirements are satisfiable
7. domain wrapper to base carrier
8. base carrier to domain wrapper when the domain validation contract passes

### Strict-mode refusal rules

Strict coercion shall reject:

1. `TIMESTAMP_NS` to millisecond or second aliases when truncated digits are
   non-zero
2. any cast into `ENUM8` or `ENUM16` where the label table does not contain the
   incoming textual or numeric label
3. `VARBIT` or `QBIT` casts that would drop significant bits
4. text to `JSONPATH` when validation fails
5. text to `KEY_BYTES (FoundationDB)`, `VALUE_BYTES (FoundationDB)`, or
   `VERSIONSTAMP (FoundationDB)` when the hex form is malformed
6. text to multirange when member ranges overlap or are not canonicalized
7. text to vector when the element count, element format, or sparse-entry
   ordering is invalid
8. casts into opaque PostgreSQL catalog payload wrappers without a dedicated
   constructor or codec-specific parser

## Extractor and setter surface

### Extract-only families

- `CID`
- `REFCURSOR`
- `REG*`
- `ACLITEM`
- `GTSVECTOR`
- `INT2VECTOR`
- `OIDVECTOR`
- `PG_BRIN_BLOOM_SUMMARY`
- `PG_BRIN_MINMAX_MULTI_SUMMARY`
- `PG_DEPENDENCIES`
- `PG_MCV_LIST`
- `PG_NDISTINCT`
- `PG_NODE_TREE`
- `PG_SNAPSHOT`
- `TID`
- `TXID_SNAPSHOT`
- `AGGREGATEFUNCTION`
- `MODULE`

### Setter-capable families

- all temporal aliases and wrappers
- unit-locked interval domains
- `VARBIT` and `QBIT`
- network and binary semantic domains
- multiranges
- typed lists
- document wrappers
- geo wrappers
- vector wrappers
- `VERSIONSTAMP`

## Canonical selectors

| Family | Required selectors |
| --- | --- |
| temporals | `year`, `month`, `day`, `hour`, `minute`, `second`, `millisecond`, `microsecond`, `nanosecond`, `timezone_offset_seconds` |
| intervals | `unit`, `sign`, `months`, `days`, `seconds`, `nanos` |
| bitstrings | `bit_length`, `byte_length`, `bit[n]` |
| network values | `family`, `address`, `prefix_length` |
| reference wrappers | `raw_id`, `catalog_binding` |
| versionstamp | `transaction_version`, `user_version` |
| multiranges | `range_count`, `range[n]`, `isempty` |
| typed lists | `length`, `element[n]` |
| document wrappers | `field(name)`, `path(path_text)`, `has_field(name)` |
| geo wrappers | `x`, `y`, `lat`, `lon`, `srid` |
| vectors | `dimension`, `element_format`, `element[n]`, `sparse_entry[n]` |

## Setter names

Setter-capable families shall use the canonical setter surface:

- `SET_FIELD(value, selector, replacement)`
- `SET_PATH(value, path_text, replacement)` for document wrappers
- `SET_ELEMENT(value, index, replacement)` for typed lists and dense vectors
- `SET_SPARSE_ENTRY(value, sparse_index, replacement)` for sparse vectors

The v3 parser may expose donor-native syntax, but it shall lower to the setter
surface above before SBLR emission.

## Sample dispatcher

```cpp
TypedValue applyBeta2Selector(const TypedValue& value,
                              const SelectorRef& selector,
                              const std::optional<TypedValue>& replacement,
                              Status* status) {
  switch (selector.family) {
    case SelectorFamily::Temporal:
      return applyTemporalSelector(value, selector, replacement, status);
    case SelectorFamily::Interval:
      return applyIntervalSelector(value, selector, replacement, status);
    case SelectorFamily::Document:
      return applyDocumentSelector(value, selector, replacement, status);
    case SelectorFamily::Vector:
      return applyVectorSelector(value, selector, replacement, status);
    case SelectorFamily::RangeCollection:
      return applyMultirangeSelector(value, selector, replacement, status);
    default:
      return rejectSelector(value, selector, replacement, status);
  }
}
```

## Required proof

1. every Beta 2 datatype shall round-trip through text under explicit cast
2. every selector listed above shall have success and fail-closed coverage
3. strict and permissive coercion shall both be exercised where behavior differs
4. setter-capable families shall prove whole-value rewrite versus targeted
   subfield rewrite legality
