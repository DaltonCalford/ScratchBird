# PostgreSQL 18.1 Responses & Data Types — Draft 1

## Authoritative Sources
- `doc/src/sgml/protocol.sgml`
- `src/backend/utils/adt/*`
- `src/include/catalog/pg_type.h`
- `src/backend/libpq/*`

## 1. Row Data Formats
- Text and binary formats as specified in protocol docs.

## 2. Type OIDs and Encodings
- Type OIDs and definitions are in `pg_type.h` and catalog sources.
- Encoding logic is in `utils/adt`.

## 3. Error Responses
- ErrorResponse fields and SQLSTATE codes are defined in protocol docs and `errcodes.txt`.

## 4. Compliance Rule
All response formatting must match source copies.
