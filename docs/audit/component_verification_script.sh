#!/bin/bash
# Component Verification Script
# Checks for existence and basic completeness of all major ScratchBird components
# Generated: 2025-10-24
# Purpose: Comprehensive code audit for planning document verification

BASE_DIR="/home/dcalford/CliWork/ScratchBird"
REPORT_FILE="$BASE_DIR/docs/audit/COMPONENT_VERIFICATION_REPORT.md"

echo "# ScratchBird Component Verification Report" > "$REPORT_FILE"
echo "**Generated**: $(date)" >> "$REPORT_FILE"
echo "**Purpose**: Verify actual implementation status vs planning document claims" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"
echo "---" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"

# Function to check file existence and line count
check_file() {
    local file="$1"
    local component="$2"

    if [ -f "$file" ]; then
        local lines=$(wc -l < "$file")
        echo "✅ **EXISTS**: $component - $file ($lines lines)" >> "$REPORT_FILE"
        return 0
    else
        echo "❌ **MISSING**: $component - $file" >> "$REPORT_FILE"
        return 1
    fi
}

# Function to check for class definition
check_class() {
    local class_name="$1"
    local component="$2"

    if grep -r "class $class_name" "$BASE_DIR/include" "$BASE_DIR/src" > /dev/null 2>&1; then
        local file=$(grep -r "class $class_name" "$BASE_DIR/include" "$BASE_DIR/src" | head -1 | cut -d: -f1)
        echo "✅ **FOUND**: Class $class_name in $file" >> "$REPORT_FILE"
        return 0
    else
        echo "❌ **NOT FOUND**: Class $class_name" >> "$REPORT_FILE"
        return 1
    fi
}

# Function to check for function implementation
check_function() {
    local function_name="$1"
    local component="$2"

    if grep -r "$function_name" "$BASE_DIR/src" > /dev/null 2>&1; then
        local count=$(grep -r "$function_name" "$BASE_DIR/src" | wc -l)
        echo "✅ **FOUND**: Function $function_name ($count occurrences)" >> "$REPORT_FILE"
        return 0
    else
        echo "❌ **NOT FOUND**: Function $function_name" >> "$REPORT_FILE"
        return 1
    fi
}

echo "## 1. Core Storage Engine" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"
check_file "$BASE_DIR/include/scratchbird/core/buffer_pool.h" "BufferPool"
check_file "$BASE_DIR/src/core/buffer_pool.cpp" "BufferPool Implementation"
check_file "$BASE_DIR/include/scratchbird/core/page_manager.h" "PageManager"
check_file "$BASE_DIR/src/core/page_manager.cpp" "PageManager Implementation"
check_file "$BASE_DIR/include/scratchbird/core/heap_page.h" "HeapPage"
check_file "$BASE_DIR/src/core/heap_page.cpp" "HeapPage Implementation"
check_file "$BASE_DIR/include/scratchbird/core/storage_engine.h" "StorageEngine"
check_file "$BASE_DIR/src/core/storage_engine.cpp" "StorageEngine Implementation"
echo "" >> "$REPORT_FILE"

echo "## 2. Transaction System (MGA)" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"
check_file "$BASE_DIR/include/scratchbird/core/transaction_manager.h" "TransactionManager"
check_file "$BASE_DIR/src/core/transaction_manager.cpp" "TransactionManager Implementation"
check_file "$BASE_DIR/include/scratchbird/core/clog.h" "CLOG (Commit Log)"
check_file "$BASE_DIR/src/core/clog.cpp" "CLOG Implementation"
check_file "$BASE_DIR/include/scratchbird/core/proc_array.h" "ProcArray"
check_file "$BASE_DIR/src/core/proc_array.cpp" "ProcArray Implementation"
echo "" >> "$REPORT_FILE"

echo "## 3. Index Implementations" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"
check_file "$BASE_DIR/include/scratchbird/core/btree.h" "B-Tree Index"
check_file "$BASE_DIR/src/core/btree.cpp" "B-Tree Implementation"
check_file "$BASE_DIR/include/scratchbird/core/hash_index.h" "Hash Index"
check_file "$BASE_DIR/src/core/hash_index.cpp" "Hash Implementation"
check_file "$BASE_DIR/include/scratchbird/core/gin_index.h" "GIN Index"
check_file "$BASE_DIR/src/core/gin_index.cpp" "GIN Implementation"
check_file "$BASE_DIR/include/scratchbird/core/hnsw_index.h" "HNSW Index"
check_file "$BASE_DIR/src/core/hnsw_index.cpp" "HNSW Implementation"
check_file "$BASE_DIR/include/scratchbird/core/brin_index.h" "BRIN Index"
check_file "$BASE_DIR/src/core/brin_index.cpp" "BRIN Implementation"
check_file "$BASE_DIR/include/scratchbird/core/bitmap_index.h" "Bitmap Index"
check_file "$BASE_DIR/src/core/bitmap_index.cpp" "Bitmap Implementation"
echo "" >> "$REPORT_FILE"

echo "## 4. TOAST and Compression" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"
check_file "$BASE_DIR/include/scratchbird/core/toast.h" "TOAST"
check_file "$BASE_DIR/src/core/toast.cpp" "TOAST Implementation"
check_file "$BASE_DIR/include/scratchbird/core/compression.h" "Compression"
check_file "$BASE_DIR/src/core/compression.cpp" "Compression Implementation"
echo "" >> "$REPORT_FILE"

echo "## 5. Type System" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"
check_file "$BASE_DIR/include/scratchbird/core/types.h" "Types"
check_file "$BASE_DIR/src/core/types.cpp" "Types Implementation"
check_file "$BASE_DIR/include/scratchbird/core/decimal_arithmetic.h" "Decimal"
check_file "$BASE_DIR/src/core/decimal_arithmetic.cpp" "Decimal Implementation"
check_file "$BASE_DIR/include/scratchbird/core/jsonb.h" "JSONB"
check_file "$BASE_DIR/src/core/jsonb.cpp" "JSONB Implementation"
check_file "$BASE_DIR/include/scratchbird/core/xml.h" "XML"
check_file "$BASE_DIR/src/core/xml.cpp" "XML Implementation"
check_file "$BASE_DIR/include/scratchbird/core/uuidv7.h" "UUIDv7"
check_file "$BASE_DIR/src/core/uuidv7.cpp" "UUIDv7 Implementation"
echo "" >> "$REPORT_FILE"

echo "## 6. Query Processing" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"
check_file "$BASE_DIR/include/scratchbird/core/lexer.h" "Lexer"
check_file "$BASE_DIR/src/core/lexer.cpp" "Lexer Implementation"
check_file "$BASE_DIR/include/scratchbird/core/parser.h" "Parser"
check_file "$BASE_DIR/src/core/parser.cpp" "Parser Implementation"
check_file "$BASE_DIR/include/scratchbird/core/ast.h" "AST"
check_file "$BASE_DIR/src/core/ast.cpp" "AST Implementation"
check_file "$BASE_DIR/include/scratchbird/core/semantic_analyzer.h" "Semantic Analyzer"
check_file "$BASE_DIR/src/core/semantic_analyzer.cpp" "Semantic Analyzer Implementation"
check_file "$BASE_DIR/include/scratchbird/core/bytecode_generator.h" "Bytecode Generator"
check_file "$BASE_DIR/src/core/bytecode_generator.cpp" "Bytecode Generator Implementation"
check_file "$BASE_DIR/include/scratchbird/core/executor.h" "Executor"
check_file "$BASE_DIR/src/core/executor.cpp" "Executor Implementation"
echo "" >> "$REPORT_FILE"

echo "## 7. Catalog Manager" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"
check_file "$BASE_DIR/include/scratchbird/core/catalog_manager.h" "Catalog Manager"
check_file "$BASE_DIR/src/core/catalog_manager.cpp" "Catalog Manager Implementation"
echo "" >> "$REPORT_FILE"

echo "## 8. Tablespace Support" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"
check_file "$BASE_DIR/include/scratchbird/core/gpid.h" "GPID (Global Page ID)"
check_file "$BASE_DIR/include/scratchbird/core/tid.h" "TID (Tuple ID with GPID)"
check_function "extendTablespace" "Tablespace Autoextend"
check_function "allocatePageInTablespace" "Tablespace Allocation"
check_function "moveTableToTablespace" "Table Migration"
echo "" >> "$REPORT_FILE"

echo "## 9. Sprint 0: MGA Bug Fix" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"
check_function "HeapPage::overwriteTuple" "overwriteTuple method"
check_file "$BASE_DIR/tests/unit/test_storage_engine_mga_crosspage.cpp" "MGA Cross-Page Update Test"
echo "" >> "$REPORT_FILE"

echo "## 10. Sprint 2: Index TID Updates" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"
check_function "updateTIDsAfterMigration" "Index TID Update (General)"
check_function "HNSWIndex::updateTIDsAfterMigration" "HNSW TID Update"
check_function "GINIndex::updateTIDsAfterMigration" "GIN TID Update"
check_function "BRINIndex::updateBlockRangesAfterMigration" "BRIN Block Range Update"
echo "" >> "$REPORT_FILE"

echo "## 11. Sprint 4: ONLINE Migration Infrastructure" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"
check_file "$BASE_DIR/include/scratchbird/core/tid_resolver.h" "TIDResolver Header"
check_file "$BASE_DIR/src/core/tid_resolver.cpp" "TIDResolver Implementation"
check_class "TIDResolver" "TIDResolver Class"
check_class "BloomFilter" "BloomFilter Class"
check_class "QueryTIDCache" "QueryTIDCache Class"
check_function "recordMigration" "recordMigration method"
check_function "resolveTablespace" "resolveTablespace method"
echo "" >> "$REPORT_FILE"

echo "## 12. Sprint 5: Migration Worker" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"
check_file "$BASE_DIR/include/scratchbird/core/migration_worker.h" "MigrationWorker Header"
check_file "$BASE_DIR/src/core/migration_worker.cpp" "MigrationWorker Implementation"
check_class "MigrationWorker" "MigrationWorker Class"
check_function "executeMigration" "executeMigration method"
check_function "executeCopyingPhase" "executeCopyingPhase method"
check_function "executeCatchUpPhase" "executeCatchUpPhase method"
check_function "executeSwapPhase" "executeSwapPhase method"
check_function "executeCleanupPhase" "executeCleanupPhase method"
echo "" >> "$REPORT_FILE"

echo "## 13. Phase 6: Attach/Detach Operations" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"
check_function "attachTablespace" "attachTablespace method"
check_function "detachTablespace" "detachTablespace method"
echo "" >> "$REPORT_FILE"

echo "## 14. Critical MGA Methods" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"
check_function "StorageEngine::updateTuple" "UPDATE implementation"
check_function "StorageEngine::deleteTuple" "DELETE implementation"
check_function "StorageEngine::insertTuple" "INSERT implementation"
echo "" >> "$REPORT_FILE"

echo "---" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"
echo "## Summary Statistics" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"

# Count results
TOTAL_EXISTS=$(grep -c "✅ \*\*EXISTS\*\*" "$REPORT_FILE" || echo "0")
TOTAL_FOUND=$(grep -c "✅ \*\*FOUND\*\*" "$REPORT_FILE" || echo "0")
TOTAL_MISSING=$(grep -c "❌ \*\*MISSING\*\*" "$REPORT_FILE" || echo "0")
TOTAL_NOT_FOUND=$(grep -c "❌ \*\*NOT FOUND\*\*" "$REPORT_FILE" || echo "0")

echo "- **Files Exist**: $TOTAL_EXISTS" >> "$REPORT_FILE"
echo "- **Components Found**: $TOTAL_FOUND" >> "$REPORT_FILE"
echo "- **Files Missing**: $TOTAL_MISSING" >> "$REPORT_FILE"
echo "- **Components Not Found**: $TOTAL_NOT_FOUND" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"

TOTAL_SUCCESS=$((TOTAL_EXISTS + TOTAL_FOUND))
TOTAL_FAIL=$((TOTAL_MISSING + TOTAL_NOT_FOUND))

if [ $TOTAL_FAIL -eq 0 ]; then
    echo "**Overall Status**: ✅ **ALL COMPONENTS VERIFIED**" >> "$REPORT_FILE"
else
    echo "**Overall Status**: ⚠️ **$TOTAL_FAIL COMPONENTS MISSING/NOT FOUND**" >> "$REPORT_FILE"
fi

echo ""
echo "Verification complete. Report generated at: $REPORT_FILE"
echo "Total Verified: $TOTAL_SUCCESS"
echo "Total Missing: $TOTAL_FAIL"
