# Additional Reference Materials for ScratchBird

## Technical Implementation References

### 1. Memory Management
- Buffer pool management strategies
- Page replacement algorithms (LRU, Clock, ARC)
- Memory allocation patterns for databases
- Lock-free data structures
- Memory-mapped I/O strategies
- NUMA-aware memory allocation

### 2. Query Processing
- Volcano-style iterators
- Vectorized execution
- Compilation to native code (JIT)
- Adaptive query execution
- Push vs pull execution models
- Morsel-driven parallelism

### 3. Storage Formats
- Row-oriented vs columnar storage
- Compression algorithms (Snappy, LZ4, ZSTD)
- Page formats and layouts
- Large object handling
- Delta encoding
- Dictionary encoding

### 4. Concurrency Control
- Lock compatibility matrices
- Deadlock detection algorithms
- Optimistic concurrency control
- Multi-version timestamp ordering
- Serializable Snapshot Isolation (SSI)
- Read-write lock implementations

## Standards and Specifications

### 5. Network and Security
- TLS 1.3 specification (RFC 8446)
- OAuth 2.0 for database auth (RFC 6749)
- SASL mechanisms (RFC 4422)
- Kerberos/GSSAPI (RFC 4121)
- SCRAM-SHA-256 (RFC 7677)
- Certificate-based authentication

### 6. Data Exchange Formats
- Apache Arrow for columnar data
- Protocol Buffers for serialization
- MessagePack for efficient encoding
- Apache Parquet for storage
- Apache Avro for schema evolution
- BSON (Binary JSON)

### 7. Distributed Systems
- CAP theorem implications
- Consensus protocols (Raft, Paxos)
- Vector clocks
- Gossip protocols
- Consistent hashing
- Two-phase commit protocol

## Testing and Validation

### 8. Benchmarks
- TPC-C (OLTP) - Transaction processing
- TPC-H (OLAP) - Decision support
- TPC-DS - Decision support with complex queries
- YCSB (Yahoo! Cloud Serving Benchmark)
- Sysbench - Multi-threaded benchmark
- HammerDB - Database load testing

### 9. Fuzzing and Testing
- SQLsmith grammar for query generation
- AFL (American Fuzzy Lop) patterns
- Chaos engineering principles
- Property-based testing with QuickCheck
- Mutation testing
- Jepsen for distributed systems testing

## Migration and Compatibility

### 10. Schema Migration Tools
- Liquibase XML format
- Flyway conventions
- Rails migrations (ActiveRecord)
- Alembic (SQLAlchemy)
- Django migrations
- Doctrine migrations (PHP)

### 11. ORM Compatibility Requirements
- Hibernate Query Language (HQL)
- SQLAlchemy query API
- Django ORM queries
- ActiveRecord patterns
- Entity Framework LINQ
- Sequelize (Node.js)

## Monitoring and Observability

### 12. Metrics and Tracing
- OpenTelemetry specification
- Prometheus metrics format
- StatsD protocol
- OpenTracing standard
- Jaeger tracing format
- Zipkin compatibility

## Internationalization

### 13. Locale and Collation
- ICU library documentation
- Unicode Collation Algorithm (UCA)
- Time zone database (tzdata/IANA)
- Currency and number formats (CLDR)
- Bidirectional text handling
- Case folding rules

## Database-Specific Internals

### 14. PostgreSQL Internals
- TOAST (The Oversized-Attribute Storage Technique)
- HOT (Heap-Only Tuples) updates
- Visibility map structure
- Free space map (FSM)
- Write-Ahead Log Sender (WAL sender)
- Background writer process
- Autovacuum daemon
- Cost-based vacuum delay

### 15. MySQL/InnoDB Internals
- Adaptive hash index
- Change buffer (insert buffer)
- Doublewrite buffer
- Redo log format
- Binary log format
- Group commit
- Purge thread
- Read-ahead algorithms

### 16. Firebird Specifics
- ODS (On-Disk Structure) versions
- Careful write protocol
- Shadow databases
- External tables
- Monitoring tables (MON$)
- Garbage collection (sweep)
- Database triggers
- External functions (UDF/UDR)

### 17. MSSQL/SQL Server Internals
- Page structure and types
- GAM/SGAM/IAM pages
- PFS (Page Free Space) pages
- Extents and allocation units
- Transaction log architecture
- Checkpoint process
- Lazy writer
- Read-ahead manager

## Documentation to Create

### 18. Architecture Decision Records (ADRs)
- ADR-001: Why MGA over traditional MVCC
- ADR-002: Why UUID-based schemas
- ADR-003: Protocol translation strategies
- ADR-004: Performance trade-offs
- ADR-005: Embedded-first architecture
- ADR-006: Plugin system design
- ADR-007: Y-Valve routing architecture
- ADR-008: Universal type system

### 19. Compatibility Matrices
```
Feature Support Matrix:
┌─────────────────┬──────┬───────┬────────┬──────────┬──────────┐
│ Feature         │ PG   │ MySQL │ MSSQL  │ Firebird │ ScratchB │
├─────────────────┼──────┼───────┼────────┼──────────┼──────────┤
│ CTEs            │ Full │ 8.0+  │ Full   │ Full     │ Full     │
│ Window Funcs    │ Full │ 8.0+  │ Full   │ 3.0+     │ Full     │
│ Arrays          │ Yes  │ No    │ No     │ No       │ Yes      │
│ JSON            │ Yes  │ Yes   │ Yes    │ No       │ Yes      │
│ Full Text       │ Yes  │ Yes   │ Yes    │ No       │ Planned  │
└─────────────────┴──────┴───────┴────────┴──────────┴──────────┘
```

### 20. Development Guides
- How to add a new SQL function
- How to add a new data type
- How to add a new protocol listener
- How to add a new storage engine
- How to add a new index type
- How to add a new authentication method
- How to add a new system catalog
- How to add a new SQL dialect

## Academic Papers and Research

### 21. Foundational Papers
- "ARIES: A Transaction Recovery Method" (Mohan et al., 1992)
- "Generalized Isolation Level Definitions" (Adya et al., 2000)
- "The Log-Structured Merge-Tree" (O'Neil et al., 1996)
- "C-Store: A Column-oriented DBMS" (Stonebraker et al., 2005)
- "Serializable Snapshot Isolation" (Cahill et al., 2008)
- "Calvin: Fast Distributed Transactions" (Thomson et al., 2012)

### 22. Query Optimization
- "Access Path Selection in a RDBMS" (Selinger et al., 1979)
- "The Volcano Optimizer Generator" (Graefe & McKenna, 1993)
- "How Good Are Query Optimizers, Really?" (Leis et al., 2015)
- "Adaptive Query Processing" (Deshpande et al., 2007)

### 23. Concurrency Control
- "On Optimistic Methods for Concurrency Control" (Kung & Robinson, 1981)
- "The Phantom Problem" (Eswaran et al., 1976)
- "A Critique of ANSI SQL Isolation Levels" (Berenson et al., 1995)

## Implementation Patterns

### 24. Design Patterns for Databases
- Iterator pattern for result sets
- Visitor pattern for query trees
- Strategy pattern for execution plans
- Factory pattern for protocol handlers
- Observer pattern for triggers
- Command pattern for transactions
- Memento pattern for savepoints
- Flyweight pattern for type descriptors

### 25. Performance Patterns
- Batch processing patterns
- Connection pooling patterns
- Query result caching
- Prepared statement caching
- Statistics caching
- Metadata caching
- Buffer pool warming
- Adaptive algorithms

## Tools and Utilities

### 26. Development Tools
- Valgrind for memory debugging
- Perf for performance profiling
- GDB for debugging
- AddressSanitizer (ASAN)
- ThreadSanitizer (TSAN)
- Undefined Behavior Sanitizer (UBSAN)
- Coverage tools (gcov, lcov)
- Static analyzers (clang-tidy, cppcheck)

### 27. Database Tools to Study
- pg_dump/pg_restore structure
- mysqldump format
- SQL Server bcp utility
- Firebird gbak format
- pgbench for benchmarking
- mysqlslap for load testing
- EXPLAIN output formats
- Query plan visualizers

## Compliance and Standards

### 28. Regulatory Compliance
- GDPR requirements for databases
- HIPAA compliance checklist
- PCI DSS for payment card data
- SOX compliance for financial data
- FIPS 140-2 for cryptography
- Common Criteria certification

### 29. Industry Standards
- SQL:2016 standard (ISO/IEC 9075)
- ODBC 3.8 specification
- JDBC 4.3 specification
- XA distributed transaction protocol
- X/Open DTP model
- ISO 8601 date/time formats

## Cloud and Containerization

### 30. Cloud Database Patterns
- Database-as-a-Service patterns
- Multi-tenancy strategies
- Backup to cloud storage
- Read replica patterns
- Sharding strategies
- Connection proxying
- Serverless database patterns

### 31. Container/Kubernetes Patterns
- StatefulSet configurations
- Persistent volume management
- Operator patterns
- Service mesh integration
- Init containers for setup
- Sidecar containers for monitoring
- Health check implementations

## File Organization

```
/workspace/references/
├── implementation/
│   ├── memory_management/
│   ├── query_processing/
│   ├── storage_formats/
│   └── concurrency_control/
├── standards/
│   ├── network_security/
│   ├── data_formats/
│   └── distributed_systems/
├── testing/
│   ├── benchmarks/
│   └── fuzzing/
├── compatibility/
│   ├── migration_tools/
│   └── orm_requirements/
├── database_internals/
│   ├── postgresql/
│   ├── mysql/
│   ├── mssql/
│   └── firebird/
├── academic_papers/
│   ├── foundational/
│   ├── optimization/
│   └── concurrency/
└── guides/
    ├── architecture_decisions/
    ├── compatibility_matrices/
    └── development_guides/
```

## Priority for Gathering

### Phase 1-10 (Core Engine)
- Firebird MGA documentation
- Basic concurrency control
- Storage formats
- Memory management basics

### Phase 11-20 (Advanced Engine)
- Query optimization papers
- Index structures
- Transaction protocols
- WAL/recovery methods

### Phase 21-30 (Multi-Protocol)
- Wire protocol specifications
- Authentication methods
- System catalogs
- Error code mappings

### Phase 31-40 (Federation/Distribution)
- Distributed transaction protocols
- Consensus algorithms
- Replication methods
- Sharding strategies

## Sources

### Books
- "Transaction Processing" by Gray & Reuter
- "Database System Implementation" by Garcia-Molina et al.
- "Database Internals" by Alex Petrov
- "Designing Data-Intensive Applications" by Martin Kleppmann

### Online Resources
- PostgreSQL Developer Documentation
- MySQL Internals Manual
- SQL Server Architecture Guide
- Firebird Internal Documentation
- SQLite Technical Documentation

### Open Source Projects to Study
- PostgreSQL (postgres/postgres)
- MySQL (mysql/mysql-server)
- MariaDB (MariaDB/server)
- Firebird (FirebirdSQL/firebird)
- CockroachDB (cockroachdb/cockroach)
- TiDB (pingcap/tidb)
- ClickHouse (ClickHouse/ClickHouse)
- DuckDB (duckdb/duckdb)