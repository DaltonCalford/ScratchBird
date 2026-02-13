# TRUNCATE TABLE ASYNC - Implementation Code

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


This document contains all the code to implement. Apply in order.

---

## 1. Parser Implementation (parser.cpp)

Add parseTruncateTable() method after parseDropIndex():

```cpp
Statement *Parser::parseTruncateTable()
{
    auto start_loc = current().location;

    // TRUNCATE already consumed by parseStatement()
    // Optional TABLE keyword
    if (match(TokenType::KW_TABLE))
    {
        advance();
    }

    // Get table name
    if (!check(TokenType::IDENTIFIER))
    {
        error("Expected table name after TRUNCATE TABLE");
        synchronize();
        return nullptr;
    }

    auto table_name = current().value.string_id;
    advance();

    // Check for ASYNC/SYNC mode (default is ASYNC)
    auto mode = TruncateTableStmt::TruncateMode::ASYNC;

    if (match(TokenType::KW_SYNC))
    {
        advance();
        mode = TruncateTableStmt::TruncateMode::SYNC;
    }
    else if (match(TokenType::KW_ASYNC))
    {
        advance();
        mode = TruncateTableStmt::TruncateMode::ASYNC;
    }

    // Create AST node
    auto *stmt = arena_.make<TruncateTableStmt>(makeSpan(start_loc), table_name, mode);

    // Consume semicolon if present
    match(TokenType::SEMICOLON);

    return stmt;
}
```

Add to parseStatement() switch (after case TokenType::KW_DROP):

```cpp
case TokenType::KW_TRUNCATE:
    return parseTruncateTable();
```

Add declaration to parser.h (after parseDropIndex):

```cpp
Statement *parseTruncateTable();
```

---

## 2. AST Visitor (ast.cpp)

Add after DropIndexStmt::accept():

```cpp
void TruncateTableStmt::accept(ASTVisitor *visitor)
{
    visitor->visit(this);
}
```

---

## 3. Bytecode Generator (bytecode_generator.cpp)

Add visitor after visit(DropIndexStmt*):

```cpp
void BytecodeGenerator::visit(parser::TruncateTableStmt *node)
{
    // Write opcode
    current_result_->writeOpcode(Opcode::TRUNCATE_TABLE);

    // Write table name
    writeStringId(node->tableName());

    // Write mode (0=ASYNC, 1=SYNC)
    current_result_->writeByte(static_cast<uint8_t>(node->mode()));
}
```

Add declaration to bytecode_visitor.h:

```cpp
virtual void visit(parser::TruncateTableStmt *node) = 0;
```

---

## 4. Executor (executor.cpp)

Add executeTruncateTable() after executeAlterTable():

```cpp
void Executor::executeTruncateTable()
{
    // Read table name from bytecode
    std::string table_name = readString();

    // Read mode (0=ASYNC, 1=SYNC)
    uint8_t mode_byte = bytecode_[pc_++];
    bool is_sync = (mode_byte == 1);

    // Get current schema
    core::CatalogManager::SchemaInfo schema_info;
    ErrorContext ctx;
    auto status = db_->catalog_manager()->getSchema(current_schema_, schema_info, &ctx);
    if (status != Status::OK)
    {
        throw std::runtime_error("Schema not found: " + current_schema_);
    }

    // Get table info
    core::CatalogManager::TableInfo table_info;
    status = db_->catalog_manager()->getTable(schema_info.schema_id, table_name, table_info, &ctx);
    if (status != Status::OK)
    {
        throw std::runtime_error("Table not found: " + table_name);
    }

    // Get current transaction ID
    uint64_t xid = current_transaction_ ? current_transaction_->getXID() : 1;

    if (is_sync)
    {
        // Synchronous mode - blocks until complete
        status = db_->catalog_manager()->truncateTableSync(table_info.table_id, table_name, xid, &ctx);
        if (status != Status::OK)
        {
            throw std::runtime_error("TRUNCATE TABLE SYNC failed");
        }
        std::cout << "TRUNCATE TABLE completed" << std::endl;
    }
    else
    {
        // Asynchronous mode - returns job ID
        uint64_t job_id = db_->catalog_manager()->truncateTableAsync(table_info.table_id, table_name, xid, &ctx);
        std::cout << "TRUNCATE TABLE job started (ID: " << job_id << ")" << std::endl;
    }
}
```

Add to executeStatement() switch (after case Opcode::ALTER_TABLE):

```cpp
case Opcode::TRUNCATE_TABLE:
    executeTruncateTable();
    break;
```

Add declaration to executor.h:

```cpp
void executeTruncateTable();
```

---

## 5. Catalog Manager Implementation (catalog_manager.cpp)

Add at end of file (before closing namespace):

```cpp
// ============================================================================
// TRUNCATE TABLE ASYNC Implementation (ALPHA Phase 1 - DDL Modifications)
// ============================================================================

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
            ErrorContext ctx;

            // Pin table catalog page
            auto table_catalog_gpid = makeGPID(PRIMARY_TABLESPACE_ID, TABLE_CATALOG_PAGE);
            auto table_pin = db_->buffer_pool()->pinPage(table_catalog_gpid, &ctx);
            if (!table_pin)
            {
                job->error = true;
                job->error_message = "Failed to pin table catalog page";
                job->completed = true;
                return;
            }

            HeapPage table_catalog_page(table_pin.frame()->data());

            // Find table record to get first_heap_gpid
            GPID first_heap_gpid;
            bool table_found = false;

            auto item_ids = table_catalog_page.getItemIds();
            for (const auto &item_id : item_ids)
            {
                if (!table_catalog_page.isItemValid(item_id))
                    continue;

                auto tuple_data = table_catalog_page.getTuple(item_id);
                TableRecord table_rec;
                std::memcpy(&table_rec, tuple_data, sizeof(TableRecord));

                if (table_rec.table_id == job->table_id && table_rec.is_valid)
                {
                    first_heap_gpid = table_rec.first_heap_gpid;
                    table_found = true;
                    break;
                }
            }

            table_pin.unpin();

            if (!table_found)
            {
                job->error = true;
                job->error_message = "Table not found";
                job->completed = true;
                return;
            }

            // Iterate all heap pages
            GPID current_gpid = first_heap_gpid;
            while (current_gpid.page_id != 0)
            {
                auto heap_pin = db_->buffer_pool()->pinPage(current_gpid, &ctx);
                if (!heap_pin) break;

                HeapPage heap_page(heap_pin.frame()->data());
                auto heap_item_ids = heap_page.getItemIds();

                for (const auto &item_id : heap_item_ids)
                {
                    if (!heap_page.isItemValid(item_id))
                        continue;

                    job->rows_processed++;

                    // Get tuple TID
                    TID tid = heap_page.getTID(item_id);

                    // Only delete rows committed BEFORE truncate started
                    // This is MGA-compliant: respects transaction visibility
                    if (db_->tip()->isVersionVisible(tid.xmin, job->snapshot_xid))
                    {
                        // Soft delete: set xmax (MGA-compliant)
                        heap_page.setTupleXMax(item_id, job->snapshot_xid);
                        job->rows_deleted++;
                    }
                }

                heap_pin.markDirty();
                GPID next_gpid = heap_page.getNextPage();
                heap_pin.unpin();

                current_gpid = next_gpid;

                // Yield CPU to avoid hogging resources
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            // Update table metadata: set row_count = 0
            table_pin = db_->buffer_pool()->pinPage(table_catalog_gpid, &ctx);
            if (table_pin)
            {
                HeapPage table_catalog_page2(table_pin.frame()->data());
                auto item_ids2 = table_catalog_page2.getItemIds();

                for (const auto &item_id : item_ids2)
                {
                    if (!table_catalog_page2.isItemValid(item_id))
                        continue;

                    auto tuple_data = table_catalog_page2.getTuple(item_id);
                    TableRecord table_rec;
                    std::memcpy(&table_rec, tuple_data, sizeof(TableRecord));

                    if (table_rec.table_id == job->table_id && table_rec.is_valid)
                    {
                        table_rec.row_count = 0;
                        table_rec.last_modified_time = std::time(nullptr);
                        std::memcpy(const_cast<uint8_t *>(tuple_data), &table_rec, sizeof(TableRecord));
                        table_pin.markDirty();
                        break;
                    }
                }
                table_pin.unpin();
            }

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

auto CatalogManager::truncateTableSync(const ID &table_id, const std::string &table_name,
                                        uint64_t snapshot_xid, ErrorContext *ctx) -> Status
{
    // Start async job
    auto job_id = truncateTableAsync(table_id, table_name, snapshot_xid, ctx);

    // Wait for completion (no timeout)
    return waitForTruncate(job_id, 0);
}

auto CatalogManager::getTruncateJobStatus(uint64_t job_id) -> std::shared_ptr<TruncateJob>
{
    std::lock_guard<std::mutex> lock(truncate_jobs_mutex_);
    auto it = truncate_jobs_.find(job_id);
    if (it != truncate_jobs_.end())
    {
        return it->second;
    }
    return nullptr;
}

auto CatalogManager::waitForTruncate(uint64_t job_id, uint32_t timeout_ms) -> Status
{
    auto job = getTruncateJobStatus(job_id);
    if (!job)
    {
        return Status::NOT_FOUND;
    }

    auto start = std::chrono::steady_clock::now();

    while (!job->completed.load())
    {
        // Check timeout
        if (timeout_ms > 0)
        {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed > std::chrono::milliseconds(timeout_ms))
            {
                return Status::TIMEOUT;
            }
        }

        // Wait a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Check if error occurred
    if (job->error.load())
    {
        return Status::INTERNAL_ERROR;
    }

    return Status::OK;
}

auto CatalogManager::listTruncateJobs(std::vector<std::shared_ptr<TruncateJob>> &jobs_out) -> void
{
    std::lock_guard<std::mutex> lock(truncate_jobs_mutex_);
    for (const auto &[job_id, job] : truncate_jobs_)
    {
        jobs_out.push_back(job);
    }
}
```

---

## Files to Modify Summary

1. src/parser/parser.cpp - Add parseTruncateTable() + switch case
2. include/scratchbird/parser/parser.h - Add method declaration
3. src/parser/ast.cpp - Add TruncateTableStmt::accept()
4. src/sblr/bytecode_generator.cpp - Add visitor
5. include/scratchbird/sblr/bytecode_visitor.h - Add visit() declaration
6. src/sblr/executor.cpp - Add executeTruncateTable() + switch case
7. include/scratchbird/sblr/executor.h - Add method declaration
8. src/core/catalog_manager.cpp - Add 5 methods (largest change)

**Total**: 8 files
