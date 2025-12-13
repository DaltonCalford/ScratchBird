# sb_verify

Database verification utility.

[Back to Tools Index](index.md) | [Back to Documentation Index](../index.md)

---

## Synopsis

```
sb_verify <database> [OPTIONS]
```

---

## Description

`sb_verify` checks database integrity by validating page structures, indexes, constraints, and data consistency. Use it for routine health checks and after suspected corruption.

---

## Options

| Option | Description |
|--------|-------------|
| `--all` | Run all verification checks |
| `--pages` | Verify page structures |
| `--indexes` | Verify index integrity |
| `--constraints` | Verify constraints (FK, CHECK) |
| `--checksums` | Verify page checksums |
| `--orphans` | Find orphaned records |
| `--verbose` | Detailed output |
| `--quiet` | Errors only |
| `--repair` | Attempt automatic repair |

---

## Connection Options

| Option | Description |
|--------|-------------|
| `-H, --host HOST` | Server hostname |
| `-P, --port PORT` | Server port |
| `-U, --user USER` | Username |
| `-p, --password` | Prompt for password |

---

## Usage Examples

### Full Verification

```bash
sb_verify mydb --all
```

Output:
```
Verifying database: mydb
  Pages: OK (12,456 pages checked)
  Indexes: OK (42 indexes verified)
  Constraints: OK (18 constraints checked)
  Checksums: OK
  Orphans: None found

Database verification complete. No errors found.
```

### Quick Check (Pages Only)

```bash
sb_verify mydb --pages
```

### Index Verification

```bash
sb_verify mydb --indexes --verbose
```

Output:
```
Verifying indexes...
  idx_users_email: OK (15,234 entries)
  idx_orders_customer: OK (45,678 entries)
  idx_products_category: OK (1,234 entries)
  ...
All 42 indexes verified successfully.
```

### Constraint Check

```bash
sb_verify mydb --constraints
```

Output:
```
Verifying constraints...
  FK: orders.customer_id -> users.id: OK
  FK: order_items.order_id -> orders.id: OK
  CHECK: orders.total >= 0: OK
  ...
All 18 constraints verified successfully.
```

---

## Verification Types

### Page Verification (--pages)

Checks:
- Page header validity
- Page type consistency
- Free space tracking
- Page linkage

### Index Verification (--indexes)

Checks:
- B-tree structure
- Key ordering
- Leaf-to-heap references
- Duplicate detection (unique indexes)

### Constraint Verification (--constraints)

Checks:
- Foreign key references
- CHECK constraints
- NOT NULL constraints
- UNIQUE constraints

### Checksum Verification (--checksums)

Checks:
- Page checksum validity
- Detects bit rot / corruption

### Orphan Detection (--orphans)

Finds:
- Unreferenced heap tuples
- Orphaned index entries
- Dangling foreign keys

---

## Interpreting Results

### Success

```
Database verification complete. No errors found.
```

### Warnings

```
WARNING: Index idx_users_email has 3 entries without heap references
  Consider: REINDEX INDEX idx_users_email;
```

### Errors

```
ERROR: Page 1234 has invalid checksum
  Expected: 0xABCD1234
  Found: 0xDEAD0000
  This indicates data corruption.
```

---

## Repair Mode

```bash
sb_verify mydb --all --repair
```

Automatic repairs:
- Rebuild corrupt indexes
- Remove orphaned entries
- Fix free space maps

**Caution:** Always backup before repair!

```bash
# Safe repair workflow
sb_backup create mydb /backup/before_repair.sbdb
sb_verify mydb --all --repair
sb_verify mydb --all  # Verify repair worked
```

---

## Scheduled Verification

### Weekly Health Check

```bash
#!/bin/bash
# /opt/scripts/weekly_verify.sh

DATABASE=mydb
LOG=/var/log/scratchbird/verify.log

echo "=== Verification $(date) ===" >> $LOG
sb_verify $DATABASE --all >> $LOG 2>&1

if [ $? -ne 0 ]; then
    echo "ALERT: Database verification failed" | \
        mail -s "Database Alert" admin@example.com
fi
```

Cron:
```bash
0 3 * * 0 /opt/scripts/weekly_verify.sh
```

---

## Verify Specific Tables

```bash
# Verify single table (via index check)
sb_verify mydb --indexes --verbose 2>&1 | grep -A1 "users"
```

---

## Performance Impact

| Check | Impact | Duration |
|-------|--------|----------|
| `--pages` | Low | Fast |
| `--checksums` | Low | Fast |
| `--indexes` | Medium | Moderate |
| `--constraints` | High | Slow (large tables) |
| `--orphans` | High | Slow |
| `--all` | High | Depends on size |

**Recommendation:** Run full verification during maintenance windows.

---

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | All checks passed |
| 1 | Warnings found |
| 2 | Errors found |
| 3 | Connection error |
| 4 | Repair failed |

---

## Troubleshooting

### "Connection refused"

```bash
# Ensure server is running
systemctl status scratchbird
```

### "Permission denied"

```bash
# Use admin user
sb_verify -U admin mydb --all
```

### "Verification taking too long"

```bash
# Run specific checks only
sb_verify mydb --pages --checksums

# Or during off-hours
```

---

## Post-Crash Verification

After unexpected shutdown:

```bash
# Quick check
sb_verify mydb --pages --checksums

# If issues found, full check
sb_verify mydb --all

# If corruption, repair
sb_verify mydb --all --repair
```

---

## Best Practices

1. **Regular verification** - Weekly for production
2. **After crashes** - Always verify after unexpected shutdown
3. **Before backups** - Verify to avoid backing up corruption
4. **After restore** - Verify restored data integrity
5. **Monitor trends** - Track verification results over time

---

## See Also

- [sb_backup](sb-backup.md)
- [Troubleshooting](../admin/troubleshooting.md)
- [Monitoring](../admin/monitoring.md)
