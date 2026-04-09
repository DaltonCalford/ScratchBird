# Section 15 Complex Types

Status: current_authority

## Current authoritative surface

Section 15 is anchored to the live complex-type and domain runtime.

The current code-backed authority is split across:
- TypeSystem for emulated complex-type resolution, mutation-boundary hints, and TOAST eligibility
- TypedValue for plain-value carriers and durable and plain serialization of ARRAY, LIST, MAP, COMPOSITE, ROW, VARIANT, and bounded binary-backed families such as JSONB, BSON, TAGGED_UNION, and DICT_ENCODED
- DomainManager for record, enum, set, variant, range, base, shell, and system-domain control-plane behavior
- extract_element_ops for the real selector vocabulary on ARRAY, COMPOSITE, VARIANT, and JSON and XML-adjacent complex values

## Main implementation corrections captured here

- the live DataType and TypeSystem surface covers more complex families than older prose admitted
- TypedValue already owns real container, composite, and variant serialization logic
- DomainManager already implements record, enum, set, variant, range, base, shell, and deterministic system-domain behavior
- extract-selector support for ARRAY, COMPOSITE, and VARIANT is materially richer than older generic accessor prose

## Most important fail-closed boundaries

- exact universal SQL front-door grammar for every domain kind is not claimed here
- exact binary compatibility with PostgreSQL array, composite, jsonb, or external BSON formats is not claimed here
- the historical exhaustive system-domain UUID registry is not treated as fully re-audited current authority
- MAP, SET, and several emulated complex families are still represented through shared carriers or domain wrappers rather than one separately closed durable payload contract per family

## Direct audit lookup anchors

- `src/core/domain_manager.cpp` search key `DomainManager::createRecordDomain(`
- `src/core/domain_manager.cpp` search key `DomainManager::createVariantDomain(`
- `src/core/domain_manager.cpp` search key `DomainManager::validateValue(`

<!-- AUTO-GENERATED:FILE-LIST:START -->
- [BETA2_COMPLEX_DOCUMENT_VECTOR_AND_CATALOG_TYPE_EXPANSION_MODEL.md](BETA2_COMPLEX_DOCUMENT_VECTOR_AND_CATALOG_TYPE_EXPANSION_MODEL.md)
- [COMPLEX_STORAGE_FORMAT.md](COMPLEX_STORAGE_FORMAT.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [DOMAIN_DDL_AND_CATALOG.md](DOMAIN_DDL_AND_CATALOG.md)
- [DOMAIN_EMULATION_PARAMETERS.md](DOMAIN_EMULATION_PARAMETERS.md)
- [EMULATED_COMPLEX_TYPE_MATRIX.md](EMULATED_COMPLEX_TYPE_MATRIX.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [SYSTEM_DOMAIN_UUID_REGISTRY.md](SYSTEM_DOMAIN_UUID_REGISTRY.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
- [TYPE_IO_AND_ERROR_SEMANTICS.md](TYPE_IO_AND_ERROR_SEMANTICS.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->
