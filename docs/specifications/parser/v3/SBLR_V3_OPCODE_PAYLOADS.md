# SBLR V3 Opcode Payload Schemas (Field-Level)

Status: Authoritative (V3)
Last Updated: 2026-02-08

This document defines the payload schema for every V3 opcode. It is sufficient
for a complete serializer/deserializer implementation without prior database
knowledge.

All opcodes and numeric values are defined in:
- `docs/specifications/parser/v3/SBLR_V3_OLD_TO_NEW_MAPPING.md`

## Global Encoding Rules

- All V3 instructions use the header in `SBLR_V3_OPCODE_SPEC.md`.
- All multi-byte fields are little-endian unless stated otherwise.
- `string` = `[len:varuint][utf8 bytes]`.
- `bytes` = `[len:varuint][raw bytes]`.
- `schema_path` = `[count:varuint][ident...count]`.
- `opt<T>` = `[present:bool][value:T if present]`.
- `list<T>` = `[count:varuint][T...count]`.

## Node Encodings

Nested nodes are encoded as full V3 instructions inside the payload.

- `expr` = one nested expression instruction
- `stmt` = one nested statement instruction
- `expr_list` = `list<expr>`
- `stmt_list` = `list<stmt>`

## Shared Structs

### TYPE_SPEC

Fields:
```
[type_opcode:u16]
[type_payload:bytes]  // schema depends on opcode
```

Type-specific payload rules are mandatory:
- `SBLR3_TYPE_ENUM`:
  - `enum_catalog_id: u128`
  - `storage_width: u8` (1/2/4)
  - `flags: u16` (0x0001 ORDERED, 0x0002 WRAP_ALLOWED)
- `SBLR3_TYPE_SET`:
  - `set_catalog_id: u128`
  - `storage_mode: u8` (0 ORDINAL_LIST, 1 BITSET)
  - `element_type: TYPE_SPEC`
- `SBLR3_TYPE_ROW` / `SBLR3_TYPE_COMPOSITE`:
  - `row_catalog_id: u128`
  - `field_count: u16`
  - repeat fields: `field_name:string`, `field_type:TYPE_SPEC`, `flags:u16`
- Geometry types:
  - `srid:u32`
  - `format:u8` (0 CANONICAL, 1 WKB_COMPAT)
- `SBLR3_TYPE_BIT`:
  - `bit_length:u16`
  - `storage:u8` (0 BITSTRING, 1 VARBIT)
- `SBLR3_TYPE_YEAR`:
  - `format:u8` (0 YEAR_2DIGIT, 1 YEAR_4DIGIT)

### COLUMN_DEF

Fields:
```
[name:ident]
[type:TYPE_SPEC]
[flags:u16]
[default_expr:opt<expr>]
[generated_expr:opt<expr>]
[identity:opt<IDENTITY_SPEC>]
[collation:opt<ident>]
[charset:opt<ident>]
[check_count:varuint]
[check_expr...check_count]
```

Flags:
- 0x0001 NOT_NULL
- 0x0002 NULL_ALLOWED_EXPLICIT
- 0x0004 GENERATED_STORED
- 0x0008 GENERATED_VIRTUAL

### IDENTITY_SPEC

Fields:
```
[start:opt<i64>]
[increment:opt<i64>]
[min_value:opt<i64>]
[max_value:opt<i64>]
[cycle:bool]
[cache:opt<u64>]
```

### TABLE_CONSTRAINT

Fields:
```
[type:u8]                 // 1=PRIMARY_KEY,2=UNIQUE,3=FOREIGN_KEY,4=CHECK
[name:opt<ident>]
[columns:list<ident>]
[ref_table:opt<schema_path>]
[ref_columns:list<ident>]
[on_update:u8]             // 0=NO_ACTION,1=RESTRICT,2=CASCADE,3=SET_NULL,4=SET_DEFAULT
[on_delete:u8]
[check_expr:opt<expr>]
```

### INDEX_KEY

Fields:
```
[kind:u8]                 // 1=COLUMN,2=EXPRESSION
[name_or_expr:expr]       // COLUMN uses COLUMN_REF expr
[order:u8]                // 0=ASC,1=DESC
[nulls:u8]                // 0=DEFAULT,1=FIRST,2=LAST
[collation:opt<ident>]
[opclass:opt<ident>]
```

### OPTION_KV

Fields:
```
[count:varuint]
repeat count:
  [key:ident]
  [value:expr]
```

### ORDER_BY_ITEM

Fields:
```
[expr:expr]
[order:u8]                // 0=ASC,1=DESC
[nulls:u8]                // 0=DEFAULT,1=FIRST,2=LAST
```

### TABLE_REF

Fields:
```
[table_path:schema_path]
[alias:opt<ident>]
[table_flags:u16]         // 0x0001 ONLY, 0x0002 LATERAL
```

### JOIN

Fields:
```
[type:u8]                 // 1=INNER,2=LEFT,3=RIGHT,4=FULL,5=CROSS
[right:TABLE_REF]
[condition:opt<expr>]
[using:list<ident>]
```

### WINDOW_SPEC

Fields:
```
[partition_by:list<expr>]
[order_by:list<ORDER_BY_ITEM>]
[frame:opt<WINDOW_FRAME>]
```

### SCHEMA_WINDOW_SPEC

```
WINDOW_SPEC
```

### SCHEMA_WINDOW_ORDER_BY

```
[order_by:list<ORDER_BY_ITEM>]
```

### WINDOW_FRAME

Fields:
```
[unit:u8]                 // 1=ROWS,2=RANGE,3=GROUPS
[start:WINDOW_BOUND]
[end:WINDOW_BOUND]
[explicit_between:bool]
```

### WINDOW_BOUND

Fields:
```
[kind:u8]                 // 1=UNBOUNDED_PRECEDING,2=UNBOUNDED_FOLLOWING,3=CURRENT_ROW,4=PRECEDING,5=FOLLOWING
[offset:opt<expr>]
```

### ASSIGNMENT

Fields:
```
[column:SCHEMA_COLUMN_REF]
[value:expr]
```

### SCHEMA_COLUMN_REF

Fields:
```
[path:schema_path]        // table or schema-qualified path (may be empty)
[column:ident]
```

### FETCH_SPEC

Fields:
```
[mode:u8]                 // 1=FIRST,2=NEXT
[with_ties:bool]
[row_count:expr]
```

### SET_OP

Fields:
```
[type:u8]                 // 1=UNION,2=INTERSECT,3=EXCEPT
[all:bool]
[right:stmt]
```

### CTE

Fields:
```
[name:ident]
[column_names:list<ident>]
[query:stmt]
[recursive:bool]
```

### ON_CONFLICT_SPEC

Fields:
```
[target_cols:list<ident>]
[action:u8]               // 1=NOTHING,2=UPDATE
[assignments:list<ASSIGNMENT>]
[where:opt<expr>]
```

### MERGE_ACTION

Fields:
```
[action:u8]               // 1=UPDATE,2=DELETE,3=INSERT
[condition:opt<expr>]
[assignments:list<ASSIGNMENT>]
[insert_columns:list<ident>]
[insert_values:list<expr>]
```

### PSQL_DECL

Fields:
```
[name:ident]
[type:TYPE_SPEC]
[constant:bool]
[default:opt<expr>]
```

### PSQL_EXCEPTION

Fields:
```
[condition:ident]
[handler:stmt_list]
```

### FUNC/AGG/WINDOW CALL

`SCHEMA_FUNC_CALL`
```
[args:expr_list]
```

`SCHEMA_AGG_CALL`
```
[distinct:bool]
[args:expr_list]
```

`SCHEMA_WINDOW_CALL`
```
[args:expr_list]
[window:WINDOW_SPEC]
```

### Expression Schemas

`SCHEMA_EXPR_BINARY`
```
[lhs:expr]
[rhs:expr]
```

`SCHEMA_EXPR_UNARY`
```
[value:expr]
```

`SCHEMA_EXPR_CAST`
```
[value:expr]
[type:TYPE_SPEC]
```

`SCHEMA_EXPR_CASE`
```
[base:opt<expr>]
[when_count:varuint]
repeat when_count:
  [when:expr]
  [then:expr]
[else:opt<expr>]
```

`CASE_ARM` (used for documentation):
```
[when:expr]
[then:expr]
```

`SCHEMA_EXPR_IN`
```
[value:expr]
[list:list<expr>]
[negated:bool]
```

`SCHEMA_EXPR_BETWEEN`
```
[value:expr]
[lower:expr]
[upper:expr]
[negated:bool]
```

`SCHEMA_EXPR_LIKE`
```
[value:expr]
[pattern:expr]
[escape:opt<expr>]
[negated:bool]
```

`SCHEMA_EXPR_SUBQUERY`
```
[query:stmt]
```

`SCHEMA_EXPR_EXISTS`
```
[query:stmt]
```

### Literal Schemas

All literal schemas are encoded using canonical storage encodings from
`types/VALUE_SPEC_STORAGE_ENCODINGS.md`.

Examples:
- `SCHEMA_LITERAL_BOOLEAN` -> `[value:u8]`
- `SCHEMA_LITERAL_INT8` -> `[value:i8]`
- `SCHEMA_LITERAL_UINT64` -> `[value:u64]`
- `SCHEMA_LITERAL_FLOAT32` -> `[value:f32]`
- `SCHEMA_LITERAL_DOUBLE` -> `[value:f64]`
- `SCHEMA_LITERAL_UUID` -> `[value:uuid]`
- `SCHEMA_LITERAL_STRING` -> `[value:string]`
- `SCHEMA_LITERAL_BINARY` -> `[value:bytes]`
- `SCHEMA_LITERAL_DATE` -> `[value:i32][offset_seconds:i32]`
- `SCHEMA_LITERAL_TIME` -> `[value:i64][offset_seconds:i32]`
- `SCHEMA_LITERAL_TIMESTAMP` -> `[value:i64][offset_seconds:i32]`
- `SCHEMA_LITERAL_TIME_TZ` -> `[value:i64][offset_seconds:i32]`
- `SCHEMA_LITERAL_TIMESTAMP_TZ` -> `[value:i64][offset_seconds:i32]`

## Statement Schemas

### SCHEMA_SELECT

```
[flags:u16]
[select_items:list<expr>]
[from:opt<TABLE_REF>]
[joins:list<JOIN>]
[where:opt<expr>]
[group_by:list<expr>]
[grouping_sets:list<expr_list>]
[grouping_type:u8]
[having:opt<expr>]
[order_by:list<ORDER_BY_ITEM>]
[limit:opt<expr>]
[offset:opt<expr>]
[fetch:opt<FETCH_SPEC>]
[set_op:opt<SET_OP>]
[with:opt<list<CTE>>]
```

### SCHEMA_INSERT

```
[target:schema_path]
[alias:opt<ident>]
[columns:list<ident>]
[source:u8]               // 1=VALUES,2=SELECT,3=DEFAULTS
[values:opt<list<expr_list>>]
[select:opt<stmt>]
[on_conflict:opt<ON_CONFLICT_SPEC>]
[returning:list<expr>]
```

### SCHEMA_UPDATE

```
[target:schema_path]
[alias:opt<ident>]
[set_items:list<ASSIGNMENT>]
[from:opt<TABLE_REF>]
[joins:list<JOIN>]
[where:opt<expr>]
[returning:list<expr>]
```

### SCHEMA_DELETE

```
[target:schema_path]
[alias:opt<ident>]
[using:opt<TABLE_REF>]
[using_joins:list<JOIN>]
[where:opt<expr>]
[returning:list<expr>]
```

### SCHEMA_MERGE

```
[target:schema_path]
[target_alias:opt<ident>]
[source_table:opt<TABLE_REF>]
[source_query:opt<stmt>]
[source_alias:opt<ident>]
[on:expr]
[when_matched:list<MERGE_ACTION>]
[when_not_matched:list<MERGE_ACTION>]
[when_not_matched_by_source:list<MERGE_ACTION>]
```

### SCHEMA_COPY

```
[has_query:bool]
[query:opt<stmt>]
[target_table:opt<schema_path>]
[columns:list<ident>]
[direction:u8]            // 1=FROM,2=TO
[filename:opt<string>]
[format:u8]               // 1=TEXT,2=CSV,3=BINARY
[options:OPTION_KV]
```

### SCHEMA_DDL_CREATE_TABLE

```
[flags:u16]
[path:schema_path]
[columns:list<COLUMN_DEF>]
[constraints:list<TABLE_CONSTRAINT>]
[inherits:list<schema_path>]
[partitioning:opt<expr>]
[tablespace:opt<schema_path>]
[options:OPTION_KV]
```

### SCHEMA_DDL_CREATE_INDEX

```
[flags:u16]
[index_path:schema_path]
[table:schema_path]
[keys:list<INDEX_KEY>]
[include:list<ident>]
[predicate:opt<expr>]
[index_type:opt<ident>]
[options:OPTION_KV]
```

### SCHEMA_DDL_CREATE_VIEW

```
[flags:u16]
[path:schema_path]
[columns:list<ident>]
[query:stmt]
```

### SCHEMA_DDL_CREATE_SEQUENCE

```
[flags:u16]
[path:schema_path]
[start:opt<i64>]
[increment:opt<i64>]
[min_value:opt<i64>]
[max_value:opt<i64>]
[cycle:opt<bool>]
[cache:opt<u64>]
```

### SCHEMA_DDL_CREATE_SCHEMA

```
[flags:u16]               // 0x0001 IF_NOT_EXISTS
[path:schema_path]
[owner:opt<ident>]
[default_charset:opt<ident>]
[path_list:list<schema_path>]
```

### SCHEMA_DDL_CREATE_DATABASE

```
[flags:u16]
[name:ident]
[page_size:opt<u32>]
[default_charset:opt<ident>]
[default_collation:opt<ident>]
[encrypted:bool]
[owner:opt<ident>]
[tablespace:opt<schema_path>]
[options:OPTION_KV]
```

### SCHEMA_DDL_CREATE_DOMAIN

```
[name:ident]
[type:TYPE_SPEC]
[default_expr:opt<expr>]
[check_expr:opt<expr>]
```

### SCHEMA_DDL_CREATE_TYPE

```
[name:ident]
[type:TYPE_SPEC]
[options:OPTION_KV]
```

### SCHEMA_DDL_CREATE_FUNCTION / PROCEDURE

```
[name:ident]
[params:list<PARAM_DEF>]
[return_type:opt<TYPE_SPEC>]
[language:ident]
[body:bytes]
[options:OPTION_KV]
```

### PARAM_DEF

```
[name:ident]
[type:TYPE_SPEC]
[mode:u8]                 // 0=IN,1=OUT,2=INOUT
[default_expr:opt<expr>]
```

### SCHEMA_DDL_CREATE_TRIGGER

```
[name:ident]
[table:schema_path]
[timing:u8]               // 0=BEFORE,1=AFTER
[event_mask:u32]          // bitmask INSERT/UPDATE/DELETE
[for_each_row:bool]
[when:opt<expr>]
[body:bytes]
```

### SCHEMA_DDL_CREATE_PACKAGE

```
[name:ident]
[spec:bytes]
[body:opt<bytes>]
```

### SCHEMA_DDL_CREATE_EXCEPTION

```
[name:ident]
[message:string]
```

### SCHEMA_DDL_CREATE_USER / ROLE / GROUP

```
[name:ident]
[options:OPTION_KV]
```

### SCHEMA_DDL_CREATE_POLICY

```
[name:ident]
[table:schema_path]
[event_mask:u8]           // 1=SELECT,2=INSERT,3=UPDATE,4=DELETE
[using_expr:opt<expr>]
[check_expr:opt<expr>]
```

### SCHEMA_DDL_CREATE_TABLESPACE

```
[name:ident]
[location:string]
[options:OPTION_KV]
```

### SCHEMA_DDL_CREATE_FOREIGN_SERVER

```
[name:ident]
[type:ident]
[host:string]
[options:OPTION_KV]
```

### SCHEMA_DDL_CREATE_FOREIGN_DATA_WRAPPER

```
[name:ident]
[handler:opt<ident>]
[validator:opt<ident>]
[options:OPTION_KV]
```

### SCHEMA_DDL_CREATE_FOREIGN_TABLE

```
[name:schema_path]
[server:ident]
[columns:list<COLUMN_DEF>]
[options:OPTION_KV]
```

### SCHEMA_DDL_CREATE_USER_MAPPING

```
[server:ident]
[user:ident]
[options:OPTION_KV]
```

### SCHEMA_DDL_CREATE_SYNONYM

```
[name:schema_path]
[target:schema_path]
```

### SCHEMA_DDL_CREATE_UDR

```
[name:ident]
[library_path:string]
[entry_point:ident]
[options:OPTION_KV]
```

### SCHEMA_DDL_CREATE_JOB

```
[name:ident]
[schedule:string]
[command:bytes]
[options:OPTION_KV]
```

### SCHEMA_DDL_ALTER_TABLE

```
[table:schema_path]
[if_exists:bool]
[only:bool]
[action:u8]
[payload:bytes]
```

Action payloads (by action code):
- 1 ADD_COLUMN: `[column:COLUMN_DEF]`
- 2 ADD_CONSTRAINT: `[constraint:TABLE_CONSTRAINT]`
- 3 DROP_COLUMN: `[name:ident][cascade:bool]`
- 4 DROP_CONSTRAINT: `[name:ident][cascade:bool]`
- 5 ALTER_COLUMN_TYPE: `[name:ident][type:TYPE_SPEC]`
- 6 ALTER_COLUMN_POSITION: `[name:ident][position_1_based:u32]`
- 7 ALTER_COLUMN_SET_DEFAULT: `[name:ident][default:expr]`
- 8 ALTER_COLUMN_DROP_DEFAULT: `[name:ident]`
- 9 ALTER_COLUMN_SET_NOT_NULL: `[name:ident]`
- 10 ALTER_COLUMN_DROP_NOT_NULL: `[name:ident]`
- 11 RENAME_TABLE: `[new_name:ident]`
- 12 RENAME_CONSTRAINT: `[old_name:ident][new_name:ident]`
- 13 SET_SCHEMA: `[schema:schema_path]`
- 15 RENAME_COLUMN: `[old_name:ident][new_name:ident]`
- 16 SET_STATISTICS: `[name:ident][target:i32]`
- 17 SET_STORAGE: `[name:ident][storage:ident]`
- 18 INHERIT: `[parent:schema_path]`
- 19 NO_INHERIT: `[parent:opt<schema_path>]`
- 20 ENABLE_TRIGGER: `[trigger_all:bool][trigger_name:opt<ident>]`
- 21 DISABLE_TRIGGER: `[trigger_all:bool][trigger_name:opt<ident>]`
- 22 ENABLE_RLS: *(empty)*
- 23 DISABLE_RLS: *(empty)*
- 24 FORCE_RLS: *(empty)*
- 25 NO_FORCE_RLS: *(empty)*
- 26 ATTACH_PARTITION: `[partition:schema_path][bounds:opt<string>]`
- 27 DETACH_PARTITION: `[partition:schema_path]`
- 28 VALIDATE_CONSTRAINT: `[name:ident]`
- 29 ALTER_COLUMN_USING: `[name:ident][type:TYPE_SPEC][using:expr]`

Notes:
- `ALTER TABLE ... SET TABLESPACE` MUST use `SBLR3_ALTER_TABLE_SET_TABLESPACE` (no `ALTER_TABLE` action).
- Multiple ALTER actions MUST be emitted as multiple `SBLR3_ALTER_TABLE` statements in source order.

### SCHEMA_DDL_ALTER_COLUMN_DEFAULT

```
[table:schema_path]
[column:ident]
[default:opt<expr>]
```

### SCHEMA_DDL_ALTER_COLUMN_TYPE

```
[table:schema_path]
[column:ident]
[type:TYPE_SPEC]
[using:opt<expr>]
```

### SCHEMA_DDL_ALTER_COLUMN_NULL

```
[table:schema_path]
[column:ident]
[set_not_null:bool]
```

### SCHEMA_DDL_ALTER_TABLESPACE

```
[tablespace:schema_path]
[options:OPTION_KV]
```

### SCHEMA_DDL_ALTER_TABLE_SET_TABLESPACE

```
[table:schema_path]
[tablespace:schema_path]
[options:OPTION_KV]
```

### SCHEMA_DDL_ALTER_RENAME

```
[object_type:u8]
[object_path:schema_path]
[new_name:ident]
```

### SCHEMA_DDL_ALTER_SEQUENCE

```
[path:schema_path]
[start:opt<i64>]
[increment:opt<i64>]
[min_value:opt<i64>]
[max_value:opt<i64>]
[cycle:opt<bool>]
[cache:opt<u64>]
```

### SCHEMA_RANGE_TYPE_OPTIONS

```
[subtype:opt<TYPE_SPEC>]
[subtype_collation:opt<string>]
[subtype_opclass:opt<string>]
[canonical:opt<string>]
[subtype_diff:opt<string>]
[multirange:opt<bool>]
```

### SCHEMA_BASE_TYPE_OPTIONS

```
[storage:opt<TYPE_SPEC>]
[input_function:opt<string>]
[output_function:opt<string>]
[receive_function:opt<string>]
[send_function:opt<string>]
[typmod_in_function:opt<string>]
[typmod_out_function:opt<string>]
[analyze_function:opt<string>]
[alignment:opt<u8>]
[storage_mode:opt<u8>]
[category:opt<u8>]
[preferred:opt<bool>]
```

### SCHEMA_DDL_ALTER_INDEX

```
[index:schema_path]
[action:u8]
[options:OPTION_KV]
```

### SCHEMA_DDL_ALTER_SCHEMA

```
[schema:schema_path]
[action:u8]
[new_name:opt<ident>]
[owner:opt<ident>]
[new_path:opt<schema_path>]
```

### SCHEMA_DDL_ALTER_DATABASE

```
[database:schema_path]
[action:u8]
[new_name:opt<ident>]
[owner:opt<ident>]
[alias:opt<ident>]
[options:OPTION_KV]
```

### SCHEMA_DDL_ALTER_DOMAIN

```
[domain:schema_path]
[action:u8]
[value:opt<string>]
[constraint:opt<ident>]
[new_name:opt<ident>]
```

### SCHEMA_DDL_ALTER_TYPE

```
[type:schema_path]
[action:u8]
[new_name:opt<ident>]
[new_schema:opt<ident>]
[value_label:opt<ident>]
[before_label:opt<ident>]
[after_label:opt<ident>]
[old_label:opt<ident>]
[new_label:opt<ident>]
[is_range_options:bool]
[is_base_options:bool]
[range_options:opt<SCHEMA_RANGE_TYPE_OPTIONS>]
[base_options:opt<SCHEMA_BASE_TYPE_OPTIONS>]
```

### SCHEMA_DDL_ALTER_POLICY

```
[policy_name:ident]
[table:schema_path]
[roles:list<ident>]
[using_expr:opt<expr>]
[check_expr:opt<expr>]
```

### SCHEMA_DDL_ALTER_SYSTEM

```
[key:ident]
[value:opt<expr>]
```

### SCHEMA_DDL_ALTER_JOB

```
[job_name:ident]
[schedule_kind:opt<u8>]
[cron_expression:opt<ident>]
[at_timestamp:opt<ident>]
[interval_seconds:opt<i64>]
[starts_at:opt<ident>]
[ends_at:opt<ident>]
[job_type:opt<u8>]
[job_sql:opt<string>]
[procedure_name:opt<ident>]
[external_command:opt<string>]
[state:opt<u8>]
[max_retries:opt<u32>]
[retry_backoff_seconds:opt<u32>]
[timeout_seconds:opt<u32>]
[on_completion:opt<u8>]
[run_as_role:opt<ident>]
[description:opt<string>]
[job_class:opt<ident>]
[partition_strategy:opt<ident>]
[partition_expression:opt<ident>]
[partition_shard:opt<ident>]
[depends_on:list<ident>]
[clear_depends_on:bool]
[secret_key:opt<ident>]
[secret_value:opt<string>]
[drop_secret:bool]
```

### SCHEMA_DDL_ATTACH_TABLESPACE

```
[name:ident]
[location:string]
[validate:bool]
[allow_mismatch:bool]
```

### SCHEMA_DDL_DETACH_TABLESPACE

```
[name:ident]
[force:bool]
```

### SCHEMA_DDL_DROP

```
[flags:u16]
[object_type:u8]
[path:schema_path]
```

### SCHEMA_DDL_TRUNCATE

```
[flags:u16]
[tables:list<schema_path>]
```

### SCHEMA_DDL_COMMENT

```
[object_type:u8]
[object_path:schema_path]
[text:string]
```

### SCHEMA_GRANT_REVOKE

```
[is_grant:bool]
[privileges:u64]
[object_type:u8]
[object_path:schema_path]
[grantees:list<ident>]
[with_grant_option:bool]
```

### SCHEMA_SET_SHOW_RESET

```
[action:u8]               // 1=SET,2=SHOW,3=RESET
[key:ident]
[value:opt<expr>]
[scope:u8]                // 0=SESSION,1=LOCAL
```

### SCHEMA_EXPLAIN

```
[analyze:bool]
[format:opt<ident>]
[query:stmt]
```

### SCHEMA_TXN_CONTROL

```
[action:u8]               // 1=BEGIN,2=COMMIT,3=ROLLBACK,4=SAVEPOINT,5=RELEASE,6=ROLLBACK_TO
[name:opt<ident>]
```

### SCHEMA_PSQL_BLOCK

```
[decls:list<PSQL_DECL>]
[body:stmt_list]
[exception_handlers:list<PSQL_EXCEPTION>]
```

## Opcode Schema Mapping Rules

The following rules are exhaustive and apply to every opcode.

### CONTROL
- `SBLR3_END`: empty payload
- `SBLR3_VERSION`: `[major:u16][minor:u16][patch:u16]`
- `SBLR3_EXTENDED_OPCODE`: `[opcode:u16]` + payload of the extended opcode

### LITERAL
All `SBLR3_LITERAL_*` opcodes use `SCHEMA_LITERAL_<type>` where `<type>`
corresponds to the opcode suffix and payload uses canonical storage encoding
from `types/VALUE_SPEC_STORAGE_ENCODINGS.md`.

### FUNC / AGG / WINDOW
- `SBLR3_FUNC_*` and `SBLR3_EXPR_FUNCTION_CALL` -> `SCHEMA_FUNC_CALL` with `[args:expr_list]`
- `SBLR3_AGG_*` -> `SCHEMA_AGG_CALL` with `[distinct:bool][args:expr_list]`
- `SBLR3_WIN_*` -> `SCHEMA_WINDOW_CALL` with `[args:expr_list][window:WINDOW_SPEC]`

### EXPR Operators
- Binary operator opcodes -> `SCHEMA_EXPR_BINARY` `[lhs:expr][rhs:expr]`
- Unary operator opcodes -> `SCHEMA_EXPR_UNARY` `[value:expr]`
- Cast opcode -> `SCHEMA_EXPR_CAST` `[value:expr][type:TYPE_SPEC]`
- `SBLR3_CASE` -> `SCHEMA_EXPR_CASE` `[base:opt<expr>][when:list<CASE_ARM>][else:opt<expr>]`
- `SBLR3_IN` / `SBLR3_NOT_IN` -> `SCHEMA_EXPR_IN`
- `SBLR3_BETWEEN` / `SBLR3_NOT_BETWEEN` -> `SCHEMA_EXPR_BETWEEN`
- `SBLR3_LIKE` / `SBLR3_NOT_LIKE` -> `SCHEMA_EXPR_LIKE`
- `SBLR3_EXISTS` -> `SCHEMA_EXPR_EXISTS`
- `SBLR3_SUBQUERY` -> `SCHEMA_EXPR_SUBQUERY`

### DML
- `SBLR3_SELECT` -> `SCHEMA_SELECT`
- `SBLR3_INSERT` -> `SCHEMA_INSERT`
- `SBLR3_UPDATE` -> `SCHEMA_UPDATE`
- `SBLR3_DELETE` -> `SCHEMA_DELETE`
- `SBLR3_MERGE` -> `SCHEMA_MERGE`
- `SBLR3_COPY` -> `SCHEMA_COPY`

### DDL
- `SBLR3_CREATE_TABLE` -> `SCHEMA_DDL_CREATE_TABLE`
- `SBLR3_CREATE_INDEX` -> `SCHEMA_DDL_CREATE_INDEX`
- `SBLR3_CREATE_VIEW` -> `SCHEMA_DDL_CREATE_VIEW`
- `SBLR3_CREATE_SEQUENCE` -> `SCHEMA_DDL_CREATE_SEQUENCE`
- `SBLR3_CREATE_SCHEMA` -> `SCHEMA_DDL_CREATE_SCHEMA`
- `SBLR3_CREATE_DATABASE` -> `SCHEMA_DDL_CREATE_DATABASE`
- `SBLR3_CREATE_DOMAIN` -> `SCHEMA_DDL_CREATE_DOMAIN`
- `SBLR3_CREATE_TYPE` -> `SCHEMA_DDL_CREATE_TYPE`
- `SBLR3_CREATE_FUNCTION` -> `SCHEMA_DDL_CREATE_FUNCTION`
- `SBLR3_CREATE_PROCEDURE` -> `SCHEMA_DDL_CREATE_PROCEDURE`
- `SBLR3_CREATE_TRIGGER` -> `SCHEMA_DDL_CREATE_TRIGGER`
- `SBLR3_CREATE_PACKAGE` -> `SCHEMA_DDL_CREATE_PACKAGE`
- `SBLR3_CREATE_EXCEPTION` -> `SCHEMA_DDL_CREATE_EXCEPTION`
- `SBLR3_CREATE_USER` / `SBLR3_CREATE_ROLE` / `SBLR3_CREATE_GROUP` -> `SCHEMA_DDL_CREATE_USER`
- `SBLR3_CREATE_POLICY` -> `SCHEMA_DDL_CREATE_POLICY`
- `SBLR3_CREATE_TABLESPACE` -> `SCHEMA_DDL_CREATE_TABLESPACE`
- `SBLR3_CREATE_FOREIGN_DATA_WRAPPER` -> `SCHEMA_DDL_CREATE_FOREIGN_DATA_WRAPPER`
- `SBLR3_CREATE_FOREIGN_SERVER` -> `SCHEMA_DDL_CREATE_FOREIGN_SERVER`
- `SBLR3_CREATE_FOREIGN_TABLE` -> `SCHEMA_DDL_CREATE_FOREIGN_TABLE`
- `SBLR3_CREATE_USER_MAPPING` -> `SCHEMA_DDL_CREATE_USER_MAPPING`
- `SBLR3_CREATE_SYNONYM` -> `SCHEMA_DDL_CREATE_SYNONYM`
- `SBLR3_CREATE_UDR` -> `SCHEMA_DDL_CREATE_UDR`
- `SBLR3_CREATE_JOB` -> `SCHEMA_DDL_CREATE_JOB`
- `SBLR3_ALTER_TABLE` -> `SCHEMA_DDL_ALTER_TABLE`
- `SBLR3_ALTER_INDEX` -> `SCHEMA_DDL_ALTER_INDEX`
- `SBLR3_ALTER_SCHEMA` -> `SCHEMA_DDL_ALTER_SCHEMA`
- `SBLR3_ALTER_DATABASE` -> `SCHEMA_DDL_ALTER_DATABASE`
- `SBLR3_ALTER_DOMAIN` -> `SCHEMA_DDL_ALTER_DOMAIN`
- `SBLR3_ALTER_TYPE` -> `SCHEMA_DDL_ALTER_TYPE`
- `SBLR3_ALTER_POLICY` -> `SCHEMA_DDL_ALTER_POLICY`
- `SBLR3_ALTER_SYSTEM` -> `SCHEMA_DDL_ALTER_SYSTEM`
- `SBLR3_ALTER_JOB` -> `SCHEMA_DDL_ALTER_JOB`
- `SBLR3_ALTER_SEQUENCE` -> `SCHEMA_DDL_ALTER_SEQUENCE`
- `SBLR3_ALTER_COLUMN_DEFAULT` -> `SCHEMA_DDL_ALTER_COLUMN_DEFAULT`
- `SBLR3_ALTER_COLUMN_TYPE` -> `SCHEMA_DDL_ALTER_COLUMN_TYPE`
- `SBLR3_ALTER_COLUMN_NULL` -> `SCHEMA_DDL_ALTER_COLUMN_NULL`
- `SBLR3_ALTER_TABLESPACE` -> `SCHEMA_DDL_ALTER_TABLESPACE`
- `SBLR3_ALTER_TABLE_SET_TABLESPACE` -> `SCHEMA_DDL_ALTER_TABLE_SET_TABLESPACE`
- `SBLR3_ATTACH_TABLESPACE` -> `SCHEMA_DDL_ATTACH_TABLESPACE`
- `SBLR3_DETACH_TABLESPACE` -> `SCHEMA_DDL_DETACH_TABLESPACE`
- `SBLR3_RENAME` -> `SCHEMA_DDL_ALTER_RENAME`
- `SBLR3_DROP` -> `SCHEMA_DDL_DROP`
- `SBLR3_TRUNCATE` -> `SCHEMA_DDL_TRUNCATE`
- `SBLR3_COMMENT` -> `SCHEMA_DDL_COMMENT`

### DCL / SESSION / TXN / PSQL
- `SBLR3_GRANT` / `SBLR3_REVOKE` -> `SCHEMA_GRANT_REVOKE`
- `SBLR3_SET` / `SBLR3_SHOW` / `SBLR3_RESET` -> `SCHEMA_SET_SHOW_RESET`
- `SBLR3_BEGIN` / `SBLR3_COMMIT` / `SBLR3_ROLLBACK` / `SBLR3_SAVEPOINT`
  / `SBLR3_RELEASE_SAVEPOINT` / `SBLR3_ROLLBACK_TO_SAVEPOINT` -> `SCHEMA_TXN_CONTROL`
- `SBLR3_PSQL_BLOCK` -> `SCHEMA_PSQL_BLOCK`
- `SBLR3_BLOCK` -> `SCHEMA_PSQL_BLOCK`
- `SBLR3_DECLARE` -> `SCHEMA_PSQL_DECLARE`
- `SBLR3_ASSIGN` -> `SCHEMA_PSQL_ASSIGN`
- `SBLR3_VAR_LOAD` -> `SCHEMA_PSQL_VAR_LOAD`
- `SBLR3_VAR_STORE` -> `SCHEMA_PSQL_VAR_STORE`
- `SBLR3_IF` -> `SCHEMA_PSQL_IF`
- `SBLR3_ELSIF` -> `SCHEMA_PSQL_IF` (inline ELSIF list)
- `SBLR3_ELSE` -> `SCHEMA_PSQL_IF` (inline ELSE body)
- `SBLR3_LOOP` -> `SCHEMA_PSQL_LOOP`
- `SBLR3_WHILE` -> `SCHEMA_PSQL_WHILE`
- `SBLR3_PSQL_FOR_SELECT` -> `SCHEMA_PSQL_FOR_SELECT`
- `SBLR3_PSQL_FOR_EXECUTE` -> `SCHEMA_PSQL_FOR_EXECUTE`
- `SBLR3_PSQL_CASE` -> `SCHEMA_PSQL_CASE`
- `SBLR3_EXIT` -> `SCHEMA_PSQL_EXIT`
- `SBLR3_PSQL_LEAVE` -> `SCHEMA_PSQL_EXIT`
- `SBLR3_PSQL_CONTINUE` -> `SCHEMA_PSQL_CONTINUE`
- `SBLR3_JUMP` -> `SCHEMA_PSQL_JUMP`
- `SBLR3_JUMP_IF_TRUE` / `SBLR3_JUMP_IF_FALSE` -> `SCHEMA_PSQL_JUMP_IF`
- `SBLR3_LABEL` -> `SCHEMA_PSQL_LABEL`
- `SBLR3_TRY` -> `SCHEMA_PSQL_TRY`
- `SBLR3_EXCEPTION_HANDLER` / `SBLR3_EXCEPT_HANDLER` -> `SCHEMA_PSQL_TRY`
- `SBLR3_RAISE` -> `SCHEMA_PSQL_RAISE`
- `SBLR3_RETURN` -> `SCHEMA_PSQL_RETURN`
- `SBLR3_SUSPEND` -> `SCHEMA_PSQL_SUSPEND`
- `SBLR3_CALL` -> `SCHEMA_PSQL_CALL`
- `SBLR3_PARAM_IN` -> `SCHEMA_PSQL_PARAM_IN`
- `SBLR3_PARAM_OUT` -> `SCHEMA_PSQL_PARAM_OUT`
- `SBLR3_PARAM_INOUT` -> `SCHEMA_PSQL_PARAM_INOUT`
- `SBLR3_PROCEDURE` -> `SCHEMA_PSQL_PROCEDURE`
- `SBLR3_FUNCTION` -> `SCHEMA_PSQL_FUNCTION`
- `SBLR3_CURSOR_DECLARE` -> `SCHEMA_PSQL_CURSOR_DECLARE`
- `SBLR3_CURSOR_OPEN` -> `SCHEMA_PSQL_CURSOR_OPEN`
- `SBLR3_CURSOR_FETCH` -> `SCHEMA_PSQL_CURSOR_FETCH`
- `SBLR3_CURSOR_CLOSE` -> `SCHEMA_PSQL_CURSOR_CLOSE`
- `SBLR3_PSQL_POST_EVENT` -> `SCHEMA_PSQL_POST_EVENT`

### SESSION (RESET/SET TIME ZONE)
- `SBLR3_SET_TIME_ZONE` -> `SCHEMA_SET_SHOW_RESET` with `action=SET`, `key='TIME ZONE'`
- `SBLR3_RESET` -> `SCHEMA_SET_SHOW_RESET` with `action=RESET`
- `SBLR3_RESET_ALL` -> `SCHEMA_SET_SHOW_RESET` with `action=RESET`, `key='ALL'`
- `SBLR3_RESET_ROLE` -> `SCHEMA_SET_SHOW_RESET` with `action=RESET`, `key='ROLE'`
- `SBLR3_RESET_SESSION_AUTH` -> `SCHEMA_SET_SHOW_RESET` with `action=RESET`, `key='SESSION AUTHORIZATION'`
- `SBLR3_RESET_TIME_ZONE` -> `SCHEMA_SET_SHOW_RESET` with `action=RESET`, `key='TIME ZONE'`

### JOB
- `SBLR3_EXECUTE_JOB` -> `SCHEMA_JOB_EXECUTE`
- `SBLR3_CANCEL_JOB_RUN` -> `SCHEMA_JOB_CANCEL`

### INDEX
- `SBLR3_INDEX_INSERT` -> `SCHEMA_INDEX_INSERT`
- `SBLR3_INDEX_DELETE` -> `SCHEMA_INDEX_DELETE`
- `SBLR3_INDEX_UPDATE` -> `SCHEMA_INDEX_UPDATE`
- `SBLR3_INDEX_SEARCH` / `SBLR3_INDEX_SCAN` -> `SCHEMA_INDEX_SCAN`
- `SBLR3_INDEX_SCAN_START` -> `SCHEMA_INDEX_SCAN_START`
- `SBLR3_INDEX_SCAN_NEXT` -> `SCHEMA_INDEX_SCAN_NEXT`
- `SBLR3_INDEX_SCAN_END` -> `SCHEMA_INDEX_SCAN_END`
- `SBLR3_INDEX_REINDEX` -> `SCHEMA_INDEX_REINDEX`
- `SBLR3_INDEX_VACUUM` -> `SCHEMA_INDEX_VACUUM`
- `SBLR3_INDEX_STATS` -> `SCHEMA_INDEX_STATS`
- `SBLR3_INDEX_TYPE` -> `SCHEMA_INDEX_TYPE`
- `SBLR3_COLUMNSTORE_*` / `SBLR3_GIN_*` / `SBLR3_HNSW_*` use their respective schemas below

### ARRAY / TEXTSEARCH / SPATIAL

All ARRAY, RANGE, TEXTSEARCH, and SPATIAL opcodes use `SCHEMA_FUNC_CALL` unless
explicitly listed with a specialized schema below.

### QUERY / WINDOW / INDEX REF

- `SBLR3_WINDOW_SPEC` -> `SCHEMA_WINDOW_SPEC`
- `SBLR3_WINDOW_ORDER_BY` -> `SCHEMA_WINDOW_ORDER_BY`
- `SBLR3_WINDOW` -> `SCHEMA_WINDOW_SPEC`
- `SBLR3_INDEX_REF` -> `SCHEMA_INDEX_REF`

## Additional Schemas (PSQL / Index / Job)

### PSQL_VAR_REF

```
[name:ident]
```

### SCHEMA_PSQL_ASSIGN

```
[target:PSQL_VAR_REF]
[value:expr]
```

### SCHEMA_PSQL_VAR_LOAD

```
[var:PSQL_VAR_REF]
```

### SCHEMA_PSQL_VAR_STORE

```
[var:PSQL_VAR_REF]
[value:expr]
```

### SCHEMA_PSQL_DECLARE

```
[decls:list<PSQL_DECL>]
```

### SCHEMA_PSQL_IF

```
[condition:expr]
[then_body:stmt_list]
[elsif:list<PSQL_ELSIF>]
[else_body:opt<stmt_list>]
```

### PSQL_ELSIF

```
[condition:expr]
[body:stmt_list]
```

### SCHEMA_PSQL_LOOP

```
[body:stmt_list]
```

### SCHEMA_PSQL_WHILE

```
[condition:expr]
[body:stmt_list]
```

### SCHEMA_PSQL_FOR_SELECT

```
[record:PSQL_VAR_REF]
[query:stmt]
[body:stmt_list]
```

### SCHEMA_PSQL_FOR_EXECUTE

```
[record:PSQL_VAR_REF]
[sql:expr]
[body:stmt_list]
```

### SCHEMA_PSQL_EXIT

```
[label:opt<ident>]
[when:opt<expr>]
```

### SCHEMA_PSQL_CONTINUE

```
[label:opt<ident>]
```

### SCHEMA_PSQL_RAISE

```
[sqlstate:opt<string>]
[message:opt<string>]
[params:list<expr>]
```

### SCHEMA_PSQL_RETURN

```
[value:opt<expr>]
```

### SCHEMA_PSQL_SUSPEND

```
<empty>
```

### SCHEMA_PSQL_LABEL

```
[label:ident]
```

### SCHEMA_PSQL_JUMP

```
[label:ident]
```

### SCHEMA_PSQL_JUMP_IF

```
[label:ident]
[condition:expr]
```

### SCHEMA_PSQL_CASE

```
[base:opt<expr>]
[when_count:varuint]
repeat when_count:
  [when:expr]
  [then:stmt_list]
[else:opt<stmt_list>]
```

### SCHEMA_PSQL_PARAM_IN

```
[param:SCHEMA_PSQL_PARAM]
```

### SCHEMA_PSQL_PARAM_OUT

```
[param:SCHEMA_PSQL_PARAM]
```

### SCHEMA_PSQL_PARAM_INOUT

```
[param:SCHEMA_PSQL_PARAM]
```

### SCHEMA_PSQL_TRY

```
[try_body:stmt_list]
[handlers:list<PSQL_EXCEPTION>]
```

### SCHEMA_PSQL_CALL

```
[proc_name:ident]
[args:expr_list]
```

### SCHEMA_PSQL_CURSOR_DECLARE

```
[cursor_name:ident]
[scroll:bool]
[query:stmt]
```

### SCHEMA_PSQL_CURSOR_OPEN

```
[cursor_name:ident]
```

### SCHEMA_PSQL_CURSOR_FETCH

```
[cursor_name:ident]
[direction:u8]            // 1=NEXT,2=PRIOR,3=FIRST,4=LAST,5=ABSOLUTE,6=RELATIVE
[offset:opt<i64>]
[target:PSQL_VAR_REF]
```

### SCHEMA_PSQL_CURSOR_CLOSE

```
[cursor_name:ident]
```

### SCHEMA_PSQL_PARAM

```
[name:ident]
[type:TYPE_SPEC]
```

### SCHEMA_PSQL_PROCEDURE

```
[name:ident]
[in_params:list<SCHEMA_PSQL_PARAM>]
[out_params:list<SCHEMA_PSQL_PARAM>]
[body:stmt_list]
```

### SCHEMA_PSQL_FUNCTION

```
[name:ident]
[in_params:list<SCHEMA_PSQL_PARAM>]
[return_type:TYPE_SPEC]
[body:stmt_list]
```

### SCHEMA_PSQL_POST_EVENT

```
[event_name:string]
```

### SCHEMA_INDEX_REF

```
[index_path:schema_path]
```

### INDEX_TYPE_SPEC

```
[type:u16]                // index type enum (matches catalog/index spec)
[options:OPTION_KV]       // type-specific options (empty if none)
```

#### Index Type-Specific Options (Normative)

The following option keys and value encodings are mandatory for each index
family when `INDEX_TYPE_SPEC.options` is used. All keys are case-insensitive
and MUST be canonicalized to lowercase identifiers before emission.

**BRIN (Block Range Index)**  
Use when `type = BRIN`.
```
brin_range_pages:u32        // pages per range (default 128)
brin_autosummarize:bool     // default false
brin_summary_mode:u8        // 1=MINMAX,2=MINMAX_MULTI
```

**GIST (Generalized Search Tree)**  
Use when `type = GIST`.
```
gist_fillfactor:u8          // 10..100 (default 90)
gist_buffering:u8           // 0=OFF,1=ON,2=AUTO
gist_penalty_mode:u8        // 1=DEFAULT,2=STRICT
```

**SPGIST (Space-Partitioned GiST)**  
Use when `type = SPGIST`.
```
spgist_fillfactor:u8        // 10..100 (default 90)
spgist_prefix_compress:bool // default true
spgist_leaf_compress:bool   // default true
```

**RTREE**  
Use when `type = RTREE`.
```
rtree_page_capacity:u16     // max entries per node
rtree_split_mode:u8         // 1=LINEAR,2=QUADRATIC,3=RSTAR
rtree_reinsert:bool         // default true (R*-tree)
```

**IVF (Inverted File / Vector)**  
Use when `type = IVF`.
```
ivf_lists:u32               // number of inverted lists (nlist)
ivf_probes:u32              // lists to probe at query time (nprobe)
ivf_distance:u8             // 1=L2,2=IP,3=COSINE
ivf_quantizer:u8            // 1=FLAT,2=PQ,3=SQ8
ivf_pq_m:u16                // sub-quantizers (when PQ)
ivf_pq_bits:u8              // bits per sub-quantizer (when PQ)
```

### SCHEMA_INDEX_KEY

```
[key_values:list<expr>]
[null_mask:opt<bytes>]
```

### SCHEMA_INDEX_INSERT

```
[index:SCHEMA_INDEX_REF]
[key:SCHEMA_INDEX_KEY]
[tuple_id:u64]
[xmin:u64]
```

### SCHEMA_INDEX_DELETE

```
[index:SCHEMA_INDEX_REF]
[key:SCHEMA_INDEX_KEY]
[tuple_id:u64]
[xmax:u64]
```

### SCHEMA_INDEX_UPDATE

```
[index:SCHEMA_INDEX_REF]
[old_key:SCHEMA_INDEX_KEY]
[new_key:SCHEMA_INDEX_KEY]
[tuple_id:u64]
[xmin:u64]
```

### SCHEMA_INDEX_SCAN

```
[index:SCHEMA_INDEX_REF]
[start_key:opt<SCHEMA_INDEX_KEY>]
[end_key:opt<SCHEMA_INDEX_KEY>]
[start_inclusive:bool]
[end_inclusive:bool]
[direction:u8]            // 1=ASC,2=DESC
[limit:opt<u64>]
[scan_flags:u16]          // bitmask (1=INDEX_ONLY,2=RETURN_TID,4=RETURN_KEY)
[filter:opt<expr>]        // post-index filter
```

### SCHEMA_INDEX_SCAN_START

```
[scan:SCHEMA_INDEX_SCAN]
```

### SCHEMA_INDEX_SCAN_NEXT

```
[scan_id:u64]
```

### SCHEMA_INDEX_SCAN_END

```
[scan_id:u64]
```

### SCHEMA_INDEX_REINDEX

```
[index:SCHEMA_INDEX_REF]
```

### SCHEMA_INDEX_VACUUM

```
[index:SCHEMA_INDEX_REF]
```

### SCHEMA_INDEX_STATS

```
[index:SCHEMA_INDEX_REF]
[with_histogram:bool]
```

### SCHEMA_INDEX_TYPE

```
[index:SCHEMA_INDEX_REF]
[type:INDEX_TYPE_SPEC]
```

### SCHEMA_COLUMNSTORE_INSERT

```
[index:SCHEMA_INDEX_REF]
[key:SCHEMA_INDEX_KEY]
[tuple_id:u64]
[xmin:u64]
[column_group:opt<u32>]
```

### SCHEMA_COLUMNSTORE_SCAN

```
[index:SCHEMA_INDEX_REF]
[projection:list<ident>]
[filter:opt<expr>]
[column_group:opt<u32>]
[segment:opt<u32>]
```

### SCHEMA_GIN_INSERT

```
[index:SCHEMA_INDEX_REF]
[lexemes:list<expr>]
[tuple_id:u64]
[xmin:u64]
[positions:opt<bytes>]    // encoded positions (if tracked)
[pending:bool]
```

### SCHEMA_GIN_SEARCH

```
[index:SCHEMA_INDEX_REF]
[query:expr]
[limit:opt<u64>]
[rank:bool]               // true to request ranked results
```

### SCHEMA_HNSW_INSERT

```
[index:SCHEMA_INDEX_REF]
[vector:expr]
[tuple_id:u64]
[xmin:u64]
[ef_construction:opt<u32>]
[level:opt<u8>]
```

### SCHEMA_HNSW_SEARCH

```
[index:SCHEMA_INDEX_REF]
[vector:expr]
[k:u32]
[ef_search:opt<u32>]
```

### SCHEMA_JOB_EXECUTE

```
[job_name:ident]
[run_id:opt<u64>]
[params:OPTION_KV]
```

### SCHEMA_JOB_CANCEL

```
[run_id:u64]
```

## Literal Opcode Map (Exhaustive)

All literal opcodes use canonical encodings from `types/VALUE_SPEC_STORAGE_ENCODINGS.md`.

- `SBLR3_LITERAL_NULL` -> empty payload
- `SBLR3_LITERAL_BOOLEAN` -> `u8`
- `SBLR3_LITERAL_INT8` / `INT16` / `INT32` / `INT64` / `INT128`
- `SBLR3_LITERAL_UINT8` / `UINT16` / `UINT32` / `UINT64` / `UINT128`
- `SBLR3_LITERAL_FLOAT32` / `DOUBLE`
- `SBLR3_LITERAL_DECIMAL`
- `SBLR3_LITERAL_MONEY`
- `SBLR3_LITERAL_UUID`
- `SBLR3_LITERAL_STRING`
- `SBLR3_LITERAL_BINARY`
- `SBLR3_LITERAL_DATE` / `TIME` / `TIMESTAMP` / `TIME_TZ` / `TIMESTAMP_TZ`
- `SBLR3_LITERAL_JSON` / `JSONB`
- `SBLR3_LITERAL_XML`
- `SBLR3_LITERAL_BIT`
- `SBLR3_LITERAL_YEAR`
- `SBLR3_LITERAL_DATETIME`
- `SBLR3_LITERAL_MEDIUMINT`
- `SBLR3_LITERAL_INTERVAL`
- `SBLR3_LITERAL_INET` / `CIDR`
- `SBLR3_LITERAL_MACADDR` / `MACADDR8`
- `SBLR3_LITERAL_GEOMETRY`
- `SBLR3_LITERAL_JSONPATH`
- `SBLR3_LITERAL_ENUM`
- `SBLR3_LITERAL_SET`
- `SBLR3_LITERAL_ROW` / `COMPOSITE` / `DOMAIN`
- `SBLR3_LITERAL_RANGE`
- `SBLR3_LITERAL_ARRAY`
- `SBLR3_LITERAL_VARIANT`
- `SBLR3_LITERAL_TSVECTOR` / `TSQUERY`
- `SBLR3_LITERAL_BLOB_LOCATOR`
