# Offline Table Migration - Session Todo Lists

**Feature**: `ALTER TABLE ... SET TABLESPACE` (Offline Mode)
**Total Estimated Time**: 20-28 hours across 4 sessions
**Status**: 📋 READY TO START

---

## Quick Navigation

- [Session 1: Parser and AST Foundation](#session-1-parser-and-ast-foundation) (3-4 hours)
- [Session 2: Core Migration Engine](#session-2-core-migration-engine) (10-12 hours)
- [Session 3: Index Updates and Progress](#session-3-index-updates-and-progress) (5-7 hours)
- [Session 4: Executor Integration](#session-4-executor-integration-and-testing) (3-5 hours)

---

## Session 1: Parser and AST Foundation

**Goal**: Add language support for `ALTER TABLE ... SET TABLESPACE`
**Estimated Time**: 3-4 hours
**Dependencies**: None
**Status**: ⏸️ NOT STARTED

### Pre-Session Checklist
- [ ] Read `OFFLINE_TABLE_MIGRATION_DESIGN.md` (this provides full context)
- [ ] Ensure clean build: `make clean && make`
- [ ] Create feature branch: `git checkout -b feature/offline-table-migration`

### Task Breakdown

#### Task 1.1: Locate and Study Parser Files (30 minutes)
- [ ] Find grammar file location
  ```bash
  find . -name "grammar.y" -o -name "*.y"
  ```
- [ ] Find lexer file location
  ```bash
  find . -name "*.l" -o -name "lexer.*"
  ```
- [ ] Study existing `ALTER TABLESPACE` syntax for reference
  ```bash
  grep -n "ALTER.*TABLESPACE" sql/parser/grammar.y
  ```
- [ ] Identify where `Statement` classes are defined
  ```bash
  grep -n "class.*Stmt.*Statement" include/scratchbird/parser/ast.h | head -20
  ```

#### Task 1.2: Add Keywords to Lexer (15 minutes)
**File**: `sql/parser/lexer.l` (or equivalent)

- [ ] Check if `ALTER` keyword exists (likely yes)
- [ ] Check if `TABLE` keyword exists (likely yes)
- [ ] Add `SET` keyword if not present
  ```lex
  "SET"       { return TOKEN_SET; }
  ```
- [ ] Add `ONLINE` keyword
  ```lex
  "ONLINE"    { return TOKEN_ONLINE; }
  ```
- [ ] Update token list in header file if necessary

#### Task 1.3: Add Grammar Rules (60 minutes)
**File**: `sql/parser/grammar.y`

- [ ] Find the location where statements are defined
- [ ] Add `alter_table_stmt` production:
  ```yacc
  statement:
      /* ... existing rules ... */
      | alter_table_stmt
      ;

  alter_table_stmt:
      ALTER TABLE table_name SET TABLESPACE identifier
      {
          $$ = new AlterTableSetTablespaceStmt(@$, $3, $6, false);
      }
      | ALTER TABLE table_name SET TABLESPACE identifier ONLINE
      {
          $$ = new AlterTableSetTablespaceStmt(@$, $3, $6, true);
      }
      ;

  table_name:
      identifier
      {
          $$ = $1;
      }
      ;
  ```
- [ ] Add token declarations at top of file:
  ```yacc
  %token TOKEN_ALTER TOKEN_TABLE TOKEN_SET TOKEN_ONLINE
  ```
- [ ] Add type declarations:
  ```yacc
  %type <stmt> alter_table_stmt
  %type <string_id> table_name
  ```

#### Task 1.4: Create AST Node (45 minutes)
**File**: `include/scratchbird/parser/ast.h`

- [ ] Find where `Statement` classes are defined (search for "class CreateTableStmt")
- [ ] Add new class after similar statement classes:
  ```cpp
  /**
   * AlterTableSetTablespaceStmt - ALTER TABLE ... SET TABLESPACE statement
   *
   * Syntax: ALTER TABLE table_name SET TABLESPACE tablespace_name [ONLINE]
   *
   * Phase 4: ONLINE clause parsed but rejected (offline only)
   * Phase 5: ONLINE clause will enable online migration
   */
  class AlterTableSetTablespaceStmt : public Statement
  {
  public:
      AlterTableSetTablespaceStmt(
          const SourceSpan &span,
          StringPool::StringId table_name,
          StringPool::StringId tablespace_name,
          bool online
      )
          : Statement(span)
          , table_name_(table_name)
          , tablespace_name_(tablespace_name)
          , online_(online)
      {
      }

      void accept(ASTVisitor *visitor) override
      {
          visitor->visit(this);
      }

      // Accessors
      StringPool::StringId tableName() const { return table_name_; }
      StringPool::StringId tablespaceName() const { return tablespace_name_; }
      bool online() const { return online_; }

  private:
      StringPool::StringId table_name_;       // Table to move
      StringPool::StringId tablespace_name_;  // Target tablespace
      bool online_;                           // true if ONLINE clause present
  };
  ```

#### Task 1.5: Add Visitor Interface (15 minutes)
**File**: `include/scratchbird/parser/ast.h`

- [ ] Find `ASTVisitor` class definition (search for "class ASTVisitor")
- [ ] Add visitor method declaration:
  ```cpp
  class ASTVisitor
  {
  public:
      // ... existing methods ...

      virtual void visit(AlterTableSetTablespaceStmt *node) = 0;  // Phase 4 Task 4.1.1
  };
  ```

#### Task 1.6: Add Opcode (10 minutes)
**File**: `include/scratchbird/sblr/opcodes.h`

- [ ] Find `enum class Opcode`
- [ ] Add new opcode (choose next available number):
  ```cpp
  enum class Opcode : uint8_t
  {
      // ... existing opcodes ...

      OP_ALTER_TABLE_SET_TABLESPACE = 0x??,  // Phase 4 Task 4.1.1 - Table migration
  };
  ```
- [ ] Update opcode documentation/comments if present

#### Task 1.7: Implement Bytecode Generation (45 minutes)
**File**: `include/scratchbird/sblr/bytecode_generator.h`

- [ ] Add visitor method declaration:
  ```cpp
  class BytecodeGenerator : public parser::ASTVisitor
  {
  public:
      // ... existing methods ...

      void visit(parser::AlterTableSetTablespaceStmt *node) override;  // Phase 4 Task 4.1.1
  };
  ```

**File**: `src/scratchbird/sblr/bytecode_generator.cpp`

- [ ] Implement visitor method:
  ```cpp
  void BytecodeGenerator::visit(parser::AlterTableSetTablespaceStmt *node)
  {
      // Write opcode
      current_result_->writeOpcode(Opcode::OP_ALTER_TABLE_SET_TABLESPACE);

      // Write table name (string)
      writeStringId(node->tableName());

      // Write tablespace name (string)
      writeStringId(node->tablespaceName());

      // Write online flag (1 byte: 0=offline, 1=online)
      current_result_->writeByte(node->online() ? 1 : 0);
  }
  ```

#### Task 1.8: Build and Test (30 minutes)

- [ ] Build the project:
  ```bash
  make clean && make
  ```
- [ ] Fix any compilation errors (common issues):
  - Missing includes
  - Typos in class names
  - Token/type mismatches in grammar
  - Bison/Flex version issues

- [ ] Test parser with simple input (if test harness exists):
  ```sql
  ALTER TABLE test_table SET TABLESPACE test_ts;
  ALTER TABLE test_table SET TABLESPACE test_ts ONLINE;
  ```

- [ ] Verify AST node created (add debug logging if needed)
- [ ] Verify bytecode generated (inspect bytecode output)

### Session 1 Deliverables Checklist

- [ ] Parser accepts `ALTER TABLE ... SET TABLESPACE` syntax
- [ ] Parser accepts optional `ONLINE` clause
- [ ] `AlterTableSetTablespaceStmt` AST node created with correct fields
- [ ] Bytecode opcode defined
- [ ] Bytecode generation implemented
- [ ] Project compiles without errors
- [ ] Basic manual testing passed

### Session 1 Completion Criteria

- [ ] All tasks marked complete
- [ ] Clean build: `make` succeeds
- [ ] Git commit created:
  ```bash
  git add .
  git commit -m "Implement Phase 4 Task 4.1.1: Add ALTER TABLE SET TABLESPACE parser support

  - Added ALTER TABLE ... SET TABLESPACE [ONLINE] grammar
  - Created AlterTableSetTablespaceStmt AST node
  - Added OP_ALTER_TABLE_SET_TABLESPACE opcode
  - Implemented bytecode generation
  - Parser accepts both offline and online syntax
  - ONLINE clause will be rejected in executor (Phase 4)

  Phase 4 Task 4.1.1 Complete
  Session 1 of 4 complete

  🤖 Generated with Claude Code"
  ```

### Troubleshooting

**Issue**: Bison conflicts or parser errors
- **Solution**: Check grammar precedence, ensure all tokens declared

**Issue**: AST node not found during bytecode generation
- **Solution**: Verify visitor method signature matches exactly

**Issue**: Linker errors
- **Solution**: Check that all new methods have implementations

---

## Session 2: Core Migration Engine

**Goal**: Implement the core table migration logic
**Estimated Time**: 10-12 hours
**Dependencies**: Session 1 complete
**Status**: ⏸️ NOT STARTED

### Pre-Session Checklist
- [ ] Session 1 complete and committed
- [ ] Read Session 2 section of `OFFLINE_TABLE_MIGRATION_DESIGN.md`
- [ ] Clean build verified: `make`

### Task Breakdown

#### Task 2.1: Add Method Declaration (30 minutes)
**File**: `include/scratchbird/core/catalog_manager.h`

- [ ] Find CatalogManager class definition
- [ ] Add method declaration (around line 300-400, near other table operations):
  ```cpp
  /**
   * moveTableToTablespace - Move table to different tablespace (OFFLINE)
   *
   * See docs/planning/OFFLINE_TABLE_MIGRATION_DESIGN.md for full design.
   *
   * @param table_id Table ID to move
   * @param target_tablespace_id Destination tablespace
   * @param online If true, rejected with NOT_IMPLEMENTED (Phase 5)
   * @param progress_callback Optional progress callback (pages_copied, total_pages)
   * @param ctx Error context
   * @return Status::OK on success
   */
  Status moveTableToTablespace(
      uint32_t table_id,
      uint16_t target_tablespace_id,
      bool online,
      std::function<bool(uint32_t, uint32_t)> progress_callback = nullptr,
      ErrorContext *ctx = nullptr
  );
  ```

- [ ] Add helper method declarations:
  ```cpp
  // Helper: Resolve table name to table_id
  uint32_t resolveTableId(const std::string &table_name, ErrorContext *ctx = nullptr);

  // Helper: Resolve tablespace name to tablespace_id
  uint16_t resolveTablespaceId(const std::string &tablespace_name, ErrorContext *ctx = nullptr);
  ```

#### Task 2.2: Add Data Structures (45 minutes)
**File**: `src/core/catalog_manager.cpp` (near top, in anonymous namespace or private section)

- [ ] Add TID mapping structure:
  ```cpp
  namespace
  {
      /**
       * TIDMapping - Maps old GPIDs to new GPIDs during table migration
       */
      struct TIDMapping
      {
          std::unordered_map<GPID, GPID> page_mapping;

          // Map a TID (GPID + slot) from old to new
          TID mapTID(const TID &old_tid) const
          {
              auto it = page_mapping.find(old_tid.gpid);
              if (it == page_mapping.end())
              {
                  throw std::runtime_error("TID mapping not found for page");
              }

              TID new_tid;
              new_tid.gpid = it->second;
              new_tid.slot = old_tid.slot;  // Slot unchanged
              return new_tid;
          }

          // Record a page move
          void recordMove(GPID old_gpid, GPID new_gpid)
          {
              page_mapping[old_gpid] = new_gpid;
          }
      };
  }
  ```

- [ ] Add progress tracking structure:
  ```cpp
  namespace
  {
      struct MigrationProgress
      {
          uint32_t total_pages = 0;
          uint32_t pages_copied = 0;
          uint32_t total_indexes = 0;
          uint32_t indexes_updated = 0;
          std::chrono::time_point<std::chrono::steady_clock> start_time;
          std::chrono::time_point<std::chrono::steady_clock> last_log_time;

          bool shouldLog() const
          {
              auto now = std::chrono::steady_clock::now();
              auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                  now - last_log_time);
              return elapsed.count() >= 5;  // Log every 5 seconds
          }

          void logProgress() const
          {
              auto now = std::chrono::steady_clock::now();
              auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                  now - start_time);
              double pages_per_sec = pages_copied /
                  std::max(1.0, static_cast<double>(elapsed.count()));

              LOG_INFO(CATALOG,
                      "Migration progress: %u/%u pages (%.1f%%), "
                      "%u/%u indexes, %.1f pages/sec",
                      pages_copied, total_pages,
                      (pages_copied * 100.0) / std::max(1u, total_pages),
                      indexes_updated, total_indexes,
                      pages_per_sec);
          }
      };
  }
  ```

#### Task 2.3: Implement Helper Methods (60 minutes)
**File**: `src/core/catalog_manager.cpp`

- [ ] Implement `resolveTableId`:
  ```cpp
  uint32_t CatalogManager::resolveTableId(const std::string &table_name, ErrorContext *ctx)
  {
      std::lock_guard<std::mutex> lock(mutex_);

      // Search table_cache_ for matching name
      for (const auto &pair : table_cache_)
      {
          if (pair.second.table_name == table_name)
          {
              return pair.first;  // Return table_id
          }
      }

      SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                       ("Table not found: " + table_name).c_str());
      return INVALID_TABLE_ID;  // Define constant: static constexpr uint32_t INVALID_TABLE_ID = 0;
  }
  ```

- [ ] Implement `resolveTablespaceId`:
  ```cpp
  uint16_t CatalogManager::resolveTablespaceId(const std::string &tablespace_name, ErrorContext *ctx)
  {
      std::lock_guard<std::mutex> lock(mutex_);

      // Search tablespace_cache_ for matching name
      for (const auto &pair : tablespace_cache_)
      {
          if (pair.second.name == tablespace_name)
          {
              return pair.first;  // Return tablespace_id
          }
      }

      SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                       ("Tablespace not found: " + tablespace_name).c_str());
      return INVALID_TABLESPACE_ID;  // Define constant
  }
  ```

#### Task 2.4: Implement Migration Skeleton (2-3 hours)
**File**: `src/core/catalog_manager.cpp`

- [ ] Create method skeleton with all 8 steps outlined:
  ```cpp
  Status CatalogManager::moveTableToTablespace(
      uint32_t table_id,
      uint16_t target_tablespace_id,
      bool online,
      std::function<bool(uint32_t, uint32_t)> progress_callback,
      ErrorContext *ctx)
  {
      // STEP 0: Reject ONLINE mode
      if (online)
      {
          SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                           "ONLINE table migration not supported in Phase 4");
          return Status::NOT_IMPLEMENTED;
      }

      // STEP 1: Acquire EXCLUSIVE lock
      LOG_INFO(CATALOG, "Acquiring EXCLUSIVE lock on table %u", table_id);
      // TODO: Implement lock manager integration

      // STEP 2: Validate inputs
      std::lock_guard<std::mutex> lock(mutex_);

      auto table_it = table_cache_.find(table_id);
      if (table_it == table_cache_.end())
      {
          SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Table not found");
          return Status::NOT_FOUND;
      }

      TableInfo &table_info = table_it->second;
      uint16_t source_tablespace_id = table_info.tablespace_id;

      // Check if already in target
      if (source_tablespace_id == target_tablespace_id)
      {
          LOG_INFO(CATALOG, "Table already in target tablespace");
          return Status::OK;
      }

      // Validate target tablespace exists
      if (tablespace_cache_.find(target_tablespace_id) == tablespace_cache_.end())
      {
          SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Target tablespace not found");
          return Status::NOT_FOUND;
      }

      LOG_INFO(CATALOG,
              "Starting migration: table=%u, from_ts=%u, to_ts=%u",
              table_id, source_tablespace_id, target_tablespace_id);

      // STEP 3: Initialize progress
      MigrationProgress progress;
      progress.start_time = std::chrono::steady_clock::now();
      progress.last_log_time = progress.start_time;

      // TODO: Count total pages and indexes
      progress.total_pages = 0;  // Implement in Task 2.5
      progress.total_indexes = 0;  // Implement in Task 2.6

      // STEP 4: Copy heap pages
      TIDMapping tid_mapping;
      // TODO: Implement in Task 2.5

      // STEP 5: Update indexes
      // TODO: Implement in Session 3

      // STEP 6: Update catalog
      table_info.tablespace_id = target_tablespace_id;
      Status status = writeTableRecord(table_info, ctx);
      if (status != Status::OK)
      {
          LOG_ERROR(CATALOG, "Failed to update catalog");
          return status;
      }

      // STEP 7: Free old pages
      // TODO: Implement in Task 2.7

      // STEP 8: Release lock
      LOG_INFO(CATALOG, "Migration complete");

      return Status::OK;
  }
  ```

#### Task 2.5: Implement Heap Page Copying (4-5 hours)

This is the core of the migration logic. Break it down into sub-tasks:

**Sub-task 2.5.1**: Count and locate heap pages (60 min)
- [ ] Determine how to find all heap pages for a table
  - Check TableInfo structure for heap_start_page or similar
  - May need to scan from first page until end marker
- [ ] Implement page counting:
  ```cpp
  // Count heap pages (scan table extent)
  uint32_t heap_start_page = table_info.first_page_id;  // Or similar field
  GPID current_gpid = makeGPID(source_tablespace_id, heap_start_page);

  std::vector<GPID> heap_pages;

  // Scan linked list of heap pages
  while (current_gpid.page_number != 0)
  {
      heap_pages.push_back(current_gpid);

      // Read page to get next page pointer
      // TODO: Implement page reading logic
      // current_gpid = page->next_page;
  }

  progress.total_pages = heap_pages.size();
  LOG_INFO(CATALOG, "Found %u heap pages to migrate", progress.total_pages);
  ```

**Sub-task 2.5.2**: Allocate pages in target tablespace (90 min)
- [ ] For each heap page, allocate new page in target:
  ```cpp
  for (const GPID &old_gpid : heap_pages)
  {
      // Allocate new page
      GPID new_gpid;
      Status status = page_mgr_->allocatePageInTablespace(
          target_tablespace_id, &new_gpid, ctx);

      if (status != Status::OK)
      {
          LOG_ERROR(CATALOG, "Failed to allocate page in target tablespace");
          // TODO: Rollback - free all allocated pages
          return status;
      }

      // Record mapping
      tid_mapping.recordMove(old_gpid, new_gpid);

      LOG_DEBUG(CATALOG, "Allocated page: old=%lu, new=%lu",
               old_gpid.raw(), new_gpid.raw());
  }
  ```

**Sub-task 2.5.3**: Copy page contents (2-3 hours)
- [ ] Read source pages and write to target:
  ```cpp
  for (const GPID &old_gpid : heap_pages)
  {
      // Get new GPID from mapping
      GPID new_gpid = tid_mapping.page_mapping[old_gpid];

      // Read source page
      auto page_buffer = std::make_unique<uint8_t[]>(page_size_);
      Status status = readPage(old_gpid, page_buffer.get(), ctx);
      if (status != Status::OK)
      {
          LOG_ERROR(CATALOG, "Failed to read source page");
          return status;
      }

      // Update page header with new GPID
      auto *page_header = reinterpret_cast<PageHeader *>(page_buffer.get());
      page_header->page_id = new_gpid.page_number;
      // Update database_uuid if needed

      // Write to target page
      status = writePage(new_gpid, page_buffer.get(), ctx);
      if (status != Status::OK)
      {
          LOG_ERROR(CATALOG, "Failed to write target page");
          return status;
      }

      // Update progress
      progress.pages_copied++;

      if (progress.shouldLog())
      {
          progress.logProgress();
          progress.last_log_time = std::chrono::steady_clock::now();
      }

      // Check for cancellation
      if (progress_callback && !progress_callback(progress.pages_copied, progress.total_pages))
      {
          LOG_WARNING(CATALOG, "Migration cancelled by user");
          // TODO: Rollback
          return Status::CANCELLED;
      }
  }
  ```

**Sub-task 2.5.4**: Implement page I/O helpers (60 min)
- [ ] Add `readPage()` helper method:
  ```cpp
  Status readPage(GPID gpid, uint8_t *buffer, ErrorContext *ctx)
  {
      // Get file descriptor for tablespace
      int fd = db_->getTablespaceFd(gpid.tablespace_id);
      if (fd < 0)
      {
          SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Tablespace not found");
          return Status::NOT_FOUND;
      }

      // Calculate offset
      off_t offset = static_cast<off_t>(gpid.page_number) * page_size_;

      // Read page
      ssize_t bytes_read = ::pread(fd, buffer, page_size_, offset);
      if (bytes_read != static_cast<ssize_t>(page_size_))
      {
          SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read page");
          return Status::IO_ERROR;
      }

      return Status::OK;
  }
  ```

- [ ] Add `writePage()` helper method:
  ```cpp
  Status writePage(GPID gpid, const uint8_t *buffer, ErrorContext *ctx)
  {
      int fd = db_->getTablespaceFd(gpid.tablespace_id);
      if (fd < 0)
      {
          SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Tablespace not found");
          return Status::NOT_FOUND;
      }

      off_t offset = static_cast<off_t>(gpid.page_number) * page_size_;

      ssize_t bytes_written = ::pwrite(fd, buffer, page_size_, offset);
      if (bytes_written != static_cast<ssize_t>(page_size_))
      {
          SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write page");
          return Status::IO_ERROR;
      }

      return Status::OK;
  }
  ```

#### Task 2.6: Count Indexes (30 minutes)
- [ ] Count indexes on this table:
  ```cpp
  // Count indexes (before main migration loop)
  for (const auto &idx_pair : index_cache_)
  {
      if (idx_pair.second.table_id == table_id)
      {
          progress.total_indexes++;
      }
  }

  LOG_INFO(CATALOG, "Found %u indexes to update", progress.total_indexes);
  ```

#### Task 2.7: Free Old Pages (60 minutes)
- [ ] After successful migration, free old heap pages:
  ```cpp
  // Free old heap pages from source tablespace
  LOG_INFO(CATALOG, "Freeing %u old pages from tablespace %u",
          static_cast<uint32_t>(heap_pages.size()), source_tablespace_id);

  for (const GPID &old_gpid : heap_pages)
  {
      Status status = page_mgr_->freePageGlobal(old_gpid, ctx);
      if (status != Status::OK)
      {
          // Log warning but don't fail migration
          LOG_WARNING(CATALOG, "Failed to free old page %lu", old_gpid.raw());
      }
  }
  ```

#### Task 2.8: Implement Basic Rollback (90 minutes)
- [ ] Add rollback helper method:
  ```cpp
  void rollbackTableMigration(
      const std::vector<GPID> &allocated_pages,
      uint16_t target_tablespace_id)
  {
      LOG_WARNING(CATALOG, "Rolling back table migration");

      // Free all newly allocated pages
      for (const GPID &gpid : allocated_pages)
      {
          page_mgr_->freePageGlobal(gpid, nullptr);
      }

      LOG_WARNING(CATALOG, "Rollback complete");
  }
  ```

- [ ] Call rollback on errors:
  ```cpp
  // In error paths:
  std::vector<GPID> allocated_pages;  // Track allocations

  // ... on error ...
  rollbackTableMigration(allocated_pages, target_tablespace_id);
  return status;
  ```

#### Task 2.9: Build and Test (60 minutes)

- [ ] Build project:
  ```bash
  make clean && make
  ```

- [ ] Fix compilation errors
- [ ] Create simple test (if possible):
  ```sql
  CREATE TABLESPACE ts1 ...;
  CREATE TABLE t1 (id INT) TABLESPACE ts1;
  INSERT INTO t1 VALUES (1), (2), (3);

  CREATE TABLESPACE ts2 ...;
  ALTER TABLE t1 SET TABLESPACE ts2;  -- Should copy pages

  SELECT * FROM t1;  -- Verify data accessible
  ```

### Session 2 Deliverables Checklist

- [ ] `moveTableToTablespace()` method implemented
- [ ] Helper methods: `resolveTableId()`, `resolveTablespaceId()`
- [ ] Heap page scanning and counting works
- [ ] Page allocation in target tablespace works
- [ ] Page copying preserves data
- [ ] TID mapping constructed correctly
- [ ] Old pages freed from source tablespace
- [ ] Catalog updated with new tablespace_id
- [ ] Basic rollback on errors
- [ ] Progress logging appears
- [ ] Project compiles

### Session 2 Completion Criteria

- [ ] Can migrate a simple table (no indexes) between tablespaces
- [ ] All rows accessible after migration
- [ ] Old pages freed
- [ ] Git commit:
  ```bash
  git add .
  git commit -m "Implement Phase 4 Task 4.1.2: Core table migration engine

  - Implemented moveTableToTablespace() with 8-step process
  - Added heap page scanning, allocation, and copying
  - Built TID mapping (old_gpid -> new_gpid)
  - Implemented page I/O helpers (readPage, writePage)
  - Added progress tracking and logging
  - Basic rollback on errors
  - Catalog update with new tablespace_id
  - Old page freeing

  Session 2 of 4 complete

  🤖 Generated with Claude Code"
  ```

---

## Session 3: Index Updates and Progress

**Goal**: Complete index support and add advanced progress features
**Estimated Time**: 5-7 hours
**Dependencies**: Session 2 complete
**Status**: ⏸️ NOT STARTED

### Pre-Session Checklist
- [ ] Session 2 complete and committed
- [ ] Read Session 3 section of `OFFLINE_TABLE_MIGRATION_DESIGN.md`
- [ ] Understand index types in codebase

### Task Breakdown

#### Task 3.1: Study Index Structures (60 minutes)
- [ ] Identify all index types in codebase:
  ```bash
  grep -r "enum.*IndexType" include/
  ```
- [ ] Find index-specific header files:
  ```bash
  ls include/scratchbird/index/
  ```
- [ ] Understand how TIDs are stored in each index type:
  - B-Tree: TIDs in leaf nodes
  - Hash: TIDs in bucket pages
  - GIN: TIDs in posting lists
  - Bitmap: Positional encoding
  - BRIN: Range summaries
  - HNSW: Neighbor lists

#### Task 3.2: Implement updateIndexTIDs Dispatcher (60 minutes)
**File**: `src/core/catalog_manager.cpp`

- [ ] Add method to update index TIDs:
  ```cpp
  Status updateIndexTIDs(
      uint32_t index_id,
      const TIDMapping &tid_mapping,
      ErrorContext *ctx)
  {
      auto it = index_cache_.find(index_id);
      if (it == index_cache_.end())
      {
          SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Index not found");
          return Status::NOT_FOUND;
      }

      const IndexInfo &idx_info = it->second;

      LOG_INFO(CATALOG, "Updating index %u (type=%d)",
              index_id, static_cast<int>(idx_info.index_type));

      // Dispatch to type-specific handler
      switch (idx_info.index_type)
      {
          case IndexType::BTREE:
              return updateBTreeIndexTIDs(index_id, tid_mapping, ctx);
          case IndexType::HASH:
              return updateHashIndexTIDs(index_id, tid_mapping, ctx);
          case IndexType::GIN:
              return updateGINIndexTIDs(index_id, tid_mapping, ctx);
          case IndexType::BITMAP:
              return updateBitmapIndexTIDs(index_id, tid_mapping, ctx);
          case IndexType::BRIN:
              return updateBRINIndexTIDs(index_id, tid_mapping, ctx);
          case IndexType::HNSW:
              return updateHNSWIndexTIDs(index_id, tid_mapping, ctx);
          default:
              SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                               "Unknown index type");
              return Status::INVALID_ARGUMENT;
      }
  }
  ```

#### Task 3.3: Implement B-Tree Index Updates (2-3 hours)
**File**: `src/core/catalog_manager.cpp`

- [ ] Implement B-Tree TID update:
  ```cpp
  Status updateBTreeIndexTIDs(
      uint32_t index_id,
      const TIDMapping &tid_mapping,
      ErrorContext *ctx)
  {
      // TODO: Get index root page from IndexInfo
      // TODO: Scan all leaf pages
      // TODO: For each TID in leaf:
      //   - Apply tid_mapping.mapTID()
      //   - Update in-place
      // TODO: Mark pages dirty

      LOG_INFO(CATALOG, "Updated B-Tree index %u", index_id);
      return Status::OK;
  }
  ```

**Note**: This may require deep integration with index subsystem. Consider:
- Option A: Direct page manipulation (complex, error-prone)
- Option B: Use index API to rebuild index entries (safer, slower)
- Option C: Defer to index-specific update methods (cleaner)

For Phase 4, **recommend Option C** - add update methods to index classes.

#### Task 3.4: Implement Other Index Type Updates (1-2 hours each)

Each index type needs a handler. Prioritize based on usage:

**Priority 1: Hash Index** (common, simple)
- [ ] Implement `updateHashIndexTIDs()`:
  ```cpp
  Status updateHashIndexTIDs(
      uint32_t index_id,
      const TIDMapping &tid_mapping,
      ErrorContext *ctx)
  {
      // Scan hash buckets
      // Update TIDs in each bucket
      LOG_INFO(CATALOG, "Updated Hash index %u", index_id);
      return Status::OK;
  }
  ```

**Priority 2: GIN Index** (for full-text search)
- [ ] Implement `updateGINIndexTIDs()`

**Priority 3: BRIN Index** (for large tables)
- [ ] Implement `updateBRINIndexTIDs()`

**Priority 4: Bitmap and HNSW** (specialized)
- [ ] Implement `updateBitmapIndexTIDs()`
- [ ] Implement `updateHNSWIndexTIDs()`

**Time-Saving Option**: If index update is too complex:
- [ ] Implement index rebuild instead of in-place update:
  ```cpp
  Status rebuildIndex(uint32_t index_id, ErrorContext *ctx)
  {
      // Drop index entries
      // Re-scan table and rebuild index
      // Simpler but slower
  }
  ```

#### Task 3.5: Integrate Index Updates into Migration (30 minutes)
**File**: `src/core/catalog_manager.cpp` (in `moveTableToTablespace()`)

- [ ] Add index update loop:
  ```cpp
  // STEP 5: Update all indexes
  LOG_INFO(CATALOG, "Updating %u indexes", progress.total_indexes);

  for (const auto &idx_pair : index_cache_)
  {
      if (idx_pair.second.table_id != table_id)
      {
          continue;
      }

      uint32_t index_id = idx_pair.first;

      Status status = updateIndexTIDs(index_id, tid_mapping, ctx);
      if (status != Status::OK)
      {
          LOG_ERROR(CATALOG, "Failed to update index %u", index_id);
          // Rollback
          rollbackTableMigration(allocated_pages, target_tablespace_id);
          return status;
      }

      progress.indexes_updated++;

      if (progress.shouldLog())
      {
          progress.logProgress();
          progress.last_log_time = std::chrono::steady_clock::now();
      }
  }
  ```

#### Task 3.6: Enhance Progress Tracking (60 minutes)

- [ ] Add ETA calculation:
  ```cpp
  void logProgressWithETA() const
  {
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
          now - start_time);

      double pages_per_sec = pages_copied /
          std::max(1.0, static_cast<double>(elapsed.count()));

      double remaining_pages = total_pages - pages_copied;
      double eta_seconds = remaining_pages / std::max(0.1, pages_per_sec);

      LOG_INFO(CATALOG,
              "Migration: %u/%u pages (%.1f%%), %u/%u indexes, "
              "%.1f pg/s, ETA: %d seconds",
              pages_copied, total_pages,
              (pages_copied * 100.0) / std::max(1u, total_pages),
              indexes_updated, total_indexes,
              pages_per_sec,
              static_cast<int>(eta_seconds));
  }
  ```

- [ ] Add periodic progress callback invocation:
  ```cpp
  // Check progress every 100 pages
  if (progress.pages_copied % 100 == 0)
  {
      if (progress_callback &&
          !progress_callback(progress.pages_copied, progress.total_pages))
      {
          LOG_WARNING(CATALOG, "Migration cancelled");
          rollbackTableMigration(allocated_pages, target_tablespace_id);
          return Status::CANCELLED;
      }
  }
  ```

#### Task 3.7: Implement Cancellation (90 minutes)

- [ ] Add signal handling:
  ```cpp
  // Global cancellation flag
  static std::atomic<bool> g_migration_cancelled{false};

  void setupMigrationCancellation()
  {
      // Install signal handler for SIGINT (Ctrl+C)
      std::signal(SIGINT, [](int) {
          g_migration_cancelled = true;
          LOG_WARNING(CATALOG, "Migration cancellation requested");
      });
  }

  void teardownMigrationCancellation()
  {
      std::signal(SIGINT, SIG_DFL);
      g_migration_cancelled = false;
  }
  ```

- [ ] Check cancellation in loops:
  ```cpp
  // In page copy loop:
  if (g_migration_cancelled)
  {
      LOG_WARNING(CATALOG, "Migration cancelled by user");
      rollbackTableMigration(allocated_pages, target_tablespace_id);
      teardownMigrationCancellation();
      return Status::CANCELLED;
  }
  ```

#### Task 3.8: Build and Test (60 minutes)

- [ ] Build:
  ```bash
  make clean && make
  ```

- [ ] Test with indexed table:
  ```sql
  CREATE TABLE t1 (id INT, name TEXT);
  CREATE INDEX idx_id ON t1(id);
  INSERT INTO t1 SELECT i, 'name' || i FROM generate_series(1, 1000) i;

  ALTER TABLE t1 SET TABLESPACE ts2;

  -- Verify index works
  SELECT * FROM t1 WHERE id = 500;  -- Should use index
  ```

### Session 3 Deliverables Checklist

- [ ] Index update dispatcher implemented
- [ ] At least B-Tree and Hash index updates working
- [ ] Other index types either implemented or stubbed
- [ ] Progress tracking enhanced with ETA
- [ ] Cancellation support (signal handling)
- [ ] Project compiles
- [ ] Indexed table migration works

### Session 3 Completion Criteria

- [ ] Can migrate table with indexes
- [ ] Indexes work after migration
- [ ] Progress logs show ETA
- [ ] Can cancel migration with Ctrl+C
- [ ] Git commit:
  ```bash
  git commit -m "Implement Phase 4 Task 4.1.3/4.1.5: Index updates and progress

  - Implemented index TID update framework
  - Added type-specific index handlers (B-Tree, Hash, etc.)
  - Enhanced progress tracking with ETA calculation
  - Added cancellation support (SIGINT handling)
  - Graceful rollback on cancellation
  - All index types supported

  Session 3 of 4 complete

  🤖 Generated with Claude Code"
  ```

---

## Session 4: Executor Integration and Testing

**Goal**: Wire everything together and validate end-to-end
**Estimated Time**: 3-5 hours
**Dependencies**: Sessions 1-3 complete
**Status**: ⏸️ NOT STARTED

### Pre-Session Checklist
- [ ] Sessions 1-3 complete and committed
- [ ] Clean build verified
- [ ] Basic manual testing successful

### Task Breakdown

#### Task 4.1: Locate Executor Files (15 minutes)
- [ ] Find executor implementation:
  ```bash
  find . -name "*executor*" -type f | grep -E "\.(cpp|h)$"
  ```
- [ ] Study existing opcode handlers (e.g., CREATE TABLE, INSERT)

#### Task 4.2: Implement Executor Handler (2-3 hours)
**File**: `sql/executor/executor.cpp` (or equivalent)

- [ ] Find opcode dispatch switch statement
- [ ] Add case for `OP_ALTER_TABLE_SET_TABLESPACE`:
  ```cpp
  case Opcode::OP_ALTER_TABLE_SET_TABLESPACE:
  {
      Status status = ExecuteAlterTableSetTablespace(bytecode, ctx);
      if (status != Status::OK)
      {
          LOG_ERROR(EXECUTOR, "ALTER TABLE SET TABLESPACE failed");
          return status;
      }
      break;
  }
  ```

- [ ] Implement handler method:
  ```cpp
  Status Executor::ExecuteAlterTableSetTablespace(
      const uint8_t *bytecode,
      ErrorContext *ctx)
  {
      LOG_INFO(EXECUTOR, "Executing ALTER TABLE SET TABLESPACE");

      // Decode bytecode (skip opcode byte)
      size_t offset = 1;

      // Read table name (4 bytes length + string)
      uint32_t table_name_len = readInt32(bytecode + offset);
      offset += 4;
      std::string table_name(
          reinterpret_cast<const char *>(bytecode + offset),
          table_name_len);
      offset += table_name_len;

      // Read tablespace name (4 bytes length + string)
      uint32_t tablespace_name_len = readInt32(bytecode + offset);
      offset += 4;
      std::string tablespace_name(
          reinterpret_cast<const char *>(bytecode + offset),
          tablespace_name_len);
      offset += tablespace_name_len;

      // Read online flag (1 byte)
      bool online = (bytecode[offset] != 0);
      offset++;

      LOG_INFO(EXECUTOR,
              "ALTER TABLE %s SET TABLESPACE %s %s",
              table_name.c_str(),
              tablespace_name.c_str(),
              online ? "ONLINE" : "");

      // Resolve table ID
      uint32_t table_id = catalog_mgr_->resolveTableId(table_name, ctx);
      if (table_id == INVALID_TABLE_ID)
      {
          SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                           ("Table not found: " + table_name).c_str());
          return Status::NOT_FOUND;
      }

      // Resolve tablespace ID
      uint16_t tablespace_id = catalog_mgr_->resolveTablespaceId(
          tablespace_name, ctx);
      if (tablespace_id == INVALID_TABLESPACE_ID)
      {
          SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                           ("Tablespace not found: " + tablespace_name).c_str());
          return Status::NOT_FOUND;
      }

      // Execute migration
      return catalog_mgr_->moveTableToTablespace(
          table_id, tablespace_id, online, nullptr, ctx);
  }
  ```

- [ ] Add helper method `readInt32()` if not present:
  ```cpp
  uint32_t readInt32(const uint8_t *data)
  {
      uint32_t value;
      std::memcpy(&value, data, sizeof(uint32_t));
      return value;  // Assumes little-endian
  }
  ```

#### Task 4.3: Add Error Messages (30 minutes)
- [ ] Update error context strings for user-friendly messages:
  ```cpp
  // Example error messages:
  // "Table 'employees' not found"
  // "Tablespace 'fast_storage' does not exist"
  // "Cannot move table 'employees': already in tablespace 'fast_storage'"
  // "Migration failed: disk full in target tablespace"
  // "ONLINE migration not supported in this version (Phase 5 feature)"
  ```

#### Task 4.4: Build and Basic Testing (60 minutes)

- [ ] Full clean build:
  ```bash
  make clean && make
  ```

- [ ] Create end-to-end test script:
  ```bash
  cat > test_migration.sql <<'EOF'
  -- Create tablespaces
  CREATE TABLESPACE ts1 LOCATION '/tmp/ts1.sbts'
      AUTOEXTEND ON AUTOEXTEND_SIZE 10 MAXSIZE 100;
  CREATE TABLESPACE ts2 LOCATION '/tmp/ts2.sbts'
      AUTOEXTEND ON AUTOEXTEND_SIZE 10 MAXSIZE 100;

  -- Create table in ts1
  CREATE TABLE test_table (
      id INT,
      name TEXT
  ) TABLESPACE ts1;

  -- Insert data
  INSERT INTO test_table VALUES (1, 'Alice');
  INSERT INTO test_table VALUES (2, 'Bob');
  INSERT INTO test_table VALUES (3, 'Charlie');

  -- Create index
  CREATE INDEX idx_id ON test_table(id);

  -- Verify data before migration
  SELECT COUNT(*) FROM test_table;  -- Should be 3

  -- Migrate table
  ALTER TABLE test_table SET TABLESPACE ts2;

  -- Verify data after migration
  SELECT COUNT(*) FROM test_table;  -- Should still be 3
  SELECT * FROM test_table WHERE id = 2;  -- Should return Bob

  -- Verify ONLINE rejection
  ALTER TABLE test_table SET TABLESPACE ts1 ONLINE;  -- Should fail

  -- Cleanup
  DROP TABLE test_table;
  DROP TABLESPACE ts1;
  DROP TABLESPACE ts2;
  EOF
  ```

- [ ] Run test:
  ```bash
  ./scratchbird_cli < test_migration.sql
  ```

#### Task 4.5: Integration Testing (90 minutes)

**Test 1: Large table migration**
- [ ] Create table with 10,000 rows
- [ ] Migrate and verify all rows accessible
- [ ] Check performance (should complete in < 30 seconds)

**Test 2: Multiple indexes**
- [ ] Create table with 3-5 indexes (different types)
- [ ] Migrate and verify all indexes work

**Test 3: Error handling**
- [ ] Test invalid table name
- [ ] Test invalid tablespace name
- [ ] Test migration to same tablespace (should be no-op)
- [ ] Test ONLINE clause (should reject)

**Test 4: Cancellation** (if implemented)
- [ ] Start migration of large table
- [ ] Cancel with Ctrl+C
- [ ] Verify table still in original tablespace
- [ ] Verify no orphaned pages

#### Task 4.6: Documentation Updates (60 minutes)

- [ ] Update `TABLESPACE_IMPLEMENTATION_PLAN.md`:
  - Mark Task 4.1 complete
  - Update status, actual times
  - Document any deviations from plan

- [ ] Create user documentation:
  **File**: `docs/features/ALTER_TABLE_SET_TABLESPACE.md`
  ```markdown
  # ALTER TABLE SET TABLESPACE

  ## Syntax
  \`\`\`sql
  ALTER TABLE table_name SET TABLESPACE tablespace_name;
  \`\`\`

  ## Description
  Moves a table and all its indexes to a different tablespace.

  ## Notes
  - **Offline operation**: Table is locked during migration
  - **Downtime**: Table unavailable for duration of migration
  - **Atomic**: Either completes fully or rolls back
  - **ONLINE clause**: Not supported in this version (Phase 5)

  ## Examples
  \`\`\`sql
  -- Move table to SSD storage
  ALTER TABLE large_table SET TABLESPACE ssd_tablespace;

  -- Move back to default
  ALTER TABLE large_table SET TABLESPACE default_tablespace;
  \`\`\`

  ## Performance
  Migration time depends on table size:
  - Small (< 1000 rows): < 1 second
  - Medium (< 100K rows): < 30 seconds
  - Large (< 1M rows): 1-5 minutes
  - Very large (> 1M rows): May take hours

  ## Limitations
  - Cannot cancel migration once started (Phase 4)
  - ONLINE migration not available (Phase 5)
  - Entire table locked during migration
  \`\`\`

### Session 4 Deliverables Checklist

- [ ] Executor handler implemented
- [ ] End-to-end testing successful
- [ ] Error handling comprehensive
- [ ] Documentation updated
- [ ] All acceptance criteria met

### Session 4 Completion Criteria

- [ ] `ALTER TABLE ... SET TABLESPACE` works end-to-end
- [ ] All test cases pass
- [ ] ONLINE clause properly rejected
- [ ] Documentation complete
- [ ] Git commit:
  ```bash
  git commit -m "Implement Phase 4 Task 4.1.6: Executor integration and testing

  - Added executor handler for ALTER TABLE SET TABLESPACE
  - Implemented bytecode decoding
  - Integrated with catalog manager
  - Comprehensive error handling
  - ONLINE clause validation (reject in Phase 4)
  - End-to-end testing complete
  - User documentation added

  Phase 4 Task 4.1 COMPLETE
  All 4 sessions complete
  Total: 20-28 hours

  🤖 Generated with Claude Code"
  ```

- [ ] Final push:
  ```bash
  git push origin feature/offline-table-migration
  ```

---

## Summary and Next Steps

### What We've Built

After completing all 4 sessions, you will have:

✅ **Parser Support**
- `ALTER TABLE ... SET TABLESPACE [ONLINE]` syntax
- AST node: `AlterTableSetTablespaceStmt`
- Bytecode generation

✅ **Core Migration Engine**
- `CatalogManager::moveTableToTablespace()`
- Heap page copying with TID mapping
- Page allocation/deallocation
- Basic rollback

✅ **Index Support**
- TID updates for all 6 index types
- Progress tracking with ETA
- Cancellation support

✅ **Executor Integration**
- End-to-end execution
- Error handling
- Validation
- Testing

### What's Deferred to Phase 5

🔮 **ONLINE Migration** (not in Phase 4):
- Concurrent access during migration
- Shadow table approach
- Incremental copying
- Lock-free reads

### Total Effort

**Estimated**: 20-28 hours
**Actual**: _(to be filled in)_

**Breakdown by Session**:
1. Parser/AST: 3-4 hours
2. Core Engine: 10-12 hours
3. Index/Progress: 5-7 hours
4. Integration: 3-5 hours

---

## Troubleshooting Guide

### Common Issues

**Issue**: Parser conflicts
- **Solution**: Check token precedence, ensure all terminals declared

**Issue**: Bytecode decode errors
- **Solution**: Verify encoding/decoding match (string lengths, byte order)

**Issue**: Page corruption after migration
- **Solution**: Ensure page headers updated with new GPID

**Issue**: Index lookups fail after migration
- **Solution**: Verify TID mapping applied correctly to all index entries

**Issue**: Memory leaks during large migrations
- **Solution**: Process in batches, free intermediate buffers

---

**Document Status**: ✅ COMPLETE - Ready for implementation
**Next Action**: Begin Session 1 when ready
**Estimated Start-to-Finish**: 4 sessions over 1-2 weeks
