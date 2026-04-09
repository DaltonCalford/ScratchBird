Status: current_authority_beta2

# Beta 2 Datatype Index Admission and Key Normalization Model

## Purpose

Define the index-admission, key-normalization, and family-binding rules for the
Beta 2 datatype families admitted by sections `14` and `15`.

## Hard invariants

1. A Beta 2 datatype is indexable only when this file grants admission to a
   concrete section `18` family.
2. Parser acceptance of donor index syntax is not proof of runtime admission.
3. Key normalization must be deterministic across native SQL, donor parsers,
   and SBLR execution.
4. Types backed by opaque donor envelopes may not silently fall into ordered
   exact indexes unless this file says so.

## Family admission matrix

| Datatype family | Admitted families | Refused families | Key-normalization rule |
| --- | --- | --- | --- |
| fixed-width numeric aliases and numeric domains | `BTREE`, `HASH`, `LSM`, `BRIN` | none by default | normalize to canonical signed, unsigned, float, or decimal key bytes |
| temporal aliases and interval domains | `BTREE`, `HASH`, `LSM`, `BRIN` | vector, spatial, inverted-text unless explicitly extracted to scalar text | normalize to canonical UTC or wall-clock payload plus precision policy |
| keyword, tag, wildcard, and identifier domains | `BTREE`, `HASH`, `LSM`, `BRIN`, `GIN_TEXT`, `INVERTED_TEXT`, `FULLTEXT` when the donor row requires text search semantics | ANN and spatial | normalize to canonical text bytes plus collation or analyzer identity |
| fixed binary and network domains | `BTREE`, `HASH`, `LSM`, `BRIN` | vector, spatial, ranked-text | normalize to canonical raw bytes or canonical address bytes |
| `VERSIONSTAMP` | `BTREE`, `HASH`, `LSM` | inverted, spatial, ANN | normalize to version bytes plus user-version suffix |
| multiranges and range wrappers | `BTREE`, `HASH`, `GIST`, `SPGIST`, `BRIN` | ANN, ranked-text | equality uses canonical merged range-set bytes; generalized families use range bounds and emptiness flags |
| typed lists | `HASH` only by default | ordered exact unless a row explicitly defines lexicographic ordering | normalize element-wise canonical bytes plus element-count prefix |
| document wrappers | `HASH`, `GIN`, `INVERTED`, `MONGODB_WILDCARD`, `FULLTEXT` for extracted text lanes | plain `BTREE` on the full document value | normalize to canonical BSON or JSONB bytes; path indexes normalize extracted scalar keys |
| geo wrappers | `RTREE`, `GIST`, `SPGIST` | exact ordered, ANN | normalize to canonical geometry encoding and SRID |
| vector wrappers | `VECTOR_FLAT`, `HNSW`, `IVF`, `SCANN`, `DISKANN`, `ANNOY`, `NSG`, `GPU_CAGRA` | ordered exact, fulltext, generalized non-vector families | normalize to vector layout descriptor plus canonical dense or sparse payload |
| opaque PostgreSQL catalog payload wrappers | `HASH` only | `BTREE`, `BRIN`, spatial, vector, inverted | normalize to codec-versioned binary envelope bytes |
| search structure wrappers such as `RANK_FEATURES` | `GIN`, `INVERTED`, `BITMAP` where applicable | exact ordered unless extracted to scalar keys | normalize by canonical feature-name ordering and scalar payload bytes |

## Path and extracted-value indexing

1. document wrappers may participate in exact or ordered indexes only through a
   declared extracted expression that yields a scalar or scalar-domain value
2. geo wrappers may participate in exact families only through an explicitly
   extracted scalar projection such as `lat` or `lon`
3. vectors may not participate in scalar-extracted exact families unless the
   extracted value itself is stored as a separate persisted expression result

## Canonical key encoder contract

```cpp
struct Beta2IndexKey {
  Bytes normalized_key;
  uint16_t canonical_type_id;
  Uuid system_domain_id;
  uint32_t codec_id;
  uint32_t modifier_hash;
};

Beta2IndexKey encodeBeta2Key(const TypedValue& value,
                             const IndexProfile& profile,
                             Status* status);
```

The `modifier_hash` must include all type modifiers that affect comparison,
ordering, or textual normalization.

## Refusal rules

1. `PG_*` opaque catalog payload wrappers are hash-only until a later canonical
   file explicitly widens support.
2. `VECTOR_*` and donor vector wrappers may not be indexed by scalar exact
   families on the full value.
3. `MODULE` is not indexable.
4. `AGGREGATEFUNCTION` is not indexable.
5. `JSONPATH` is indexable as text or hashable program text only. It is not a
   document-path index by itself.

## Required proof

1. each admitted family shall prove deterministic key bytes for the Beta 2
   datatype families it accepts
2. each refused family shall fail closed at create time
3. donor-specific index renderers shall round-trip canonical family metadata
   without losing the donor-facing type name
