# Section 15 Test Contract

Status: current_authority

## Direct proof surfaces audited in this pass

- tests/unit/test_domain_opcodes.cpp
  - domain control-plane and opcode-level behavior
- tests/unit/test_type_mapping.cpp
  - emulated type-mapping rows and mapping boundaries
- tests/unit/test_type_serialization.cpp
  - array, composite, variant, geometry, and other complex-value round-trip closure
- tests/integration/test_domain_validation.cpp
  - domain validation success and fail-closed update behavior
- tests/integration/test_domain_quality.cpp
  - quality pipeline application and returned metadata
- tests/integration/test_domain_integrity.cpp
  - uniqueness, normalization, and update behavior
- tests/integration/test_domain_security.cpp
  - masking, privilege bypass, and audit behavior
- tests/integration/test_domain_encryption.cpp
  - encrypted-at-rest domain flow
- tests/integration/test_domain_e2e_scenarios.cpp
  - end-to-end multi-domain workflows and inheritance cases

## Current section-level proof state

Directly supported by current audited code and tests:
- domain control-plane existence and catalog-backed behavior
- validation, normalization, masking, encryption, quality, and integrity workflows
- array, range, network, text-search, composite, variant, and geometry value round-trip or mapping behavior
- emulated-type mapping presence and bounded resolution truth
- shared conversion and error behavior inherited from TypedValue

## Fail-closed test boundary

This section does not claim direct proof for:
- one full SQL DDL suite for every domain kind
- exact donor-engine binary-compatibility gates for arrays, composites, jsonb, or bson
- every static system-domain UUID row from the historical registry file

## Beta 2 required proof additions

The Beta 2 datatype expansion is not certified unless evidence covers:

- native multirange serialization, comparison, extract, and setter behavior
- typed-list non-null element enforcement
- BSON or document wrapper constructor, selector, and round-trip behavior
- vector layout descriptor enforcement for dense, sparse, binary, and quantized
  families
- opaque PostgreSQL catalog payload wrapper decode, encode, and fail-closed
  unsupported-operation behavior
