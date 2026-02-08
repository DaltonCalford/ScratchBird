# Dialect Gap Examples (Authoritative, Exhaustive)

Status: Authoritative (V3)

Purpose: provide deterministic examples for **every** item listed in the MySQL
and PostgreSQL gap matrices. Each item includes either:
- a full SBLR emission example (payload bytes), or
- an explicit rejection rule with SQLSTATE (no SBLR emitted).

Encoding rules: little-endian, `string = [len:varuint][utf8]`,
`schema_path = [count][ident...]`, `list<T> = [count][T...]`.

Maintenance rule: if a gap item is added, removed, or changed in the gap
matrices, the corresponding example below MUST be updated in the same change.

---

## MySQL Gap Matrix Examples

### CREATE INDEX
Statement: `CREATE INDEX idx_name ON users (name)`

Emission: `SBLR3_CREATE_INDEX`
```
flags             = 00 00
index_path        = 01 08 69 64 78 5F 6E 61 6D 65
table             = 01 05 75 73 65 72 73
keys.count        = 01
key               = 01 04 06 00 00 04 00 00 00 00 04 6E 61 6D 65 00 00 00 00
include.count     = 00
predicate.opt     = 00
index_type.opt    = 00
options.count     = 00
```

### CREATE VIEW
Statement: `CREATE VIEW v AS SELECT 1`

Emission: `SBLR3_CREATE_VIEW`
```
flags             = 00 00
path              = 01 01 76
columns.count     = 00
query             = 12 02 00 00 1C 00 00 00 00 00 01 0F 0C 00 00 04 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

### CREATE TEMPORARY TABLE
Statement: `CREATE TEMPORARY TABLE t (id INT)`

Emission: `SBLR3_CREATE_TABLE` (TEMPORARY flag)
```
flags             = 02 00
path              = 01 01 74
columns.count     = 01
column_def        = 02 69 64 3F 0B 00 00 00 00 00 00 00 00 00 00
constraints.count = 00
inherits.count    = 00
partitioning.opt  = 00
tablespace.opt    = 00
options.count     = 00
```

### ON DUPLICATE KEY UPDATE
Statement: `INSERT INTO users(id,name) VALUES (1,'Alice') ON DUPLICATE KEY UPDATE name='Alice'`

Emission: `SBLR3_INSERT` with `on_conflict.action=UPDATE`
```
target            = 01 05 75 73 65 72 73
alias.opt         = 00
columns.count     = 02 02 69 64 04 6E 61 6D 65
source            = 01
values.count      = 01
expr_list.count   = 02
expr1 (INT32)     = 0F 0C 00 00 04 00 00 00 01 00 00 00
expr2 (STRING)    = 13 0C 00 00 06 00 00 00 05 41 6C 69 63 65
select.opt        = 00
on_conflict.opt   = 01
on_conflict       = 00 00 00 00 02 01 04 6E 61 6D 65 13 0C 00 00 06 00 00 00 05 41 6C 69 63 65 00
returning.count   = 00
```

### REPLACE INTO
Statement: `REPLACE INTO users(id,name) VALUES (1,'Alice')`

Emission: `SBLR3_INSERT` with `on_conflict.action=UPDATE`
```
target            = 01 05 75 73 65 72 73
alias.opt         = 00
columns.count     = 02 02 69 64 04 6E 61 6D 65
source            = 01
values.count      = 01
expr_list.count   = 02
expr1 (INT32)     = 0F 0C 00 00 04 00 00 00 01 00 00 00
expr2 (STRING)    = 13 0C 00 00 06 00 00 00 05 41 6C 69 63 65
select.opt        = 00
on_conflict.opt   = 01
on_conflict       = 00 00 00 00 02 01 04 6E 61 6D 65 13 0C 00 00 06 00 00 00 05 41 6C 69 63 65 00
returning.count   = 00
```

### INSERT IGNORE
Statement: `INSERT IGNORE INTO t(id) VALUES (1)`

Emission: `SBLR3_INSERT` with `on_conflict.action=NOTHING`
```
target            = 01 01 74
alias.opt         = 00
columns.count     = 01 02 69 64
source            = 01
values.count      = 01
expr_list.count   = 01
expr1 (INT32)     = 0F 0C 00 00 04 00 00 00 01 00 00 00
select.opt        = 00
on_conflict.opt   = 01
on_conflict       = 00 00 00 00 01 00 00
returning.count   = 00
```

### UNSIGNED
Statement: `CREATE TABLE t (id INT UNSIGNED)`

Emission: `TYPE_UINT32` in `COLUMN_DEF.type`
```
flags             = 00 00
path              = 01 01 74
columns.count     = 01
column_def        = 02 69 64 2B 0B 00 00 00 00 00 00 00 00 00 00
constraints.count = 00
```

### ZEROFILL
Statement: `CREATE TABLE t (id INT ZEROFILL) ZEROFILL_COLUMNS='id'`

Emission: `OPTION_KV` with `zerofill_columns`
```
options.count     = 01
key               = 10 7A 65 72 6F 66 69 6C 6C 5F 63 6F 6C 75 6D 6E 73
value (STRING)    = 13 0C 00 00 03 00 00 00 02 69 64
```

### MULTI* Geometry Types
Statement: `CREATE TABLE t (g MULTIPOINT)`

Emission: `TYPE_MULTIPOINT` with `srid=0`, `format=0`
```
column_def        = 01 67 17 0B 00 00 00 00
```

### ENGINE
Statement: `CREATE TABLE t (id INT) ENGINE=InnoDB`

Emission: `OPTION_KV` key `ENGINE`
```
options.count     = 01
key               = 06 45 4E 47 49 4E 45
value (STRING)    = 13 0C 00 00 07 00 00 00 06 49 6E 6E 6F 44 42
```

### CHARSET / COLLATE / COMMENT
Statement: `CREATE TABLE t (id INT) CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Test'`

Emission: `OPTION_KV`
```
options.count     = 03
key1              = 07 43 48 41 52 53 45 54
val1 (STRING)     = 13 0C 00 00 08 00 00 00 07 75 74 66 38 6D 62 34
key2              = 07 43 4F 4C 4C 41 54 45
val2 (STRING)     = 13 0C 00 00 13 00 00 00 12 75 74 66 38 6D 62 34 5F 30 39 30 30 5F 61 69 5F 63 69
key3              = 07 43 4F 4D 4D 45 4E 54
val3 (STRING)     = 13 0C 00 00 05 00 00 00 04 54 65 73 74
```

---

## PostgreSQL Gap Matrix Examples

### ARRAY Types
Statement: `CREATE TABLE t (tags TEXT[])`

Emission: `TYPE_ARRAY` with element `TYPE_TEXT`
```
flags             = 00 00
path              = 01 01 74
columns.count     = 01
column_def        = 04 74 61 67 73 31 0B 20 00 41 0B 00 00 00 00 00 00 00 00 00
```

### TEMPORARY TABLE
Statement: `CREATE TEMP TABLE t (id INT)`

Emission: `SBLR3_CREATE_TABLE` with TEMPORARY flag
```
flags             = 02 00
path              = 01 01 74
columns.count     = 01
column_def        = 02 69 64 3F 0B 00 00 00 00 00 00 00 00 00 00
```

### UNLOGGED TABLE
Statement: `CREATE UNLOGGED TABLE t (id INT)`

Emission: `SBLR3_CREATE_TABLE` with UNLOGGED flag
```
flags             = 04 00
path              = 01 01 74
columns.count     = 01
column_def        = 02 69 64 3F 0B 00 00 00 00 00 00 00 00 00 00
```

### EXPRESSION INDEX
Statement: `CREATE INDEX idxe ON t ((id))`

Emission: `SBLR3_CREATE_INDEX` (expression key kind)
```
flags             = 00 00
index_path        = 01 04 69 64 78 65
table             = 01 01 74
keys.count        = 01
key               = 02 04 06 00 00 04 00 00 00 00 02 69 64 00 00 00 00
include.count     = 00
predicate.opt     = 00
index_type.opt    = 00
options.count     = 00
```

### INCLUDE
Statement: `CREATE INDEX idx ON t(id) INCLUDE (name)`

Emission: `SBLR3_CREATE_INDEX` (include list)
```
flags             = 00 00
index_path        = 01 03 69 64 78
table             = 01 01 74
keys.count        = 01
key               = 01 04 06 00 00 04 00 00 00 00 02 69 64 00 00 00 00
include.count     = 01 04 6E 61 6D 65
predicate.opt     = 00
index_type.opt    = 00
options.count     = 00
```

### INHERITS
Statement: `CREATE TABLE c (id INT) INHERITS (p)`

Emission: `SBLR3_CREATE_TABLE` with `inherits.count=1`
```
flags             = 00 00
path              = 01 01 63
columns.count     = 01
column_def        = 02 69 64 3F 0B 00 00 00 00 00 00 00 00 00 00
constraints.count = 00
inherits.count    = 01 01 01 70
partitioning.opt  = 00
tablespace.opt    = 00
options.count     = 00
```

### JSONB / XML / INTERVAL / MONEY / COMPOSITE / TIME_TZ / TIMESTAMP_TZ
Statement: `CREATE TABLE t (jb JSONB, xm XML, iv INTERVAL, m MONEY, c COMPOSITE, tt TIME WITH TIME ZONE, ts TIMESTAMP WITH TIME ZONE)`

Emission: `SBLR3_CREATE_TABLE` with type opcodes in `COLUMN_DEF.type`
```
flags             = 00 00
path              = 01 01 74
columns.count     = 07
col_jb            = 02 6A 62 0F 0B 00 00 00 00 00 00 00 00 00 00
col_xm            = 02 78 6D 30 0B 00 00 00 00 00 00 00 00 00 00
col_iv            = 02 69 76 0E 0B 00 00 00 00 00 00 00 00 00 00
col_m             = 01 6D 15 0B 00 00 00 00 00 00 00 00 00 00
col_c             = 01 63 02 0B 00 00 00 00 00 00 00 00 00 00
col_tt            = 02 74 74 20 0B 00 00 00 00 00 00 00 00 00 00
col_ts            = 02 74 73 1F 0B 00 00 00 00 00 00 00 00 00 00
constraints.count = 00
inherits.count    = 00
partitioning.opt  = 00
tablespace.opt    = 00
options.count     = 00
```

### INET / CIDR / MACADDR / MACADDR8
Statement: `CREATE TABLE t (i INET, c CIDR, m MACADDR, m8 MACADDR8)`

Emission: `SBLR3_CREATE_TABLE` with type opcodes in `COLUMN_DEF.type`
```
flags             = 00 00
path              = 01 01 74
columns.count     = 04
col_i             = 01 69 08 0B 00 00 00 00 00 00 00 00 00 00
col_c             = 01 63 01 0B 00 00 00 00 00 00 00 00 00 00
col_m             = 01 6D 13 0B 00 00 00 00 00 00 00 00 00 00
col_m8            = 02 6D 38 14 0B 00 00 00 00 00 00 00 00 00 00
constraints.count = 00
inherits.count    = 00
partitioning.opt  = 00
tablespace.opt    = 00
options.count     = 00
```

### CREATE TABLE format mismatch
Statement: `CREATE TABLE t (id INT NOT NULL)`

Emission: `SBLR3_CREATE_TABLE` (ScratchBird column flags)
```
flags             = 00 00
path              = 01 01 74
columns.count     = 01
column_def        = 02 69 64 3F 0B 01 00 00 00 00 00 00 00 00 00 00
constraints.count = 00
inherits.count    = 00
partitioning.opt  = 00
tablespace.opt    = 00
options.count     = 00
```

### CREATE INDEX format mismatch
Statement: `CREATE INDEX idx ON t(id)`

Emission: `SBLR3_CREATE_INDEX` (ScratchBird key encoding)
```
flags             = 00 00
index_path        = 01 03 69 64 78
table             = 01 01 74
keys.count        = 01
key               = 01 04 06 00 00 04 00 00 00 00 02 69 64 00 00 00 00
include.count     = 00
predicate.opt     = 00
index_type.opt    = 00
options.count     = 00
```

### CREATE VIEW format mismatch
Statement: `CREATE VIEW v AS SELECT 1`

Emission: `SBLR3_CREATE_VIEW` (ScratchBird query payload)
```
flags             = 00 00
path              = 01 01 76
columns.count     = 00
query             = 12 02 00 00 1C 00 00 00 00 00 01 0F 0C 00 00 04 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

### SELECT format mismatch (DISTINCT)
Statement: `SELECT DISTINCT id FROM t WHERE id > 1`

Emission: `SBLR3_SELECT`
```
flags             = 01 00
select_items      = 01 04 06 00 00 04 00 00 00 00 02 69 64
from.opt          = 01 5C 06 00 00 09 00 00 00 01 01 01 74 00 00 00 00 00
joins.count       = 00
where.opt         = 01 1C 06 00 00 12 00 00 00 01 04 06 00 00 04 00 00 00 00 02 69 64 0F 0C 00 00 04 00 00 00 01 00 00 00
group_by.count    = 00
grouping_sets.cnt = 00
grouping_type     = 00
having.opt        = 00
order_by.count    = 00
limit.opt         = 00
offset.opt        = 00
fetch.opt         = 00
set_op.opt        = 00
with.opt          = 00
```

### INSERT format mismatch (alias)
Statement: `INSERT INTO t AS tt (id) VALUES (1)`

Emission: `SBLR3_INSERT` with alias payload
```
target            = 01 01 74
alias.opt         = 01 02 74 74
columns.count     = 01 02 69 64
source            = 01
values.count      = 01
row.count         = 01
row.int32         = 0F 0C 00 00 04 00 00 00 01 00 00 00
select.opt        = 00
on_conflict.opt   = 00
returning.count   = 00
```

### UPDATE format mismatch (alias)
Statement: `UPDATE t AS tt SET id = 2`

Emission: `SBLR3_UPDATE`
```
target            = 01 01 74
alias.opt         = 01 02 74 74
set_items.count   = 01
set_item          = 02 69 64 0F 0C 00 00 04 00 00 00 02 00 00 00
from.opt          = 00
joins.count       = 00
where.opt         = 00
returning.count   = 00
```

### DELETE format mismatch (USING)
Statement: `DELETE FROM t USING u`

Emission: `SBLR3_DELETE`
```
target            = 01 01 74
alias.opt         = 00
using.opt         = 01 5C 06 00 00 09 00 00 00 01 01 01 75 00 00 00 00 00
using_joins.count = 00
where.opt         = 00
returning.count   = 00
```

### MERGE format mismatch
Statement: `MERGE INTO t USING s ON t.id = s.id WHEN MATCHED THEN UPDATE SET id = 2 WHEN NOT MATCHED THEN INSERT (id) VALUES (1)`

Emission: `SBLR3_MERGE`
```
target            = 01 01 74
target_alias.opt  = 00
source_table.opt  = 01 01 01 73
source_query.opt  = 00
source_alias.opt  = 00
on.expr           = 09 0C 00 00 01 00 00 00 01
when_matched.cnt  = 01
matched           = 00 01 01 02 69 64 0F 0C 00 00 04 00 00 00 02 00 00 00
when_not_matched.cnt = 01
not_matched       = 00 01 02 69 64 01 0F 0C 00 00 04 00 00 00 01 00 00 00
when_not_matched_by_source.cnt = 00
```
