# Phase 20: Backup and Restore

## Objective
Implement backup and restore functionality.

## Prerequisites
- Phase 19 complete (network server)

## Tasks

### 20.1 Logical Backup
```cpp
class LogicalBackup {
    void dump_database(string filename);
    void dump_table(string table, string filename);
    void restore(string filename);
};
```

Output format:
- SQL statements
- CREATE TABLE/INDEX statements
- INSERT statements with data
- Constraints and permissions

### 20.2 Physical Backup
```cpp
class PhysicalBackup {
    void backup_database(string backup_dir);
    void restore_database(string backup_dir);
    void verify_backup(string backup_dir);
};
```

### 20.3 Online Backup
- Backup while database is running
- Consistent snapshot
- Minimal performance impact

### 20.4 Incremental Backup
- Track changes since last backup
- Use WAL for incremental data
- Merge incremental into base

### 20.5 Point-in-Time Recovery
- Restore to specific timestamp
- Apply WAL up to target time
- Verify consistency

## Files to Create/Modify
- `include/scratchbird/backup.h`
- `src/backup/logical_backup.cpp`
- `src/backup/physical_backup.cpp`
- `src/backup/pitr.cpp`

## Validation Tests
```cpp
// Logical backup/restore
execute("CREATE TABLE test (id INTEGER, data TEXT)");
execute("INSERT INTO test VALUES (1, 'test')");

backup.dump_database("backup.sql");
execute("DROP TABLE test");

backup.restore("backup.sql");
auto result = execute("SELECT * FROM test");
assert(result.rows.size() == 1);

// Physical backup
physical_backup.backup_database("/backup/full");
// Corrupt database
corrupt_data_file();

physical_backup.restore_database("/backup/full");
result = execute("SELECT * FROM test");
assert(result.rows.size() == 1);

// PITR
auto backup_time = now();
execute("INSERT INTO test VALUES (2, 'after backup')");
auto target_time = now();
execute("INSERT INTO test VALUES (3, 'too late')");

pitr.restore_to_time(backup_time, target_time);
result = execute("SELECT COUNT(*) FROM test");
assert(result.rows[0][0] == "2");  // Row 3 not restored
```

## Exit Criteria
- Logical backup creates valid SQL
- Physical backup preserves all data
- Online backup doesn't block operations
- PITR restores to exact point