# B-Tree Specification

Status: current_authority

## Purpose

This document defines the ordered exact/range index family used by ScratchBird for the native B-tree implementation and all current alias surfaces routed through the same ordered-page runtime.

## ScratchBird shipped type coverage

Current runtime routes the following index types through the ordered B-tree family:

- `BTREE`
- `STL_SORT`
- `ART` current runtime surface
- `MONGODB_GEO_HAYSTACK`
- `NEO4J_RANGE`
- `NEO4J_POINT`
- `REDIS_LIST`
- `REDIS_ZSET`
- `REDIS_STREAM`

If a named surface needs semantics stronger than the current ordered-family runtime, it must remain parser-gated or fail-closed until a distinct implementation is promoted.

## Authoritative companion documents

The following documents remain authoritative where they provide more exact protocol detail:

- `INDEX_ARCHITECTURE.md`
- `INDEX_MGA_PUBLICATION_AND_RECLAIM.md`
- `BTREE_CONCURRENCY_AND_SPLIT_TOLERANT_DESCENT.md`
- `BTREE_PAGE_DELETION_MERGE_AND_RECLAMATION.md`
- `BTREE_DUPLICATE_KEY_AND_POSTING_LIST_MANAGEMENT.md`
- `BTREE_MGA_VERSION_CHURN_MANAGEMENT.md`
- `BTREE_VERIFICATION_AND_HARDENING_FRAMEWORK.md`

## MGA-first contract

The ordered-family contract is:

1. insert or update materializes heap/version truth first
2. ordered-family entries are published as candidate locators only
3. commit publishes transaction visibility through transaction inventory
4. readers descend the tree and fetch candidate TIDs
5. final row acceptance is decided by MGA visibility and backversion traversal
6. old ordered-family entries remain legal historical candidates until heap reclaim proof allows cleanup

The tree never becomes visibility authority.

## Structural rules

- split-tolerant sibling chase is required during descent
- page latches are structural only and must not redefine visibility
- duplicate keys may produce posting lists or repeated leaf entries according to the page-local format
- internal routing keys and high-key fences are structural hints only

## Write, delete, and reclaim rules

- insert publishes the new candidate entry for the new heap version
- update keeps old and new candidate entries coexisting until reclaim proof authorizes cleanup
- delete is represented by a new transactional state in heap lineage; index cleanup waits for heap reclaim proof
- dead entry cleanup follows `INDEX_VERSION_SEMANTICS_AND_DEAD_ENTRY_LIFECYCLE.md`
- page merge, deletion, and compaction are permitted only after structural validation and cleanup eligibility proof

## Search contract

- equality and range scans use ordered descent
- sibling chase is allowed when split or routing movement is detected
- every returned candidate TID must pass MGA visibility recheck
- range amplification and visibility rejects must be measured for optimizer calibration

## Required optimizer metrics

The ordered-family metrics packet shall include at minimum:

- tree depth
- branch page count
- leaf page count
- average keys per leaf
- duplicate density
- split rate
- merge or compaction rate
- right-sibling chase frequency
- range-scan page amplification
- equality probe candidate count
- MGA visibility reject rate
- dead-entry debt
- metrics freshness and confidence

## Alias-surface rule

Named surfaces such as `ART`, `NEO4J_RANGE`, or `REDIS_ZSET` may expose dialect-specific parser and option rules, but while they are routed through this family they inherit this MGA contract, this cleanup contract, and this metrics contract.
