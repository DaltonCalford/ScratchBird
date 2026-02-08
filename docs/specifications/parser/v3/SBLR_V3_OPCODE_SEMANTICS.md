# V3 SBLR Opcode Semantics
Status: Authoritative (V3)
Last Updated: 2026-02-08

This document defines per-opcode runtime semantics for V3 SBLR. It must be used by executors and validators to ensure deterministic behavior.

## Conventions
- `stack_in` is the operand consumption pattern.
- `stack_out` is the value(s) pushed back to the stack.
- `effect` describes side effects and execution context requirements.
- `errors` lists required error categories; exact SQLSTATE values are handled by the error subsystem.

## Lock Ordering (Normative)
All opcodes that acquire multiple locks must follow this exact order to avoid deadlocks:
1. Catalog global DDL lock (if required)
2. Schema-level metadata lock
3. Object metadata lock (table/view/index/sequence/etc.)
4. Table DML lock (row-level intent)
5. Index locks (per index, ascending by index_id)
6. Row locks (primary key order)
7. TOAST/LOB locks (by LOB id)

Lock upgrades must follow the same order (never downgrade or reorder within a statement).

## CONTROL
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_END (0x0001) | - | - | End of instruction stream; commit implicit result if any and stop execution. | ERROR_INVALID_STREAM |
| SBLR3_VERSION (0x0002) | - | - | Verify bytecode version compatibility; abort if mismatch. | ERROR_UNSUPPORTED_VERSION |
| SBLR3_EXTENDED_OPCODE (0x0003) | - | - | Dispatch extended opcode (16-bit) and execute its handler. | ERROR_INVALID_STREAM |

## DDL
DDL opcodes mutate catalog state. They must acquire DDL locks, validate definitions, update dependency graphs, and invalidate relevant caches.
DDL opcodes must acquire locks in the order defined in Lock Ordering (Normative).
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_ALTER_JOB (0x0101) | - | - | Alter job schedule or target; update scheduler registration. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_SEQUENCE (0x0102) | - | - | Alter sequence parameters; validate bounds and cache; update stored state. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_TABLE (0x0103) | - | - | Alter table definition; apply column/constraint/index/table options in payload order; validate dependencies and data compatibility. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_TABLESPACE (0x0104) | - | - | Alter tablespace metadata/options; validate path/engine constraints. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_TABLE_SET_TABLESPACE (0x0105) | - | - | Move table storage to target tablespace; lock table and indexes; copy/relocate data and rebuild indexes as needed. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ATTACH_TABLESPACE (0x0106) | - | - | Attach existing tablespace files to catalog; validate format and ownership. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CHECK_CONSTRAINT (0x0107) | - | - | Define CHECK constraint; validate expression boolean; verify existing rows if NOT VALID not set; register deferrability. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_COLUMN_CHARSET (0x0108) | - | - | Assign column character set; validate charset availability; update collation defaults. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_COLUMN_COLLATE (0x0109) | - | - | Assign column collation; validate compatibility with charset; update collation metadata. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_INDEX (0x010A) | - | - | Create index metadata; lock table; build index over existing rows; validate expressions and collations; mark index valid on success. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_JOB (0x010B) | - | - | Create job metadata; validate schedule and target procedure; register with scheduler. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_SEQUENCE (0x010C) | - | - | Create sequence metadata; initialize sequence value and cache parameters. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_TABLE (0x010D) | - | - | Create table metadata; validate definition; register dependencies and default privileges. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_TABLESPACE (0x010E) | - | - | Create tablespace metadata; validate path/engine; initialize storage files if required. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_VIEW (0x010F) | - | - | Create view metadata; store query definition and dependencies; validate referenced objects and columns. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DEFAULT_VALUE (0x0110) | - | - | Define default expression for column/domain; validate type compatibility; store expression. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DETACH_TABLESPACE (0x0111) | - | - | Detach tablespace from catalog; ensure no active references. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_INDEX (0x0112) | - | - | Drop index; validate ownership; remove index metadata and storage. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_JOB (0x0113) | - | - | Drop job; enforce dependency checks and CASCADE/RESTRICT. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_SEQUENCE (0x0114) | - | - | Drop sequence; enforce dependency checks and CASCADE/RESTRICT. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_TABLE (0x0115) | - | - | Drop table; check dependencies; drop indexes/triggers; release storage; apply CASCADE/RESTRICT rules. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_TABLESPACE (0x0116) | - | - | Drop tablespace; ensure no objects reference it; release storage. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_VIEW (0x0117) | - | - | Drop view; enforce dependency checks and CASCADE/RESTRICT. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_COLUMN_DEFAULT (0x0118) | - | - | Set/clear column default expression; validate expression type; update catalog and dependency graph. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_DATABASE (0x0119) | - | - | Alter database-level options (default collation, timezone, compatibility flags); update catalog and session defaults. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_DEFAULT_PRIVILEGES (0x011A) | - | - | Alter default privileges for schema/object classes; update privilege templates. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_DOMAIN (0x011B) | - | - | Modify domain constraints/defaults; validate dependent columns; rebind or mark for validation. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_ELEMENT (0x011C) | - | - | Alter element of composite/array type; validate element type and dependencies. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_FUNCTION_STMT (0x011D) | - | - | Alter routine definition; recompile body; update dependencies and invalidations. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_INDEX (0x011E) | - | - | Alter index options (fillfactor, predicate, expression); rebuild if required; update metadata. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_POLICY (0x0120) | - | - | Alter policy definition; validate expressions; update enforcement order. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_PROCEDURE_STMT (0x0121) | - | - | Alter routine definition; recompile body; update dependencies and invalidations. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_ROLE (0x0122) | - | - | Alter role attributes (password, membership, defaults); validate security rules. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_SCHEMA (0x0123) | - | - | Alter schema properties (owner, comment, options); update catalog. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_SYSTEM (0x0124) | - | - | Alter system-wide configuration parameters; apply dynamic changes or mark restart-required. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_TABLE_RLS (0x0126) | - | - | Enable/disable row-level security on table; validate policies and update enforcement flags. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_USER (0x0128) | - | - | Alter user attributes (password, membership, defaults); validate security rules. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_DATABASE (0x0129) | - | - | Create database catalog root and system schemas; allocate bootstrap pages; initialize system domains and tables. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_DB_TRIGGER (0x012B) | - | - | Create trigger metadata; validate timing/event/target; compile trigger body; register dependencies and firing order. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_DOMAIN (0x012D) | - | - | Create domain metadata; validate base type and constraints; register default and check constraints. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_EXCEPTION_STMT (0x012E) | - | - | Create exception definition with SQLSTATE and message; store in catalog. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_FOREIGN_SERVER (0x012F) | - | - | Create foreign server metadata; validate FDW and options; store connection parameters. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_FOREIGN_TABLE (0x0130) | - | - | Create foreign table metadata; validate server/options; store column mapping and FDW options. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_FUNCTION_STMT (0x0132) | - | - | Create function stmt metadata; validate signature and body; compile/plan if required; register dependencies. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_GROUP (0x0134) | - | - | Create group in security catalog; validate name uniqueness and password/attributes. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_PACKAGE_STMT (0x0136) | - | - | Create package stmt metadata; validate signature and body; compile/plan if required; register dependencies. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_POLICY (0x0138) | - | - | Create row-level security policy; validate target table and USING/CHECK expressions; register policy order. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_PROCEDURE_STMT (0x013A) | - | - | Create procedure stmt metadata; validate signature and body; compile/plan if required; register dependencies. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_ROLE (0x013C) | - | - | Create role in security catalog; validate name uniqueness and password/attributes. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_SCHEMA (0x013D) | - | - | Create schema metadata; validate definition; register dependencies and default privileges. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_SYNONYM (0x013E) | - | - | Create synonym mapping; validate target object and privileges; store resolution rules. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_TABLE_AS (0x013F) | - | - | Create table metadata, allocate storage, and populate using SELECT pipeline; apply constraints after load unless payload specifies WITH NO DATA. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_TRIGGER (0x0141) | - | - | Create trigger metadata; validate timing/event/target; compile trigger body; register dependencies and firing order. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_TYPE (0x0143) | - | - | Create user-defined type; validate element definitions; store type descriptor and dependencies. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_UDR (0x0144) | - | - | Create udr metadata; validate signature and body; compile/plan if required; register dependencies. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_USER (0x0146) | - | - | Create user in security catalog; validate name uniqueness and password/attributes. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_USER_MAPPING (0x0147) | - | - | Create user mapping for foreign server; validate mapping options and target. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_DATABASE (0x0148) | - | - | Drop database catalog and storage; terminate connections; remove files. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_DB_TRIGGER (0x014A) | - | - | Drop trigger metadata and compiled body; update dependencies. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_DOMAIN (0x014B) | - | - | Drop domain; enforce dependency restrictions; remove constraints and defaults. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_EXCEPTION_STMT (0x014C) | - | - | Drop exception definition; invalidate dependent code. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_FOREIGN_SERVER (0x014D) | - | - | Drop foreign server/table; validate no dependent objects; remove metadata. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_FOREIGN_TABLE (0x014E) | - | - | Drop foreign server/table; validate no dependent objects; remove metadata. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_FUNCTION_STMT (0x0150) | - | - | Drop routine/package metadata; invalidate dependents; release compiled artifacts. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_GROUP (0x0152) | - | - | Drop group; revoke memberships; clean security metadata. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_PACKAGE_STMT (0x0154) | - | - | Drop routine/package metadata; invalidate dependents; release compiled artifacts. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_POLICY (0x0156) | - | - | Drop row-level security policy; remove enforcement from target table. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_PROCEDURE_STMT (0x0158) | - | - | Drop routine/package metadata; invalidate dependents; release compiled artifacts. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_ROLE (0x015A) | - | - | Drop role; revoke memberships; clean security metadata. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_SCHEMA (0x015B) | - | - | Drop schema; enforce CASCADE/RESTRICT; remove contained objects and privileges. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_SYNONYM (0x015C) | - | - | Drop synonym mapping; update name resolution cache. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_TRIGGER (0x015E) | - | - | Drop trigger metadata and compiled body; update dependencies. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_UDR (0x015F) | - | - | Drop routine/package metadata; invalidate dependents; release compiled artifacts. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_USER (0x0161) | - | - | Drop user; revoke memberships; clean security metadata. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_USER_MAPPING (0x0162) | - | - | Drop user mapping; enforce dependency checks and CASCADE/RESTRICT. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_FB_ALTER_MAPPING (0x0163) | - | - | Manage authentication or user mappings (Firebird compatibility); update mapping catalog. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_FB_CREATE_MAPPING (0x0164) | - | - | Manage authentication or user mappings (Firebird compatibility); update mapping catalog. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_FB_CREATE_SHADOW (0x0165) | - | - | Create shadow database metadata; validate shadow file location. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_FB_DROP_MAPPING (0x0166) | - | - | Manage authentication or user mappings (Firebird compatibility); update mapping catalog. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_FB_DROP_SHADOW (0x0167) | - | - | Drop shadow metadata; release shadow storage. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_FIRE_DB_TRIGGER (0x0169) | - | - | Execute trigger body as part of DDL or DML event; establish trigger context and execute statements. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_FIRE_TRIGGER (0x016B) | - | - | Execute trigger body as part of DDL or DML event; establish trigger context and execute statements. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_MOVE_OBJECT (0x016C) | - | - | Move object between schemas/namespaces; update path references and dependencies. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_REBIND_DOMAIN (0x016D) | - | - | Rebind domain to dependent columns; revalidate constraints if required. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_RENAME_COLUMN (0x016E) | - | - | Rename column; update dependencies, indexes, constraints, and view definitions. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_RENAME_OBJECT (0x016F) | - | - | Rename object; update name resolution cache and dependencies. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_RESOLVE_DOMAIN_CONFLICT (0x0170) | - | - | Resolve conflicting domain definitions across schemas; apply selected canonical domain. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_TABLE_INHERITS (0x0171) | - | - | Define table inheritance relationship; merge columns/constraints; update dependency graph. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_TABLE_OPTIONS (0x0172) | - | - | Apply table storage/options (fillfactor, autovacuum, compression, toast) to metadata. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_TABLE_PARTITIONING (0x0173) | - | - | Define/alter table partitioning scheme; validate partition bounds and keys; update routing rules. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_FOREIGN_KEY (0x0174) | - | - | Define foreign key constraint; validate referenced unique/PK; enforce match type; check existing rows if required; create supporting index if missing. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_GENERATED_COLUMN (0x0175) | - | - | Define generated column expression; validate deterministic expression and type; materialize or virtual per payload. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_IDENTITY_COLUMN (0x0176) | - | - | Define identity column; bind to sequence; set generation mode and increment bounds. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_NOT_NULL (0x0177) | - | - | Define NOT NULL constraint; validate existing rows; register constraint. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_PRIMARY_KEY (0x0178) | - | - | Define primary key constraint; create unique index; validate uniqueness of existing rows; mark columns NOT NULL. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_REFRESH_MATERIALIZED_VIEW (0x0179) | - | - | Refresh materialized view; recompute data using stored query; apply CONCURRENTLY rules if specified. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_TABLE_FK (0x017A) | - | - | Define foreign key constraint; validate referenced unique/PK; enforce match type; check existing rows if required; create supporting index if missing. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_TRUNCATE_TABLE (0x017B) | - | - | Truncate table data; optionally cascade to dependent tables; reset identity/sequence per payload; release storage pages. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_UNIQUE_CONSTRAINT (0x017C) | - | - | Define unique constraint; create unique index; validate uniqueness of existing rows. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_PUBLIC_SYNONYM (0x017D) | - | - | Create synonym mapping; validate target object and privileges; store resolution rules. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_CREATE_EXCEPTION (0x017E) | - | - | Create exception definition with SQLSTATE and message; store in catalog. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_TYPE (0x017F) | - | - | Alter user-defined type definition; validate compatibility; rebuild dependent objects if necessary. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_FOREIGN_SERVER (0x0180) | - | - | Alter foreign server options; validate updated configuration. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_USER_MAPPING (0x0181) | - | - | Alter user mapping options for foreign server. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_SYNONYM (0x0182) | - | - | Alter synonym target or attributes; validate new target object. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_PUBLIC_SYNONYM (0x0183) | - | - | Alter synonym target or attributes; validate new target object. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_UDR (0x0184) | - | - | Alter routine definition; recompile body; update dependencies and invalidations. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_ALTER_EXCEPTION (0x0185) | - | - | Alter exception SQLSTATE/message; update catalog. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_PUBLIC_SYNONYM (0x0186) | - | - | Drop synonym mapping; update name resolution cache. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_EXCEPTION (0x0187) | - | - | Drop exception definition; invalidate dependent code. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |
| SBLR3_DROP_TYPE (0x0188) | - | - | Drop user-defined type; enforce dependency restrictions. | ERROR_PERMISSION, ERROR_INVALID_DDL, ERROR_DEPENDENCY, ERROR_CONSTRAINT, ERROR_LOCK_CONFLICT, ERROR_STORAGE |

## DML
DML opcodes execute against MVCC snapshots and must enforce domain and table constraints before commit.
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_DELETE (0x0201) | - | push(rows_affected) | Delete rows; enforce FK actions (RESTRICT/CASCADE/SET NULL/SET DEFAULT); update indexes and storage. | ERROR_PERMISSION, ERROR_CONSTRAINT, ERROR_TYPE_MISMATCH, ERROR_LOCK_CONFLICT |
| SBLR3_COPY (0x0202) | - | push(rows_affected) | Bulk import/export rows per COPY options; enforce constraints unless disabled; update indexes in batch mode. | ERROR_PERMISSION, ERROR_CONSTRAINT, ERROR_TYPE_MISMATCH, ERROR_LOCK_CONFLICT |
| SBLR3_MERGE_END (0x0204) | - | - | Finalize MERGE; enforce constraints; update indexes; release merge context. | ERROR_PERMISSION, ERROR_CONSTRAINT, ERROR_TYPE_MISMATCH, ERROR_LOCK_CONFLICT |
| SBLR3_MERGE_ON (0x0206) | - | - | Evaluate join predicate between source and target; establish match state for each source row. | ERROR_PERMISSION, ERROR_CONSTRAINT, ERROR_TYPE_MISMATCH, ERROR_LOCK_CONFLICT |
| SBLR3_MERGE_SOURCE (0x0208) | - | - | Evaluate source query and build source rowset stream for MERGE. | ERROR_PERMISSION, ERROR_CONSTRAINT, ERROR_TYPE_MISMATCH, ERROR_LOCK_CONFLICT |
| SBLR3_MERGE_START (0x020A) | - | - | Initialize MERGE context; bind target table and merge clauses; establish snapshot and locks. | ERROR_PERMISSION, ERROR_CONSTRAINT, ERROR_TYPE_MISMATCH, ERROR_LOCK_CONFLICT |
| SBLR3_MERGE_WHEN_MATCHED (0x020C) | - | - | Apply matched clause actions (UPDATE/DELETE) for rows where join predicate matches. | ERROR_PERMISSION, ERROR_CONSTRAINT, ERROR_TYPE_MISMATCH, ERROR_LOCK_CONFLICT |
| SBLR3_MERGE_WHEN_NOT_MATCHED (0x020E) | - | - | Apply not-matched-by-target actions (INSERT) for source rows without matches. | ERROR_PERMISSION, ERROR_CONSTRAINT, ERROR_TYPE_MISMATCH, ERROR_LOCK_CONFLICT |
| SBLR3_MERGE_WHEN_NOT_MATCHED_SOURCE (0x0210) | - | - | Apply not-matched-by-source actions to target rows not matched by source (DELETE/UPDATE) if specified. | ERROR_PERMISSION, ERROR_CONSTRAINT, ERROR_TYPE_MISMATCH, ERROR_LOCK_CONFLICT |
| SBLR3_INSERT (0x0211) | pop(values...) | push(rows_affected) | Insert rows; apply defaults; constraint order: domain -> NOT NULL -> CHECK -> FK -> UNIQUE/PK; register deferrable constraints; update indexes. | ERROR_PERMISSION, ERROR_CONSTRAINT, ERROR_TYPE_MISMATCH, ERROR_LOCK_CONFLICT |
| SBLR3_SELECT (0x0212) | - | push(result_stream) | Execute query pipeline; honor isolation level and RLS; acquire read locks for FOR UPDATE/SHARE; return result stream. | ERROR_PERMISSION, ERROR_CONSTRAINT, ERROR_TYPE_MISMATCH, ERROR_LOCK_CONFLICT |
| SBLR3_UPDATE (0x0213) | pop(values...) | push(rows_affected) | Update rows; apply generated columns; constraint order: domain -> NOT NULL -> CHECK -> FK -> UNIQUE/PK; register deferrable constraints; update indexes. | ERROR_PERMISSION, ERROR_CONSTRAINT, ERROR_TYPE_MISMATCH, ERROR_LOCK_CONFLICT |
| SBLR3_FOR_UPDATE (0x0214) | - | - | Apply row locking mode to subsequent SELECT/UPDATE pipeline. | ERROR_PERMISSION, ERROR_CONSTRAINT, ERROR_TYPE_MISMATCH, ERROR_LOCK_CONFLICT |
| SBLR3_FOR_SHARE (0x0215) | - | - | Apply row locking mode to subsequent SELECT/UPDATE pipeline. | ERROR_PERMISSION, ERROR_CONSTRAINT, ERROR_TYPE_MISMATCH, ERROR_LOCK_CONFLICT |
| SBLR3_FETCH_FIRST (0x0216) | - | - | Apply LIMIT/OFFSET or pagination semantics to result stream. | ERROR_PERMISSION, ERROR_CONSTRAINT, ERROR_TYPE_MISMATCH, ERROR_LOCK_CONFLICT |
| SBLR3_FETCH_NEXT (0x0217) | - | - | Apply LIMIT/OFFSET or pagination semantics to result stream. | ERROR_PERMISSION, ERROR_CONSTRAINT, ERROR_TYPE_MISMATCH, ERROR_LOCK_CONFLICT |
| SBLR3_FETCH_ONLY (0x0218) | - | - | Apply LIMIT/OFFSET or pagination semantics to result stream. | ERROR_PERMISSION, ERROR_CONSTRAINT, ERROR_TYPE_MISMATCH, ERROR_LOCK_CONFLICT |

## TXN
Transaction opcodes control snapshot lifetimes and lock scopes. They are illegal inside nested statement contexts unless explicitly allowed by payload.
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_COMMIT (0x0301) | - | - | Commit transaction; validate deferrable constraints; flush WAL; release locks. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_COMMIT_PREPARED (0x0302) | - | - | Two-phase commit operation as defined by payload; validate prepared transaction state. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_COMMIT_RETAINING (0x0303) | - | - | Commit current transaction and immediately start a new transaction with retained session context and new snapshot. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_DEALLOCATE_PREPARED (0x0304) | - | - | Two-phase commit operation as defined by payload; validate prepared transaction state. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_EXECUTE_PREPARED (0x0305) | - | - | Two-phase commit operation as defined by payload; validate prepared transaction state. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_FUNC_CURRENT_TRANSACTION (0x0306) | - | push(value) | Return current transaction ID. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_PREPARE_TRANSACTION (0x0307) | - | - | Two-phase commit operation as defined by payload; validate prepared transaction state. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_RELEASE_SAVEPOINT (0x0309) | - | - | Release savepoint; merge boundary into parent context. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_ROLLBACK_PREPARED (0x030A) | - | - | Two-phase commit operation as defined by payload; validate prepared transaction state. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_ROLLBACK_RETAINING (0x030B) | - | - | Rollback current transaction and immediately start a new transaction with retained session context and new snapshot. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_ROLLBACK_TO_SAVEPOINT (0x030D) | - | - | Rollback to savepoint; discard changes after savepoint; retain transaction. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_SAVEPOINT (0x030F) | - | push(id) | Create savepoint; mark rollback boundary. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_SAVEPOINT_BEGIN (0x0310) | - | - | Begin explicit savepoint block with a name/label; nest savepoint context. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_SAVEPOINT_END (0x0311) | - | - | End explicit savepoint block; if no errors, release savepoint; otherwise rollback to it. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_SET_AUTOCOMMIT (0x0312) | - | - | Enable or disable autocommit for session; adjust implicit transaction behavior. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_SHOW_TRANSACTION_LEVEL (0x0314) | - | push(value) | Return current isolation level and access mode. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_ROLLBACK (0x0315) | - | - | Rollback transaction; discard changes; release locks. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_SET_TRANSACTION (0x0316) | - | - | Set transaction options for current or next transaction (isolation, read/write, timeout). | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_START_TRANSACTION (0x0317) | - | push(id) | Begin new transaction with payload isolation/flags; establish snapshot and lock table. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |
| SBLR3_SWEEP (0x0318) | - | - | Trigger garbage collection/sweep for old transaction versions. | ERROR_TXN_STATE, ERROR_LOCK_CONFLICT, ERROR_INVALID_TRANSACTION |

## DCL
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_APPLY_DOMAIN_MASKING (0x0401) | - | - | Apply masking policy to domain value before output; enforce masking rules and user context. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_APPLY_QUALITY_PIPELINE (0x0402) | - | - | Apply data quality pipeline to domain value (validation, normalization, enrichment) as configured. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_AUDIT_DOMAIN_ACCESS (0x0403) | - | - | Record audit event for domain access with user, object, and operation metadata. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_CHECK_DOMAIN_CONSTRAINT (0x0404) | - | - | Evaluate domain constraints for value; return error on violation. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_CHECK_DOMAIN_PRIVILEGE (0x0405) | - | - | Verify subject has privilege to access domain or masked value. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_CHECK_GLOBAL_UNIQUENESS (0x0406) | - | - | Check uniqueness across cluster/global catalog as required by constraint. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_DECRYPT_DOMAIN_VALUE (0x0407) | - | - | Decrypt domain value using configured key/algorithm; validate MAC if present. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_ENCRYPT_DOMAIN_VALUE (0x0408) | - | - | Encrypt domain value using configured key/algorithm; attach metadata for decryption. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_GRANT (0x040A) | - | - | Grant privileges to grantees; update privilege catalog; validate grantor authority. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_GRANT_OPTION (0x040C) | - | - | Grant WITH GRANT OPTION for privileges; update privilege catalog. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_GRANT_PRIVILEGE (0x040E) | - | - | Grant specific object or schema privilege; validate privilege scope and target. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_GRANT_ROLE (0x0410) | - | - | Grant role membership to grantee; validate role hierarchy and admin option. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_NORMALIZE_DOMAIN_VALUE (0x0411) | - | - | Normalize domain value (case-folding, trimming, canonicalization) per domain rules. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_PRIVILEGE (0x0413) | - | - | Construct/evaluate privilege descriptor for current security context and target. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_REVOKE (0x0415) | - | - | Revoke privileges; apply cascade rules and dependent privilege cleanup. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_REVOKE_PRIVILEGE (0x0417) | - | - | Revoke specific object or schema privilege. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_REVOKE_ROLE (0x0419) | - | - | Revoke role membership; update role graph. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_SET_CONSTRAINTS (0x041B) | - | - | Set constraint checking mode (IMMEDIATE/DEFERRED) for current transaction. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_SET_ROLE (0x041D) | - | - | Set active role for session; validate role membership and defaults. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_SET_SESSION_AUTH (0x041F) | - | - | Set session authorization identity; re-evaluate effective privileges. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |
| SBLR3_VALIDATE_DOMAIN_VALUE (0x0420) | - | - | Validate domain value (type, range, constraints); return success or error. | ERROR_PERMISSION, ERROR_INVALID_SECURITY |

## SESSION
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_RESET (0x0565) | - | - | Reset specified session parameter to default. | ERROR_SESSION_STATE, ERROR_PERMISSION, ERROR_INVALID_SETTING |
| SBLR3_RESET_ALL (0x0566) | - | - | Reset all session parameters to defaults; clear overrides. | ERROR_SESSION_STATE, ERROR_PERMISSION, ERROR_INVALID_SETTING |
| SBLR3_RESET_ROLE (0x0567) | - | - | Reset active role to default or NONE. | ERROR_SESSION_STATE, ERROR_PERMISSION, ERROR_INVALID_SETTING |
| SBLR3_RESET_SESSION_AUTH (0x0568) | - | - | Reset session authorization to original authenticated identity. | ERROR_SESSION_STATE, ERROR_PERMISSION, ERROR_INVALID_SETTING |
| SBLR3_RESET_TIME_ZONE (0x0569) | - | - | Reset session time zone to system or database default. | ERROR_SESSION_STATE, ERROR_PERMISSION, ERROR_INVALID_SETTING |
| SBLR3_SET_TIME_ZONE (0x056A) | - | - | Set session parameter; validate scope and value; update session state. | ERROR_SESSION_STATE, ERROR_PERMISSION, ERROR_INVALID_SETTING |

## QUERY
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_ASSIGNMENT (0x0601) | - | push(query_node) | Bind assignment expression (target := expr) for UPDATE/INSERT/SET clauses. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_BEGIN_LIST (0x0602) | - | push(query_node) | Begin/end list context for multi-element payloads. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_COLUMN_DEF (0x0603) | - | push(query_node) | Define column expression in target list; bind alias and type. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_COLUMN_REF (0x0604) | - | push(query_node) | Resolve column reference in query scope; bind to column metadata. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_END_LIST (0x0605) | - | push(query_node) | Begin/end list context for multi-element payloads. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_CTE_DEF (0x0607) | - | push(query_node) | Define a CTE with name and subquery; register in namespace. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_CTE_SCAN (0x0609) | - | push(query_node) | Scan CTE result set as a relation in FROM. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_DISTINCT_ON (0x060A) | - | push(query_node) | Apply DISTINCT ON with specified key list; select first row per group by ORDER BY. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_EXCEPT (0x060C) | - | push(query_node) | Combine two input streams with set operation; enforce type compatibility and duplicate handling. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_EXCEPT_ALL (0x060E) | - | push(query_node) | Combine two input streams with set operation; enforce type compatibility and duplicate handling. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_GROUPING_FUNC (0x0610) | - | push(query_node) | Compute GROUPING() indicator for grouping sets. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_GROUP_CUBE (0x0612) | - | push(query_node) | Apply GROUP BY CUBE expansion to grouping sets. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_GROUP_GROUPING_SETS (0x0614) | - | push(query_node) | Apply GROUPING SETS to define grouping combinations. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_GROUP_ROLLUP (0x0616) | - | push(query_node) | Apply GROUP BY ROLLUP expansion to grouping sets. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_INSERTED_COLUMN_REF (0x0617) | - | push(query_node) | Reference to inserted/updated column value (e.g., in RETURNING). | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_INTERSECT (0x0619) | - | push(query_node) | Combine two input streams with set operation; enforce type compatibility and duplicate handling. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_INTERSECT_ALL (0x061B) | - | push(query_node) | Combine two input streams with set operation; enforce type compatibility and duplicate handling. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_IN_LIST (0x061D) | - | push(query_node) | Construct IN list operand for IN predicate; enforce element type compatibility. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_ON_CONFLICT (0x061F) | - | push(query_node) | Define ON CONFLICT target/action for INSERT; bind constraint or index columns and optional WHERE. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_ON_CONFLICT_COLUMN (0x0621) | - | push(query_node) | Define ON CONFLICT target/action for INSERT; bind constraint or index columns and optional WHERE. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_ON_CONFLICT_CONSTRAINT (0x0623) | - | push(query_node) | Define ON CONFLICT target/action for INSERT; bind constraint or index columns and optional WHERE. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_ON_CONFLICT_DO_NOTHING (0x0625) | - | push(query_node) | Define ON CONFLICT target/action for INSERT; bind constraint or index columns and optional WHERE. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_ON_CONFLICT_DO_UPDATE (0x0627) | - | push(query_node) | Define ON CONFLICT target/action for INSERT; bind constraint or index columns and optional WHERE. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_ON_CONFLICT_WHERE (0x0629) | - | push(query_node) | Define ON CONFLICT target/action for INSERT; bind constraint or index columns and optional WHERE. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_RETURNING (0x062B) | - | push(query_node) | Attach RETURNING clause to DML to output modified rows. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_SELECT_TABLE_STAR (0x062D) | - | push(query_node) | Expand star to all visible columns in scope; apply column privilege checks. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_SUBQUERY_ARRAY (0x062F) | - | push(query_node) | Embed subquery as expression or FROM source; enforce correlation rules and return scalar/array/exists/in semantics. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_SUBQUERY_END (0x0631) | - | push(query_node) | Embed subquery as expression or FROM source; enforce correlation rules and return scalar/array/exists/in semantics. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_SUBQUERY_EXISTS (0x0633) | - | push(query_node) | Embed subquery as expression or FROM source; enforce correlation rules and return scalar/array/exists/in semantics. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_SUBQUERY_IN (0x0635) | - | push(query_node) | Embed subquery as expression or FROM source; enforce correlation rules and return scalar/array/exists/in semantics. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_SUBQUERY_NOT_IN (0x0637) | - | push(query_node) | Embed subquery as expression or FROM source; enforce correlation rules and return scalar/array/exists/in semantics. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_SUBQUERY_SCALAR (0x0639) | - | push(query_node) | Embed subquery as expression or FROM source; enforce correlation rules and return scalar/array/exists/in semantics. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_UNION (0x063B) | - | push(query_node) | Combine two input streams with set operation; enforce type compatibility and duplicate handling. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_UNION_ALL (0x063D) | - | push(query_node) | Combine two input streams with set operation; enforce type compatibility and duplicate handling. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_WITH_CLAUSE (0x063F) | - | push(query_node) | Define common table expressions; bind into query namespace. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_FRAME_CLAUSE (0x0640) | - | push(query_node) | Define window frame clause or boundary (ROWS/RANGE/GROUPS) and frame endpoints. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_FRAME_CURRENT_ROW (0x0641) | - | push(query_node) | Define window frame clause or boundary (ROWS/RANGE/GROUPS) and frame endpoints. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_FRAME_FOLLOWING (0x0642) | - | push(query_node) | Define window frame clause or boundary (ROWS/RANGE/GROUPS) and frame endpoints. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_FRAME_GROUPS (0x0643) | - | push(query_node) | Define window frame clause or boundary (ROWS/RANGE/GROUPS) and frame endpoints. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_FRAME_PRECEDING (0x0644) | - | push(query_node) | Define window frame clause or boundary (ROWS/RANGE/GROUPS) and frame endpoints. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_FRAME_RANGE (0x0645) | - | push(query_node) | Define window frame clause or boundary (ROWS/RANGE/GROUPS) and frame endpoints. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_FRAME_ROWS (0x0646) | - | push(query_node) | Define window frame clause or boundary (ROWS/RANGE/GROUPS) and frame endpoints. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_FRAME_UNBOUNDED_FOLLOWING (0x0647) | - | push(query_node) | Define window frame clause or boundary (ROWS/RANGE/GROUPS) and frame endpoints. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_FRAME_UNBOUNDED_PRECEDING (0x0648) | - | push(query_node) | Define window frame clause or boundary (ROWS/RANGE/GROUPS) and frame endpoints. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_GROUP_BY (0x0649) | - | push(query_node) | Group input rows by specified keys; prepare aggregate context. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_HASH_JOIN (0x064A) | - | push(query_node) | Perform join between left/right input streams using join type and join condition. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_HAVING (0x064B) | - | push(query_node) | Apply post-aggregate filter to grouped rows. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_INDEX_REF (0x064C) | - | push(query_node) | Reference index in query plan or hint; bind to index metadata. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_JOIN_CONDITION (0x064D) | - | push(query_node) | Perform join between left/right input streams using join type and join condition. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_JOIN_TYPE (0x064E) | - | push(query_node) | Perform join between left/right input streams using join type and join condition. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_JOIN_USING (0x064F) | - | push(query_node) | Perform join between left/right input streams using join type and join condition. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_LIMIT (0x0650) | - | push(query_node) | Apply limit/offset to input stream. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_NESTED_LOOP_JOIN (0x0651) | - | push(query_node) | Perform join between left/right input streams using join type and join condition. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_NULLS_FIRST (0x0652) | - | push(query_node) | Set NULL ordering for ORDER BY key. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_NULLS_LAST (0x0653) | - | push(query_node) | Set NULL ordering for ORDER BY key. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_OFFSET (0x0654) | - | push(query_node) | Apply limit/offset to input stream. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_ORDER_BY (0x0655) | - | push(query_node) | Sort input rows according to sort keys and collations. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_PARTITION_BY (0x0656) | - | push(query_node) | Define PARTITION BY clause for window specification. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_SCAN_HINT (0x0657) | - | push(query_node) | Provide scan or index usage hint to optimizer. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_SELECT_STAR (0x0658) | - | push(query_node) | Expand star to all visible columns in scope; apply column privilege checks. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_SORT_ASC (0x0659) | - | push(query_node) | Set sort direction for ORDER BY key. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_SORT_DESC (0x065A) | - | push(query_node) | Set sort direction for ORDER BY key. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_SORT_KEY (0x065B) | - | push(query_node) | Define an ORDER BY key expression. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_TABLE_REF (0x065C) | - | push(query_node) | Bind table reference into FROM clause; resolve schema path and alias. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_WHERE_CLAUSE (0x065D) | - | push(query_node) | Define WHERE clause predicate node. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_WINDOW (0x065E) | - | push(query_node) | Attach window definitions to query pipeline for window function evaluation. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_WINDOW_ORDER_BY (0x065F) | - | push(query_node) | Define ORDER BY for window specification. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_WINDOW_SPEC (0x0660) | - | push(query_node) | Define window specification (partition, order, frame) for named or inline windows. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_LATERAL (0x0661) | - | push(query_node) | Mark subquery or function as LATERAL; allow correlation to preceding FROM items. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |
| SBLR3_USING_CLAUSE (0x0662) | - | push(query_node) | Define USING clause for JOIN; generate equality predicates for named columns. | ERROR_QUERY, ERROR_TYPE_MISMATCH, ERROR_PERMISSION |

## EXPR
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_CASE_WHEN (0x0701) | pop(args...) | push(result) | Evaluate CASE WHEN predicate and yield corresponding result; used inside CASE expression evaluation. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_COALESCE (0x0702) | pop(args...) | push(result) | Return first non-NULL argument; evaluate left-to-right. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_ADD (0x0703) | pop(left,right) | push(result) | Evaluate arithmetic operation with numeric coercion; propagate NULL on NULL operands. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_AND (0x0704) | pop(left,right) | push(result) | Evaluate boolean operation with three-valued logic; short-circuit where applicable. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_CAST (0x0705) | pop(left,right) | push(result) | Cast value to target type; enforce cast rules and overflow checks. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_DIVIDE (0x0706) | pop(left,right) | push(result) | Evaluate arithmetic operation with numeric coercion; propagate NULL on NULL operands. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_EQ (0x0707) | pop(left,right) | push(result) | Compare two values with type coercion and collation rules; return boolean or NULL (NULL_SAFE_EQ is NULL-safe). | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_GE (0x0708) | pop(left,right) | push(result) | Compare two values with type coercion and collation rules; return boolean or NULL (NULL_SAFE_EQ is NULL-safe). | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_GT (0x0709) | pop(left,right) | push(result) | Compare two values with type coercion and collation rules; return boolean or NULL (NULL_SAFE_EQ is NULL-safe). | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_ILIKE (0x070A) | pop(left,right) | push(result) | Pattern match using LIKE semantics, optional ESCAPE, and collation rules; return boolean. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_LE (0x070B) | pop(left,right) | push(result) | Compare two values with type coercion and collation rules; return boolean or NULL (NULL_SAFE_EQ is NULL-safe). | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_LIKE (0x070C) | pop(left,right) | push(result) | Pattern match using LIKE semantics, optional ESCAPE, and collation rules; return boolean. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_LT (0x070D) | pop(left,right) | push(result) | Compare two values with type coercion and collation rules; return boolean or NULL (NULL_SAFE_EQ is NULL-safe). | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_MODULO (0x070E) | pop(left,right) | push(result) | Evaluate arithmetic operation with numeric coercion; propagate NULL on NULL operands. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_MULTIPLY (0x070F) | pop(left,right) | push(result) | Evaluate arithmetic operation with numeric coercion; propagate NULL on NULL operands. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_NE (0x0710) | pop(left,right) | push(result) | Compare two values with type coercion and collation rules; return boolean or NULL (NULL_SAFE_EQ is NULL-safe). | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_OR (0x0711) | pop(left,right) | push(result) | Evaluate boolean operation with three-valued logic; short-circuit where applicable. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_SUBTRACT (0x0712) | pop(left,right) | push(result) | Evaluate arithmetic operation with numeric coercion; propagate NULL on NULL operands. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_DIV_INT (0x0713) | pop(left,right) | push(result) | Evaluate arithmetic operation with numeric coercion; propagate NULL on NULL operands. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_FUNCTION_CALL (0x0714) | pop(args...) | push(result) | Invoke scalar function in expression context; bind arguments and return computed value. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_IS_NULL (0x0715) | pop(value) | push(result) | Evaluate NULL predicate; return boolean. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_EXPR_NOT (0x0716) | pop(value) | push(result) | Negate boolean using three-valued logic. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_ILIKE_ESCAPE (0x0717) | pop(left,right) | push(result) | Pattern match using LIKE semantics, optional ESCAPE, and collation rules; return boolean. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_LIKE_ESCAPE (0x0718) | pop(left,right) | push(result) | Pattern match using LIKE semantics, optional ESCAPE, and collation rules; return boolean. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_NULL_SAFE_EQ (0x0719) | pop(left,right) | push(result) | Compare two values with type coercion and collation rules; return boolean or NULL (NULL_SAFE_EQ is NULL-safe). | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_Required (0x071A) | pop(left,right) | push(result) | Resolve parameter Required to bound value at execution time; enforce type and nullability. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_PRED_CONTAINING (0x071B) | pop(left,right) | push(result) | Evaluate CONTAINING predicate (substring match) with collation rules. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_PRED_STARTING_WITH (0x071C) | pop(left,right) | push(result) | Evaluate STARTING WITH predicate (prefix match) with collation rules. | ERROR_EXPR, ERROR_TYPE_MISMATCH |
| SBLR3_NULLIF (0x071D) | pop(args...) | push(result) | Return NULL if two arguments are equal; else return first argument. | ERROR_EXPR, ERROR_TYPE_MISMATCH |

## FUNC
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_ASCII (0x0802) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_BIT_AND (0x0804) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_BIT_COUNT (0x0806) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_BIT_LENGTH (0x0808) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_BIT_MASK (0x080A) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_BIT_NOT (0x080C) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_BIT_OR (0x080E) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_BIT_SHIFT_LEFT (0x0810) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_BIT_SHIFT_RIGHT (0x0812) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_BIT_SHIFT_RIGHT_LOGICAL (0x0814) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_BIT_XOR (0x0816) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_CHR (0x0818) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_DECODE (0x081A) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_ENCODE (0x081C) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_EXTRACT (0x081E) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_ABS (0x0820) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_ACOS (0x0822) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_ACOSH (0x0824) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_AGE (0x0826) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_ARRAY_POSITION (0x0827) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_ASIN (0x0829) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_ASINH (0x082B) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_ATAN (0x082D) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_ATAN2 (0x082F) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_ATANH (0x0831) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_CBRT (0x0833) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_CEIL (0x0835) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_COL_DESCRIPTION (0x0836) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_CONCAT (0x0837) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_CONCAT_WS (0x0838) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_COS (0x083A) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_COSH (0x083C) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_COT (0x083E) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_CURRENT_CONNECTION (0x083F) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_CURRENT_ROLE (0x0840) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_CURRENT_TIME (0x0841) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_CURRENT_USER (0x0842) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_DEGREES (0x0844) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_ENDS_WITH (0x0845) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_EXP (0x0847) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_FLOOR (0x0849) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_FORMAT_TYPE (0x084A) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_GREATEST (0x084B) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_JSON_EXISTS (0x084C) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_JSON_HAS_KEY (0x084D) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_LEAST (0x084E) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_LN (0x0850) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_LOG (0x0852) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_LOG10 (0x0854) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_LOG2 (0x0856) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_LTRIM (0x0857) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_MOD (0x0859) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_OBJ_DESCRIPTION (0x085A) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_PI (0x085C) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_POWER (0x085E) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_RADIANS (0x0860) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_REPLACE (0x0861) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_ROUND (0x0863) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_RTRIM (0x0864) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_SHOBJ_DESCRIPTION (0x0865) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_SIGN (0x0867) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_SIN (0x0869) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_SINH (0x086B) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_SQRT (0x086D) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_TAN (0x086F) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_TANH (0x0871) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_TO_CHAR (0x0872) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_TO_DATE (0x0873) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_TO_TIMESTAMP (0x0874) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_TRUNC (0x0876) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_GET_BIT (0x0878) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_GET_BYTE (0x087A) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_INITCAP (0x087C) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_LPAD (0x087E) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_MD5 (0x0880) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_OVERLAY (0x0882) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_POSITION (0x0884) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_QUOTE_IDENT (0x0886) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_QUOTE_LITERAL (0x0888) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_REPEAT (0x088A) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_REVERSE (0x088C) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_RPAD (0x088E) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_SHA1 (0x0890) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_SHA256 (0x0892) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_SHA512 (0x0894) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_SPLIT_PART (0x0896) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_STRING_TO_TABLE (0x0898) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_STRPOS (0x089A) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_UNNEST_TEXT (0x089C) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_XMLAGG (0x089E) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_XMLCOMMENT (0x08A0) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_XMLCONCAT (0x08A2) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_XMLELEMENT (0x08A4) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_XMLEXISTS (0x08A6) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_XMLFOREST (0x08A8) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_XMLPARSE (0x08AA) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_XMLROOT (0x08AC) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_XMLSERIALIZE (0x08AE) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_XPATH (0x08B0) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_CHAR_LENGTH (0x08B1) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_COLLATE (0x08B2) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_CONVERT (0x08B3) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_CURRENT_DATE (0x08B4) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_DATE_ADD (0x08B5) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_DATE_DIFF (0x08B6) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_DATE_SUB (0x08B7) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_LENGTH (0x08B8) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_LOWER (0x08B9) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_NOW (0x08BA) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_OCTET_LENGTH (0x08BB) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_SUBSTRING (0x08BC) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_TRIM (0x08BD) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_FUNC_UPPER (0x08BE) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_JSONB_BUILD_ARRAY (0x08BF) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_JSONB_BUILD_OBJECT (0x08C0) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_JSONB_EXTRACT_PATH (0x08C1) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_JSONB_SET (0x08C2) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_JSON_ARRAY (0x08C3) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_JSON_ARROW (0x08C4) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_JSON_DOUBLE_ARROW (0x08C5) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_JSON_EXTRACT (0x08C6) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_JSON_HASH_ARROW (0x08C7) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_JSON_HASH_DOUBLE_ARROW (0x08C8) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_JSON_INSERT (0x08C9) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_JSON_OBJECT (0x08CA) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_JSON_REMOVE (0x08CB) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_JSON_SET (0x08CC) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_SEQUENCE_CURRVAL (0x08CD) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_SEQUENCE_NEXTVAL (0x08CE) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |
| SBLR3_SEQUENCE_SETVAL (0x08CF) | pop(args...) | push(result) | Evaluate scalar function with NULL propagation rules; perform type coercion; return computed value. | ERROR_FUNCTION, ERROR_TYPE_MISMATCH |

## AGG
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_AGG_ACCUMULATE (0x0901) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_AVG (0x0902) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_CORR (0x0903) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_COUNT (0x0904) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_COVAR_POP (0x0905) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_FINALIZE (0x0906) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_INIT (0x0907) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_MAX (0x0908) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_MIN (0x0909) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_REGR_AVGX (0x090A) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_REGR_AVGY (0x090B) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_REGR_COUNT (0x090C) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_REGR_INTERCEPT (0x090D) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_REGR_R2 (0x090E) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_REGR_SLOPE (0x090F) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_REGR_SXX (0x0910) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_REGR_SXY (0x0911) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_REGR_SYY (0x0912) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_STDDEV_POP (0x0913) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_STDDEV_SAMP (0x0914) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_SUM (0x0915) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_VAR_POP (0x0916) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_AGG_VAR_SAMP (0x0917) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_ARRAY_AGG (0x0918) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_CORR (0x091A) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_COVAR_POP (0x091C) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_STDDEV_POP (0x091E) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_STDDEV_SAMP (0x0920) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_TS_RANK (0x0922) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_VAR_POP (0x0924) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |
| SBLR3_VAR_SAMP (0x0926) | pop(args...) | push(result) | Compute aggregate over group; honor DISTINCT/ORDER/FILTER; return aggregate value. | ERROR_AGG, ERROR_TYPE_MISMATCH |

## WINDOW
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_WIN_CUME_DIST (0x0A02) | pop(args...) | push(result) | Compute window function over partition/frame; apply ORDER BY and frame boundaries; return value per row. | ERROR_WINDOW, ERROR_TYPE_MISMATCH |
| SBLR3_WIN_PERCENT_RANK (0x0A04) | pop(args...) | push(result) | Compute window function over partition/frame; apply ORDER BY and frame boundaries; return value per row. | ERROR_WINDOW, ERROR_TYPE_MISMATCH |
| SBLR3_WIN_DENSE_RANK (0x0A05) | pop(args...) | push(result) | Compute window function over partition/frame; apply ORDER BY and frame boundaries; return value per row. | ERROR_WINDOW, ERROR_TYPE_MISMATCH |
| SBLR3_WIN_FIRST_VALUE (0x0A06) | pop(args...) | push(result) | Compute window function over partition/frame; apply ORDER BY and frame boundaries; return value per row. | ERROR_WINDOW, ERROR_TYPE_MISMATCH |
| SBLR3_WIN_LAG (0x0A07) | pop(args...) | push(result) | Compute window function over partition/frame; apply ORDER BY and frame boundaries; return value per row. | ERROR_WINDOW, ERROR_TYPE_MISMATCH |
| SBLR3_WIN_LAST_VALUE (0x0A08) | pop(args...) | push(result) | Compute window function over partition/frame; apply ORDER BY and frame boundaries; return value per row. | ERROR_WINDOW, ERROR_TYPE_MISMATCH |
| SBLR3_WIN_LEAD (0x0A09) | pop(args...) | push(result) | Compute window function over partition/frame; apply ORDER BY and frame boundaries; return value per row. | ERROR_WINDOW, ERROR_TYPE_MISMATCH |
| SBLR3_WIN_NTH_VALUE (0x0A0A) | pop(args...) | push(result) | Compute window function over partition/frame; apply ORDER BY and frame boundaries; return value per row. | ERROR_WINDOW, ERROR_TYPE_MISMATCH |
| SBLR3_WIN_RANK (0x0A0B) | pop(args...) | push(result) | Compute window function over partition/frame; apply ORDER BY and frame boundaries; return value per row. | ERROR_WINDOW, ERROR_TYPE_MISMATCH |
| SBLR3_WIN_ROW_NUMBER (0x0A0C) | pop(args...) | push(result) | Compute window function over partition/frame; apply ORDER BY and frame boundaries; return value per row. | ERROR_WINDOW, ERROR_TYPE_MISMATCH |

## TYPE
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_TYPE_UNKNOWN (0x0B00) | - | push(type_descriptor) | Push type descriptor for type unknown onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_CIDR (0x0B01) | - | push(type_descriptor) | Push type descriptor for type cidr onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_COMPOSITE (0x0B02) | - | push(type_descriptor) | Push type descriptor for type composite onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_GEOMETRY (0x0B03) | - | push(type_descriptor) | Push type descriptor for type geometry onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_DATERANGE (0x0B04) | - | push(type_descriptor) | Push type descriptor for type daterange onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_DECFLOAT16 (0x0B05) | - | push(type_descriptor) | Push type descriptor for type decfloat16 onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_DECFLOAT34 (0x0B06) | - | push(type_descriptor) | Push type descriptor for type decfloat34 onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_GEOMETRYCOLLECTION (0x0B07) | - | push(type_descriptor) | Push type descriptor for type geometrycollection onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_INET (0x0B08) | - | push(type_descriptor) | Push type descriptor for type inet onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_INT128 (0x0B09) | - | push(type_descriptor) | Push type descriptor for type int128 onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_ENUM (0x0B0A) | - | push(type_descriptor) | Push type descriptor for type enum onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_INT4RANGE (0x0B0B) | - | push(type_descriptor) | Push type descriptor for type int4range onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_SET (0x0B0C) | - | push(type_descriptor) | Push type descriptor for type set onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_INT8RANGE (0x0B0D) | - | push(type_descriptor) | Push type descriptor for type int8range onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_INTERVAL (0x0B0E) | - | push(type_descriptor) | Push type descriptor for type interval onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_JSONB (0x0B0F) | - | push(type_descriptor) | Push type descriptor for type jsonb onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_JSONPATH (0x0B10) | - | push(type_descriptor) | Push type descriptor for type jsonpath onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_ROW (0x0B11) | - | push(type_descriptor) | Push type descriptor for type row onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_LINESTRING (0x0B12) | - | push(type_descriptor) | Push type descriptor for type linestring onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_MACADDR (0x0B13) | - | push(type_descriptor) | Push type descriptor for type macaddr onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_MACADDR8 (0x0B14) | - | push(type_descriptor) | Push type descriptor for type macaddr8 onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_MONEY (0x0B15) | - | push(type_descriptor) | Push type descriptor for type money onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_MULTILINESTRING (0x0B16) | - | push(type_descriptor) | Push type descriptor for type multilinestring onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_MULTIPOINT (0x0B17) | - | push(type_descriptor) | Push type descriptor for type multipoint onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_MULTIPOLYGON (0x0B18) | - | push(type_descriptor) | Push type descriptor for type multipolygon onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_BIT (0x0B19) | - | push(type_descriptor) | Push type descriptor for type bit onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_NUMRANGE (0x0B1A) | - | push(type_descriptor) | Push type descriptor for type numrange onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_YEAR (0x0B1B) | - | push(type_descriptor) | Push type descriptor for type year onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_POINT (0x0B1C) | - | push(type_descriptor) | Push type descriptor for type point onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_DATETIME (0x0B1D) | - | push(type_descriptor) | Push type descriptor for type datetime onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_POLYGON (0x0B1E) | - | push(type_descriptor) | Push type descriptor for type polygon onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_TIMESTAMP_TZ (0x0B1F) | - | push(type_descriptor) | Push type descriptor for type timestamp tz onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_TIME_TZ (0x0B20) | - | push(type_descriptor) | Push type descriptor for type time tz onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_MEDIUMINT (0x0B21) | - | push(type_descriptor) | Push type descriptor for type mediumint onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_TSQUERY (0x0B22) | - | push(type_descriptor) | Push type descriptor for type tsquery onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_NULL (0x0B23) | - | push(type_descriptor) | Push type descriptor for type null onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_TSRANGE (0x0B24) | - | push(type_descriptor) | Push type descriptor for type tsrange onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_BLOB_TEXT (0x0B25) | - | push(type_descriptor) | Push type descriptor for type blob text onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_TSTZRANGE (0x0B26) | - | push(type_descriptor) | Push type descriptor for type tstzrange onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_TSVECTOR (0x0B28) | - | push(type_descriptor) | Push type descriptor for type tsvector onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_UINT128 (0x0B29) | - | push(type_descriptor) | Push type descriptor for type uint128 onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_UINT16 (0x0B2A) | - | push(type_descriptor) | Push type descriptor for type uint16 onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_UINT32 (0x0B2B) | - | push(type_descriptor) | Push type descriptor for type uint32 onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_UINT64 (0x0B2C) | - | push(type_descriptor) | Push type descriptor for type uint64 onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_UINT8 (0x0B2D) | - | push(type_descriptor) | Push type descriptor for type uint8 onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_VARIANT (0x0B2E) | - | push(type_descriptor) | Push type descriptor for type variant onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_VECTOR (0x0B2F) | - | push(type_descriptor) | Push type descriptor for type vector onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_XML (0x0B30) | - | push(type_descriptor) | Push type descriptor for type xml onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_ARRAY (0x0B31) | - | push(type_descriptor) | Push type descriptor for type array onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_BIGINT (0x0B32) | - | push(type_descriptor) | Push type descriptor for type bigint onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_BINARY (0x0B33) | - | push(type_descriptor) | Push type descriptor for type binary onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_BLOB (0x0B34) | - | push(type_descriptor) | Push type descriptor for type blob onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_BOOLEAN (0x0B35) | - | push(type_descriptor) | Push type descriptor for type boolean onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_BYTEA (0x0B36) | - | push(type_descriptor) | Push type descriptor for type bytea onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_CHAR (0x0B37) | - | push(type_descriptor) | Push type descriptor for type char onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_DATE (0x0B38) | - | push(type_descriptor) | Push type descriptor for type date onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_DECIMAL (0x0B39) | - | push(type_descriptor) | Push type descriptor for type decimal onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_DOMAIN (0x0B3A) | - | push(type_descriptor) | Push type descriptor for type domain onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_DOUBLE (0x0B3B) | - | push(type_descriptor) | Push type descriptor for type double onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_FLOAT32 (0x0B3C) | - | push(type_descriptor) | Push type descriptor for type float32 onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_INT16 (0x0B3D) | - | push(type_descriptor) | Push type descriptor for type int16 onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_INT8 (0x0B3E) | - | push(type_descriptor) | Push type descriptor for type int8 onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_INTEGER (0x0B3F) | - | push(type_descriptor) | Push type descriptor for type integer onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_JSON (0x0B40) | - | push(type_descriptor) | Push type descriptor for type json onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_TEXT (0x0B41) | - | push(type_descriptor) | Push type descriptor for type text onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_TIME (0x0B42) | - | push(type_descriptor) | Push type descriptor for type time onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_TIMESTAMP (0x0B43) | - | push(type_descriptor) | Push type descriptor for type timestamp onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_UUID (0x0B44) | - | push(type_descriptor) | Push type descriptor for type uuid onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_VARBINARY (0x0B45) | - | push(type_descriptor) | Push type descriptor for type varbinary onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |
| SBLR3_TYPE_VARCHAR (0x0B46) | - | push(type_descriptor) | Push type descriptor for type varchar onto the type stack; used by CAST/LITERAL decoding. | ERROR_TYPE_MISMATCH |

## LITERAL
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_LITERAL_CIDR (0x0C01) | - | push(typed value) | Decode literal payload for cidr and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_INET (0x0C02) | - | push(typed value) | Decode literal payload for inet and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_INTERVAL (0x0C03) | - | push(typed value) | Decode literal payload for interval and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_JSONB (0x0C04) | - | push(typed value) | Decode literal payload for jsonb and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_MACADDR (0x0C05) | - | push(typed value) | Decode literal payload for macaddr and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_MACADDR8 (0x0C06) | - | push(typed value) | Decode literal payload for macaddr8 and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_MONEY (0x0C07) | - | push(typed value) | Decode literal payload for money and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_BINARY (0x0C08) | - | push(typed value) | Decode literal payload for binary and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_BOOLEAN (0x0C09) | - | push(typed value) | Decode literal payload for boolean and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_CHARSET (0x0C0A) | - | push(typed value) | Decode literal payload for charset and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_COLLATION (0x0C0B) | - | push(typed value) | Decode literal payload for collation and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_DATE (0x0C0C) | - | push(typed value) | Decode literal payload for date and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_DECIMAL (0x0C0D) | - | push(typed value) | Decode literal payload for decimal and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_DOUBLE (0x0C0E) | - | push(typed value) | Decode literal payload for double and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_INT32 (0x0C0F) | - | push(typed value) | Decode literal payload for int32 and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_INT64 (0x0C10) | - | push(typed value) | Decode literal payload for int64 and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_JSON (0x0C11) | - | push(typed value) | Decode literal payload for json and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_NULL (0x0C12) | - | push(typed value) | Decode literal payload for null and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_STRING (0x0C13) | - | push(typed value) | Decode literal payload for string and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_TIME (0x0C14) | - | push(typed value) | Decode literal payload for time and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_TIMESTAMP (0x0C15) | - | push(typed value) | Decode literal payload for timestamp and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_UUID (0x0C16) | - | push(typed value) | Decode literal payload for uuid and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_XML (0x0C17) | - | push(typed value) | Decode literal payload for xml and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_BIT (0x0C18) | - | push(typed value) | Decode literal payload for bit and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_YEAR (0x0C19) | - | push(typed value) | Decode literal payload for year and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_DATETIME (0x0C1A) | - | push(typed value) | Decode literal payload for datetime and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_MEDIUMINT (0x0C1B) | - | push(typed value) | Decode literal payload for mediumint and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_GEOMETRY (0x0C1C) | - | push(typed value) | Decode literal payload for geometry and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_JSONPATH (0x0C1D) | - | push(typed value) | Decode literal payload for jsonpath and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_ENUM (0x0C1E) | - | push(typed value) | Decode literal payload for enum and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_SET (0x0C1F) | - | push(typed value) | Decode literal payload for set and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_ROW (0x0C20) | - | push(typed value) | Decode literal payload for row and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_COMPOSITE (0x0C21) | - | push(typed value) | Decode literal payload for composite and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_DOMAIN (0x0C22) | - | push(typed value) | Decode literal payload for domain and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_INT8 (0x0C23) | - | push(typed value) | Decode literal payload for int8 and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_INT16 (0x0C24) | - | push(typed value) | Decode literal payload for int16 and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_UINT8 (0x0C25) | - | push(typed value) | Decode literal payload for uint8 and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_UINT16 (0x0C26) | - | push(typed value) | Decode literal payload for uint16 and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_UINT32 (0x0C27) | - | push(typed value) | Decode literal payload for uint32 and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_UINT64 (0x0C28) | - | push(typed value) | Decode literal payload for uint64 and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_UINT128 (0x0C29) | - | push(typed value) | Decode literal payload for uint128 and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_INT128 (0x0C2A) | - | push(typed value) | Decode literal payload for int128 and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_FLOAT32 (0x0C2B) | - | push(typed value) | Decode literal payload for float32 and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_TIME_TZ (0x0C2C) | - | push(typed value) | Decode literal payload for time with timezone and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_TIMESTAMP_TZ (0x0C2D) | - | push(typed value) | Decode literal payload for timestamp with timezone and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_RANGE (0x0C2E) | - | push(typed value) | Decode range literal payload and push typed range value onto the stack; enforce bound flags and base type. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_ARRAY (0x0C2F) | - | push(typed value) | Decode array literal payload, validate dimensions and element types, and push typed array value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_VARIANT (0x0C30) | - | push(typed value) | Decode variant literal payload, validate tag and value type against variant definition, and push typed value. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_TSVECTOR (0x0C31) | - | push(typed value) | Decode tsvector literal payload and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_TSQUERY (0x0C32) | - | push(typed value) | Decode tsquery literal payload and push typed value onto the stack. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |
| SBLR3_LITERAL_BLOB_LOCATOR (0x0C33) | - | push(typed value) | Decode blob locator payload and push typed locator value; payload must match blob subtype. | ERROR_INVALID_LITERAL, ERROR_TYPE_MISMATCH |

## PSQL
PSQL opcodes operate within procedure/function frames. Variable scoping is lexical per BLOCK and handler frames.
Detailed runtime semantics are defined in `/docs/specifications/parser/v3/PSQL_RUNTIME_V3.md`.
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_ASSIGN (0x0D02) | pop(value) | - | Assign value to variable; run domain validation, NOT NULL, and CHECK constraints for variable domain. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_BLOCK (0x0D04) | - | - | Enter new block frame; allocate local variables; execute contained statements; unwind on exit. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_CALL (0x0D06) | pop(args...) | push(return_values?) | Invoke procedure/function; bind parameters; push returned values or result handle. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_CURSOR_CLOSE (0x0D08) | - | - | Close cursor and release resources. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_CURSOR_DECLARE (0x0D0A) | - | push(cursor_handle) | Declare cursor for query; prepare plan and bind parameters. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_CURSOR_FETCH (0x0D0C) | pop(cursor_handle) | push(row_or_bool) | Fetch next row from cursor; push row or EOF flag. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_CURSOR_OPEN (0x0D0E) | - | - | Open cursor; execute query and produce row stream. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_DECLARE (0x0D10) | - | - | Declare variable/parameter in current scope; allocate storage and type metadata. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_ELSE (0x0D12) | - | - | Control-flow marker within IF/CASE; directs jump targets. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_ELSIF (0x0D14) | - | - | Control-flow marker within IF/CASE; directs jump targets. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_EXCEPTION_HANDLER (0x0D16) | - | - | Establish exception handling scope; define handler blocks and matching rules. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_EXCEPT_HANDLER (0x0D18) | - | - | Establish exception handling scope; define handler blocks and matching rules. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_EXIT (0x0D1A) | - | - | Exit current loop or block; unwind scopes and release cursor resources. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_FUNCTION (0x0D1C) | - | - | Define routine execution context; establish signature, variables, and body. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_IF (0x0D1E) | pop(condition?) | - | Evaluate condition; execute THEN or ELSE branch based on three-valued logic (NULL treated as false unless specified). | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_JUMP (0x0D20) | pop(condition?) | - | Perform control-flow jump to label or offset; conditional jumps consume boolean. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_JUMP_IF_FALSE (0x0D22) | pop(condition?) | - | Perform control-flow jump to label or offset; conditional jumps consume boolean. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_JUMP_IF_TRUE (0x0D24) | pop(condition?) | - | Perform control-flow jump to label or offset; conditional jumps consume boolean. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_LABEL (0x0D26) | - | - | Define label target for jumps; no runtime effect. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_LOOP (0x0D28) | pop(condition?) | - | Execute loop; evaluate condition each iteration; manage loop scope and exit/continue targets. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_PARAM_IN (0x0D2A) | - | - | Define routine parameter; bind type and mode (IN/OUT/INOUT) to frame. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_PARAM_INOUT (0x0D2C) | - | - | Define routine parameter; bind type and mode (IN/OUT/INOUT) to frame. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_PARAM_OUT (0x0D2E) | - | - | Define routine parameter; bind type and mode (IN/OUT/INOUT) to frame. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_PROCEDURE (0x0D30) | - | - | Define routine execution context; establish signature, variables, and body. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_RAISE (0x0D32) | - | - | Raise exception; transfer control to nearest handler; unwind frame as needed. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_RETURN (0x0D34) | - | - | Return from routine; validate return type; release frame and propagate return value. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_SUSPEND (0x0D35) | pop(row?) | push(yielded) | Yield row from selectable procedure; keep frame for next call. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_TRY (0x0D37) | - | - | Establish exception handling scope; define handler blocks and matching rules. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_VAR_LOAD (0x0D39) | - | push(value) | Load variable value onto the stack. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_VAR_STORE (0x0D3B) | pop(value) | - | Store value into variable slot; apply domain validation and normalization. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_WHILE (0x0D3D) | pop(condition?) | - | Execute loop; evaluate condition each iteration; manage loop scope and exit/continue targets. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_PSQL_FOR_SELECT (0x0D3E) | - | - | Execute FOR SELECT loop; open cursor and iterate rows, assigning into variables. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_PSQL_FOR_EXECUTE (0x0D3F) | - | - | Execute FOR EXECUTE loop; evaluate statement each iteration and assign outputs. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_PSQL_CASE (0x0D40) | - | - | Evaluate CASE expression in PSQL context; push result. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_PSQL_LEAVE (0x0D41) | - | - | Exit current loop or block; unwind scopes and release cursor resources. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_PSQL_CONTINUE (0x0D42) | - | - | Continue loop; jump to loop condition evaluation. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |
| SBLR3_PSQL_POST_EVENT (0x0D43) | - | - | Post event to event subsystem; notify listeners. | ERROR_PSQL_RUNTIME, ERROR_TYPE_MISMATCH, ERROR_INVALID_SCOPE |

## INDEX
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_COLUMNSTORE_INSERT (0x0E02) | pop(index_key, tuple_id) | - | Insert key into index; update index metadata and visibility maps. | ERROR_INDEX, ERROR_LOCK_CONFLICT |
| SBLR3_COLUMNSTORE_SCAN (0x0E04) | pop(scan_spec) | push(result_stream) | Perform index scan/search per scan_spec; return result stream in key order. | ERROR_INDEX, ERROR_LOCK_CONFLICT |
| SBLR3_GIN_INSERT (0x0E06) | pop(index_key, tuple_id) | - | Insert key into index; update index metadata and visibility maps. | ERROR_INDEX, ERROR_LOCK_CONFLICT |
| SBLR3_GIN_SEARCH (0x0E08) | pop(scan_spec) | push(result_stream) | Perform index scan/search per scan_spec; return result stream in key order. | ERROR_INDEX, ERROR_LOCK_CONFLICT |
| SBLR3_HNSW_INSERT (0x0E0A) | pop(index_key, tuple_id) | - | Insert key into index; update index metadata and visibility maps. | ERROR_INDEX, ERROR_LOCK_CONFLICT |
| SBLR3_HNSW_SEARCH (0x0E0C) | pop(scan_spec) | push(result_stream) | Perform index scan/search per scan_spec; return result stream in key order. | ERROR_INDEX, ERROR_LOCK_CONFLICT |
| SBLR3_INDEX_DELETE (0x0E0E) | pop(index_key, tuple_id) | - | Delete key from index; update index metadata and visibility maps. | ERROR_INDEX, ERROR_LOCK_CONFLICT |
| SBLR3_INDEX_INSERT (0x0E10) | pop(index_key, tuple_id) | - | Insert key into index; update index metadata and visibility maps. | ERROR_INDEX, ERROR_LOCK_CONFLICT |
| SBLR3_INDEX_REINDEX (0x0E12) | - | - | Rebuild index structure; lock table and index; swap in new index on success. | ERROR_INDEX, ERROR_LOCK_CONFLICT |
| SBLR3_INDEX_SCAN (0x0E14) | pop(scan_spec) | push(result_stream) | Perform index scan/search per scan_spec; return result stream in key order. | ERROR_INDEX, ERROR_LOCK_CONFLICT |
| SBLR3_INDEX_SCAN_END (0x0E16) | - | - | Index maintenance or scan operation. | ERROR_INDEX, ERROR_LOCK_CONFLICT |
| SBLR3_INDEX_SCAN_NEXT (0x0E18) | - | - | Index maintenance or scan operation. | ERROR_INDEX, ERROR_LOCK_CONFLICT |
| SBLR3_INDEX_SCAN_START (0x0E1A) | - | - | Index maintenance or scan operation. | ERROR_INDEX, ERROR_LOCK_CONFLICT |
| SBLR3_INDEX_SEARCH (0x0E1C) | pop(scan_spec) | push(result_stream) | Perform index scan/search per scan_spec; return result stream in key order. | ERROR_INDEX, ERROR_LOCK_CONFLICT |
| SBLR3_INDEX_STATS (0x0E1E) | - | - | Collect and return index statistics. | ERROR_INDEX, ERROR_LOCK_CONFLICT |
| SBLR3_INDEX_TYPE (0x0E20) | - | - | Index maintenance or scan operation. | ERROR_INDEX, ERROR_LOCK_CONFLICT |
| SBLR3_INDEX_UPDATE (0x0E22) | pop(old_key, new_key, tuple_id) | - | Update index entry for tuple (delete old key, insert new key). | ERROR_INDEX, ERROR_LOCK_CONFLICT |
| SBLR3_INDEX_VACUUM (0x0E24) | - | - | Vacuum index; remove dead entries; update stats. | ERROR_INDEX, ERROR_LOCK_CONFLICT |

## ARRAY
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_ARRAY_TO_STRING (0x0F01) | pop(args...) | push(result) | Perform array operation: to string; return resulting array/value; enforce bounds and element type constraints. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_ARRAY_APPEND (0x0F03) | pop(args...) | push(result) | Perform array operation: append; return resulting array/value; enforce bounds and element type constraints. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_ARRAY_CAT (0x0F05) | pop(args...) | push(result) | Perform array operation: cat; return resulting array/value; enforce bounds and element type constraints. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_ARRAY_CONSTRUCT (0x0F07) | pop(args...) | push(result) | Perform array operation: construct; return resulting array/value; enforce bounds and element type constraints. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_ARRAY_CONTAINED_BY (0x0F09) | pop(args...) | push(result) | Perform array operation: contained by; return resulting array/value; enforce bounds and element type constraints. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_ARRAY_CONTAINS (0x0F0B) | pop(args...) | push(result) | Perform array operation: contains; return resulting array/value; enforce bounds and element type constraints. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_ARRAY_DIMS (0x0F0D) | pop(args...) | push(result) | Perform array operation: dims; return resulting array/value; enforce bounds and element type constraints. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_ARRAY_EQ (0x0F0F) | pop(args...) | push(result) | Perform array operation: eq; return resulting array/value; enforce bounds and element type constraints. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_ARRAY_LENGTH (0x0F11) | pop(args...) | push(result) | Perform array operation: length; return resulting array/value; enforce bounds and element type constraints. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_ARRAY_LOWER (0x0F13) | pop(args...) | push(result) | Perform array operation: lower; return resulting array/value; enforce bounds and element type constraints. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_ARRAY_NE (0x0F15) | pop(args...) | push(result) | Perform array operation: ne; return resulting array/value; enforce bounds and element type constraints. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_ARRAY_OVERLAP (0x0F17) | pop(args...) | push(result) | Perform array operation: overlap; return resulting array/value; enforce bounds and element type constraints. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_ARRAY_PREPEND (0x0F19) | pop(args...) | push(result) | Perform array operation: prepend; return resulting array/value; enforce bounds and element type constraints. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_ARRAY_REMOVE (0x0F1B) | pop(args...) | push(result) | Perform array operation: remove; return resulting array/value; enforce bounds and element type constraints. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_ARRAY_REPLACE (0x0F1D) | pop(args...) | push(result) | Perform array operation: replace; return resulting array/value; enforce bounds and element type constraints. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_ARRAY_SLICE (0x0F1E) | pop(args...) | push(result) | Perform array operation: slice; return resulting array/value; enforce bounds and element type constraints. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_ARRAY_SUBSCRIPT (0x0F20) | pop(args...) | push(result) | Perform array operation: subscript; return resulting array/value; enforce bounds and element type constraints. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_ARRAY_UPPER (0x0F22) | pop(args...) | push(result) | Perform array operation: upper; return resulting array/value; enforce bounds and element type constraints. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_ADJACENT (0x0F24) | pop(args...) | push(result) | Perform range operation: adjacent; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_CONSTRUCT (0x0F26) | pop(args...) | push(result) | Perform range operation: construct; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_CONTAINED_BY (0x0F28) | pop(args...) | push(result) | Perform range operation: contained by; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_CONTAINS_ELEM (0x0F2A) | pop(args...) | push(result) | Perform range operation: contains elem; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_CONTAINS_RANGE (0x0F2C) | pop(args...) | push(result) | Perform range operation: contains range; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_DIFFERENCE (0x0F2E) | pop(args...) | push(result) | Perform range operation: difference; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_INTERSECTION (0x0F30) | pop(args...) | push(result) | Perform range operation: intersection; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_ISEMPTY (0x0F32) | pop(args...) | push(result) | Perform range operation: isempty; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_LOWER (0x0F34) | pop(args...) | push(result) | Perform range operation: lower; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_LOWER_INC (0x0F36) | pop(args...) | push(result) | Perform range operation: lower inc; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_LOWER_INF (0x0F38) | pop(args...) | push(result) | Perform range operation: lower inf; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_MERGE (0x0F3A) | pop(args...) | push(result) | Perform range operation: merge; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_OVERLAPS (0x0F3C) | pop(args...) | push(result) | Perform range operation: overlaps; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_STRICTLY_LEFT (0x0F3E) | pop(args...) | push(result) | Perform range operation: strictly left; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_STRICTLY_RIGHT (0x0F40) | pop(args...) | push(result) | Perform range operation: strictly right; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_UNION (0x0F42) | pop(args...) | push(result) | Perform range operation: union; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_UPPER (0x0F44) | pop(args...) | push(result) | Perform range operation: upper; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_UPPER_INC (0x0F46) | pop(args...) | push(result) | Perform range operation: upper inc; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_RANGE_UPPER_INF (0x0F48) | pop(args...) | push(result) | Perform range operation: upper inf; validate range bounds and element type. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_STRING_TO_ARRAY (0x0F49) | pop(args...) | push(result) | Split string into array using delimiter and optional null string; return array of text. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |
| SBLR3_UNNEST (0x0F4A) | pop(args...) | push(result) | Expand array to set of rows; preserve order if specified. | ERROR_ARRAY, ERROR_TYPE_MISMATCH |

## TEXTSEARCH
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_PHRASETO_TSQUERY (0x1002) | pop(args...) | push(result) | Perform text search operation phraseto tsquery with locale and collation rules. | ERROR_TEXTSEARCH, ERROR_TYPE_MISMATCH |
| SBLR3_PLAINTO_TSQUERY (0x1004) | pop(args...) | push(result) | Perform text search operation plainto tsquery with locale and collation rules. | ERROR_TEXTSEARCH, ERROR_TYPE_MISMATCH |
| SBLR3_REGEXP_MATCHES (0x1006) | pop(args...) | push(result) | Perform text search operation regexp matches with locale and collation rules. | ERROR_TEXTSEARCH, ERROR_TYPE_MISMATCH |
| SBLR3_REGEXP_REPLACE (0x1008) | pop(args...) | push(result) | Perform text search operation regexp replace with locale and collation rules. | ERROR_TEXTSEARCH, ERROR_TYPE_MISMATCH |
| SBLR3_REGEXP_SPLIT_TO_ARRAY (0x100A) | pop(args...) | push(result) | Perform text search operation regexp split to array with locale and collation rules. | ERROR_TEXTSEARCH, ERROR_TYPE_MISMATCH |
| SBLR3_REGEXP_SPLIT_TO_TABLE (0x100C) | pop(args...) | push(result) | Perform text search operation regexp split to table with locale and collation rules. | ERROR_TEXTSEARCH, ERROR_TYPE_MISMATCH |
| SBLR3_REGEX_MATCH (0x100E) | pop(args...) | push(result) | Perform text search operation regex match with locale and collation rules. | ERROR_TEXTSEARCH, ERROR_TYPE_MISMATCH |
| SBLR3_REGEX_MATCH_CI (0x1010) | pop(args...) | push(result) | Perform text search operation regex match ci with locale and collation rules. | ERROR_TEXTSEARCH, ERROR_TYPE_MISMATCH |
| SBLR3_REGEX_NOT_MATCH (0x1012) | pop(args...) | push(result) | Perform text search operation regex not match with locale and collation rules. | ERROR_TEXTSEARCH, ERROR_TYPE_MISMATCH |
| SBLR3_REGEX_NOT_MATCH_CI (0x1014) | pop(args...) | push(result) | Perform text search operation regex not match ci with locale and collation rules. | ERROR_TEXTSEARCH, ERROR_TYPE_MISMATCH |
| SBLR3_TO_TSQUERY (0x1016) | pop(args...) | push(result) | Perform text search operation to tsquery with locale and collation rules. | ERROR_TEXTSEARCH, ERROR_TYPE_MISMATCH |
| SBLR3_TO_TSVECTOR (0x1018) | pop(args...) | push(result) | Perform text search operation to tsvector with locale and collation rules. | ERROR_TEXTSEARCH, ERROR_TYPE_MISMATCH |
| SBLR3_TSMATCH (0x101A) | pop(args...) | push(result) | Perform text search operation tsmatch with locale and collation rules. | ERROR_TEXTSEARCH, ERROR_TYPE_MISMATCH |

## SPATIAL
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_ST_AREA (0x1102) | pop(args...) | push(result) | Perform spatial operation st area using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_ASBINARY (0x1104) | pop(args...) | push(result) | Perform spatial operation st asbinary using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_ASTEXT (0x1106) | pop(args...) | push(result) | Perform spatial operation st astext using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_BUFFER (0x1108) | pop(args...) | push(result) | Perform spatial operation st buffer using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_COLLECT (0x110A) | pop(args...) | push(result) | Perform spatial operation st collect using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_CONTAINS (0x110C) | pop(args...) | push(result) | Perform spatial operation st contains using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_CONVEXHULL (0x110E) | pop(args...) | push(result) | Perform spatial operation st convexhull using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_CROSSES (0x1110) | pop(args...) | push(result) | Perform spatial operation st crosses using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_DIFFERENCE (0x1112) | pop(args...) | push(result) | Perform spatial operation st difference using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_DISJOINT (0x1114) | pop(args...) | push(result) | Perform spatial operation st disjoint using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_DISTANCE (0x1116) | pop(args...) | push(result) | Perform spatial operation st distance using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_DISTANCE_SPHERE (0x1118) | pop(args...) | push(result) | Perform spatial operation st distance sphere using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_DUMP (0x111A) | pop(args...) | push(result) | Perform spatial operation st dump using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_ENVELOPE (0x111C) | pop(args...) | push(result) | Perform spatial operation st envelope using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_EQUALS (0x111E) | pop(args...) | push(result) | Perform spatial operation st equals using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_GEOMETRYCOLLECTION (0x1120) | pop(args...) | push(result) | Perform spatial operation st geometrycollection using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_GEOMETRYN (0x1122) | pop(args...) | push(result) | Perform spatial operation st geometryn using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_GEOMETRYTYPE (0x1124) | pop(args...) | push(result) | Perform spatial operation st geometrytype using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_INTERSECTION (0x1126) | pop(args...) | push(result) | Perform spatial operation st intersection using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_INTERSECTS (0x1128) | pop(args...) | push(result) | Perform spatial operation st intersects using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_ISVALID (0x112A) | pop(args...) | push(result) | Perform spatial operation st isvalid using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_LENGTH (0x112C) | pop(args...) | push(result) | Perform spatial operation st length using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_MAKELINE (0x112E) | pop(args...) | push(result) | Perform spatial operation st makeline using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_MAKEPOLYGON (0x1130) | pop(args...) | push(result) | Perform spatial operation st makepolygon using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_MULTILINESTRING (0x1132) | pop(args...) | push(result) | Perform spatial operation st multilinestring using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_MULTIPOINT (0x1134) | pop(args...) | push(result) | Perform spatial operation st multipoint using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_MULTIPOLYGON (0x1136) | pop(args...) | push(result) | Perform spatial operation st multipolygon using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_NUMGEOMETRIES (0x1138) | pop(args...) | push(result) | Perform spatial operation st numgeometries using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_OVERLAPS (0x113A) | pop(args...) | push(result) | Perform spatial operation st overlaps using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_PERIMETER (0x113C) | pop(args...) | push(result) | Perform spatial operation st perimeter using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_POINT (0x113E) | pop(args...) | push(result) | Perform spatial operation st point using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_SETSRID (0x1140) | pop(args...) | push(result) | Perform spatial operation st setsrid using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_SRID (0x1142) | pop(args...) | push(result) | Perform spatial operation st srid using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_TOUCHES (0x1144) | pop(args...) | push(result) | Perform spatial operation st touches using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_TRANSFORM (0x1146) | pop(args...) | push(result) | Perform spatial operation st transform using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_UNION (0x1148) | pop(args...) | push(result) | Perform spatial operation st union using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |
| SBLR3_ST_WITHIN (0x114A) | pop(args...) | push(result) | Perform spatial operation st within using SRID rules; validate geometry inputs. | ERROR_SPATIAL, ERROR_TYPE_MISMATCH |

## JOB
| Opcode | Stack In | Stack Out | Effect | Errors |
| --- | --- | --- | --- | --- |
| SBLR3_CANCEL_JOB_RUN (0x1201) | pop(args...) | push(result?) | Cancel a running job execution; mark run as cancelled and release resources. | ERROR_JOB, ERROR_PERMISSION |
| SBLR3_EXECUTE_JOB (0x1202) | pop(args...) | push(result?) | Execute job immediately; enqueue or run per scheduler policy; record run metadata. | ERROR_JOB, ERROR_PERMISSION |

## Semantics Coverage Checklist
- Every opcode in `SBLR_V3_OPCODE_SPEC.md` appears exactly once in this document.
- Each opcode row specifies `stack_in`, `stack_out`, `effect`, and `errors`.
- DDL rows describe constraint validation order and dependency checks when relevant.
- DML rows describe constraint validation order, index maintenance, and locking behavior.
- TXN rows describe snapshot/lock behavior and 2PC lifecycle where applicable.
- PSQL rows describe scope/flow behavior and variable/domain validation.
- QUERY/EXPR rows describe operator intent (joins, set ops, predicates, coercions).
- DCL/SESSION rows describe security/session state transitions and audit effects.
- INDEX/ARRAY/TEXTSEARCH/SPATIAL/JOB rows describe execution intent and required validation.
