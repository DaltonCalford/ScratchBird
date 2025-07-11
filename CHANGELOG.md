# ScratchBird Changelog

## Version 0.5 (2024-01-11)

### 🎯 SQL Dialect 4 Implementation
- **FROM-less SELECT statements**: Enable singleton queries without FROM clause
- **Multi-row INSERT VALUES**: Support multiple value groups in single statement
- **SYNONYM support**: Complete DDL infrastructure with schema-aware targeting

### 🌳 Hierarchical Schema System
- **8-level schema nesting**: Deep hierarchical schema organization
- **Schema navigation**: Path-based schema resolution and navigation
- **PostgreSQL compatibility**: Exceeds PostgreSQL schema capabilities

### 🔗 Database Links Enhancement
- **Schema-aware links**: 5 resolution modes for distributed databases
- **Context resolution**: CURRENT, HOME, USER schema references
- **Performance optimization**: Cached schema depth and path resolution

### 🛠️ Client Tool Updates
- **ISQL enhancements**: SET SCHEMA, SHOW SCHEMA commands
- **GBAK integration**: Hierarchical schema backup/restore
- **Python drivers**: SQL Dialect 4 support and object type constants
- **FlameRobin**: Complete metadata support and tree navigation

### 🏗️ Build System
- **CMake modernization**: Updated build configuration
- **Cross-platform support**: Enhanced POSIX/Linux compatibility
- **Tool renaming**: All tools prefixed with `sb_` (sb_isql, sb_gbak, etc.)

## Version 0.4 (Based on Firebird 6.0.0.929)

### Core Features
- Full Firebird 6.0 feature set
- ACID compliance and transaction management
- Multi-Version Concurrency Control (MVCC)
- Advanced indexing and query optimization
- User-defined functions and procedures
- Triggers and stored procedures
- International character set support

### Security
- SRP (Secure Remote Password) authentication
- Plugin-based authentication architecture
- Database encryption support
- Role-based access control
