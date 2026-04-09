Status: current_authority_beta2

# Beta 2 SBLR Type Payload and Extract/Set Extension Model

## Purpose

Define the SBLR transport contract for the Beta 2 datatype families from
sections `14` and `15`.

## Hard invariants

1. SBLR must carry enough type information to execute Beta 2 datatypes without
   reparsing donor SQL text.
2. Donor type identity, delivery lane, and system-domain binding must survive
   SBLR serialization.
3. Selector and setter operations must use canonical payloads, not parser-local
   callback state.

## Canonical Beta 2 type payload

```cpp
struct SblrBeta2TypeRef {
  uint16_t base_type_id;
  uint8_t delivery_lane;
  uint8_t donor_engine_id;
  PoolRef donor_name_ref;
  U128 system_domain_id;
  uint32_t codec_id;
  uint32_t modifier_flags;
  PoolRef modifier_blob_ref;
};
```

`modifier_blob_ref` shall encode precision, scale, vector layout, interval
unit, temporal precision, element non-null policy, and any donor-specific codec
arguments required by sections `14` and `15`.

## Literal and constructor rules

1. `NEW_NATIVE` carriers require literal or constructor support in SBLR for:
   - `BFLOAT16`
   - `BIGNUM`
   - `VERSIONSTAMP`
   - `MULTIRANGE`
2. `SYSTEM_DOMAIN` rows use the base carrier literal plus the beta2 type payload
   above
3. document, tuple, vector, and opaque catalog payload constructors shall emit
   deterministic codec-tagged payload sections

## Selector and setter payloads

```cpp
struct SblrBeta2Selector {
  uint8_t selector_family;
  PoolRef selector_name_ref;
  PoolRef selector_path_ref;
  int32_t ordinal;
};
```

The existing extract and setter expression surface shall carry
`SblrBeta2Selector` whenever the target type belongs to the Beta 2 families.

## Emission rules

1. the emitter shall serialize the full beta2 type payload for every typed
   literal, column reference, cast target, constructor, and bind parameter
2. the lowerer shall attach selector payloads for every `EXTRACT`, `SET_FIELD`,
   `SET_PATH`, `SET_ELEMENT`, and `SET_SPARSE_ENTRY` operation
3. verification shall reject missing or inconsistent system-domain ids when the
   delivery lane is `SYSTEM_DOMAIN`

## Sample emission snippet

```cpp
SblrBeta2TypeRef emitBeta2TypeRef(const AstTypeRefV3& ref) {
  return {
      .base_type_id = static_cast<uint16_t>(ref.base_type),
      .delivery_lane = static_cast<uint8_t>(ref.delivery_lane),
      .donor_engine_id = encodeDonorEngine(ref.donor_engine),
      .donor_name_ref = poolIntern(ref.donor_name),
      .system_domain_id = encodeUuid(ref.system_domain_id_hint.value_or(Uuid{})),
      .codec_id = resolveCodecId(ref),
      .modifier_flags = computeModifierFlags(ref),
      .modifier_blob_ref = poolIntern(serializeTypeModifiers(ref)),
  };
}
```

## Required proof

1. SBLR encode and decode shall round-trip every Beta 2 type reference
2. verifier rejection shall cover missing donor-name refs, invalid modifier
   blobs, and lane/domain mismatches
3. selector payloads shall round-trip for every setter-capable Beta 2 family
