# TODO: CDC/ETL Support via Transaction Stamps

Goal: Provide change data capture for ETL/TEL/ELT based on transaction stamps, allowing agents to query changes since a given transaction ID up to current.

## Concept
- Every modification is transaction-stamped; use this to expose changes between txn ranges.
- Table-level flag to mark tables for CDC so agents can limit scope.

## Requirements
- Expose per-row or per-change transaction IDs (xmin/xmax equivalent) in a queryable form for CDC-enabled tables.
- Provide a CDC API/view to list changes since txn X (inclusive/exclusive options) up to “now”.
- Allow table-level opt-in/flags for CDC visibility.
- Include operation type (insert/update/delete), primary key, modified columns, and commit timestamp if available.
- Respect transaction isolation: only committed changes; no uncommitted exposure.
- Security: only authorized roles can query CDC streams.

## Work Items
- Define CDC catalog metadata (table flag, last-seen tx per agent optional).
- Add views/functions to stream changes by txn range, filtered by table flag.
- Ensure executor exposes commit-stamped visibility safely; avoid scanning uncommitted versions.
- Tests: multi-transaction updates/inserts/deletes; agent polling; security checks.
- Documentation: usage patterns, performance considerations, retention/cleanup strategy.
