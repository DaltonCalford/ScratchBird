# Migration Checklist

**Last Updated:** 2026-01-30

---

## Overview

This comprehensive checklist helps you track progress through all phases of a database migration to ScratchBird. Use it as a template for your migration project, checking off items as you complete them.

**How to use:**
- Copy this checklist to your project documentation
- Check off items as you complete them
- Add notes in the "Notes" column for your specific situation
- Skip items that don't apply to your migration

---

## Phase 1: Assessment and Planning

### Database Assessment

| Task | Status | Notes |
|------|--------|-------|
| Document source database type and version | [ ] | |
| Inventory all databases to migrate | [ ] | |
| Document total database size | [ ] | |
| Count tables, views, procedures, triggers | [ ] | |
| List all custom data types | [ ] | |
| Identify stored procedures and functions | [ ] | |
| Document triggers and their purposes | [ ] | |
| List foreign keys and constraints | [ ] | |
| Identify indexes (including full-text, spatial) | [ ] | |
| Document partitioned tables | [ ] | |
| List sequences/generators | [ ] | |
| Identify materialized views | [ ] | |
| Document database character sets/encodings | [ ] | |

### Application Assessment

| Task | Status | Notes |
|------|--------|-------|
| Inventory all applications connecting to database | [ ] | |
| Document connection methods for each application | [ ] | |
| List driver libraries and versions | [ ] | |
| Identify connection pooling configurations | [ ] | |
| Document ORM usage (if any) | [ ] | |
| List database-specific SQL in application code | [ ] | |
| Identify stored procedure calls | [ ] | |
| Document transaction patterns | [ ] | |
| List any direct catalog/system table queries | [ ] | |

### Dependency Assessment

| Task | Status | Notes |
|------|--------|-------|
| List database extensions in use | [ ] | |
| Document external dependencies (UDFs, etc.) | [ ] | |
| Identify replication configurations | [ ] | |
| List scheduled jobs/events | [ ] | |
| Document backup procedures | [ ] | |
| Identify monitoring integrations | [ ] | |

### Risk Assessment

| Task | Status | Notes |
|------|--------|-------|
| Identify high-risk tables (large, critical) | [ ] | |
| Document downtime requirements | [ ] | |
| Assess rollback requirements | [ ] | |
| Identify compliance/regulatory constraints | [ ] | |
| Document SLA requirements | [ ] | |
| Plan for data validation approach | [ ] | |

### Planning Documents

| Task | Status | Notes |
|------|--------|-------|
| Create migration project plan | [ ] | |
| Define success criteria | [ ] | |
| Establish go/no-go criteria | [ ] | |
| Create communication plan | [ ] | |
| Schedule migration windows | [ ] | |
| Assign team responsibilities | [ ] | |
| Create rollback plan | [ ] | |
| Document emergency contacts | [ ] | |

---

## Phase 2: Environment Setup

### Infrastructure

| Task | Status | Notes |
|------|--------|-------|
| Provision ScratchBird server(s) | [ ] | |
| Size hardware appropriately (CPU, RAM, disk) | [ ] | |
| Configure network connectivity | [ ] | |
| Set up firewall rules | [ ] | |
| Configure DNS entries (if needed) | [ ] | |
| Set up load balancer (if needed) | [ ] | |

### ScratchBird Installation

| Task | Status | Notes |
|------|--------|-------|
| Install ScratchBird software | [ ] | |
| Configure sb_server.conf | [ ] | |
| Configure sb_hba.conf | [ ] | |
| Set up TLS certificates | [ ] | |
| Configure appropriate ports | [ ] | |
| Start and verify server | [ ] | |
| Create initial admin user | [ ] | |

### Monitoring and Logging

| Task | Status | Notes |
|------|--------|-------|
| Configure logging levels | [ ] | |
| Set up log rotation | [ ] | |
| Install Prometheus metrics exporter | [ ] | |
| Configure Grafana dashboards | [ ] | |
| Set up alerting rules | [ ] | |
| Test monitoring connectivity | [ ] | |

### Backup Configuration

| Task | Status | Notes |
|------|--------|-------|
| Configure backup storage location | [ ] | |
| Set up automated backup schedule | [ ] | |
| Test backup process | [ ] | |
| Document restore procedure | [ ] | |
| Test restore process | [ ] | |

---

## Phase 3: Schema Migration

### Schema Analysis

| Task | Status | Notes |
|------|--------|-------|
| Export schema DDL from source | [ ] | |
| Review data types for compatibility | [ ] | |
| Identify schema transformations needed | [ ] | |
| Document column mapping decisions | [ ] | |
| Plan constraint migration | [ ] | |

### Data Type Conversions

| Task | Status | Notes |
|------|--------|-------|
| Map numeric types | [ ] | |
| Map string/text types | [ ] | |
| Map date/time types | [ ] | |
| Map binary/blob types | [ ] | |
| Map JSON types | [ ] | |
| Map array types | [ ] | |
| Map enum/set types | [ ] | |
| Map custom/composite types | [ ] | |

### Schema Transformation

| Task | Status | Notes |
|------|--------|-------|
| Remove storage engine references | [ ] | |
| Convert auto-increment to SERIAL/GENERATED | [ ] | |
| Transform tablespace references | [ ] | |
| Convert character set specifications | [ ] | |
| Transform index definitions | [ ] | |
| Convert check constraints | [ ] | |
| Transform default values | [ ] | |

### Schema Import

| Task | Status | Notes |
|------|--------|-------|
| Create target database | [ ] | |
| Create schemas if needed | [ ] | |
| Import table definitions | [ ] | |
| Verify table creation | [ ] | |
| Create indexes (after data load) | [ ] | |
| Add foreign key constraints (after data load) | [ ] | |
| Verify constraint creation | [ ] | |

---

## Phase 4: Stored Procedure Migration

### Procedure Analysis

| Task | Status | Notes |
|------|--------|-------|
| Inventory all stored procedures | [ ] | |
| Inventory all functions | [ ] | |
| Document procedure dependencies | [ ] | |
| Identify procedures by complexity | [ ] | |
| Plan conversion priority | [ ] | |

### Procedure Conversion

| Task | Status | Notes |
|------|--------|-------|
| Convert simple procedures | [ ] | |
| Convert procedures with cursors | [ ] | |
| Convert procedures with transactions | [ ] | |
| Convert procedures with exception handling | [ ] | |
| Convert table-returning functions | [ ] | |

### Trigger Migration

| Task | Status | Notes |
|------|--------|-------|
| Inventory all triggers | [ ] | |
| Document trigger purposes | [ ] | |
| Convert BEFORE triggers | [ ] | |
| Convert AFTER triggers | [ ] | |
| Convert INSTEAD OF triggers | [ ] | |
| Verify trigger functionality | [ ] | |

### Testing Procedures

| Task | Status | Notes |
|------|--------|-------|
| Create test cases for each procedure | [ ] | |
| Execute procedure tests | [ ] | |
| Compare results with source | [ ] | |
| Document any behavior differences | [ ] | |

---

## Phase 5: Data Migration

### Data Preparation

| Task | Status | Notes |
|------|--------|-------|
| Calculate data transfer time estimates | [ ] | |
| Plan migration order (dependencies) | [ ] | |
| Identify tables for parallel migration | [ ] | |
| Prepare data validation queries | [ ] | |
| Create row count comparison queries | [ ] | |
| Create checksum validation queries | [ ] | |

### Data Export

| Task | Status | Notes |
|------|--------|-------|
| Export table data (method: _____) | [ ] | |
| Verify export file integrity | [ ] | |
| Document export file sizes | [ ] | |
| Transfer files to target (if needed) | [ ] | |

### Data Import

| Task | Status | Notes |
|------|--------|-------|
| Disable foreign keys (for faster import) | [ ] | |
| Disable triggers (for faster import) | [ ] | |
| Import data to target tables | [ ] | |
| Re-enable foreign keys | [ ] | |
| Re-enable triggers | [ ] | |
| Update sequences to current values | [ ] | |

### Data Validation

| Task | Status | Notes |
|------|--------|-------|
| Compare row counts for all tables | [ ] | |
| Run checksum comparisons | [ ] | |
| Validate sample data manually | [ ] | |
| Test foreign key integrity | [ ] | |
| Verify NULL handling | [ ] | |
| Check date/time conversions | [ ] | |
| Validate character encoding | [ ] | |

---

## Phase 6: Index and Performance

### Index Creation

| Task | Status | Notes |
|------|--------|-------|
| Create primary key indexes | [ ] | |
| Create foreign key indexes | [ ] | |
| Create unique indexes | [ ] | |
| Create composite indexes | [ ] | |
| Create partial indexes | [ ] | |
| Handle full-text index alternatives | [ ] | |

### Statistics and Optimization

| Task | Status | Notes |
|------|--------|-------|
| Run ANALYZE on all tables | [ ] | |
| Update catalog statistics | [ ] | |
| Review query plans for key queries | [ ] | |
| Identify missing indexes | [ ] | |
| Tune configuration parameters | [ ] | |

### Performance Baseline

| Task | Status | Notes |
|------|--------|-------|
| Run benchmark queries | [ ] | |
| Document response times | [ ] | |
| Compare with source database performance | [ ] | |
| Identify performance issues | [ ] | |
| Optimize problematic queries | [ ] | |

---

## Phase 7: Security Configuration

### User Migration

| Task | Status | Notes |
|------|--------|-------|
| Create application users | [ ] | |
| Create administrative users | [ ] | |
| Create read-only users | [ ] | |
| Set password policies | [ ] | |

### Role Configuration

| Task | Status | Notes |
|------|--------|-------|
| Create application roles | [ ] | |
| Create administrative roles | [ ] | |
| Map source permissions to roles | [ ] | |
| Assign users to roles | [ ] | |

### Permission Grants

| Task | Status | Notes |
|------|--------|-------|
| Grant table permissions | [ ] | |
| Grant view permissions | [ ] | |
| Grant procedure/function permissions | [ ] | |
| Grant sequence permissions | [ ] | |
| Verify permission restrictions | [ ] | |

### Security Testing

| Task | Status | Notes |
|------|--------|-------|
| Test authentication for each user | [ ] | |
| Verify permission restrictions work | [ ] | |
| Test TLS connections | [ ] | |
| Verify audit logging | [ ] | |

---

## Phase 8: Application Updates

### Connection String Changes

| Task | Status | Notes |
|------|--------|-------|
| Update application configuration files | [ ] | |
| Update environment variables | [ ] | |
| Update connection pooling settings | [ ] | |
| Update timeout settings | [ ] | |

### Code Changes

| Task | Status | Notes |
|------|--------|-------|
| Update SQL syntax differences | [ ] | |
| Update function calls | [ ] | |
| Update date format strings | [ ] | |
| Update stored procedure calls | [ ] | |
| Update catalog queries | [ ] | |

### ORM Configuration

| Task | Status | Notes |
|------|--------|-------|
| Update ORM dialect settings | [ ] | |
| Regenerate models (if needed) | [ ] | |
| Test ORM-generated queries | [ ] | |

---

## Phase 9: Testing

### Functional Testing

| Task | Status | Notes |
|------|--------|-------|
| Test all CRUD operations | [ ] | |
| Test complex queries | [ ] | |
| Test stored procedures | [ ] | |
| Test triggers | [ ] | |
| Test transactions | [ ] | |
| Test concurrent access | [ ] | |

### Application Testing

| Task | Status | Notes |
|------|--------|-------|
| Run unit tests | [ ] | |
| Run integration tests | [ ] | |
| Execute end-to-end tests | [ ] | |
| Test all user workflows | [ ] | |
| Test error handling | [ ] | |
| Test edge cases | [ ] | |

### Performance Testing

| Task | Status | Notes |
|------|--------|-------|
| Run load tests | [ ] | |
| Run stress tests | [ ] | |
| Verify response time requirements | [ ] | |
| Test connection pooling under load | [ ] | |
| Monitor resource utilization | [ ] | |

### Regression Testing

| Task | Status | Notes |
|------|--------|-------|
| Compare query results with source | [ ] | |
| Verify calculation accuracy | [ ] | |
| Test report generation | [ ] | |
| Validate data exports | [ ] | |

---

## Phase 10: Cutover

### Pre-Cutover

| Task | Status | Notes |
|------|--------|-------|
| Notify stakeholders of cutover schedule | [ ] | |
| Prepare rollback scripts | [ ] | |
| Take final source database backup | [ ] | |
| Verify ScratchBird backup is current | [ ] | |
| Confirm team availability | [ ] | |
| Review go/no-go criteria | [ ] | |

### Cutover Execution

| Task | Status | Notes |
|------|--------|-------|
| Start maintenance window | [ ] | |
| Stop applications | [ ] | |
| Final data sync (if using parallel operation) | [ ] | |
| Update application configurations | [ ] | |
| Restart applications | [ ] | |
| Verify applications connect to ScratchBird | [ ] | |
| Run smoke tests | [ ] | |

### Post-Cutover

| Task | Status | Notes |
|------|--------|-------|
| Monitor error logs | [ ] | |
| Monitor performance metrics | [ ] | |
| Check alerting systems | [ ] | |
| Verify data integrity | [ ] | |
| Announce cutover completion | [ ] | |

### Rollback (if needed)

| Task | Status | Notes |
|------|--------|-------|
| Stop applications | [ ] | |
| Revert application configurations | [ ] | |
| Restart applications | [ ] | |
| Verify connection to source database | [ ] | |
| Document rollback reason | [ ] | |
| Plan remediation | [ ] | |

---

## Phase 11: Post-Migration

### First 24 Hours

| Task | Status | Notes |
|------|--------|-------|
| Monitor for errors | [ ] | |
| Check performance metrics | [ ] | |
| Verify backup jobs running | [ ] | |
| Address any issues | [ ] | |
| Keep rollback option available | [ ] | |

### First Week

| Task | Status | Notes |
|------|--------|-------|
| Continue monitoring | [ ] | |
| Optimize slow queries | [ ] | |
| Fine-tune configuration | [ ] | |
| Gather user feedback | [ ] | |
| Document any issues | [ ] | |

### First Month

| Task | Status | Notes |
|------|--------|-------|
| Decommission source database (when ready) | [ ] | |
| Update documentation | [ ] | |
| Conduct lessons-learned session | [ ] | |
| Archive migration artifacts | [ ] | |
| Close migration project | [ ] | |

---

## Quick Reference: Validation Queries

### Row Count Comparison

```sql
-- Run on both source and target, compare results
SELECT 'customers' AS table_name, COUNT(*) FROM customers
UNION ALL SELECT 'orders', COUNT(*) FROM orders
UNION ALL SELECT 'products', COUNT(*) FROM products
UNION ALL SELECT 'order_items', COUNT(*) FROM order_items;
```

### Data Checksum

```sql
-- Run on both databases, compare results
SELECT
    COUNT(*) AS row_count,
    SUM(id) AS id_sum,
    COUNT(DISTINCT email) AS unique_emails,
    MIN(created_at) AS earliest,
    MAX(created_at) AS latest
FROM customers;
```

### Constraint Verification

```sql
-- Check foreign key constraints
SELECT
    tc.constraint_name,
    tc.table_name,
    kcu.column_name,
    ccu.table_name AS foreign_table_name,
    ccu.column_name AS foreign_column_name
FROM information_schema.table_constraints AS tc
JOIN information_schema.key_column_usage AS kcu
    ON tc.constraint_name = kcu.constraint_name
JOIN information_schema.constraint_column_usage AS ccu
    ON ccu.constraint_name = tc.constraint_name
WHERE tc.constraint_type = 'FOREIGN KEY';
```

### Index Verification

```sql
-- List all indexes
SELECT
    schemaname,
    tablename,
    indexname,
    indexdef
FROM pg_indexes
WHERE schemaname = 'public'
ORDER BY tablename, indexname;
```

---

## Migration Sign-Off

### Stakeholder Approval

| Role | Name | Approval | Date |
|------|------|----------|------|
| Project Manager | | [ ] | |
| Database Administrator | | [ ] | |
| Application Owner | | [ ] | |
| Security Officer | | [ ] | |
| Operations Manager | | [ ] | |

### Final Checklist

| Item | Status |
|------|--------|
| All data migrated and validated | [ ] |
| All applications tested and working | [ ] |
| Performance meets requirements | [ ] |
| Security configured and verified | [ ] |
| Backup/recovery tested | [ ] |
| Monitoring in place | [ ] |
| Documentation updated | [ ] |
| Team trained on ScratchBird | [ ] |
| Rollback plan documented | [ ] |
| Go-live approved | [ ] |

---

## See Also

- [Migration Overview](Migration-Overview.md)
- [Migrating from Firebird](From-Firebird.md)
- [Migrating from PostgreSQL](From-PostgreSQL.md)
- [Migrating from MySQL](From-MySQL.md)
- [Administration Guide](../admin/)
- [Troubleshooting](../troubleshooting/)

