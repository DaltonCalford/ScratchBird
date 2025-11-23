# B-Tree Prefix Compression - Testing Plan

**Issue**: 2.17 - B-Tree Prefix Compression
**Status**: Implementation Complete ✅
**Testing Status**: Plan Created, Execution Pending

---

## Testing Requirements (Per ALPHA_ISSUES_TRACKER.md)

Since B-Tree compression is not a critical/high/medium concurrency issue, it doesn't require ThreadSanitizer or Helgrind tests. However, we still need comprehensive functional and performance testing.

### Required Tests for B-Tree Compression

#### 1. Unit Tests ✅ (Created, Execution Pending)
- [x] **BasicInsertAndSearch** - Validates basic compression workflow
- [x] **ShortKeysNotCompressed** - Verifies keys < 8 bytes remain uncompressed
- [x] **SmallPrefixNotCompressed** - Verifies prefix < 4 bytes not compressed
- [x] **LargePrefixShouldCompress** - Validates compression activates for large prefixes

**Status**: Test code written, pending execution

#### 2. Integration Tests ✅ (Created, Execution Pending)
- [x] **UUIDv7KeysCompression** - Tests temporally-ordered UUIDs (40-60% expected savings)
- [x] **StringKeysWithCommonPrefix** - Tests strings with common prefix (30-50% expected savings)
- [x] **MixedCompressibleAndNonCompressible** - Tests mixed workload
- [x] **RangeScanWithCompression** - Sequential scan correctness
- [x] **RemoveWithCompression** - Deletion with compressed keys

**Status**: Test code written, pending execution

#### 3. Performance Benchmarks ✅ (Created, Execution Pending)
- [x] **BenchmarkUUIDv7Compression** - 1,000 UUIDv7 inserts + searches (μs precision)
- [x] **BenchmarkStringKeyCompression** - 1,000 string inserts + searches
- [x] **BenchmarkRandomKeyNoCompression** - 1,000 random keys (baseline)

**Status**: Test code written, pending execution

#### 4. Edge Case Tests ✅ (Created, Execution Pending)
- [x] **EmptyKeyHandling** - Empty key behavior
- [x] **IdenticalKeys** - Duplicate keys (100% prefix match)
- [x] **LargeScaleStressTest** - 10,000 key stress test

**Status**: Test code written, pending execution

---

## Test Execution Blockers

### Current Status
- ✅ **Test Suite Created**: 623 lines, 15 comprehensive test cases
- ✅ **Test Compilation**: Individual file compiles successfully
- ❌ **Test Linking**: Blocked by infrastructure issues (40+ files with standalone main())

### Resolution Required
1. **Fix Test Infrastructure** (2-3 days):
   - Remove `main()` functions from standalone test files
   - Convert to GoogleTest TEST/TEST_F format
   - Ensure single test binary links successfully

2. **Execute Test Suite** (1 hour):
   ```bash
   cd build
   make scratchbird_tests    # Should succeed after infrastructure fix
   ctest -R test_btree_compression -V
   ```

3. **Collect Results** (30 minutes):
   - Verify all 15 tests pass
   - Record benchmark timing data
   - Measure actual compression ratios achieved

---

## Detailed Test Plan

### Phase 1: Unit Test Execution (Priority: CRITICAL)

**Test 1: BasicInsertAndSearch**
- **Purpose**: Validate core compression/decompression workflow
- **Steps**:
  1. Create B-tree index
  2. Insert 10 keys with common prefixes (e.g., "user_00001", "user_00002", ...)
  3. Search for all keys
  4. Verify all keys found correctly
- **Expected Result**: All searches succeed, no data corruption
- **Validation**: Search returns correct TID for each key

**Test 2: ShortKeysNotCompressed**
- **Purpose**: Verify heuristic skips short keys
- **Steps**:
  1. Insert keys of lengths 4, 6, 7 bytes with common prefixes
  2. Check internal storage (if possible) or measure space used
  3. Verify keys are stored uncompressed
- **Expected Result**: Short keys stored in full (no compression)
- **Validation**: Key retrieval works correctly, no space savings

**Test 3: SmallPrefixNotCompressed**
- **Purpose**: Verify heuristic skips small prefixes
- **Steps**:
  1. Insert 20-byte keys with 1, 2, 3-byte common prefixes
  2. Verify keys stored uncompressed
- **Expected Result**: Small prefix keys stored in full
- **Validation**: Key retrieval correct, minimal compression overhead

**Test 4: LargePrefixShouldCompress**
- **Purpose**: Verify compression activates for large prefixes
- **Steps**:
  1. Insert 32-byte keys with 16-byte common prefix
  2. Verify compression occurs
  3. Check keys stored as ~16 bytes (prefix metadata + suffix)
- **Expected Result**: Keys compressed to ~50% of original size
- **Validation**: Retrieval correct, space savings observed

---

### Phase 2: Integration Test Execution (Priority: HIGH)

**Test 5: UUIDv7KeysCompression**
- **Purpose**: Validate real-world workload (time-series data)
- **Steps**:
  1. Generate 1,000 UUIDv7 keys (temporally ordered)
  2. Insert into B-tree
  3. Measure space used
  4. Search for all keys
- **Expected Result**:
  - 40-60% space savings (UUIDv7 first 8 bytes are timestamp)
  - All searches succeed
  - No performance degradation
- **Validation**:
  - Compression ratio ≥ 40%
  - Search success rate = 100%
  - Insert time < 2x baseline

**Test 6: StringKeysWithCommonPrefix**
- **Purpose**: Validate compression with hierarchical data
- **Steps**:
  1. Insert 1,000 strings with common prefixes (e.g., "/users/alice/...", "/users/bob/...")
  2. Measure space used vs. uncompressed
  3. Perform range scan
- **Expected Result**:
  - 30-50% space savings
  - Range scan returns keys in correct order
- **Validation**:
  - Compression ratio ≥ 30%
  - Range scan order correct
  - No missing keys

**Test 7: MixedCompressibleAndNonCompressible**
- **Purpose**: Validate heuristics work correctly with heterogeneous data
- **Steps**:
  1. Insert mix of: short keys (4 bytes), medium keys (12 bytes), long keys (32 bytes)
  2. Insert mix of: high prefix similarity, low prefix similarity
  3. Verify heuristics apply correctly
- **Expected Result**:
  - Short keys remain uncompressed
  - Long keys with common prefix are compressed
  - No data corruption across mixed workload
- **Validation**:
  - All keys retrievable
  - Compression only where beneficial
  - No performance regression

**Test 8: RangeScanWithCompression**
- **Purpose**: Validate sequential scan correctness
- **Steps**:
  1. Insert 1,000 sorted keys with compression
  2. Perform full range scan
  3. Verify all keys returned in sorted order
- **Expected Result**:
  - All 1,000 keys returned
  - Sorted order maintained
  - No duplicates or missing keys
- **Validation**:
  - key[i] < key[i+1] for all i
  - Count = 1,000
  - Decompression works during iteration

**Test 9: RemoveWithCompression**
- **Purpose**: Validate deletion with compressed keys
- **Steps**:
  1. Insert 100 compressed keys
  2. Delete every other key (50 deletions)
  3. Verify remaining 50 keys still accessible
  4. Verify deleted keys return "not found"
- **Expected Result**:
  - All 50 remaining keys found
  - All 50 deleted keys not found
  - No corruption
- **Validation**:
  - Search success rate: 100% for remaining, 0% for deleted
  - No crashes or corruption

---

### Phase 3: Performance Benchmark Execution (Priority: MEDIUM)

**Test 10: BenchmarkUUIDv7Compression**
- **Purpose**: Measure actual performance with compressed UUIDs
- **Steps**:
  1. Generate 1,000 UUIDv7 keys
  2. Time: 1,000 inserts with compression
  3. Time: 1,000 searches
  4. Record: throughput, average latency, P99 latency
- **Expected Result**:
  - Insert: 1-5 μs per key
  - Search: 0.5-3 μs per key
  - Space: 40-60% reduction
- **Validation**:
  - Performance within expected range
  - Space savings ≥ 40%
  - No performance cliff

**Test 11: BenchmarkStringKeyCompression**
- **Purpose**: Measure performance with string keys
- **Steps**:
  1. Generate 1,000 strings with common prefix
  2. Time: inserts and searches
  3. Record: throughput and latency distribution
- **Expected Result**:
  - Insert: 1-5 μs per key
  - Search: 0.5-3 μs per key
  - Space: 30-50% reduction
- **Validation**:
  - Performance acceptable
  - Space savings ≥ 30%

**Test 12: BenchmarkRandomKeyNoCompression**
- **Purpose**: Establish baseline with no compression
- **Steps**:
  1. Generate 1,000 random keys (no common prefix)
  2. Time: inserts and searches
  3. Record: baseline performance
- **Expected Result**:
  - Insert: 1-5 μs per key (baseline)
  - Search: 0.5-3 μs per key (baseline)
  - Space: ~0% reduction (no compression)
- **Validation**:
  - Baseline established
  - Compression overhead minimal (<10%)

---

### Phase 4: Edge Case Test Execution (Priority: LOW)

**Test 13: EmptyKeyHandling**
- **Purpose**: Validate graceful handling of empty keys
- **Steps**:
  1. Attempt to insert empty key
  2. Verify behavior (reject or accept based on design)
  3. No crashes
- **Expected Result**:
  - Either: reject with error, or accept and store correctly
  - No crashes or undefined behavior
- **Validation**:
  - System remains stable
  - Behavior documented

**Test 14: IdenticalKeys**
- **Purpose**: Validate 100% prefix match scenario
- **Steps**:
  1. Insert duplicate keys (same key multiple times)
  2. Verify compression with 100% prefix overlap
  3. Verify key retrieval
- **Expected Result**:
  - System handles duplicates correctly
  - Compression works (if duplicates allowed)
  - Or rejects duplicates (if unique constraint)
- **Validation**:
  - No crashes
  - Documented behavior followed

**Test 15: LargeScaleStressTest**
- **Purpose**: Validate stability with large dataset
- **Steps**:
  1. Insert 10,000 keys with compression
  2. Verify all keys searchable
  3. Measure memory usage
  4. Check for leaks
- **Expected Result**:
  - All 10,000 keys inserted successfully
  - All 10,000 keys searchable
  - No memory leaks
  - Stable performance
- **Validation**:
  - valgrind: no leaks
  - Search success rate: 100%
  - Memory usage reasonable

---

## Success Criteria

### Must Pass (Blocking Issues)
- [ ] All 15 tests pass with 100% success rate
- [ ] No crashes or segfaults
- [ ] No data corruption (all inserted keys retrievable)
- [ ] No memory leaks (valgrind clean)

### Should Pass (Performance Goals)
- [ ] UUIDv7 compression ratio ≥ 40%
- [ ] String compression ratio ≥ 30%
- [ ] Insert performance within 2x of baseline
- [ ] Search performance within 1.5x of baseline

### Nice to Have (Optimization Goals)
- [ ] UUIDv7 compression ratio ≥ 50%
- [ ] String compression ratio ≥ 40%
- [ ] Insert performance within 1.5x of baseline
- [ ] Search performance within 1.2x of baseline

---

## Failure Scenarios and Mitigation

### Scenario 1: Tests Fail Due to Compression Bugs
**Symptoms**: Keys not found, wrong data returned
**Diagnosis**:
- Check `decompress_key()` logic (prefix + suffix reconstruction)
- Verify `calculate_prefix_length()` correctness
- Check `prev_key` tracking during search

**Mitigation**:
1. Add extensive logging to helper functions
2. Print compressed/decompressed keys for debugging
3. Fix bugs and re-test

### Scenario 2: Performance Worse Than Expected
**Symptoms**: Insert/search slower than baseline
**Diagnosis**:
- Profile with gprof or perf
- Check if decompression is hot path
- Verify binary search still O(log n)

**Mitigation**:
1. Add decompression caching
2. Optimize hot paths
3. Consider adaptive compression (enable/disable per page)

### Scenario 3: Compression Ratio Lower Than Expected
**Symptoms**: Space savings < 30% for strings, < 40% for UUIDs
**Diagnosis**:
- Check if heuristics too conservative
- Verify prefix calculation correct
- Check if key patterns have low similarity

**Mitigation**:
1. Adjust heuristic thresholds (4-byte → 3-byte prefix minimum)
2. Add more aggressive compression
3. Document actual ratios achieved

### Scenario 4: Memory Leaks Detected
**Symptoms**: valgrind reports leaks
**Diagnosis**:
- Check if std::vector cleanup correct
- Verify no orphaned allocations
- Check RAII patterns

**Mitigation**:
1. Add missing cleanup
2. Use smart pointers
3. Re-run valgrind

---

## Test Execution Schedule

### Week 1: Fix Test Infrastructure
- Days 1-2: Remove standalone main() from test files
- Day 3: Convert tests to GoogleTest format
- Day 4: Build and link test binary successfully

### Week 2: Execute Tests
- Day 1: Run unit tests (Tests 1-4)
- Day 2: Run integration tests (Tests 5-9)
- Day 3: Run performance benchmarks (Tests 10-12)
- Day 4: Run edge case tests (Tests 13-15)
- Day 5: Analyze results, fix any issues

### Week 3: Document Results
- Day 1: Compile test results
- Day 2: Update documentation with measured metrics
- Day 3: Write test report
- Day 4: Mark Issue 2.17 as FULLY VALIDATED
- Day 5: Buffer day for unexpected issues

---

## Test Results Documentation Template

```markdown
# B-Tree Compression Test Results

**Date**: [Execution Date]
**Build**: [Git Commit Hash]
**Environment**: [CPU, RAM, OS]

## Test Summary
- Total Tests: 15
- Passed: [X]
- Failed: [Y]
- Success Rate: [X/15 * 100]%

## Detailed Results

### Unit Tests (4 tests)
1. BasicInsertAndSearch: [PASS/FAIL] - [Duration] ms
2. ShortKeysNotCompressed: [PASS/FAIL] - [Duration] ms
3. SmallPrefixNotCompressed: [PASS/FAIL] - [Duration] ms
4. LargePrefixShouldCompress: [PASS/FAIL] - [Duration] ms

### Integration Tests (5 tests)
5. UUIDv7KeysCompression: [PASS/FAIL] - Compression: [X]%
6. StringKeysWithCommonPrefix: [PASS/FAIL] - Compression: [X]%
7. MixedCompressibleAndNonCompressible: [PASS/FAIL]
8. RangeScanWithCompression: [PASS/FAIL]
9. RemoveWithCompression: [PASS/FAIL]

### Performance Benchmarks (3 tests)
10. BenchmarkUUIDv7Compression:
    - Insert: [X] μs/key
    - Search: [Y] μs/key
    - Compression: [Z]%
11. BenchmarkStringKeyCompression:
    - Insert: [X] μs/key
    - Search: [Y] μs/key
    - Compression: [Z]%
12. BenchmarkRandomKeyNoCompression:
    - Insert: [X] μs/key (baseline)
    - Search: [Y] μs/key (baseline)

### Edge Case Tests (3 tests)
13. EmptyKeyHandling: [PASS/FAIL]
14. IdenticalKeys: [PASS/FAIL]
15. LargeScaleStressTest: [PASS/FAIL] - Memory: [X] MB

## Performance Analysis
- Average insert overhead: [X]% vs baseline
- Average search overhead: [Y]% vs baseline
- Average compression ratio: [Z]%
- Memory usage: [Acceptable/High/Low]

## Issues Found
[List any bugs or issues discovered during testing]

## Recommendations
[Any recommendations for optimization or fixes]

## Conclusion
[Overall assessment: Ready for production / Needs work / ...]
```

---

## Integration with ALPHA_ISSUES_TRACKER.md

### Current Status in Tracker
```
- ✅ Issue 2.17 (B-Tree prefix compression) ✅ IMPLEMENTED (Oct 17, 2025)
```

### After Testing Complete
```
- ✅ Issue 2.17 (B-Tree prefix compression) ✅ IMPLEMENTED & VALIDATED (Oct 17, 2025)
  - Implementation: Oct 17, 2025
  - Testing: [Date]
  - Test Results: 15/15 tests passed
  - Compression Ratios: UUIDs 45%, Strings 35%
  - Performance: Insert overhead 5%, Search overhead 2%
  - Status: PRODUCTION READY ✅
```

---

## Next Steps

### Immediate (This Week)
1. **Priority**: Fix test infrastructure (remove standalone main())
2. **Execute**: Run all 15 tests
3. **Document**: Record results in format above

### Short Term (Next Week)
4. **Analyze**: Review performance data
5. **Optimize**: Address any performance issues found
6. **Validate**: Re-run tests after optimizations

### Long Term (Beta Phase)
7. **Monitor**: Track compression ratios in production
8. **Benchmark**: Larger datasets (100K, 1M keys)
9. **Extend**: Add suffix truncation for internal nodes

---

**Document Version**: 1.0
**Author**: Claude (AI Assistant)
**Date**: 2025-10-17
**Status**: Testing Plan Complete, Awaiting Execution
