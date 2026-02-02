# Migration Overview

**Last Updated:** 2026-01-30

---

## Overview

This guide helps you plan and execute a successful migration to ScratchBird from other database systems. ScratchBird's multi-dialect emulation makes migration straightforward—in most cases, your existing applications can connect to ScratchBird using their current database drivers with minimal or no code changes.

**Topics covered:**
- Understanding ScratchBird's architecture
- Choosing a migration strategy
- Migration paths by source database
- Planning your migration
- Risk assessment and mitigation

---

## Part 1: Understanding ScratchBird Architecture

### Multi-Dialect Emulation

ScratchBird is a unique database that combines:

1. **Firebird-style Multi-Generational Architecture (MGA)** - The storage engine
2. **Multi-protocol support** - Native, PostgreSQL, MySQL, and Firebird wire protocols
3. **SQL dialect emulation** - Parse and execute SQL from multiple database systems

```
┌─────────────────────────────────────────────────────────────────┐
│                        Applications                              │
├───────────────┬───────────────┬───────────────┬─────────────────┤
│   PostgreSQL  │     MySQL     │   Firebird    │     Native      │
│    Clients    │    Clients    │    Clients    │    Clients      │
├───────────────┴───────────────┴───────────────┴─────────────────┤
│              ScratchBird Protocol Layer                          │
│   (PostgreSQL:5432, MySQL:3306, Firebird:3050, Native:3092)     │
├─────────────────────────────────────────────────────────────────┤
│              SQL Parser Layer (Dialect Emulation)                │
│         PostgreSQL SQL → SBLR ← MySQL SQL ← Firebird SQL        │
├─────────────────────────────────────────────────────────────────┤
│              SBLR Bytecode Executor                              │
├─────────────────────────────────────────────────────────────────┤
│              MGA Storage Engine                                  │
│         (Firebird-style Multi-Generational Architecture)        │
└─────────────────────────────────────────────────────────────────┘
```

### What This Means for Migration

**Key benefits:**
- **No driver changes** - Use existing PostgreSQL, MySQL, or Firebird drivers
- **Same SQL syntax** - Write SQL in your familiar dialect
- **Gradual migration** - Move applications one at a time
- **Single database** - All dialects access the same underlying data

**Important limitations:**
- Some dialect-specific features may have limited support
- Stored procedures use SBLR, not native PL/pgSQL or MySQL procedures
- Some system catalog views return ScratchBird equivalents
- Replication uses ScratchBird's native replication, not source DB replication

---

## Part 2: Migration Strategies

### Strategy 1: Direct Migration (Recommended for Most Cases)

**Best for:** New deployments, applications you control, standard SQL usage

```
┌──────────────┐    Export    ┌──────────────┐    Import    ┌──────────────┐
│   Source DB  │ ──────────→ │   SQL Files  │ ──────────→ │ ScratchBird  │
│  (PostgreSQL │    Schema    │   (DDL/DML)  │   Transform │              │
│   MySQL,     │    + Data    │              │   + Load    │              │
│   Firebird)  │              │              │             │              │
└──────────────┘              └──────────────┘             └──────────────┘
```

**Steps:**
1. Export schema and data from source database
2. Transform SQL if needed (usually minimal)
3. Create schema in ScratchBird
4. Load data
5. Validate
6. Switch applications

**Pros:**
- Clean break from old system
- Full control over migration process
- Opportunity to optimize schema

**Cons:**
- Requires downtime for cutover
- Need to coordinate application changes
- Data validation required

### Strategy 2: Parallel Operation

**Best for:** Mission-critical systems, zero-downtime requirements

```
┌──────────────┐              ┌──────────────┐
│   Source DB  │ ←──────────→ │ ScratchBird  │
│   (Primary)  │    Sync      │  (Secondary) │
└──────────────┘              └──────────────┘
       ↑                             ↑
       │         Application         │
       └──────────────┬──────────────┘
                      │
                 Dual Write
```

**Steps:**
1. Set up ScratchBird alongside source database
2. Initial data sync
3. Implement dual-write in application
4. Validate data consistency
5. Gradually shift read traffic
6. Complete cutover

**Pros:**
- Zero downtime possible
- Easy rollback
- Gradual validation

**Cons:**
- More complex implementation
- Dual-write requires application changes
- Temporary increased infrastructure cost

### Strategy 3: Incremental Migration

**Best for:** Large databases, microservices, multi-tenant systems

```
Phase 1: Service A ──────→ ScratchBird
Phase 2: Service B ──────→ ScratchBird
Phase 3: Service C ──────→ ScratchBird
...
Final:   Legacy DB ────X   (Decommission)
```

**Steps:**
1. Identify service boundaries
2. Migrate one service/tenant at a time
3. Keep cross-service queries working via federation
4. Continue until complete
5. Decommission source database

**Pros:**
- Manageable scope per phase
- Risk contained to single service
- Learn and improve with each phase

**Cons:**
- Longer overall timeline
- May need cross-database queries during transition
- Complex coordination

---

## Part 3: Migration Decision Framework

### Quick Decision Guide

```
┌─────────────────────────────────────────────────────────────────┐
│                   Do you need zero downtime?                     │
└─────────────────────────────────────────────────────────────────┘
                    │                          │
                   YES                         NO
                    │                          │
                    ▼                          ▼
    ┌───────────────────────┐    ┌───────────────────────────────┐
    │ Is your database      │    │ Is your database > 100GB?     │
    │ > 1TB?                │    └───────────────────────────────┘
    └───────────────────────┘              │               │
          │           │                   YES              NO
         YES          NO                   │               │
          │           │                    ▼               ▼
          ▼           ▼           ┌────────────┐  ┌────────────────┐
    ┌──────────┐ ┌──────────┐    │Incremental │  │Direct Migration│
    │Incremental│ │ Parallel │    │ Migration  │  │ (Recommended)  │
    │ Migration │ │Operation │    └────────────┘  └────────────────┘
    └──────────┘ └──────────┘
```

### Choosing by Source Database

| Source Database | Recommended Strategy | Complexity | Typical Duration |
|-----------------|---------------------|------------|------------------|
| PostgreSQL | Direct Migration | Low | Days to weeks |
| MySQL | Direct Migration | Low-Medium | Days to weeks |
| Firebird | Direct Migration | Low | Days |
| Oracle | Incremental | High | Weeks to months |
| SQL Server | Incremental | Medium-High | Weeks to months |
| SQLite | Direct Migration | Very Low | Hours |

### Factors to Consider

**Database size:**
| Size | Recommendation |
|------|----------------|
| < 10 GB | Direct migration, any method |
| 10-100 GB | Direct migration with parallel validation |
| 100 GB - 1 TB | Incremental or parallel operation |
| > 1 TB | Incremental with dedicated migration plan |

**Application complexity:**
| Scenario | Recommendation |
|----------|----------------|
| Single application | Direct migration |
| Multiple independent apps | Incremental by application |
| Tightly coupled services | Parallel operation |
| Microservices | Incremental by service |

**Downtime tolerance:**
| Tolerance | Strategy |
|-----------|----------|
| Hours acceptable | Direct migration |
| Minutes only | Parallel with fast cutover |
| Zero downtime | Parallel operation with gradual cutover |

---

## Part 4: Migration Paths by Source Database

### From PostgreSQL

**Compatibility level:** High

PostgreSQL applications typically work with minimal changes:

```bash
# Export from PostgreSQL
pg_dump -h source-host -U user -d mydb -F p -f export.sql

# Import to ScratchBird (using PostgreSQL protocol)
psql -h scratchbird-host -p 5432 -U admin -d mydb -f export.sql
```

**What works automatically:**
- Standard SQL (SELECT, INSERT, UPDATE, DELETE)
- Common data types (INTEGER, VARCHAR, TEXT, TIMESTAMP, etc.)
- Indexes, constraints, foreign keys
- Basic transactions
- Most built-in functions

**What needs attention:**
- PL/pgSQL stored procedures → Convert to SBLR
- PostgreSQL-specific extensions
- Some system catalog queries
- Replication setup

**See:** [Migrating from PostgreSQL](From-PostgreSQL.md)

### From MySQL

**Compatibility level:** High

MySQL applications typically work with minimal changes:

```bash
# Export from MySQL
mysqldump -h source-host -u user -p mydb > export.sql

# Import to ScratchBird (using MySQL protocol)
mysql -h scratchbird-host -P 3306 -u admin -p mydb < export.sql
```

**What works automatically:**
- Standard SQL statements
- Common data types
- AUTO_INCREMENT columns
- Indexes and constraints
- Basic transactions

**What needs attention:**
- MySQL-specific functions
- Storage engine hints (InnoDB, MyISAM)
- MySQL stored procedures
- FULLTEXT indexes (use ScratchBird alternatives)

**See:** [Migrating from MySQL](From-MySQL.md)

### From Firebird

**Compatibility level:** Very High

Firebird is the most compatible source due to shared MGA heritage:

```bash
# Export from Firebird
gbak -b -user SYSDBA -password masterkey source.fdb backup.fbk

# Convert and import to ScratchBird
sb_restore --from-firebird backup.fbk -d mydb
```

**What works automatically:**
- Nearly all SQL syntax
- Generators/sequences
- Domains
- Stored procedures (with minor adjustments)
- Triggers
- Character sets

**What needs attention:**
- External tables
- UDFs (User Defined Functions)
- GBAK-specific features

**See:** [Migrating from Firebird](From-Firebird.md)

### From Other Databases

For Oracle, SQL Server, or other databases:

1. **Export to standard SQL** using native tools or third-party converters
2. **Transform SQL** to PostgreSQL or MySQL dialect
3. **Import via** appropriate ScratchBird protocol
4. **Test thoroughly** for dialect-specific issues

---

## Part 5: Planning Your Migration

### Pre-Migration Checklist

**1. Assessment (Week 1-2)**
- [ ] Inventory all databases and sizes
- [ ] Document applications and connection methods
- [ ] Identify stored procedures and triggers
- [ ] List custom functions and extensions
- [ ] Map data types to ScratchBird equivalents
- [ ] Estimate data transfer time

**2. Environment Setup (Week 2-3)**
- [ ] Install ScratchBird on target infrastructure
- [ ] Configure authentication and TLS
- [ ] Set up monitoring
- [ ] Create test databases
- [ ] Configure backup procedures

**3. Schema Migration (Week 3-4)**
- [ ] Export and transform schema DDL
- [ ] Create schema in ScratchBird
- [ ] Migrate stored procedures to SBLR
- [ ] Create indexes and constraints
- [ ] Validate schema matches source

**4. Data Migration (Week 4-5)**
- [ ] Plan data transfer method
- [ ] Execute initial data load
- [ ] Verify row counts
- [ ] Validate data integrity
- [ ] Test queries against migrated data

**5. Application Testing (Week 5-6)**
- [ ] Update connection strings to ScratchBird
- [ ] Run integration tests
- [ ] Performance testing
- [ ] Load testing
- [ ] Fix any compatibility issues

**6. Cutover Planning (Week 6-7)**
- [ ] Document cutover procedure
- [ ] Plan rollback procedure
- [ ] Schedule maintenance window
- [ ] Notify stakeholders
- [ ] Prepare monitoring dashboards

### Migration Timeline Template

```
Week 1-2:  Assessment & Planning
           ├── Database inventory
           ├── Application mapping
           └── Risk assessment

Week 2-3:  Environment Setup
           ├── ScratchBird installation
           ├── Security configuration
           └── Monitoring setup

Week 3-4:  Schema Migration
           ├── DDL export/transform
           ├── Schema creation
           └── Stored procedure conversion

Week 4-5:  Data Migration
           ├── Initial data load
           ├── Data validation
           └── Delta sync (if parallel)

Week 5-6:  Testing
           ├── Application testing
           ├── Performance testing
           └── Bug fixes

Week 6-7:  Cutover
           ├── Final sync
           ├── Application switch
           └── Monitoring & validation

Week 7+:   Stabilization
           ├── Performance tuning
           ├── Issue resolution
           └── Documentation
```

---

## Part 6: Risk Assessment

### Common Risks and Mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Data loss during migration | Low | Critical | Multiple backups, validation checksums |
| Application incompatibility | Medium | High | Thorough testing, gradual rollout |
| Performance degradation | Medium | Medium | Performance testing, query optimization |
| Extended downtime | Medium | High | Parallel operation, automation |
| Rollback needed | Low | Medium | Maintain source DB, tested rollback plan |
| Missing features | Low | Medium | Feature mapping, workarounds documented |

### Rollback Planning

**Always have a rollback plan:**

```bash
# Before cutover
# 1. Take final backup of source database
pg_dump -h source-host -U user -d mydb -F c -f pre-cutover.dump

# 2. Document current connection strings
# 3. Keep source database running (read-only if possible)

# If rollback needed
# 1. Stop applications
# 2. Revert connection strings to source database
# 3. Start applications
# 4. Analyze what went wrong
```

### Go/No-Go Criteria

**Go criteria (all must be true):**
- [ ] All critical application tests pass
- [ ] Data validation shows 100% accuracy
- [ ] Performance meets or exceeds baseline
- [ ] Rollback procedure tested
- [ ] Team and stakeholders aligned
- [ ] Monitoring in place

**No-Go triggers:**
- Critical test failures
- Data integrity issues
- Unacceptable performance
- Missing rollback capability
- Team concerns unaddressed

---

## Part 7: Post-Migration Tasks

### Immediate (First 24 Hours)

```sql
-- Verify database health
SELECT
    'Connections' AS metric,
    COUNT(*)::text AS value
FROM pg_stat_activity
UNION ALL
SELECT
    'Database Size',
    pg_size_pretty(pg_database_size(current_database()));

-- Check for errors in logs
-- Monitor application error rates
-- Verify backup jobs running
```

### First Week

- Monitor query performance
- Address any slow queries
- Fine-tune configuration
- Document any issues encountered
- Gather user feedback

### First Month

- Optimize based on real workload
- Decommission source database (if applicable)
- Update documentation
- Conduct lessons-learned session
- Plan for future optimizations

---

## Part 8: Quick Reference

### Migration Commands by Source

**PostgreSQL:**
```bash
# Export
pg_dump -F p -f export.sql mydb

# Import via PostgreSQL protocol
psql -h localhost -p 5432 -d mydb -f export.sql
```

**MySQL:**
```bash
# Export
mysqldump mydb > export.sql

# Import via MySQL protocol
mysql -h localhost -P 3306 -d mydb < export.sql
```

**Firebird:**
```bash
# Export
gbak -b database.fdb backup.fbk

# Import with conversion
sb_restore --from-firebird backup.fbk -d mydb
```

### Connection String Changes

**PostgreSQL applications:**
```
# Before
host=pg-server port=5432 dbname=mydb

# After (just change host)
host=scratchbird-server port=5432 dbname=mydb
```

**MySQL applications:**
```
# Before
mysql://user:pass@mysql-server:3306/mydb

# After (just change host)
mysql://user:pass@scratchbird-server:3306/mydb
```

**Firebird applications:**
```
# Before
firebird://user:pass@fb-server:3050/database.fdb

# After
firebird://user:pass@scratchbird-server:3050/mydb
```

---

## Detailed Guides

For step-by-step migration instructions, see the source-specific guides:

- **[Migrating from Firebird](From-Firebird.md)** - Highest compatibility, minimal changes
- **[Migrating from PostgreSQL](From-PostgreSQL.md)** - Common path, well-tested
- **[Migrating from MySQL](From-MySQL.md)** - Good compatibility, some transforms needed
- **[Migration Checklist](Migration-Checklist.md)** - Comprehensive task tracking

---

## Getting Help

- **Documentation:** Browse the [wiki home](../Home.md)
- **Installation:** [Install ScratchBird](../installation/)
- **Tutorials:** [Data Migration Project](../tutorials/Data-Migration-Project.md) - Hands-on tutorial
- **Troubleshooting:** [Connection Problems](../troubleshooting/Connection-Problems.md)

