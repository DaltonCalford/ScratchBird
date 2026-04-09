Status: current_authority_beta2

# Beta 2 Donor Datatype Grammar and AST Extension Model

## Purpose

Define the v3 parser and AST expansion required to admit the Beta 2 donor
datatype surface from sections `14` and `15`.

## Hard invariants

1. Every donor datatype name must survive parsing as a first-class identity.
2. Lowering to an existing SB carrier may not discard the original donor type
   spelling, modifiers, or donor-engine profile.
3. All type modifiers needed by sections `14` and `15` must be represented in
   the AST, not reconstructed later by guesswork.

## Canonical AST extension

The v3 type-reference node shall expose at least:

```cpp
struct AstTypeRefV3 {
  std::string canonical_name;
  std::string donor_name;
  std::string donor_engine;
  TypeDeliveryLane delivery_lane;
  core::DataType base_type;
  std::optional<Uuid> system_domain_id_hint;
  std::optional<uint32_t> length;
  std::optional<uint32_t> precision;
  std::optional<uint32_t> scale;
  std::optional<TemporalPrecision> temporal_precision;
  std::optional<IntervalUnit> interval_unit;
  std::optional<VectorLayout> vector_layout;
  std::optional<SchemaPath> element_type;
  bool element_non_null = false;
  ModifierMap modifiers;
};
```

## Required grammar coverage

The parser shall recognize:

1. donor alias spellings for fixed-width integer and float families
2. compact decimal and scaled-float spellings
3. all temporal precision and timezone spellings from the donor inventories
4. unit-locked interval spellings
5. keyword, wildcard, tag, uri, and jsonpath spellings
6. multirange spellings and typed-list spellings
7. document, tuple, struct, module, vector, and geo spellings
8. PostgreSQL-lineage catalog payload type names such as `ACLITEM`,
   `TXID_SNAPSHOT`, and `PG_NODE_TREE`

## Lowering rules

1. `TRANSLATED_ALIAS` rows lower to the existing carrier in the AST while
   retaining the donor-facing `donor_name`
2. `SYSTEM_DOMAIN` rows bind the deterministic system-domain hint during
   lowering
3. `NEW_NATIVE` rows bind the new `DataType` directly
4. typed lists, vectors, and multiranges must retain element or base-range
   metadata in the AST
5. document wrappers must retain codec and donor-profile metadata

## Native v3 surface

The native parser shall accept system-domain creation and column-definition
surfaces for every Beta 2 row so that ScratchBird-native DDL can create and use
the donor-compatible datatypes directly.

## Sample lowering snippet

```cpp
AstTypeRefV3 buildBeta2TypeRef(const ParsedDonorType& parsed) {
  AstTypeRefV3 ref;
  ref.donor_name = parsed.surface_name;
  ref.donor_engine = parsed.engine_name;
  ref.delivery_lane = classifyDeliveryLane(parsed.surface_name);
  ref.base_type = resolveBaseCarrier(parsed.surface_name);
  ref.system_domain_id_hint = maybeResolveSystemDomainHint(parsed.surface_name);
  ref.modifiers = parsed.modifiers;
  ref.vector_layout = parsed.vector_layout;
  ref.interval_unit = parsed.interval_unit;
  ref.temporal_precision = parsed.temporal_precision;
  return ref;
}
```

## Required proof

1. each Beta 2 datatype shall parse in the donor parser that owns it
2. native v3 shall parse system-domain definitions or equivalent native DDL for
   the same row
3. AST round-trip shall preserve donor-facing names and modifiers
