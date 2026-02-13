# SBLR V3 Test Vectors (Authoritative)
Status: Authoritative (V3)
Last Updated: 2026-02-08

Purpose: provide canonical bytecode test vectors with expected validation
results for each opcode family. These vectors are sufficient to implement a
bytecode verifier and basic executor harness.

All payloads reference `SBLR_V3_BYTECODE_EXAMPLES.md` unless explicitly listed.
Full stream-level vectors are in `SBLR_V3_TEST_VECTORS_FULL.md`.

---

## 1) DDL Vectors

### CREATE TABLE (basic)
- Payload: `SBLR3_CREATE_TABLE` → `CREATE TABLE t (id INT)`
- Source: `SBLR_V3_BYTECODE_EXAMPLES.md`
- Expect: **VALID**

### CREATE INDEX (basic)
- Payload: `SBLR3_CREATE_INDEX` → `CREATE INDEX idx ON t(id)`
- Expect: **VALID**

### ALTER TABLE ADD COLUMN
- Payload: `SBLR3_ALTER_TABLE` action `ADD_COLUMN`
- Expect: **VALID**

### DROP TABLE IF EXISTS
- Payload: `SBLR3_DROP_TABLE` with `if_exists=1`
- Expect: **VALID**

---

## 2) DML Vectors

### SELECT DISTINCT
- Payload: `SBLR3_SELECT` with `flags=0x0001`
- Expect: **VALID**

### INSERT VALUES
- Payload: `SBLR3_INSERT` values list
- Expect: **VALID**

### UPDATE
- Payload: `SBLR3_UPDATE` with set_items
- Expect: **VALID**

### DELETE USING
- Payload: `SBLR3_DELETE` with `using` table
- Expect: **VALID**

### MERGE
- Payload: `SBLR3_MERGE` canonical form
- Expect: **VALID**

### COPY
- Payload: `SBLR3_COPY` TO STDOUT
- Expect: **VALID**

---

## 3) Literal Vectors

### INT32
- Opcode: `SBLR3_LITERAL_INT32` value `1`
- Expect: **VALID**

### UINT64
- Opcode: `SBLR3_LITERAL_UINT64` value `18446744073709551615`
- Expect: **VALID**

### FLOAT32
- Opcode: `SBLR3_LITERAL_FLOAT32` value `1.5`
- Expect: **VALID**

### TIME_TZ
- Opcode: `SBLR3_LITERAL_TIME_TZ` time `01:00:00+00`
- Expect: **VALID**

### TIMESTAMP_TZ
- Opcode: `SBLR3_LITERAL_TIMESTAMP_TZ` epoch `0` + `+00`
- Expect: **VALID**

### ARRAY
- Opcode: `SBLR3_LITERAL_ARRAY` with 2 elements
- Expect: **VALID**

### RANGE (invalid bounds)
- Opcode: `SBLR3_LITERAL_RANGE` lower > upper
- Expect: **INVALID** (`V3E-0020`)

### TSVECTOR (non-canonical)
- Opcode: `SBLR3_LITERAL_TSVECTOR` non-canonical string
- Expect: **INVALID** (`V3E-0050`)

---

## 4) Transaction Vectors

### START TRANSACTION
- Opcode: `SBLR3_TXN_START` with default flags
- Expect: **VALID**

### COMMIT
- Opcode: `SBLR3_TXN_COMMIT`
- Expect: **VALID**

### ROLLBACK
- Opcode: `SBLR3_TXN_ROLLBACK`
- Expect: **VALID**

---

## 5) Canonicalization Vectors

### SYMBOL_TABLE not sorted
- Expect: **INVALID** (`V3E-0100`)

### CONSTANT_POOL not sorted
- Expect: **INVALID** (`V3E-0101`)
