# MySQL 8.4 Responses & Data Types — Draft 1

## Authoritative Sources
- `source_copies/include/mysql_com.h`
- `source_copies/include/field_types.h`
- `source_copies/sql/protocol_classic.*`
- `source_copies/sql/field.*`
- `source_copies/strings/*` (charset/collation)

## 1. OK/ERR/EOF Packets
- OK, ERR, and EOF packet structures are defined in `protocol_classic.*` and `mysql_com.h`.
- EOF deprecation is controlled by `CLIENT_DEPRECATE_EOF` capability.

## 2. Resultsets
- Resultset packet sequence: column count, column definitions, EOF/OK, rows, EOF/OK.
- Binary protocol resultsets are used for prepared statements.

## 3. Data Type Codes
- Column type codes are defined in `include/field_types.h` (`enum_field_types`).

## 4. Encoding Rules
- Text protocol row values are length-encoded strings.
- Binary protocol row values are encoded per type in `protocol_classic.*` and `field.*`.

## 5. Compliance Rule
All type encodings and edge cases must match source copies.
