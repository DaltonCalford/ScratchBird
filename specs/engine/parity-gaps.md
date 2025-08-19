# Parser Parity Gaps (vs Firebird 6 parse.y)

Status: draft

Purpose: single source of truth for remaining gaps to achieve equal-or-better parser parity with Firebird. When this checklist has no unchecked must-have items, CI should pass the “parity gaps == 0” gate.

Guidelines
- [x] Items checked = implemented and covered by tests/specs.
- [ ] Unchecked items = remaining. Mark Must or Nice in parentheses.
- Keep messages concise; link to specs/tests where helpful.

SELECT
- [x] WITH/CTE (RECURSIVE)
- [x] FROM (tables, subqueries, table functions, LATERAL)
- [x] JOIN tree (NATURAL, USING, ON; grouped/parenthesized joins)
- [x] WHERE / GROUP BY / HAVING (diagnostics for HAVING without GROUP BY)
- [x] WINDOW (PARTITION/ORDER/frames; warnings for RANGE without ORDER BY, invalid bounds)
- [x] DISTINCT / DISTINCT ON (prefix validation vs ORDER BY)
- [x] ORDER BY (ordinals, NULLS FIRST/LAST)
- [x] FETCH FIRST/NEXT ROWS ONLY
- [x] FOR UPDATE (OF cols, NOWAIT/SKIP LOCKED)
- [x] Set ops (UNION/INTERSECT/EXCEPT [ALL])
- [x] PLAN clause (JOIN/NESTED/MERGE/SORT/HASH/UNION; ORDER <index> [INDEX(...)], hints, R marker)

DML
- [x] INSERT (DEFAULT VALUES; VALUES multi-row; INSERT … SELECT)
- [x] UPDATE (FROM; WHERE CURRENT OF)
- [x] DELETE (USING; WHERE CURRENT OF)
- [x] MERGE (multiple WHENs; AND guards; UPDATE/INSERT/DELETE; THEN DO NOTHING)
- [x] UPSERT (UPDATE OR INSERT … MATCHING; diagnostics for mismatched columns)
- [x] RETURNING (normalized capture)

DDL (core)
- [x] TABLE (column defs; NOT NULL; IDENTITY {ALWAYS|BY DEFAULT}; IDENTITY options; COMPUTED BY; GENERATED ALWAYS AS (expr) [VIRTUAL]; constraints PK/UK/CHK/FK with actions; ALTER ops)
- [x] INDEX (columns/expressions; ASC/DESC; per-entry COLLATE; COMPUTED BY; REBUILD; SET STATISTICS)
- [x] SEQUENCE/GENERATOR (START WITH/INCREMENT BY; CYCLE)
- [x] VIEW (columns; WITH [LOCAL|CASCADED] CHECK OPTION; column-count heuristic)
- [x] DOMAIN (AS type; DEFAULT; CHECK; COLLATE; warnings for missing parentheses)

DDL (secondary)
- [x] COLLATION / CHARACTER SET
- [x] EXCEPTION
- [x] COMMENT ON / RENAME TO
- [x] ROLE / USER (attrs accepted raw; optional to structure later)
- [x] UDR / UDF (EXTERNAL NAME/ENGINE)
- [x] MAPPING
- [x] GLOBAL TEMPORARY TABLE
- [x] BLOB FILTER (DECLARE/CREATE/DROP)
- [x] EXTERNAL TABLE attributes (CREATE TABLE … EXTERNAL FILE 'path') (Must)

PSQL
- [x] EXECUTE BLOCK (params/returns; nested blocks)
- [x] DECLARE var/cursor; OPEN/FETCH INTO/CLOSE
- [x] FOR SELECT INTO (var-count vs projection diagnostics)
- [x] IF / WHILE / LEAVE / CONTINUE / CASE
- [x] WHEN … DO / EXCEPTION
- [x] RETURN / SUSPEND
- [x] EXECUTE STATEMENT (basic capture)
- [x] EXECUTE PROCEDURE / CALL (args capture)
- [x] POST EVENT / RAISE
- [x] EXECUTE STATEMENT options matrix (WITH CALLER PRIVILEGES; AS USER/PASSWORD; ON EXTERNAL; BIND/TIMEOUT; INTO variants) (Must)
- [ ] DML … RETURNING … INTO varlist inside blocks (Nice)

Session/Transactions
- [x] CREATE/ALTER/DROP DATABASE options (PAGE_SIZE, DEFAULT CHARSET, DIALECT, files/shadows, page_cache/sweep/reserve_space)
- [x] SET TRANSACTION (isolation, access, wait, record consistency; table reservations; SNAPSHOT TABLE STABILITY)
- [x] SET TIME ZONE / BIND / OPTIMIZE / SEARCH PATH / DEBUG / DECFLOAT ROUND / DECFLOAT TRAPS / SESSION RESET

GRANT/REVOKE
- [x] Object-type allowlists (table/view, procedure/function, sequence, package, exception/domain/generator)
- [x] PUBLIC grantee; WITH GRANT/ADMIN; REVOKE GRANT OPTION FOR; role grants
- [x] Warnings contain object type and name context
- [ ] System privilege enumeration (acceptance is present; optional strict normalization) (Nice)

Types/Expressions & Lexer
- [x] TYPEOF / TYPE OF COLUMN
- [x] Arrays; INT128/UINT128; DECFLOAT
- [x] CHAR/VARCHAR charset & collate modifiers on types (CAST/DECLARE)
- [x] Datetime literals with 4-digit formats, TZ suffixes; ISO T; UUID/BINARY; national strings

Diagnostics/Recovery
- [x] Standardized warnings (concise, stable phrases) and SourceSpan in SELECT/DML; extended to DDL/PSQL
- [x] Clause-level recovery guards; never crash on End; resume after malformed segments

Owners
- EXECUTE STATEMENT options: owner @parser-psql
- External table attributes: owner @parser-ddl
- System privileges strict list: owner @parser-ddl (grant/revoke)
- DML RETURNING INTO in PSQL: owner @parser-psql

CI Gate (TODO)
- Add a CI step that fails when any Must item remains unchecked.
- Proposed: simple script greps this file for lines matching `^- \[ \] .*\(Must\)` and fails if any are found.
