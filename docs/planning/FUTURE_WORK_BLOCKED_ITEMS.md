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

### P3-15: Materialized View Rewriting (40-50 hours)
**Blocker:** Requires query cost estimation/statistics
**Original Plan:** IMPROVEMENTS_P3_LOW_PRIORITY_PLAN.md

**Features:**
- Automatic MV selection for queries
- Query pattern matching
- Cost-based MV selection
- MV substitution in query plan

**Dependencies:**
- P1-10: Statistics & ANALYZE (COMPLETE)
- Query cost model refinement
- MV dependency tracking

---

### P3-20: Join Ordering Optimization (25-30 hours)
**Blocker:** Requires cardinality estimates
**Original Plan:** IMPROVEMENTS_P3_LOW_PRIORITY_PLAN.md

**Features:**
- Cost-based join ordering
- Dynamic programming or greedy algorithm
- Multi-way join optimization

**Dependencies:**
- P1-10: Statistics & ANALYZE (COMPLETE)
- Accurate cardinality estimation
- Join selectivity estimates

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

| Item | Blocker | Est. Hours | Priority |
|------|---------|------------|----------|
| P3-2: MFA | Alpha 3 (Network) | 40-50 | Medium |
| P3-3: IP Whitelisting | Alpha 3 (Network) | 8-10 | Medium |
| P3-4: Certificate Auth | Alpha 3 (Network) | 20-25 | Medium |
| P3-14: Partition Pruning | Table Partitioning | 30-40 | Medium |
| P3-15: MV Rewriting | Cost Model | 40-50 | Low |
| P3-20: Join Ordering | Cardinality Est. | 25-30 | Low |
| Window Function Args | Infrastructure | 8-12 | Low |
| Window PARTITION BY | Frame Handling | 12-16 | Low |
| NTH_VALUE | Arg Parsing | 4-6 | Low |

**Total Blocked Hours:** ~170-240 hours

---

## When to Revisit

- **P3-2, P3-3, P3-4:** After Alpha Phase 3 network layer is implemented
- **P3-14:** After table partitioning is implemented in Alpha Phase 2
- **P3-15, P3-20:** Can be started now but will be more effective with better statistics
- **Window Function Enhancements:** Low priority - current implementations are functional

---

## Note on Local Server Architecture

Local Server Architecture is **NO LONGER BLOCKED** - it is now the **NEXT PRIORITY** after completion of Catalog Cleanup (all 4 phases complete as of November 26, 2025).

See [LOCAL_SERVER_ARCHITECTURE_PLAN.md](LOCAL_SERVER_ARCHITECTURE_PLAN.md) for the detailed implementation plan (~140-190 hours).

---

**Document Version:** 1.1
**Last Updated:** November 26, 2025
