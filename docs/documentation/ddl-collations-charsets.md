### DDL: Collations and Character Sets

Parsing for COLLATION and CHARACTER SET objects is present in `src/engine/parser_ddl.cpp`.
- Collation: `ast.ddlCollation` captures `name`, `based_on`, `from_external`
- Charset: `ast.ddlCharset` captures `name`, raw `attributes`

Examples:
```sql
CREATE COLLATION mycoll FROM EXTERNAL 'icu:en_US';
CREATE CHARACTER SET mycs; -- attributes captured raw
```

