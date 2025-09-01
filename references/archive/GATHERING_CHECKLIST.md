# Reference Material Gathering Checklist

## Immediate Priority (For Your Current Work)

### 1. C++ Database Connection Specifications ✓ In Progress

#### PostgreSQL libpq
- [ ] Connection functions (PQconnectdb, PQconnectStart)
- [ ] Query execution (PQexec, PQexecParams, PQprepare)
- [ ] Result handling (PQresultStatus, PQgetvalue)
- [ ] Async operations (PQsendQuery, PQgetResult)
- [ ] COPY operations (PQputCopyData, PQgetCopyData)
- [ ] Error handling (PQerrorMessage, PQresultErrorField)

#### MySQL Connector/C
- [ ] Connection (mysql_real_connect)
- [ ] Query execution (mysql_query, mysql_real_query)
- [ ] Prepared statements (mysql_stmt_prepare, mysql_stmt_execute)
- [ ] Result sets (mysql_store_result, mysql_use_result)
- [ ] Error handling (mysql_error, mysql_errno)

#### MSSQL (ODBC/Native Client)
- [ ] SQLConnect/SQLDriverConnect
- [ ] SQLExecDirect/SQLExecute
- [ ] SQLFetch/SQLGetData
- [ ] SQLPrepare/SQLBindParameter
- [ ] Error handling (SQLGetDiagRec)

#### Firebird API
- [ ] isc_attach_database
- [ ] isc_dsql_execute
- [ ] isc_dsql_fetch
- [ ] isc_start_transaction
- [ ] Error handling (isc_sql_interprete)

### 2. Data Types Specification ✓ In Progress

#### Numeric Types
- [ ] Integer sizes and ranges per database
- [ ] Decimal/Numeric precision rules
- [ ] Float/Real precision differences
- [ ] Auto-increment/Serial/Identity behavior
- [ ] Unsigned type support

#### String Types
- [ ] Character set handling
- [ ] Collation rules
- [ ] VARCHAR vs TEXT differences
- [ ] Binary string types
- [ ] Unicode handling (UTF-8, UTF-16)

#### Temporal Types
- [ ] Date range limits
- [ ] Time precision (microseconds vs milliseconds)
- [ ] Timezone handling
- [ ] Interval types
- [ ] Timestamp behavior

#### Special Types
- [ ] UUID/GUID format and storage
- [ ] JSON/JSONB operations
- [ ] Array syntax and operations
- [ ] Boolean representation
- [ ] NULL handling differences

### 3. SQL Dialect Specifications ✓ In Progress

#### Core SQL Syntax
- [ ] SELECT statement variations
- [ ] JOIN syntax differences
- [ ] Subquery support levels
- [ ] CTE syntax
- [ ] Window function syntax

#### DDL Differences
- [ ] CREATE TABLE syntax
- [ ] ALTER TABLE capabilities
- [ ] Index creation syntax
- [ ] Constraint syntax
- [ ] View creation

#### DML Differences
- [ ] INSERT syntax variations (VALUES, SELECT, DEFAULT)
- [ ] UPDATE with JOIN syntax
- [ ] DELETE with JOIN syntax
- [ ] MERGE/UPSERT variations
- [ ] RETURNING/OUTPUT clause

#### Database-Specific
- [ ] MySQL: SHOW commands, HANDLER, REPLACE
- [ ] PostgreSQL: Dollar quoting, COPY, Arrays
- [ ] MSSQL: TOP, CROSS APPLY, PIVOT
- [ ] Firebird: EXECUTE BLOCK, EXECUTE STATEMENT

### 4. Core ScratchBird SQL Dialect ✓ In Progress

#### Decisions Needed
- [ ] Identifier quoting rules (backticks vs quotes)
- [ ] Case sensitivity default
- [ ] NULL sorting behavior
- [ ] String concatenation operator
- [ ] Boolean literal format
- [ ] Comment syntax

#### Core Features
- [ ] Transaction syntax
- [ ] Savepoint support
- [ ] Prepared statement syntax
- [ ] Cursor declaration
- [ ] Exception handling

## Additional Critical References

### 5. Wire Protocol Basics

#### Message Formats
- [ ] PostgreSQL message types (list of byte codes)
- [ ] MySQL packet structure
- [ ] TDS packet headers
- [ ] Firebird op codes

#### Authentication Flows
- [ ] PostgreSQL auth state machine
- [ ] MySQL handshake sequence
- [ ] MSSQL login sequence
- [ ] Firebird auth phases

### 6. System Catalogs

#### Minimal Set for Each Database
- [ ] How to list databases
- [ ] How to list tables
- [ ] How to list columns
- [ ] How to list indexes
- [ ] How to list constraints

### 7. Error Handling

#### Error Code Mappings
- [ ] SQLSTATE standard codes
- [ ] PostgreSQL error codes
- [ ] MySQL error numbers
- [ ] MSSQL error numbers
- [ ] Firebird error codes

#### Common Error Scenarios
- [ ] Constraint violations
- [ ] Type mismatches
- [ ] Syntax errors
- [ ] Connection errors
- [ ] Permission errors

### 8. Transaction Control

#### Isolation Level Syntax
- [ ] SET TRANSACTION syntax per database
- [ ] Default isolation levels
- [ ] Lock timeout handling
- [ ] Deadlock error codes

### 9. Character Sets and Collations

#### Default Encodings
- [ ] PostgreSQL: UTF8
- [ ] MySQL: utf8mb4
- [ ] MSSQL: UCS-2/UTF-16
- [ ] Firebird: UTF8

#### Collation Naming
- [ ] PostgreSQL collation names
- [ ] MySQL collation names
- [ ] MSSQL collation names
- [ ] Firebird collation names

### 10. Connection String Formats

#### Standard Formats
- [ ] PostgreSQL: postgresql://user:pass@host:port/db
- [ ] MySQL: mysql://user:pass@host:port/db
- [ ] MSSQL: Server=host;Database=db;User Id=user;Password=pass
- [ ] ODBC: DSN format
- [ ] JDBC: jdbc:subprotocol:subname

## File Organization Suggestion

```
/workspace/references/
├── README.md                    # Index of all references
├── cpp_apis/
│   ├── libpq_reference.md
│   ├── mysql_connector_c.md
│   ├── odbc_reference.md
│   └── firebird_api.md
├── data_types/
│   ├── type_comparison_matrix.md
│   ├── numeric_types.md
│   ├── string_types.md
│   ├── temporal_types.md
│   └── special_types.md
├── sql_dialects/
│   ├── ansi_sql_2016.md
│   ├── postgresql_dialect.md
│   ├── mysql_dialect.md
│   ├── mssql_dialect.md
│   └── firebird_dialect.md
├── scratchbird_sql/
│   ├── core_dialect.md
│   ├── design_decisions.md
│   └── compatibility_modes.md
├── wire_protocols/
│   ├── postgresql_protocol.md
│   ├── mysql_protocol.md
│   ├── tds_protocol.md
│   └── firebird_protocol.md
├── system_catalogs/
│   ├── catalog_comparison.md
│   └── minimal_catalog_api.md
├── error_codes/
│   ├── sqlstate_codes.md
│   └── error_mapping.md
└── examples/
    ├── connection_examples.cpp
    ├── type_conversion.cpp
    └── dialect_translation.cpp
```

## Quick Reference Sources

### Official Documentation
- PostgreSQL: https://www.postgresql.org/docs/current/
- MySQL: https://dev.mysql.com/doc/refman/8.0/en/
- MSSQL: https://docs.microsoft.com/en-us/sql/
- Firebird: https://firebirdsql.org/file/documentation/reference_manuals/

### Protocol Documentation
- PostgreSQL Protocol: https://www.postgresql.org/docs/current/protocol.html
- MySQL Protocol: https://dev.mysql.com/doc/internals/en/client-server-protocol.html
- TDS Protocol: https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-tds/
- Firebird Protocol: Source code in src/remote/protocol.h

### SQL Standards (Draft Versions)
- SQL:2016 Draft: Search for "SQL 2016 draft" or check university libraries
- SQL/PSM: Persistent Stored Modules standard
- SQL/JSON: JSON functionality standard

### Implementation References
- PostgreSQL Parser: src/backend/parser/gram.y
- MySQL Parser: sql/sql_yacc.yy
- Firebird Parser: src/dsql/parse.y

## Notes for Gathering

1. **Start with Connection**: Get basic connection working for each database first
2. **Focus on Core Types**: Don't worry about exotic types initially
3. **Document Differences**: Note where databases diverge from standards
4. **Test Everything**: Create small test programs for each feature
5. **Version Specific**: Note which version of each database you're targeting

## Legal and Licensing Notes

- PostgreSQL: BSD-like license (very permissive)
- MySQL: GPL (be careful with direct code copying)
- MSSQL: Proprietary (use only public documentation)
- Firebird: IPL/IDPL (permissive)
- ODBC: Public standard

Always prefer:
1. Public specifications over source code
2. Clean-room implementation over copying
3. Interface compatibility over code reuse