# SBLR V3 Constant Pool and Symbol Table Specification
Status: Authoritative (V3)
Last Updated: 2026-02-08

This document defines how V3 modules pool strings, literals, and catalog IDs for
deterministic bytecode size, stable hashing, and reproducible builds.

## Goals
- Deterministic ordering of pooled entries across builds.
- Stable hashing of bytecode containers independent of runtime memory layout.
- Explicit rules for when pooling is required, optional, or forbidden.

## Definitions
- **Symbol**: A UTF-8 string stored in the SYMBOL_TABLE and referenced by
  `string_id` (0-based index).
- **Constant**: A typed literal stored in CONSTANT_POOL and referenced by
  `const_id` (0-based index).
- **Catalog ID**: A UUID v7 identifying a catalog object (table, column, index,
  schema, etc.).

## Symbol Table (Normative)
The SYMBOL_TABLE is the authoritative pool for all identifiers and strings
used in the module unless explicitly exempted.

### Required Symbol Pooling
The following MUST be stored in SYMBOL_TABLE and referenced by `string_id`:
- Identifiers (schema, table, column, index, constraint, role, user, domain).
- Function/procedure names and parameter names.
- Named labels (PSQL labels, named windows, named CTEs).
- Collation and charset names.
- Error/exception names and SQLSTATE labels.
- Debug file names (if DEBUG_INFO present).

### optional Symbol Pooling
These MAY be pooled to reduce size, but are not required:
- Short string literals <= 32 bytes.
- SQL text in debug-only sections.

### Forbidden Symbol Pooling
These MUST NOT be pooled (must be stored inline in the relevant section):
- Privacy-sensitive secrets (password hashes, keys).
- Binary payloads (use CONSTANT_POOL bytes or inline payloads).
- Generated temporary names (when payload explicitly marks them as non-deterministic).

### Symbol Table Ordering (Deterministic)
Symbols are stored in lexicographic order using **raw UTF-8 byte order**:
1. Compare byte-by-byte (unsigned 0–255).
2. Shorter byte sequence sorts first if all common bytes match.

Normalization rules:
- **Identifiers** MUST be NFC-normalized **after** case folding and **before**
  being inserted into SYMBOL_TABLE.
- **String literals** MUST NOT be normalized (store exact UTF-8 bytes).

Symbols MUST be unique. Duplicate symbols are collapsed to the same `string_id`.

## Constant Pool (Normative)
The CONSTANT_POOL stores typed literal values shared across the module.

### Required Constant Pooling
The following literal types MUST be pooled:
- Integer and floating-point literals.
- Decimal literals (normalized representation).
- UUID literals.
- Boolean literals.
- Typed NULL literals.

### optional Constant Pooling
The following MAY be pooled:
- Short string literals <= 32 bytes.
- Small binary literals <= 32 bytes.

### Forbidden Constant Pooling
The following MUST NOT be pooled:
- Values marked `volatile` by opcode payload (e.g., random seed, session-specific).
- Runtime-computed defaults (e.g., `CURRENT_TIMESTAMP`) unless explicitly frozen by payload.
- Large byte arrays or LOB payloads (store inline or in TOAST/LOB).

### Constant Normalization Rules
To ensure deterministic hashing, constants are normalized before pooling:
- **int/uint**: minimal-width canonical value representation (no leading zeros).
- **float**: IEEE 754 binary64 canonical bytes.
- **decimal**: sign + scale + BCD bytes with no leading zero digits; scale normalized.
- **string**: NFC normalization of Unicode before pooling if marked as identifier or catalog string.
- **uuid**: 16-byte binary UUID v7 (network order preserved in payload; pool stores raw bytes).
- **bytes**: exact byte sequence, no normalization.
- **typed null**: type_id from TYPE opcode table.

### Constant Pool Ordering (Deterministic)
Constants are ordered by:
1. Tag order (ascending numeric tag).
2. Normalized payload bytes (lexicographic order, byte-by-byte, shorter first).

Duplicates are collapsed to the same `const_id`.

## Catalog ID Pooling
Catalog IDs are UUID v7 and MUST be pooled in CONSTANT_POOL using tag `uuid`.
All catalog references in bytecode must use `const_id` (no inline UUIDs) unless
explicitly marked as `inline_id` by payload (allowed only for tests or ephemeral
objects).

## Literal Reference Rules
- LITERAL opcodes refer to constants by `const_id` when the literal is pooled.
- LITERAL opcodes may inline payloads only when pooling is explicitly forbidden
  by this spec or the opcode payload.

## Deterministic Hashing Rules
To produce stable module hashes:
- Symbol table and constant pool must be deterministically ordered per this spec.
- Non-deterministic fields (timestamps, build IDs) must be zeroed before hashing
  unless the build explicitly opts in to non-deterministic hashes.
- Debug and integrity sections are excluded from hashing unless explicitly required.

Canonical module hash input (authoritative):
1. MODULE_METADATA with `build_id=""` and `source_hash=""`.
2. SYMBOL_TABLE bytes (as encoded in container).
3. CONSTANT_POOL bytes (as encoded in container).
4. BYTECODE_STREAM bytes.
5. optional: EXCEPTION_TABLE bytes if present.

Hash algorithm: SHA‑256 over concatenation of the above sections in the listed
order. The resulting 32‑byte digest is the canonical module hash.

## Validation Checklist
- All identifiers are resolved via SYMBOL_TABLE `string_id`.
- All catalog IDs are pooled as UUID constants.
- Constant pool order matches tag + payload sort rules.
- No forbidden pooling cases are present.
