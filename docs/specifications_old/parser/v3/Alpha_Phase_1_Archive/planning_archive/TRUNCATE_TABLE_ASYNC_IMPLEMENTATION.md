# TRUNCATE TABLE ASYNC - Complete Implementation Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: November 7, 2025
**Goal**: MGA-compliant TRUNCATE TABLE with async execution
**Estimated Effort**: 15-20 hours

---

## Design Summary

TRUNCATE TABLE ASYNC is MGA-compliant because it:
1. **Soft deletes only** - Sets `xmax` on tuples, doesn't physically remove data
2. **Respects transaction visibility** - Only deletes rows committed before TRUNCATE started
3. **Non-blocking** - Runs in background thread, returns immediately
4. **Concurrent-safe** - New INSERTs during truncation are preserved
5. **Progress tracking** - Users can monitor truncation progress

---

## Architecture

### Header Additions (COMPLETE ✅)

**File**: `include/scratchbird/core/catalog_manager.h`

```cpp
// TruncateJob struct added after TableInfo
struct TruncateJob {
    uint64_t job_id;
    ID table_id;
    std::string table_name;
    uint64_t snapshot_xid;
    std::atomic<uint64_t> rows_processed;
    std::atomic<uint64_t> rows_deleted;
    std::atomic<bool> completed;
    std::atomic<bool> error;
    std::string error_message;
    uint64_t start_time;
    std::atomic<uint64_t> end_time;
    double getProgress() const;
};

// Private members added
std::unordered_map<uint64_t, std::shared_ptr<TruncateJob>> truncate_jobs_;
std::mutex truncate_jobs_mutex_;
std::atomic<uint64_t> next_truncate_job_id_{1};

// Public methods added
auto truncateTableAsync(...) -> uint64_t;
auto truncateTableSync(...) -> Status;
auto getTruncateJobStatus(uint64_t job_id) -> std::shared_ptr<TruncateJob>;
auto waitForTruncate(uint64_t job_id, uint32_t timeout_ms) -> Status;
auto listTruncateJobs(...) -> void;
```

---

## Implementation Steps

### Step 1: Catalog Manager Implementation

**File**: `src/core/catalog_manager.cpp`

#### 1.1 truncateTableAsync()
```cpp
auto CatalogManager::truncateTableAsync(const ID &table_id, const std::string &table_name,
                                         uint64_t snapshot_xid, ErrorContext *ctx) -> uint64_t
{
    // Create job
    auto job = std::make_shared<TruncateJob>();
    job->job_id = next_truncate_job_id_.fetch_add(1);
    job->table_id = table_id;
    job->table_name = table_name;
    job->snapshot_xid = snapshot_xid;
    job->start_time = std::time(nullptr);

    // Register job
    {
        std::lock_guard<std::mutex> lock(truncate_jobs_mutex_);
        truncate_jobs_[job->job_id] = job;
    }

    // Spawn background thread
    std::thread([this, job]() {
        try {
            // Get table info
            TableInfo table_info;
            ErrorContext ctx;
            auto status = getTable(..., table_info, &ctx);
            if (status != Status::OK) {
                job->error = true;
                job->error_message = "Table not found";
                job->completed = true;
                return;
            }

            // Find first heap page
            GPID current_gpid = table_info.first_heap_gpid; // Need to add this field!

            // Iterate all heap pages
            while (current_gpid.page_id != 0) {
                auto pin = db_->buffer_pool()->pinPage(current_gpid, &ctx);
                if (!pin) break;

                HeapPage heap_page(pin.frame()->data());
                auto item_ids = heap_page.getItemIds();

                for (const auto &item_id : item_ids) {
                    if (!heap_page.isItemValid(item_id)) continue;

                    job->rows_processed++;

                    auto tuple_data = heap_page.getTuple(item_id);
                    TID tid = heap_page.getTID(item_id);

                    // Only delete rows committed BEFORE truncate started
                    if (db_->tip()->isVersionVisible(tid.xmin, job->snapshot_xid)) {
                        // Soft delete: set xmax
                        heap_page.setTupleXMax(item_id, job->snapshot_xid);
                        job->rows_deleted++;
                    }
                }

                pin.markDirty();
                pin.unpin();

                // Get next page
                current_gpid = heap_page.getNextPage();

                // Yield to avoid hogging CPU
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            // Update table row_count
            updateTableRowCount(job->table_id, 0, &ctx);

            job->completed = true;
            job->end_time = std::time(nullptr);

        } catch (const std::exception &e) {
            job->error = true;
            job->error_message = e.what();
            job->completed = true;
        }
    }).detach();

    return job->job_id;
}
```

#### 1.2 truncateTableSync()
```cpp
auto CatalogManager::truncateTableSync(const ID &table_id, const std::string &table_name,
                                        uint64_t snapshot_xid, ErrorContext *ctx) -> Status
{
    // Start async job
    auto job_id = truncateTableAsync(table_id, table_name, snapshot_xid, ctx);

    // Wait for completion (no timeout)
    return waitForTruncate(job_id, 0);
}
```

#### 1.3 getTruncateJobStatus()
```cpp
auto CatalogManager::getTruncateJobStatus(uint64_t job_id) -> std::shared_ptr<TruncateJob>
{
    std::lock_guard<std::mutex> lock(truncate_jobs_mutex_);
    auto it = truncate_jobs_.find(job_id);
    if (it != truncate_jobs_.end()) {
        return it->second;
    }
    return nullptr;
}
```

#### 1.4 waitForTruncate()
```cpp
auto CatalogManager::waitForTruncate(uint64_t job_id, uint32_t timeout_ms) -> Status
{
    auto job = getTruncateJobStatus(job_id);
    if (!job) {
        return Status::NOT_FOUND;
    }

    auto start = std::chrono::steady_clock::now();

    while (!job->completed.load()) {
        // Check timeout
        if (timeout_ms > 0) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed > std::chrono::milliseconds(timeout_ms)) {
                return Status::TIMEOUT;
            }
        }

        // Wait a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Check if error occurred
    if (job->error.load()) {
        return Status::INTERNAL_ERROR;
    }

    return Status::OK;
}
```

#### 1.5 listTruncateJobs()
```cpp
auto CatalogManager::listTruncateJobs(std::vector<std::shared_ptr<TruncateJob>> &jobs_out) -> void
{
    std::lock_guard<std::mutex> lock(truncate_jobs_mutex_);
    for (const auto &[job_id, job] : truncate_jobs_) {
        jobs_out.push_back(job);
    }
}
```

---

### Step 2: AST & Parser

**Files Modified**:
- `include/scratchbird/parser/ast.h` ✅ (TRUNCATE_TABLE added to ASTKind)
- `include/scratchbird/parser/token.h` ✅ (KW_TRUNCATE added)
- `src/parser/lexer.cpp` ✅ (TRUNCATE keyword mapped)

**Add to ast.h** (TruncateTableStmt class - already added ✅):
```cpp
class TruncateTableStmt : public Statement {
public:
    enum class TruncateMode : uint8_t {
        ASYNC,  // Background job (default)
        SYNC    // Block until complete
    };

    TruncateTableStmt(const SourceSpan &span, StringPool::StringId table_name,
                      TruncateMode mode = TruncateMode::ASYNC)
        : Statement(ASTKind::TRUNCATE_TABLE, span),
          table_name_(table_name), mode_(mode) {}

    StringPool::StringId tableName() const { return table_name_; }
    TruncateMode mode() const { return mode_; }

    void accept(ASTVisitor *visitor) override;

private:
    StringPool::StringId table_name_;
    TruncateMode mode_;
};
```

**Add to parser.cpp** (parseTruncateTable method):
```cpp
Statement *Parser::parseTruncateTable()
{
    auto start_loc = current().location;

    // TRUNCATE already consumed
    // Optional TABLE keyword
    if (match(TokenType::KW_TABLE)) {
        advance();
    }

    // Get table name
    if (!check(TokenType::IDENTIFIER)) {
        error("Expected table name after TRUNCATE TABLE");
        return nullptr;
    }
    auto table_name = current().value.string_id;
    advance();

    // Check for ASYNC/SYNC mode
    auto mode = TruncateTableStmt::TruncateMode::ASYNC; // default
    if (match(TokenType::KW_SYNC)) {
        advance();
        mode = TruncateTableStmt::TruncateMode::SYNC;
    } else if (match(TokenType::KW_ASYNC)) {
        advance();
        mode = TruncateTableStmt::TruncateMode::ASYNC;
    }

    // Create AST node
    auto *stmt = arena_.make<TruncateTableStmt>(makeSpan(start_loc), table_name, mode);

    // Consume semicolon
    match(TokenType::SEMICOLON);

    return stmt;
}
```

**Add to parseStatement()**:
```cpp
case TokenType::KW_TRUNCATE:
    return parseTruncateTable();
```

---

### Step 3: Bytecode Generation

**File**: `src/sblr/bytecode_generator.cpp`

**Add visitor**:
```cpp
void BytecodeGenerator::visit(parser::TruncateTableStmt *node)
{
    // Write opcode
    current_result_->writeOpcode(Opcode::TRUNCATE_TABLE);

    // Write table name
    writeStringId(node->tableName());

    // Write mode (ASYNC=0, SYNC=1)
    current_result_->writeByte(static_cast<uint8_t>(node->mode()));
}
```

**Add to ast.cpp**:
```cpp
void TruncateTableStmt::accept(ASTVisitor *visitor)
{
    visitor->visit(this);
}
```

**Add opcode** (`include/scratchbird/sblr/opcodes.h`):
```cpp
TRUNCATE_TABLE = 0x22,        // Truncate table (ALPHA Phase 1 - DDL Modifications)
```

---

### Step 4: Executor

**File**: `src/sblr/executor.cpp`

```cpp
void Executor::executeTruncateTable()
{
    // Read table name
    std::string table_name = readString();

    // Read mode (0=ASYNC, 1=SYNC)
    uint8_t mode_byte = bytecode_[pc_++];
    bool is_sync = (mode_byte == 1);

    // Get current schema
    core::CatalogManager::SchemaInfo schema_info;
    ErrorContext ctx;
    auto status = db_->catalog_manager()->getSchema(current_schema_, schema_info, &ctx);
    if (status != Status::OK) {
        throw std::runtime_error("Schema not found: " + current_schema_);
    }

    // Get table
    core::CatalogManager::TableInfo table_info;
    status = db_->catalog_manager()->getTable(schema_info.schema_id, table_name, table_info, &ctx);
    if (status != Status::OK) {
        throw std::runtime_error("Table not found: " + table_name);
    }

    // Get current transaction ID
    uint64_t xid = current_transaction_->getXID();

    if (is_sync) {
        // Synchronous mode - blocks
        status = db_->catalog_manager()->truncateTableSync(table_info.table_id, table_name, xid, &ctx);
        if (status != Status::OK) {
            throw std::runtime_error("TRUNCATE TABLE failed");
        }
    } else {
        // Asynchronous mode - returns job ID
        uint64_t job_id = db_->catalog_manager()->truncateTableAsync(table_info.table_id, table_name, xid, &ctx);

        // TODO: Return job_id to user (need result mechanism)
        // For now, just log it
        std::cout << "TRUNCATE TABLE job started (ID: " << job_id << ")" << std::endl;
    }
}
```

**Add to executeStatement() switch**:
```cpp
case Opcode::TRUNCATE_TABLE:
    executeTruncateTable();
    break;
```

---

## SQL Examples

```sql
-- Async truncate (default, returns immediately)
TRUNCATE TABLE large_table;
-- Output: TRUNCATE TABLE job started (ID: 42)

-- Explicit async
TRUNCATE TABLE large_table ASYNC;

-- Synchronous (blocks until complete)
TRUNCATE TABLE small_table SYNC;

-- Legacy syntax support (TABLE keyword optional)
TRUNCATE users;
TRUNCATE TABLE users;
```

---

## Testing Plan

### Unit Tests
1. TRUNCATE empty table
2. TRUNCATE table with 1000 rows
3. ASYNC vs SYNC modes
4. Concurrent INSERT during TRUNCATE
5. Multiple concurrent TRUNCATEs
6. Job status tracking
7. Error handling (non-existent table)

### Performance Test
```cpp
// Create table with 1M rows
// Time DELETE vs TRUNCATE
```

---

## Files Modified (Summary)

1. ✅ `include/scratchbird/core/catalog_manager.h` - TruncateJob struct + methods
2. ✅ `include/scratchbird/parser/ast.h` - TRUNCATE_TABLE + TruncateTableStmt
3. ✅ `include/scratchbird/parser/token.h` - KW_TRUNCATE
4. ✅ `src/parser/lexer.cpp` - TRUNCATE keyword mapping
5. ⏳ `include/scratchbird/parser/ast.h` - TruncateTableStmt class (NEEDS mode field)
6. ⏳ `src/core/catalog_manager.cpp` - Implement 5 methods
7. ⏳ `src/parser/parser.cpp` - parseTruncateTable()
8. ⏳ `src/parser/ast.cpp` - TruncateTableStmt::accept()
9. ⏳ `include/scratchbird/sblr/opcodes.h` - TRUNCATE_TABLE opcode
10. ⏳ `src/sblr/bytecode_generator.cpp` - visit(TruncateTableStmt*)
11. ⏳ `include/scratchbird/sblr/bytecode_visitor.h` - visit() declaration
12. ⏳ `src/sblr/executor.cpp` - executeTruncateTable()
13. ⏳ Need KW_SYNC, KW_ASYNC keywords

---

## Next Steps

1. Add KW_SYNC, KW_ASYNC keywords
2. Update TruncateTableStmt to include mode field
3. Implement catalog methods in catalog_manager.cpp
4. Implement parser method
5. Implement bytecode visitor
6. Implement executor method
7. Build and test

**Current Status**: Headers complete, ready for implementation

**Estimated Remaining**: 12-15 hours
