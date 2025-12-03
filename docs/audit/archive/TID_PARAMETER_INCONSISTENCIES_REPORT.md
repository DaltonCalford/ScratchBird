# TID Parameter Inconsistencies Report
## ScratchBird Index Function Consistency Audit

**Date:** November 22, 2025
**Analysis Scope:** All index implementations in `include/scratchbird/core/`
**Analysis Focus:** TID parameter passing styles and return value conventions

---

## Executive Summary

This audit reveals **significant inconsistencies** in how TID (Tuple ID) parameters are passed and returned across different index implementations. These inconsistencies affect:

1. **Insert/Remove Parameter Styles** - Mostly consistent but with spacing variations
2. **Search/Find Return Styles** - **CRITICAL: Three different patterns used**
3. **API Completeness** - Missing methods and alternative parameter types

### Critical Issues Found: 3
### Warning Issues Found: 5
### Info Issues Found: 2

---

## CRITICAL INCONSISTENCIES

### 1. Search/Find Return Value Patterns - THREE DIFFERENT CONVENTIONS

The most significant inconsistency is how search/find operations return results. There are three competing patterns:

#### Pattern A: Value Return (4 indexes)
**Used by:** HashIndex, GinIndex, BitmapIndex, FullTextIndex

```cpp
// Returns vector by value
std::vector<TID> find(const void *key_data, size_t key_len,
                      uint64_t current_xid, ErrorContext *ctx = nullptr);

// Called as:
auto results = index->find(key, key_len, xid, ctx);
```

**Files:**
- `/home/user/ScratchBird/include/scratchbird/core/hash_index.h` (line 127-129)
- `/home/user/ScratchBird/include/scratchbird/core/gin_index.h` (line 283-285)
- `/home/user/ScratchBird/include/scratchbird/core/bitmap_index.h` (line 186-190)
- `/home/user/ScratchBird/include/scratchbird/core/fulltext_index.h` (line 153-155)

#### Pattern B: Output Parameter by Pointer (3 indexes)
**Used by:** BTree, RTreeIndex, HnswIndex

```cpp
// Status return + output parameter by pointer
Status search(const std::vector<uint8_t> &key, uint64_t current_xid,
              std::vector<TID> *results_out, ErrorContext *ctx = nullptr);

// Called as:
std::vector<TID> results;
auto status = index->search(key, xid, &results, ctx);
```

**Files:**
- `/home/user/ScratchBird/include/scratchbird/core/btree.h` (line 181-184)
- `/home/user/ScratchBird/include/scratchbird/core/rtree_index.h` (line 47-48)
- `/home/user/ScratchBird/include/scratchbird/core/hnsw_index.h` (line 292-296) [wrapped in HnswSearchResult]

#### Pattern C: Output Parameter by Reference (2 indexes)
**Used by:** GiSTIndex, SPGiSTIndex

```cpp
// Status return + output parameter by reference
Status search(const std::vector<uint8_t>& query, GiSTStrategy strategy,
              uint64_t current_xid, std::vector<TID>& results,
              ErrorContext* ctx);

// Called as:
std::vector<TID> results;
auto status = index->search(query, strategy, xid, results, ctx);
```

**Files:**
- `/home/user/ScratchBird/include/scratchbird/core/gist_index.h` (line 412-416)
- `/home/user/ScratchBird/include/scratchbird/core/spgist_index.h` (line 408-411)

#### Impact Analysis

| Index Type | Pattern | Implications |
|-----------|---------|--------------|
| BTree | B (pointer) | Must check Status AND use *results_out |
| HashIndex | A (value) | Simple return, clear ownership |
| GinIndex | A (value) | Simple return, clear ownership |
| GiSTIndex | C (reference) | Less clear about initialization |
| SPGiSTIndex | C (reference) | Less clear about initialization |
| RTreeIndex | B (pointer) | Must check Status AND use *results_out |
| HnswIndex | B (pointer, wrapped) | Different struct; requires unwrapping |
| BrinIndex | DIFFERENT | Returns block numbers, not TIDs! |
| BitmapIndex | A (value) | Simple return, clear ownership |
| FullTextIndex | A (value) | Simple return, clear ownership |

**Recommendation:** Standardize on **Pattern A (value return)** for all indexes. It's the most idiomatic C++ and matches HashIndex, GinIndex, BitmapIndex, FullTextIndex patterns.

---

## WARNING LEVEL INCONSISTENCIES

### 2. BrinIndex: Completely Different API - Uses Block Numbers Instead of TIDs

**Severity:** CRITICAL - API mismatch with all other indexes

**Issue:**
BrinIndex uses `uint32_t block_number` instead of `TID` throughout its API. This violates the expected contract for index implementations.

**Inconsistency Details:**

```cpp
// Insert - DIFFERENT from other indexes
Status insert(const std::vector<uint8_t> &value,
              uint32_t block_number,              // NOT TID!
              ErrorContext *ctx = nullptr);

// Search/Scan - DIFFERENT from other indexes
Status scan(const std::vector<uint8_t> *min_value,
            const std::vector<uint8_t> *max_value,
            uint64_t current_xid,
            std::vector<uint32_t> *block_numbers_out,  // Returns blocks, not TIDs!
            ErrorContext *ctx = nullptr);

// Remove - DIFFERENT from other indexes
Status remove(const std::vector<uint8_t> &value,
              uint32_t block_number,              // NOT TID!
              ErrorContext *ctx = nullptr);
```

**File:** `/home/user/ScratchBird/include/scratchbird/core/brin_index.h` (lines 218-259)

**Architectural Rationale:** BRIN (Block Range Index) stores summaries for block ranges rather than individual tuple IDs. This is by design for space efficiency in time-series data.

**Recommendation:** Document this as an intentional API difference. Consider creating a separate interface `BlockRangeIndexInterface` for indexes that don't use TID-based operations.

---

### 3. Insert/Remove Parameter Spacing Inconsistency

**Severity:** INFO - Cosmetic, but inconsistent

The TID parameter is passed consistently as `const TID reference`, but with different spacing conventions:

#### Group 1: `const TID &` (with space - 6 indexes)
- BTree: `const TID &tid` (line 174, 188, 207)
- HashIndex: `const TID &tid` (line 119, 134)
- RTreeIndex: `const TID &tid` (line 43, 51)
- HnswIndex: `const TID &tid` (line 250, 266)
- BitmapIndex: `const TID &tid` (line 174, 180)

#### Group 2: `const TID&` (no space - 4 indexes)
- GiSTIndex: `const TID& tid` (line 398, 428)
- SPGiSTIndex: `const TID& tid` (line 396, 417)
- GinIndex: `const TID &tid` (mixed)
- FullTextIndex: `const TID& tid` (line 137)

**Code Examples:**

BTree (with space):
```cpp
Status insert(const std::vector<uint8_t> &key, const TID &tid, uint64_t xid, ...);
```

GiSTIndex (no space):
```cpp
Status insert(const GiSTPredicate& predicate, const TID& tid, uint64_t current_xid, ...);
```

**Recommendation:** Standardize on `const TID&` (no space after reference operator) to match modern C++ style guide conventions.

---

### 4. HnswIndex: TID Return Wrapped in Custom Structure

**Severity:** WARNING - Inconsistent return type

HnswIndex wraps TID results in a custom structure instead of returning TID directly:

```cpp
struct HnswSearchResult
{
    TID tid;          // Actual TID
    double distance;  // Distance/similarity score
};

Status search(const VectorValue &query_vector,
              uint32_t k,
              uint64_t current_xid,
              std::vector<HnswSearchResult> *results_out,  // Custom struct, not TID!
              ...);
```

**File:** `/home/user/ScratchBird/include/scratchbird/core/hnsw_index.h` (lines 193-202, 292-296)

**Rationale:** Justified - HnswIndex needs to return distance scores alongside TIDs for k-NN search ranking.

**Recommendation:** Acceptable design decision. Document this intentional difference.

---

### 5. FullTextIndex: Missing remove() Method

**Severity:** WARNING - Incomplete API

FullTextIndex has no explicit `remove()` method while all other indexes provide delete/remove operations.

**Defined methods:**
- `insert()` (line 136-137)
- `search()` (line 153-155)
- **Missing:** `remove()`

**File:** `/home/user/ScratchBird/include/scratchbird/core/fulltext_index.h`

**Questions:**
1. Is removal handled through the underlying GinIndex?
2. Should explicit remove() be added for API consistency?

**Recommendation:** Add explicit `remove()` method with signature:
```cpp
Status remove(const void *tsvector_data, size_t tsvector_len, const TID& tid,
              ErrorContext* ctx = nullptr);
```

---

## DETAILED CONSISTENCY TABLE

### Insert Function Style

| Index | File | Line | Parameter Style | TID Type | Notes |
|-------|------|------|-----------------|----------|-------|
| BTree | btree.h | 174 | Modern | `const TID &tid` | Space after & |
| HashIndex | hash_index.h | 119 | Modern | `const TID &tid` | Space after & |
| GinIndex | gin_index.h | 268 | Modern | `const TID &tid` | Space after & |
| GiSTIndex | gist_index.h | 398 | Modern | `const TID& tid` | No space after & |
| SPGiSTIndex | spgist_index.h | 396 | Modern | `const TID& tid` | No space after & |
| RTreeIndex | rtree_index.h | 43 | Modern | `const TID &tid` | Space after & |
| HnswIndex | hnsw_index.h | 250 | Modern | `const TID &tid` | Space after & |
| BrinIndex | brin_index.h | 218 | DIFFERENT | `uint32_t block_number` | **Uses block_number, not TID** |
| BitmapIndex | bitmap_index.h | 174 | Modern | `const TID &tid` | Space after & |
| FullTextIndex | fulltext_index.h | 137 | Modern | `const TID& tid` | No space after & |

### Remove Function Style

| Index | File | Line | Parameter Style | TID Type | Notes |
|-------|------|------|-----------------|----------|-------|
| BTree | btree.h | 188 | Modern | `const TID &tid` | Space after & |
| HashIndex | hash_index.h | 134 | Modern | `const TID &tid` | Space after & |
| GinIndex | gin_index.h | 275 | Modern | `const TID &tid` | Space after & |
| GiSTIndex | gist_index.h | 428 | Modern | `const TID& tid` | No space after & |
| SPGiSTIndex | spgist_index.h | 417 | Modern | `const TID& tid` | No space after & |
| RTreeIndex | rtree_index.h | 51 | Modern | `const TID &tid` | Space after & |
| HnswIndex | hnsw_index.h | 266 | Modern | `const TID &tid` | Space after & |
| BrinIndex | brin_index.h | 257 | DIFFERENT | `uint32_t block_number` | **Uses block_number, not TID** |
| BitmapIndex | bitmap_index.h | 180 | Simplified | `const TID &tid` | Only takes TID (no key) |
| FullTextIndex | fulltext_index.h | - | N/A | N/A | **Method not defined** |

### Search/Find Return Style

| Index | File | Line | Return Pattern | Signature |
|-------|------|------|-----------------|-----------|
| BTree | btree.h | 181-183 | **Pattern B** (pointer) | `Status search(..., std::vector<TID> *tids_out, ...)` |
| HashIndex | hash_index.h | 127-129 | **Pattern A** (value) | `std::vector<TID> find(...)` |
| GinIndex | gin_index.h | 283-285 | **Pattern A** (value) | `std::vector<TID> find(...)` |
| GiSTIndex | gist_index.h | 412-416 | **Pattern C** (reference) | `Status search(..., std::vector<TID>& results, ...)` |
| SPGiSTIndex | spgist_index.h | 408-411 | **Pattern C** (reference) | `Status search(..., std::vector<TID>& results, ...)` |
| RTreeIndex | rtree_index.h | 47-48 | **Pattern B** (pointer) | `Status search(..., std::vector<TID> *results_out, ...)` |
| HnswIndex | hnsw_index.h | 292-296 | **Pattern B** (pointer, wrapped) | `Status search(..., std::vector<HnswSearchResult> *results_out, ...)` |
| BrinIndex | brin_index.h | 241-245 | **DIFFERENT** | `Status scan(..., std::vector<uint32_t> *block_numbers_out, ...)` |
| BitmapIndex | bitmap_index.h | 186-190 | **Pattern A** (value) | `std::vector<TID> find(...)` |
| FullTextIndex | fulltext_index.h | 153-155 | **Pattern A** (value) | `std::vector<TID> search(...)` |

---

## RECOMMENDATIONS BY PRIORITY

### Priority 1: CRITICAL - Standardize Search/Find Return Pattern

**Current State:** 3 competing patterns used across 10 indexes

**Recommendation:** Standardize on **Pattern A (value return)** for consistency with:
- HashIndex
- GinIndex
- BitmapIndex
- FullTextIndex

**Action Items:**
1. Update BTree::search() to return `std::vector<TID>` instead of using output parameter
2. Update RTreeIndex::search() to return `std::vector<TID>` instead of using output parameter
3. Update GiSTIndex::search() to return `std::vector<TID>` instead of using reference parameter
4. Update SPGiSTIndex::search() to return `std::vector<TID>` instead of using reference parameter
5. For HnswIndex, consider whether distance score is essential or can be retrieved separately

**Rationale:**
- Value return is idiomatic C++11+
- More consistent with modern API design (auto results = index->find(key))
- Clearer ownership semantics
- Eliminates need to check return value for success when also using output parameter

---

### Priority 2: HIGH - Add remove() Method to FullTextIndex

**Current State:** FullTextIndex lacks remove() method

**Recommendation:** Add explicit remove() method:

```cpp
Status remove(const void *tsvector_data, size_t tsvector_len,
              const TID &tid, ErrorContext *ctx = nullptr);
```

**File:** `/home/user/ScratchBird/include/scratchbird/core/fulltext_index.h`

**Rationale:** API completeness and consistency with other indexes

---

### Priority 3: MEDIUM - Standardize Parameter Spacing

**Current State:** Inconsistent spacing around `&` in `const TID&` vs `const TID &`

**Recommendation:** Standardize on `const TID&` (no space) following modern C++ style guides

**Files to Update:**
- BTree (btree.h)
- HashIndex (hash_index.h)
- RTreeIndex (rtree_index.h)
- HnswIndex (hnsw_index.h)
- BitmapIndex (bitmap_index.h)

---

### Priority 4: MEDIUM - Document BrinIndex API Difference

**Current State:** BrinIndex uses block_number instead of TID

**Recommendation:** Add prominent documentation explaining:
1. Why BrinIndex uses block numbers (space efficiency for range summaries)
2. How block numbers relate to TIDs
3. Whether this is Phase 1.5 complete or pending migration

**File:** `/home/user/ScratchBird/include/scratchbird/core/brin_index.h` - Add comments at class level

---

### Priority 5: LOW - Document HnswIndex Return Wrapper

**Current State:** HnswIndex returns custom HnswSearchResult struct

**Recommendation:** Document intentional design decision in header comments

**File:** `/home/user/ScratchBird/include/scratchbird/core/hnsw_index.h`

**Comment to Add:**
```cpp
// Design Note: Returns HnswSearchResult struct instead of bare TID to include
// distance/similarity scores needed for k-NN result ranking. This is an
// intentional API difference from other index types.
```

---

## MIGRATION NOTES

### Phase 1.5 Completion Status

Per project documentation, Phase 1.5 Task 1.5.2 was to migrate all indexes to TID struct API. Based on this audit:

**✓ COMPLETE:**
- BTree (uses const TID&)
- HashIndex (uses const TID&)
- GinIndex (uses const TID&)
- GiSTIndex (uses const TID&)
- SPGiSTIndex (uses const TID&)
- RTreeIndex (uses const TID&)
- HnswIndex (uses const TID&)
- BitmapIndex (uses const TID&)
- FullTextIndex (uses const TID&)

**✗ POTENTIALLY INCOMPLETE:**
- BrinIndex: Uses block_number instead of TID - Verify if this is intentional or pending

---

## APPENDIX: API CONSISTENCY CHECKLIST

### Index Interface Completeness

| Index | insert() | remove() | search() | scan() | removeDeadEntries() | Status |
|-------|----------|----------|----------|--------|-------------------|--------|
| BTree | ✓ | ✓ | ✓ | ✓ (rangeScan) | ✓ | Complete |
| HashIndex | ✓ | ✓ | ✓ (find) | - | ✓ | Complete |
| GinIndex | ✓ | ✓ | ✓ (find+variants) | - | ✓ | Complete |
| GiSTIndex | ✓ | ✓ | ✓ | - | ✓ | Complete |
| SPGiSTIndex | ✓ | ✓ | ✓ | - | ✓ | Complete |
| RTreeIndex | ✓ | ✓ | ✓ | - | ✓ | Complete |
| HnswIndex | ✓ | ✓ | ✓ | - | ✓ | Complete |
| BrinIndex | ✓ | ✓ | ✓ (scan) | ✓ (scan) | ✓ | Complete (but different API) |
| BitmapIndex | ✓ | ✓ | ✓ (find+variants) | ✓ | ✓ | Complete |
| FullTextIndex | ✓ | ✗ | ✓ | - | - | **Incomplete** |

---

## Conclusion

The ScratchBird index implementations show good Phase 1.5 migration completion for TID struct usage, but reveal **significant inconsistency in search/find return patterns** that should be standardized. The priority should be:

1. Standardize search/find return pattern across all indexes
2. Complete FullTextIndex API with remove() method
3. Standardize parameter formatting
4. Document intentional API differences (BrinIndex, HnswIndex)

These changes would significantly improve API consistency and developer experience.
