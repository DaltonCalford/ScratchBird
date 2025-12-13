# Domain Implementation Audit & Gaps

Context: Current code supports basic domains (type aliases with constraints) persisted in catalog. Requested capabilities include complex structured domains with computed fields, element-level EXTRACT/SET, and explicit cast definitions. These features are not present in the current implementation.

## Current (observed)
- Domains stored in catalog (domains table) with base type/constraints.
- No structured/record-style domains (only scalar type + constraints).
- No computed members inside domains.
- No domain-level custom cast definitions.
- EXTRACT/SET on domain members not supported (domains treated as scalars).

## Requested Features (missing)
- Structured domain definition (record-like): e.g., `CREATE DOMAIN NAME_AGE_BIRTHDATE (FIRST_NAME VARCHAR(50), MIDDLE_NAME VARCHAR(50), LAST_NAME VARCHAR(50), AGE INTEGER CALCULATED BY (NOW-BIRTHDATE), BIRTHDATE DATETIMETZ)`.
- Computed fields inside domain (AGE derived from BIRTHDATE).
- EXTRACT semantics on domain members (`EXTRACT FIRST_NAME FROM NAME_AGE_BIRTHDATE` etc.) and SET to assign members.
- Domain-level cast definitions (to/from standard types), e.g. custom `CAST DOMAIN TO VARCHAR` with user-specified expression and reverse casts.
- Catalog persistence of cast rules per domain (to/from target types).
- Validation that computed members remain consistent on updates (enforced expression).

## Work Items to Close Gaps
1) **Structured Domain Model**  
   - Extend domain metadata to hold field list (names, types, computed expressions, defaults).  
   - Store computed expressions in catalog (TOAST) and validate on insert/update.
2) **Parser/AST/Semantic**  
   - Grammar for structured domain fields and computed-by expressions.  
   - EXTRACT/SET syntax for domain fields.  
   - Cast specification syntax at domain definition (TO/ FROM type).  
3) **Evaluator/Execution**  
   - Represent domain values as composites internally; add assignment/extract ops.  
   - Enforce computed fields on write; recompute derived fields.  
4) **Casts**  
   - Catalog entries for domain-specific casts (to/from).  
   - Execution support to invoke custom cast expressions; fallback to defaults when undefined.  
5) **Dependency Tracking**  
   - Link domains to referenced types/expressions/functions; block drop when depended upon.  
6) **Wire/Storage**  
   - Serialization for structured domain values (likely piggyback on composite/record encoding).  
   - Adapter mapping (ScratchBird only; emulated dialects remain unchanged).  
7) **Tests**  
   - Domain create/alter/drop with structured fields.  
   - Computed field enforcement on insert/update.  
   - EXTRACT/SET behavior per field.  
   - Custom cast round-trips and error cases.  
