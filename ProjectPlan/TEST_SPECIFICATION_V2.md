# Comprehensive Test Specification V2
## Aligned with Universal Database Architecture

## Test Categories

### 1. Core Engine Tests (Phases 1-24)
Tests that verify the embedded engine works correctly without any network layer.

### 2. Protocol Tests (Phases 25-30)
Tests that verify each database protocol works correctly.

### 3. Compatibility Tests (Phases 25-30)
Tests using actual database clients and applications.

### 4. Federation Tests (Phases 34-37)
Tests that verify cross-database queries work.

### 5. Performance Tests (All Phases)
Tests that verify performance targets are met.

## Phase-Specific Test Requirements

### Phase 1-5: Foundation Tests
```cpp
TEST_F(Foundation, DatabaseCreation)
TEST_F(Foundation, PageReadWrite)
TEST_F(Foundation, ChecksumValidation)
TEST_F(Foundation, TupleStorage)
TEST_F(Foundation, SpaceAllocation)
TEST_F(Foundation, MultiSegmentGrowth)
```

### Phase 6-8: MGA Core Tests (Critical)
```cpp
// MGA must work WITHOUT WAL
TEST_F(MGA, TransactionsWithoutWAL) {
    disable_wal();
    auto txn = begin_transaction();
    execute(txn, "INSERT INTO test VALUES (1)");
    commit(txn);
    // Verify visible through TIP, not WAL
}

TEST_F(MGA, NoReadLocks) {
    // Start long read
    auto read_future = async([]() {
        execute("SELECT * FROM large_table");
    });
    
    // Concurrent write must not block
    auto write_future = async([]() {
        execute("UPDATE large_table SET val = 2");
    });
    
    // Both complete without deadlock
    EXPECT_TRUE(read_future.wait_for(1s) == future_status::ready);
    EXPECT_TRUE(write_future.wait_for(1s) == future_status::ready);
}

TEST_F(MGA, VersionChains)
TEST_F(MGA, GarbageCollection)
TEST_F(MGA, IsolationLevels)
TEST_F(MGA, UUIDCatalog)
```

### Phase 9-15: SQL Engine Tests
```cpp
TEST_F(SQL, Parser)
TEST_F(SQL, BasicCRUD)
TEST_F(SQL, Indexes)
TEST_F(SQL, Constraints)
TEST_F(SQL, JoinOperations)
TEST_F(SQL, Aggregation)
TEST_F(SQL, WindowFunctions)
```

### Phase 16: WAL Secondary Tests
```cpp
TEST_F(WAL, OptionalWAL) {
    // Database works without WAL
    disable_wal();
    execute("INSERT INTO test VALUES (1)");
    EXPECT_EQ(select_count("test"), 1);
    
    // But not durable
    simulate_crash();
    restart();
    EXPECT_EQ(select_count("test"), 0);
}

TEST_F(WAL, DurabilityWithWAL) {
    enable_wal();
    execute("INSERT INTO test VALUES (1)");
    simulate_crash();
    restart();
    EXPECT_EQ(select_count("test"), 1);  // Recovered
}
```

### Phase 25-30: Multi-Protocol Tests

#### PostgreSQL Protocol Tests
```cpp
TEST_F(PostgreSQL, WireProtocol) {
    // Connect with libpq
    PGconn* conn = PQconnectdb("host=localhost port=5432");
    ASSERT_EQ(PQstatus(conn), CONNECTION_OK);
    
    // Execute query
    PGresult* res = PQexec(conn, "SELECT 1");
    ASSERT_EQ(PQresultStatus(res), PGRES_TUPLES_OK);
    ASSERT_STREQ(PQgetvalue(res, 0, 0), "1");
}

TEST_F(PostgreSQL, SystemCatalogs) {
    // pg_catalog visible
    auto res = pg_execute("SELECT * FROM pg_catalog.pg_class");
    EXPECT_GT(res.rows.size(), 0);
}

TEST_F(PostgreSQL, PSQLClient) {
    // psql command-line client works
    system("psql -h localhost -c 'SELECT 1'");
}

TEST_F(PostgreSQL, DjangoORM) {
    // Django migrations work
    system("python manage.py migrate");
}
```

#### MySQL Protocol Tests
```cpp
TEST_F(MySQL, WireProtocol) {
    // Connect with MySQL client library
    MYSQL* conn = mysql_init(NULL);
    mysql_real_connect(conn, "localhost", "user", "pass", "db", 3306, NULL, 0);
    
    // Execute query
    mysql_query(conn, "SELECT 1");
    MYSQL_RES* result = mysql_store_result(conn);
    MYSQL_ROW row = mysql_fetch_row(result);
    ASSERT_STREQ(row[0], "1");
}

TEST_F(MySQL, InformationSchema) {
    // information_schema visible
    auto res = mysql_execute("SELECT * FROM information_schema.tables");
    EXPECT_GT(res.rows.size(), 0);
}

TEST_F(MySQL, WordPressInstall) {
    // WordPress can install and run
    // This is the ultimate MySQL compatibility test
}
```

#### MSSQL Protocol Tests
```cpp
TEST_F(MSSQL, TDSProtocol) {
    // Connect with TDS
    // Execute T-SQL specific syntax
    auto res = mssql_execute("SELECT TOP 10 * FROM users");
}

TEST_F(MSSQL, SystemViews) {
    // sys schema visible
    auto res = mssql_execute("SELECT * FROM sys.tables");
}
```

### Phase 31-33: UUID Schema Tests
```cpp
TEST_F(UUIDSchema, RenameWithoutBreaking) {
    // Create procedure referencing table
    execute("CREATE TABLE users (id INT)");
    execute("CREATE PROCEDURE p() SELECT * FROM users");
    
    // Rename table
    execute("ALTER TABLE users RENAME TO customers");
    
    // Procedure still works (uses UUID)
    execute("CALL p()");  // No error
}

TEST_F(UUIDSchema, SchemaMount) {
    // Mount remote MySQL database
    mount_mysql("/mysql/", mysql_connection);
    
    // Query through mount point
    auto res = execute("SELECT * FROM `/mysql/`.users");
    EXPECT_GT(res.rows.size(), 0);
}

TEST_F(UUIDSchema, MultipleViews) {
    // MySQL client sees MySQL view
    set_client_type(ClientType::MySQL);
    auto res = execute("SHOW TABLES");
    
    // PostgreSQL client sees different view
    set_client_type(ClientType::PostgreSQL);
    res = execute("\\dt");
    
    // Both work on same data
}
```

### Phase 34-37: Federation Tests
```cpp
TEST_F(Federation, CrossDatabaseJoin) {
    // Mount external PostgreSQL
    mount_postgresql("/pg/", pg_connection);
    
    // Mount external MySQL
    mount_mysql("/mysql/", mysql_connection);
    
    // Join across databases
    auto res = execute(
        "SELECT p.*, m.* "
        "FROM `/pg/`.users p "
        "JOIN `/mysql/`.orders m ON p.id = m.user_id"
    );
    
    EXPECT_GT(res.rows.size(), 0);
}

TEST_F(Federation, TransactionAcrossDatabases) {
    begin_transaction();
    execute("INSERT INTO local_table VALUES (1)");
    execute("INSERT INTO `/pg/`.remote_table VALUES (1)");
    commit();  // Two-phase commit
}
```

## Performance Requirements

### Latency Targets
| Operation | Target | Maximum |
|-----------|--------|---------|
| Simple SELECT | < 0.1ms | 1ms |
| Index lookup | < 0.05ms | 0.5ms |
| Insert row | < 0.5ms | 5ms |
| Protocol overhead | < 0.1ms | 1ms |

### Throughput Targets
| Operation | Target | Minimum |
|-----------|--------|---------|
| Sequential scan | > 1GB/s | 500MB/s |
| Index scan | > 100K rows/s | 50K rows/s |
| Inserts | > 50K rows/s | 10K rows/s |
| Network requests | > 10K req/s | 5K req/s |

### Scalability Targets
| Metric | Target |
|--------|--------|
| Concurrent connections | > 10,000 |
| Database size | > 1TB |
| Table size | > 1 billion rows |
| Concurrent transactions | > 1,000 |

## Compatibility Test Suite

### ORM Frameworks
```python
# SQLAlchemy
def test_sqlalchemy():
    engine = create_engine('scratchbird://localhost/db')
    # Full ORM operations
    
# Django
def test_django():
    # DATABASE = {'ENGINE': 'scratchbird.django'}
    # Run migrations, queries
    
# Hibernate
def test_hibernate():
    // EntityManager operations
```

### Applications
```bash
# WordPress (MySQL compatibility)
./test_wordpress_install.sh

# GitLab (PostgreSQL compatibility)
./test_gitlab_install.sh

# Drupal (Multiple database support)
./test_drupal_install.sh
```

### Database Tools
```bash
# Backup/Restore tools
pg_dump scratchbird://localhost/db > backup.sql
mysql_dump -h localhost -P 3306 > backup.sql

# Admin tools
pgAdmin4 connects successfully
phpMyAdmin connects successfully
DBeaver connects successfully
```

## Test Execution Strategy

### Continuous Integration
```yaml
# .github/workflows/test.yml
test:
  matrix:
    phase: [1-5, 6-8, 9-15, 16-24, 25-30, 31-37]
  steps:
    - run: ./test_phase_${{ matrix.phase }}
```

### Test Ordering
1. **Unit tests** - Each component in isolation
2. **Integration tests** - Components together
3. **Protocol tests** - Wire protocol compliance
4. **Compatibility tests** - Real clients
5. **Performance tests** - Meet targets
6. **Stress tests** - Break points

### Coverage Requirements
- Line coverage: > 90% for core engine
- Branch coverage: > 80% for core engine
- Protocol coverage: 100% of required messages
- SQL coverage: 100% of SQL-92

## Test Data Sets

### Standard Data Sets
- **Tiny**: 100 rows (unit tests)
- **Small**: 10K rows (integration tests)
- **Medium**: 1M rows (performance tests)
- **Large**: 100M rows (stress tests)
- **Huge**: 10B rows (scalability tests)

### Industry Benchmarks
- TPC-C: OLTP workload
- TPC-H: Analytics queries
- YCSB: NoSQL-style workload
- Sysbench: Mixed workload

## Regression Prevention

### Performance Regression
```cpp
TEST_F(Performance, NoRegression) {
    auto baseline = load_baseline_metrics();
    auto current = run_benchmark();
    
    EXPECT_LE(current.latency, baseline.latency * 1.1);  // Max 10% regression
    EXPECT_GE(current.throughput, baseline.throughput * 0.9);
}
```

### Compatibility Regression
```cpp
TEST_F(Compatibility, ClientsStillWork) {
    for (auto client : {libpq, mysql_client, tedious}) {
        EXPECT_TRUE(client.connect());
        EXPECT_TRUE(client.execute("SELECT 1"));
    }
}
```

## Test Documentation

Each test must document:
1. **Purpose**: What it tests
2. **Setup**: Prerequisites
3. **Steps**: How it tests
4. **Expected**: Success criteria
5. **Cleanup**: Resource cleanup

Example:
```cpp
/**
 * Purpose: Verify MGA provides isolation without locks
 * Setup: Create large table with 1M rows
 * Steps: 
 *   1. Start long-running read query
 *   2. Concurrent write to same table
 *   3. Verify both complete
 * Expected: No deadlock, both succeed
 * Cleanup: Drop test table
 */
TEST_F(MGA, NoReadLocks) {
    // ... test implementation
}
```

## Success Criteria

A phase is complete when:
1. All unit tests pass
2. All integration tests pass
3. Performance targets met
4. No memory leaks (Valgrind clean)
5. No race conditions (ThreadSanitizer clean)
6. Documentation complete
7. Code review passed