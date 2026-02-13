# MySQL 8.4 Source Map (Authoritative References)

## SQL Grammar and Semantics
- `source_copies/sql/sql_yacc.yy` — SQL grammar (Bison)
- `source_copies/sql/sql_lex.cc` — lexer and keyword handling
- `source_copies/sql/*` — semantic analysis and execution paths

## Wire Protocol (Classic Protocol)
- `source_copies/sql/protocol_classic.cc`
- `source_copies/sql/protocol_classic.h`
- `source_copies/sql/sql_class.cc` (THD/Protocol usage)
- `source_copies/include/mysql_com.h` (command codes, protocol constants, field types)
- `source_copies/include/mysql/` headers for client/server protocol types
- `source_copies/vio/` (I/O and packet framing)

## Authentication
- `source_copies/sql/auth/*`
- `source_copies/sql/password.c` and auth plugin sources under `source_copies/plugin/`

## Data Types and Encoding
- `source_copies/include/mysql_com.h` (field type codes)
- `source_copies/sql/field.*` (type storage/serialization)
- `source_copies/strings/*` (charset/collation)

## Docs
- `mysql_docs/Docs/` and `mysql_docs/man/`
