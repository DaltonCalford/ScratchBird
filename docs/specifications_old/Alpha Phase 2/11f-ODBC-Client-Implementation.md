# ODBC Client Implementation

ODBC connector for Remote Database UDR.

## 1. Scope
- ODBC 3.8 API
- Embedded driver manager shipped in UDR bundle
- Drivers bundled in the UDR (no host install required)

## 2. Architecture
- UDR loads embedded driver manager (e.g., unixODBC compatible interface)
- Driver is selected by DSN or DRIVER= in connection string
- ODBC handles are managed per connection:
  - SQLHENV (environment)
  - SQLHDBC (connection)
  - SQLHSTMT (statement)

## 3. Connection String
Example options:
```
DRIVER={PostgreSQL};SERVER=host;PORT=5432;DATABASE=db;UID=user;PWD=pass;SSLmode=require
```

## 4. Execution Model
- Use SQLPrepare/SQLBindParameter/SQLExecute for prepared statements
- Use SQLExecDirect for ad-hoc SQL
- Result sets read via SQLFetch / SQLGetData
- Fetch size via SQL_ATTR_ROW_ARRAY_SIZE

## 5. Transactions
- Autocommit controlled by SQL_ATTR_AUTOCOMMIT
- Explicit transactions via SQLEndTran

## 6. Error Mapping
- Use SQLGetDiagRec for SQLSTATE and native error
- Map to ScratchBird error catalog

## 7. Type Mapping
- SQL type info via SQLDescribeCol and SQLGetTypeInfo
- Map ODBC types to ScratchBird types (int, numeric, char, binary, date/time)

## 8. Required Capabilities
- prepared statements
- parameter binding
- paging (row array fetch)
- cancellation (SQLCancel/SQLCancelHandle)
- schema introspection (SQLTables, SQLColumns, SQLStatistics)

## 9. References
- https://learn.microsoft.com/en-us/sql/odbc/reference/odbc-programmer-s-reference

