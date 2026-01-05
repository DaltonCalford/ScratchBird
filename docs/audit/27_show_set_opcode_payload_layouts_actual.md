# SHOW/SET Opcode Payload Layouts (Actual)

Purpose: Byte-accurate payload layouts for SHOW/SET opcodes, based on V2 bytecode generation and executor decoding.

Status: static code review snapshot; no runtime execution performed.

## Encoding conventions
- Extended opcode prefix: [EXTENDED_OPCODE=0xFF][ext_opcode:uint16 LE][payload...]
- string: [length:int32 LE][bytes]
- list: [BEGIN_LIST opcode][count:uint32][...][END_LIST opcode]
- Expression: inline expression bytecode stream (not expanded here)

## SET opcodes

### SET_TRANSACTION (Opcode::SET_TRANSACTION)
Payload order:
- flags:uint16
- conflict_action:uint8
- conflict_error_code:int32 if flags HAS_CONFLICT_ERROR_CODE
- autocommit_mode:uint8 if flags HAS_AUTOCOMMIT
- isolation:uint8 if flags HAS_ISOLATION
- read_committed_mode:uint8 if flags HAS_READ_COMMITTED_MODE
- access_mode:uint8 if flags HAS_ACCESS_MODE
- deferrable:uint8 if flags HAS_DEFERRABLE
- wait_mode:uint8 if flags HAS_WAIT_MODE
- lock_timeout:uint32 if flags HAS_LOCK_TIMEOUT
- reservations if flags HAS_RESERVATIONS:
  - BEGIN_LIST opcode
  - count:uint32
  - repeated count:
    - TABLE_REF opcode
    - table_name:string
    - lock_mode:uint8
    - for_write:uint8
  - END_LIST opcode

### EXT_SET_AUTOCOMMIT (0x0102)
Payload:
- mode:uint8 (0=OFF,1=ON)
- conflict_action:uint8
- conflict_error_code:int32 if conflict_action==ERROR

### EXT_SET_SQL_DIALECT (0x72)
Payload:
- dialect:uint8 (1..3)

### EXT_SET_NAMES (0x73)
Payload:
- charset_name:string

### EXT_SET_LOCAL_TIMEOUT (0x74)
Payload:
- timeout_seconds:int32

### EXT_SET_ROLE (0xD5)
Executor expects:
- flags:uint8 (bit0=reset)
- role_name:string if not reset
Current V2 generator emits:
- role_name:string if set, OR
- int32 0 for reset

### EXT_SET_SESSION_AUTH (0xD6)
Executor expects:
- flags:uint8 (bit0=reset)
- user_name:string if not reset
Current V2 generator emits:
- user_name:string if set, OR
- int32 0 for reset

### EXT_SET_CONSTRAINTS (0x4F)
Payload:
- flags:uint8 (bit0=ALL, bit1=DEFERRED)
- name_count:uint8 if not ALL
- constraint_name:string repeated name_count

### EXT_SET_VARIABLE (0x81)
Payload (executor decoding):
- variable_name:string
- next byte determines form:
  - 0 -> DEFAULT/RESET
  - 1 -> expression follows
  - BEGIN_LIST opcode -> list form (count:uint32, then string entries, then END_LIST opcode)
  - LITERAL_NULL opcode -> DEFAULT/RESET
  - otherwise -> expression follows (executor path)
V2 generator emits only the marker 0 or 1 form.

## SHOW opcodes

### Basic catalog SHOW (MySQL-style)
- EXT_SHOW_TABLES (0x05): database:string, like_pattern:string
- EXT_SHOW_DATABASES (0x06): like_pattern:string
- EXT_SHOW_COLUMNS (0x07): table:string, like_pattern:string
- EXT_SHOW_INDEXES (0x08): table:string
- EXT_SHOW_CREATE_TABLE (0x09): table:string

### Firebird-style detailed SHOW
- EXT_SHOW_TABLE (0x5E): object_name:string
- EXT_SHOW_INDEX (0x5F): object_name:string
- EXT_SHOW_TRIGGER (0x60): object_name:string
- EXT_SHOW_PROCEDURE (0x61): object_name:string
- EXT_SHOW_FUNCTION (0x62): object_name:string
- EXT_SHOW_VIEW (0x63): object_name:string
- EXT_SHOW_DOMAIN (0x64): object_name:string
- EXT_SHOW_GENERATOR (0x65): object_name:string
- EXT_SHOW_SCHEMA (0x66): schema_name:string (empty means list all)
- EXT_SHOW_ROLE (0x67): object_name:string
- EXT_SHOW_GRANTS (0x68): object_name:string (empty means all)
- EXT_SHOW_CHECKS (0x69): object_name:string
- EXT_SHOW_COLLATIONS (0x6A): like_pattern:string
- EXT_SHOW_COMMENTS (0x6B): object_name:string
- EXT_SHOW_DEPENDENCIES (0x6C): object_name:string
- EXT_SHOW_PACKAGE (0x6D): object_name:string
- EXT_SHOW_SYSTEM (0x6E): no payload
- EXT_SHOW_SQL_DIALECT (0x6F): no payload
- EXT_SHOW_VERSION (0x70): no payload
- EXT_SHOW_DATABASE (0x71): no payload

### Schema navigation SHOW
- EXT_SHOW_SCHEMA_PATH (0x75): no payload
- EXT_SHOW_SCHEMA_TREE (0x76): max_depth:int32, from_path:string
- EXT_SHOW_SEARCH_PATH (0x77): no payload
- EXT_SHOW_LOCATION (0x78): object_name:string, type_hint:string
- EXT_SHOW_RESOLVED (0x79): object_name:string
- EXT_SHOW_OBJECTS (0x7A):
  - schema_scope:uint8 (0=CURRENT,1=IN_PATH,2=IN_SCHEMA)
  - schema_path:string
  - like_pattern:string

### Session parameter SHOW
- EXT_SHOW_VARIABLE (0x7B): variable_name:string
- EXT_SHOW_ALL (0x7C): no payload
- EXT_SHOW_TRANSACTION_LEVEL (0x7D): no payload

Notes:
- EXT_SHOW_VARIABLE/EXT_SHOW_ALL/EXT_SHOW_TRANSACTION_LEVEL are emitted by V2 but have no executor handlers.
- Schema-navigation SHOW opcodes are implemented in the executor but are not emitted by V2.
