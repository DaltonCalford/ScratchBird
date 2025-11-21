# VIEWS Implementation Plan

**Date**: November 7, 2025
**Status**: Design Phase
**Priority**: HIGH (Foundation for query abstraction)
**Estimated Effort**: 60-80 hours

---

## Overview

Implement SQL views (virtual tables) with full DDL support, query expansion, and basic updatable view support.

### Goals

1. **CREATE VIEW** - Define new views from SELECT queries
2. **CREATE OR REPLACE VIEW** - Update existing view definitions
3. **DROP VIEW** - Remove views
4. **View Expansion** - Replace view references with underlying SELECT queries
5. **Basic Updatable Views** - Simple INSERT/UPDATE/DELETE through single-table views

### SQL Syntax

```sql
-- CREATE
CREATE VIEW view_name [(column_list)] AS
    SELECT ...
    [WITH CHECK OPTION];

-- CREATE OR REPLACE
CREATE OR REPLACE VIEW view_name [(column_list)] AS
    SELECT ...;

-- DROP
DROP VIEW [IF EXISTS] view_name [CASCADE | RESTRICT];

-- Query views
SELECT * FROM view_name WHERE ...;
```

---

## Architecture Design

### Catalog Schema

**sys_views** table (new):
```cpp
struct ViewInfo {
    ID view_id;              // UUID (16 bytes)
    ID schema_id;            // Schema UUID (16 bytes)
    char name[512];          // View name (UTF-8)
    char definition[8192];   // SELECT query text (stored)
    bool check_option;       // WITH CHECK OPTION flag
    uint64_t created_time;   // Creation timestamp
    uint64_t last_modified_time; // Last modification timestamp

    // Column information (if specified)
    uint16_t column_count;
    char column_names[4096]; // Comma-separated column names (optional)
};
```

### View Storage Approach

**Option 1: Store Query Text** (Chosen for simplicity)
- Store the SELECT statement as text in catalog
- Parse and re-execute on each view access
- Pros: Simple, flexible, handles schema changes
- Cons: Slower (requires re-parsing)

**Option 2: Store Parsed AST** (Future optimization)
- Store serialized AST in catalog
- Pros: Faster execution
- Cons: More complex, harder to debug

### View Expansion Strategy

When a query references a view:
1. **Parse** the main query
2. **Identify** view references in FROM clause
3. **Look up** view definition from catalog
4. **Parse** view's SELECT statement
5. **Replace** view reference with subquery (wrapped in parentheses)
6. **Continue** with normal query planning

Example:
```sql
-- View definition
CREATE VIEW active_users AS
    SELECT id, name, email FROM users WHERE active = true;

-- Query using view
SELECT name FROM active_users WHERE id > 100;

-- Expanded query (internal)
SELECT name FROM (
    SELECT id, name, email FROM users WHERE active = true
) AS active_users WHERE id > 100;
```

### Dependency Tracking

**Challenge**: Dropping a table used by a view should either:
- Fail with error (RESTRICT)
- Drop the view too (CASCADE)

**Solution**: Track dependencies in catalog
```cpp
struct ViewDependency {
    ID view_id;
    ID table_id;  // Referenced table
    DependencyType type;  // TABLE, VIEW, FUNCTION
};
```

**For ALPHA Phase 1**: Simple dependency tracking
- Track table dependencies when creating view
- Check dependencies when dropping table (RESTRICT mode)
- Implement CASCADE later

---

## Implementation Steps

### Step 1: Tokens & Keywords (30 minutes)

**File**: `include/scratchbird/parser/token.h`
```cpp
KW_VIEW,          // VIEW
KW_REPLACE,       // REPLACE (for CREATE OR REPLACE)
KW_CHECK,         // CHECK (for WITH CHECK OPTION)
KW_OPTION,        // OPTION
```

**File**: `src/parser/lexer.cpp`
```cpp
{"VIEW", TokenType::KW_VIEW},
{"REPLACE", TokenType::KW_REPLACE},
// CHECK and OPTION may already exist
```

### Step 2: AST Nodes (2-3 hours)

**File**: `include/scratchbird/parser/ast.h`

```cpp
// AST kinds
CREATE_VIEW,
DROP_VIEW,

// CreateViewStmt
class CreateViewStmt : public Statement {
public:
    CreateViewStmt(const SourceSpan& span, StringPool::StringId name,
                   SelectStmt* query, bool or_replace = false)
        : Statement(ASTKind::CREATE_VIEW, span),
          name_(name), query_(query), or_replace_(or_replace),
          check_option_(false) {}

    StringPool::StringId name() const { return name_; }
    SelectStmt* query() const { return query_; }
    bool orReplace() const { return or_replace_; }
    bool checkOption() const { return check_option_; }

    const std::vector<StringPool::StringId>& columnNames() const {
        return column_names_;
    }

    void setCheckOption(bool check) { check_option_ = check; }
    void setColumnNames(std::vector<StringPool::StringId> names) {
        column_names_ = std::move(names);
    }

    void accept(ASTVisitor* visitor) override;

private:
    StringPool::StringId name_;
    SelectStmt* query_;
    bool or_replace_;
    bool check_option_;
    std::vector<StringPool::StringId> column_names_;  // Optional column list
};

// DropViewStmt
class DropViewStmt : public Statement {
public:
    DropViewStmt(const SourceSpan& span, StringPool::StringId name,
                 bool if_exists, bool cascade)
        : Statement(ASTKind::DROP_VIEW, span),
          name_(name), if_exists_(if_exists), cascade_(cascade) {}

    StringPool::StringId name() const { return name_; }
    bool ifExists() const { return if_exists_; }
    bool cascade() const { return cascade_; }

    void accept(ASTVisitor* visitor) override;

private:
    StringPool::StringId name_;
    bool if_exists_;
    bool cascade_;
};

// Add to ASTVisitor
virtual void visit(CreateViewStmt* node) = 0;
virtual void visit(DropViewStmt* node) = 0;
```

### Step 3: Parser (6-8 hours)

**File**: `include/scratchbird/parser/parser.h`
```cpp
Statement* parseCreateView();
Statement* parseDropView();
```

**File**: `src/parser/parser.cpp`

Implement parsers:
```cpp
Statement* Parser::parseCreateView()
{
    auto start_loc = previous().location;
    bool or_replace = false;

    // CREATE
    // [OR REPLACE]
    if (match(TokenType::KW_OR)) {
        advance();
        expect(TokenType::KW_REPLACE, "Expected REPLACE after OR");
        or_replace = true;
    }

    // VIEW
    expect(TokenType::KW_VIEW, "Expected VIEW");

    // view_name
    if (!check(TokenType::IDENTIFIER)) {
        error("Expected view name");
        return nullptr;
    }
    auto view_name = current().value.string_id;
    advance();

    // Optional column list: (col1, col2, ...)
    std::vector<StringPool::StringId> column_names;
    if (match(TokenType::LEFT_PAREN)) {
        advance();
        do {
            if (!check(TokenType::IDENTIFIER)) {
                error("Expected column name");
                return nullptr;
            }
            column_names.push_back(current().value.string_id);
            advance();
        } while (match(TokenType::COMMA) && (advance(), true));

        expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");
    }

    // AS
    expect(TokenType::KW_AS, "Expected AS");

    // SELECT statement
    auto* query = parseSelect();
    if (!query) {
        return nullptr;
    }

    auto* stmt = arena_.make<CreateViewStmt>(makeSpan(start_loc), view_name,
                                              static_cast<SelectStmt*>(query),
                                              or_replace);

    if (!column_names.empty()) {
        stmt->setColumnNames(std::move(column_names));
    }

    // Optional WITH CHECK OPTION
    if (match(TokenType::KW_WITH)) {
        advance();
        expect(TokenType::KW_CHECK, "Expected CHECK");
        expect(TokenType::KW_OPTION, "Expected OPTION");
        stmt->setCheckOption(true);
    }

    match(TokenType::SEMICOLON);
    return stmt;
}

Statement* Parser::parseDropView()
{
    auto start_loc = previous().location;

    // DROP VIEW
    expect(TokenType::KW_VIEW, "Expected VIEW");

    // [IF EXISTS]
    bool if_exists = false;
    if (match(TokenType::KW_IF)) {
        advance();
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        if_exists = true;
    }

    // view_name
    if (!check(TokenType::IDENTIFIER)) {
        error("Expected view name");
        return nullptr;
    }
    auto view_name = current().value.string_id;
    advance();

    // [CASCADE | RESTRICT]
    bool cascade = false;
    if (match(TokenType::KW_CASCADE)) {
        cascade = true;
        advance();
    } else if (match(TokenType::KW_RESTRICT)) {
        advance();
    }

    auto* stmt = arena_.make<DropViewStmt>(makeSpan(start_loc), view_name,
                                            if_exists, cascade);
    match(TokenType::SEMICOLON);
    return stmt;
}
```

Add switch cases in `parseStatement()`:
```cpp
// In CREATE handler
else if (peek().type == TokenType::KW_VIEW ||
         (peek().type == TokenType::KW_OR && peekAhead(2).type == TokenType::KW_VIEW)) {
    stmt = parseCreateView();
}

// In DROP handler
else if (peek().type == TokenType::KW_VIEW) {
    stmt = parseDropView();
}
```

### Step 4: Bytecode Opcodes (1-2 hours)

**File**: `include/scratchbird/sblr/opcodes.h`
```cpp
CREATE_VIEW = 0x2F,
DROP_VIEW = 0x30,
```

**File**: `src/sblr/bytecode_generator.cpp`

```cpp
void BytecodeGenerator::visit(CreateViewStmt* node)
{
    current_result_->writeOpcode(Opcode::CREATE_VIEW);

    // Write view name
    writeStringId(node->name());

    // Write flags: [or_replace, check_option, has_column_names]
    uint8_t flags = 0;
    if (node->orReplace()) flags |= 0x01;
    if (node->checkOption()) flags |= 0x02;
    if (!node->columnNames().empty()) flags |= 0x04;
    current_result_->writeByte(flags);

    // Write column names if present
    if (!node->columnNames().empty()) {
        current_result_->writeByte(static_cast<uint8_t>(node->columnNames().size()));
        for (auto col_name : node->columnNames()) {
            writeStringId(col_name);
        }
    }

    // Write SELECT query (as nested statement)
    // Store query text instead of bytecode for simplicity
    std::string query_text = "<query_text>";  // TODO: serialize SELECT to string
    current_result_->writeString(query_text);
}

void BytecodeGenerator::visit(DropViewStmt* node)
{
    current_result_->writeOpcode(Opcode::DROP_VIEW);

    writeStringId(node->name());

    uint8_t flags = 0;
    if (node->ifExists()) flags |= 0x01;
    if (node->cascade()) flags |= 0x02;
    current_result_->writeByte(flags);
}
```

### Step 5: Catalog Manager (12-16 hours)

**File**: `include/scratchbird/core/catalog_manager.h`

```cpp
// View information structure
struct ViewInfo {
    ID view_id;
    ID schema_id;
    std::string name;
    std::string definition;  // SELECT query text
    bool check_option;
    std::vector<std::string> column_names;  // Optional explicit columns
    uint64_t created_time;
    uint64_t last_modified_time;
};

// Private members
std::unordered_map<ID, ViewInfo> view_cache_;
std::unordered_map<std::string, ID> view_name_to_id_;
std::mutex view_cache_mutex_;

// Public methods
auto createView(const ID& schema_id, const std::string& name,
                const std::string& definition, bool or_replace,
                bool check_option, const std::vector<std::string>& column_names,
                ErrorContext* ctx = nullptr) -> Status;

auto dropView(const ID& view_id, bool cascade,
              ErrorContext* ctx = nullptr) -> Status;

auto getView(const ID& schema_id, const std::string& name,
             ViewInfo& info_out, ErrorContext* ctx = nullptr) -> Status;

auto getViewIdByName(const std::string& name, ID& id_out,
                     ErrorContext* ctx = nullptr) -> Status;

auto isView(const std::string& name, ErrorContext* ctx = nullptr) -> bool;
```

**File**: `src/core/catalog_manager.cpp`

Implement methods:
```cpp
auto CatalogManager::createView(const ID& schema_id, const std::string& name,
                                  const std::string& definition, bool or_replace,
                                  bool check_option,
                                  const std::vector<std::string>& column_names,
                                  ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(view_cache_mutex_);

    // Check if view exists
    auto it = view_name_to_id_.find(name);
    if (it != view_name_to_id_.end()) {
        if (!or_replace) {
            SET_ERROR_CONTEXT(ctx, Status::ALREADY_EXISTS,
                              "View already exists: " + name);
            return Status::ALREADY_EXISTS;
        }
        // Update existing view
        ViewInfo& view = view_cache_[it->second];
        view.definition = definition;
        view.check_option = check_option;
        view.column_names = column_names;
        view.last_modified_time = std::time(nullptr);
        return Status::OK;
    }

    // Create new view
    ViewInfo view;
    view.view_id = generateUuidV7();
    view.schema_id = schema_id;
    view.name = name;
    view.definition = definition;
    view.check_option = check_option;
    view.column_names = column_names;
    view.created_time = std::time(nullptr);
    view.last_modified_time = view.created_time;

    view_cache_[view.view_id] = view;
    view_name_to_id_[name] = view.view_id;

    LOG_INFO(CATALOG, "Created view '%s'", name.c_str());
    return Status::OK;
}

auto CatalogManager::dropView(const ID& view_id, bool cascade,
                                ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(view_cache_mutex_);

    auto it = view_cache_.find(view_id);
    if (it == view_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "View not found");
        return Status::NOT_FOUND;
    }

    std::string view_name = it->second.name;

    // TODO: Check for dependent views if CASCADE is false

    view_cache_.erase(it);
    view_name_to_id_.erase(view_name);

    LOG_INFO(CATALOG, "Dropped view '%s'", view_name.c_str());
    return Status::OK;
}

auto CatalogManager::getView(const ID& schema_id, const std::string& name,
                               ViewInfo& info_out, ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(view_cache_mutex_);

    auto it = view_name_to_id_.find(name);
    if (it == view_name_to_id_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "View not found: " + name);
        return Status::NOT_FOUND;
    }

    info_out = view_cache_[it->second];
    return Status::OK;
}

auto CatalogManager::getViewIdByName(const std::string& name, ID& id_out,
                                       ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(view_cache_mutex_);

    auto it = view_name_to_id_.find(name);
    if (it == view_name_to_id_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "View not found: " + name);
        return Status::NOT_FOUND;
    }

    id_out = it->second;
    return Status::OK;
}

auto CatalogManager::isView(const std::string& name, ErrorContext* ctx) -> bool
{
    std::lock_guard<std::mutex> lock(view_cache_mutex_);
    return view_name_to_id_.find(name) != view_name_to_id_.end();
}
```

### Step 6: Executor (8-12 hours)

**File**: `include/scratchbird/sblr/executor.h`
```cpp
void executeCreateView();
void executeDropView();
```

**File**: `src/sblr/executor.cpp`

```cpp
void Executor::executeCreateView()
{
    // Read view name
    std::string view_name = readString();

    // Read flags
    uint8_t flags = bytecode_[pc_++];
    bool or_replace = flags & 0x01;
    bool check_option = flags & 0x02;
    bool has_column_names = flags & 0x04;

    // Read column names if present
    std::vector<std::string> column_names;
    if (has_column_names) {
        uint8_t count = bytecode_[pc_++];
        for (uint8_t i = 0; i < count; i++) {
            column_names.push_back(readString());
        }
    }

    // Read query definition
    std::string definition = readString();

    // Get default schema
    ErrorContext ctx;
    core::CatalogManager::SchemaInfo schema_info;
    auto status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, &ctx);
    if (status != Status::OK) {
        throw std::runtime_error("Schema not found: PUBLIC");
    }

    // Create view
    status = db_->catalog_manager()->createView(schema_info.schema_id, view_name,
                                                  definition, or_replace, check_option,
                                                  column_names, &ctx);
    if (status != Status::OK) {
        throw std::runtime_error("CREATE VIEW failed: " + view_name);
    }

    std::cout << "CREATE VIEW" << std::endl;
}

void Executor::executeDropView()
{
    // Read view name
    std::string view_name = readString();

    // Read flags
    uint8_t flags = bytecode_[pc_++];
    bool if_exists = flags & 0x01;
    bool cascade = flags & 0x02;

    // Look up view ID
    ID view_id;
    ErrorContext ctx;
    auto status = db_->catalog_manager()->getViewIdByName(view_name, view_id, &ctx);

    if (status == Status::NOT_FOUND) {
        if (if_exists) {
            std::cout << "NOTICE: view \"" << view_name << "\" does not exist, skipping" << std::endl;
            return;
        }
        throw std::runtime_error("View not found: " + view_name);
    }

    // Drop view
    status = db_->catalog_manager()->dropView(view_id, cascade, &ctx);
    if (status != Status::OK) {
        throw std::runtime_error("DROP VIEW failed: " + view_name);
    }

    std::cout << "DROP VIEW" << std::endl;
}
```

### Step 7: View Expansion in Query Planner (20-30 hours)

**Challenge**: When SELECT references a view, replace it with the view's definition

**File**: `src/optimizer/query_planner.cpp`

```cpp
// In FROM clause processing
if (isTableReference(name)) {
    // Check if it's a view
    if (catalog_manager->isView(name)) {
        // Get view definition
        ViewInfo view_info;
        catalog_manager->getView(schema_id, name, view_info, &ctx);

        // Parse view definition
        Parser parser(view_info.definition, string_pool);
        auto* view_query = parser.parseStatement();

        // Replace table reference with subquery
        // ... (complex AST manipulation)
    } else {
        // Regular table access
        // ...
    }
}
```

**Simplified Approach for ALPHA**:
- Store view definition as text
- When executing SELECT, check if table is a view
- If yes, parse view definition and execute as subquery
- Wrap result in temporary result set

### Step 8: Testing (8-12 hours)

**File**: `test_views.sql`

```sql
-- Create test table
CREATE TABLE users (
    id INTEGER,
    name VARCHAR(100),
    email VARCHAR(100),
    active BOOLEAN
);

INSERT INTO users VALUES (1, 'Alice', 'alice@example.com', true);
INSERT INTO users VALUES (2, 'Bob', 'bob@example.com', false);
INSERT INTO users VALUES (3, 'Charlie', 'charlie@example.com', true);

-- Test CREATE VIEW
CREATE VIEW active_users AS
    SELECT id, name, email FROM users WHERE active = true;

-- Test querying view
SELECT * FROM active_users;

-- Test CREATE OR REPLACE
CREATE OR REPLACE VIEW active_users AS
    SELECT id, name FROM users WHERE active = true;

-- Test view with column names
CREATE VIEW user_summary (user_id, user_name) AS
    SELECT id, name FROM users;

-- Test DROP VIEW
DROP VIEW user_summary;
DROP VIEW IF EXISTS user_summary;

-- Test CASCADE
-- (requires dependent views or constraints)

-- Clean up
DROP VIEW active_users;
DROP TABLE users;
```

---

## Simplified Scope for ALPHA Phase 1

To reduce complexity, implement **basic views only**:

### ✅ **In Scope**:
- CREATE VIEW (basic)
- CREATE OR REPLACE VIEW
- DROP VIEW (IF EXISTS, CASCADE stub)
- Query simple views (SELECT * FROM view)
- View name resolution
- Basic dependency tracking

### ❌ **Out of Scope** (defer to later):
- Updatable views (INSERT/UPDATE/DELETE through views)
- Materialized views
- WITH CHECK OPTION enforcement
- Complex view expansion (joins, subqueries in view definitions)
- View dependencies CASCADE (just stub)
- View column renaming
- Recursive views

### Simplified Implementation Time: **30-40 hours**

---

## Success Criteria

1. ✅ Can create views with SELECT definitions
2. ✅ Can replace existing views with CREATE OR REPLACE
3. ✅ Can drop views with IF EXISTS
4. ✅ Can query views like tables (basic SELECT)
5. ✅ Views stored in catalog and persist
6. ✅ Build compiles with no errors
7. ✅ Test file demonstrates all functionality

---

**Author**: Claude Code Assistant
**Date**: November 7, 2025
**Status**: Ready for implementation
