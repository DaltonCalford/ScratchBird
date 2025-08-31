# ScratchBird Universal Type System

## Overview

ScratchBird implements a universal type system that provides transparent mapping between all supported database engines. This document defines the canonical types and their mappings.

## ScratchBird Core Types

### Numeric Types

#### SB_SMALLINT
- **Internal Storage**: 2 bytes
- **Range**: -32,768 to 32,767
- **Mappings**:
  - PostgreSQL: `SMALLINT`
  - MySQL/MariaDB: `SMALLINT`
  - Firebird: `SMALLINT`
  - MSSQL: `SMALLINT`
  - JDBC: `Types.SMALLINT (5)`
  - ODBC: `SQL_SMALLINT`

#### SB_INTEGER
- **Internal Storage**: 4 bytes
- **Range**: -2,147,483,648 to 2,147,483,647
- **Mappings**:
  - PostgreSQL: `INTEGER`
  - MySQL/MariaDB: `INT`
  - Firebird: `INTEGER`
  - MSSQL: `INT`
  - JDBC: `Types.INTEGER (4)`
  - ODBC: `SQL_INTEGER`

#### SB_BIGINT
- **Internal Storage**: 8 bytes
- **Range**: -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
- **Mappings**:
  - PostgreSQL: `BIGINT`
  - MySQL/MariaDB: `BIGINT`
  - Firebird: `BIGINT`
  - MSSQL: `BIGINT`
  - JDBC: `Types.BIGINT (-5)`
  - ODBC: `SQL_BIGINT`

#### SB_INT128
- **Internal Storage**: 16 bytes
- **Range**: -170,141,183,460,469,231,731,687,303,715,884,105,728 to 170,141,183,460,469,231,731,687,303,715,884,105,727
- **Mappings**:
  - PostgreSQL: `NUMERIC(39,0)` (emulated)
  - MySQL/MariaDB: `DECIMAL(39,0)` (emulated)
  - Firebird: `INT128` (native in 4.0+)
  - MSSQL: `DECIMAL(39,0)` (emulated)

#### SB_DECIMAL(p,s)
- **Internal Storage**: Variable (4, 8, or 16 bytes)
- **Precision**: Up to 38 digits
- **Scale**: Up to precision
- **Mappings**:
  - PostgreSQL: `NUMERIC(p,s)`
  - MySQL/MariaDB: `DECIMAL(p,s)` (max 65,30)
  - Firebird: `DECIMAL(p,s)`
  - MSSQL: `DECIMAL(p,s)`

#### SB_FLOAT
- **Internal Storage**: 4 bytes (IEEE 754)
- **Mappings**:
  - PostgreSQL: `REAL`
  - MySQL/MariaDB: `FLOAT`
  - Firebird: `FLOAT`
  - MSSQL: `REAL`

#### SB_DOUBLE
- **Internal Storage**: 8 bytes (IEEE 754)
- **Mappings**:
  - PostgreSQL: `DOUBLE PRECISION`
  - MySQL/MariaDB: `DOUBLE`
  - Firebird: `DOUBLE PRECISION`
  - MSSQL: `FLOAT`

### Unsigned Types (Extended)

#### SB_UINT8
- **Internal Storage**: 1 byte
- **Range**: 0 to 255
- **Mappings**:
  - PostgreSQL: `SMALLINT` with check constraint
  - MySQL/MariaDB: `TINYINT UNSIGNED`
  - Firebird: `SMALLINT` with check constraint
  - MSSQL: `TINYINT`

#### SB_UINT16
- **Internal Storage**: 2 bytes
- **Range**: 0 to 65,535
- **Mappings**:
  - PostgreSQL: `INTEGER` with check constraint
  - MySQL/MariaDB: `SMALLINT UNSIGNED`
  - Firebird: `INTEGER` with check constraint
  - MSSQL: `INT` with check constraint

#### SB_UINT32
- **Internal Storage**: 4 bytes
- **Range**: 0 to 4,294,967,295
- **Mappings**:
  - PostgreSQL: `BIGINT` with check constraint
  - MySQL/MariaDB: `INT UNSIGNED`
  - Firebird: `BIGINT` with check constraint
  - MSSQL: `BIGINT` with check constraint

#### SB_UINT64
- **Internal Storage**: 8 bytes
- **Range**: 0 to 18,446,744,073,709,551,615
- **Mappings**:
  - PostgreSQL: `NUMERIC(20,0)` with check constraint
  - MySQL/MariaDB: `BIGINT UNSIGNED`
  - Firebird: `NUMERIC(20,0)` with check constraint
  - MSSQL: `NUMERIC(20,0)` with check constraint

### Character Types

#### SB_CHAR(n)
- **Internal Storage**: Fixed n bytes
- **Max Length**: 32,767 bytes
- **Mappings**:
  - PostgreSQL: `CHAR(n)`
  - MySQL/MariaDB: `CHAR(n)` (max 255)
  - Firebird: `CHAR(n)`
  - MSSQL: `CHAR(n)` (max 8000)

#### SB_VARCHAR(n)
- **Internal Storage**: Variable up to n bytes
- **Max Length**: 65,535 bytes
- **Mappings**:
  - PostgreSQL: `VARCHAR(n)`
  - MySQL/MariaDB: `VARCHAR(n)` (max 65,535)
  - Firebird: `VARCHAR(n)` (max 32,765)
  - MSSQL: `VARCHAR(n)` (max 8000) or `VARCHAR(MAX)`

#### SB_TEXT
- **Internal Storage**: Variable, up to 2GB
- **Mappings**:
  - PostgreSQL: `TEXT`
  - MySQL/MariaDB: `LONGTEXT`
  - Firebird: `BLOB SUB_TYPE TEXT`
  - MSSQL: `VARCHAR(MAX)`

### Binary Types

#### SB_BINARY(n)
- **Internal Storage**: Fixed n bytes
- **Mappings**:
  - PostgreSQL: `BYTEA`
  - MySQL/MariaDB: `BINARY(n)`
  - Firebird: `CHAR(n) CHARACTER SET OCTETS`
  - MSSQL: `BINARY(n)`

#### SB_VARBINARY(n)
- **Internal Storage**: Variable up to n bytes
- **Mappings**:
  - PostgreSQL: `BYTEA`
  - MySQL/MariaDB: `VARBINARY(n)`
  - Firebird: `VARCHAR(n) CHARACTER SET OCTETS`
  - MSSQL: `VARBINARY(n)`

#### SB_BLOB
- **Internal Storage**: Up to 4GB
- **Mappings**:
  - PostgreSQL: `BYTEA` or `LARGE OBJECT`
  - MySQL/MariaDB: `LONGBLOB`
  - Firebird: `BLOB SUB_TYPE BINARY`
  - MSSQL: `VARBINARY(MAX)`

### Date/Time Types

#### SB_DATE
- **Internal Storage**: 4 bytes
- **Range**: 0001-01-01 to 9999-12-31
- **Mappings**:
  - PostgreSQL: `DATE`
  - MySQL/MariaDB: `DATE`
  - Firebird: `DATE`
  - MSSQL: `DATE`

#### SB_TIME
- **Internal Storage**: 4-8 bytes
- **Precision**: Microseconds
- **Mappings**:
  - PostgreSQL: `TIME`
  - MySQL/MariaDB: `TIME(6)`
  - Firebird: `TIME`
  - MSSQL: `TIME(7)`

#### SB_TIMESTAMP
- **Internal Storage**: 8 bytes
- **Precision**: Microseconds
- **Mappings**:
  - PostgreSQL: `TIMESTAMP`
  - MySQL/MariaDB: `TIMESTAMP(6)`
  - Firebird: `TIMESTAMP`
  - MSSQL: `DATETIME2(6)`

#### SB_TIMESTAMPTZ
- **Internal Storage**: 8 bytes + timezone
- **Mappings**:
  - PostgreSQL: `TIMESTAMP WITH TIME ZONE`
  - MySQL/MariaDB: `TIMESTAMP(6)` + session timezone
  - Firebird: `TIMESTAMP WITH TIME ZONE` (v4.0+)
  - MSSQL: `DATETIMEOFFSET`

### Special Types

#### SB_BOOLEAN
- **Internal Storage**: 1 byte
- **Mappings**:
  - PostgreSQL: `BOOLEAN`
  - MySQL/MariaDB: `BOOLEAN` (alias for TINYINT(1))
  - Firebird: `BOOLEAN` (v3.0+)
  - MSSQL: `BIT`

#### SB_UUID
- **Internal Storage**: 16 bytes
- **Mappings**:
  - PostgreSQL: `UUID`
  - MySQL/MariaDB: `BINARY(16)` or `CHAR(36)`
  - Firebird: `CHAR(16) CHARACTER SET OCTETS`
  - MSSQL: `UNIQUEIDENTIFIER`

#### SB_JSON
- **Internal Storage**: Variable
- **Mappings**:
  - PostgreSQL: `JSON` or `JSONB`
  - MySQL/MariaDB: `JSON`
  - Firebird: `BLOB SUB_TYPE TEXT` with JSON validation
  - MSSQL: `NVARCHAR(MAX)` with JSON functions

#### SB_XML
- **Internal Storage**: Variable
- **Mappings**:
  - PostgreSQL: `XML`
  - MySQL/MariaDB: `TEXT` with XML functions
  - Firebird: `BLOB SUB_TYPE TEXT` with XML validation
  - MSSQL: `XML`

#### SB_ARRAY(type)
- **Internal Storage**: Variable
- **Mappings**:
  - PostgreSQL: Native array type
  - MySQL/MariaDB: `JSON` array
  - Firebird: Native array type
  - MSSQL: `JSON` array or table type

## Type Conversion Matrix

### Numeric Conversions

| From | To | Safety | Notes |
|------|-----|--------|-------|
| SB_SMALLINT | SB_INTEGER | Safe | Widening |
| SB_SMALLINT | SB_BIGINT | Safe | Widening |
| SB_INTEGER | SB_SMALLINT | Check | May overflow |
| SB_INTEGER | SB_BIGINT | Safe | Widening |
| SB_BIGINT | SB_INTEGER | Check | May overflow |
| SB_BIGINT | SB_INT128 | Safe | Widening |
| SB_FLOAT | SB_DOUBLE | Safe | Widening |
| SB_DOUBLE | SB_FLOAT | Check | Loss of precision |
| SB_INTEGER | SB_DECIMAL | Safe | No loss |
| SB_DECIMAL | SB_INTEGER | Check | May truncate |

### String Conversions

| From | To | Safety | Notes |
|------|-----|--------|-------|
| SB_CHAR | SB_VARCHAR | Safe | No padding |
| SB_VARCHAR | SB_CHAR | Check | May truncate |
| SB_VARCHAR | SB_TEXT | Safe | Widening |
| SB_TEXT | SB_VARCHAR | Check | May truncate |

## Conflict Resolution Rules

### Precision Conflicts
When databases have different precision limits:
1. Use the minimum common precision
2. Warn if precision loss possible
3. Allow override with explicit cast

### Type Unavailability
When target database lacks a type:
1. Use documented mapping
2. Add constraints to emulate behavior
3. Warn about semantic differences

### Collation Conflicts
1. Store original collation in metadata
2. Use closest equivalent in target
3. Warn about sort order changes

## Implementation Guidelines

### Type Detection
```c
enum SBType detect_type(int protocol_type, int db_type) {
    switch(db_type) {
        case POSTGRES_INT4:
        case MYSQL_INT:
        case FIREBIRD_INTEGER:
        case MSSQL_INT:
            return SB_INTEGER;
        // ... more mappings
    }
}
```

### Type Conversion
```c
bool convert_type(SBType from, SBType to, void* data, size_t* size) {
    if (is_widening_conversion(from, to)) {
        return widen_type(from, to, data, size);
    }
    if (requires_check(from, to)) {
        return checked_convert(from, to, data, size);
    }
    return false; // Incompatible
}
```

## Protocol-Specific Handling

### PostgreSQL
- Use binary format for efficiency
- Handle TOAST for large values
- Map arrays natively

### MySQL
- Use prepared statement binary protocol
- Handle length-encoded integers
- Convert UNSIGNED types carefully

### Firebird
- Use XDR encoding
- Handle BLOB subtypes
- Map arrays natively

### MSSQL
- Use TDS type tokens
- Handle NCHAR/NVARCHAR as UTF-16
- Map CLR types appropriately

## Testing Requirements

1. **Round-trip testing**: Value → ScratchBird → Database → ScratchBird → Value
2. **Boundary testing**: Min/max values for each type
3. **Precision testing**: Decimal precision preservation
4. **Null testing**: NULL handling across all types
5. **Overflow testing**: Narrowing conversions
6. **Encoding testing**: Character set conversions

## Future Considerations

1. **User-Defined Types**: DOMAIN support
2. **Composite Types**: Record/struct types
3. **Spatial Types**: Geometry/geography
4. **Temporal Extensions**: Period types
5. **Vector Types**: For ML workloads