# SBLR V3 Bytecode Canonicalization Rules

Status: Authoritative (V3)
Last Updated: 2026-02-08

Purpose: define deterministic canonicalization rules for bytecode generation.
These rules ensure identical SQL produces identical bytecode (hash‑stable).

---

## 1) Identifier Folding

- Unquoted identifiers MUST be folded to lowercase ASCII.
- Quoted identifiers preserve exact UTF‑8 bytes.
- Identifiers MUST be NFC‑normalized after folding.

---

## 2) Symbol Table Canonicalization

- Symbol table MUST be sorted by raw UTF‑8 byte order (byte‑by‑byte, shorter first).
- Duplicate symbols MUST be collapsed to a single `string_id`.
- Only identifiers and required strings are pooled (see
  `SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md`).

---

## 3) Constant Pool Canonicalization

- Constants MUST be sorted by `(tag, payload_bytes)` lexicographic order.
- Duplicate constants MUST be collapsed to a single `const_id`.
- Decimal constants must be normalized (no leading zero digits, normalized scale).

---

## 4) Unordered List Canonicalization

The following lists are **unordered** and MUST be emitted in sorted order:

- `OPTION_KV` list (by key `ident`, bytewise).
- GRANT/REVOKE privilege lists (by privilege name).
- Column lists in `ON CONFLICT (col, ...)` (by column name).
- Index `include` list (by column name).

All other lists preserve **source order**.

---

## 5) Whitespace and Formatting

- Whitespace is not preserved in bytecode.
- All emitted strings are canonical UTF‑8 (no BOM).

---

## 6) Canonicalization Verifier

The verifier MUST reject bytecode that violates any rule above, with error
codes defined in `SBLR_V3_VALIDATION_RULES.md`.
