
**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.

### Test Suite Specification

This section defines the required test suites, referencing existing files that can be refactored and specifying new tests needed to fill coverage gaps.

---

### Current Status

**Build System:** ✅ **FUNCTIONAL** - All compilation errors resolved, tests compile and link successfully.

**Executor Status:** ⚠️ **DISABLED** - The SQL executor tests (`test_executor.cpp.disabled`) are currently disabled. The file exists and compiles when renamed, but is not currently integrated into the test suite. Re-enabling requires:
  - Renaming `test_executor.cpp.disabled` → `test_executor.cpp`
  - Verifying Database initialization works correctly in test context
  - Ensuring SBLR bytecode execution completes without errors

**Database Initialization:** ✅ **RESOLVED** - No hanging issues detected. The initialization concern mentioned in Phase 1 appears to be outdated. Current tests (`Alpha101Test`, `MGAIntegrationTest`, etc.) successfully create and initialize databases without blocking.

**Test Infrastructure Currently Working:**
- ✅ Suite 1 (Data Integrity): All tests passing (Alpha101, storage corruption, boundary, CRC, UUID validation)
- ✅ Suite 2 (Storage/TOAST): Core functionality tested and passing (heap page, TOAST, compression)
- ✅ Suite 3 (Indexing): Both Hash and B-Tree indexes fully tested (12 hash tests, 11 B-tree tests all passing)
- ✅ Suite 4 (MGA): Basic MVCC infrastructure tested (9 integration tests passing including lock manager, version chains, vacuum)
- ⚠️ Suite 5 (SQL Front-End): Parser/Lexer/Analyzer tested extensively, **Executor disabled**
- ✅ Suite 6 (Robustness): Memory safety, security, and edge case tests operational

**What Can Be Tested Now:**
- All low-level storage operations (heap pages, TOAST, compression)
- B-Tree and Hash index correctness (insert, search, delete, vacuum)
- Multi-generational architecture (transaction visibility, lock conflicts, version chains)
- SQL parsing, lexing, and semantic analysis
- Bytecode generation from SQL statements
- On-disk format integrity (CRC, UUID, page headers)
- Memory safety and security boundaries

**What Needs Prerequisite Work:**
- **End-to-end SQL execution:** Requires re-enabling `test_executor.cpp` and ensuring Database/Executor integration
- **Complex MGA scenarios:** Lock conflict matrix test, deadlock detection, and complex snapshot isolation scenarios (Suite 4 gap tests)
- **B-Tree range scans:** Iterator tests for bounded/unbounded scans (Phase 3 feature)
- **TOAST update scenarios:** Update from small→TOAST, TOAST→small, TOAST→TOAST (Suite 2 gap tests)
- **Performance benchmarks:** All 5 benchmark suites require stable executor and sufficient test data

**Immediate Next Steps:**
1. Re-enable `test_executor.cpp` to validate end-to-end SQL pipeline
2. Implement critical MGA gap tests (lock conflict matrix, deadlock detection)
3. Add B-Tree iterator tests for range scans
4. Implement TOAST update test scenarios

---

#### Suite 1: Data Integrity & On-Disk Format

- **Goal:** Verify the physical correctness of the database file and ensure the system is resilient to corruption and boundary conditions.

- **Existing Foundation:** This suite will consolidate the excellent existing tests from `test_storage_corruption.cpp`, `test_storage_boundary.cpp`, `test_ondisk_crc_uuid.cpp`, `test_alpha101_create_open.cpp`, and the corruption tests within `test_heap_page.cpp`.

- **Required Coverage (Existing):**
  
  - Validation of `PageHeader` for all page sizes.
  
  - CRC32C checksum correctness and failure detection.
  
  - UUIDv7 monotonicity and format validation.
  
  - Graceful failure on corrupted magic numbers, page types, item pointers, and tuple headers.
  
  - Handling of truncated/short database files.
  
  - Boundary checks for max/min tuple size and page capacity.

#### Suite 2: Core Storage, Heap & TOAST

- **Goal:** Validate the storage of in-line and out-of-line (TOAST) tuples.

- **Existing Foundation:** `test_storage_engine.cpp`, `test_heap_page.cpp`, `test_toast.cpp`, `test_heap_toast_integration.cpp`, `test_compression.cpp`, and `test_compression_interop.cpp`.

- **Required Coverage (Existing & New):**
  
  - **Heap Page:** All existing tests for `insertTuple`, `getTuple`, `deleteTuple`, and free space management.
  
  - **TOAST:** All existing tests for automatic toasting of large values, correct detoasting, cleanup on deletion, and compression.
  
  - **New - TOAST Update Test:**
    
    - Test updating a row from a small value to a TOASTed value.
    
    - Test updating a row from a TOASTed value to a small value (should clean up TOAST chunks).
    
    - Test updating a TOASTed value to a different TOASTed value.

#### Suite 3: Indexing (Hash & B-Tree)

- **Goal:** Verify the correctness of both Hash and B-Tree indexes.

- **Existing Foundation:** The comprehensive `test_hash_index.cpp` and the foundational `test_btree.cpp`.

- **Required Coverage:**
  
  - **Hash Index:** The existing 12-test suite is sufficient and should be validated to pass post-MGA.
  
  - **B-Tree (Existing):** Keep all tests for `create`/`open`, insert, search, splits (random and sequential), duplicate keys, soft deletes, and persistence.
  
  - **New - B-Tree Range Scan Iterator Tests (Phase 3 Feature):**
    
    - Full table scan on empty, single-page, and multi-page trees.
    
    - Bounded scans with inclusive and exclusive boundaries.
    
    - Unbounded scans (e.g., `key > X`).
    
    - Scans that return duplicate keys.
    
    - Verification of `getScannedCount()` statistic.
  
  - **New - B-Tree Vacuum/Compaction Tests (Phase 5 Feature):**
    
    - Verify `vacuum()` physically removes soft-deleted nodes and reclaims page space.
    
    - Test compaction on a page with a mix of live and dead nodes.
    
    - Test edge cases: compacting a page with no dead nodes, and a page where all nodes are dead.
    
    - Verify the accuracy of all fields in the `VacuumStats` struct.
  
  - **New - B-Tree Compression (Future):** Add an integration test to verify that `add_node()` utilizes the compression infrastructure once it is wired up.

#### Suite 4: MGA & Concurrency Control

- **Goal:** Rigorously validate the new MVCC architecture and ensure correct concurrent behavior. This is the **most critical area for new test development**.

- **Existing Foundation:** The excellent `test_mga_integration.cpp` provides a solid starting point for basic functionality. `test_security_issues.cpp` provides process-level locking tests. `test_transaction_fixes_corrected.cpp` provides a `NoDeadlock` smoke test.

- **Required New Tests:**
  
  - **Lock Manager Conflict Matrix Test:** Create a test that systematically iterates through all 64 pairs of the 8 lock modes on the same object from two different backends, asserting whether the lock is granted or conflicts, to validate the conflict matrix.
  
  - **Deadlock Detection Test:**
    
    1. Backend A acquires `ROW_EXCLUSIVE` lock on Tuple 1.
    
    2. Backend B acquires `ROW_EXCLUSIVE` lock on Tuple 2.
    
    3. Backend A attempts to acquire lock on Tuple 2 (and waits).
    
    4. Backend B attempts to acquire lock on Tuple 1, creating a deadlock.
    
    5. Verify that one of the transactions is aborted with a deadlock error.
  
  - **Complex Visibility Scenario Test (Snapshot Isolation):**
    
    1. T1 starts.
    
    2. T2 starts, inserts row R1, and commits.
    
    3. T3 starts, updates R1 to R1v2.
    
    4. T1 reads R1. It **must see R1v1** (from T2), not R1v2 (from T3).
    
    5. T3 commits.
    
    6. T1 reads R1 again. It **must still see R1v1** because its snapshot was taken at the start.
  
  - **Version Chain & Vacuum Integration Test:**
    
    1. Insert a row. `UPDATE` it 5 times, creating a version chain of 6 tuples.
    
    2. Start a new transaction (this sets the vacuum horizon).
    
    3. Run `vacuum()`.
    
    4. Verify that the old, dead versions in the chain have been pruned/removed.

#### Suite 5: SQL Front-End & Execution

- **Goal:** Ensure the entire SQL pipeline (Lexer -> Parser -> Analyzer -> Bytecode -> Executor) is correct and robust.

- **Existing Foundation:** A massive and high-quality set of tests exists across `test_lexer*.cpp`, `test_parser*.cpp`, `test_semantic_analyzer.cpp`, `test_sql_to_bytecode.cpp`, and the disabled `test_executor.cpp.disabled`.

- **Required Coverage:**
  
  - **Lexer/Parser/Analyzer:** The existing tests are comprehensive. They should be organized, enabled, and run as a single suite. No major new tests are required for Alpha.
  
  - **SQL Executor:** The primary task is to **fix and re-enable `test_executor.cpp.disabled`**. This existing suite already covers `CREATE TABLE`, `INSERT`, `SELECT *`, and `SELECT` with `WHERE`, providing excellent E2E test coverage once functional.

#### Suite 6: Robustness, Memory & Security

- **Goal:** Consolidate all non-functional tests that probe the engine's limits and resilience.

- **Existing Foundation:** `test_memory_safety.cpp`, `test_security_issues.cpp`, `test_page_management_edge_cases.cpp`.

- **Required Coverage (Existing):**
  
  - OOM testing via allocation failure injection.
  
  - File descriptor leak detection.
  
  - Path traversal and symbolic link vulnerability checks.
  
  - Process-level file lock enforcement and cleanup on crash.
  
  - Buffer pool exhaustion handling.

---

### 5. Performance Analysis Specification 🚀

Performance testing is critical for a database engine. The existing `test_storage_performance.cpp` and `test_storage_stress.cpp` provide a strong foundation. This specification formalizes and expands upon them. All benchmarks must be run against **each of the five supported page sizes** to analyze their impact.

- **BM-1: Insert Throughput (ops/sec):**
  
  - Measure sequential insertion rate for various tuple sizes (100B, 1KB, 8KB).
  
  - This will stress page allocation, buffer pool eviction, and I/O.

- **BM-2: Read/Scan Throughput (MB/sec):**
  
  - **Random Reads:** Measure latency (µs/op) of fetching tuples by ID.
  
  - **Sequential Scans:** Measure MB/sec for full table scans.
  
  - **B-Tree Range Scans:** Measure tuple/sec for a range scan yielding 10% of a large table.

- **BM-3: Update & Delete Performance (ops/sec):**
  
  - Measure the rate of `UPDATE` operations, which stresses version chain creation.
  
  - Measure the rate of `DELETE` operations.

- **BM-4: Concurrency Scalability:**
  
  - Run a mixed workload (e.g., 80% reads, 20% writes) with 1, 2, 4, 8, 16, and 32 concurrent threads.
  
  - Measure the total throughput and how it scales with the number of threads. This will be the ultimate test of the MGA implementation's efficiency.

- **BM-5: Vacuum Performance:**
  
  - Create a large table, delete 50% of its rows, and measure the time taken for `vacuum()` to complete.

---

### 6. Implementation Roadmap

1. **Phase 1: Unblock the Environment (Highest Priority)**
   
   - **Task 1.1:** Resolve the database initialization hang.
   
   - **Task 1.2:** Fix the SBLR Executor compilation errors and re-enable `test_executor.cpp`.

2. **Phase 2: Refactor & Organize Test Workflow**
   
   - **Task 2.1:** Implement the parameterized, chained execution model (Setup/Test/Teardown).
   
   - **Task 2.2:** Consolidate existing test files into the logical suites defined above. Remove obsolete tests like `test_transaction_manager.cpp`.

3. **Phase 3: Implement Critical Gap Coverage**
   
   - **Task 3.1:** Write the new, detailed **MGA & Concurrency Control** tests (deadlock, conflict matrix, visibility).
   
   - **Task 3.2:** Write the new **B-Tree Range Scan and Vacuum** tests.

4. **Phase 4: Execute, Benchmark & Iterate**
   
   - **Task 4.1:** Run the complete, unified test suite and fix any discovered bugs.
   
   - **Task 4.2:** Execute the full performance benchmark suite and establish baseline metrics.
   
   - **Task 4.3:** Integrate the test suite into a Continuous Integration (CI) process.
