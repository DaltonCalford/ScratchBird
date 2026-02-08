# SBLR V3 Test Vectors (Full Bytecode Streams)
Status: Authoritative (V3)
Last Updated: 2026-02-08

Purpose: provide complete, byte-level SBLR V3 streams including instruction
headers so a verifier/executor can be implemented without ambiguity.

Instruction header (per `SBLR_V3_OPCODE_SPEC.md`):
```
[opcode:u16][flags:u16][payload_len:u32][payload...]
```

All values are little-endian. `payload_len` is the exact byte count of the
payload section.

---

## 1) Minimal Valid Module

Stream:
```
SBLR3_VERSION:
  opcode      = 01 00
  flags       = 00 00
  payload_len = 00 00 00 00

SBLR3_END:
  opcode      = 00 00
  flags       = 00 00
  payload_len = 00 00 00 00
```

Expect: **VALID**

---

## 2) SELECT 1 (Full Stream)

Payload (from examples):
```
flags             = 00 00
select_items      = 01 0F 0C 00 00 04 00 00 00 01 00 00 00
from.opt          = 00
joins.count       = 00
where.opt         = 00
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

Instruction:
```
opcode      = 12 02
flags       = 00 00
payload_len = 1C 00 00 00
payload     = 00 00 01 0F 0C 00 00 04 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

Stream:
```
01 00 00 00 00 00 00 00
12 02 00 00 1C 00 00 00 00 00 01 0F 0C 00 00 04 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
```

Expect: **VALID**

---

## 3) INSERT VALUES (Full Stream)

Payload:
```
target            = 01 01 74
alias.opt         = 00
columns.count     = 01 02 69 64
source            = 01
values.count      = 01
row.count         = 01
row.int32         = 0F 0C 00 00 04 00 00 00 01 00 00 00
select.opt        = 00
on_conflict.opt   = 00
returning.count   = 00
```

Instruction:
```
opcode      = 13 02
flags       = 00 00
payload_len = 1B 00 00 00
payload     = 01 01 74 00 01 02 69 64 01 01 01 0F 0C 00 00 04 00 00 00 01 00 00 00 00 00 00
```

Stream:
```
01 00 00 00 00 00 00 00
13 02 00 00 1B 00 00 00 01 01 74 00 01 02 69 64 01 01 01 0F 0C 00 00 04 00 00 00 01 00 00 00 00 00 00
00 00 00 00 00 00 00 00
```

Expect: **VALID**

---

## 4) Invalid Canonicalization (Unsorted SYMBOL_TABLE)

Condition: SYMBOL_TABLE contains `b`, `a` (unsorted).

Expect: **INVALID** with `V3E-0100`.

---

## 5) Invalid Range (Lower > Upper)

Condition: `SBLR3_LITERAL_RANGE` lower > upper.

Expect: **INVALID** with `V3E-0020`.
