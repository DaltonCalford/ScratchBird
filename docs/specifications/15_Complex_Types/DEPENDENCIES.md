# Section 15 Complex Types Dependencies

Status: current_authority

## Upstream dependencies

- section 07 catalog bootstrap and UUID mapping
- section 11 TOAST and oversized-value storage
- section 14 base scalar types
- section 22 SBLR canonical model and opcodes
- section 24 catalog model and virtual overlays

## Primary live code authorities

- src/core/type_system.cpp
- include/scratchbird/core/typed_value.h
- src/core/typed_value.cpp
- include/scratchbird/core/domain_manager.h
- src/core/domain_manager.cpp
- src/sblr/extract_element_ops.cpp
- tests/unit/test_domain_opcodes.cpp
- tests/unit/test_type_mapping.cpp
- tests/unit/test_type_conversions.cpp

## Downstream consumers

- section 13 operator and coercion rules consume complex conversion and bounded cast truth
- sections 21 and 22 consume domain and extract-selector capability boundaries
- section 24 consumes domain catalog and system-domain bootstrap truth

## Fail-closed boundaries

- this section does not prove one universal SQL DDL front door for every domain kind
- this section does not prove full donor-engine semantic parity for every emulated complex row
- this section does not own TOAST retention, reclaim, or restart cleanup semantics
