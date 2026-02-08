# Canonicalization Rules (V3)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.

Date: 2026-02-07
Status: Normative (Authoritative for literal encoding and normalization)

This document defines canonicalization rules used by V3 SBLR literals and
storage encodings. Any value emitted to SBLR or persisted to storage MUST
conform to these rules.

## 1. Timezone Name Canonicalization

### 1.1 Source of Truth
- Timezone names MUST be validated against the `pg_timezone` system catalog
  defined in `TIMEZONE_SYSTEM_CATALOG.md`.
- The catalog MUST be populated from IANA tzdata via the loader tool.

### 1.2 Canonical Form
- Canonical form is the exact `pg_timezone.name` string.
- Canonical names are case-sensitive and must match IANA tzdata entries.
- Aliases are allowed only if present in `pg_timezone` and MUST be resolved to
  the canonical name stored in the catalog.

### 1.3 Literal Encoding Requirements
For `SBLR3_LITERAL_TIME_TZ` and `SBLR3_LITERAL_TIMESTAMP_TZ`:
- `tz_name` MAY be empty to indicate “offset only”.
- If `tz_name` is present, it MUST be a valid `pg_timezone.name`.
- If `tz_name` is present and an explicit `tz_offset_minutes` is also provided,
  the offset MUST be compatible with the timezone rules for the literal’s
  timestamp value. If ambiguity exists (DST transition), the literal MUST
  include an explicit offset that disambiguates.

## 2. TSVECTOR Canonical Form

### 2.1 Textual Representation
The canonical textual form is a sequence of lexemes sorted in ascending
lexicographic order, separated by single spaces:

```
lexeme[:pos[weight][,pos[weight]...]] lexeme[:pos[weight]...]
```

- `lexeme` is a single token, escaped if it contains whitespace or `:`.
- `pos` is a 1-based integer position.
- `weight` is one of `A`, `B`, `C`, `D`.

### 2.2 Canonicalization Algorithm
1. Tokenize the input tsvector literal into lexemes and position lists.
2. Normalize each lexeme using Unicode NFC and lowercasing.
3. Deduplicate identical lexemes by merging position lists.
4. Sort lexemes lexicographically (byte-wise UTF-8 order).
5. For each lexeme, sort positions ascending and remove duplicates.
6. Omit weights when they are the default weight `D`.
7. Serialize using the canonical textual representation above.

### 2.3 Empty Vector
- An empty vector is encoded as an empty string.

## 3. TSQUERY Canonical Form

### 3.1 Textual Representation
Canonical form is a normalized prefix notation with explicit operators:

- `&` (AND)
- `|` (OR)
- `!` (NOT)
- `<->` (FOLLOWED BY / PHRASE)

Operands are lexemes or parenthesized subexpressions.

### 3.2 Canonicalization Algorithm
1. Parse the input into a TSQUERY AST (using the dedicated TSQUERY parser).
2. Normalize each lexeme using Unicode NFC and lowercasing.
3. Flatten associative operators (`&`, `|`) and sort operands
   lexicographically to ensure deterministic ordering.
4. Remove redundant parentheses.
5. Serialize the AST with minimal parentheses to preserve precedence:
   - `!` highest
   - `<->` next
   - `&` next
   - `|` lowest

### 3.3 Empty Query
- An empty query is encoded as an empty string and evaluates to FALSE.

## 4. Reference Links
- Timezone catalog and loader: `TIMEZONE_SYSTEM_CATALOG.md`
- TSVECTOR/TSQUERY functions and parser: `BETA_SQL_STANDARD_COMPLIANCE_SPECIFICATION.md`

