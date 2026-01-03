# SBLR Bytecode Examples for Alpha 1.05

## IMPLEMENTATION STATUS: 🟢 FULLY IMPLEMENTED (Updated 2025-10-01)

**SBLR (ScratchBird Language Representation) is now complete:**
- ✅ **Bytecode Generator** - Converts AST to bytecode (fully implemented)
- ✅ **Executor** - Complete execution engine (941 lines added October 2025)
- ✅ **All SQL Operations** - CREATE TABLE, INSERT, SELECT with WHERE
- ✅ **Expression Evaluation** - Arithmetic, comparison, logical operators
- ✅ **Type System** - Full type conversion support
- ✅ **Tuple Serialization** - Binary format with null bitmap
- ✅ **Table Scanning** - Iterator-based heap scans
- ✅ **WHERE Clause** - Row context evaluation

**Implementation files:**
- `src/sblr/bytecode_generator_v2.cpp` - AST to bytecode conversion
- `src/sblr/executor.cpp` - Bytecode execution (lines 23-1218)
- `include/scratchbird/sblr/executor.h` - Executor interface
- `include/scratchbird/sblr/opcodes.h` - Opcode definitions

## Overview

This document shows example SBLR bytecode generated for the SQL statements supported in Alpha 1.05. All examples below are **actually working** in the current implementation.

## 1. CREATE TABLE Example

### SQL Statement
```sql
CREATE TABLE users (
    id INTEGER NOT NULL,
    name VARCHAR(50)
)
```

### Generated SBLR Bytecode
```
00: SBLR_VERSION     0x01            ; SBLR version 1
01: SBLR_BEGIN                       ; Begin module

; Message 0: Table definition
02: SBLR_MESSAGE     0x00            ; Message ID 0
03: SBLR_LONG        0x02            ; 2 fields
04: SBLR_FIELD       0x00            ; Field 0: id
05: SBLR_LONG                        ; Type: 32-bit integer
06: SBLR_NOT_NULL                    ; Constraint: NOT NULL
07: SBLR_FIELD       0x01            ; Field 1: name
08: SBLR_VARYING     0x32            ; Type: VARCHAR(50)
09: SBLR_NULLABLE                    ; Constraint: NULL allowed

; Create table operation
0A: SBLR_CREATE_TABLE
0B: SBLR_LITERAL     "users"         ; Table name
0C: SBLR_MESSAGE_REF 0x00            ; Reference message 0

0D: SBLR_EOC                         ; End of command
0E: SBLR_END                         ; End module
```

## 2. INSERT Example

### SQL Statement
```sql
INSERT INTO users (id, name) VALUES (1, 'John Doe')
```

### Generated SBLR Bytecode
```
00: SBLR_VERSION     0x01            ; SBLR version 1
01: SBLR_BEGIN                       ; Begin module

; Message 0: Insert data
02: SBLR_MESSAGE     0x00            ; Message ID 0
03: SBLR_LONG        0x02            ; 2 values
04: SBLR_LITERAL     0x01            ; Value: 1
05: SBLR_LITERAL     "John Doe"      ; Value: 'John Doe'

; Store operation
06: SBLR_STORE                       ; INSERT operation
07: SBLR_LITERAL     "users"         ; Table name
08: SBLR_FIELD_LIST  0x02            ; 2 fields
09: SBLR_LITERAL     "id"            ; Field: id
0A: SBLR_LITERAL     "name"          ; Field: name
0B: SBLR_MESSAGE_REF 0x00            ; Values from message 0

0C: SBLR_EOC                         ; End of command
0D: SBLR_END                         ; End module
```

## 3. SELECT Example (Simple)

### SQL Statement
```sql
SELECT * FROM users
```

### Generated SBLR Bytecode
```
00: SBLR_VERSION     0x01            ; SBLR version 1
01: SBLR_BEGIN                       ; Begin module

; Message 0: Result set definition
02: SBLR_MESSAGE     0x00            ; Message ID 0
03: SBLR_LONG        0x02            ; 2 fields
04: SBLR_FIELD       0x00            ; Field 0
05: SBLR_FIELD       0x01            ; Field 1

; Select operation
06: SBLR_FOR_SELECT                  ; SELECT loop
07: SBLR_LITERAL     "users"         ; Table name
08: SBLR_ALL_FIELDS                  ; Select *
09: SBLR_BEGIN                       ; Begin loop body
0A: SBLR_SEND        0x00            ; Send message 0
0B: SBLR_END                         ; End loop body

0C: SBLR_EOC                         ; End of command
0D: SBLR_END                         ; End module
```

## 4. SELECT with WHERE Example

### SQL Statement
```sql
SELECT id, name FROM users WHERE id = 1
```

### Generated SBLR Bytecode
```
00: SBLR_VERSION     0x01            ; SBLR version 1
01: SBLR_BEGIN                       ; Begin module

; Message 0: Result set definition
02: SBLR_MESSAGE     0x00            ; Message ID 0
03: SBLR_LONG        0x02            ; 2 fields
04: SBLR_FIELD       0x00            ; Field 0: id
05: SBLR_FIELD       0x01            ; Field 1: name

; Select operation with condition
06: SBLR_FOR_SELECT                  ; SELECT loop
07: SBLR_LITERAL     "users"         ; Table name
08: SBLR_FIELD_LIST  0x02            ; 2 fields
09: SBLR_LITERAL     "id"            ; Field: id
0A: SBLR_LITERAL     "name"          ; Field: name
0B: SBLR_CONDITION                   ; WHERE clause
0C: SBLR_FIELD       "id"            ; Left: field id
0D: SBLR_EQL                         ; Operator: =
0E: SBLR_LITERAL     0x01            ; Right: 1
0F: SBLR_BEGIN                       ; Begin loop body
10: SBLR_SEND        0x00            ; Send message 0
11: SBLR_END                         ; End loop body

12: SBLR_EOC                         ; End of command
13: SBLR_END                         ; End module
```

## Bytecode Structure Notes

### Header Format
Every SBLR module starts with:
- Version byte (0x01 for version 1)
- Begin marker (0x02)

### Message Format
Messages define data structures:
- Message ID (0-255)
- Field count
- Field definitions (type, constraints)

### Literal Encoding
- Integers: Direct binary encoding
- Strings: Length-prefixed UTF-8
- NULL: Special marker (0xFF)

### Control Flow
- FOR_SELECT creates an implicit loop
- BEGIN/END mark block boundaries
- SEND transmits result rows

### Type Encoding
- SBLR_LONG: 32-bit integer
- SBLR_INT64: 64-bit integer
- SBLR_DOUBLE: 64-bit float
- SBLR_VARYING: Variable-length string

## Size Estimates

Typical bytecode sizes:
- CREATE TABLE: 20-50 bytes
- INSERT (single row): 30-60 bytes
- SELECT (simple): 20-40 bytes
- SELECT (with WHERE): 30-50 bytes

The compact binary format ensures minimal memory usage and fast parsing.
