# MySQL - Utilities

## LOCK TABLES / UNLOCK TABLES
Description: Parsed but treated as no-op in executor.

Syntax (actual):
```sql
LOCK TABLES <table> READ|WRITE [, ...]
UNLOCK TABLES
```
Example:
```sql
LOCK TABLES users WRITE;
UNLOCK TABLES;
```
Status: Partial (no-op).

## EXPLAIN / ANALYZE / COPY
Description: Not implemented in MySQL parser.

Status: Missing.
