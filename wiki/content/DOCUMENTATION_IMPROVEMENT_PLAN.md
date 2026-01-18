# Wiki Documentation Improvement Plan

**Created:** 2026-01-18
**Status:** Active
**Tracker:** [DOCUMENTATION_IMPROVEMENT_TRACKER.md](DOCUMENTATION_IMPROVEMENT_TRACKER.md)

---

## Overview

This plan addresses 60 documentation items across 9 phases, prioritized by user impact. Each phase includes specific file targets, content requirements, and source references.

**Estimated Total Items:** 60 files
**Priority Distribution:** HIGH (28), MEDIUM-HIGH (15), MEDIUM (17)

---

## Phase 1: Installation & Getting Started

**Priority:** HIGH
**Items:** 7 files
**Goal:** Enable new users to install and connect to ScratchBird

### 1.1 Linux Installation (`installation/Linux.md`)

**Current:** 14 lines, references external docs
**Target:** 150+ lines

**Required Sections:**
1. System Requirements
   - Supported distributions (Ubuntu 22.04+, Debian 12+, RHEL 8+, Fedora 38+)
   - Hardware requirements (RAM, disk, CPU)
   - Dependencies (libc, openssl, etc.)

2. Package Installation
   - APT repository setup (Debian/Ubuntu)
   - YUM/DNF repository setup (RHEL/Fedora)
   - Package commands with version pinning

3. Building from Source
   - Prerequisites (cmake, g++, etc.)
   - Clone and build commands
   - Build options (Debug/Release, features)

4. Post-Installation
   - Service setup (systemd)
   - Initial configuration
   - Verification steps

**Source References:**
- `docs/development/BUILD_INSTRUCTIONS.md`
- `docs/specifications/deployment/SYSTEMD_SERVICE_SPECIFICATION.md`
- `CMakeLists.txt` for build options

---

### 1.2 Windows Installation (`installation/Windows.md`)

**Current:** 14 lines, stub
**Target:** 150+ lines

**Required Sections:**
1. System Requirements
   - Windows 10/11/Server versions
   - Visual C++ runtime requirements
   - Hardware requirements

2. Installer Installation
   - MSI download and install
   - Silent install options
   - Installation directory structure

3. Building from Source
   - Visual Studio requirements
   - CMake GUI or command line
   - vcpkg for dependencies

4. Post-Installation
   - Windows Service configuration
   - Firewall rules
   - Verification steps

**Source References:**
- `docs/specifications/beta_requirements/cloud-container/docker/README.md` (build context)
- Windows-specific code in `src/server/`

---

### 1.3 macOS Installation (`installation/macOS.md`)

**Current:** 14 lines, stub
**Target:** 120+ lines

**Required Sections:**
1. System Requirements
   - macOS versions (12+)
   - Xcode Command Line Tools
   - Hardware (Intel/Apple Silicon)

2. Homebrew Installation
   - Tap and install commands
   - Configuration location

3. Building from Source
   - Prerequisites (brew packages)
   - Build commands
   - Universal binary notes

4. Post-Installation
   - launchd service setup
   - Verification steps

---

### 1.4 Kubernetes Installation (`installation/Kubernetes.md`)

**Current:** 8 lines, spec reference only
**Target:** 200+ lines

**Required Sections:**
1. Prerequisites
   - Kubernetes version requirements
   - kubectl, helm requirements
   - Storage class requirements

2. Helm Chart Installation
   - Add repository
   - Values file configuration
   - Install command with examples

3. Manual Manifest Installation
   - Deployment YAML
   - Service YAML
   - ConfigMap/Secret examples
   - PersistentVolumeClaim

4. Configuration
   - Environment variables
   - Resource limits
   - Persistence options

5. Scaling and HA
   - Replica configuration
   - Load balancing
   - Backup considerations

**Source References:**
- `docs/specifications/beta_requirements/cloud-container/`

---

### 1.5 First Connection (`getting-started/first-connection.md`)

**Current:** 18 lines, minimal
**Target:** 150+ lines

**Required Sections:**
1. Connection Methods Overview
   - Native port (3092)
   - PostgreSQL port (5432)
   - MySQL port (3306)
   - Firebird port (3050)

2. Using sb-isql (Native)
   ```bash
   sb-isql -p 3092 -d scratchbird -u admin
   ```
   - Connection string format
   - Authentication options

3. Using psql (PostgreSQL mode)
   ```bash
   psql -h localhost -p 5432 -U admin -d scratchbird
   ```

4. Using mysql CLI (MySQL mode)
   ```bash
   mysql -h localhost -P 3306 -u admin -p scratchbird
   ```

5. Connection Troubleshooting
   - Common errors
   - Firewall issues
   - Authentication failures

---

### 1.6 Basic SQL (`getting-started/basic-sql.md`)

**Current:** 8 lines, stub
**Target:** 200+ lines

**Required Sections:**
1. Creating a Database
   ```sql
   CREATE DATABASE myapp;
   ```

2. Creating Tables
   - Basic CREATE TABLE
   - Data types overview
   - Constraints (PRIMARY KEY, NOT NULL, UNIQUE)

3. Inserting Data
   - Single row INSERT
   - Multi-row INSERT
   - INSERT with defaults

4. Querying Data
   - SELECT basics
   - WHERE clauses
   - ORDER BY, LIMIT

5. Updating Data
   - UPDATE with WHERE
   - Common patterns

6. Deleting Data
   - DELETE with WHERE
   - TRUNCATE vs DELETE

7. Next Steps
   - Links to language guides
   - Links to tutorials

---

### 1.7 Getting Started Index (`Getting-Started.md`)

**Current:** Exists
**Target:** Index page linking all getting-started content

---

## Phase 2: Tutorials

**Priority:** HIGH
**Items:** 7 files
**Goal:** Provide working code examples for common use cases

### 2.1 First Application (`tutorials/First-Application.md`)

**Current:** 15 lines
**Target:** 300+ lines

**Required Sections:**
1. What We're Building
   - Simple task list application
   - Technologies used

2. Database Schema
   ```sql
   CREATE TABLE tasks (
       id SERIAL PRIMARY KEY,
       title VARCHAR(200) NOT NULL,
       completed BOOLEAN DEFAULT FALSE,
       created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
   );
   ```

3. Application Code (Python example)
   - Connection setup
   - CRUD operations
   - Error handling

4. Running the Application
   - Step-by-step execution
   - Expected output

5. Exercises
   - Add due dates
   - Add categories

---

### 2.2 Flask Web App (`tutorials/Web-App-Python-Flask.md`)

**Current:** 13 lines
**Target:** 400+ lines

**Required Sections:**
1. Project Setup
   - Virtual environment
   - Dependencies (Flask, psycopg2)

2. Database Configuration
   - Connection pooling
   - Environment variables

3. Models and Schema
   - Table definitions
   - Migrations approach

4. Routes Implementation
   - CRUD endpoints
   - Error handling
   - JSON responses

5. Complete Code Listing
   - app.py
   - requirements.txt

6. Running and Testing
   - Development server
   - curl examples

---

### 2.3 Node.js Express App (`tutorials/Web-App-NodeJS-Express.md`)

**Current:** 7 lines
**Target:** 400+ lines

**Required Sections:**
1. Project Setup
   - npm init
   - Dependencies (express, pg)

2. Database Connection
   - Pool configuration
   - Connection string

3. API Implementation
   - Express routes
   - Async/await patterns
   - Error middleware

4. Complete Code
   - index.js
   - package.json

5. Running and Testing

---

### 2.4 Delphi Desktop App (`tutorials/Desktop-App-Delphi.md`)

**Current:** 7 lines
**Target:** 300+ lines

**Required Sections:**
1. Project Setup
   - RAD Studio/Delphi version
   - FireDAC components

2. Connection Configuration
   - TFDConnection setup
   - Driver parameters (PostgreSQL mode)

3. Data Module
   - TFDQuery examples
   - TDataSource binding

4. Form Implementation
   - DBGrid display
   - CRUD buttons

5. Complete Code Units

---

### 2.5 Docker Deployment (`tutorials/Docker-Deployment.md`)

**Current:** 8 lines
**Target:** 250+ lines

**Required Sections:**
1. Single Container
   ```bash
   docker run -d -p 3092:3092 -v sb-data:/var/lib/scratchbird scratchbird:latest
   ```

2. Docker Compose
   - Full compose.yml
   - Environment configuration
   - Volume persistence

3. Multi-Container Setup
   - App + Database
   - Network configuration

4. Production Considerations
   - Resource limits
   - Health checks
   - Logging

**Source References:**
- `docs/specifications/beta_requirements/cloud-container/docker/`

---

### 2.6 REST API Design (`tutorials/REST-API-Design.md`)

**Current:** 8 lines
**Target:** 250+ lines

**Required Sections:**
1. Schema Design for REST
   - Resource tables
   - Relationships

2. Common Patterns
   - Pagination queries
   - Filtering
   - Sorting

3. JSON Responses
   - JSONB columns
   - JSON functions

4. Performance Tips
   - Indexes for API queries
   - Connection pooling

---

### 2.7 Data Migration Project (`tutorials/Data-Migration-Project.md`)

**Current:** 8 lines
**Target:** 300+ lines

**Required Sections:**
1. Migration Planning
   - Schema analysis
   - Data volume assessment

2. Schema Migration
   - DDL translation
   - Data type mapping

3. Data Migration
   - Export from source
   - Import to ScratchBird
   - Verification

4. Application Updates
   - Connection string changes
   - Query adjustments

---

## Phase 3: Admin & Operations

**Priority:** HIGH
**Items:** 6 files
**Goal:** Enable administrators to manage ScratchBird

### 3.1 Backup and Restore (`admin/backup-restore.md`)

**Current:** 9 lines
**Target:** 200+ lines

**Required Sections:**
1. Backup Overview
   - Backup types (full, incremental)
   - Storage requirements

2. Using sb_backup
   ```bash
   sb_backup -d mydb -o /backups/mydb_$(date +%Y%m%d).sbk
   ```
   - Options reference
   - Scheduling with cron

3. Restore Procedures
   ```bash
   sb_restore -i /backups/mydb_20260118.sbk -d mydb_restored
   ```

4. Point-in-Time Recovery
   - WAL archiving
   - Recovery targets

5. Verification
   - Backup integrity checks
   - Test restores

**Source References:**
- `docs/user-documentation/tools/sb-backup.md`
- `wiki/content/cli-tools/sb_backup.md`

---

### 3.2 Monitoring (`admin/monitoring.md`)

**Current:** 9 lines
**Target:** 200+ lines

**Required Sections:**
1. Built-in Monitoring
   - System catalog queries
   - Connection statistics

2. Metrics Export
   - Prometheus endpoint (if available)
   - StatsD integration

3. Log Monitoring
   - Log locations
   - Log levels
   - Log rotation

4. Alerting Setup
   - Key metrics to watch
   - Threshold recommendations

5. Performance Dashboards
   - Grafana examples (if applicable)

---

### 3.3 Security Configuration (`admin/security.md`)

**Current:** 9 lines
**Target:** 250+ lines

**Required Sections:**
1. Authentication
   - Password authentication
   - Certificate authentication
   - External auth (LDAP, if supported)

2. TLS/SSL Configuration
   - Certificate generation
   - Server configuration
   - Client verification

3. Network Security
   - Listen addresses
   - Firewall recommendations
   - Port security

4. Audit Logging
   - Enabling audit logs
   - Log format

**Source References:**
- `docs/user-documentation/tools/sb-security.md`
- `wiki/content/cli-tools/sb_security.md`

---

### 3.4 User Management (`admin/user-management.md`)

**Current:** 9 lines
**Target:** 200+ lines

**Required Sections:**
1. Creating Users
   ```sql
   CREATE USER appuser WITH PASSWORD 'secure_password';
   ```

2. Role Management
   ```sql
   CREATE ROLE readonly;
   GRANT SELECT ON ALL TABLES IN SCHEMA public TO readonly;
   GRANT readonly TO appuser;
   ```

3. Permission Management
   - GRANT/REVOKE syntax
   - Object permissions
   - Schema permissions

4. Password Policies
   - Password requirements
   - Rotation procedures

5. Auditing User Activity

**Source References:**
- `wiki/content/language-guides/native/09_security_dcl.md`

---

### 3.5 Admin Troubleshooting (`admin/troubleshooting.md`)

**Current:** 7 lines
**Target:** 150+ lines

**Required Sections:**
1. Service Won't Start
   - Log analysis
   - Common causes

2. Performance Degradation
   - Diagnostic queries
   - Resource checks

3. Connection Issues
   - Max connections
   - Authentication failures

4. Storage Problems
   - Disk space
   - Corruption detection

---

### 3.6 Admin Index (`admin/README.md`)

**Current:** MISSING
**Target:** Index page for admin section

---

## Phase 4: Migration Guides

**Priority:** HIGH
**Items:** 5 files
**Goal:** Help users migrate from other databases

### 4.1 Migration Overview (`migration/Migration-Overview.md`)

**Current:** 15 lines
**Target:** 150+ lines

**Required Sections:**
1. Migration Approaches
   - Big bang vs. phased
   - Dual-write patterns

2. Compatibility Modes
   - When to use PostgreSQL mode
   - When to use MySQL mode
   - When to use Firebird mode

3. Assessment Checklist
   - Schema complexity
   - Data volume
   - Application changes needed

4. Migration Tools
   - Schema converters
   - Data migration utilities

---

### 4.2 From Firebird (`migration/From-Firebird.md`)

**Current:** 16 lines
**Target:** 300+ lines

**Required Sections:**
1. Why Migrate
   - Feature comparison
   - Performance considerations

2. Schema Translation
   | Firebird | ScratchBird |
   |----------|-------------|
   | GENERATOR | SEQUENCE |
   | BLOB SUB_TYPE TEXT | TEXT |

3. SQL Syntax Changes
   - SELECT FIRST → LIMIT
   - EXECUTE BLOCK differences

4. Stored Procedure Migration
   - PSQL differences
   - Variable declarations

5. Data Migration
   - Using gbak export
   - Import procedures

6. Application Changes
   - Connection string updates
   - Driver changes

**Source References:**
- `wiki/content/language-guides/firebirdsql/`
- `docs/audit/languages/firebird/`

---

### 4.3 From PostgreSQL (`migration/From-PostgreSQL.md`)

**Current:** 12 lines
**Target:** 250+ lines

**Required Sections:**
1. Compatibility Level
   - What works directly
   - What needs changes

2. Schema Considerations
   - Supported types
   - Unsupported features

3. pg_dump Migration
   ```bash
   pg_dump -h old_server mydb | sb-isql -p 5432 -d mydb
   ```

4. Application Changes
   - Usually minimal (same wire protocol)
   - Driver considerations

5. Known Limitations
   - Features not supported
   - Workarounds

---

### 4.4 From MySQL (`migration/From-MySQL.md`)

**Current:** 12 lines
**Target:** 250+ lines

**Required Sections:**
1. Compatibility Level
   - Supported features
   - Unsupported features

2. Schema Translation
   | MySQL | ScratchBird |
   |-------|-------------|
   | AUTO_INCREMENT | SERIAL |
   | TINYINT(1) | BOOLEAN |
   | ENUM | CHECK constraint |

3. mysqldump Migration
   - Export commands
   - Import procedures

4. SQL Syntax Changes
   - Backticks → double quotes
   - LIMIT syntax (compatible)

5. Application Changes
   - Connection updates
   - Query modifications

---

### 4.5 Migration Checklist (`migration/Migration-Checklist.md`)

**Current:** 15 lines
**Target:** 150+ lines

**Required Sections:**
1. Pre-Migration
   - [ ] Schema analysis complete
   - [ ] Data volume assessed
   - [ ] Downtime window planned
   - [ ] Rollback plan documented

2. Schema Migration
   - [ ] Tables created
   - [ ] Indexes created
   - [ ] Constraints verified
   - [ ] Sequences configured

3. Data Migration
   - [ ] Data exported
   - [ ] Data imported
   - [ ] Row counts verified
   - [ ] Data integrity checked

4. Application Migration
   - [ ] Connection strings updated
   - [ ] Queries tested
   - [ ] Performance validated

5. Post-Migration
   - [ ] Old system decommissioned
   - [ ] Monitoring enabled
   - [ ] Documentation updated

---

## Phase 5: User Guides

**Priority:** HIGH
**Items:** 4 files

### 5.1 User Backup/Restore (`user-guides/Backup-Restore.md`)

**Target:** End-user focused (vs admin focus in admin/)

---

### 5.2 Performance Tuning (`user-guides/Performance-Tuning.md`)

**Current:** 11 lines
**Target:** 200+ lines

**Required Sections:**
1. Query Optimization
   - EXPLAIN usage
   - Index selection

2. Configuration Tuning
   - Memory settings
   - Connection limits

3. Schema Optimization
   - Denormalization
   - Partitioning

---

### 5.3 Vector Search (`user-guides/Vector-Search.md`)

**Current:** 12 lines
**Target:** 150+ lines (or note if not implemented)

---

### 5.4 User Guides Index (`user-guides/README.md`)

---

## Phase 6: Language Guide Completion

**Priority:** MEDIUM-HIGH
**Items:** 15 files

See DOCUMENTATION_IMPROVEMENT_TRACKER.md for detailed file list.

**Key Work:**
1. PostgreSQL: Expand SELECT, DML, CTE, system catalog docs
2. MySQL: Clarify stubbed status, expand security/utilities
3. Firebird: Audit and fill gaps
4. All: Document limitations clearly in each topic file

---

## Phase 7: Driver Documentation

**Priority:** MEDIUM
**Items:** 8 files

Each driver doc needs:
1. Installation
2. Connection example
3. Query example
4. Transaction example
5. Error handling
6. Connection pooling

---

## Phase 8: Troubleshooting

**Priority:** MEDIUM
**Items:** 5 files

Focus on common issues with solutions.

---

## Phase 9: Reference Expansion

**Priority:** MEDIUM
**Items:** 4 files

Cross-dialect comparison tables needed.

---

## Implementation Order

```
Week 1: Phase 1 (Installation & Getting Started)
Week 2: Phase 2 (Tutorials - First App, Flask, Express)
Week 3: Phase 2 (Tutorials - Docker, REST, Migration) + Phase 3 (Admin)
Week 4: Phase 4 (Migration Guides)
Week 5: Phase 5 (User Guides) + Phase 6 (Language Guides)
Week 6: Phase 7 (Drivers) + Phase 8 (Troubleshooting)
Week 7: Phase 9 (Reference) + Review/Polish
```

---

## Source Reference Index

| Topic | Primary Sources |
|-------|-----------------|
| Installation | `docs/development/BUILD_INSTRUCTIONS.md`, `CMakeLists.txt` |
| Docker | `docs/specifications/beta_requirements/cloud-container/docker/` |
| Backup | `docs/user-documentation/tools/sb-backup.md` |
| Security | `docs/user-documentation/tools/sb-security.md` |
| CLI Tools | `wiki/content/cli-tools/` |
| Native SQL | `wiki/content/language-guides/native/` |
| PostgreSQL | `wiki/content/language-guides/postgresql/` |
| MySQL | `wiki/content/language-guides/mysql/` |
| Firebird | `wiki/content/language-guides/firebirdsql/` |
| Architecture | `docs/ARCHITECTURAL_LAYERS.md` |
| MGA | `MGA_RULES.md` |

---

## Progress Updates

Updates should be logged in DOCUMENTATION_IMPROVEMENT_TRACKER.md.

