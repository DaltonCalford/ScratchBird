# JDBC Driver Requirements

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Scope**
- JDBC driver contract for ScratchBird.

**Driver Requirements**
- **JDBC URL:** `jdbc:scratchbird://<host>:<port>/<database>?param=value`.
- **Authentication:** Username/password; optional TLS.
- **Metadata:** `DatabaseMetaData` must enumerate catalogs, schemas, tables, columns, indexes, and constraints.
- **Type Mapping:** Java types must map deterministically to ScratchBird types (see `docs/specifications/parser/v3/types/`).

**Statement Semantics**
- PreparedStatement must bind parameters with explicit type inference.
- Batch execution must be atomic when executed in a transaction.
- Generated keys must be returned when supported by the selected dialect.

**Transactions**
- Auto-commit must map to implicit transactions.
- Isolation levels must map to MGA semantics (`docs/specifications/parser/v3/transaction/TRANSACTION_MGA_CORE.md`).

**Conformance Tests**
- JDBC CTS-like suite: metadata, DDL, DML, transactions, batch, error handling.

**Non-Goals**
- This document does not replace core engine specifications in `docs/specifications/parser/v3/`.
- This document does not define new SQL syntax beyond the dialect specifications.


