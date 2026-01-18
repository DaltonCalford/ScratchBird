[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# FirebirdSQL - Transaction Control

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

Spec refs:
- `ScratchBird/docs/specifications/sblr/FIREBIRD_TRANSACTION_MODEL_SPEC.md`
- `ScratchBird/docs/specifications/reference/firebird/FirebirdReferenceDocument.md`

## SET TRANSACTION / START TRANSACTION
Description: Firebird MGA transaction options (isolation, read consistency,
wait/no-wait, lock timeout, reserving).

Syntax (actual, abbreviated):
```sql
SET TRANSACTION <options>
START TRANSACTION <options>
```
Example:
```sql
SET TRANSACTION READ COMMITTED RECORD VERSION WAIT LOCK TIMEOUT 5000;
```
Status: Implemented.

## COMMIT / ROLLBACK
Description: Ends a transaction with optional RETAINING.

Syntax (actual):
```sql
COMMIT [RETAINING]
ROLLBACK [RETAINING]
```
Example:
```sql
COMMIT RETAINING;
```
Status: Implemented.

## SAVEPOINT / RELEASE / ROLLBACK TO SAVEPOINT
Description: Savepoint control.

Syntax (actual):
```sql
SAVEPOINT <name>
RELEASE SAVEPOINT <name>
ROLLBACK TO SAVEPOINT <name>
```
Example:
```sql
SAVEPOINT step1;
ROLLBACK TO SAVEPOINT step1;
```
Status: Implemented.
