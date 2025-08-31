# Reference Materials Needed for ScratchBird

## 1. Wire Protocol Specifications

### PostgreSQL
- Frontend/Backend Protocol v3.0
- Startup message format
- Authentication methods (MD5, SCRAM-SHA-256)
- Extended Query Protocol
- COPY protocol
- Replication protocol
- Error message format

### MySQL
- MySQL Client/Server Protocol
- Handshake process (v10)
- Authentication plugins
- COM_* commands
- Prepared statement protocol
- Result set format
- Replication protocol

### MSSQL (TDS - Tabular Data Stream)
- TDS 7.4 specification
- Pre-login handshake
- Login7 packet structure
- RPC protocol
- Bulk copy protocol
- MARS (Multiple Active Result Sets)

### MariaDB
- MariaDB/MySQL protocol differences
- MariaDB-specific authentication
- Extended type information

### Firebird
- Firebird wire protocol v13+
- Op codes
- Authentication architecture
- Events protocol
- Service API protocol

## 2. System Catalogs / Information Schema

### PostgreSQL System Catalogs
```
pg_catalog schema:
- pg_class (tables, indexes, sequences)
- pg_attribute (columns)
- pg_index (index definitions)
- pg_constraint (constraints)
- pg_proc (functions)
- pg_type (data types)
- pg_namespace (schemas)
- pg_database (databases)
- pg_tablespace (tablespaces)
- pg_roles (users/roles)
```

### MySQL Information Schema
```
information_schema:
- TABLES
- COLUMNS
- KEY_COLUMN_USAGE
- REFERENTIAL_CONSTRAINTS
- ROUTINES
- VIEWS
- TRIGGERS
- EVENTS
- PARTITIONS
```

### MSSQL System Views
```
sys schema:
- sys.tables
- sys.columns
- sys.indexes
- sys.foreign_keys
- sys.procedures
- sys.views
- sys.triggers
- sys.databases
- sys.filegroups
```

## 3. SQL Dialect Specifications

### ANSI SQL Standards
- SQL:2016 (or latest available)
- SQL/PSM (Persistent Stored Modules)
- SQL/JSON
- SQL/XML
- SQL/MDA (Multi-Dimensional Arrays)

### PostgreSQL Extensions
- Dollar quoting
- RETURNING clause
- ON CONFLICT
- Array syntax
- Range types
- LATERAL joins
- Window functions
- CTEs with RECURSIVE
- JSONB operators

### MySQL Extensions
- REPLACE INTO
- INSERT IGNORE
- ON DUPLICATE KEY UPDATE
- LIMIT/OFFSET syntax
- Backtick identifiers
- SHOW commands
- HANDLER statements
- GROUP BY behavior

### MSSQL Extensions
- TOP clause
- MERGE statement
- OUTPUT clause
- Cross apply / Outer apply
- Common Table Expressions
- Temporal tables
- Columnstore indexes

### Firebird Extensions
- EXECUTE BLOCK
- EXECUTE STATEMENT
- RETURNING clause
- MERGE statement
- Window functions
- Common Table Expressions
- Recursive CTEs

## 4. Data Type Specifications

### Numeric Types Mapping
```
Standard     PostgreSQL    MySQL        MSSQL        Firebird
SMALLINT     smallint     SMALLINT     smallint     SMALLINT
INTEGER      integer      INT          int          INTEGER
BIGINT       bigint       BIGINT       bigint       BIGINT
DECIMAL(p,s) decimal      DECIMAL      decimal      DECIMAL
REAL         real         FLOAT        real         FLOAT
DOUBLE       double       DOUBLE       float        DOUBLE PRECISION
```

### String Types Mapping
```
Standard     PostgreSQL    MySQL        MSSQL        Firebird
CHAR(n)      char         CHAR         char         CHAR
VARCHAR(n)   varchar      VARCHAR      varchar      VARCHAR
TEXT         text         TEXT         varchar(max) BLOB SUB_TYPE TEXT
BLOB         bytea        BLOB         varbinary    BLOB
```

### Special Types
- UUID handling across databases
- JSON/JSONB types
- XML types
- Array types
- Geographic/Spatial types
- Network address types
- Money types
- Bit strings

## 5. Function and Operator Mappings

### String Functions
```
Function     PostgreSQL    MySQL          MSSQL         Firebird
Length       length()      LENGTH()       LEN()         CHAR_LENGTH()
Substring    substring()   SUBSTRING()    SUBSTRING()   SUBSTRING()
Concatenate  ||           CONCAT()       +             ||
Upper        upper()      UPPER()        UPPER()       UPPER()
Trim         trim()       TRIM()         TRIM()        TRIM()
```

### Date/Time Functions
```
Function     PostgreSQL    MySQL          MSSQL         Firebird
Current      now()        NOW()          GETDATE()     CURRENT_TIMESTAMP
Extract      extract()    EXTRACT()      DATEPART()    EXTRACT()
Add          +interval    DATE_ADD()     DATEADD()     DATEADD()
Format       to_char()    DATE_FORMAT()  FORMAT()      [custom]
```

### Aggregate Functions
- Standard: COUNT, SUM, AVG, MIN, MAX
- Statistical: STDDEV, VARIANCE
- Advanced: PERCENTILE, MEDIAN
- String aggregates: STRING_AGG, GROUP_CONCAT
- JSON aggregates: JSON_AGG, JSON_OBJECTAGG

## 6. Transaction and Isolation Specifications

### Isolation Levels
- READ UNCOMMITTED behavior per database
- READ COMMITTED differences
- REPEATABLE READ implementation
- SERIALIZABLE strategies
- Database-specific levels (e.g., PostgreSQL's SSI)

### Lock Types
- Row-level locks
- Page-level locks
- Table-level locks
- Advisory locks
- Deadlock detection strategies

### Transaction Commands
- BEGIN/START TRANSACTION variations
- COMMIT/ROLLBACK syntax
- SAVEPOINT support
- Two-phase commit
- Distributed transactions

## 7. Connection and Authentication

### Authentication Methods
- Password (plain, MD5, SHA256)
- SCRAM-SHA-256
- Kerberos/GSSAPI
- LDAP
- Certificate-based
- PAM
- Windows authentication (SSPI)

### Connection Parameters
- Standard parameters across databases
- SSL/TLS configuration
- Connection pooling parameters
- Timeout settings
- Character set/collation

## 8. Error Codes and Messages

### SQLSTATE Codes
- Standard 5-character codes
- Database-specific extensions
- Mapping between databases

### Error Message Formats
- PostgreSQL error fields
- MySQL error format
- MSSQL error format
- Firebird error format

## 9. Performance and Optimization

### Query Plan Formats
- PostgreSQL EXPLAIN
- MySQL EXPLAIN
- MSSQL execution plans
- Firebird PLAN

### Statistics and Metadata
- Table statistics format
- Index statistics
- Query statistics
- Performance counters

### Optimization Hints
- PostgreSQL planner hints
- MySQL optimizer hints
- MSSQL query hints
- Firebird PLAN clause

## 10. Replication and Clustering

### Replication Methods
- Logical replication protocols
- Physical/streaming replication
- Statement-based replication
- Row-based replication

### Cluster Coordination
- Consensus protocols (Raft, Paxos)
- Distributed transaction protocols
- Clock synchronization
- Split-brain prevention

## 11. Backup and Recovery

### Backup Formats
- SQL dump formats
- Binary backup formats
- Incremental backup strategies
- Point-in-time recovery

### Recovery Procedures
- Crash recovery
- Media recovery
- Point-in-time recovery
- Partial restore

## 12. Client Library APIs

### C/C++ APIs
- libpq (PostgreSQL)
- MySQL Connector/C
- ODBC specification
- Firebird API

### Connection String Formats
- PostgreSQL connection URIs
- MySQL connection strings
- MSSQL connection strings
- Standard ODBC/JDBC URLs

## 13. Testing Resources

### SQL Compliance Tests
- NIST SQL Test Suite
- SQL Logic Test
- Database test suites from each vendor

### Compatibility Tests
- Application test suites (WordPress, Django, Rails)
- ORM test suites (Hibernate, SQLAlchemy)
- BI tool compatibility

## 14. Standards Documents

### ISO/IEC Standards
- ISO/IEC 9075 (SQL standard)
- ISO/IEC 13249 (SQL/MM)
- ISO 8601 (Date/time formats)

### Internet Standards
- RFC 7159 (JSON)
- RFC 3986 (URI syntax)
- RFC 5802 (SCRAM)

## 15. Implementation References

### Open Source Databases
- PostgreSQL source (parser, executor)
- MySQL source (protocol, optimizer)
- MariaDB source (differences from MySQL)
- Firebird source (MGA implementation)
- SQLite source (simple reference)

### Academic Papers
- ARIES (WAL recovery)
- Multi-Version Concurrency Control
- Query optimization papers
- Distributed database papers

## Directory Structure for References

```
/workspace/references/
├── wire_protocols/
│   ├── postgresql/
│   ├── mysql/
│   ├── mssql_tds/
│   ├── firebird/
│   └── mariadb/
├── sql_dialects/
│   ├── ansi_sql/
│   ├── postgresql/
│   ├── mysql/
│   ├── mssql/
│   └── firebird/
├── system_catalogs/
│   ├── pg_catalog/
│   ├── information_schema/
│   ├── sys_schema/
│   └── firebird_system/
├── data_types/
│   ├── type_mappings.md
│   ├── numeric_types/
│   ├── string_types/
│   ├── temporal_types/
│   └── special_types/
├── functions/
│   ├── string_functions/
│   ├── date_functions/
│   ├── math_functions/
│   └── aggregate_functions/
├── error_codes/
│   ├── sqlstate.md
│   └── vendor_specific/
├── authentication/
│   ├── methods/
│   └── protocols/
├── standards/
│   ├── iso_sql/
│   └── rfcs/
└── test_suites/
    ├── compliance/
    └── compatibility/
```

## Tools to Create/Acquire

### Protocol Analyzers
- Wireshark dissectors for each protocol
- Protocol test harnesses
- Connection replay tools

### Parser Generators
- ANTLR grammars for SQL dialects
- Flex/Bison specifications
- Parser test suites

### Documentation Generators
- Tools to extract docs from source
- API documentation generators
- Compatibility matrix generators

## Priority Order for Gathering

1. **Critical** (Needed for Phases 1-10):
   - Firebird MGA documentation
   - Basic SQL grammar
   - Core data types
   - Transaction semantics

2. **Important** (Needed for Phases 11-20):
   - Wire protocol specifications
   - System catalog structures
   - Authentication methods
   - Error code mappings

3. **Advanced** (Needed for Phases 21+):
   - Replication protocols
   - Clustering specifications
   - Performance optimization guides
   - Compatibility test suites

## Sources for Materials

- PostgreSQL Documentation: postgresql.org/docs
- MySQL Documentation: dev.mysql.com/doc
- MSSQL Documentation: docs.microsoft.com/sql
- Firebird Documentation: firebirdsql.org/file/documentation
- SQL Standards: ISO store or draft versions
- Academic Papers: Google Scholar, ACM Digital Library
- Protocol Analysis: Wireshark wiki, vendor source code

## Legal Considerations

- Check licenses for any copied documentation
- Prefer public specifications over proprietary
- Create clean-room implementations
- Document sources for all materials