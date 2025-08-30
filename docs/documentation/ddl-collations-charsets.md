### DDL: Collations and Character Sets

What it is
- Text comparison and encoding definitions that affect ordering and storage.

Why it matters
- Correct collation/charset ensures proper sorting and internationalization; impacts index behavior.

How to use it
- Define collations/charsets when defaults are insufficient; apply via column definitions or domains.

Parsing for COLLATION and CHARACTER SET objects is present in `src/engine/parser_ddl.cpp`.
- Collation: `ast.ddlCollation` captures `name`, `based_on`, `from_external`
- Charset: `ast.ddlCharset` captures `name`, raw `attributes`
See also
- [Lexical](./sql-lexical.md) · [Tables](./ddl-tables.md)
Examples:
```sql
CREATE COLLATION mycoll FROM EXTERNAL 'icu:en_US';
CREATE CHARACTER SET mycs; -- attributes captured raw
```

