# MySQL - Transaction Control

Spec refs:
- `ScratchBird/docs/specifications/parser/MYSQL_PARSER_SPECIFICATION.md`
- `ScratchBird/docs/audit/parsers/CRITICAL_FINDINGS.md` (isolation mapping)

## BEGIN / START TRANSACTION
Description: Starts a transaction; isolation mapped to MGA equivalents.

Syntax (actual):
```sql
START TRANSACTION [READ ONLY | READ WRITE]
BEGIN
```
Example:
```sql
START TRANSACTION READ WRITE;
```
Status: Implemented.

## COMMIT / ROLLBACK
Description: Ends a transaction.

Syntax (actual):
```sql
COMMIT
ROLLBACK
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

## SET AUTOCOMMIT
Description: MySQL autocommit toggle.

Syntax (actual):
```sql
SET AUTOCOMMIT = {0|1}
```
Example:
```sql
SET AUTOCOMMIT = 0;
```
Status: Implemented.
