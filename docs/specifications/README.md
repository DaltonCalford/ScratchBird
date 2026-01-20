# ScratchBird Technical Specifications

**Master Index and Navigation Guide**

This directory contains comprehensive technical specifications for the ScratchBird database management system. ScratchBird implements a Firebird-style Multi-Generational Architecture (MGA) with multi-dialect SQL support and advanced distributed cluster capabilities.

**Total Specifications:** 350+ documents across 95+ subdirectories
**Last Updated:** January 2026

---

## Scope Clarifications (Current Version)

- **Write-after log (WAL)/LSN**: MGA provides recovery without write-after log (WAL); the optional write-after log (WAL) is not implemented in the current version. Any write-after log (WAL) references describe a future, optional write-after log for replication/PITR (not recovery).
- **MSSQL/TDS**: SQL Server/TDS support is deferred until after the project goes gold; references are forward-looking.
- **Emulated databases**: Emulated databases are metadata-only schemas; CREATE DATABASE in emulated dialects does not create physical files.

## Quick Navigation

### 🚀 Getting Started

**For Implementers:**
1. Start with [MGA_RULES.md](../../MGA_RULES.md) - **CRITICAL** architecture rules
2. Read [IMPLEMENTATION_STANDARDS.md](../../IMPLEMENTATION_STANDARDS.md) - Implementation requirements
3. Review [CORE_IMPLEMENTATION_SPECS_SUMMARY.md](CORE_IMPLEMENTATION_SPECS_SUMMARY.md)
4. Follow reading order below for your area

**For Architects:**
1. [Security Architecture](security/00_SECURITY_SPEC_INDEX.md) - Complete security design (19 specs)
2. [Cluster Architecture](cluster/SBCLUSTER-SUMMARY.md) - Distributed system design (18 specs)
3. [Transaction System](TRANSACTION_MAIN.md) - MGA transaction model
4. [Storage Engine](STORAGE_ENGINE_MAIN.md) - Storage architecture

**For Security Reviewers:**
1. [Security Specifications](security/) - 19 comprehensive security specs
2. [Cluster Security](cluster/SBCLUSTER-04-SECURITY-BUNDLE.md)
3. [Authentication](AUTH_CORE_FRAMEWORK.md)
4. [Authorization](security/03_AUTHORIZATION_MODEL.md)

---

## Directory Organization

### Core Database Engine

| Directory | Description | Key Files | Status |
|-----------|-------------|-----------|--------|
| **Root** | Core specifications | BACKUP_AND_RESTORE, MEMORY_MANAGEMENT, PERFORMANCE_BENCHMARKS | ✅ Clean |
| [**parser/**](parser/) | SQL parsing and grammar | 8 parser specs, BNF grammar, emulated parsers | ✅ Organized |
| [**ddl/**](ddl/) | Data Definition Language | 19 DDL operation specs | ✅ Organized |
| [**dml/**](dml/) | Data Manipulation Language | 7 DML operation specs | ✅ Organized |
| [**transaction/**](transaction/) | MGA transactions | Transaction, locking, MGA core, distributed | ✅ Organized |
| [**storage/**](storage/) | Storage layer | Buffer pool, page management, TOAST, heap, tablespace | ✅ Organized |
| [**indexes/**](indexes/) | Index implementations | 11 index types including HNSW, LSM, columnstore | ✅ Organized |
| [**types/**](types/) | Data types | Types, domains, arrays, geometry, timezones | ✅ Organized |
| [**query/**](query/) | Query optimization | Optimizer, planner | ✅ Organized |
| [**sblr/**](sblr/) | Bytecode runtime | SBLR opcodes, BLR mapping, execution, performance (Alpha/Beta) | ✅ Organized |
| [**core/**](core/) | Core internals | Listener/pool architecture, thread safety, internal functions | ✅ Organized |
| [**catalog/**](catalog/) | System catalog | Catalog structure, schema resolution, components | ✅ Organized |

### Security & Authentication

| Directory | Description | Files | Status |
|-----------|-------------|-------|--------|
| [**Security Design Specification/**](Security%20Design%20Specification/) | Complete security architecture | 26 specs (all auth frameworks included) | ✅ Complete |
| └─ Authentication | Auth frameworks | Certificate/TLS, OAuth/MFA, LDAP/Kerberos, Password | ✅ Organized |
| └─ [Encryption](Security%20Design%20Specification/04_ENCRYPTION_KEY_MANAGEMENT.md) | Key management | Hierarchical key system | ✅ Complete |
| └─ [Audit](Security%20Design%20Specification/08_AUDIT_COMPLIANCE.md) | Audit chain | Cryptographic audit trail | ✅ Complete |
| └─ Role Composition | RBAC | Role hierarchies and composition | ✅ Complete |

### Distributed System (Beta)

| Directory | Description | Files | Status |
|-----------|-------------|-------|--------|
| [**cluster/**](cluster/) | Distributed cluster architecture | 18 specs | ✅ Complete |
| [Raft Consensus](cluster/SBCLUSTER-01-CLUSTER-CONFIG-EPOCH.md) | Cluster config management | CCE specification | ✅ Complete |
| [Sharding](cluster/SBCLUSTER-05-SHARDING.md) | Data partitioning | Consistent hashing | ✅ Complete |
| [Replication](cluster/SBCLUSTER-07-REPLICATION.md) | Data replication | Async logical stream (optional write-after log) | ✅ Complete |

### Network & Connectivity

| Directory | Description | Files | Status |
|-----------|-------------|-------|--------|
| [**network/**](network/) | Network layer & wire protocols | Network layer, listener & parser pool (legacy Y-Valve term) | ✅ Organized |
| [**wire_protocols/**](wire_protocols/) | Protocol specifications | TDS (post-gold) and other protocols | ✅ Active |
| [**api/**](api/) | Client APIs | Client library API, connection pooling | ✅ Organized |

Key network specs:
- [network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md](network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md) - Listener startup, parser pools, and socket handoff
- [network/CONTROL_PLANE_PROTOCOL_SPEC.md](network/CONTROL_PLANE_PROTOCOL_SPEC.md) - Listener <-> parser control plane
- [network/PARSER_AGENT_SPEC.md](network/PARSER_AGENT_SPEC.md) - Parser agent binaries and lifecycle
- [network/ENGINE_PARSER_IPC_CONTRACT.md](network/ENGINE_PARSER_IPC_CONTRACT.md) - Parser <-> engine IPC contract
- [network/DIALECT_AUTH_MAPPING_SPEC.md](network/DIALECT_AUTH_MAPPING_SPEC.md) - Dialect auth mapping

### Drivers & Integrations

| Directory | Description | Files | Status |
|-----------|-------------|-------|--------|
| [**drivers/**](drivers/) | Database drivers & emulation | JDBC, ODBC, Firebird, PostgreSQL, MySQL, MSSQL (post-gold) specs | ✅ Organized |
| [**beta_requirements/**](beta_requirements/) | Beta drivers & integrations | 140+ specs | ✅ Excellent |
| └─ [drivers/](beta_requirements/drivers/) | Language drivers | 11 driver specs (P0: 7) | ✅ Specified |
| └─ [orms/](beta_requirements/orms-frameworks/) | ORM frameworks | 12 ORM integrations | ✅ Specified |
| └─ [tools/](beta_requirements/tools/) | Database tools | DBeaver, pgAdmin, etc. | ✅ Specified |
| └─ [cloud/](beta_requirements/cloud-container/) | Cloud deployment | Docker, K8s, Helm | ✅ Specified |
| └─ [optional/](beta_requirements/optional/) | Optional beta engine features | Storage encoding optimizations | ✅ Draft |

### Additional Subsystems

| Directory | Description | Files | Status |
|-----------|-------------|-------|--------|
| [**replication/**](replication/) | Replication protocols | Replication, shadow protocols, optional write-after log | ✅ Organized |
| [**compression/**](compression/) | Compression framework | Compression specifications | ✅ Organized |
| [**udr/**](udr/) | User-Defined Resources | UDR system specification | ✅ Organized |
| [**triggers/**](triggers/) | Trigger system | Trigger context variables | ✅ Organized |
| [**scheduler/**](scheduler/) | Job scheduler and runner | Canonical scheduler spec (Alpha + Beta) | ✅ Organized |
| [**operations/**](operations/) | Operations & monitoring | Prometheus metrics + listener/pool metrics | ✅ Organized |
| [**admin/**](admin/) | Administration tools | CLI administration + sb_server network CLI | ✅ Organized |
| [**deployment/**](deployment/) | Deployment | systemd service specification | ✅ Organized |
| [**testing/**](testing/) | Test plans | Alpha 3 test plan | ✅ Organized |

### User-Defined Resources

| Directory | Description | Files | Status |
|-----------|-------------|-------|--------|
| **udr_connectors/** | UDR connectors | 7 connector specs | ⚠️ Consolidate |
| **remote_database_udr/** | Remote DB adapters | 10 adapter specs | ⚠️ Consolidate |
| [UDR System](10-UDR-System-Specification.md) | UDR architecture | System design | ✅ Active |

### Reference Material

| Directory | Description | Files | Status |
|-----------|-------------|-------|--------|
| [**reference/firebird/**](reference/firebird/) | Firebird reference docs | 27 split docs | ✅ Reference |
| └─ [Firebird 5.0 Reference](reference/firebird/FirebirdReferenceDocument.md) | Complete Firebird docs | 50,000+ lines | ✅ Reference |

### Archived Content

| Directory | Description | Files | Status |
|-----------|-------------|-------|--------|
| [**archive/**](archive/) | Legacy specifications | Research docs | 📦 Archived |

---

## Detailed Category Listings

### Grammar & Parser

SQL grammar and parser specifications:

**Core Grammar:**
- [SCRATCHBIRD_SQL_COMPLETE_BNF.md](SCRATCHBIRD_SQL_COMPLETE_BNF.md) (1,527 lines) - Complete SQL BNF grammar
- [ScratchBird Master Grammar Specification v2.0.md](ScratchBird%20Master%20Grammar%20Specification%20v2.0.md) - V2 grammar overview
- [ScratchBird SQL Language Specification - Master Document.md](ScratchBird%20SQL%20Language%20Specification%20-%20Master%20Document.md)
- [01_SQL_DIALECT_OVERVIEW.md](01_SQL_DIALECT_OVERVIEW.md) (115 lines) - SQL dialect overview

**Parser Architecture:**
- [EMULATED_DATABASE_PARSER_SPECIFICATION.md](EMULATED_DATABASE_PARSER_SPECIFICATION.md) (303 lines) - **CRITICAL** - Parser rules
- [08_PARSER_AND_DEVELOPER_EXPERIENCE.md](08_PARSER_AND_DEVELOPER_EXPERIENCE.md) (163 lines) - Parser DX

**Emulated Parsers:**
- [POSTGRESQL_PARSER_SPECIFICATION.md](POSTGRESQL_PARSER_SPECIFICATION.md) (1,626 lines)
- [POSTGRESQL_PARSER_IMPLEMENTATION.md](POSTGRESQL_PARSER_IMPLEMENTATION.md) (671 lines)
- [MYSQL_PARSER_SPECIFICATION.md](MYSQL_PARSER_SPECIFICATION.md) (949 lines)

### DDL Statements

All Data Definition Language specifications:

**Overview:**
- [02_DDL_STATEMENTS_OVERVIEW.md](02_DDL_STATEMENTS_OVERVIEW.md) - DDL operations overview

**Database Objects:**
- [DDL_DATABASES.md](DDL_DATABASES.md) - CREATE/ALTER/DROP DATABASE
- [DDL_SCHEMAS.md](DDL_SCHEMAS.md) - Schema management
- [DDL_TABLES.md](DDL_TABLES.md) - Table creation and modification
- [DDL_VIEWS.md](DDL_VIEWS.md) - View management
- [DDL_INDEXES.md](DDL_INDEXES.md) - Index creation
- [DDL_SEQUENCES.md](DDL_SEQUENCES.md) - Sequence management

**Advanced Objects:**
- [DDL_DOMAINS_COMPREHENSIVE.md](DDL_DOMAINS_COMPREHENSIVE.md) (963 lines) - Complete domain specification
- [DDL_FUNCTIONS.md](DDL_FUNCTIONS.md) - User-defined functions
- [DDL_PROCEDURES.md](DDL_PROCEDURES.md) - Stored procedures
- [DDL_PACKAGES.md](DDL_PACKAGES.md) - Package objects
- [DDL_TRIGGERS.md](DDL_TRIGGERS.md) - Trigger management
- [DDL_EVENTS.md](DDL_EVENTS.md) - Event management
- [DDL_EXCEPTIONS.md](DDL_EXCEPTIONS.md) - Exception handling
- [DDL_USER_DEFINED_RESOURCES.md](DDL_USER_DEFINED_RESOURCES.md) - UDR management

**Security & Access:**
- [DDL_ROLES_AND_GROUPS.md](DDL_ROLES_AND_GROUPS.md) - Role-based access control
- [DDL_ROW_LEVEL_SECURITY.md](DDL_ROW_LEVEL_SECURITY.md) - RLS implementation

**Advanced Features:**
- [DDL_TABLE_PARTITIONING.md](DDL_TABLE_PARTITIONING.md) - Table partitioning
- [DDL_TEMPORAL_TABLES.md](DDL_TEMPORAL_TABLES.md) - Temporal table support
- [09_DDL_FOREIGN_DATA.md](09_DDL_FOREIGN_DATA.md) - Foreign data wrappers
- [CASCADE_DROP_SPECIFICATION.md](CASCADE_DROP_SPECIFICATION.md) (1,029 lines) - Cascade operations

### DML Statements

Data Manipulation Language specifications:

- [04_DML_STATEMENTS_OVERVIEW.md](04_DML_STATEMENTS_OVERVIEW.md) - DML overview
- [DML_SELECT.md](DML_SELECT.md) - Query operations
- [DML_INSERT.md](DML_INSERT.md) - Insert operations
- [DML_UPDATE.md](DML_UPDATE.md) - Update operations
- [DML_DELETE.md](DML_DELETE.md) - Delete operations
- [DML_MERGE.md](DML_MERGE.md) - Merge/upsert operations
- [DML_XML_JSON_TABLES.md](DML_XML_JSON_TABLES.md) - XML/JSON table functions

### Transaction System

Multi-Generational Architecture (MGA) and transaction management:

**Core Specifications:**
- [TRANSACTION_MAIN.md](TRANSACTION_MAIN.md) (741 lines) - Main transaction spec
- [TRANSACTION_MGA_CORE.md](TRANSACTION_MGA_CORE.md) (1,059 lines) - MGA implementation
- [TRANSACTION_LOCK_MANAGER.md](TRANSACTION_LOCK_MANAGER.md) (1,120 lines) - Lock management
- [TRANSACTION_DISTRIBUTED.md](TRANSACTION_DISTRIBUTED.md) (1,136 lines) - Distributed transactions
- [MGA_IMPLEMENTATION.md](MGA_IMPLEMENTATION.md) (1,024 lines) - MGA details
- [07_TRANSACTION_AND_SESSION_CONTROL.md](07_TRANSACTION_AND_SESSION_CONTROL.md) (181 lines) - Session control

**Reference:**
- [FIREBIRD_TRANSACTION_MODEL_SPEC.md](FIREBIRD_TRANSACTION_MODEL_SPEC.md) (1,570 lines) - Firebird reference

### Storage Engine

Storage layer and buffer management:

**Core Storage:**
- [STORAGE_ENGINE_MAIN.md](STORAGE_ENGINE_MAIN.md) (804 lines) - Storage architecture
- [STORAGE_ENGINE_BUFFER_POOL.md](STORAGE_ENGINE_BUFFER_POOL.md) (1,025 lines) - Buffer pool
- [STORAGE_ENGINE_PAGE_MANAGEMENT.md](STORAGE_ENGINE_PAGE_MANAGEMENT.md) (1,288 lines) - Page management
- [ON_DISK_FORMAT.md](ON_DISK_FORMAT.md) (552 lines) - Disk format

**Large Objects:**
- [HEAP_TOAST_INTEGRATION.md](HEAP_TOAST_INTEGRATION.md) (195 lines) - TOAST integration
- [TOAST_LOB_STORAGE.md](TOAST_LOB_STORAGE.md) (506 lines) - LOB storage

**Advanced:**
- [TABLESPACE_SPECIFICATION.md](TABLESPACE_SPECIFICATION.md) (1,352 lines) - Tablespace management
- [EXTENDED_PAGE_SIZES.md](EXTENDED_PAGE_SIZES.md) (107 lines) - Configurable page sizes
- [COMPRESSION_FRAMEWORK.md](COMPRESSION_FRAMEWORK.md) (234 lines) - Compression support
- [COLUMNSTORE_SPEC.md](COLUMNSTORE_SPEC.md) (712 lines) - Columnar storage
- [MEMORY_MANAGEMENT.md](MEMORY_MANAGEMENT.md) (95 lines) - ⚠️ Needs expansion
- [THREAD_SAFETY.md](THREAD_SAFETY.md) (105 lines) - Thread safety

**Alternative Storage:**
- [LSM_TREE_SPEC.md](LSM_TREE_SPEC.md) (1,596 lines) - LSM tree design
- [LSM_TREE_ARCHITECTURE.md](LSM_TREE_ARCHITECTURE.md) (493 lines) - LSM architecture

### Index System

11 index types and index infrastructure:

**Core Infrastructure:**
- [INDEX_ARCHITECTURE.md](INDEX_ARCHITECTURE.md) (983 lines) - Index architecture
- [INDEX_IMPLEMENTATION_GUIDE.md](INDEX_IMPLEMENTATION_GUIDE.md) (1,135 lines) - Implementation guide
- [INDEX_IMPLEMENTATION_SPEC.md](INDEX_IMPLEMENTATION_SPEC.md) (915 lines) - Implementation spec
- [INDEX_GC_PROTOCOL.md](INDEX_GC_PROTOCOL.md) (622 lines) - Garbage collection

**Advanced Index Types:**
- [AdvancedIndexes.md](AdvancedIndexes.md) (1,283 lines) - Advanced index overview
- [BloomFilterIndex.md](BloomFilterIndex.md) (1,529 lines) - Bloom filter index
- [InvertedIndex.md](InvertedIndex.md) (2,333 lines) - Inverted index (full-text)
- [IVFIndex.md](IVFIndex.md) (2,243 lines) - IVF vector index
- [ZoneMapsIndex.md](ZoneMapsIndex.md) (2,222 lines) - Zone maps index

### Type System

Data types, domains, and type coercion:

- [03_TYPES_AND_DOMAINS.md](03_TYPES_AND_DOMAINS.md) (285 lines) - Type system overview
- [DATA_TYPE_PERSISTENCE_AND_CASTS.md](DATA_TYPE_PERSISTENCE_AND_CASTS.md) (129 lines) - Type persistence
- [POSTGRESQL_ARRAY_TYPE_SPEC.md](POSTGRESQL_ARRAY_TYPE_SPEC.md) (883 lines) - Array types
- [MULTI_GEOMETRY_TYPES_SPEC.md](MULTI_GEOMETRY_TYPES_SPEC.md) (718 lines) - Geometric types
- [UUID_IDENTITY_COLUMNS.md](UUID_IDENTITY_COLUMNS.md) (536 lines) - UUID support
- [character_sets_and_collations.md](character_sets_and_collations.md) (780 lines) - Character sets
- [design_limits.md](design_limits.md) (225 lines) - System limits

### Query Processing

Query optimization and execution:

- [QUERY_OPTIMIZER_SPEC.md](QUERY_OPTIMIZER_SPEC.md) (1,248 lines) - Query optimizer

### SBLR Bytecode

ScratchBird Bytecode Language Runtime:

- [Appendix_A_SBLR_BYTECODE.md](Appendix_A_SBLR_BYTECODE.md) (519 lines) - SBLR bytecode spec
- [SBLR_OPCODE_REGISTRY.md](SBLR_OPCODE_REGISTRY.md) (84 lines) - Opcode registry
- [SBLR_DOMAIN_PAYLOADS.md](SBLR_DOMAIN_PAYLOADS.md) (215 lines) - Domain payloads
- [FIREBIRD_BLR_TO_SBLR_MAPPING.md](FIREBIRD_BLR_TO_SBLR_MAPPING.md) (173 lines) - Firebird BLR mapping
- [FIREBIRD_BLR_FIXTURES.md](FIREBIRD_BLR_FIXTURES.md) (153 lines) - BLR fixtures

### Authentication

Authentication frameworks and methods:

**Core Framework:**
- [AUTH_CORE_FRAMEWORK.md](AUTH_CORE_FRAMEWORK.md) (1,263 lines) - Core auth framework
- [EXTERNAL_AUTHENTICATION_DESIGN.md](EXTERNAL_AUTHENTICATION_DESIGN.md) (484 lines) - External auth
- [ROLE_COMPOSITION_AND_HIERARCHIES.md](ROLE_COMPOSITION_AND_HIERARCHIES.md) (451 lines) - Role hierarchies

**Authentication Methods:**
- [AUTH_PASSWORD_METHODS.md](AUTH_PASSWORD_METHODS.md) (1,260 lines) - Password authentication
- [AUTH_CERTIFICATE_TLS.md](AUTH_CERTIFICATE_TLS.md) (1,419 lines) - Certificate-based auth
- [AUTH_ENTERPRISE_LDAP_KERBEROS.md](AUTH_ENTERPRISE_LDAP_KERBEROS.md) (1,116 lines) - Enterprise auth
- [AUTH_MODERN_OAUTH_MFA.md](AUTH_MODERN_OAUTH_MFA.md) (1,511 lines) - OAuth & MFA

### Replication & Backup

Durability and replication:

- [BACKUP_AND_RESTORE.md](BACKUP_AND_RESTORE.md) (72 lines) - ⚠️ Needs major expansion
- [WAL_IMPLEMENTATION.md](WAL_IMPLEMENTATION.md) (79 lines) - Optional write-after log (post-gold)
- [REPLICATION_AND_SHADOW_PROTOCOLS.md](REPLICATION_AND_SHADOW_PROTOCOLS.md) (589 lines) - Replication

### Special Features

Unique ScratchBird features:

- [GIT_METADATA_INTEGRATION_SPECIFICATION.md](GIT_METADATA_INTEGRATION_SPECIFICATION.md) (981 lines) - Git integration
- [LIVE_MIGRATION_PASSTHROUGH_SPECIFICATION.md](LIVE_MIGRATION_PASSTHROUGH_SPECIFICATION.md) (1,824 lines) - Live migration
- [TRIGGER_CONTEXT_VARIABLES.md](TRIGGER_CONTEXT_VARIABLES.md) (592 lines) - Trigger variables
- [EXTRACT_AND_ALTER_ELEMENT.md](EXTRACT_AND_ALTER_ELEMENT.md) (390 lines) - Element operations
- [INTERNAL_FUNCTIONS.md](INTERNAL_FUNCTIONS.md) (98 lines) - Internal functions

### Tools & Administration

Administrative tools and interfaces:

- [SB_ADMIN_CLI_SPECIFICATION.md](SB_ADMIN_CLI_SPECIFICATION.md) (608 lines) - Admin CLI
- [SB_SERVER_NETWORK_CLI_SPECIFICATION.md](SB_SERVER_NETWORK_CLI_SPECIFICATION.md) (new) - sb_server network CLI
- [SB_ISQL_CLI_SPECIFICATION.md](SB_ISQL_CLI_SPECIFICATION.md) (new) - sb_isql network CLI
- [SB_TOOLING_NETWORK_SPEC.md](SB_TOOLING_NETWORK_SPEC.md) (new) - Remote tooling support
- [FlameRobin_Specification_for_AI.md](FlameRobin_Specification_for_AI.md) (442 lines) - FlameRobin integration
- [PROMETHEUS_METRICS_REFERENCE.md](PROMETHEUS_METRICS_REFERENCE.md) (824 lines) - Metrics export
- [LISTENER_POOL_METRICS.md](LISTENER_POOL_METRICS.md) (new) - Listener/pool metrics
- [PERFORMANCE_BENCHMARKS.md](PERFORMANCE_BENCHMARKS.md) (52 lines) - ⚠️ Needs expansion

### Deployment

Deployment and operations:

- [SYSTEMD_SERVICE_SPECIFICATION.md](SYSTEMD_SERVICE_SPECIFICATION.md) (2,127 lines) - systemd integration
- [ALPHA3_TEST_PLAN.md](ALPHA3_TEST_PLAN.md) (727 lines) - Test planning

### Architecture

Core architecture documents:

- [COMPONENT_MODEL_AND_RESPONSIBILITIES.md](COMPONENT_MODEL_AND_RESPONSIBILITIES.md) (155 lines) - Component model
- [IMPLEMENTATION_RECOMMENDATIONS.md](IMPLEMENTATION_RECOMMENDATIONS.md) (436 lines) - Strategic recommendations
- [CORE_IMPLEMENTATION_SPECS_SUMMARY.md](CORE_IMPLEMENTATION_SPECS_SUMMARY.md) (204 lines) - Implementation summary

---

## Specification Status Indicators

- ✅ **Complete** - Fully specified and reviewed
- 🚧 **In Progress** - Active development
- ⚠️ **Needs Work** - Incomplete or needs expansion
- ⏳ **Pending** - Waiting for dependencies
- 📦 **Archived** - Legacy/deprecated
- 🔍 **Reference** - External reference material

---

## Reading Orders

### For New Implementers

1. **Essential Reading** (Must Read First)
   - [../../MGA_RULES.md](../../MGA_RULES.md) - **CRITICAL** - MGA architecture rules
   - [../../IMPLEMENTATION_STANDARDS.md](../../IMPLEMENTATION_STANDARDS.md) - Implementation requirements
   - [EMULATED_DATABASE_PARSER_SPECIFICATION.md](EMULATED_DATABASE_PARSER_SPECIFICATION.md) - Parser architecture

2. **Core Engine** (Read in Order)
   - [TRANSACTION_MAIN.md](TRANSACTION_MAIN.md) → [TRANSACTION_MGA_CORE.md](TRANSACTION_MGA_CORE.md)
   - [STORAGE_ENGINE_MAIN.md](STORAGE_ENGINE_MAIN.md) → [STORAGE_ENGINE_BUFFER_POOL.md](STORAGE_ENGINE_BUFFER_POOL.md)
   - [INDEX_ARCHITECTURE.md](INDEX_ARCHITECTURE.md) → [INDEX_IMPLEMENTATION_GUIDE.md](INDEX_IMPLEMENTATION_GUIDE.md)

3. **SQL Layer** (Read in Order)
   - [SCRATCHBIRD_SQL_COMPLETE_BNF.md](SCRATCHBIRD_SQL_COMPLETE_BNF.md)
   - [02_DDL_STATEMENTS_OVERVIEW.md](02_DDL_STATEMENTS_OVERVIEW.md)
   - [04_DML_STATEMENTS_OVERVIEW.md](04_DML_STATEMENTS_OVERVIEW.md)
   - [QUERY_OPTIMIZER_SPEC.md](QUERY_OPTIMIZER_SPEC.md)

### For Security Reviewers

1. [security/00_SECURITY_SPEC_INDEX.md](security/00_SECURITY_SPEC_INDEX.md) - Start here
2. [security/01_SECURITY_ARCHITECTURE.md](security/01_SECURITY_ARCHITECTURE.md) - Architecture
3. [security/02_IDENTITY_AUTHENTICATION.md](security/02_IDENTITY_AUTHENTICATION.md) - Authentication
4. [security/03_AUTHORIZATION_MODEL.md](security/03_AUTHORIZATION_MODEL.md) - Authorization
5. [security/04_ENCRYPTION_KEY_MANAGEMENT.md](security/04_ENCRYPTION_KEY_MANAGEMENT.md) - Encryption
6. [security/08_AUDIT_COMPLIANCE.md](security/08_AUDIT_COMPLIANCE.md) - Audit

### For Cluster/Distributed Architects

1. [cluster/SBCLUSTER-SUMMARY.md](cluster/SBCLUSTER-SUMMARY.md) - Executive summary
2. [cluster/SBCLUSTER-00-GUIDING-PRINCIPLES.md](cluster/SBCLUSTER-00-GUIDING-PRINCIPLES.md) - Principles
3. [cluster/SBCLUSTER-01-CLUSTER-CONFIG-EPOCH.md](cluster/SBCLUSTER-01-CLUSTER-CONFIG-EPOCH.md) - CCE & Raft
4. [cluster/SBCLUSTER-05-SHARDING.md](cluster/SBCLUSTER-05-SHARDING.md) - Sharding
5. [cluster/SBCLUSTER-07-REPLICATION.md](cluster/SBCLUSTER-07-REPLICATION.md) - Replication

---

## Contributing to Specifications

### Adding New Specifications

1. **Determine category** - Which subdirectory does this belong in?
2. **Use consistent naming** - Follow existing patterns (e.g., `DDL_*.md`, `TRANSACTION_*.md`)
3. **Include standard sections:**
   - Purpose
   - Requirements
   - Implementation Details
   - Testing Criteria
   - Examples
4. **Update this index** - Add to appropriate category
5. **Cross-reference** - Link to related specifications

### Expanding Incomplete Specifications

**Priority expansions needed:**
- [BACKUP_AND_RESTORE.md](BACKUP_AND_RESTORE.md) - Currently 72 lines, needs 1,000+
- [MEMORY_MANAGEMENT.md](MEMORY_MANAGEMENT.md) - Currently 95 lines, needs 800+
- [PERFORMANCE_BENCHMARKS.md](PERFORMANCE_BENCHMARKS.md) - Currently 52 lines, needs 1,000+

---

## Recent Changes

**January 2026:**
- ✅ Removed duplicate specifications (00_GRAMMAR_BNF.md, DDL_DOMAINS.md)
- ✅ Moved Firebird reference docs to reference/firebird/
- ✅ Created archive/ for legacy specifications
- ✅ Created this master index

**Pending Reorganization:**
- Create subdirectories for major categories (grammar/, ddl/, dml/, transaction/, storage/, indexes/, query/, sblr/, types/, catalog/, network/, replication/, udr/)
- Move authentication specs to security/authentication/
- Consolidate UDR specifications
- Rename directories for consistency (no spaces)

See [SPECIFICATIONS_REORGANIZATION_ANALYSIS.md](../SPECIFICATIONS_REORGANIZATION_ANALYSIS.md) for full reorganization plan.

---

## Additional Resources

- **Project Context:** [../../PROJECT_CONTEXT.md](../../PROJECT_CONTEXT.md)
- **MGA Rules:** [../../MGA_RULES.md](../../MGA_RULES.md) ← **CRITICAL**
- **Implementation Standards:** [../../IMPLEMENTATION_STANDARDS.md](../../IMPLEMENTATION_STANDARDS.md)
- **Official Roadmap:** [../../OFFICIAL_ROADMAP.md](../../OFFICIAL_ROADMAP.md)
- **Beta Requirements:** [beta_requirements/00_DRIVERS_AND_INTEGRATIONS_INDEX.md](beta_requirements/00_DRIVERS_AND_INTEGRATIONS_INDEX.md)

---

**Maintained by:** ScratchBird Development Team
**Last Updated:** January 2026
**Total Specifications:** 350+ documents
