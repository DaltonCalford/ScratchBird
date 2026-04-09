# Section 16 Specification Outline

## Owned topics

1. ConnectionContext session and identity state
2. Generic session-variable storage and lookup
3. Current schema and search-path state
4. Bounded SHOW variable inventory
5. Unknown SHOW fail-closed behavior
6. Internal row and trigger context boundary
7. Transaction and statement context exposure boundary

## Canonical section files

1. CONTEXT_VARIABLES_NORMATIVE_IMPLEMENTATION.md
2. DECISION_RECORD.md
3. DEPENDENCIES.md
4. TEST_CONTRACT.md

## Required guarantees

- connection-context state remains the current source of session identity and schema-path state
- SHOW remains explicitly bounded to the names proved in executor behavior
- unknown SHOW names fail closed
- generic session-variable storage is string-key and string-value oriented unless a stronger typed authority is introduced
- transaction and statement context in this section remain bounded to current operator-visible exposure only

## Explicit non-guarantees

- no universal typed registry is claimed
- no catalog-backed alias inventory is claimed
- no public ROW.NEW or ROW.OLD variable syntax is claimed
