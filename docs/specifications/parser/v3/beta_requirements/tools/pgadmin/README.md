# Pgadmin Integration Requirements

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Scope**
- Define the compatibility contract for Pgadmin integration.

**Compatibility Requirements**
- **Protocol:** Must work over the supported wire protocol(s) for the selected dialect (PostgreSQL v3, MySQL, or Firebird) as defined in `docs/specifications/parser/v3/network/WIRE_PROTOCOL_SPECIFICATIONS.md`.
- **Metadata Introspection:** Must return stable results for catalog/schema/table/column enumeration. Use `information_schema`/system catalog views defined in `docs/specifications/parser/v3/catalog/`.
- **Type Mapping:** All columns must map to stable, deterministic client types using the canonical type map in `docs/specifications/parser/v3/types/`.
- **Prepared Statements:** Must support bind parameters, consistent parameter typing, and correct execution plan caching semantics.
- **Transactions:** Must honor MGA visibility rules (`docs/specifications/parser/v3/transaction/TRANSACTION_MGA_CORE.md`) and lock ordering (`docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`).
- **Errors:** Must return structured error codes with SQLSTATE where available; error text must not leak internal details.

**Required Behaviors**
- **Schema Browsing:** All system schemas are visible but read-only. User schemas are writable.
- **Explain/Analyze:** The tool must be able to request plan text. The server must accept and respond per `docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md` session opcodes.
- **Large Results:** Server must stream results in chunks; client cancellation must terminate queries promptly.

**Conformance Tests**
- Connect, authenticate, list schemas, list tables, fetch table columns.
- Create a schema, create a table, insert rows, query rows, drop objects.
- Prepare a statement with parameters, execute with multiple parameter sets.
- Run a transaction with concurrent reads and writes and confirm MGA visibility.

**Observability**
- Metrics for query count, latency, and result sizes must be exposed via `docs/specifications/parser/v3/operations/` views.

**Non-Goals**
- This document does not replace core engine specifications in `docs/specifications/parser/v3/`.
- This document does not define new SQL syntax beyond the dialect specifications.


