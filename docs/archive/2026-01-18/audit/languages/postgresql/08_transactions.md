# PostgreSQL - Transaction Control

Spec refs:
- `ScratchBird/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `ScratchBird/docs/audit/parsers/CRITICAL_FINDINGS.md` (isolation mapping)

## BEGIN / START TRANSACTION / SET TRANSACTION
Description: Starts or configures a transaction; isolation levels are mapped to
Firebird MGA equivalents.

Syntax (actual, abbreviated):
```sql
BEGIN [WORK | TRANSACTION]
START TRANSACTION <options>
SET TRANSACTION <options>
```
Example:
```sql
BEGIN ISOLATION LEVEL SERIALIZABLE;
```
Status: Implemented.
Spec delta: SERIALIZABLE/REPEATABLE READ map to MGA SNAPSHOT/STABILITY.

## COMMIT / ROLLBACK
Description: Ends a transaction.

Syntax (actual):
```sql
COMMIT [WORK]
ROLLBACK [WORK]
```
Example:
```sql
COMMIT;
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
