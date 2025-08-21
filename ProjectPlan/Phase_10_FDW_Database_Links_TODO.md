# Phase 10 — FDW/SPI and Database Links: Detailed Implementation TODO

**Status**: Not Started
**Priority**: Medium (Enterprise Integration Features)
**Estimated Effort**: 12-16 weeks
**Dependencies**: Phases 1-9 (Complete executor, security model)

---

## Overview and Goals

Implement Foreign Data Wrapper (FDW) infrastructure with Service Provider Interface (SPI), enabling ScratchBird to query external data sources. Implement database links for cross-database operations and provide adapters for common data sources including files, PostgreSQL, and other databases.

### Exit Criteria
- ✅ FDW SPI framework allows third-party data source integration
- ✅ FOREIGN SERVER/USER MAPPING/FOREIGN TABLE DDL operations working
- ✅ IMPORT FOREIGN SCHEMA for automatic schema discovery
- ✅ Local adapters for Files (CSV/JSON) and PostgreSQL functional
- ✅ Database links with table@link syntax for cross-database queries
- ✅ Transaction semantics working (best-effort distributed transactions)
- ✅ GRANT/REVOKE permissions on DATABASE LINK enforced
- ✅ Performance acceptable for federated queries
- ✅ Comprehensive error handling and connection management

---

## Phase 10.1: FDW Infrastructure Foundation

### 10.1.1 FDW Service Provider Interface (SPI)
- [ ] **FDW plugin architecture**
  - [ ] Dynamic library loading for FDW implementations
  - [ ] FDW registration and discovery system
  - [ ] Version compatibility checking
  - [ ] Plugin lifecycle management (load/unload)

- [ ] **FDW base interface**
  - [ ] `ForeignDataWrapper` base class with virtual methods
  - [ ] Connection establishment and management
  - [ ] Schema introspection interface
  - [ ] Query execution interface
  - [ ] Transaction coordination interface

- [ ] **FDW capability negotiation**
  - [ ] Pushdown capability advertisement
  - [ ] Join pushdown support detection
  - [ ] Aggregate pushdown capability
  - [ ] Transaction support levels
  - [ ] Security requirement specification

### 10.1.2 Foreign Server Management
- [ ] **Server object infrastructure**
  - [ ] CREATE FOREIGN SERVER DDL implementation
  - [ ] Server configuration storage in catalog
  - [ ] Connection pooling and management
  - [ ] Health monitoring and failover support

- [ ] **Connection management**
  - [ ] Connection establishment protocols
  - [ ] Connection authentication and security
  - [ ] Connection pooling with limits
  - [ ] Connection cleanup and resource management

### 10.1.3 User Mapping System
- [ ] **User mapping infrastructure**
  - [ ] CREATE USER MAPPING DDL implementation
  - [ ] Local to remote user credential mapping
  - [ ] Credential storage and encryption
  - [ ] Role-based access control for mappings

- [ ] **Security integration**
  - [ ] Integration with ScratchBird security model
  - [ ] Credential validation and refresh
  - [ ] Audit logging for foreign operations
  - [ ] Permission inheritance and delegation

---

## Phase 10.2: Foreign Table Implementation

### 10.2.1 Foreign Table DDL
- [ ] **CREATE FOREIGN TABLE**
  - [ ] Foreign table definition parsing
  - [ ] Column type mapping from remote sources
  - [ ] Foreign table options and configuration
  - [ ] Catalog integration for foreign tables

- [ ] **Foreign table metadata**
  - [ ] SDB$FOREIGN_TABLE catalog table
  - [ ] Foreign table column definitions
  - [ ] Server and user mapping associations
  - [ ] Foreign table statistics and costs

### 10.2.2 IMPORT FOREIGN SCHEMA
- [ ] **Schema discovery**
  - [ ] Remote schema introspection
  - [ ] Automatic table and column discovery
  - [ ] Type mapping and compatibility checking
  - [ ] Selective import with filtering

- [ ] **Schema mapping**
  - [ ] Local schema creation for imported objects
  - [ ] Naming conflict resolution
  - [ ] Type conversion and validation
  - [ ] Foreign key and constraint mapping

### 10.2.3 Foreign Table Operations
- [ ] **Query execution**
  - [ ] SELECT operations on foreign tables
  - [ ] WHERE clause pushdown optimization
  - [ ] JOIN operations between local and foreign tables
  - [ ] Aggregate pushdown where supported

- [ ] **DML operations**
  - [ ] INSERT into foreign tables (where supported)
  - [ ] UPDATE of foreign table rows
  - [ ] DELETE from foreign tables
  - [ ] Bulk operations optimization

---

## Phase 10.3: File-Based FDW Implementation

### 10.3.1 CSV File FDW
- [ ] **CSV parsing infrastructure**
  - [ ] RFC 4180 compliant CSV parsing
  - [ ] Custom delimiter and quote handling
  - [ ] Header row processing
  - [ ] Character encoding support (UTF-8, Latin1, etc.)

- [ ] **CSV query execution**
  - [ ] Sequential file scanning
  - [ ] Basic WHERE clause filtering
  - [ ] Column projection optimization
  - [ ] Large file handling with streaming

### 10.3.2 JSON File FDW
- [ ] **JSON parsing support**
  - [ ] JSON document parsing and validation
  - [ ] Nested object and array handling
  - [ ] JSON path expression evaluation
  - [ ] Schema inference from JSON structure

- [ ] **JSON query operations**
  - [ ] Path-based column extraction
  - [ ] JSON array iteration
  - [ ] Nested query support
  - [ ] JSON type conversion to SQL types

### 10.3.3 File System Integration
- [ ] **File access management**
  - [ ] File system permission checking
  - [ ] Directory traversal for multi-file tables
  - [ ] File modification tracking
  - [ ] Concurrent file access handling

---

## Phase 10.4: PostgreSQL FDW Implementation

### 10.4.1 PostgreSQL Connection Management
- [ ] **libpq integration**
  - [ ] PostgreSQL connection establishment
  - [ ] Connection parameter handling
  - [ ] SSL/TLS connection support
  - [ ] Connection error handling and recovery

- [ ] **PostgreSQL authentication**
  - [ ] Password authentication
  - [ ] Certificate-based authentication
  - [ ] Kerberos authentication (basic)
  - [ ] Connection security validation

### 10.4.2 PostgreSQL Query Translation
- [ ] **SQL dialect translation**
  - [ ] Basic SQL syntax differences
  - [ ] Type mapping between PostgreSQL and ScratchBird
  - [ ] Function name translation
  - [ ] Operator mapping and precedence

- [ ] **Query pushdown optimization**
  - [ ] WHERE clause pushdown to PostgreSQL
  - [ ] JOIN pushdown for PostgreSQL-PostgreSQL joins
  - [ ] Aggregate function pushdown
  - [ ] ORDER BY/LIMIT pushdown

### 10.4.3 PostgreSQL Type System Integration
- [ ] **Type mapping**
  - [ ] PostgreSQL to ScratchBird type conversion
  - [ ] Array type handling
  - [ ] JSON/JSONB type support
  - [ ] Custom type handling

---

## Phase 10.5: Database Link Implementation

### 10.5.1 Database Link Infrastructure
- [ ] **Link definition and management**
  - [ ] CREATE DATABASE LINK DDL
  - [ ] Link configuration and connection parameters
  - [ ] Link authentication and security
  - [ ] Link health monitoring and status

- [ ] **Link naming and resolution**
  - [ ] Global link naming conventions
  - [ ] Link resolution in query context
  - [ ] Link availability checking
  - [ ] Link failover and redundancy

### 10.5.2 Cross-Database Query Syntax
- [ ] **table@link syntax parsing**
  - [ ] Parser extensions for @link notation
  - [ ] Link resolution in FROM clauses
  - [ ] JOIN operations across links
  - [ ] Subquery support with links

- [ ] **Query execution planning**
  - [ ] Cross-database join planning
  - [ ] Data movement optimization
  - [ ] Remote vs local execution decisions
  - [ ] Cost estimation for linked operations

### 10.5.3 Transaction Coordination
- [ ] **Distributed transaction support**
  - [ ] Two-phase commit (2PC) protocol implementation
  - [ ] Transaction coordinator logic
  - [ ] Participant transaction management
  - [ ] Failure recovery and cleanup

- [ ] **Best-effort transaction semantics**
  - [ ] Single-phase commit for read-only operations
  - [ ] Rollback coordination across links
  - [ ] Timeout handling for distributed operations
  - [ ] Consistency guarantee documentation

---

## Phase 10.6: Query Planning and Optimization

### 10.6.1 Federated Query Planning
- [ ] **Multi-source query planning**
  - [ ] Cost estimation for remote operations
  - [ ] Data movement cost calculation
  - [ ] Join order optimization with remote sources
  - [ ] Pushdown vs local execution decisions

- [ ] **Pushdown optimization**
  - [ ] WHERE clause pushdown to remote sources
  - [ ] JOIN pushdown when possible
  - [ ] Aggregate pushdown optimization
  - [ ] LIMIT/OFFSET pushdown

### 10.6.2 Execution Strategy Optimization
- [ ] **Data movement strategies**
  - [ ] Minimal data transfer optimization
  - [ ] Bulk data transfer for large datasets
  - [ ] Streaming for result sets
  - [ ] Parallel execution across sources

- [ ] **Caching strategies**
  - [ ] Remote schema caching
  - [ ] Statistics caching for cost estimation
  - [ ] Connection pooling and reuse
  - [ ] Result set caching (basic)

---

## Phase 10.7: Security and Access Control

### 10.7.1 FDW Security Model
- [ ] **Permission model**
  - [ ] USAGE permission on foreign servers
  - [ ] Permission checks for user mappings
  - [ ] Row-level security for foreign tables
  - [ ] Column-level access control

- [ ] **Credential management**
  - [ ] Secure credential storage
  - [ ] Credential rotation support
  - [ ] Integration with external credential stores
  - [ ] Audit logging for credential usage

### 10.7.2 Database Link Security
- [ ] **Link access control**
  - [ ] GRANT/REVOKE permissions on database links
  - [ ] Role-based link access
  - [ ] Link usage auditing
  - [ ] Cross-database permission validation

- [ ] **Security boundary enforcement**
  - [ ] Network security requirements
  - [ ] Data encryption in transit
  - [ ] Authentication validation
  - [ ] SQL injection prevention

---

## Phase 10.8: Error Handling and Diagnostics

### 10.8.1 Error Management
- [ ] **Connection error handling**
  - [ ] Network failure recovery
  - [ ] Authentication failure handling
  - [ ] Timeout management
  - [ ] Graceful degradation strategies

- [ ] **Query error handling**
  - [ ] Remote query failure propagation
  - [ ] Type conversion error handling
  - [ ] Transaction conflict resolution
  - [ ] Partial result handling

### 10.8.2 Diagnostics and Monitoring
- [ ] **Performance monitoring**
  - [ ] Query execution time tracking
  - [ ] Data transfer volume monitoring
  - [ ] Connection usage statistics
  - [ ] Error rate tracking

- [ ] **Diagnostic tools**
  - [ ] Connection testing utilities
  - [ ] Query explain for federated operations
  - [ ] Performance analysis tools
  - [ ] Health check interfaces

---

## Phase 10.9: Catalog Integration

### 10.9.1 FDW Catalog Extensions
- [ ] **Foreign server catalog**
  - [ ] SDB$FOREIGN_SERVER table
  - [ ] Server configuration and options
  - [ ] Server status and health tracking
  - [ ] Version and capability information

- [ ] **User mapping catalog**
  - [ ] SDB$USER_MAPPING table
  - [ ] Credential association tracking
  - [ ] Permission and role mapping
  - [ ] Usage statistics and auditing

### 10.9.2 Foreign Table Catalog
- [ ] **Foreign table metadata**
  - [ ] SDB$FOREIGN_TABLE table extension
  - [ ] Column mapping and type information
  - [ ] Performance statistics
  - [ ] Dependency tracking

- [ ] **Database link catalog**
  - [ ] SDB$DATABASE_LINK table
  - [ ] Link configuration and status
  - [ ] Usage permissions and access control
  - [ ] Connection pool information

---

## Phase 10.10: Testing and Validation

### 10.10.1 Unit Tests
- [ ] **FDW infrastructure tests**
  - [ ] Plugin loading and registration
  - [ ] Connection management
  - [ ] Schema discovery operations
  - [ ] Query pushdown logic

### 10.10.2 Integration Tests
- [ ] **End-to-end scenarios**
  - [ ] File-based data access
  - [ ] PostgreSQL integration
  - [ ] Cross-database joins
  - [ ] Transaction coordination

### 10.10.3 Performance Tests
- [ ] **Federated query performance**
  - [ ] Large dataset operations
  - [ ] Complex join scenarios
  - [ ] Concurrent foreign operations
  - [ ] Network latency impact analysis

---

## Implementation Priority

### **Foundation (Weeks 1-4)**
1. FDW SPI framework and plugin architecture
2. Foreign server and user mapping DDL
3. Basic connection management
4. Catalog integration for FDW objects

### **File Adapters (Weeks 5-8)**
1. CSV file FDW implementation
2. JSON file FDW implementation
3. File system integration
4. Basic query pushdown for files

### **PostgreSQL Integration (Weeks 9-12)**
1. PostgreSQL FDW implementation
2. Query translation and pushdown
3. Type system integration
4. Performance optimization

### **Database Links (Weeks 13-16)**
1. Database link infrastructure
2. Cross-database query syntax
3. Transaction coordination
4. Security and access control

---

## Success Metrics

- [ ] **Functionality**: All core FDW operations working correctly
- [ ] **Performance**: < 50% overhead for simple foreign queries
- [ ] **Scalability**: Support for 100+ concurrent foreign connections
- [ ] **Reliability**: Graceful handling of network and remote failures
- [ ] **Security**: Proper credential management and access control
- [ ] **Usability**: Easy setup and configuration of foreign data sources

This phase enables ScratchBird to integrate with existing data infrastructure and provides enterprise-grade data federation capabilities.
