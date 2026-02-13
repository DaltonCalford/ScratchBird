# PostgreSQL 18.1 Source Map (Authoritative References)

## SQL Grammar and Semantics
- `src/backend/parser/gram.y` — SQL grammar
- `src/backend/parser/scan.l` — lexer
- `src/backend/parser/*` — semantic analysis

## PL/pgSQL
- `src/pl/plpgsql/src/*`

## Wire Protocol (FE/BE 3.0)
- `doc/src/sgml/protocol.sgml`
- `src/backend/libpq/*`

## Data Types and Encoding
- `src/backend/utils/adt/*`
- `src/include/catalog/pg_type.h`

## Error Codes
- `src/backend/utils/errcodes.txt`
- `src/include/utils/errcodes.h`
