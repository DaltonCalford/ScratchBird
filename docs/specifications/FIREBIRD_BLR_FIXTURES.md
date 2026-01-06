# Firebird BLR Fixtures (for SBLR Mapping Validation)

Status: Draft (compatibility fixtures)

Purpose: provide small, deterministic BLR blobs that a BLR-to-SBLR translator
can consume to validate opcode mapping. Each fixture includes:
- BLR hex (byte order as written)
- Semantics (plain English)
- Expected SBLR (symbolic opcode sequence)
- Status (Supported or GAP)

## Conventions
- Expression fixtures: `blr_version5` + `<expr>` + `blr_eoc`
- Statement fixtures: `blr_version5` + `blr_begin` + `<stmt>` + `blr_end` + `blr_eoc`
- Numeric literals use little-endian payloads after the scale byte.
- `blr_text2` literals encode: `ttype` (USHORT LE), `length` (USHORT LE), bytes.
- `blr_begin` / `blr_end` map to `EXT_BLOCK` / `END` in expected output.
- `blr_parameter` encodes: `message` (UCHAR), `parameter` (USHORT LE).

## Fixtures

### F-EXPR-ADD-INT32
BLR (hex):
```
05 22 15 08 00 01 00 00 00 15 08 00 02 00 00 00 4c
```
Semantics: `1 + 2`
Expected SBLR (symbolic):
```
VERSION(2) EXPR_ADD LITERAL_INT32(1) LITERAL_INT32(2)
```
Status: Supported

### F-EXPR-IS-NULL-PARAM
BLR (hex):
```
05 3d 19 00 00 00 4c
```
Semantics: `:p0 IS NULL` (message 0, parameter 0)
Expected SBLR (symbolic):
```
VERSION(2) EXT_EXPR_IS_NULL PARAM(0)
```
Status: Supported

### F-EXPR-NOT-IS-NULL-PARAM
BLR (hex):
```
05 3b 3d 19 00 00 00 4c
```
Semantics: `NOT (:p0 IS NULL)`
Expected SBLR (symbolic):
```
VERSION(2) EXT_EXPR_NOT EXT_EXPR_IS_NULL PARAM(0)
```
Status: Supported

### F-EXPR-TEXT2-PADDED
BLR (hex):
```
05 15 0f 02 00 05 00 48 49 20 20 20 4c
```
Semantics: `'HI' padded to CHAR(5)` (explicit trailing spaces in literal bytes)
Expected SBLR (symbolic):
```
VERSION(2) LITERAL_STRING("HI   ")
```
Status: Supported

### F-SAVEPOINT-EMPTY
BLR (hex):
```
05 02 86 02 ff 87 ff 4c
```
Semantics: savepoint wrapper around an empty block
Expected SBLR (symbolic):
```
VERSION(2) EXT_BLOCK EXT_SAVEPOINT_BEGIN EXT_BLOCK END EXT_SAVEPOINT_END END
```
Status: Supported

### F-CURSOR-OPEN
BLR (hex):
```
05 02 a7 00 01 00 ff 4c
```
Semantics: `OPEN CURSOR #1`
Expected SBLR (symbolic):
```
VERSION(2) EXT_BLOCK CURSOR_OPEN id=1 END
```
Status: GAP (P1 - cursors)

### F-CURSOR-DECLARE-MINIMAL
BLR (hex):
```
05 02 a6 01 00 43 01 4a 04 54 45 53 54 ff 01 00 15 08 00 01 00 00 00 ff 4c
```
Semantics: `DECLARE CURSOR #1 FOR SELECT 1 FROM TEST`
Expected SBLR (symbolic):
```
VERSION(2) EXT_BLOCK CURSOR_DECLARE id=1 RSE(TEST) SELECT(LITERAL_INT32(1)) END
```
Status: GAP (P1 - cursors)

### F-CURSOR-FETCH-EMPTY
BLR (hex):
```
05 02 a7 02 01 00 02 ff ff 4c
```
Semantics: `FETCH CURSOR #1` (no INTO assignments)
Expected SBLR (symbolic):
```
VERSION(2) EXT_BLOCK CURSOR_FETCH id=1 END
```
Status: GAP (P1 - cursors)

### F-EXEC-STATEMENT-SQL
BLR (hex):
```
05 02 bd 03 15 0f 02 00 08 00 53 45 4c 45 43 54 20 31 ff ff 4c
```
Semantics: `EXECUTE STATEMENT 'SELECT 1'`
Expected SBLR (symbolic):
```
VERSION(2) EXT_BLOCK EXEC_STMT EXEC_STMT_SQL(LITERAL_STRING("SELECT 1")) END
```
Status: GAP (P2 - exec_stmt)

### F-EXEC-STATEMENT-OPTIONS
BLR (hex):
```
05 02 bd 01 02 00 02 01 00 03 15 0f 02 00 08 00 53 45 4c 45 43 54 20 31
05 15 0f 02 00 03 00 44 42 31 06 15 0f 02 00 06 00 53 59 53 44 42 41
07 15 0f 02 00 02 00 70 77 0e 15 0f 02 00 02 00 52 31 09 01 0a
0b 15 08 00 0a 00 00 00 19 00 00 00 0f 01 00 03 00 0d 19 00 01 00
ff ff 4c
```
Semantics: `EXECUTE STATEMENT 'SELECT 1' WITH DATA SOURCE/USER/ROLE + IN/OUT params`
Expected SBLR (symbolic):
```
VERSION(2) EXT_BLOCK EXEC_STMT EXEC_STMT_INPUTS(2) EXEC_STMT_OUTPUTS(1) EXEC_STMT_SQL(LITERAL_STRING("SELECT 1")) EXEC_STMT_DATA_SRC(LITERAL_STRING("DB1")) EXEC_STMT_USER(LITERAL_STRING("SYSDBA")) EXEC_STMT_PWD(LITERAL_STRING("pw")) EXEC_STMT_ROLE(LITERAL_STRING("R1")) EXEC_STMT_TRAN_CLONE(1) EXEC_STMT_PRIVS EXEC_STMT_IN_PARAMS(LITERAL_INT32(10),PARAM(0)) EXEC_STMT_IN_EXCESS(3) EXEC_STMT_OUT_PARAMS(PARAM(1)) END
```
Status: GAP (P2 - exec_stmt)

## Notes
- The fixtures are intentionally minimal and omit message definitions, full
  cursor declarations, and parameter metadata. The translator should bind
  `blr_parameter` references to input metadata supplied out-of-band.
- For `EXECUTE STATEMENT`, this fixture uses the `blr_exec_stmt` form with only
  the SQL expression and terminates with `blr_end`.
- Harness: `ScratchBird/tools/blr_fixture_harness.py` writes the report to
  `ScratchBird-Analysis/reports/blr_fixture_report.md`.
