# MySQL Insert Fast-Path Delta

Date: 2026-04-03

## Purpose

Explain why ordinary MySQL/InnoDB `INSERT` is fast, then classify the closest
ScratchBird equivalents as:

- `IMPLEMENTED`
- `IMPLEMENTED_WITH_BETA1_EXPANSION`
- `BETA1_SPEC_ONLY`
- `NOT_PROVED`

This audit is limited to local source and current approved ScratchBird Beta 1
specifications.

## High-Confidence Summary

The biggest MySQL/InnoDB insert wins are:

1. statement-level bulk setup when multi-row `INSERT` is safe
2. batched `AUTO_INCREMENT` reservation
3. reuse of a prebuilt row-conversion template and insert graph
4. change buffering for eligible non-unique secondary indexes
5. optimistic insert first, pessimistic path only on failure
6. approximate stats maintenance instead of heavier exact accounting

ScratchBird already has:

- direct multi-row `COPY` to SBLR `INSERT` generation
- relaxed approximate DML stats maintenance
- pending-list buffering for `GIN`
- buffered or append-oriented mutable lanes for some heavy families
- hot-right-edge exact B-tree mitigation
- exact bulk-build and shadow-index substrate

ScratchBird does not yet prove the two biggest MySQL-style retail exact-write
reductions in runtime code:

1. commit-group exact apply
2. cold-page exact-secondary delta buffering

Those are both approved Beta 1 canon in section `18`, but they are not proved
from the current `src/` tree in this pass.

## Delta Table

| Lever | MySQL evidence | ScratchBird live evidence | ScratchBird Beta 1 canon | Status | Assessment |
| --- | --- | --- | --- | --- | --- |
| Statement-level multi-row bulk setup | `ha_start_bulk_insert()` is invoked for safe multi-row `INSERT` in `sql_insert.cc:581-603`. | `CopyBytecodeGenerator::generateInsertBytecode()` emits one SBLR `INSERT` with multiple row lists in `engine_ipc_session_handler.cpp:2006-2062`. | `Retail micro-batch` is required in `BULK_INGEST_LANES_AND_SHADOW_LOAD_CUTOVER_MODEL.md:52-60`. | `IMPLEMENTED_WITH_BETA1_EXPANSION` | ScratchBird proves parser-side multi-row lowering today, but not a comparable exact-family commit-time coalescing path for ordinary retail inserts. |
| Batched identity allocation | Multi-row inserts reserve auto-inc ranges in `handler.cc:3954-4005`. | No equivalent bulk reservation path was proved in this pass. | ScratchBird's Beta 1 bulk path assumes row UUID identity assignment before index work in `BULK_INGEST_LANES_AND_SHADOW_LOAD_CUTOVER_MODEL.md:56-58`. | `NOT_PROVED` | This is less central for ScratchBird because row identity is UUID-first, but donor identity and sequence-heavy emulations may still want a batched sequence or identity allocator. |
| Reuse of insert conversion machinery | InnoDB reuses a prebuilt whole-row template and insert graph in `ha_innodb.cc:9362-9379` and `row0mysql.cc:1557-1582`. | Current proof shows direct executor routing from opcode to per-index runtime insert in `executor.cpp:117818-118040`, not a proved whole-row insert graph cache. | No matching Beta 1 DML canon was found in the current approved insert optimization specs. | `NOT_PROVED` | ScratchBird likely still pays more repeated foreground setup work than InnoDB for ordinary exact writes. |
| Deferred exact-secondary maintenance for cold pages | InnoDB change buffering is admitted only for eligible non-clustered, non-spatial, non-descending, non-unique secondaries in `ibuf0ibuf.ic:116-129`; search can complete without touching the target page in `row0ins.cc:2957-2960`; background merge runs in `srv0srv.cc:2347-2352`. | No exact-family equivalent was proved in `src/`. | `Narrow cold-page secondary delta buffer` is required in `DML_WRITE_PATH_AND_INDEX_OPTIMIZATION_MODEL.md:350-406`. | `BETA1_SPEC_ONLY` | This is the closest ScratchBird analogue to MySQL's biggest retail insert advantage, and it is still canon-only. |
| Deferred mutable lane for many-key indexes | InnoDB defers some secondary maintenance through the change buffer. | `GIN` inserts go to a pending list and auto-merge at threshold in `gin_index.h:44-45` and `gin_index.cpp:690-727`. | `Pending lanes for many-key families` are required in `DML_WRITE_PATH_AND_INDEX_OPTIMIZATION_MODEL.md:408-420`. | `IMPLEMENTED_WITH_BETA1_EXPANSION` | ScratchBird already has the pattern for many-key families, but not yet for exact secondaries where MySQL gets the biggest OLTP insert gain. |
| Optimistic insert first, heavier path only on failure | InnoDB uses `btr_cur_optimistic_insert()` before pessimistic fallback in `row0ins.cc:3070-3089`. | ScratchBird B-tree inserts try direct leaf insert, compact garbage if present, and only then split and retry in `btree.cpp:1479-1760`. | Hot-leaf and batch-apply extensions are required in `DML_WRITE_PATH_AND_INDEX_OPTIMIZATION_MODEL.md:612-644`. | `IMPLEMENTED_WITH_BETA1_EXPANSION` | ScratchBird already has a fast-first exact insert path, though its future batch and deferred-lane canon should reduce more foreground work. |
| Approximate stats instead of exact protected counters | InnoDB explicitly prefers approximate `stat_n_rows` without extra locking in `row0mysql.cc:1684-1688`. | ScratchBird uses relaxed atomic stats updates and relaxed live/dead row estimates in `table_stats_manager.cpp:112-148`. | No extra Beta 1 work is needed for this specific pattern. | `IMPLEMENTED` | ScratchBird already matches the low-overhead stats-maintenance pattern. |
| Hot-right-edge monotonic insert mitigation | MySQL relies on optimistic insert and general B-tree behavior; no similarly explicit right-edge mitigation rule was pulled in this pass. | ScratchBird has explicit rightmost-leaf hinting, reserved free-space logic, and presplit behavior in `btree.cpp:1197-1208` and `btree.cpp:1571-1596`. | `Hot-leaf and monotonic-key mitigation` is required in `DML_WRITE_PATH_AND_INDEX_OPTIMIZATION_MODEL.md:612-644`. | `IMPLEMENTED_WITH_BETA1_EXPANSION` | ScratchBird is already stronger and more explicit here than the MySQL evidence reviewed in this pass. |
| Exact bulk build path | MySQL SQL layer starts bulk mode, and non-InnoDB engines such as MyISAM have specialized bulk paths; InnoDB ordinary insert still remains row-oriented after setup. | ScratchBird has `BTree::bulkLoad()` with sorted bottom-up exact build in `btree.cpp:4962-5068`, plus shadow-index creation and promotion in `catalog_manager.cpp:33039-33213`. | `Sorted exact bulk` and `shadow-load and cutover` are required in `BULK_INGEST_LANES_AND_SHADOW_LOAD_CUTOVER_MODEL.md:61-99`. | `IMPLEMENTED_WITH_BETA1_EXPANSION` | ScratchBird has the main substrate for large sorted loads and online rebuilds, but this does not yet prove equivalent retail insert acceleration. |
| Commit-time coalescing of exact-family deltas | No exact MySQL equivalent by that name; MySQL's foreground win comes from cheaper per-row setup plus deferred eligible secondaries. | No `commit_group` runtime path was found in current `src/`. | `Commit-group batch apply` is required in `DML_WRITE_PATH_AND_INDEX_OPTIMIZATION_MODEL.md:252-297`. | `BETA1_SPEC_ONLY` | This is the main ScratchBird planned mechanism that should reduce exact-family foreground overhead below the current direct-per-transaction path. |
| Same-key exact update suppression | Not part of the MySQL insert path reviewed here. | Current proof only shows stats fields such as `rows_hot_updated` and `rows_newpage_updated` in `table_stats_manager.cpp:120-147`; no shipped exact-head redirect or anchor runtime was proved in this pass. | `Same-key exact update suppression` is required in `DML_WRITE_PATH_AND_INDEX_OPTIMIZATION_MODEL.md:155-239`. | `BETA1_SPEC_ONLY` | This is an important ScratchBird-side write reduction, but it is about updates rather than pure insert throughput. |

## Most Important Gap

The most important current difference is simple:

1. InnoDB can avoid foreground reads and writes for some cold non-unique
   secondary index targets.
2. ScratchBird currently proves direct exact runtime insert calls for ordinary
   exact-family maintenance.
3. ScratchBird's approved answer exists, but remains spec-only: commit-group
   exact apply plus narrow cold-page exact-secondary delta buffering.

The direct exact-routing proof is in `executor.cpp:117818-118040`, while the
planned Beta 1 answer is in section `18` at
`DML_WRITE_PATH_AND_INDEX_OPTIMIZATION_MODEL.md:252-406`.

## Recommended Implementation Order

To close the gap against MySQL's fast insert path while staying aligned with
ScratchBird MGA and current canon:

1. implement `commit-group batch apply`
2. implement `cold-page exact-secondary delta buffering`
3. wire `retail micro-batch` through the same exact-family machinery
4. implement `same-key exact update suppression`
5. keep existing hot-right-edge mitigation and extend its metrics to the Beta 1
   observability contract

## Boundaries

- This pass did not fully audit ScratchBird's sequence allocation internals.
- This pass focused on ordinary InnoDB insert behavior, not every MySQL storage
  engine.
- This pass found no live `commit_group`, `cold_delta`, or exact-head redirect
  implementation in current ScratchBird source.
