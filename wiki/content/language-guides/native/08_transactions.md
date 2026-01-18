[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# Native (V2) - Transaction Control

Spec refs:
- `ScratchBird/docs/specifications/transaction/TRANSACTION_MAIN.md`
- `ScratchBird/docs/specifications/sblr/FIREBIRD_TRANSACTION_MODEL_SPEC.md`

## BEGIN / START TRANSACTION / SET TRANSACTION
Description: Starts or configures a transaction with isolation and behavior
options (Firebird MGA semantics).

Syntax (actual, abbreviated):
```sql
BEGIN [WORK | TRANSACTION]
START TRANSACTION <options>
SET TRANSACTION <options>

-- Options include:
-- READ ONLY | READ WRITE
-- ISOLATION LEVEL {READ COMMITTED | SNAPSHOT | SNAPSHOT TABLE STABILITY}
-- READ COMMITTED {RECORD VERSION | NO RECORD VERSION | READ CONSISTENCY}
-- WAIT | NO WAIT
-- LOCK TIMEOUT <ms>
-- RESERVING <table_list>
-- AUTOCOMMIT {ON|OFF}
-- ON CONFLICT {COMMIT|ROLLBACK|ERROR|KEEP}
```
Example:
```sql
SET TRANSACTION READ COMMITTED RECORD VERSION WAIT LOCK TIMEOUT 5000;
```
Status: Implemented.
Spec delta: None known; confirm lock timeout semantics vs spec.

## COMMIT / ROLLBACK
Description: Ends a transaction, optionally retaining context or chaining.

Syntax (actual):
```sql
COMMIT [WORK] [AND {CHAIN | NO CHAIN}] [RETAINING]
ROLLBACK [WORK] [AND {CHAIN | NO CHAIN}] [RETAINING]
```
Example:
```sql
COMMIT RETAINING;
```
Status: Implemented.
Spec delta: None known.

## SAVEPOINT / RELEASE / ROLLBACK TO SAVEPOINT
Description: Manages savepoints within a transaction.

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
Spec delta: None known.

## PREPARE TRANSACTION
Description: Two-phase commit prepare step.

Syntax (actual):
```sql
PREPARE TRANSACTION '<gid>'
```
Example:
```sql
PREPARE TRANSACTION 'txn_42';
```
Status: Implemented.
Spec delta: None known.

## SET AUTOCOMMIT
Description: Enables/disables autocommit mode.

Syntax (actual):
```sql
SET AUTOCOMMIT {ON|OFF|1|0} [ON CONFLICT <action>]
```
Example:
```sql
SET AUTOCOMMIT OFF;
```
Status: Implemented.
Spec delta: None known.
