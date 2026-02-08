# Cypher Opencypher ORM Compatibility Requirements

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Scope**
- ORM compatibility contract for Cypher Opencypher.

**Required SQL Features**
- **DDL:** CREATE/ALTER/DROP TABLE, INDEX, VIEW, SEQUENCE.
- **DML:** SELECT/INSERT/UPDATE/DELETE with predicates, joins, and ORDER BY.
- **Transactions:** BEGIN/COMMIT/ROLLBACK with MGA semantics.
- **Upserts:** Either `ON CONFLICT` (Postgres mode) or `ON DUPLICATE KEY` (MySQL mode) must be supported in the selected dialect.
- **Identity/Sequence:** Auto-generated keys must be supported via sequences or identity columns.

**Reflection/Introspection**
- ORM schema reflection must work using system catalog views.
- Columns must expose nullability, default expressions, and constraints.

**Parameter Binding**
- Named and positional parameters must be accepted; types must be inferred deterministically.

**Conformance Tests**
- Run the ORM’s migration generator to create schema and apply migrations.
- Insert/read/update/delete across multiple entities in a single transaction.
- Validate FK constraints and cascade behaviors.

**Non-Goals**
- This document does not replace core engine specifications in `docs/specifications/parser/v3/`.
- This document does not define new SQL syntax beyond the dialect specifications.


