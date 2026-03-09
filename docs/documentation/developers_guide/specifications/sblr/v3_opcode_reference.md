# Specification: SBLR v3 Opcode Reference

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | sblr |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird SBLR v3 |
| **Authors** | Dalton Calford |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_opcodes.generated.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_opcodes.generated.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_opcode_registry.h:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_sblr_v3_opcode_identity.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_sblr_v3_payload_codec.cpp:1`

## Synopsis

This specification defines the complete opcode catalog for the ScratchBird Language Runtime (SBLR) version 3. SBLR opcodes are 16-bit unsigned integers organized into functional families. Each opcode represents a distinct operation that the executor can perform, from DDL statements to expression evaluation and control flow.

## Scope

### In Scope

- Complete opcode catalog with hexadecimal values and symbolic names
- Opcode family organization and naming conventions
- Opcode-to-schema mapping for payload encoding
- Runtime opcode lookup and validation

### Out of Scope

- Parser AST-to-opcode emission rules (see parser specifications)
- Specific payload field definitions (see v3_payload_schemas.md)
- JIT compilation targets for opcodes (see JIT specifications)

## Background

SBLR v3 uses a unified opcode space where all operations—from SQL DDL to expression operators—share a common 16-bit encoding. The opcode space is partitioned into families:

- **0x0000-0x00FF**: Control and versioning
- **0x0100-0x01FF**: DDL statements (CREATE, ALTER, DROP)
- **0x0200-0x02FF**: DML statements (INSERT, UPDATE, DELETE, MERGE)
- **0x0300-0x03FF**: Transaction control
- **0x0400-0x04FF**: Security and domain operations
- **0x0500-0x05FF**: Session and utility statements
- **0x0600-0x06FF**: Query composition (SELECT, JOIN, SET operations)
- **0x0700-0x07FF**: Expression operators
- **0x0800-0x08FF**: Built-in functions
- **0x0900-0x09FF**: Aggregate functions
- **0x0A00-0x0AFF**: Window functions
- **0x0B00-0x0BFF**: Type descriptors
- **0x0C00-0x0CFF**: Literal value opcodes
- **0x0D00-0x0DFF**: PSQL control flow
- **0x0E00-0x0EFF**: Index operations
- **0x0F00-0x0FFF**: Array and range operations
- **0x1000-0x10FF**: Text search and regex
- **0x1100-0x11FF**: Spatial functions
- **0x1200-0x12FF**: Job control
- **0x6000-0x61FF**: Extended/multi-model opcodes

## Specification

### Control Opcodes (0x0000-0x00FF)

| Opcode | Hex | Name | Description |
|--------|-----|------|-------------|
| 0x0001 | 0x0001 | SBLR3_END | Marks end of bytecode stream |
| 0x0002 | 0x0002 | SBLR3_VERSION | Container version marker |
| 0x0003 | 0x0003 | SBLR3_EXTENDED_OPCODE | Prefix for 32-bit extended opcodes |

Source: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_opcodes.generated.h:2`

### DDL Opcodes (0x0100-0x01FF)

| Opcode | Hex | Name | Description |
|--------|-----|------|-------------|
| 0x0101 | 0x0101 | SBLR3_ALTER_JOB | Alter job definition |
| 0x0102 | 0x0102 | SBLR3_ALTER_SEQUENCE | Alter sequence |
| 0x0103 | 0x0103 | SBLR3_ALTER_TABLE | Alter table structure |
| 0x0104 | 0x0104 | SBLR3_ALTER_TABLESPACE | Alter tablespace |
| 0x0105 | 0x0105 | SBLR3_ALTER_TABLE_SET_TABLESPACE | Move table to tablespace |
| 0x0106 | 0x0106 | SBLR3_ATTACH_TABLESPACE | Attach tablespace |
| 0x0107 | 0x0107 | SBLR3_CHECK_CONSTRAINT | CHECK constraint definition |
| 0x0108 | 0x0108 | SBLR3_COLUMN_CHARSET | Column charset spec |
| 0x0109 | 0x0109 | SBLR3_COLUMN_COLLATE | Column collation spec |
| 0x010A | 0x010A | SBLR3_CREATE_INDEX | Create index |
| 0x010B | 0x010B | SBLR3_CREATE_JOB | Create job |
| 0x010C | 0x010C | SBLR3_CREATE_SEQUENCE | Create sequence |
| 0x010D | 0x010D | SBLR3_CREATE_TABLE | Create table |
| 0x010E | 0x010E | SBLR3_CREATE_TABLESPACE | Create tablespace |
| 0x010F | 0x010F | SBLR3_CREATE_VIEW | Create view |
| 0x0110 | 0x0110 | SBLR3_DEFAULT_VALUE | Default value constraint |
| 0x0111 | 0x0111 | SBLR3_DETACH_TABLESPACE | Detach tablespace |
| 0x0112 | 0x0112 | SBLR3_DROP_INDEX | Drop index |
| 0x0113 | 0x0113 | SBLR3_DROP_JOB | Drop job |
| 0x0114 | 0x0114 | SBLR3_DROP_SEQUENCE | Drop sequence |
| 0x0115 | 0x0115 | SBLR3_DROP_TABLE | Drop table |
| 0x0116 | 0x0116 | SBLR3_DROP_TABLESPACE | Drop tablespace |
| 0x0117 | 0x0117 | SBLR3_DROP_VIEW | Drop view |
| 0x0118 | 0x0118 | SBLR3_ALTER_COLUMN_DEFAULT | Alter column default |
| 0x0119 | 0x0119 | SBLR3_ALTER_DATABASE | Alter database |
| 0x011A | 0x011A | SBLR3_ALTER_DEFAULT_PRIVILEGES | Alter default privileges |
| 0x011B | 0x011B | SBLR3_ALTER_DOMAIN | Alter domain |
| 0x011C | 0x011C | SBLR3_ALTER_ELEMENT | Alter element (EXTRACT) |
| 0x011D | 0x011D | SBLR3_ALTER_FUNCTION_STMT | Alter function |
| 0x011E | 0x011E | SBLR3_ALTER_INDEX | Alter index |
| 0x0120 | 0x0120 | SBLR3_ALTER_POLICY | Alter policy |
| 0x0121 | 0x0121 | SBLR3_ALTER_PROCEDURE_STMT | Alter procedure |
| 0x0122 | 0x0122 | SBLR3_ALTER_ROLE | Alter role |
| 0x0123 | 0x0123 | SBLR3_ALTER_SCHEMA | Alter schema |
| 0x0124 | 0x0124 | SBLR3_ALTER_SYSTEM | Alter system |
| 0x0126 | 0x0126 | SBLR3_ALTER_TABLE_RLS | Alter table RLS |
| 0x0128 | 0x0128 | SBLR3_ALTER_USER | Alter user |
| 0x0129 | 0x0129 | SBLR3_CREATE_DATABASE | Create database |
| 0x012A | 0x012A | SBLR3_CREATE_FOREIGN_DATA_WRAPPER | Create FDW |
| 0x012B | 0x012B | SBLR3_CREATE_DB_TRIGGER | Create database trigger |
| 0x012D | 0x012D | SBLR3_CREATE_DOMAIN | Create domain |
| 0x012E | 0x012E | SBLR3_CREATE_EXCEPTION_STMT | Create exception |
| 0x012F | 0x012F | SBLR3_CREATE_FOREIGN_SERVER | Create foreign server |
| 0x0130 | 0x0130 | SBLR3_CREATE_FOREIGN_TABLE | Create foreign table |
| 0x0132 | 0x0132 | SBLR3_CREATE_FUNCTION_STMT | Create function |
| 0x0134 | 0x0134 | SBLR3_CREATE_GROUP | Create group |
| 0x0136 | 0x0136 | SBLR3_CREATE_PACKAGE_STMT | Create package |
| 0x0138 | 0x0138 | SBLR3_CREATE_POLICY | Create policy |
| 0x013A | 0x013A | SBLR3_CREATE_PROCEDURE_STMT | Create procedure |
| 0x013C | 0x013C | SBLR3_CREATE_ROLE | Create role |
| 0x013D | 0x013D | SBLR3_CREATE_SCHEMA | Create schema |
| 0x013E | 0x013E | SBLR3_CREATE_SYNONYM | Create synonym |
| 0x013F | 0x013F | SBLR3_CREATE_TABLE_AS | CREATE TABLE AS |
| 0x0141 | 0x0141 | SBLR3_CREATE_TRIGGER | Create trigger |
| 0x0143 | 0x0143 | SBLR3_CREATE_TYPE | Create type |
| 0x0144 | 0x0144 | SBLR3_CREATE_UDR | Create UDR |
| 0x0146 | 0x0146 | SBLR3_CREATE_USER | Create user |
| 0x0147 | 0x0147 | SBLR3_CREATE_USER_MAPPING | Create user mapping |
| 0x0148 | 0x0148 | SBLR3_DROP_DATABASE | Drop database |
| 0x014A | 0x014A | SBLR3_DROP_DB_TRIGGER | Drop database trigger |
| 0x014B | 0x014B | SBLR3_DROP_DOMAIN | Drop domain |
| 0x014C | 0x014C | SBLR3_DROP_EXCEPTION_STMT | Drop exception |
| 0x014D | 0x014D | SBLR3_DROP_FOREIGN_SERVER | Drop foreign server |
| 0x014E | 0x014E | SBLR3_DROP_FOREIGN_TABLE | Drop foreign table |
| 0x0150 | 0x0150 | SBLR3_DROP_FUNCTION_STMT | Drop function |
| 0x0152 | 0x0152 | SBLR3_DROP_GROUP | Drop group |
| 0x0154 | 0x0154 | SBLR3_DROP_PACKAGE_STMT | Drop package |
| 0x0156 | 0x0156 | SBLR3_DROP_POLICY | Drop policy |
| 0x0158 | 0x0158 | SBLR3_DROP_PROCEDURE_STMT | Drop procedure |
| 0x015A | 0x015A | SBLR3_DROP_ROLE | Drop role |
| 0x015B | 0x015B | SBLR3_DROP_SCHEMA | Drop schema |
| 0x015C | 0x015C | SBLR3_DROP_SYNONYM | Drop synonym |
| 0x015E | 0x015E | SBLR3_DROP_TRIGGER | Drop trigger |
| 0x015F | 0x015F | SBLR3_DROP_UDR | Drop UDR |
| 0x0161 | 0x0161 | SBLR3_DROP_USER | Drop user |
| 0x0162 | 0x0162 | SBLR3_DROP_USER_MAPPING | Drop user mapping |
| 0x0163 | 0x0163 | SBLR3_FB_ALTER_MAPPING | Firebird alter mapping |
| 0x0164 | 0x0164 | SBLR3_FB_CREATE_MAPPING | Firebird create mapping |
| 0x0165 | 0x0165 | SBLR3_FB_CREATE_SHADOW | Firebird create shadow |
| 0x0166 | 0x0166 | SBLR3_FB_DROP_MAPPING | Firebird drop mapping |
| 0x0167 | 0x0167 | SBLR3_FB_DROP_SHADOW | Firebird drop shadow |
| 0x0169 | 0x0169 | SBLR3_FIRE_DB_TRIGGER | Fire database trigger |
| 0x016B | 0x016B | SBLR3_FIRE_TRIGGER | Fire trigger |
| 0x016C | 0x016C | SBLR3_MOVE_OBJECT | Move object |
| 0x016D | 0x016D | SBLR3_REBIND_DOMAIN | Rebind domain |
| 0x016E | 0x016E | SBLR3_RENAME_COLUMN | Rename column |
| 0x016F | 0x016F | SBLR3_RENAME_OBJECT | Rename object |
| 0x0170 | 0x0170 | SBLR3_RESOLVE_DOMAIN_CONFLICT | Resolve domain conflict |
| 0x0171 | 0x0171 | SBLR3_TABLE_INHERITS | Table inheritance |
| 0x0172 | 0x0172 | SBLR3_TABLE_OPTIONS | Table options |
| 0x0173 | 0x0173 | SBLR3_TABLE_PARTITIONING | Table partitioning |
| 0x0174 | 0x0174 | SBLR3_FOREIGN_KEY | Foreign key constraint |
| 0x0175 | 0x0175 | SBLR3_GENERATED_COLUMN | Generated column |
| 0x0176 | 0x0176 | SBLR3_IDENTITY_COLUMN | Identity column |
| 0x0177 | 0x0177 | SBLR3_NOT_NULL | NOT NULL constraint |
| 0x0178 | 0x0178 | SBLR3_PRIMARY_KEY | Primary key constraint |
| 0x0179 | 0x0179 | SBLR3_REFRESH_MATERIALIZED_VIEW | Refresh materialized view |
| 0x017A | 0x017A | SBLR3_TABLE_FK | Table foreign key |
| 0x017B | 0x017B | SBLR3_TRUNCATE_TABLE | Truncate table |
| 0x017C | 0x017C | SBLR3_UNIQUE_CONSTRAINT | Unique constraint |

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_opcodes.generated.cpp:6-110`

### DML Opcodes (0x0200-0x02FF)

| Opcode | Hex | Name | Description |
|--------|-----|------|-------------|
| 0x0201 | 0x0201 | SBLR3_DELETE | DELETE statement |
| 0x0202 | 0x0202 | SBLR3_COPY | COPY statement |
| 0x0204 | 0x0204 | SBLR3_MERGE_END | MERGE statement end |
| 0x0206 | 0x0206 | SBLR3_MERGE_ON | MERGE ON clause |
| 0x0208 | 0x0208 | SBLR3_MERGE_SOURCE | MERGE source table |
| 0x020A | 0x020A | SBLR3_MERGE_START | MERGE statement start |
| 0x020C | 0x020C | SBLR3_MERGE_WHEN_MATCHED | MERGE WHEN MATCHED |
| 0x020E | 0x020E | SBLR3_MERGE_WHEN_NOT_MATCHED | MERGE WHEN NOT MATCHED |
| 0x0210 | 0x0210 | SBLR3_MERGE_WHEN_NOT_MATCHED_SOURCE | MERGE WHEN NOT MATCHED BY SOURCE |
| 0x0211 | 0x0211 | SBLR3_INSERT | INSERT statement |
| 0x0212 | 0x0212 | SBLR3_SELECT | SELECT statement |
| 0x0213 | 0x0213 | SBLR3_UPDATE | UPDATE statement |

### Transaction Opcodes (0x0300-0x03FF)

| Opcode | Hex | Name | Description |
|--------|-----|------|-------------|
| 0x0301 | 0x0301 | SBLR3_COMMIT | COMMIT |
| 0x0302 | 0x0302 | SBLR3_COMMIT_PREPARED | COMMIT PREPARED |
| 0x0303 | 0x0303 | SBLR3_COMMIT_RETAINING | COMMIT RETAINING |
| 0x0304 | 0x0304 | SBLR3_DEALLOCATE_PREPARED | Deallocate prepared |
| 0x0305 | 0x0305 | SBLR3_EXECUTE_PREPARED | Execute prepared |
| 0x0306 | 0x0306 | SBLR3_FUNC_CURRENT_TRANSACTION | Current transaction function |
| 0x0307 | 0x0307 | SBLR3_PREPARE_TRANSACTION | Prepare transaction |
| 0x0309 | 0x0309 | SBLR3_RELEASE_SAVEPOINT | Release savepoint |
| 0x030A | 0x030A | SBLR3_ROLLBACK_PREPARED | ROLLBACK PREPARED |
| 0x030B | 0x030B | SBLR3_ROLLBACK_RETAINING | ROLLBACK RETAINING |
| 0x030D | 0x030D | SBLR3_ROLLBACK_TO_SAVEPOINT | ROLLBACK TO SAVEPOINT |
| 0x030F | 0x030F | SBLR3_SAVEPOINT | Savepoint |
| 0x0310 | 0x0310 | SBLR3_SAVEPOINT_BEGIN | Begin savepoint block |
| 0x0311 | 0x0311 | SBLR3_SAVEPOINT_END | End savepoint block |
| 0x0312 | 0x0312 | SBLR3_SET_AUTOCOMMIT | SET AUTOCOMMIT |
| 0x0314 | 0x0314 | SBLR3_SHOW_TRANSACTION_LEVEL | Show transaction level |
| 0x0315 | 0x0315 | SBLR3_ROLLBACK | ROLLBACK |
| 0x0316 | 0x0316 | SBLR3_SET_TRANSACTION | SET TRANSACTION |
| 0x0317 | 0x0317 | SBLR3_START_TRANSACTION | START TRANSACTION |
| 0x0318 | 0x0318 | SBLR3_SWEEP | Database sweep |

### Security Opcodes (0x0400-0x04FF)

| Opcode | Hex | Name | Description |
|--------|-----|------|-------------|
| 0x0401 | 0x0401 | SBLR3_APPLY_DOMAIN_MASKING | Apply domain masking |
| 0x0402 | 0x0402 | SBLR3_APPLY_QUALITY_PIPELINE | Apply quality pipeline |
| 0x0403 | 0x0403 | SBLR3_AUDIT_DOMAIN_ACCESS | Audit domain access |
| 0x0404 | 0x0404 | SBLR3_CHECK_DOMAIN_CONSTRAINT | Check domain constraint |
| 0x0405 | 0x0405 | SBLR3_CHECK_DOMAIN_PRIVILEGE | Check domain privilege |
| 0x0406 | 0x0406 | SBLR3_CHECK_GLOBAL_UNIQUENESS | Check global uniqueness |
| 0x0407 | 0x0407 | SBLR3_DECRYPT_DOMAIN_VALUE | Decrypt domain value |
| 0x0408 | 0x0408 | SBLR3_ENCRYPT_DOMAIN_VALUE | Encrypt domain value |
| 0x040A | 0x040A | SBLR3_GRANT | GRANT |
| 0x040C | 0x040C | SBLR3_GRANT_OPTION | GRANT OPTION |
| 0x040E | 0x040E | SBLR3_GRANT_PRIVILEGE | Grant privilege |
| 0x0410 | 0x0410 | SBLR3_GRANT_ROLE | Grant role |
| 0x0411 | 0x0411 | SBLR3_NORMALIZE_DOMAIN_VALUE | Normalize domain value |
| 0x0413 | 0x0413 | SBLR3_PRIVILEGE | Privilege check |
| 0x0415 | 0x0415 | SBLR3_REVOKE | REVOKE |
| 0x0417 | 0x0417 | SBLR3_REVOKE_PRIVILEGE | Revoke privilege |
| 0x0419 | 0x0419 | SBLR3_REVOKE_ROLE | Revoke role |
| 0x041B | 0x041B | SBLR3_SET_CONSTRAINTS | SET CONSTRAINTS |
| 0x041D | 0x041D | SBLR3_SET_ROLE | SET ROLE |
| 0x041F | 0x041F | SBLR3_SET_SESSION_AUTH | SET SESSION AUTHORIZATION |
| 0x0420 | 0x0420 | SBLR3_VALIDATE_DOMAIN_VALUE | Validate domain value |

### Session/Utility Opcodes (0x0500-0x05FF)

| Opcode | Hex | Name | Description |
|--------|-----|------|-------------|
| 0x0501 | 0x0501 | SBLR3_EXPLAIN_PLAN | EXPLAIN |
| 0x0503 | 0x0503 | SBLR3_ANALYZE | ANALYZE |
| 0x0505 | 0x0505 | SBLR3_COMMENT | COMMENT |
| 0x0507 | 0x0507 | SBLR3_CONNECT | CONNECT |
| 0x0508 | 0x0508 | SBLR3_DEBUG_SPAN | Debug span |
| 0x050A | 0x050A | SBLR3_DESCRIBE_TABLE | DESCRIBE table |
| 0x050C | 0x050C | SBLR3_DISCONNECT | DISCONNECT |
| 0x050D | 0x050D | SBLR3_EXECUTE_STMT | Execute statement |
| 0x050E | 0x050E | SBLR3_MYSQL_FLUSH | MySQL FLUSH |
| 0x050F | 0x050F | SBLR3_MYSQL_KILL | MySQL KILL |
| 0x0510 | 0x0510 | SBLR3_MYSQL_LOCK_TABLES | MySQL LOCK TABLES |
| 0x0511 | 0x0511 | SBLR3_MYSQL_UNLOCK_TABLES | MySQL UNLOCK TABLES |
| 0x0512 | 0x0512 | SBLR3_PREPARE_STMT | PREPARE statement |
| 0x0514 | 0x0514 | SBLR3_SET_BIT | Set bit |
| 0x0516 | 0x0516 | SBLR3_SET_BYTE | Set byte |
| 0x0518 | 0x0518 | SBLR3_SET_LOCAL_TIMEOUT | Set local timeout |
| 0x051A | 0x051A | SBLR3_SET_NAMES | SET NAMES |
| 0x051C | 0x051C | SBLR3_SET_SQL_DIALECT | SET SQL DIALECT |
| 0x051E | 0x051E | SBLR3_SET_VARIABLE | SET variable |

SHOW statements (0x0520-0x0564):
- SBLR3_SHOW_ALL through SBLR3_SHOW_VIEW (see generated header for full list)

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_opcodes.generated.cpp:163-218`

### Query Composition Opcodes (0x0600-0x06FF)

| Opcode | Hex | Name | Description |
|--------|-----|------|-------------|
| 0x0601 | 0x0601 | SBLR3_ASSIGNMENT | Assignment |
| 0x0602 | 0x0602 | SBLR3_BEGIN_LIST | Begin list |
| 0x0603 | 0x0603 | SBLR3_COLUMN_DEF | Column definition |
| 0x0604 | 0x0604 | SBLR3_COLUMN_REF | Column reference |
| 0x0605 | 0x0605 | SBLR3_END_LIST | End list |
| 0x0607 | 0x0607 | SBLR3_CTE_DEF | CTE definition |
| 0x0609 | 0x0609 | SBLR3_CTE_SCAN | CTE scan |
| 0x060A | 0x060A | SBLR3_DISTINCT_ON | DISTINCT ON |
| 0x060C | 0x060C | SBLR3_EXCEPT | EXCEPT |
| 0x060E | 0x060E | SBLR3_EXCEPT_ALL | EXCEPT ALL |
| 0x0610 | 0x0610 | SBLR3_GROUPING_FUNC | GROUPING function |
| 0x0612 | 0x0612 | SBLR3_GROUP_CUBE | GROUP BY CUBE |
| 0x0614 | 0x0614 | SBLR3_GROUP_GROUPING_SETS | GROUP BY GROUPING SETS |
| 0x0616 | 0x0616 | SBLR3_GROUP_ROLLUP | GROUP BY ROLLUP |
| 0x0617 | 0x0617 | SBLR3_INSERTED_COLUMN_REF | INSERTED column ref |
| 0x0619 | 0x0619 | SBLR3_INTERSECT | INTERSECT |
| 0x061B | 0x061B | SBLR3_INTERSECT_ALL | INTERSECT ALL |
| 0x061D | 0x061D | SBLR3_IN_LIST | IN list |
| 0x061F | 0x061F | SBLR3_ON_CONFLICT | ON CONFLICT |
| 0x0621 | 0x0621 | SBLR3_ON_CONFLICT_COLUMN | ON CONFLICT column |
| 0x0623 | 0x0623 | SBLR3_ON_CONFLICT_CONSTRAINT | ON CONFLICT constraint |
| 0x0625 | 0x0625 | SBLR3_ON_CONFLICT_DO_NOTHING | DO NOTHING |
| 0x0627 | 0x0627 | SBLR3_ON_CONFLICT_DO_UPDATE | DO UPDATE |
| 0x0629 | 0x0629 | SBLR3_ON_CONFLICT_WHERE | ON CONFLICT WHERE |
| 0x062B | 0x062B | SBLR3_RETURNING | RETURNING |
| 0x062D | 0x062D | SBLR3_SELECT_TABLE_STAR | SELECT table.* |
| 0x062F | 0x062F | SBLR3_SUBQUERY_ARRAY | Subquery as array |
| 0x0631 | 0x0631 | SBLR3_SUBQUERY_END | Subquery end |
| 0x0633 | 0x0633 | SBLR3_SUBQUERY_EXISTS | EXISTS subquery |
| 0x0635 | 0x0635 | SBLR3_SUBQUERY_IN | IN subquery |
| 0x0637 | 0x0637 | SBLR3_SUBQUERY_NOT_IN | NOT IN subquery |
| 0x0639 | 0x0639 | SBLR3_SUBQUERY_SCALAR | Scalar subquery |
| 0x063B | 0x063B | SBLR3_UNION | UNION |
| 0x063D | 0x063D | SBLR3_UNION_ALL | UNION ALL |
| 0x063F | 0x063F | SBLR3_WITH_CLAUSE | WITH clause |

Window/Frame opcodes (0x0640-0x0660):
- FRAME_CLAUSE, FRAME_CURRENT_ROW, FRAME_FOLLOWING, FRAME_GROUPS, etc.
- GROUP_BY, HASH_JOIN, HAVING, INDEX_REF, JOIN_CONDITION, etc.

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_opcodes.generated.cpp:219-286`

### Expression Opcodes (0x0700-0x07FF)

| Opcode | Hex | Name | Description |
|--------|-----|------|-------------|
| 0x0701 | 0x0701 | SBLR3_CASE_WHEN | CASE WHEN |
| 0x0702 | 0x0702 | SBLR3_COALESCE | COALESCE |
| 0x0703 | 0x0703 | SBLR3_EXPR_ADD | Addition (+) |
| 0x0704 | 0x0704 | SBLR3_EXPR_AND | Logical AND |
| 0x0705 | 0x0705 | SBLR3_EXPR_CAST | CAST |
| 0x0706 | 0x0706 | SBLR3_EXPR_DIVIDE | Division (/) |
| 0x0707 | 0x0707 | SBLR3_EXPR_EQ | Equality (=) |
| 0x0708 | 0x0708 | SBLR3_EXPR_GE | Greater or equal (>=) |
| 0x0709 | 0x0709 | SBLR3_EXPR_GT | Greater than (>) |
| 0x070A | 0x070A | SBLR3_EXPR_ILIKE | ILIKE |
| 0x070B | 0x070B | SBLR3_EXPR_LE | Less or equal (<=) |
| 0x070C | 0x070C | SBLR3_EXPR_LIKE | LIKE |
| 0x070D | 0x070D | SBLR3_EXPR_LT | Less than (<) |
| 0x070E | 0x070E | SBLR3_EXPR_MODULO | Modulo (%) |
| 0x070F | 0x070F | SBLR3_EXPR_MULTIPLY | Multiplication (*) |
| 0x0710 | 0x0710 | SBLR3_EXPR_NE | Not equal (<>) |
| 0x0711 | 0x0711 | SBLR3_EXPR_OR | Logical OR |
| 0x0712 | 0x0712 | SBLR3_EXPR_SUBTRACT | Subtraction (-) |
| 0x0713 | 0x0713 | SBLR3_EXPR_DIV_INT | Integer division |
| 0x0714 | 0x0714 | SBLR3_EXPR_FUNCTION_CALL | Function call |
| 0x0715 | 0x0715 | SBLR3_EXPR_IS_NULL | IS NULL |
| 0x0716 | 0x0716 | SBLR3_EXPR_NOT | Logical NOT |
| 0x0717 | 0x0717 | SBLR3_ILIKE_ESCAPE | ILIKE ESCAPE |
| 0x0718 | 0x0718 | SBLR3_LIKE_ESCAPE | LIKE ESCAPE |
| 0x0719 | 0x0719 | SBLR3_NULL_SAFE_EQ | NULL-safe equality (<=>) |
| 0x071B | 0x071B | SBLR3_PRED_CONTAINING | CONTAINING |
| 0x071C | 0x071C | SBLR3_PRED_STARTING_WITH | STARTING WITH |
| 0x071D | 0x071D | SBLR3_NULLIF | NULLIF |

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_opcodes.generated.cpp:287-314`

### Function Opcodes (0x0800-0x08FF)

String functions (0x0802-0x08BE):
- ASCII, BIT_AND, BIT_COUNT, BIT_LENGTH, BIT_MASK, BIT_NOT, BIT_OR, etc.
- CHR, DECODE, ENCODE, EXTRACT
- FUNC_ABS through FUNC_UPPER (see generated header for full list)

JSON functions (0x08BF-0x08CC):
- JSONB_BUILD_ARRAY, JSONB_BUILD_OBJECT, JSONB_EXTRACT_PATH, JSONB_SET
- JSON_ARRAY, JSON_ARROW, JSON_DOUBLE_ARROW, etc.

Sequence functions (0x08CD-0x08CF):
- SEQUENCE_CURRVAL, SEQUENCE_NEXTVAL, SEQUENCE_SETVAL

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_opcodes.generated.cpp:315-444`

### Aggregate Opcodes (0x0900-0x09FF)

| Opcode | Hex | Name | Description |
|--------|-----|------|-------------|
| 0x0901 | 0x0901 | SBLR3_AGG_ACCUMULATE | Accumulate |
| 0x0902 | 0x0902 | SBLR3_AGG_AVG | AVG |
| 0x0903 | 0x0903 | SBLR3_AGG_CORR | CORR |
| 0x0904 | 0x0904 | SBLR3_AGG_COUNT | COUNT |
| 0x0905 | 0x0905 | SBLR3_AGG_COVAR_POP | COVAR_POP |
| 0x0906 | 0x0906 | SBLR3_AGG_FINALIZE | Finalize aggregate |
| 0x0907 | 0x0907 | SBLR3_AGG_INIT | Initialize aggregate |
| 0x0908 | 0x0908 | SBLR3_AGG_MAX | MAX |
| 0x0909 | 0x0909 | SBLR3_AGG_MIN | MIN |
| 0x090A | 0x090A | SBLR3_AGG_REGR_AVGX | REGR_AVGX |
| 0x090B | 0x090B | SBLR3_AGG_REGR_AVGY | REGR_AVGY |
| 0x090C | 0x090C | SBLR3_AGG_REGR_COUNT | REGR_COUNT |
| 0x090D | 0x090D | SBLR3_AGG_REGR_INTERCEPT | REGR_INTERCEPT |
| 0x090E | 0x090E | SBLR3_AGG_REGR_R2 | REGR_R2 |
| 0x090F | 0x090F | SBLR3_AGG_REGR_SLOPE | REGR_SLOPE |
| 0x0910 | 0x0910 | SBLR3_AGG_REGR_SXX | REGR_SXX |
| 0x0911 | 0x0911 | SBLR3_AGG_REGR_SXY | REGR_SXY |
| 0x0912 | 0x0912 | SBLR3_AGG_REGR_SYY | REGR_SYY |
| 0x0913 | 0x0913 | SBLR3_AGG_STDDEV_POP | STDDEV_POP |
| 0x0914 | 0x0914 | SBLR3_AGG_STDDEV_SAMP | STDDEV_SAMP |
| 0x0915 | 0x0915 | SBLR3_AGG_SUM | SUM |
| 0x0916 | 0x0916 | SBLR3_AGG_VAR_POP | VAR_POP |
| 0x0917 | 0x0917 | SBLR3_AGG_VAR_SAMP | VAR_SAMP |
| 0x0918 | 0x0918 | SBLR3_ARRAY_AGG | ARRAY_AGG |

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_opcodes.generated.cpp:445-475`

### Window Function Opcodes (0x0A00-0x0AFF)

| Opcode | Hex | Name | Description |
|--------|-----|------|-------------|
| 0x0A02 | 0x0A02 | SBLR3_WIN_CUME_DIST | CUME_DIST |
| 0x0A04 | 0x0A04 | SBLR3_WIN_PERCENT_RANK | PERCENT_RANK |
| 0x0A05 | 0x0A05 | SBLR3_WIN_DENSE_RANK | DENSE_RANK |
| 0x0A06 | 0x0A06 | SBLR3_WIN_FIRST_VALUE | FIRST_VALUE |
| 0x0A07 | 0x0A07 | SBLR3_WIN_LAG | LAG |
| 0x0A08 | 0x0A08 | SBLR3_WIN_LAST_VALUE | LAST_VALUE |
| 0x0A09 | 0x0A09 | SBLR3_WIN_LEAD | LEAD |
| 0x0A0A | 0x0A0A | SBLR3_WIN_NTH_VALUE | NTH_VALUE |
| 0x0A0B | 0x0A0B | SBLR3_WIN_RANK | RANK |
| 0x0A0C | 0x0A0C | SBLR3_WIN_ROW_NUMBER | ROW_NUMBER |

### Type Opcodes (0x0B00-0x0BFF)

Complete type descriptor opcodes from SBLR3_TYPE_UNKNOWN (0x0B00) through SBLR3_TYPE_VARCHAR (0x0B46):
- Network: CIDR, INET, MACADDR, MACADDR8
- Geometry: GEOMETRY, GEOMETRYCOLLECTION, LINESTRING, POINT, POLYGON, etc.
- Ranges: DATERANGE, INT4RANGE, INT8RANGE, NUMRANGE, TSRANGE, TSTZRANGE
- Complex: ARRAY, COMPOSITE, DOMAIN, ENUM, JSON, JSONB, ROW, SET, VARIANT, VECTOR
- Scalars: INT8-INT128, UINT8-UINT128, FLOAT32, DOUBLE, DECIMAL, BOOLEAN, etc.

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_opcodes.generated.cpp:486-555`

### Literal Opcodes (0x0C00-0x0CFF)

Literal value opcodes from SBLR3_LITERAL_CIDR (0x0C01) through SBLR3_LITERAL_BLOB_LOCATOR (0x0C33):
- Network literals: CIDR, INET, MACADDR, MACADDR8
- Temporal: DATE, TIME, TIMESTAMP, INTERVAL, DATETIME, YEAR
- Numeric: INT8-INT128, UINT8-UINT128, FLOAT32, DOUBLE, DECIMAL, MEDIUMINT
- String/Binary: STRING, BINARY, BLOB, TEXT, BYTEA
- Complex: JSON, JSONB, XML, UUID, ARRAY, RANGE, VARIANT, TSVECTOR, TSQUERY

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_opcodes.generated.cpp:556-607`

### PSQL Control Flow Opcodes (0x0D00-0x0DFF)

| Opcode | Hex | Name | Description |
|--------|-----|------|-------------|
| 0x0D02 | 0x0D02 | SBLR3_ASSIGN | Variable assignment |
| 0x0D04 | 0x0D04 | SBLR3_BLOCK | Code block |
| 0x0D06 | 0x0D06 | SBLR3_CALL | Procedure call |
| 0x0D08 | 0x0D08 | SBLR3_CURSOR_CLOSE | Close cursor |
| 0x0D0A | 0x0D0A | SBLR3_CURSOR_DECLARE | Declare cursor |
| 0x0D0C | 0x0D0C | SBLR3_CURSOR_FETCH | Fetch cursor |
| 0x0D0E | 0x0D0E | SBLR3_CURSOR_OPEN | Open cursor |
| 0x0D10 | 0x0D10 | SBLR3_DECLARE | Variable declaration |
| 0x0D12 | 0x0D12 | SBLR3_ELSE | ELSE branch |
| 0x0D14 | 0x0D14 | SBLR3_ELSIF | ELSIF branch |
| 0x0D16 | 0x0D16 | SBLR3_EXCEPTION_HANDLER | Exception handler |
| 0x0D18 | 0x0D18 | SBLR3_EXCEPT_HANDLER | Exception handler (alt) |
| 0x0D1A | 0x0D1A | SBLR3_EXIT | EXIT statement |
| 0x0D1C | 0x0D1C | SBLR3_FUNCTION | Function definition |
| 0x0D1E | 0x0D1E | SBLR3_IF | IF statement |
| 0x0D20 | 0x0D20 | SBLR3_JUMP | Unconditional jump |
| 0x0D22 | 0x0D22 | SBLR3_JUMP_IF_FALSE | Conditional jump (false) |
| 0x0D24 | 0x0D24 | SBLR3_JUMP_IF_TRUE | Conditional jump (true) |
| 0x0D26 | 0x0D26 | SBLR3_LABEL | Label definition |
| 0x0D28 | 0x0D28 | SBLR3_LOOP | LOOP construct |
| 0x0D2A | 0x0D2A | SBLR3_PARAM_IN | IN parameter |
| 0x0D2C | 0x0D2C | SBLR3_PARAM_INOUT | INOUT parameter |
| 0x0D2E | 0x0D2E | SBLR3_PARAM_OUT | OUT parameter |
| 0x0D30 | 0x0D30 | SBLR3_PROCEDURE | Procedure definition |
| 0x0D32 | 0x0D32 | SBLR3_RAISE | RAISE statement |
| 0x0D34 | 0x0D34 | SBLR3_RETURN | RETURN statement |
| 0x0D35 | 0x0D35 | SBLR3_SUSPEND | SUSPEND statement |
| 0x0D37 | 0x0D37 | SBLR3_TRY | TRY block |
| 0x0D39 | 0x0D39 | SBLR3_VAR_LOAD | Load variable |
| 0x0D3B | 0x0D3B | SBLR3_VAR_STORE | Store variable |
| 0x0D3D | 0x0D3D | SBLR3_WHILE | WHILE loop |

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_opcodes.generated.cpp:608-644`

### Index Operations (0x0E00-0x0EFF)

| Opcode | Hex | Name | Description |
|--------|-----|------|-------------|
| 0x0E02 | 0x0E02 | SBLR3_COLUMNSTORE_INSERT | Columnstore insert |
| 0x0E04 | 0x0E04 | SBLR3_COLUMNSTORE_SCAN | Columnstore scan |
| 0x0E06 | 0x0E06 | SBLR3_GIN_INSERT | GIN insert |
| 0x0E08 | 0x0E08 | SBLR3_GIN_SEARCH | GIN search |
| 0x0E0A | 0x0E0A | SBLR3_HNSW_INSERT | HNSW insert |
| 0x0E0C | 0x0E0C | SBLR3_HNSW_SEARCH | HNSW search |
| 0x0E0E | 0x0E0E | SBLR3_INDEX_DELETE | Index delete |
| 0x0E10 | 0x0E10 | SBLR3_INDEX_INSERT | Index insert |
| 0x0E12 | 0x0E12 | SBLR3_INDEX_REINDEX | REINDEX |
| 0x0E14 | 0x0E14 | SBLR3_INDEX_SCAN | Index scan |
| 0x0E16 | 0x0E16 | SBLR3_INDEX_SCAN_END | End index scan |
| 0x0E18 | 0x0E18 | SBLR3_INDEX_SCAN_NEXT | Index scan next |
| 0x0E1A | 0x0E1A | SBLR3_INDEX_SCAN_START | Start index scan |
| 0x0E1C | 0x0E1C | SBLR3_INDEX_SEARCH | Index search |
| 0x0E1E | 0x0E1E | SBLR3_INDEX_STATS | Index statistics |
| 0x0E20 | 0x0E20 | SBLR3_INDEX_TYPE | Index type |
| 0x0E22 | 0x0E22 | SBLR3_INDEX_UPDATE | Index update |
| 0x0E24 | 0x0E24 | SBLR3_INDEX_VACUUM | Index vacuum |

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_opcodes.generated.cpp:644-662`

### Array/Range Operations (0x0F00-0x0FFF)

Array operations (0x0F01-0x0F4A):
- ARRAY_TO_STRING, ARRAY_APPEND, ARRAY_CAT, ARRAY_CONSTRUCT, etc.
- ARRAY_CONTAINS, ARRAY_DIMS, ARRAY_EQ, ARRAY_LENGTH, etc.

Range operations (0x0F24-0x0F48):
- RANGE_ADJACENT, RANGE_CONSTRUCT, RANGE_CONTAINS_ELEM, etc.
- RANGE_INTERSECTION, RANGE_MERGE, RANGE_OVERLAPS, etc.

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_opcodes.generated.cpp:662-701`

### Extended Opcodes (0x6000-0x61FF)

Multi-model and service channel opcodes:
- 0x6001-0x6009: Document, timeseries, vector, search, UDR operations
- 0x6101-0x6173: Session, config, domain, index, textsearch, admin, CQL, MongoDB, Cypher, Redis, Milvus, cluster, alert, healing, job, shard, cube, security, service channel, and FDW operations

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_opcodes.generated.cpp:751-877`

### Runtime API

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_opcode_registry.h
namespace scratchbird::sblr::v3 {
    // Get opcode name from value
    const char* opcodeName(uint16_t opcode);
    
    // Check if opcode is known
    bool isKnownOpcode(uint16_t opcode);
}
```

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_opcodes.generated.cpp:879-887`

## Invariants

1. **Opcode Uniqueness**: Each 16-bit opcode value maps to exactly one symbolic name
   - Verification: Generated code maintains unique mapping in kOpcodeNames hash map

2. **Family Boundaries**: Opcodes are grouped by functional family in contiguous ranges
   - Verification: Static assertions in generated header

3. **Schema Mapping**: Every opcode has an associated payload schema (may be empty)
   - Verification: schemaForOpcode() returns non-null for most opcodes

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| Unknown opcode | isKnownOpcode() returns false | Return error in executor |
| Invalid opcode in stream | Validation during decode | Reject container with SBLR-E-0012 |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `test_sblr_v3_opcode_identity.cpp` | Opcode identity and mapping |
| `test_sblr_v3_payload_codec.cpp` | Payload encoding/decoding |
| `test_sblr_v3_container.cpp` | Container validation |
| `test_sblr_type_opcodes.cpp` | Type opcode mapping |

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| Opcode | 16-bit unsigned integer identifying an operation |
| SBLR | ScratchBird Language Runtime |
| Family | Logical grouping of related opcodes |
| Schema | Payload structure definition for an opcode |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | SBLR Team |
