# Alpha 1: Command-Line Tools & Views Completion Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Created:** November 21, 2025
**Status:** Not Started (~20% of Alpha 1 remaining)
**Priority:** MEDIUM to HIGH
**Dependencies:** PSQL/Triggers for full functionality

---

## Part A: Command-Line Tools

### Overview

Implement essential command-line tools for database management:
1. **sb_isql** - Interactive SQL shell (like psql, mysql)
2. **sb_verify** - Database integrity checker
3. **sb_backup** - Backup and restore tool
4. **sb_security** - User/role management tool

---

## Tool 1: sb_isql (Interactive SQL Shell)

### Overview

Interactive SQL command-line interface for ScratchBird database, similar to `psql` (PostgreSQL) or `mysql` client.

### Features

#### Core Features
1. **Interactive Mode:**
   - Read-eval-print loop (REPL)
   - SQL statement execution
   - Result set display (formatted table)
   - Error message display

2. **Batch Mode:**
   - Execute SQL from file (`sb_isql < script.sql`)
   - Command-line SQL (`sb_isql -c "SELECT * FROM users"`)
   - Quiet mode for scripting

3. **Connection Management:**
   - Database file path
   - User authentication
   - Connection pooling (optional)

4. **Meta-Commands:**
   - `\d` - List tables
   - `\d table` - Describe table
   - `\dt` - List tables
   - `\di` - List indexes
   - `\du` - List users
   - `\l` - List databases
   - `\c database` - Connect to database
   - `\q` - Quit
   - `\?` - Help
   - `\i filename` - Execute SQL file
   - `\o filename` - Output to file

5. **Display Options:**
   - Table format (default)
   - CSV format
   - JSON format
   - Vertical format (\\x)
   - Timing (show query execution time)

### Implementation

#### Task 1.1: Core REPL

**File:** `tools/sb_isql/main.cpp`
**Estimated Lines:** ~400

**Dependencies:**
- libreadline or libedit (for line editing and history)
- ScratchBird database library

**Implementation:**
```cpp
class ISQLShell {
    Database* db;
    bool interactive_mode;
    bool timing_enabled;
    OutputFormat output_format;

    void runInteractive();
    void runBatch(const std::string& sql);
    void executeSQL(const std::string& sql);
    void displayResult(const ResultSet& result);
    void handleMetaCommand(const std::string& cmd);
};

void ISQLShell::runInteractive() {
    while (true) {
        // Read line with readline (history support)
        char* line = readline("scratchbird=> ");
        if (!line) break;  // EOF

        std::string sql(line);
        add_history(line);
        free(line);

        // Check for meta-command
        if (sql.starts_with("\\")) {
            handleMetaCommand(sql);
            continue;
        }

        // Execute SQL
        executeSQL(sql);
    }
}

void ISQLShell::executeSQL(const std::string& sql) {
    auto start = std::chrono::steady_clock::now();

    ResultSet result;
    Status status = db->execute(sql, result);

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (status == Status::OK) {
        displayResult(result);

        if (timing_enabled) {
            std::cout << "Time: " << duration.count() << " ms\n";
        }
    } else {
        std::cerr << "ERROR: " << status.message() << "\n";
    }
}
```

**Testing:**
- Interactive mode with multiple queries
- Batch mode with SQL file
- Meta-command execution
- Error handling
- History navigation

---

#### Task 1.2: Result Display Formatting

**File:** `tools/sb_isql/formatter.cpp`
**Estimated Lines:** ~300

**Formats:**
1. **Table Format (default):**
```
 id | name  | age
----+-------+-----
  1 | Alice |  30
  2 | Bob   |  25
(2 rows)
```

2. **CSV Format:**
```
id,name,age
1,Alice,30
2,Bob,25
```

3. **JSON Format:**
```json
[
  {"id": 1, "name": "Alice", "age": 30},
  {"id": 2, "name": "Bob", "age": 25}
]
```

4. **Vertical Format:**
```
-[ RECORD 1 ]-
id   | 1
name | Alice
age  | 30
-[ RECORD 2 ]-
id   | 2
name | Bob
age  | 25
```

**Implementation:**
```cpp
class ResultFormatter {
    OutputFormat format;

    void formatTable(const ResultSet& result, std::ostream& out);
    void formatCSV(const ResultSet& result, std::ostream& out);
    void formatJSON(const ResultSet& result, std::ostream& out);
    void formatVertical(const ResultSet& result, std::ostream& out);

    // Helper: calculate column widths for table format
    std::vector<size_t> calculateColumnWidths(const ResultSet& result);
};
```

**Testing:**
- Table format with various column widths
- CSV format escaping
- JSON format with special characters
- Vertical format for wide tables

---

#### Task 1.3: Meta-Commands

**File:** `tools/sb_isql/meta_commands.cpp`
**Estimated Lines:** ~250

**Commands:**
- `\d` - Call `SHOW TABLES`
- `\d table` - Call `DESCRIBE table`
- `\dt` - Call `SHOW TABLES`
- `\di` - Call `SHOW INDEXES`
- `\du` - Call `SELECT * FROM sb_users`
- `\l` - Call `SHOW DATABASES`
- `\i filename` - Read and execute SQL from file
- `\o filename` - Redirect output to file
- `\x` - Toggle vertical format
- `\timing` - Toggle query timing

**Implementation:**
```cpp
void ISQLShell::handleMetaCommand(const std::string& cmd) {
    if (cmd == "\\d") {
        executeSQL("SHOW TABLES");
    } else if (cmd.starts_with("\\d ")) {
        std::string table = cmd.substr(3);
        executeSQL("DESCRIBE " + table);
    } else if (cmd == "\\timing") {
        timing_enabled = !timing_enabled;
        std::cout << "Timing is " << (timing_enabled ? "on" : "off") << "\n";
    } else if (cmd.starts_with("\\i ")) {
        std::string filename = cmd.substr(3);
        executeFile(filename);
    } else {
        std::cerr << "Unknown command: " << cmd << "\n";
    }
}
```

**Testing:**
- All meta-commands
- File execution
- Output redirection
- Toggle options

---

#### Task 1.4: Build System Integration

**File:** `tools/sb_isql/CMakeLists.txt`
**Estimated Lines:** ~40

**Steps:**
1. Create CMakeLists.txt for sb_isql
2. Link against ScratchBird library
3. Link against libreadline or libedit
4. Install to bin/ directory

---

### Tool 1 Completion Criteria

- [  ] Interactive mode working with readline
- [  ] Batch mode functional
- [  ] All meta-commands implemented
- [  ] All output formats working
- [  ] History and line editing functional
- [  ] Builds on Linux, macOS, Windows
- [  ] Documentation complete (man page)

---

## Tool 2: sb_verify (Database Integrity Checker)

### Overview

Database integrity verification tool, checks:
1. Page checksums
2. Index consistency (indexes match heap)
3. Foreign key consistency
4. Catalog consistency
5. TIP (Transaction Inventory Page) validity

### Features

1. **Page-Level Checks:**
   - CRC32 checksum verification
   - Page header validation
   - Page free space tracking

2. **Index Checks:**
   - Index-to-heap TID verification
   - B-Tree structure validation
   - Duplicate key detection (UNIQUE indexes)

3. **Constraint Checks:**
   - Foreign key references exist
   - CHECK constraints satisfied
   - NOT NULL constraints satisfied

4. **Catalog Checks:**
   - Catalog table consistency
   - Referential integrity in catalog

5. **Transaction Checks:**
   - TIP page validity
   - No orphaned transactions

### Implementation

#### Task 2.1: Core Verification Engine

**File:** `tools/sb_verify/verify.cpp`
**Estimated Lines:** ~600

**Implementation:**
```cpp
class DatabaseVerifier {
    Database* db;
    std::vector<VerificationError> errors;

    void verifyPages();
    void verifyIndexes();
    void verifyConstraints();
    void verifyCatalog();
    void verifyTransactions();

    void reportError(const std::string& category, const std::string& message);
};

void DatabaseVerifier::verifyIndexes() {
    // For each table
    for (const auto& table : db->getTables()) {
        // For each index
        for (const auto& index : table.indexes) {
            // Scan index, verify each TID points to valid heap record
            std::vector<TID> index_tids;
            index.scanAll(index_tids);

            for (TID tid : index_tids) {
                Record* record = db->fetchRecord(tid);
                if (!record) {
                    reportError("INDEX", "Index points to non-existent record: " + tid.toString());
                } else {
                    // Verify indexed key matches record
                    Value index_key = index.extractKey(record);
                    if (index_key != record->getColumn(index.column_id)) {
                        reportError("INDEX", "Index key mismatch for record: " + tid.toString());
                    }
                }
            }
        }
    }
}
```

**Testing:**
- Verify clean database (no errors)
- Verify database with corrupted page
- Verify database with index inconsistency
- Verify database with FK violation

---

#### Task 2.2: CLI Interface

**File:** `tools/sb_verify/main.cpp`
**Estimated Lines:** ~150

**Usage:**
```bash
sb_verify /path/to/database.db
sb_verify /path/to/database.db --fix  # Attempt to fix errors
sb_verify /path/to/database.db --verbose  # Detailed output
```

**Output:**
```
ScratchBird Database Verification Tool
Database: /data/mydb.db
Database size: 1.2 GB
Tables: 15
Indexes: 23

[1/5] Verifying pages...       OK (1000 pages checked)
[2/5] Verifying indexes...     FAIL (3 errors)
  ERROR: Index users_email_idx points to non-existent record (page=5, line=10)
  ERROR: Index orders_date_idx has duplicate key for UNIQUE index
  ERROR: Index products_sku_idx key mismatch (page=8, line=3)
[3/5] Verifying constraints... OK (500 constraints checked)
[4/5] Verifying catalog...     OK
[5/5] Verifying transactions... OK

Summary:
  Total errors: 3
  Critical errors: 2
  Warnings: 1

Database is INCONSISTENT. Repair recommended.
```

---

### Tool 2 Completion Criteria

- [  ] Page verification working
- [  ] Index verification working
- [  ] Constraint verification working
- [  ] Catalog verification working
- [  ] Transaction verification working
- [  ] Fix mode implemented (optional)
- [  ] Documentation complete

---

## Tool 3: sb_backup (Backup and Restore Tool)

### Overview

Database backup and restore utility with compression and incremental backup support.

### Features

1. **Full Backup:**
   - Copy entire database file
   - Include metadata
   - Compress with zlib/zstd

2. **Incremental Backup:**
   - Only back up changed pages since last backup
   - Smaller backup size
   - Faster backup time

3. **Restore:**
   - Full restore from backup
   - Point-in-time recovery (if WAL available)
   - Verify backup integrity

4. **Formats:**
   - `.sbbackup` (custom format)
   - `.tar.gz` (plain file backup)

### Implementation

#### Task 3.1: Backup Engine

**File:** `tools/sb_backup/backup.cpp`
**Estimated Lines:** ~400

**Implementation:**
```cpp
class BackupEngine {
    Database* db;

    void performFullBackup(const std::string& backup_path);
    void performIncrementalBackup(const std::string& backup_path, const std::string& base_backup);
    void restoreBackup(const std::string& backup_path, const std::string& target_path);
};

void BackupEngine::performFullBackup(const std::string& backup_path) {
    // 1. Begin read transaction (to get consistent snapshot)
    TransactionId xid = db->beginTransaction(IsolationLevel::SERIALIZABLE, true);

    // 2. Open backup file
    std::ofstream backup(backup_path, std::ios::binary);

    // 3. Write backup header
    BackupHeader header;
    header.magic = BACKUP_MAGIC;
    header.version = BACKUP_VERSION;
    header.database_size = db->getDatabaseSize();
    header.page_count = db->getPageCount();
    header.timestamp = std::time(nullptr);
    backup.write((char*)&header, sizeof(header));

    // 4. Copy all pages
    for (uint32_t page_no = 0; page_no < db->getPageCount(); page_no++) {
        Page* page = db->pinPage(page_no);
        backup.write((char*)page->data, PAGE_SIZE);
        db->unpinPage(page_no);
    }

    // 5. Write catalog metadata
    writeCatalog(backup);

    // 6. Commit transaction
    db->commitTransaction(xid);

    backup.close();
}
```

**Testing:**
- Full backup of small database
- Full backup of large database (>1GB)
- Restore from backup
- Verify backup integrity
- Incremental backup

---

#### Task 3.2: CLI Interface

**File:** `tools/sb_backup/main.cpp`
**Estimated Lines:** ~200

**Usage:**
```bash
# Full backup
sb_backup backup /data/mydb.db /backups/mydb_full_2025-11-21.sbbackup

# Incremental backup
sb_backup backup /data/mydb.db /backups/mydb_incr_2025-11-21.sbbackup --incremental --base /backups/mydb_full.sbbackup

# Restore
sb_backup restore /backups/mydb_full_2025-11-21.sbbackup /data/mydb_restored.db

# List backup info
sb_backup info /backups/mydb_full_2025-11-21.sbbackup
```

---

### Tool 3 Completion Criteria

- [  ] Full backup working
- [  ] Incremental backup working
- [  ] Restore working
- [  ] Backup compression working
- [  ] Backup verification working
- [  ] Documentation complete

---

## Tool 4: sb_security (User/Role Management Tool)

### Overview

Command-line tool for managing users, roles, and permissions.

### Features

1. **User Management:**
   - Create user
   - Drop user
   - Change password
   - List users

2. **Role Management:**
   - Create role
   - Drop role
   - Grant role to user
   - Revoke role from user

3. **Permission Management:**
   - Grant table permissions
   - Grant column permissions
   - Revoke permissions
   - List permissions

### Implementation

#### Task 4.1: Core Functionality

**File:** `tools/sb_security/security.cpp`
**Estimated Lines:** ~300

**Implementation:**
```cpp
class SecurityManager {
    Database* db;

    void createUser(const std::string& username, const std::string& password);
    void dropUser(const std::string& username);
    void changePassword(const std::string& username, const std::string& new_password);
    void listUsers();

    void createRole(const std::string& rolename);
    void dropRole(const std::string& rolename);
    void grantRole(const std::string& rolename, const std::string& username);
    void revokeRole(const std::string& rolename, const std::string& username);

    void grantPermission(const std::string& table, const std::string& privilege, const std::string& grantee);
    void revokePermission(const std::string& table, const std::string& privilege, const std::string& grantee);
    void listPermissions(const std::string& table);
};
```

---

#### Task 4.2: CLI Interface

**File:** `tools/sb_security/main.cpp`
**Estimated Lines:** ~250

**Usage:**
```bash
# User management
sb_security create-user alice --password secret123
sb_security drop-user alice
sb_security change-password alice --new-password newsecret
sb_security list-users

# Role management
sb_security create-role developers
sb_security grant-role developers alice
sb_security revoke-role developers alice

# Permission management
sb_security grant SELECT,INSERT ON users TO alice
sb_security grant SELECT(email) ON users TO bob  # Column-level
sb_security revoke INSERT ON users FROM alice
sb_security list-permissions users
```

---

### Tool 4 Completion Criteria

- [  ] User management working
- [  ] Role management working
- [  ] Permission management working
- [  ] Interactive mode (optional)
- [  ] Documentation complete

---

## Part B: Views Completion (20% Remaining)

### Current Status

**Complete (80%):**
- ✅ CREATE VIEW / CREATE OR REPLACE VIEW
- ✅ CREATE MATERIALIZED VIEW
- ✅ DROP VIEW [IF EXISTS] [CASCADE | RESTRICT]
- ✅ REFRESH [CONCURRENTLY] MATERIALIZED VIEW
- ✅ Query expansion (SELECT from views → underlying tables)
- ✅ Column projection
- ✅ WITH CHECK OPTION (parser + catalog)

**Incomplete (20%):**
- ⧗ Physical materialization (table creation + data population)
- ❌ Updatable views (INSERT/UPDATE/DELETE through views)

---

### Task B.1: Physical Materialization for Materialized Views

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~200

**Current Behavior:**
- Materialized view metadata created in catalog
- `materialized_table_id` field exists but table not created

**Required Behavior:**
```
CREATE MATERIALIZED VIEW mv_users AS
  SELECT id, name FROM users WHERE active = true;

Should:
1. Create hidden table: sys.mv_users_data (or similar naming)
2. Execute query: SELECT id, name FROM users WHERE active = true
3. Populate hidden table with result rows
4. Store hidden table ID in ViewInfo.materialized_table_id
5. When querying mv_users: SELECT * FROM sys.mv_users_data
```

**Implementation:**
```cpp
Status executeCreateMaterializedView(const CreateViewStmt* stmt, TransactionId xid) {
    // 1. Create hidden materialized table
    std::string mat_table_name = "sys.mv_" + stmt->view_name + "_data";

    TableInfo mat_table;
    mat_table.table_name = mat_table_name;
    mat_table.table_type = TableType::MATERIALIZED_VIEW_DATA;
    mat_table.columns = deriveColumnsFromQuery(stmt->query);

    uint64_t mat_table_id = catalog->createTable(mat_table);

    // 2. Execute query and populate materialized table
    ResultSet result;
    executeQuery(stmt->query, xid, result);

    for (const auto& row : result.rows) {
        insertTuple(mat_table_id, row, xid);
    }

    // 3. Create view metadata
    ViewInfo view;
    view.view_name = stmt->view_name;
    view.view_definition = stmt->query;
    view.is_materialized = true;
    view.materialized_table_id = mat_table_id;
    view.last_refresh_time = std::time(nullptr);

    catalog->createView(view);

    return Status::OK;
}

Status executeRefreshMaterializedView(const std::string& view_name, TransactionId xid) {
    // 1. Get view metadata
    ViewInfo view = catalog->getView(view_name);

    if (!view.is_materialized) {
        return error("Cannot refresh non-materialized view");
    }

    // 2. Truncate existing materialized table
    truncateTable(view.materialized_table_id);

    // 3. Re-execute query and repopulate
    ResultSet result;
    executeQuery(view.view_definition, xid, result);

    for (const auto& row : result.rows) {
        insertTuple(view.materialized_table_id, row, xid);
    }

    // 4. Update last_refresh_time
    view.last_refresh_time = std::time(nullptr);
    catalog->updateView(view);

    return Status::OK;
}
```

**Testing:**
- CREATE MATERIALIZED VIEW
- Query materialized view
- REFRESH MATERIALIZED VIEW
- Verify data consistency
- Large materialized view (performance test)

---

### Task B.2: Updatable Views

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~400

**Specification:**
A view is automatically updatable if:
1. Has exactly one table in FROM clause (no joins)
2. No DISTINCT
3. No GROUP BY
4. No aggregate functions
5. No set operations (UNION, INTERSECT, EXCEPT)

**Implementation:**
```cpp
bool isViewUpdatable(const ViewInfo& view) {
    // Parse view definition
    ASTNode* query = parseSQL(view.view_definition);

    // Check conditions
    if (query->table_count != 1) return false;
    if (query->has_distinct) return false;
    if (query->has_group_by) return false;
    if (query->has_aggregates) return false;
    if (query->has_set_operations) return false;

    return true;
}

Status executeInsertThroughView(const std::string& view_name, const InsertStmt* stmt, TransactionId xid) {
    ViewInfo view = catalog->getView(view_name);

    if (!isViewUpdatable(view)) {
        return error("View is not updatable");
    }

    // 1. Get underlying table
    uint64_t base_table_id = extractBaseTableFromView(view.view_definition);

    // 2. Translate column names (view columns → table columns)
    std::vector<uint16_t> base_column_ids = mapViewColumnsToBase(view, stmt->columns);

    // 3. Insert into base table
    insertTuple(base_table_id, stmt->values, base_column_ids, xid);

    // 4. If WITH CHECK OPTION, verify inserted row satisfies view condition
    if (view.has_check_option) {
        if (!rowSatisfiesViewCondition(view.view_definition, inserted_row)) {
            return error("WITH CHECK OPTION violation");
        }
    }

    return Status::OK;
}

Status executeUpdateThroughView(const std::string& view_name, const UpdateStmt* stmt, TransactionId xid) {
    ViewInfo view = catalog->getView(view_name);

    if (!isViewUpdatable(view)) {
        return error("View is not updatable");
    }

    // Similar to INSERT: translate to base table UPDATE
    // Apply view WHERE condition as additional filter
    // Enforce WITH CHECK OPTION

    // ...
}

Status executeDeleteThroughView(const std::string& view_name, const DeleteStmt* stmt, TransactionId xid) {
    ViewInfo view = catalog->getView(view_name);

    if (!isViewUpdatable(view)) {
        return error("View is not updatable");
    }

    // Translate to base table DELETE
    // Apply view WHERE condition

    // ...
}
```

**Testing:**
- INSERT through updatable view
- UPDATE through updatable view
- DELETE through updatable view
- WITH CHECK OPTION enforcement
- Attempt to update non-updatable view (should error)

---

### Part B Completion Criteria

- [  ] Materialized view physical table creation
- [  ] Materialized view data population
- [  ] REFRESH MATERIALIZED VIEW working
- [  ] Updatable views (INSERT/UPDATE/DELETE)
- [  ] WITH CHECK OPTION enforcement
- [  ] All view tests passing

---

## Overall Completion Criteria

### Part A: Command-Line Tools
- [  ] sb_isql fully functional
- [  ] sb_verify working
- [  ] sb_backup/restore working
- [  ] sb_security working
- [  ] All tools documented

### Part B: Views Completion
- [  ] Materialized views 100% complete
- [  ] Updatable views working
- [  ] All view tests passing

---

## Estimated Effort

**Total Estimated Lines:** ~5,100 lines
**Estimated Time:** 120-150 hours
**Priority:** MEDIUM to HIGH (tools are important for usability)

**Breakdown:**
- sb_isql: 40 hours
- sb_verify: 30 hours
- sb_backup: 35 hours
- sb_security: 25 hours
- Views completion: 30 hours

---

## Dependencies

**Blocked By:**
- PSQL/Triggers (for full sb_isql functionality)

**Blocks:** Alpha 1 completion

---

**Last Updated:** November 21, 2025
**Next Review:** After sb_isql completion
