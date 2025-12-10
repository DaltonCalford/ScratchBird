# Parser V1 Deprecation Plan

## Overview

This document outlines the plan to deprecate and eventually remove Parser V1 in favor of the more feature-complete Parser V2.

## Current Status (2024-12)

**Phase 2 COMPLETE**: Parser V2 is now the ONLY parser used for client queries.

Key changes implemented:
- ServerSession exclusively uses QueryCompilerV2 for all query compilation
- V1 parser code path removed from ServerSession
- ParserVersion enum removed from server_session.h
- Executor updated to use V2 for view/MV parsing (was using V1)
- All 1671 tests pass with V2 as the sole parser

**Important Notes:**
- V1 source files (lexer.cpp, parser.cpp, etc.) are still compiled but ONLY used by:
  - V1 BytecodeGenerator (not in client execution path)
  - Optimizer (query_planner.cpp, mv_rewriter.cpp, index_advisor.cpp)
  - Unit tests that directly test V1 components
  - Benchmark comparisons
- Runtime client queries ALWAYS use Parser V2

**Execution Paths:**
- Client queries: ServerSession → QueryCompilerV2 → BytecodeGeneratorV2 → Executor
- V2 is exclusive for all client-facing SQL compilation

## Timeline

### Phase 1: Parallel Operation (Alpha 2)

**Status**: COMPLETE

- Both parsers available and functional
- V1 was the default for backward compatibility
- `SET PARSER VERSION` command was available
- Parity tests documented differences

### Phase 2: V2 Exclusive for Clients (Current)

**Status**: COMPLETE

Changes implemented:
- V2 is now the ONLY parser for client queries (ServerSession)
- `SET PARSER VERSION V1` no longer works (V1 path removed)
- V1 code kept for tests and optimizer compatibility
- Documentation updated to reflect V2-only state

Migration path:
- All client queries automatically use V2
- No migration needed - V2 handles all SQL

### Phase 3: Shared Types Infrastructure (COMPLETE)

**Status**: COMPLETE (December 2024)

**Changes Implemented:**

1. **Created shared_types.h** - Unified enum types across V1 and V2:
   - JoinType (with NATURAL variants for V2 compatibility)
   - WindowFunc, FrameBoundaryType, FrameMode, FrameExclusion
   - SubqueryType, GroupingType, SortOrder, NullsOrder
   - Helper functions: joinTypeToString(), windowFuncToString(), etc.

2. **Updated ast.h (V1)** - Now includes shared_types.h for enum types

3. **Updated ast_v2.h** - Now imports shared types via `using` declarations

4. **Updated plan_node.h** - Explicitly includes shared_types.h with documentation

**Architecture After Phase 3:**
```
shared_types.h
    ├── ast.h (V1) imports via #include
    ├── ast_v2.h (V2) imports via using declarations
    └── plan_node.h imports directly
```

### Phase 3.5: Optimizer V1 Usage (Current State)

**Status**: Deferred to Beta Release

**Current Architecture:**
- Optimizer (query_planner, mv_rewriter, index_advisor) still uses V1 parser internally
- Plan nodes use V1 AST expression types (Expression*, WindowSpec*, etc.)
- This is acceptable because BytecodeGeneratorV2 does NOT use QueryPlanner
- Client execution path is fully V2

**Files Still Using V1 Parser Internally:**
- src/optimizer/query_planner.cpp (lines 116-118, 212)
- src/optimizer/mv_rewriter.cpp (lines 549-551, 573-575)
- src/optimizer/index_advisor.cpp (lines 512-514)

**Why This Is Acceptable:**
1. BytecodeGeneratorV2 has its own optimization logic
2. QueryPlanner is NOT called by the V2 compilation pipeline
3. V1 parser code is isolated to optimizer subsystem
4. No impact on client query execution

### Phase 4: Full Optimizer V2 Migration (Future)

**Target**: 1.0 Release

**Scope Analysis:**

To fully remove V1 parser, the optimizer would need:

1. **plan_node.h migration** - Change from V1 to V2 types:
   - parser::Expression* → parser::v2::ResolvedExpression*
   - parser::AggregateExpr* → parser::v2::ResolvedFunctionCall*
   - parser::WindowSpec* → V2 equivalent (needs creation)
   - parser::OrderByItem → parser::v2::ResolvedOrderByItem*

2. **Optimizer function signatures** - Accept V2 ResolvedAST:
   - QueryPlanner::planQuery() takes ResolvedSelectStmt*
   - MVRewriter::tryRewrite() takes ResolvedSelectStmt*
   - IndexAdvisor::analyzeQuery() takes ResolvedSelectStmt*

3. **Internal parsing migration** - Replace V1 parser usage:
   - View/MV definition parsing → Use QueryCompilerV2
   - Policy expression parsing → Use SemanticAnalyzerV2

**Estimated Effort**: ~80-120 hours

**Dependencies:**
- V2 WindowSpec resolution (not yet implemented)
- V2 aggregate function resolution (partial)
- Integration of optimizer into V2 pipeline (optional)

### Phase 5: Full V1 Code Removal (1.0 Release)

**Target**: 1.0 Stable Release

Changes:
- V1 parser source files removed
- V1-specific tests archived or migrated to V2
- Shared types (JoinType, etc.) moved to common location
- Documentation reflects V2-only state

## Files to Remove in Phase 4

The following V1-specific files will be removed:

```
include/scratchbird/parser/lexer.h          # V1 Lexer
include/scratchbird/parser/parser.h         # V1 Parser
include/scratchbird/parser/ast.h            # V1 AST (partially - shared types remain)
include/scratchbird/parser/symbol_table.h   # V1 Symbol table
src/parser/lexer.cpp                        # V1 Lexer impl
src/parser/parser.cpp                       # V1 Parser impl
src/parser/ast.cpp                          # V1 AST impl
src/parser/symbol_table.cpp                 # V1 Symbol table impl
src/parser/semantic_analyzer.cpp            # V1 Semantic analyzer
src/sblr/bytecode_generator.cpp             # V1 Bytecode generator (replaced by V2)
```

Files to keep (shared infrastructure):
```
include/scratchbird/parser/source_location.h  # Shared source location types
src/parser/parser_common.cpp                  # Shared utilities (if any)
```

## Code Changes Required

### Phase 2 Changes (COMPLETE)

1. ✅ Updated `server_session.cpp`:
   - Removed V1 parser includes (lexer.h, parser.h, ast.h, bytecode_generator.h)
   - Removed V1 code path in executeQuery()
   - Removed parser version sync logic
   - Removed ast_arena_ creation (was V1-specific)

2. ✅ Updated `server_session.h`:
   - Removed ParserVersion enum
   - Removed parser namespace forward declarations
   - Removed ast_arena_ member
   - Removed parser_version_ member
   - Removed setParserVersion/parserVersion methods

3. ✅ Updated `executor.cpp`:
   - Replaced V1 parser includes with V2 (query_compiler_v2.h)
   - Updated MV creation to use QueryCompilerV2 (line ~3809)
   - Updated REFRESH MATERIALIZED VIEW to use V2 (line ~4181)
   - Updated executeViewQuery to use V2 for view parsing (line ~9892)

4. ✅ Build and test verification:
   - All 1671 tests pass
   - No regressions in functionality

### Phase 3 Changes (Future - Optimizer Migration)

1. Create shared types file:
   ```cpp
   // include/scratchbird/parser/shared_types.h
   // Move JoinType, Expression base types, etc. from ast.h
   ```

2. Update optimizer files to use shared types instead of ast.h

3. Update test files to use V2 parser or shared types

### Phase 4 Changes (Future - Full Removal)

1. Delete V1 source files
2. Archive V1-specific tests
3. Update CMakeLists.txt to exclude V1 files explicitly if needed

## Testing Requirements

Before each phase transition:

1. Run full test suite with V2 as default
2. Verify all critical SQL patterns work with V2
3. Run parity tests to confirm known differences are documented
4. Run performance benchmarks to ensure V2 meets requirements

## Rollback Plan

If issues are discovered after a phase transition:

- Phase 2 → Phase 1: Change default back to V1
- Phase 3 → Phase 2: Downgrade warning to info
- Phase 4: No rollback possible (V1 code removed)

Before Phase 4, maintain a git tag for the last V1-available version.

## Feature Gaps to Address Before Phase 4

The following V1 features need V2 equivalents:

1. TRUNCATE TABLE parsing (partially implemented)
2. EXPLAIN statement (partially implemented)
3. Additional SET commands

These gaps should be addressed during Alpha 3 development.

## Communication Plan

- Alpha 2 release notes: Announce V2 availability
- Alpha 3 release notes: Announce V2 as default
- Beta 1 release notes: Announce deprecation warnings
- 1.0 release notes: Announce V1 removal

## Metrics to Track

During parallel operation, track:
- Percentage of queries using V1 vs V2
- V2 adoption rate over time
- Types of SQL causing V2 compilation failures
- Performance differential between parsers

## Owner and Review

- Owner: Core Parser Team
- Review cycle: Each alpha/beta release
- Final decision: Technical Lead approval required for Phase 4
