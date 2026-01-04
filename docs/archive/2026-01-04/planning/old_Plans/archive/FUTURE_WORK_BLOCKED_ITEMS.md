# Future Work - Items Blocked by Alpha Phase 2/3 Dependencies

**Created:** November 26, 2025
**Status:** DEFERRED
**Purpose:** Track completed planning items that are blocked until Alpha Phase 2 or 3

---

## Overview

This document consolidates all improvement items that were planned and documented but are blocked by dependencies on Alpha Phase 2 (Distributed MVCC, Wire Protocols) or Alpha Phase 3 (Network Layer). These items should be revisited once their dependencies are available.

---

## P3 Items Blocked by Alpha Phase 3 (Network Layer)

These security features require the network layer implementation before they can be completed.

### P3-2: Multi-Factor Authentication (40-50 hours)
**Blocker:** Requires Alpha 3 network layer
**Original Plan:** IMPROVEMENTS_P3_LOW_PRIORITY_PLAN.md

**Features:**
- TOTP (time-based one-time passwords)
- WebAuthn support
- SMS/email verification

**Dependencies:**
- Network authentication handshake
- Client-server challenge-response protocol
- Session token management over network

---

### P3-3: IP Whitelisting/Blacklisting (8-10 hours)
**Blocker:** Requires Alpha 3 network layer
**Original Plan:** IMPROVEMENTS_P3_LOW_PRIORITY_PLAN.md

**Features:**
- IP-based access control
- CIDR range support
- Per-user and global rules

**Dependencies:**
- Network listener to capture client IP
- Connection pre-authentication hook

---

### P3-4: Certificate-Based Authentication (20-25 hours)
**Blocker:** Requires Alpha 3 network layer
**Original Plan:** IMPROVEMENTS_P3_LOW_PRIORITY_PLAN.md

**Features:**
- X.509 client certificates
- Certificate chain validation
- CN-to-user mapping

**Dependencies:**
- TLS/SSL implementation
- Certificate store management

---

## P3 Items Blocked by Other Dependencies

### P3-14: Partition Pruning (30-40 hours)
**Blocker:** Requires table partitioning implementation
**Original Plan:** IMPROVEMENTS_P3_LOW_PRIORITY_PLAN.md

**Features:**
- Table partitioning (RANGE, LIST, HASH)
- Predicate-based partition elimination
- Partition-wise joins
- Dynamic partition pruning

**Dependencies:**
- CREATE TABLE ... PARTITION BY syntax
- Partition metadata in catalog
- Partition-aware query planner

---

### P3-15: Materialized View Rewriting ✅ COMPLETE
**Status:** IMPLEMENTED - November 28, 2025

**Implementation:**
- `include/scratchbird/optimizer/mv_rewriter.h` - MV rewriter header
- `src/optimizer/mv_rewriter.cpp` - Full implementation
- `tests/test_mv_rewriter.cpp` - 9 unit tests (all passing)

**Features Implemented:**
- QueryPattern extraction from SELECT statements
- MVCandidate finding and cost comparison
- Subsumption checking (exact match and superset)
- Cost-based MV selection
- Query rewriting to use MVs
- Staleness tolerance configuration
- Statistics tracking (attempts, successes, cost savings)

---

### P3-20: Join Ordering Optimization ✅ COMPLETE
**Status:** IMPLEMENTED - November 28, 2025

**Implementation:**
- `include/scratchbird/optimizer/join_ordering.h` - Join ordering header
- `src/optimizer/join_ordering.cpp` - Full implementation
- `tests/test_join_ordering.cpp` - 5 unit tests (all passing)

**Features Implemented:**
- Dynamic programming join order optimizer (Selinger-style)
- Greedy fallback for queries with >12 tables
- RelationInfo and JoinEdge structures
- Cost-based join costing
- Selectivity estimation integration
- O(3^N) DP algorithm with O(2^N) memoization

---

## Optional Window Function Enhancements

### NTH_VALUE Full Implementation (~4-6 hours)
**Blocker:** Window function argument parsing infrastructure
**Original Plan:** MISSING_FUNCTIONS_IMPLEMENTATION_STATUS.md

**Features:**
- Currently returns NULL
- Requires argument parsing infrastructure
- Low priority - simplified window functions meet most use cases

---

### Window Function Argument Parsing (~8-12 hours)
**Blocker:** Infrastructure work
**Original Plan:** MISSING_FUNCTIONS_IMPLEMENTATION_STATUS.md

**Features:**
- Enable LAG(column, offset), LEAD(column, offset) with custom columns/offsets
- Enable NTH_VALUE(column, n) full implementation

---

### Full PARTITION BY/ORDER BY Support for Window Functions (~12-16 hours)
**Blocker:** Requires proper window frame handling
**Original Plan:** MISSING_FUNCTIONS_IMPLEMENTATION_STATUS.md

**Features:**
- Proper ranking based on ORDER BY columns
- Partitioned window calculations
- Frame specification handling

---

## Summary

| Item | Blocker | Est. Hours | Priority | Status |
|------|---------|------------|----------|--------|
| P3-2: MFA | Alpha 3 (Network) | 40-50 | Medium | BLOCKED |
| P3-3: IP Whitelisting | Alpha 3 (Network) | 8-10 | Medium | BLOCKED |
| P3-4: Certificate Auth | Alpha 3 (Network) | 20-25 | Medium | BLOCKED |
| P3-14: Partition Pruning | Table Partitioning | 30-40 | Medium | BLOCKED |
| P3-15: MV Rewriting | N/A | 40-50 | Low | ✅ **COMPLETE** |
| P3-20: Join Ordering | N/A | 25-30 | Low | ✅ **COMPLETE** |
| Window Function Args | Infrastructure | 8-12 | Low | DEFERRED |
| Window PARTITION BY | Frame Handling | 12-16 | Low | DEFERRED |
| NTH_VALUE | Arg Parsing | 4-6 | Low | DEFERRED |

**Hard Blocked Hours:** ~100-125 hours (Alpha 3 network + partitioning)
**Completed:** ~65-80 hours (P3-15 + P3-20) - November 28, 2025

---

## When to Revisit

- **P3-2, P3-3, P3-4:** After Alpha Phase 3 network layer is implemented (BLOCKED)
- **P3-14:** After table partitioning syntax/catalog is implemented (BLOCKED)
- **P3-15, P3-20:** ✅ **COMPLETE** - Implemented November 28, 2025
- **Window Function Enhancements:** Low priority - current implementations are functional (DEFERRED)

---

## Note on Alpha 1 Status

**Alpha 1 is COMPLETE!** (November 28, 2025)

- ✅ Local Server Architecture - All 5 phases complete
- ✅ CLI Tools - All 4 tools implemented and tested
- ✅ CODE_COMPLETION_MASTER_PLAN - 135/135 items (100%)
- ✅ P0-P2 Improvements - 48/48 complete
- ✅ Catalog Cleanup - All 4 phases complete

**Next Phase:** Alpha 2 - Parser Separation
See [OFFICIAL_ROADMAP.md](/OFFICIAL_ROADMAP.md) for details.

---

**Document Version:** 1.3
**Last Updated:** November 28, 2025 - Alpha 1 complete, P3-15 and P3-20 completed
