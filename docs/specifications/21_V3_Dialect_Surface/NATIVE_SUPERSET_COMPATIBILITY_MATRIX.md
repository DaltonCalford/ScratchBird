# Native Superset Compatibility Matrix

## Current code-backed truth
- Native parser and emulated parser families exist.
- Builtin emulation package scaffolds are real for PostgreSQL, MySQL, and FirebirdSQL.
- MySQL and Firebird parser code paths exist, and PostgreSQL parser family scaffolding is present in the canonical parser tree.

## Capability-state matrix
- `supported`:
  - emulated parser family presence
  - builtin scaffold inventory for `postgresql`, `mysql`, `firebirdsql`
- `partial`:
  - donor semantic parity
  - bundle lifecycle full closure
- `fail_closed`:
  - any claim that parser-family presence proves full engine-semantic parity

## Boundary
- This matrix must be read as a bounded parser-family and compatibility-inventory surface.
- It is not proof of complete semantic parity across all donor dialects.
- Exact compatibility closure remains a contradiction-decomposition task.
