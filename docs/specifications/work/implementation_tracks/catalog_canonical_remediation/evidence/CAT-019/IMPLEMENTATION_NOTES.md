# Implementation Notes

Status: `Completed`

## Completed in this pass
- Added CAT-019 catalog root fields and persistence wiring:
  - `connection_page`
  - `transaction_page`
- Added bootstrap allocation and legacy backfill allocation for both runtime context families.
- Added on-disk record contracts in `CatalogManager`:
  - `RuntimeConnectionRecord`
  - `RuntimeTransactionRecord`
- Added full CAT-019 CRUD/public APIs for:
  - `connection`
  - `transaction`
- Enforced deterministic constraints:
  - `connection`: strict protocol whitelist (`native`, `postgresql`, `mysql`, `firebird`, `cassandra`, `mongodb`, `neo4j`, `redis`, `milvus`) and strict enum validation for transport/auth method.
  - `transaction`: strict enum validation for emulation engine/isolation/state.
  - `transaction`: `state=IN_PROGRESS` requires `end_time` null, terminal states require non-null `end_time`.
  - `transaction`: non-null `session_uuid` must resolve to a persisted session with non-zero `current_schema_uuid`.
  - `transaction`: non-null `connection_uuid` must resolve to a persisted runtime connection row.
- Added/updated CAT-019 bootstrap persistence and runtime context contract tests.
