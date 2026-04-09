# Section 15 Complex Types Specification Outline

Status: current_authority

## Current section structure

1. COMPLEX_STORAGE_FORMAT.md
   - TypedValue plain-value framing for ARRAY, LIST, MAP, COMPOSITE, ROW, VARIANT, and bounded binary-backed complex-adjacent families
2. TYPE_IO_AND_ERROR_SEMANTICS.md
   - runtime conversion, corruption, selector, and domain-operation boundaries for complex values
3. EMULATED_COMPLEX_TYPE_MATRIX.md
   - TypeSystem resolveEmulatedType mapping scope and mutation-boundary hints
4. DOMAIN_DDL_AND_CATALOG.md
   - DomainManager control-plane and catalog persistence behavior for record, enum, set, variant, range, base, shell, and system domains
5. DOMAIN_EMULATION_PARAMETERS.md
   - current structured domain options and mapping hints
6. SYSTEM_DOMAIN_UUID_REGISTRY.md
   - deterministic system-domain ID mechanism and audited bootstrap examples
7. DEPENDENCIES.md
   - upstream and downstream subsystem boundaries
8. TEST_CONTRACT.md
   - direct proof surfaces and open gaps
9. DECISION_RECORD.md
   - explicit implementation-vs-prose drift and section decisions

## Boundary notes

- section 15 is not the authority for generic scalar serialization; that remains in section 14
- section 15 is not the authority for TOAST retention or reclaim; that remains in sections 02, 10, and 11
- section 15 is not the authority for parser-only CREATE TYPE or engine-wide operator semantics; those boundaries stay with sections 13, 21, and 22
