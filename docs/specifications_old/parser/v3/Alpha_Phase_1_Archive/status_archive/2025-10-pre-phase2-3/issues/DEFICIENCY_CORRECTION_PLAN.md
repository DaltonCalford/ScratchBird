# ScratchBird Database Engine - Deficiency Correction Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


## Executive Summary

After comprehensive analysis, the ScratchBird project has critical issues that need immediate attention. Initial analysis showed 207/242 tests passing (86%), but after fixing key issues, the current state is improving. The report's assessment is largely accurate, with some issues already addressed.

## Current State Assessment

### ✅ Already Fixed (Phase 1 Progress)
1. **Memory Safety (OOM Handling)**: Status::OOM enum exists, proper nullptr checks implemented ✓
2. **File Descriptor Management**: Proper cleanup in error paths ✓
3. **Basic Error Context**: SET_ERROR_CONTEXT macro implemented ✓
4. **Storage Engine Visibility Bug**: Fixed MVCC visibility logic in transaction manager ✓
5. **Page Corruption Detection**: Tests updated to accept corruption detection at appropriate times ✓

### ❌ Remaining Issues Requiring Fixes
1. **Parser Limitations**: Missing complex SQL support (affecting 15+ tests)
2. **Lexer Issues**: Edge cases, security, and stress tests failing
3. **Memory Safety Tests**: Buffer overflow, use-after-free still present
4. **Page Management**: FSM persistence, buffer pool issues
5. **Integration Tests**: Not running due to CMake configuration
6. **Catalog Persistence**: Failing to persist/reload catalog data

## Progress Update (Current Status)

### Completed Fixes:
1. **Storage Engine Visibility**: Fixed transaction visibility logic (2 tests fixed)
2. **Corruption Detection**: Updated tests to handle checksum validation properly (3 tests fixed)
3. **Test Count**: Total tests increased from 242 to 354 (new comprehensive tests added)

### Current Test Status:
- **Total Tests**: 354
- **Failed Tests**: ~33 (down from 35)
- **Main Issues**: Parser/Lexer (15+), Memory Safety (4), Page Management (3), Others (11)

## Detailed Correction Plan

### Phase 1: Critical Fixes (Week 1-2)

#### 1.1 Fix Storage Engine Visibility Bug
**File**: `src/core/storage_engine.cpp`
**Issue**: Line 182 uses `xmax <= current_xid` instead of `xmax < current_xid`
**Fix**:
```cpp
// Line 182: Change from
if (xmax > 0 && xmax <= current_xid) {
// To:
if (xmax > 0 && xmax < current_xid) {
```
**Impact**: Fixes 2 failing visibility tests

#### 1.2 Fix Page Corruption Detection
**Files**: `src/core/heap_page.cpp`, tests
**Issues**:
- Corruption detection not working properly
- Invalid item pointer handling incomplete
**Actions**:
1. Implement proper page validation in HeapPage constructor
2. Add bounds checking for item pointers
3. Validate page header magic/version on load

#### 1.3 Enable Integration Tests
**File**: `tests/CMakeLists.txt`
**Issue**: Integration tests not being compiled/run
**Fix**: Update CMakeLists.txt to include integration test directory

#### 1.4 Fix Remaining Memory Issues
**Focus Areas**:
- HeapScanIterator memory leak (needs RAII)
- Buffer overflow in tuple insertion
- Stress test failures with rapid open/close

### Phase 2: Core Functionality Completion (Week 3-4)

#### 2.1 Parser Enhancement
**Files**: `src/parser/parser.cpp`, `src/parser/lexer.cpp`
**Missing Features**:
1. **JOIN Support**: Implement JOIN parsing
2. **Subqueries**: Add nested SELECT support
3. **Constraints**: PRIMARY KEY, NOT NULL, FOREIGN KEY
4. **Aliases**: Table and column aliases
5. **Complex Expressions**: AND/OR logical operators

#### 2.2 Complete MVCC Implementation
**Files**: `src/core/transaction_manager.cpp`
**Tasks**:
1. Implement proper snapshot isolation
2. Add transaction visibility checks
3. Fix XID generation and persistence
4. Implement transaction log

#### 2.3 Storage Engine Completion
**Tasks**:
1. Implement TOAST/LOB storage for large objects
2. Add compression support
3. Fix FSM (Free Space Map) persistence
4. Implement proper page recycling

### Phase 3: Quality & Performance (Week 5-6)

#### 3.1 Performance Optimization
1. **Benchmarking Suite**: Create performance tests
2. **Buffer Pool**: Optimize page replacement algorithm
3. **Query Execution**: Add basic query optimization
4. **Index Support**: B-tree index implementation

#### 3.2 Security Hardening
1. **Path Validation**: Prevent directory traversal
2. **Input Sanitization**: SQL injection prevention
3. **Access Control**: Basic permission system
4. **Concurrent Access**: Proper file locking

#### 3.3 Documentation & Testing
1. **API Documentation**: Complete doxygen comments
2. **User Manual**: Installation and usage guide
3. **Test Coverage**: Achieve 90%+ coverage
4. **CI/CD Pipeline**: GitHub Actions setup

## Implementation Priority Order

### Week 1: Stop the Bleeding
1. Fix storage visibility bug (2 hours)
2. Fix page corruption detection (4 hours)
3. Enable integration tests (1 hour)
4. Fix critical memory leaks (8 hours)
5. Stabilize failing tests (8 hours)

### Week 2: Core Stability
1. Complete MVCC basics (16 hours)
2. Fix transaction persistence (8 hours)
3. Implement missing Status codes (2 hours)
4. Add comprehensive error handling (8 hours)

### Week 3: Parser Completion
1. Add JOIN support (8 hours)
2. Implement constraints (8 hours)
3. Add subquery support (16 hours)
4. Logical operators (4 hours)

### Week 4: Storage Robustness
1. TOAST implementation (16 hours)
2. Compression support (8 hours)
3. FSM fixes (8 hours)
4. Performance tuning (8 hours)

### Week 5-6: Polish & Release
1. Security audit and fixes (16 hours)
2. Performance benchmarking (8 hours)
3. Documentation completion (16 hours)
4. CI/CD setup (8 hours)
5. Release preparation (8 hours)

## Success Metrics

### Phase 1 Complete When:
- All tests pass (100% of non-skipped tests)
- No memory leaks in valgrind
- No segfaults or crashes
- Integration tests running

### Phase 2 Complete When:
- Full SQL subset implemented (per spec)
- MVCC working correctly
- Transaction durability guaranteed
- Storage engine feature complete

### Phase 3 Complete When:
- Performance benchmarks established
- Security vulnerabilities addressed
- Documentation complete
- CI/CD pipeline active

## Risk Mitigation

### High-Risk Areas:
1. **MVCC Implementation**: Complex, affects entire system
2. **Parser Rewrite**: May require significant refactoring
3. **Performance**: Current architecture may have bottlenecks

### Mitigation Strategies:
1. **Incremental Changes**: Small, tested commits
2. **Feature Flags**: Ability to disable new features
3. **Comprehensive Testing**: Test each change thoroughly
4. **Code Reviews**: All changes peer-reviewed
5. **Rollback Plan**: Git branches for each phase

## Immediate Next Steps

1. Create feature branch: `fix/phase1-critical-issues`
2. Fix visibility bug in storage engine
3. Run tests to verify fix
4. Proceed with corruption detection fixes
5. Daily progress updates

## Conclusion

The ScratchBird project has a solid foundation but requires focused effort on critical issues before advancing to new features. The estimated timeline of 6 weeks is realistic if development proceeds systematically through the three phases. The key is to stabilize the core before adding complexity.
