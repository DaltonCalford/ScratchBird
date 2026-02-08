# MySQL Parser Alpha Tracker

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Source audit:** `docs/findings/EMULATED_PARSER_FULL_AUDIT_2026-02-02.md`

## Alpha Blockers
- [x] ON DUPLICATE KEY UPDATE emits executable bytecode (remap to MERGE or ON CONFLICT). (`/docs/specifications/parser/v3/MYSQL_PARSER_IMPLEMENTATION_GAPS.md:90-150`).
- [x] TEMPORARY TABLE semantics (not silently permanent). (`/docs/specifications/parser/v3/MYSQL_PARSER_IMPLEMENTATION_GAPS.md:38-78`).

## Progress Notes
- 2026-02-02: Emit explicit ON CONFLICT constraint target marker for ON DUPLICATE KEY UPDATE in `src/parser/mysql/mysql_parser.cpp:2360-2395`.
- 2026-02-02: Verified TEMPORARY tables emit CREATE_TABLE flags and executor honors temp_type in `src/parser/mysql/mysql_parser.cpp:3404-3475` and `src/sblr/executor.cpp:5437-6210`.
- 2026-02-02: Executor falls back to all unique constraints when no ON CONFLICT target is specified, matching MySQL ON DUPLICATE semantics (`src/sblr/executor.cpp:13697-13780`).
- 2026-01-28: Implemented INSERT IGNORE mapping and ALTER TABLE ADD/DROP INDEX emission in `src/parser/mysql/mysql_parser.cpp`.
- 2026-01-28: Added explicit INSERT modifier handling and ALTER TABLE ALTER COLUMN diagnostics in `src/parser/mysql/mysql_parser.cpp`.

## High Priority (Alpha)
- [x] INSERT IGNORE -> ON CONFLICT DO NOTHING mapping. (`/docs/specifications/parser/v3/MYSQL_PARSER_IMPLEMENTATION_GAPS.md:246-266`).
- [x] INSERT modifiers (LOW_PRIORITY/DELAYED/HIGH_PRIORITY/IGNORE) explicit behavior (accept+ignore or reject). (`src/parser/mysql/mysql_parser.cpp:1982-1985`).
- [x] ALTER TABLE ADD/DROP INDEX support or explicit rejection in spec/tests. (`src/parser/mysql/mysql_parser.cpp:3282-3298`).
- [x] ALTER TABLE ALTER COLUMN support or explicit rejection. (`src/parser/mysql/mysql_parser.cpp:3349`).

## optional extension
- [ ] Table options coverage (ROW_FORMAT, KEY_BLOCK_SIZE, ENGINE, etc.). (`/docs/specifications/parser/v3/MYSQL_PARSER_IMPLEMENTATION_GAPS.md:431-557`).
- [ ] Partition options support or explicit error. (`src/parser/mysql/mysql_parser.cpp:3781`).
