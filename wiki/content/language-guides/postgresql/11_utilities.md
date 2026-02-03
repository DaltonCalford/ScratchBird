# Utilities

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| EXPLAIN | ScratchBird tracked | Planner + executor | Emits ScratchBird plan output with PostgreSQL syntax. |
| ANALYZE | ScratchBird tracked | Statistics manager | Updates ScratchBird stats; maps to ANALYZE semantics. |
| COPY (STDIN/STDOUT) | ScratchBird tracked | COPY subsystem | Streaming supported for CSV/TEXT where configured. |
| COPY (file path) | Restricted | Compatibility layer | Server file access restricted in ScratchBird Alpha. |
| COPY PROGRAM | Unsupported | Compatibility layer | Not allowed in ScratchBird Alpha. |
| LISTEN / NOTIFY | ScratchBird tracked | Notification subsystem | Scoped to ScratchBird server instance. |
| Large objects (LO) | Emulated | LOB storage layer | Mapped to ScratchBird LOB storage. |
| Replication commands | Emulated (metadata-only) | Catalog metadata | CREATE PUBLICATION/SUBSCRIPTION stored when allowed; no streaming replication. |

## COPY

ScratchBird supports PostgreSQL COPY for bulk load/unload with a focus on
STDIN/STDOUT streaming. File-path and PROGRAM variants are restricted.

### Supported Forms

```sql
COPY users (id, email) FROM STDIN;
COPY users (id, email) TO STDOUT;
COPY (SELECT id, email FROM users) TO STDOUT;
```

### Common Options

Accepted options include (case-insensitive):

- `FORMAT csv | text`
- `DELIMITER`
- `NULL`
- `HEADER` (CSV)
- `QUOTE` (CSV)
- `ESCAPE` (CSV)

Options that depend on PostgreSQL storage internals may be ignored or rejected.
Examples: `FREEZE`, `OIDS`, `FORCE_QUOTE`, `FORCE_NOT_NULL`.

### Not Supported

- `COPY ... FROM/TO PROGRAM`
- File-path COPY that reads/writes server filesystem paths
- `FORMAT binary`

## LISTEN / NOTIFY

LISTEN/NOTIFY are exposed for in-server notifications when enabled.

```sql
LISTEN job_events;
NOTIFY job_events, 'job 42 finished';
```

Notes:
- Notifications are scoped to the ScratchBird server instance.
- LISTEN/NOTIFY does not provide cross-cluster broadcast.
- If notification support is disabled, these statements return an error.

## Large Objects (LO)

PostgreSQL large objects are emulated on top of ScratchBird’s LOB storage.

- `pg_largeobject` and related catalogs are exposed as views.
- LO identifiers map to internal LOB IDs.
- File-system access is not supported; large objects are stored in ScratchBird
  data files.

Common LO functions (if enabled): `lo_create`, `lo_open`, `lo_read`, `lo_write`,
`lo_close`, `lo_unlink`.

## Replication-Related Commands

ScratchBird does not implement PostgreSQL streaming replication in Alpha.
Statements such as the following are rejected or treated as metadata-only:

- `CREATE PUBLICATION`
- `CREATE SUBSCRIPTION`
- `CREATE SLOT`

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
