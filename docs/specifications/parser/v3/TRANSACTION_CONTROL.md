# V3 Parser: Transaction Control (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define parsing and SBLR emission for transaction control statements. The engine
never parses SQL; parsers emit SBLR only.

## Supported Statements

- `BEGIN` / `START TRANSACTION`
- `COMMIT` [WORK]
- `ROLLBACK` [WORK]
- `SAVEPOINT <name>`
- `ROLLBACK TO SAVEPOINT <name>`
- `RELEASE SAVEPOINT <name>`
- `SET TRANSACTION ...`

## Parsing Rules (Authoritative)

1. Parse the transaction control verb.
2. Parse any modifiers and characteristics in any order.
3. Emit the corresponding `SBLR3_TXN_*` opcode with payload defined in
   `SBLR_V3_OPCODE_PAYLOADS.md`.

## Transaction Characteristics

Supported characteristics for `BEGIN` / `SET TRANSACTION`:
- Isolation: `READ UNCOMMITTED`, `READ COMMITTED`, `REPEATABLE READ`,
  `SERIALIZABLE`, `SNAPSHOT`, `SNAPSHOT TABLE STABILITY`.
- Access mode: `READ ONLY`, `READ WRITE`.
- Read committed sub-variants: `READ CONSISTENCY`, `RECORD VERSION`,
  `NO RECORD VERSION`.
- Conflict handling: `ON CONFLICT COMMIT/ROLLBACK/ERROR/KEEP`.
- Deferrable: `DEFERRABLE` / `NOT DEFERRABLE`.

## Emission Rules

- `BEGIN` / `START TRANSACTION` → `SBLR3_TXN_BEGIN`.
- `COMMIT` → `SBLR3_TXN_COMMIT`.
- `ROLLBACK` → `SBLR3_TXN_ROLLBACK`.
- `SAVEPOINT` → `SBLR3_TXN_SAVEPOINT`.
- `ROLLBACK TO SAVEPOINT` → `SBLR3_TXN_ROLLBACK_TO_SAVEPOINT`.
- `RELEASE SAVEPOINT` → `SBLR3_TXN_RELEASE_SAVEPOINT`.
- `SET TRANSACTION` → `SBLR3_TXN_SET_OPTIONS`.

## Errors

- Invalid option or duplicate characteristic: `ERR_TXN_OPTION_INVALID`.
- Unsupported isolation for a dialect: `ERR_FEATURE_NOT_SUPPORTED`.

## Related Specs

- `07_TRANSACTION_AND_SESSION_CONTROL.md`
- `SBLR_V3_OPCODE_SPEC.md`
- `SBLR_V3_OPCODE_PAYLOADS.md`
- `PARSER_TO_SBLR_EMISSION_RULES.md`
