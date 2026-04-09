Status: current_authority_beta2

# Beta 2 TypedValue, Serializer, and Executor Type Expansion Model

## Purpose

Define the runtime implementation contract for the Beta 2 datatype families
admitted by sections `14`, `15`, `13`, `18`, `21`, and `22`.

## Runtime ownership surface

Beta 2 type execution is owned jointly by:

- `scratchbird/core/types.h`
- `scratchbird/core/typed_value.h`
- `src/core/typed_value.cpp`
- `scratchbird/core/type_serialization.h`
- `src/core/type_serialization.cpp`
- `src/core/type_system.cpp`
- `src/sblr/expression_evaluator.cpp`
- `src/sblr/extract_element_catalog.cpp`
- `src/sblr/extract_element_ops.cpp`
- `src/sblr/executor.cpp`

## Required native additions

Beta 2 shall add new core runtime carriers for:

1. `BFLOAT16`
2. `BIGNUM`
3. `VERSIONSTAMP`
4. `MULTIRANGE`

All other Beta 2 rows are delivered through existing carriers plus deterministic
system-domain metadata or translated alias handling.

## Canonical runtime descriptor

```cpp
struct Beta2RuntimeTypeDescriptor {
  core::DataType base_type;
  TypeDeliveryLane delivery_lane;
  Uuid system_domain_id;
  uint32_t codec_id;
  ModifierVector modifiers;
  SelectorProfile selector_profile;
  IndexProfile index_profile;
};
```

`TypedValue` and `TypeSerializer` shall be able to obtain a
`Beta2RuntimeTypeDescriptor` from `TypeInfo` without consulting the parser.

## Serializer rules

1. `NEW_NATIVE` carriers shall have dedicated serializer tags and deterministic
   binary envelopes
2. `SYSTEM_DOMAIN` rows shall serialize the underlying carrier payload plus a
   stable domain reference and codec or modifier block
3. translated aliases serialize identically to their base carrier and rely on
   `TypeInfo` or catalog metadata for donor-name preservation

## Executor rules

1. arithmetic is legal for numeric, decimal, and interval families only
2. ordering is legal where sections `14` or `15` define a total order
3. equality and hashing are required for every Beta 2 datatype except where the
   donor itself refuses equality
4. document, vector, geo, and multirange operations must use family-local
   helpers instead of ad-hoc generic branches
5. write-path coercion shall route through the same runtime descriptor used by
   expression evaluation and serialization

## Sample runtime dispatch

```cpp
Status evaluateBeta2CastOrSelector(const TypedValue& input,
                                   const Beta2RuntimeTypeDescriptor& desc,
                                   const RuntimeRequest& req,
                                   TypedValue* out) {
  switch (desc.base_type) {
    case DataType::BFLOAT16:
      return evalBfloat16Request(input, desc, req, out);
    case DataType::BIGNUM:
      return evalBignumRequest(input, desc, req, out);
    case DataType::VERSIONSTAMP:
      return evalVersionstampRequest(input, desc, req, out);
    case DataType::MULTIRANGE:
      return evalMultirangeRequest(input, desc, req, out);
    default:
      return evalDomainBackedBeta2Request(input, desc, req, out);
  }
}
```

## Query-result and bridge contract

1. executor result materialization must preserve donor-facing type identity so
   bridge UDRs and donor wire renderers can send the expected metadata
2. result-cache and JIT paths may specialize on the runtime descriptor, but may
   not erase the donor-facing type name
3. native and donor result renderers shall both consume the same runtime type
   descriptor

## Required proof

1. serializer round-trip for every Beta 2 family
2. executor comparison and arithmetic proof for numeric and temporal families
3. selector and setter proof for every setter-capable family
4. fail-closed behavior for unsupported operations on opaque catalog payloads
