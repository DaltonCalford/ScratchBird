# Emulated Complex Type Matrix

Status: current_authority

## Current authoritative mapping surface

The current code-backed authority for emulated complex types is TypeSystem resolveEmulatedType and the audited rows in the emulation type matrix.

## Capability boundary matrix

| Area | Current authority | Main boundary |
| --- | --- | --- |
| row resolution | audited engine and type rows resolve correctly | row presence is not parity proof |
| storage kind | current storage_kind rows are real | storage mapping is not parity proof |
| canonical type | current canonical_type rows are real | canonical type is not parity proof |
| domain hint | current domain_hint rows are real | hint exposure is not full runtime parity |
| whole-value update hint | requiresWholeValueUpdate is real | bounded helper truth only |
| element-level mutation hint | allowsElementLevelMutation is real | bounded helper truth only |
| broader donor-engine semantics | not claimed here | fail closed |

## Directly audited matrix breadth

The audited live matrix includes complex-family rows for at least PostgreSQL, MySQL, Cassandra, Milvus, MongoDB, Neo4j, Redis, ClickHouse, and OpenSearch families listed in current source.

## Fail-closed boundary

The matrix does not currently prove:
- parser parity
- operator parity
- exact donor-engine binary parity
- exact DDL front-door parity
- complete nested update semantics for every mapped engine or type
