# Query Planner Integration Design

**Phase**: Phase 1, Task 1.3 (Final Integration)
**Component**: Query Planner Integration with Parser and Bytecode Generator
**Date**: October 25, 2025
**Status**: In Development

---

## Overview

This document describes how to integrate the QueryPlanner into the existing query execution pipeline. The integration inserts query optimization between parsing and bytecode generation.

---

## Current Architecture

```
┌──────────┐     ┌────────────────────┐     ┌──────────┐
│  Parser  │────>│ BytecodeGenerator  │────>│ Executor │
└──────────┘     └────────────────────┘     └──────────┘
     │                    │
     v                    v
   AST              SBLR Bytecode
(SelectStmt)
```

**Problems**:
- No query optimization
- Always uses sequential scan
- Ignores available indexes
- Poor performance on large tables

---

## Target Architecture

```
┌──────────┐     ┌──────────────┐     ┌────────────────────┐     ┌──────────┐
│  Parser  │────>│ QueryPlanner │────>│ BytecodeGenerator  │────>│ Executor │
└──────────┘     └──────────────┘     └────────────────────┘     └──────────┘
     │                  │                       │
     v                  v                       v
   AST             PlanNode               SBLR Bytecode
(SelectStmt)    (SeqScan/IndexScan)
```

**Benefits**:
- Cost-based optimization
- Chooses best access method (SeqScan vs IndexScan)
- Uses statistics for accurate estimates
- Scales to large tables

---

## Integration Points

### 1. Database Class Enhancement

Add optimizer components to Database:

```cpp
class Database {
public:
    // Existing accessors
    CatalogManager *catalog_manager();
    StorageEngine *storage_engine();

    // NEW: Optimizer accessors
    optimizer::StatisticsManager *statistics_manager();
    optimizer::QueryPlanner *query_planner();

private:
    // Existing components
    std::unique_ptr<CatalogManager> catalog_manager_;
    std::unique_ptr<StorageEngine> storage_engine_;

    // NEW: Optimizer components
    std::unique_ptr<optimizer::StatisticsManager> statistics_manager_;
    std::unique_ptr<optimizer::QueryPlanner> query_planner_;
    std::unique_ptr<optimizer::CostModel> cost_model_;
};
```

### 2. BytecodeGenerator Enhancement

Modify BytecodeGenerator to accept optional PlanNode:

```cpp
class BytecodeGenerator : public parser::ASTVisitor {
public:
    // Existing method - direct AST to bytecode
    BytecodeResult generate(parser::Statement *stmt);

    // NEW: Generate bytecode from PlanNode
    BytecodeResult generateFromPlan(std::shared_ptr<optimizer::PlanNode> plan,
                                     parser::SelectStmt *original_stmt);

private:
    // NEW: Generate bytecode for different plan node types
    void generateSeqScan(optimizer::SeqScanNode *node,
                        parser::SelectStmt *stmt);
    void generateIndexScan(optimizer::IndexScanNode *node,
                          parser::SelectStmt *stmt);
};
```

### 3. Query Execution Flow

Update BytecodeGenerator::visit(SelectStmt) to use planner:

```cpp
void BytecodeGenerator::visit(parser::SelectStmt *node) {
    // NEW: Try to use query planner if available
    if (database_ && database_->query_planner()) {
        auto plan = database_->query_planner()->planQuery(node);
        if (plan) {
            generateFromPlan(plan, node);
            return;
        }
        // Fallback to direct bytecode generation if planning fails
    }

    // EXISTING: Direct bytecode generation (no optimization)
    generateDirectSelect(node);
}
```

---

## Implementation Plan

### Task 1.3.6.1: Add Optimizer Components to Database (2-3 hours)

**File**: `include/scratchbird/core/database.h`

```cpp
// Forward declarations
namespace optimizer {
    class StatisticsManager;
    class QueryPlanner;
    class CostModel;
}

class Database {
public:
    // Optimizer accessors
    optimizer::StatisticsManager *statistics_manager() {
        return statistics_manager_.get();
    }

    optimizer::QueryPlanner *query_planner() {
        return query_planner_.get();
    }

private:
    // Optimizer components (initialized in Database::open())
    std::unique_ptr<optimizer::StatisticsManager> statistics_manager_;
    std::unique_ptr<optimizer::QueryPlanner> query_planner_;
    std::unique_ptr<optimizer::CostModel> cost_model_;
};
```

**File**: `src/core/database.cpp`

```cpp
#include "scratchbird/optimizer/statistics_manager.h"
#include "scratchbird/optimizer/query_planner.h"
#include "scratchbird/optimizer/cost_model.h"

Status Database::open(const std::string &path, ErrorContext *ctx) {
    // ... existing open logic ...

    // Initialize optimizer components
    statistics_manager_ = std::make_unique<optimizer::StatisticsManager>(
        this, catalog_manager_.get());

    cost_model_ = std::make_unique<optimizer::CostModel>();

    query_planner_ = std::make_unique<optimizer::QueryPlanner>(
        this, *cost_model_, statistics_manager_.get());

    return Status::OK;
}
```

---

### Task 1.3.6.2: Enhance BytecodeGenerator with PlanNode Support (3-4 hours)

**File**: `include/scratchbird/sblr/bytecode_generator.h`

```cpp
#include "scratchbird/optimizer/plan_node.h"

namespace scratchbird::sblr {

class BytecodeGenerator : public parser::ASTVisitor {
public:
    BytecodeGenerator(const parser::StringPool &string_pool,
                     core::Database *database = nullptr);  // NEW: Optional database

    // Generate bytecode from PlanNode (optimized path)
    BytecodeResult generateFromPlan(
        std::shared_ptr<optimizer::PlanNode> plan,
        parser::SelectStmt *original_stmt);

private:
    core::Database *database_;  // NEW: For accessing query planner

    // Plan node bytecode generation
    void generateSeqScanPlan(optimizer::SeqScanNode *node,
                            parser::SelectStmt *stmt);
    void generateIndexScanPlan(optimizer::IndexScanNode *node,
                              parser::SelectStmt *stmt);

    // Existing direct generation (fallback)
    void generateDirectSelect(parser::SelectStmt *node);
};

}
```

**File**: `src/sblr/bytecode_generator.cpp`

```cpp
void BytecodeGenerator::visit(parser::SelectStmt *node) {
    // Try query planner if available
    if (database_ && database_->query_planner()) {
        core::ErrorContext ctx;
        auto plan = database_->query_planner()->planQuery(node, &ctx);

        if (plan) {
            // Use optimized plan
            generateFromPlan(plan, node);
            return;
        }

        // Planning failed - log warning and fallback
        if (!ctx.message.empty()) {
            DEBUG_LOG_DB("Query planning failed: " + ctx.message);
        }
    }

    // Fallback: Direct bytecode generation (no optimization)
    generateDirectSelect(node);
}

void BytecodeGenerator::generateDirectSelect(parser::SelectStmt *node) {
    // EXISTING IMPLEMENTATION: Direct AST to bytecode
    current_result_->writeOpcode(Opcode::SELECT);
    // ... rest of existing implementation ...
}

BytecodeResult BytecodeGenerator::generateFromPlan(
    std::shared_ptr<optimizer::PlanNode> plan,
    parser::SelectStmt *original_stmt)
{
    BytecodeResult result;
    current_result_ = &result;

    // Dispatch based on plan node type
    switch (plan->type()) {
        case optimizer::PlanNodeType::SEQ_SCAN:
            generateSeqScanPlan(
                static_cast<optimizer::SeqScanNode*>(plan.get()),
                original_stmt);
            break;

        case optimizer::PlanNodeType::INDEX_SCAN:
            generateIndexScanPlan(
                static_cast<optimizer::IndexScanNode*>(plan.get()),
                original_stmt);
            break;

        default:
            result.addError("Unsupported plan node type");
            break;
    }

    return result;
}

void BytecodeGenerator::generateSeqScanPlan(
    optimizer::SeqScanNode *node,
    parser::SelectStmt *stmt)
{
    // Generate SELECT bytecode
    current_result_->writeOpcode(Opcode::SELECT);

    // Write select list (from original stmt)
    current_result_->writeOpcode(Opcode::BEGIN_LIST);
    current_result_->writeInt32(stmt->selectList().size());

    for (const auto &item : stmt->selectList()) {
        if (item.is_star) {
            current_result_->writeOpcode(Opcode::SELECT_STAR);
        } else {
            generateExpression(item.expr);
            if (item.alias != 0) {
                current_result_->writeOpcode(Opcode::COLUMN_REF);
                writeStringId(item.alias);
            }
        }
    }

    current_result_->writeOpcode(Opcode::END_LIST);

    // Write table reference
    current_result_->writeOpcode(Opcode::TABLE_REF);
    writeStringId(stmt->tableName());

    // Write WHERE clause
    if (stmt->whereClause()) {
        current_result_->writeOpcode(Opcode::WHERE_CLAUSE);
        generateExpression(stmt->whereClause());
    }

    // NEW: Write scan hint (for executor optimization)
    current_result_->writeOpcode(Opcode::SCAN_HINT);
    current_result_->writeByte(0);  // 0 = Sequential scan
}

void BytecodeGenerator::generateIndexScanPlan(
    optimizer::IndexScanNode *node,
    parser::SelectStmt *stmt)
{
    // Generate SELECT bytecode
    current_result_->writeOpcode(Opcode::SELECT);

    // Write select list
    current_result_->writeOpcode(Opcode::BEGIN_LIST);
    current_result_->writeInt32(stmt->selectList().size());

    for (const auto &item : stmt->selectList()) {
        if (item.is_star) {
            current_result_->writeOpcode(Opcode::SELECT_STAR);
        } else {
            generateExpression(item.expr);
            if (item.alias != 0) {
                current_result_->writeOpcode(Opcode::COLUMN_REF);
                writeStringId(item.alias);
            }
        }
    }

    current_result_->writeOpcode(Opcode::END_LIST);

    // Write table reference
    current_result_->writeOpcode(Opcode::TABLE_REF);
    writeStringId(stmt->tableName());

    // NEW: Write index reference
    current_result_->writeOpcode(Opcode::INDEX_REF);
    // Write index ID as string (UUID format)
    current_result_->writeString(node->indexId().toString());

    // Write WHERE clause
    if (stmt->whereClause()) {
        current_result_->writeOpcode(Opcode::WHERE_CLAUSE);
        generateExpression(stmt->whereClause());
    }

    // NEW: Write scan hint
    current_result_->writeOpcode(Opcode::SCAN_HINT);
    current_result_->writeByte(1);  // 1 = Index scan
}
```

---

### Task 1.3.6.3: Add New Opcodes (1 hour)

**File**: `include/scratchbird/sblr/opcodes.h`

```cpp
enum class Opcode : uint8_t {
    // ... existing opcodes ...

    // NEW: Query optimization hints
    SCAN_HINT = 0x50,     // Hint about scan method (0=seq, 1=index)
    INDEX_REF = 0x51,     // Reference to index ID
};
```

---

### Task 1.3.6.4: Update Executor to Use Hints (Optional - Phase 2)

For Phase 1, the executor can ignore these hints. The bytecode will execute correctly with sequential scans.

For Phase 2, the executor can use hints to:
- Use index scans when INDEX_REF is present
- Skip unnecessary full table scans
- Optimize join order

---

## Testing Strategy

### Unit Tests

1. **Database Initialization**
   - Verify statistics_manager() returns valid pointer
   - Verify query_planner() returns valid pointer
   - Components initialized after open()

2. **Bytecode Generation**
   - generateFromPlan() creates valid bytecode
   - SeqScan plan generates correct opcodes
   - IndexScan plan generates correct opcodes with INDEX_REF

3. **Plan Integration**
   - BytecodeGenerator uses planner when available
   - Falls back to direct generation if planner unavailable
   - Falls back if planning fails

### Integration Tests

1. **End-to-End Query Flow**
   ```sql
   CREATE TABLE users (id INT, name VARCHAR(50));
   INSERT INTO users VALUES (1, 'Alice'), (2, 'Bob');
   ANALYZE users;
   SELECT * FROM users WHERE id = 1;
   -- Should use query planner and generate optimized bytecode
   ```

2. **Index Selection**
   ```sql
   CREATE INDEX idx_users_id ON users(id);
   ANALYZE users;
   SELECT * FROM users WHERE id = 1;
   -- Should choose IndexScan plan
   ```

3. **Fallback Behavior**
   ```sql
   SELECT * FROM users WHERE id = 1;
   -- Without ANALYZE, should fallback to direct generation
   ```

---

## Design Decisions

### 1. Optional Database Parameter in BytecodeGenerator

**Choice**: Make database parameter optional

**Rationale**:
- Backward compatibility with existing code
- BytecodeGenerator can work without planner (direct mode)
- Graceful degradation if planner unavailable

### 2. Fallback to Direct Generation

**Choice**: Always have fallback path

**Rationale**:
- Query must succeed even if planner fails
- Protects against planner bugs during development
- Allows disabling optimizer for debugging

### 3. Scan Hints as Optional Opcodes

**Choice**: Add SCAN_HINT and INDEX_REF opcodes

**Rationale**:
- Executor can use hints for optimization
- Backward compatible (old executor ignores unknown opcodes)
- Future-proof for advanced optimizations

### 4. Reuse Original SelectStmt for Select List

**Choice**: Use original AST for select list generation

**Rationale**:
- PlanNode only contains scan information
- Select list projection is same regardless of scan method
- Avoids duplicating expression information in PlanNode

---

## Future Enhancements

1. **Executor Index Scan Support** (Phase 2)
   - Executor reads INDEX_REF opcode
   - Uses index for row lookup
   - Faster execution for selective queries

2. **Join Planning** (Phase 2)
   - Add join plan nodes (NestLoop, HashJoin, MergeJoin)
   - Multi-table query optimization
   - Join order selection

3. **EXPLAIN Command** (Task 1.5)
   - Bytecode annotated with plan information
   - User can see optimizer decisions
   - Cost estimates visible in output

---

**Document Version**: 1.0
**Last Updated**: October 25, 2025
**Author**: Claude Code
**Status**: Design Complete - Ready for Implementation
