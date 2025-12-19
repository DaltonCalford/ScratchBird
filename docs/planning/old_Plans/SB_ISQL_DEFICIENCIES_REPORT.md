# sb_isql Deficiencies Report
## Comparison with Firebird ISQL (v1.4, May 2024)

**Date:** 2025-12-06
**Current sb_isql:** ~845 lines
**Reference:** `/docs/specifications/firebird-isql.pdf` (75 pages)

---

## Executive Summary

The current sb_isql implementation provides basic interactive SQL capabilities but lacks significant features present in Firebird's mature isql utility. This report identifies **115+ missing features** across 7 categories, prioritized for implementation.

---

## 1. Command-Line Switches

### Currently Implemented in sb_isql
| Switch | Description |
|--------|-------------|
| `-U` | Username |
| `-P` | Password (visible) |
| `-p` | Port |
| `-H` | Host |
| `-c` | Execute single command |
| `-f` | Input file |
| `-o` | Output file |
| `-t` | Tuples only |
| `-A` | Unaligned output |
| `-F` | Field separator |
| `-q` | Quiet mode |
| `-e` | Echo input |
| `-v` | Verbose |
| `-h, --help` | Help |
| `--version` | Version |

### Missing from Firebird isql (Priority: HIGH)

| Switch | Description | Priority |
|--------|-------------|----------|
| `-a` | Extract DDL (all objects) | HIGH |
| `-x` | Extract DDL (no data) | HIGH |
| `-ex` | Extract DDL with CREATE DATABASE | HIGH |
| `-b` | Bail on first error | HIGH |
| `-m` | Merge stderr into stdout | MEDIUM |
| `-m2` | Merge with distinct prefixes | MEDIUM |
| `-n` | Suppress readline | MEDIUM |
| `-nod` | No database triggers | MEDIUM |
| `-now` | No auto-wrap long lines | LOW |
| `-pag N` | Pagination (N lines per page) | MEDIUM |
| `-r` | Role name | HIGH |
| `-r2` | Trusted role | MEDIUM |
| `-s N` | SQL dialect (1, 2, or 3) | HIGH |
| `-t` | Terminator character | MEDIUM |
| `-tr` | Transaction options | HIGH |
| `-z` | Display version info | LOW |
| `-ch` | Character set | MEDIUM |
| `-i` | Input file (Firebird style) | LOW (already have -f) |
| `-d` | Database path | LOW (positional) |

---

## 2. SET Commands

### Currently Implemented in sb_isql
- `\set` - Display settings (partial)
- `\pset` - Set output format options
- `\timing` - Toggle timing display

### Missing SET Commands (Priority: HIGH)

| Command | Description | Priority |
|---------|-------------|----------|
| `SET AUTODDL [ON\|OFF]` | Auto-commit DDL statements | HIGH |
| `SET BAIL [ON\|OFF]` | Stop on first error | HIGH |
| `SET BLOB [ALL\|N]` | BLOB display mode | MEDIUM |
| `SET BLOBDISPLAY [N]` | Subtype for BLOB display | MEDIUM |
| `SET COUNT [ON\|OFF]` | Display row counts | HIGH |
| `SET ECHO [ON\|OFF]` | Echo input commands | HIGH |
| `SET EXPLAIN [ON\|OFF]` | Show execution plan details | HIGH |
| `SET HEADING [ON\|OFF]` | Show column headings | HIGH |
| `SET KEEP_TRAN_PARAMS [ON\|OFF]` | Preserve transaction parameters | MEDIUM |
| `SET LIST [ON\|OFF]` | Vertical display mode | MEDIUM |
| `SET LOCAL_TIMEOUT N` | Statement timeout (seconds) | MEDIUM |
| `SET MAXROWS N` | Limit rows returned | MEDIUM |
| `SET NAMES charset` | Set client character set | HIGH |
| `SET PER_TABLE_STATS [ON\|OFF]` | Per-table statistics | MEDIUM |
| `SET PLAN [ON\|OFF]` | Show query plan | HIGH |
| `SET PLANONLY [ON\|OFF]` | Show plan without executing | HIGH |
| `SET SQLDA_DISPLAY [ON\|OFF]` | Show SQLDA info | LOW |
| `SET SQL DIALECT N` | Set SQL dialect (1, 2, 3) | HIGH |
| `SET STATS [ON\|OFF]` | Show query statistics | HIGH |
| `SET TIME [ON\|OFF]` | Display current time in prompt | LOW |
| `SET TERM string` | Change statement terminator | HIGH |
| `SET TRANSACTION ...` | Set transaction parameters | HIGH |
| `SET WARNINGS [ON\|OFF]` | Display warnings | MEDIUM |
| `SET WIDTH col N` | Set column display width | MEDIUM |
| `SET WNG [ON\|OFF]` | Alias for WARNINGS | LOW |

---

## 3. SHOW Commands

### Currently Implemented in sb_isql
| Command | sb_isql Equivalent |
|---------|-------------------|
| `\d` | Describe table |
| `\dt` | List tables |
| `\di` | List indexes |
| `\du` | List users |
| `\l` | List databases |

### Missing SHOW Commands (Priority: HIGH)

| Command | Description | Priority |
|---------|-------------|----------|
| `SHOW CHECKS [table]` | Show check constraints | HIGH |
| `SHOW COLLATIONS` | Show collation sequences | MEDIUM |
| `SHOW COMMENTS` | Show object comments | MEDIUM |
| `SHOW DATABASE` | Database info (pages, ODS, etc.) | HIGH |
| `SHOW DEPENDENCIES obj` | Object dependencies | MEDIUM |
| `SHOW DOMAIN [name]` | Domain definitions | HIGH |
| `SHOW EXCEPTION [name]` | User exceptions | MEDIUM |
| `SHOW FILTER [name]` | BLOB filters | LOW |
| `SHOW FUNCTION [name]` | User-defined functions | HIGH |
| `SHOW GENERATOR [name]` | Generators/sequences | HIGH |
| `SHOW GRANTS obj` | Object privileges | HIGH |
| `SHOW INDEX [name]` | Index details | HIGH |
| `SHOW MAPPING [name]` | Security mappings | LOW |
| `SHOW PROCEDURE [name]` | Stored procedures | HIGH |
| `SHOW PACKAGE [name]` | Package definitions | MEDIUM |
| `SHOW PUBLICATION [name]` | Replication publications | LOW |
| `SHOW ROLE [name]` | Role definitions | MEDIUM |
| `SHOW SECCLASS [name]` | Security classes | LOW |
| `SHOW SQL DIALECT` | Current SQL dialect | HIGH |
| `SHOW SYSTEM` | System tables/views | MEDIUM |
| `SHOW SCHEMA [name]` | Schema definitions | MEDIUM |
| `SHOW TABLE [name]` | Table structure (enhanced) | HIGH |
| `SHOW TRIGGER [name]` | Trigger definitions | HIGH |
| `SHOW VERSION` | Server version info | HIGH |
| `SHOW VIEW [name]` | View definitions | HIGH |

---

## 4. Special Commands

### Currently Implemented in sb_isql
| Command | Description |
|---------|-------------|
| `\q` | Quit |
| `\i file` | Include/execute file |
| `\o file` | Output to file |
| `\!` | Shell escape |
| `\echo` | Echo text |
| `\c` | Connect to database |
| `\x` | Toggle expanded display |

### Missing Commands (Priority: MEDIUM)

| Command | Description | Priority |
|---------|-------------|----------|
| `BLOBDUMP id file` | Dump BLOB to file | MEDIUM |
| `BLOBVIEW id` | View BLOB contents | MEDIUM |
| `EDIT [file]` | Edit command/file in $EDITOR | MEDIUM |
| `INPUT file` | Execute SQL from file | LOW (have \i) |
| `OUTPUT [file]` | Redirect output | LOW (have \o) |
| `SHELL [cmd]` | Execute shell command | LOW (have \!) |
| `ADD name value` | Add row (deprecated, interactive) | LOW |
| `COPY src TO dest` | Copy data between tables/DBs | MEDIUM |
| `EXIT [N]` | Exit with optional code | HIGH |
| `QUIT` | Quit (immediate, no commit) | HIGH |
| `WHENEVER ERROR ...` | Error handling | MEDIUM |
| `WHENEVER SQLERROR ...` | SQL error handling | MEDIUM |

---

## 5. SQL Dialect Support

### Current Status: NOT IMPLEMENTED

Firebird supports three SQL dialects for backward compatibility:

| Dialect | Description |
|---------|-------------|
| 1 | InterBase 5.5 compatibility (double quotes as strings) |
| 2 | Transitional (warns about dialect differences) |
| 3 | Full Firebird SQL (double quotes for identifiers) |

**Required Implementation:**
- Command-line switch `-s N` to set initial dialect
- `SET SQL DIALECT N` command
- `SHOW SQL DIALECT` command
- Dialect-aware parsing/execution

**Priority: HIGH** - Essential for Firebird compatibility

---

## 6. Transaction Handling

### Current Status: MINIMAL

### Missing Features (Priority: HIGH)

| Feature | Description |
|---------|-------------|
| `SET TRANSACTION` | Full transaction parameter support |
| Isolation levels | READ COMMITTED, SNAPSHOT, SNAPSHOT TABLE STABILITY |
| Lock resolution | WAIT, NO WAIT, LOCK TIMEOUT |
| Access mode | READ ONLY, READ WRITE |
| Table reservations | SHARED, PROTECTED, READ, WRITE |
| `COMMIT [WORK] [RETAIN]` | Commit with optional retain |
| `ROLLBACK [WORK] [RETAIN]` | Rollback with optional retain |
| `SAVEPOINT name` | Named savepoints |
| `RELEASE SAVEPOINT name` | Release savepoint |
| `ROLLBACK TO SAVEPOINT name` | Rollback to savepoint |

---

## 7. Output Formatting

### Currently Implemented
- Aligned/unaligned modes
- Field separator
- Expanded display (\x)
- Tuples-only mode
- Timing display

### Missing Features (Priority: MEDIUM)

| Feature | Description | Priority |
|---------|-------------|----------|
| Column width control | `SET WIDTH col N` | MEDIUM |
| NULL representation | Configurable NULL display | MEDIUM |
| Binary display | Hex/octal for binary data | MEDIUM |
| BLOB handling | Inline display options | MEDIUM |
| Pagination | Page breaks for long output | LOW |
| Header repeat | Repeat headers every N rows | LOW |
| Recordsep | Custom record separator | LOW |
| Border styles | Box drawing variants | LOW |

---

## 8. Missing Utility Features

| Feature | Description | Priority |
|---------|-------------|----------|
| History file | Persistent command history | HIGH |
| Tab completion | Object name completion | HIGH |
| Prompt customization | `SET TIME`, `SET PROMPT` | MEDIUM |
| `\g` and `\G` | Alternative query terminators | LOW |
| `\gexec` | Execute query results as SQL | LOW |
| Variable substitution | $var, :var expansion | MEDIUM |
| `\ir` | Include relative path | LOW |
| `\watch N` | Repeat query every N seconds | MEDIUM |
| `\crosstabview` | Pivot table display | LOW |
| `\copy` | Client-side COPY | MEDIUM |

---

## Implementation Roadmap

### Phase 1: Core Functionality (Priority: HIGH)
**Estimated: 20-30 items**

1. SQL Dialect support (`-s`, `SET SQL DIALECT`, `SHOW SQL DIALECT`)
2. Error handling (`SET BAIL`, `WHENEVER ERROR`)
3. DDL extraction (`-a`, `-x`, `-ex` switches)
4. Transaction commands (`SET TRANSACTION`, `COMMIT RETAIN`, `ROLLBACK`)
5. Essential SET commands (`SET TERM`, `SET COUNT`, `SET PLAN`, `SET STATS`)
6. Essential SHOW commands (`SHOW DATABASE`, `SHOW TABLE`, `SHOW INDEX`, `SHOW TRIGGER`, `SHOW VIEW`, `SHOW PROCEDURE`, `SHOW FUNCTION`, `SHOW DOMAIN`, `SHOW GENERATOR`, `SHOW GRANTS`)
7. Role support (`-r` switch)

### Phase 2: Enhanced Display (Priority: MEDIUM)
**Estimated: 15-20 items**

1. `SET HEADING`, `SET WIDTH`, `SET LIST`
2. `SET BLOB`, `SET BLOBDISPLAY`
3. `BLOBDUMP`, `BLOBVIEW` commands
4. `SET EXPLAIN`, `SET PLANONLY`
5. Pagination (`-pag` switch)
6. Column width control

### Phase 3: Developer Features (Priority: MEDIUM)
**Estimated: 10-15 items**

1. History persistence
2. Tab completion
3. `EDIT` command
4. Variable substitution
5. `SET NAMES` (character set)
6. `SET WARNINGS`

### Phase 4: Advanced Features (Priority: LOW)
**Estimated: 15-20 items**

1. Remaining SHOW commands (COLLATIONS, COMMENTS, DEPENDENCIES, etc.)
2. `COPY` command
3. `\watch` repeat feature
4. Prompt customization
5. Advanced output formats

---

## Summary Statistics

| Category | Firebird isql | sb_isql | Missing |
|----------|---------------|---------|---------|
| Command-line switches | 25+ | 15 | ~12 |
| SET commands | 25+ | 3 | ~22 |
| SHOW commands | 28+ | 5 | ~24 |
| Special commands | 15+ | 8 | ~8 |
| Transaction features | 15+ | 2 | ~13 |
| Output formatting | 15+ | 6 | ~10 |
| **TOTAL** | **125+** | **39** | **~89** |

---

## Recommendations

1. **Immediate Priority**: Implement SQL Dialect support - this is fundamental to Firebird compatibility and affects parsing behavior.

2. **High Value**: The `SET TERM` command is essential for script compatibility, as Firebird scripts routinely change the statement terminator for stored procedure definitions.

3. **User Experience**: Tab completion and command history persistence would significantly improve usability for interactive sessions.

4. **DDL Extraction**: The `-a` and `-x` switches provide critical database documentation and migration capabilities.

5. **Transaction Control**: Full transaction handling is essential for any production-grade SQL client.

---

## Notes

- This report compares against Firebird 5.0 isql documentation
- Some features may already exist in ScratchBird's core engine but lack isql exposure
- Implementation priority should align with ScratchBird's Alpha 2 roadmap
- Consider PostgreSQL psql conventions where they don't conflict with Firebird semantics
