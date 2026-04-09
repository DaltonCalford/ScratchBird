# V3 EBNF Parser-Accurate Rewrite Parity Review

Timestamp: `20260310T131501Z`

## Scope

This review records the verification pass for the new parser-accurate EBNF file:

- new file: `/home/dcalford/CliWork/ScratchBird-v3-Parser-Accurate-EBNF.md`
- parser source of truth: `src/parser/parser_v3.cpp`
- schema-path source of truth: `src/parser/schema_path_v3.cpp`

The earlier hallucination-preservation report remains:

- `docs/specifications/work/findings/V3_EBNF_PARSER_PARITY_HALLUCINATED_PATHS_REPORT_20260310T120313Z.md`

## Verification Performed

### 1. Top-level route coverage

A mechanical symbol-presence check was run against the replacement file for the
live top-level parser surfaces identified from `parseStatementInternal()`.

Result:

- `missing_count = 0`

Covered route families included:

- `WITH`
- `RECREATE`
- `CREATE`
- `ALTER`
- `DROP`
- `TRUNCATE`
- `DECLARE EXTERNAL FUNCTION`
- `DOC PATH FILTER`
- `TS BUCKET AGG`
- `SEARCH` / `JOIN` / `PERCOLATOR`
- `VECTOR ANN QUERY`
- `GRAPH PATH`
- `REDIS`
- `HYBRID BRIDGE`
- `UDR COMPILE/VALIDATE`
- `INSTALL` / `LOAD EXTENSION`
- `RESYNC REPLICATION CHANNEL`
- `BACKUP` / `RESTORE` / `VALIDATE DATABASE` / `CHECKPOINT`
- `REFRESH CUBE`
- `CLUSTER`
- `CUBE`
- `SERVICE CHANNEL`
- `SELECT`
- `INSERT`
- `UPDATE OR INSERT`
- `UPDATE`
- `DELETE`
- `COPY`
- transaction/session statements
- `SET` / `SHOW` / `RESET` / `DESCRIBE`
- `SECURITY LABEL`
- `EXPLAIN`
- `ANALYZE`
- `VALIDATE INDEX`
- `SWEEP DATABASE`
- `CANCEL JOB RUN`
- `EXECUTE JOB`
- `EXECUTE`
- `CALL`
- `GRANT`
- `REVOKE`
- `REVOKE TOKEN`
- `CONNECT`
- `DISCONNECT`
- `COMMENT`
- `MERGE`
- PSQL statement family

### 2. Placeholder/non-parser production scan

The replacement file was scanned for the undefined placeholder production names
that existed in the old EBNF.

Result:

- no occurrences found for:
  - `search_specification`
  - `vector_specification`
  - `graph_specification`
  - `stream_command`
  - `nosql_operation`
  - `bridge_specification`
  - `cluster_operation`
  - `show_specification`
  - `cube_operation`
  - `service_operation`

### 3. Hallucinated-path carryover scan

The replacement file was checked to ensure previously confirmed non-parser
surfaces were not reintroduced as active grammar. These legacy names still
appear only in the explicit rejection section, which is intentional.

Checked legacy drift items:

- `VACUUM`
- `CREATE/ALTER/DROP SCHEDULE`
- `CREATE MATERIALIZED VIEW`
- `DROP MATERIALIZED VIEW`
- `REINDEX`
- `REFRESH MATERIALIZED VIEW`
- `LISTEN`
- `NOTIFY`
- `UNLISTEN`
- `FROM DUAL`
- `PIVOT`
- `UNPIVOT`
- `CROSS APPLY`
- `OUTER APPLY`
- `START WITH ... CONNECT BY`
- `MINUS`
- `INSERT IGNORE`
- `ON DUPLICATE KEY UPDATE`

## Mechanical Modeling Notes

The replacement file uses explicit raw-capture meta symbols where the live
parser does not parse a richer inner grammar and instead stores text or token
fragments verbatim.

These include:

- `<raw-tail>`
- `<nonempty-raw-tail>`
- `<raw-parenthesized-payload>`
- `<raw-nonempty-parenthesized-payload>`
- `<raw-fragment>`

This is deliberate and parser-faithful. Those regions were not expanded into
invented subgrammars.

## Remaining Review Guidance

The new file is parser-accurate at the route and helper-grammar level used for
this rewrite. Review should focus on:

- whether any prose note should be promoted into a stricter grammar note
- whether additional parent-qualified-path warnings should be added alongside
  `INDEX`, `TRIGGER`, and related object families
- whether the new file should replace the old root EBNF path after human review

No evidence found in this pass that the replacement file reintroduced the
previous placeholder families or the previously documented hallucinated
top-level paths as accepted grammar.
