# ScratchBird MGA Index Hardening Report

Date: `2026-03-10`

Status: Draft engineering report based on repository inspection, canonical specification review, and local donor-engine clone analysis.

Audience: ScratchBird storage-engine, transaction, catalog, optimizer, and maintenance-framework engineers preparing the next wave of detailed index specifications after B-tree and platform hardening.

## 1. Executive Summary

ScratchBird already contains a wide index surface area. The repository is not a placeholder with one experimental B-tree and a few empty headers. It contains concrete implementations for B-tree, hash, GIN, GiST, SP-GiST, BRIN, bitmap, R-tree, HNSW, inverted/full-text, columnstore-attached indexing, and LSM-style structures. The catalog layer also exposes a much larger logical index taxonomy than the runtime actually implements directly, and the optimizer, storage engine, garbage collector, and catalog versioning layers already know enough about indexes to make the subsystem strategically important rather than optional. The problem is not lack of features. The problem is that the hardening level is uneven, with a substantial gap between the current code and the canonical architecture that the newer specification bundle now describes.

The repository currently shows three distinct evidence streams that must not be conflated. The first stream is live implementation in `src/core`, `include/scratchbird/core`, `src/catalog`, and `src/optimizer`; this is the only source that describes what the engine can do today. The second stream is older reverse-engineered or implementation-adjacent documentation under `docs/documentation/developers_guide/specifications/indexes` and related paths; these files are useful for historical intent but cannot be treated as authoritative because their layout and protocol descriptions diverge from both current code and the canonical hardening bundle. The third stream is the canonical specification set under `docs/specifications/18_Index_Framework`, including the B-tree hardening bundle added after the first B-tree implementation. Those canonical files define the target architecture, but they also contain internal precedence rules because the baseline `BTREE_SPEC.md` is intentionally overridden in many protocol areas by more specific companion documents.

The most important conclusion from repository analysis is that ScratchBird already has the beginnings of an index platform, but not yet a hardened one. Shared infrastructure exists in the form of catalog metadata, index family registration, storage-engine DML dispatch, MGA-oriented dead-entry cleanup hooks, generic page diagnostics, and optimizer path types. However, the current implementations still mix early-page-structure decisions, page-lock-based concurrency, in-place mutation assumptions, and incomplete rebuild publication rules. The B-tree implementation is the clearest example. It stores per-entry `xmin` and `xmax`, supports logical deletion and GC compaction, and can split, merge, bulk load, and rebuild Bloom filters. At the same time it lacks the authoritative metapage, split intent persistence, restart repair model, fence/high-key contract, split-tolerant descent, quarantine-based reclamation, and validation/publication state machine required by the canonical bundle. The code is promising, but it is not yet production-safe for heavy MGA churn, crash windows during structural change, or high-contention mixed read/write workloads.

The second major conclusion is that ScratchBird’s catalog model is already more mature than the underlying access methods. `CatalogManager::IndexInfo` includes physical and logical identity, rebuild lifecycle state, visible-version windows, logical versus physical index IDs, expression/predicate metadata, tablespace placement, and enough state to support shadow builds and versioned cutover. Shadow rebuild tests also exist. This means the platform can support mature rebuild semantics, but individual index families have not been brought up to the same level of rigor. The catalog is ahead of the access methods. That asymmetry is useful because it provides a control plane for hardening, but it also creates correctness risk when runtime components continue to use simplified assumptions such as “active indexes only” or “root page stored only in catalog metadata.”

The third major conclusion is that ScratchBird should not copy one donor system wholesale. The right pattern is hybrid. Mutable exact tree families should borrow structural discipline and validation culture from PostgreSQL, Firebird, and InnoDB while still respecting ScratchBird’s anti-WAL Alpha direction. Storage-attached or write-optimized families should borrow atomic cutover, component validation, and delayed-reclaim discipline from Cassandra SAI, ClickHouse, DuckDB, Milvus, and OpenSearch without importing their segment-refresh semantics wholesale. Vector families should borrow strict configuration validation, prepared-in-background/commit-at-publication mutation patterns, and repair tooling from Milvus, Neo4j, Redis vector sets, and OpenSearch. MongoDB’s online index build pattern is especially valuable for shadow-build publication barriers even though ScratchBird should not copy MongoDB’s replication-side-write machinery directly.

The maturity goal for ScratchBird is therefore not “implement more index types.” The maturity goal is to make every supported family obey one canonical MGA index contract:

- candidate generation is always separate from row-version visibility truth
- structural changes are durably publishable and restart-repairable
- old physical structures are never reclaimed before the catalog and the reader horizon make that safe
- validation and corruption detection are first-class runtime surfaces rather than last-resort debugging tools
- rebuild and cutover are explicit state machines
- observability is rich enough that operators can diagnose churn, bloat, split storms, duplicate pressure, ANN staleness, and pending reclaim without raw page inspection

This report therefore recommends a platform-first hardening program. B-tree remains the first family to harden because it is the donor for many generic rules and already has a canonical hardening bundle. But the output should not be a one-off B-tree rewrite. The output should be a hardened MGA index platform with one exact-tree gold standard, one rebuild publication framework, one validation/repair framework, one observability vocabulary, and family-specific rules layered on top.

## 2. Scope, Method, and Evidence Hierarchy

### 2.1 Scope

This report covers:

- ScratchBird repository structure as it exists in the live repository
- implemented index access methods and adjacent shared infrastructure
- catalog metadata and lifecycle representation for indexes
- current MGA interaction between indexes, heap visibility, transaction state, and GC
- canonical specification targets in the section 18 index framework bundle
- donor-engine patterns from Firebird, PostgreSQL, MySQL/InnoDB, MariaDB/InnoDB, Cassandra, ClickHouse, DuckDB, Milvus, OpenSearch, MongoDB, Neo4j, and Redis
- hardening requirements for exact, summary, approximate, storage-attached, and write-optimized index families

This report does not treat generic WAL adoption as a prerequisite because the canonical hardening material explicitly rejects that for Alpha as recovery truth. Where donor systems rely on WAL, redo, translog, oplog, or Lucene refresh, the report extracts the design pattern rather than the literal mechanism.

### 2.2 Evidence Hierarchy

The correct precedence for engineering decisions is:

1. Canonical specification bundle under `docs/specifications/18_Index_Framework`.
2. Live implementation under `ScratchBird/include`, `ScratchBird/src`, and the active tests.
3. Reverse-engineered or older implementation-adjacent docs under `ScratchBird/docs/documentation/developers_guide/specifications`.
4. Donor-engine patterns from local clones under `~/CliWork`.

That ordering matters because the B-tree baseline spec and older in-repo docs are not fully consistent with the newer hardening bundle. The workplan dated `2026-03-10` already states that `BTREE_SPEC.md` is the baseline page-layout and algorithm contract only, and that the companion hardening documents override it wherever protocol detail is more specific. This report adopts that rule throughout.

### 2.3 Repository Paths Examined

Primary ScratchBird implementation paths:

- `include/scratchbird/core`
- `include/scratchbird/optimizer`
- `src/core`
- `src/catalog`
- `src/index`
- `src/optimizer`
- `src/sblr`
- `tests/unit`
- `tests/integration`
- `tests/stress`

Canonical specification paths:

- `docs/specifications/18_Index_Framework`
- `docs/specifications/work/planning`

Older in-repo documentation paths:

- `docs/documentation/developers_guide/specifications/indexes`
- `docs/documentation/developers_guide/specifications/catalog`

Donor clones examined locally:

- `firebird`
- `postgresql`
- `mysql-server`
- `server` (MariaDB)
- `cassandra`
- `ClickHouse`
- `duckdb`
- `milvus`
- `OpenSearch`
- `mongo`
- `neo4j`
- `redis`

## 3. Repository Discovery and Current Platform Structure

### 3.1 Top-Level Repository Layout

ScratchBird is organized like a full database engine rather than a narrow library. The top-level directories of architectural interest are:

- `include/scratchbird`: public and internal C++ headers for engine subsystems
- `src/core`: storage engine, page structures, transaction manager, buffer pool, lock manager, GC, and most index implementations
- `src/catalog`: catalog-adjacent indexing and metadata overlays
- `src/index`: auxiliary index helpers such as bitmap RLE, enhanced columnstore helpers, and vector quantization support
- `src/optimizer`: semantic analysis, planner, path, and cost code
- `src/sblr`: bytecode and execution support
- `src/server`, `src/network`, `src/ipc`, `src/protocol`: server/runtime plumbing
- `src/spatial`, `src/geo`: spatial support logic adjacent to index families
- `tests`: unit, integration, stress, fuzz, compatibility, and conformance suites
- `docs`: mixed historical docs, reverse-engineered implementation notes, audit material, and old specifications

The key practical point is that indexes are not isolated in one clean subtree. They are distributed across `src/core`, `src/index`, `src/catalog`, `src/optimizer`, and `tests`, with control-plane responsibilities split between the catalog, storage engine, transaction manager, garbage collector, and planner.

### 3.2 Major Engine Subsystems Identified

The repository contains clear implementations or partial implementations of the major engine subsystems requested in the brief.

Core storage engine:

- Primary files: `include/scratchbird/core/storage_engine.h`, `src/core/storage_engine.cpp`
- Responsibilities: heap DML, tuple visibility mediation, index maintenance dispatch, scan creation, TOAST-aware key extraction integration

Buffer pool:

- Primary files: `include/scratchbird/core/buffer_pool.h`, `src/core/buffer_pool.cpp`
- Responsibilities: global page pin/unpin, access strategy hints, page residency mediation between storage structures and IO

Transaction manager:

- Primary files: `include/scratchbird/core/transaction_manager.h`, `src/core/transaction_manager.cpp`
- Responsibilities: MGA visibility checks, current transaction identity, oldest-XID horizon tracking, snapshot capture helpers, TIP-related state

Page manager:

- Primary files: `include/scratchbird/core/page_manager.h`, `src/core/page_manager.cpp`
- Responsibilities: page allocation/freeing, tablespace-aware GPID handling, low-level persistent page lifecycle

Lock manager:

- Primary files: `include/scratchbird/core/lock_manager.h`, `src/core/lock_manager.cpp`
- Responsibilities: page/table/maintenance lock modes, blocking coordination between concurrent operations

Catalog manager:

- Primary files: `include/scratchbird/core/catalog_manager.h`, `src/core/catalog_manager.cpp`
- Responsibilities: index metadata, rebuild versioning, logical/physical identity, dependency tracking, index object cache, logical index visibility selection

Index access methods:

- Primary implementation cluster: `src/core`
- Shared registration/dispatch: `include/scratchbird/core/index_factory.h`, `src/core/index_factory.cpp`
- Auxiliaries: `src/index`, `src/catalog/catalog_index.cpp`

Query planner and semantic analysis:

- Primary files: `include/scratchbird/optimizer/query_planner.h`, `src/optimizer/query_planner.cpp`, `src/optimizer/v3_semantic_analyzer.cpp`
- Responsibilities: index path enumeration, path types such as `INDEX_SCAN`, `INDEX_ONLY_SCAN`, `BITMAP_INDEX_SCAN`, and family-aware scan-kind selection

GC and compaction:

- Primary files: `src/core/garbage_collector.cpp`, `gc_manager.*`, `garbage_collector.*`, `sweep_manager.*`, `tip_compaction.*`
- Responsibilities: heap-first dead tuple discovery, index dead-entry cleanup callbacks, sweep coordination, partial horizon advancement

Observability:

- Primary files: `telemetry.*`, `observability_contract.*`, `query_profiler.cpp`, `audit_logger.cpp`, plus index metrics catalog contracts
- Responsibilities: mixed runtime instrumentation, but not yet a complete per-index health and maintenance telemetry surface

### 3.3 Startup and Runtime Architecture

ScratchBird startup wiring in `src/core/database.cpp` shows a central `Database` object composing `PageManager`, `BufferPool`, `CatalogManager`, `StorageEngine`, `TransactionManager`, `LockManager`, and GC/sweep/statistics helpers. This matters for index hardening because it means indexes are not independent services. They are runtime participants that rely on the same buffer, page, lock, and transaction subsystems as the heap.

At execution time, the important path is:

1. session and connection context establish current process and transaction identity
2. semantic analysis resolves catalog objects and index metadata
3. planner produces scan paths and plan nodes
4. storage engine executes heap and index operations
5. transaction manager remains authoritative for MGA visibility
6. garbage collector later removes dead heap and index state

This architecture is already compatible with a platform-first hardening approach. The platform does not need a new service boundary. It needs stronger contracts between existing participants.

### 3.4 Current Structural Weaknesses Visible at the Repository Level

Several system-level weaknesses are visible before drilling into individual index families.

First, much of the sophisticated catalog and specification machinery exists beside early implementation code rather than underneath it. The repository can describe shadow indexes, retirement horizons, and family capabilities more richly than some runtime classes can actually honor.

Second, some index types in the catalog taxonomy are true implementations while others are aliases collapsed onto a smaller runtime class set. That is acceptable for Alpha surfacing, but it increases the importance of making the runtime classes truly hardened. If `ART`, `STL_SORT`, and several graph or Redis range styles collapse onto B-tree, then B-tree hardening becomes a platform dependency rather than one feature.

Third, validation exists, but it is fragmented. There is generic page diagnostic logic, some unit/runtime contracts, and some family-specific tests, but not yet one coherent index validator framework with lightweight online checks, deep offline validation, repair guidance, and cutover gating.

Fourth, rebuild semantics exist in the catalog and tests, but not yet as a uniform family contract. Shadow promotion is already supported at metadata level; physical families need a hardened build-state, publication, and reclaim discipline to match it.

## 4. Index Subsystem Discovery

### 4.1 Runtime Families and Primary Files

The live repository contains the following concrete runtime index families.

B-tree:

- headers: `include/scratchbird/core/btree.h`, `include/scratchbird/core/btree_page.h`
- implementation: `src/core/btree.cpp`, `src/core/btree_iterator.cpp`, `src/core/btree_page.cpp`
- core types: `BTree`, `BTreeIterator`, `SBBTreePage`, `SBBTreeNode`, `SBBTreeIndex`

Hash:

- headers: `include/scratchbird/core/hash_index.h`
- implementation: `src/core/hash_index.cpp`
- core types: `HashIndex`, `SBHashIndexMetaPage`, `SBHashDirectoryPage`, `SBHashBucketPage`, `HashEntry`

GIN:

- headers: `include/scratchbird/core/gin_index.h`
- implementation: `src/core/gin_index.cpp`
- related helpers: `gin_compression.*`, `gin_tsvector_ops.*`, `src/sblr/gin_extractors.*`

GiST:

- headers: `include/scratchbird/core/gist_index.h`, `gist_box_ops.h`
- implementation: `src/core/gist_index.cpp`

SP-GiST:

- headers: `include/scratchbird/core/spgist_index.h`, `spgist_quad_ops.h`, `spgist_text_ops.h`
- implementation: `src/core/spgist_index.cpp`

BRIN:

- headers: `include/scratchbird/core/brin_index.h`, `brin_minmax_ops.h`
- implementation: `src/core/brin_index.cpp`

Bitmap:

- headers: `include/scratchbird/core/bitmap_index.h`, `include/scratchbird/index/bitmap_rle.h`
- implementation: `src/core/bitmap_index.cpp`

R-tree:

- headers: `include/scratchbird/core/rtree.h`, `rtree_node.h`, `rtree_index.h`
- implementation: `src/core/rtree.cpp`, `src/core/rtree_index.cpp`

HNSW / vector:

- headers: `include/scratchbird/core/hnsw_index.h`, `include/scratchbird/core/vector.h`, `include/scratchbird/index/vector_quantization.h`
- implementation: `src/core/hnsw_index.cpp`

Columnstore / storage-attached indexing:

- headers: `include/scratchbird/core/columnstore.h`, `include/scratchbird/core/columnstore_index.h`, `include/scratchbird/index/columnstore_enhanced.h`
- implementation: `src/core/columnstore.cpp`

LSM:

- headers: `include/scratchbird/core/lsm_tree_index.h`, `include/scratchbird/core/lsm_tree.h`, `include/scratchbird/core/lsm_bloom_filter.h`
- implementation: `src/core/lsm_tree_index.cpp`, `src/core/lsm_tree_components.cpp`, older helper code in `src/core/lsm_tree.cpp`

Inverted / standalone text:

- headers: `include/scratchbird/core/inverted_index.h`
- implementation: `src/core/inverted_index.cpp`

Full-text wrapper:

- headers: `include/scratchbird/core/fulltext_index.h`
- implementation: `src/core/fulltext_index.cpp`
- note: runtime dispatch is inconsistent, discussed later

Global uniqueness helper:

- headers: `include/scratchbird/core/global_uniqueness_index.h`
- implementation: `src/core/global_uniqueness_index.cpp`
- note: in-memory support structure, not a general page-managed index AM

Catalog internal B-tree:

- headers: `include/scratchbird/catalog/catalog_index.h`
- implementation: `src/catalog/catalog_index.cpp`

### 4.2 Page Formats Observed

The implemented families already use family-specific page or storage formats rather than one universal page record layout.

B-tree page format:

- `SBBTreePage` contains `PageHeader`, index/table UUIDs, level, flags, count, free-space accounting, left/right sibling, parent page, rightmost child, compression metadata, page-level `xmin/xmax`, `btr_lsn`, and high-water mark
- `SBBTreeNode` contains node flags, prefix length, suffix truncation, key length, tuple count, child page, `btn_xmin`, `btn_xmax`, and variable payload

Hash page format:

- metapage, directory page, bucket page, and entry structs exist
- entries carry hash, TID, and MGA fields

GIN page format:

- pending-list pages, posting list pages, entry trees, and posting trees are explicit
- pending and posting entries carry versioning state

GiST, SP-GiST, BRIN, bitmap, R-tree, HNSW, and columnstore:

- each family defines its own opaque page or segment headers under `include/scratchbird/core`
- MGA fields or soft-delete state are present in many of them, but the exact semantics vary by family

LSM:

- not page-buffer based in the same way as B-tree/hash/GIN
- uses memtable, SSTable writer/reader, and Bloom components

This diversity is normal, but it has an architectural consequence: the platform must define common behavioral contracts above different physical layouts, not force one binary layout across all families.

### 4.3 Mutation Paths

Mutation dispatch is centered in `src/core/storage_engine.cpp`. The storage engine resolves the index type and routes insert or delete/update work into family-specific methods. This is a strong sign that the engine already treats index maintenance as part of DML correctness rather than a separate utility job.

Examples:

- B-tree-like aliases call `BTree::insert` and `BTree::remove`
- hash-like aliases call `HashIndex::insert` and hash delete paths
- GIN/GiST/BRIN/SP-GiST/R-tree/HNSW/inverted/columnstore each have their own insert dispatch
- vector aliases such as IVF variants currently collapse to `HnswIndex`
- BRIN and zonemap-like variants collapse to `BrinIndex`
- text-like or trie-like aliases collapse to `InvertedIndex`

This design is useful but dangerous if aliasing outruns runtime maturity. When many logical types collapse onto one runtime class, the hardening envelope of that class becomes the envelope of all aliases.

### 4.4 GC Integration Paths

`IndexGCInterface` is the shared contract between heap GC and indexes. The garbage collector cleans heap pages first, gathers dead TIDs, then resolves open index objects and calls `removeDeadEntries(dead_tids)` on the relevant index runtime instance. This is a Firebird-like heap-first cleanup discipline rather than a PostgreSQL-style “vacuum the index while consulting heap visibility repeatedly” model.

This is a good platform choice for ScratchBird’s MGA direction, but it imposes strict requirements:

- dead TIDs emitted by heap/sweep must be precise
- index cleanup must remove only those entries proven dead or fall back to rebuild/resummarize semantics where precision is impossible
- approximate or summary families must not over-delete to compensate for weak precision

The current code already contains that hook across B-tree, hash, GIN, GiST, SP-GiST, BRIN, bitmap, R-tree, HNSW, inverted, columnstore, and LSM, but the per-family reclamation sophistication varies significantly.

### 4.5 Rebuild and Cutover Mechanisms

The catalog layer already supports shadow rebuild at metadata level through:

- `createShadowIndex`
- `promoteShadowIndex`
- `gcRetiredIndexes`
- `getVisibleIndexVersion`

The integration tests include `test_shadow_index_rebuild.cpp`, indicating this is not merely speculative. However, per-family physical rebuild discipline is uneven. The current B-tree bulk-load implementation can build a tree bottom-up, but it does not yet implement the canonical build-state markers, metapage-root publication, or restart repair semantics required by the hardening bundle.

## 5. Catalog Layer and Index Metadata Model

### 5.1 Current Catalog Representation

`CatalogManager::IndexInfo` is the central metadata contract for indexes. It already includes:

- physical identity: `index_id`
- owning table: `table_id`
- persistent location: `root_gpid`, `tablespace_id`, `tablespace_uuid`
- logical type: `index_type`
- structural options: uniqueness, indexed columns, include columns, collation
- expression/predicate metadata: expression and predicate TOAST OIDs, expression strings, serialized data
- dependency ID for lifecycle cleanup
- logical identity across rebuilds: `logical_index_id`
- lifecycle state: `BUILDING`, `ACTIVE`, `RETIRED`, `FAILED`, `INACTIVE`
- version windows: `valid_from_xid`, `retired_xid`
- build timing: `build_started_time`, `build_completed_time`

This is an unusually strong metadata baseline for an engine whose physical access methods are still maturing. It means the platform can already distinguish “same logical index, different physical generation,” which is exactly what a hardened rebuild/cutover system requires.

### 5.2 Logical Versus Physical Identity

The presence of both `index_id` and `logical_index_id` is critical. The physical ID identifies one concrete index instance. The logical ID binds together generations of the same logical index definition across rebuilds. That model is correct for MGA-safe cutover because:

- active readers may continue using an older physical generation
- new transactions may start using a new generation after publication
- GC can retire old physical generations only after the horizon makes them unreachable

This is stronger than a simple “rename root page” model. It provides an explicit place to encode state transitions and reader-horizon gating.

### 5.3 Index States and Lifecycle Fields

The current code uses a minimal state enum:

- `BUILDING`
- `ACTIVE`
- `RETIRED`
- `FAILED`
- `INACTIVE`

This is sufficient for current shadow rebuild wiring, but it is weaker than the canonical spec vocabulary, which also refers to `validating`, `invalid`, and `dropping` in the shared index architecture docs, and to detailed build-phase markers for B-tree. The implication is not that the current catalog is wrong. The implication is that the catalog needs a layered state model:

- cross-family logical states visible to DDL, planner, and operators
- family-specific physical build or repair substates stored in auxiliary metadata or metapage state

### 5.4 Catalog Visibility Semantics

The catalog contains a correct versioned-view concept through `getVisibleIndexVersion(table_id, index_name, txn_xid, info_out)`. It selects the physical generation whose state and `valid_from_xid` / `retired_xid` window make it visible to the requesting transaction. This is exactly the right control-plane primitive for MGA-safe cutover.

However, repository inspection shows a policy gap: common lookup surfaces and planner-side enumeration primarily use `listIndexesForTable(..., false)`, which hides non-`ACTIVE` versions and does not appear to route normal planning through the version-window-aware accessor. That is acceptable for a simple engine with no overlapping generations, but it is not sufficient for production-grade MGA rebuild semantics. Once shadow generations exist concurrently, normal planning and execution must be explicit about which visibility surface they use.

### 5.5 Rebuild Metadata and Gaps

The catalog already captures enough metadata to support a hardened rebuild framework, but three gaps remain.

First, the catalog does not yet provide one canonical mapping from logical state to family-specific physical publication state. For B-tree, the canonical bundle requires metapage build states, SMO counts, publication sequence, verification epoch, and pending reclaim counts. Those are not represented as first-class B-tree persistent metadata today.

Second, the catalog assumes root location through `root_gpid`. That is necessary, but for hardened B-tree it is not sufficient. The canonical bundle requires the authoritative published root to live in the B-tree metapage, not only in catalog metadata or in-memory state.

Third, there is no platform-wide “maintenance evidence” record tying together validation results, build outcome, repaired-on-restart count, or corruption findings. The operator model needs that.

## 6. Shared Index Infrastructure

### 6.1 Index Factory and Runtime Class Registry

`IndexFactory` is the family registry. It maps the large catalog `IndexType` taxonomy to a smaller set of runtime classes and storage models. This is currently the shared compatibility layer for index creation, opening, closing, and capability discovery.

Important runtime classes:

- `BTREE`
- `HASH`
- `LSM`
- `GIN`
- `GIST`
- `BRIN`
- `RTREE`
- `SPGIST`
- `BITMAP`
- `COLUMNSTORE`
- `HNSW`
- `INVERTED`

Important alias examples:

- `ART`, `STL_SORT`, several Neo4j and Redis range-like styles route to `BTREE`
- `ZONEMAP` and `BLOOM` route to `BRIN`
- a broad vector taxonomy routes to `HNSW`
- `TRIE`, `NGRAM`, Cassandra SAI/SASI-style logical types, sparse inverted variants, and several text-like logical types route to `INVERTED`

This registry is strategically useful because it gives ScratchBird one place to declare what a family can do. It is also a hardening burden because capability statements must be enforced, validated, and surfaced honestly.

### 6.2 Storage Engine DML Dispatch

The storage engine contains the actual DML coupling to indexes. It already knows how to:

- compute keys
- insert into exact, summary, and approximate family instances
- remove or logically delete from those instances
- decode vector payloads for HNSW-like families
- handle columnstore and bitmap integration

This is where platform-level MGA hardening must ultimately land. If DML integration is wrong here, no downstream validator can save correctness.

### 6.3 GC Interface

`IndexGCInterface` is the core shared cleanup surface. It is simple by design, which is correct. The interface should remain small:

- remove dead entries
- report statistics
- expose type name

The hardening work should not explode the interface with family-specific special cases. Instead, the platform should add:

- richer metrics emitted by implementations
- optional validator and maintenance capability interfaces
- explicit rebuild recommendation or fallback reporting for summary/approximate families

### 6.4 Generic Page Diagnostics

`index_page_diagnostics.cpp` provides shared page validation functions that check:

- page header magic, size, and type
- checksum validity
- generic index page header shape
- sibling contract
- expected index UUID

This is valuable, but it also reveals a design mismatch. The current B-tree page format predates the canonical generic index opaque-header contract and stores a large custom header directly in the page body. Therefore the generic diagnostics layer is not yet the finished validation story for all families. It is a base layer that needs family adapters and validators above it.

### 6.5 Planner and Path Integration

The optimizer already exposes path and plan node types for:

- `INDEX_SCAN`
- `INDEX_ONLY_SCAN`
- `BITMAP_INDEX_SCAN`
- family-specific scan kinds such as `LSM_SCAN`

Semantic analysis loads table indexes through catalog lookup and uses relatively simple family heuristics such as “first indexed column matches” to choose candidate indexes. This confirms that index hardening is not only a storage concern. Planner correctness depends on accurate metadata about which index generations are valid, which families are exact or lossy, and which can support index-only semantics under MGA and security rules.

### 6.6 Shared Infrastructure Gaps

The repository analysis identified several platform-wide inconsistencies.

`FULLTEXT` inconsistency:

- wrapper class `FullTextIndex` exists
- `IndexFactory` maps `FULLTEXT` to runtime class `INVERTED`
- catalog opening paths have also treated `FULLTEXT` as `GinIndex` in at least one code path

This must be resolved before production hardening. A logical family cannot have multiple contradictory physical identities.

Visibility API inconsistency:

- current implementations often use TIP-style `isVersionVisible(xmin, current_xid)` checks and a current-XID calling convention
- canonical specs use the more general language of MGA visibility and sometimes discuss snapshot visibility

The correct resolution is not to abandon the existing transaction manager API. The correct resolution is to define one canonical index visibility service that can answer both current-transaction and explicit-snapshot questions without family-specific shortcuts.

Validation mismatch:

- some page-layout contracts are being standardized in tests and docs
- several live families still use pre-hardening internal layouts

The hardening effort must separate “what current pages look like” from “what hardened pages must look like,” and it must include rebuild or upgrade rules whenever binary layout compatibility is broken.

## 7. MGA Integration in the Current Repository

### 7.1 Heap Visibility is Already the Authority

ScratchBird’s heap model is MGA-oriented. `TupleHeader` stores `xmin`, `xmax`, back-version linkage, and row identity. `HeapPage::findVisibleVersion()` walks version chains and consults transaction state. `TransactionManager::isVersionVisible()` is the canonical visibility check in normal runtime. This is the right foundation.

The indexes in the repository generally follow the intended rule that index entries are candidates, not visibility truth. Many families store `xmin` and `xmax` in the index record as maintenance hints or soft-delete state, but actual user-visible correctness still depends on heap/version visibility.

### 7.2 MGA Fields in Index Entries

Observed families storing MGA-oriented fields:

- B-tree: page-level `btr_xmin/btr_xmax` and node-level `btn_xmin/btn_xmax`
- hash: `he_xmin/he_xmax`
- GIN: version fields in pending/posting structures
- BRIN: range-level `brn_xmin/brn_xmax`
- HNSW: node-level `node_xmin/node_xmax`
- columnstore: metadata/segment `cs_xmin/cs_xmax`
- global uniqueness helper: versioned value locations

This indicates the platform has already embraced version-aware index maintenance. That is an asset. The problem is not whether MGA belongs in the index subsystem. The problem is whether its lifecycle is precise, uniform, and safe under concurrency and crash.

### 7.3 Current Visibility Checks

B-tree explicitly routes entry visibility through `TransactionManager::isVersionVisible()`. Searches and range scans take a current transaction ID and treat `0` as a special case meaning “return all matches” for GC or internal use. Other families follow similar patterns, with some variance between current-XID and snapshot-style calling conventions.

This is directionally correct but still underspecified. Production-grade MGA behavior requires the platform to define:

- the visibility horizon used by normal user scans
- the horizon used by unique checks
- the horizon used by shadow build delta replay
- the horizon used by GC
- what “return all matches” means for approximate and lossy families

### 7.4 Dead Entry Lifecycle Today

The intended current lifecycle is:

1. DML creates new heap versions and inserts new index candidates.
2. Deletes or updates do not immediately remove old index candidates.
3. Many families set `xmax` or a deleted flag to mark logical death.
4. Garbage collection later discovers heap-dead TIDs.
5. GC calls `removeDeadEntries` on each index.
6. Families remove or compact dead candidates.

This is fundamentally the correct MGA shape. The weaknesses are in the details:

- exact families do not yet have a single precise deletion protocol
- summary families do not yet always resummarize from heap
- approximate families need bounded backlog and rebuild thresholds
- page-reclamation rules after dead-entry cleanup are not uniformly scan-safe

### 7.5 Rebuild and Snapshot Safety

The catalog has the notion of visible physical generations by transaction horizon. That is the right MGA idea for rebuild cutover. However, the family implementations do not yet all provide a snapshot-safe physical cutover discipline. B-tree, the first family with a detailed hardening spec bundle, still lacks the canonical metapage publication and restart-repair protocol. Until that exists, the control plane is ahead of the data plane.

## 8. B-tree Current Implementation Versus Canonical Hardening Target

### 8.1 What the Current B-tree Actually Implements

The current B-tree is not trivial. It already supports:

- single-root create and open
- leaf and internal pages
- sibling pointers and parent pointers
- rightmost child tracking in page header
- prefix compression with per-node prefix length
- node-level `xmin/xmax`
- logical deletion via `btn_xmax`
- GC compaction
- leaf and internal page splitting
- internal parent insertion
- new-root creation
- bottom-up bulk load
- page merging during GC compaction
- Bloom filter attachment and rebuild
- iterator/range scan support

That is enough functionality that hardening can proceed incrementally. The goal is not to replace an empty stub. The goal is to stabilize a real implementation.

### 8.2 Canonical B-tree Target from the Spec Bundle

The canonical bundle under section 18 requires a harder design:

- one authoritative metapage with `root_gpid`, height, counts, build state, publication sequence, active SMO count, and verification epoch
- bounded structural-modification intent records for split, merge, root change, and bulk publish
- restart repair driven by metapage plus page headers plus SMO intents
- fence/high-key/right-link semantics
- split-tolerant descent with explicit latch modes
- conservative, quarantine-based page deletion and merge
- duplicate and MGA churn accounting with compact-before-split decisions
- crash-safe bulk build and rebuild publication states
- operator-facing observability and gate evidence

### 8.3 Spec Contradictions and Resolution Order

The B-tree specification set contains real overlap and must be normalized before detailed implementation specs are derived.

The correct resolution order is:

1. `BTREE_SPEC.md` defines baseline layout and algorithm vocabulary.
2. More specific companion specs override it for protocol details.
3. The `2026-03-10` B-tree hardening workplan explicitly codifies those override rules.

Concretely:

- the baseline spec’s simpler split description is overridden by the structural durability protocol
- the baseline spec’s generic concurrency text is overridden by the split-tolerant descent spec
- the baseline spec’s simple bulk build outline is overridden by the bulk build and rebuild protocol
- the baseline spec’s deletion and rebalance text is overridden by the page deletion/merge/reclamation hardening docs
- the baseline spec’s simple compressed-page search description is overridden by the bounded-decode/restart-anchor design

This report therefore treats contradictions not as fatal, but as a documentation debt item that must be cleaned before downstream implementation specs are finalized.

### 8.4 Discrepancy Register: Current B-tree Versus Canonical Target

Authoritative metapage:

- current code: no B-tree metapage; open/create use `root_gpid` from catalog and `idx_root_page` in memory
- canonical target: authoritative metapage with publication sequence, counts, build state, and verification epoch
- hardening implication: root publication is not durably self-describing today; crash-safe rebuild and root-change semantics remain underspecified

Structural durability:

- current code: splits and root creation rely on ordered page updates and direct parent mutation; no bounded SMO intent record exists
- canonical target: split/merge/root-change phases with durable intent records and restart repair
- hardening implication: crash windows during structural modification are not yet proven repairable

Fence/high key:

- current code: sibling pointers exist, but no explicit high-key/fence-key contract is stored and enforced as canonical search truth
- canonical target: high key and right sibling define the authoritative rightward escape path
- hardening implication: readers cannot rely on split-tolerant right-link chase under concurrent structural change

Concurrency model:

- current code: page traversal uses lock-coupling comments and lock-manager page locks; explicit latch modes such as `SEARCH_SHARED` and `WRITE_INTENT` do not yet exist as a hardened contract
- canonical target: dedicated latch protocol with split-tolerant descent and maintenance-exclusive modes
- hardening implication: upper-level contention is likely higher than necessary, and correctness depends on stronger serialization than the target architecture intends

Search algorithm:

- current code: `searchPage` linearly reconstructs keys in slot order and compares them one by one
- canonical target: bounded compressed-page search using restart anchors or equivalent acceleration
- hardening implication: search cost on compressed pages grows poorly and does not yet match the specification

Separator rigor:

- current code: minimal separator helper exists, but the family lacks the full pivot/fence correctness framework from the canonical bundle
- canonical target: separator promotion and suffix truncation must preserve exact fence truth
- hardening implication: edge cases around truncation, duplicates, and compressed pivots need formalization and validator coverage

Duplicate handling:

- current code: leaf nodes can store multiple TIDs per key, but split logic does not yet implement the fully specified duplicate/posting-list management policy from the hardening bundle
- canonical target: duplicate-heavy workloads are first-class and must avoid pathological split storms
- hardening implication: duplicate pressure remains a known high-risk workload

Compact-before-split:

- current code: when a page is full, normal flow proceeds to split; GC compaction exists but is not integrated as a canonical pre-split decision engine
- canonical target: dead-version density and reclaimability score must drive compact-before-split
- hardening implication: MGA churn can produce unnecessary splits and bloat

Merge and reclamation:

- current code: `mergePages` updates siblings and parent, then frees the right page through `PageManager::freePageGlobal`
- canonical target: merge victim enters quarantine; allocator reuse is forbidden until reader horizon and parent unlink are safe
- hardening implication: current deletion/reclamation is too eager for hardened split/scan safety

Build and rebuild:

- current code: `bulkLoad` builds pages and sets a new root, then rebuilds Bloom filter
- canonical target: metapage build phases `INIT` through `COMPLETE`, validation before publication, restart handling
- hardening implication: interrupted build/cutover safety is not yet explicit

Validation:

- current code: there are unit tests and generic page diagnostics, but no dedicated B-tree validator implementing the hardening bundle’s structural proofs
- canonical target: tree validator, failpoint coverage, and operator-visible health surface
- hardening implication: corruption detection remains weaker than required for production diagnosis

Binary layout alignment:

- current code: `SBBTreePage` and `SBBTreeNode` are concrete layouts with 64-bit sibling/parent/child fields, custom header fields, and no separate metapage
- baseline spec: different explicit layout assumptions exist, including a high-key special area and jump-table fields
- canonical target: baseline layout plus companion overrides, with metapage and hardened publication semantics
- hardening implication: the current implementation cannot be considered layout-final; rebuild/format-version machinery is required

### 8.5 Immediate Conclusion on B-tree

The current B-tree should be treated as the prototype implementation that seeds the hardened design, not as the structure to bless with minor cleanup. The right path is a controlled hardening migration:

- preserve proven utility code where possible, especially key encoding, prefix compression primitives, iterator logic, and bottom-up loading ideas
- replace root publication, split publication, merge reclamation, search descent, and validation with the canonical hardened model
- formalize the binary compatibility story instead of assuming the current page layout is already final

## 9. ScratchBird Index Platform Architecture

### 9.1 Access Method Abstraction

ScratchBird does not currently expose a PostgreSQL-style pluggable `IndexAmRoutine` with one uniform vtable covering build, insert, scan, vacuum, validate, and cost hooks. Instead, the abstraction is split across four layers:

- `CatalogManager::IndexType` is the logical taxonomy
- `IndexFactory` maps logical types to runtime classes and storage models
- `StorageEngine` performs DML dispatch through type switches
- `IndexGCInterface` is the only truly shared virtual maintenance contract

This architecture is workable, but it is incomplete as a hardened platform abstraction. The missing elements are:

- a shared validator capability
- a shared rebuild/publication capability
- a shared metrics/health capability
- a shared exact-versus-lossy-versus-approximate capability declaration
- a shared unique-enforcement and conflict policy contract

The recommendation is not to force every family into a huge base class. The recommendation is to define a compact family capability descriptor and a few narrow optional interfaces. Exact families, summary families, and approximate families should be able to advertise different obligations without every call site open-coding large type switches forever.

### 9.2 Index Lifecycle in Current Code

The practical lifecycle today is:

1. catalog row creation with type, columns, tablespace, and root GPID
2. runtime `IndexFactory::createIndex()` or `openIndex()`
3. storage-engine DML maintenance through switch-based dispatch
4. optimizer/planner exposure through catalog enumeration
5. dead-entry cleanup through `IndexGCInterface`
6. optional shadow creation and promotion through catalog metadata
7. retirement and eventual physical drop after horizon advancement

The logical lifecycle is therefore already present. What is missing is the hardened physical lifecycle beneath it. The platform needs each family to surface:

- build state
- validation state
- publication state
- reclaim/quarantine state
- corruption/repair state

### 9.3 Mutation Flow

For a normal exact index update, the current repository implements approximately this flow:

1. storage engine creates or updates a heap version
2. `IndexKeyExtractor` derives key bytes, with TOAST detoasting when needed
3. storage engine enumerates relevant indexes for the table
4. family-specific insert or delete/mark-delete API is called
5. transaction manager remains the visibility authority
6. garbage collector eventually removes dead physical candidates

The critical observation is that ScratchBird already follows the correct MGA order: new row version first, index candidates second, cleanup later. This is a good foundation. The hardening work is about making the “cleanup later” phase and the “publication during concurrent change” phase precise enough for production.

### 9.4 Rebuild and Cutover Flow

The catalog model already describes the right high-level rebuild flow:

1. existing logical index identified
2. shadow physical generation created in `BUILDING`
3. shadow physically built
4. old generation marked `RETIRED`
5. shadow promoted `ACTIVE` with `valid_from_xid`
6. old generation eventually removed after OIT or equivalent horizon moves past `retired_xid`

That is the correct MGA shape. The current gap is that not every family has a hardened physical build-and-publish protocol underneath the metadata transitions. The B-tree spec bundle is the first family where the canonical physical protocol is already defined in detail.

### 9.5 Cleanup Flow

Cleanup today is heap-led:

1. sweep or garbage collection identifies dead heap tuples
2. dead TIDs are collected
3. each relevant index family is asked to remove or compact dead references
4. family-specific page or component cleanup occurs
5. retired physical generations are eventually dropped by catalog-driven GC

This is directionally closer to Firebird than to PostgreSQL. For ScratchBird MGA, that is the right default. However, summary and approximate families need explicit exceptions:

- BRIN-like structures should resummarize or rebuild rather than blindly delete summaries
- bitmap and posting-list families must delete precise postings, not broad key buckets
- approximate/vector families may soft-delete and rebuild later, but must surface backlog and threshold metrics

### 9.6 Interaction with the Optimizer

The optimizer is already index-aware, but the interaction is still immature in several ways:

- index enumeration primarily sees active catalog entries rather than full versioned visibility semantics
- family choice heuristics are relatively simple and often first-column based
- per-family costing is not deep enough to account for duplicate pressure, dead-version density, pending-list backlog, ANN recall degradation, or BRIN false-positive density
- lossy versus exact behavior is not yet elevated strongly enough into plan validation

This matters for hardening because an MGA-safe index is not enough if the optimizer over-trusts stale or lossy metadata. The mature platform must feed visibility, exactness, and maintenance-state information back into planning.

## 10. Cross-Database Comparison and Donor Patterns

### 10.1 Firebird

Firebird is the most conceptually relevant donor for ScratchBird’s MGA direction because index maintenance is tightly integrated with record-version lifecycle rather than delegated to a separate generic vacuum framework. Firebird’s index build and GC logic in `jrd/idx.cpp`, split handling in `jrd/btr.cpp`, and validation routines in `jrd/validation.cpp` show three patterns worth borrowing.

The first pattern is heap-led exact cleanup. Firebird treats the relation/version system as the truth and removes index state when version reachability allows it. ScratchBird should keep that principle. The second pattern is structural caution during split and propagation. Firebird carries explicit awareness that a page in the middle of structural change is not ordinary reclaimable garbage. ScratchBird’s B-tree hardening bundle mirrors this idea with SMO intents and quarantine. The third pattern is serious validation. Firebird’s validator walks the tree, checks sibling relationships, and correlates leaf references with relation-level reality. ScratchBird needs the same seriousness for every exact family.

What ScratchBird should not copy from Firebird is the degree of tight coupling between index internals and the rest of the engine. ScratchBird already has multiple index families and a broader donor-emulation strategy. It needs a more explicit platform abstraction than Firebird’s historically integrated codebase.

### 10.2 PostgreSQL

PostgreSQL is the strongest donor for access-method discipline. The AM contract, family-specific `vacuum` and `validate` hooks, `amcheck`, concurrent build machinery, and README-driven invariants provide a mature model for how to specify and gate index families. PostgreSQL’s B-tree, GIN, GiST, SP-GiST, hash, and BRIN code all benefit from living under an explicit AM contract rather than ad hoc switch logic.

ScratchBird should borrow:

- formal AM capability surfaces
- validator-first culture
- BRIN autosummarization thinking
- careful distinction between exact and lossy scan semantics
- build/cutover state machines with explicit validation before publication

ScratchBird should not copy:

- WAL as the only structural truth
- heapam/HOT-specific assumptions that do not map directly to ScratchBird’s MGA chain semantics
- PostgreSQL’s exact concurrency or lock implementation details where the underlying storage model differs

### 10.3 MySQL/InnoDB and MariaDB/InnoDB

InnoDB contributes two valuable ideas even though its redo and purge model should not be copied literally.

The first idea is online build/rebuild discipline. InnoDB and MariaDB both show that a hardened index subsystem cannot treat rebuild as “just bulk load another tree.” It needs change capture, a publication barrier, strong metadata validation, and a clear distinction between build-time and post-publication ownership. MariaDB’s explicit online-DDL/index-status metadata is especially instructive because it is conceptually easy to adapt to ScratchBird’s logical-versus-physical generation model.

The second idea is purge as a separate correctness obligation. InnoDB secondary indexes retain old state until purge proves it obsolete. ScratchBird’s heap-led GC already points in this direction, but the platform needs a stronger notion of backlog accounting and delayed physical reclaim. Approximate families in particular need a purge-like backlog surface.

ScratchBird should avoid copying redo/mini-transaction internals as architectural truth. The transferable value is disciplined publication and delayed reclaim, not the specific redo machinery.

### 10.4 Cassandra SAI

Cassandra SAI demonstrates that storage-attached indexing hardens best when the unit of publication is the immutable storage component, not the individual logical posting or summary entry. Build once per SSTable, validate the files, register the view, and quarantine incomplete or corrupt components. This is highly relevant to ScratchBird’s columnstore, inverted, and possibly future zonemap or storage-attached text families.

ScratchBird should borrow:

- per-component build and validation
- quarantine of incomplete outputs
- explicit registration/cutover after successful build
- checksum/header validation on index components

ScratchBird should avoid adopting Cassandra’s compaction worldview as its visibility truth. ScratchBird is MGA, so heap/version visibility remains authoritative even when storage-attached indexes are used.

### 10.5 ClickHouse

ClickHouse is valuable for skip indexes, zonemaps, and storage-attached summaries. The important pattern is “build a new part, validate it, then atomically publish it.” ClickHouse also demonstrates that min-max and text-supporting structures can be hardened without pretending they are exact row-version indexes.

ScratchBird should borrow:

- part- or segment-level cutover
- checksum validation
- explicit distinction between exact indexes and skipping aids
- summary rebuild rather than in-place mutation where appropriate

ScratchBird should not copy eventual visibility rules tied to MergeTree part activation semantics directly. ScratchBird still needs MGA-safe candidate filtering after any summary or skip stage.

### 10.6 DuckDB

DuckDB provides a useful hybrid donor. Its ART implementation gives a clean example of an exact in-memory-friendly tree family with explicit verification and checkpoint attach. Its row-group statistics and zonemap pruning show how summary metadata can cooperate with exact indexes and scans without trying to become full index families in the same sense as B-tree.

ScratchBird should borrow:

- verify-before-attach discipline
- explicit delta structures during checkpoint or maintenance windows
- strong row-group or segment statistic synergy between columnar storage and secondary access paths

ScratchBird should not copy DuckDB’s checkpoint/WAL durability model literally. The transferable design is validation before publication and clean separation between mutable deltas and published structures.

### 10.7 Milvus

Milvus is the strongest donor for ANN publication discipline. It separates mutable growing data from sealed/indexed segments, tracks delete deltas, allows search to continue without an index when necessary, and delays GC until replacement structures are built and old references are gone.

ScratchBird should borrow:

- explicit mutable-versus-published ANN states
- delete delta replay and stale-delta rejection
- rebuild/compaction GC that waits for replacement readiness and reader-horizon clearance
- file existence and load-time validation for ANN artifacts

ScratchBird should not copy Milvus’s segment lifecycle as-is. ScratchBird’s page and MGA control plane differs. The key transfer is delayed reclaim and publication barriers for approximate indexes.

### 10.8 OpenSearch

OpenSearch contributes two areas of value. The first is corruption handling. Lucene-derived systems take validation, checksums, and repair/exorcise tooling seriously. The second is strict input normalization and parser validation for geo and text structures.

ScratchBird should borrow:

- deep validation tooling comparable in spirit to `CheckIndex`
- clear corruption surfacing and operator repair options
- geometry and text normalization before index insertion

ScratchBird should avoid:

- refresh-gated visibility as a user-visible semantic model
- translog reader assumptions for vector or text visibility

### 10.9 MongoDB

MongoDB’s strongest contribution is its online build barrier pattern. A shadow structure is bulk-built, concurrent writes are captured separately, side writes are drained repeatedly, and only the final drain and commit run under stronger locking. That pattern maps well to ScratchBird’s shadow index model even if the exact mechanism does not.

ScratchBird should borrow:

- multi-phase shadow build
- repeated delta draining before final publication
- count reconciliation between recorded and applied side writes
- user-facing validation modes

ScratchBird should not copy:

- oplog-dependent mechanics
- storage-engine-specific side-write plumbing that does not fit ScratchBird’s architecture

### 10.10 Neo4j

Neo4j is most useful in vector and spatial-adjacent areas. It validates index configuration aggressively, versions vector semantics, coalesces concurrent population work, and runs consistency checking as a first-class operator experience. It also contains useful spatial decomposition ideas through space-filling curves.

ScratchBird should borrow:

- strict vector parameter validation
- versioned configuration semantics
- work-synced or coalesced population pipelines
- consistency checks and index statistics stores
- spatial decomposition helpers for range routing or coarse filtering

ScratchBird should avoid importing Lucene-centered eventual-consistency queue assumptions without adaptation.

### 10.11 Redis

Redis contributes a compact but important design pattern for ANN mutation: prepare heavy work in the background, then commit in the foreground with revalidation so a stale prepared result cannot win a race incorrectly. Redis vector-set code also takes serialization versioning, dimension checks, graph validation, and digest surfaces seriously.

ScratchBird should borrow:

- prepared-in-background, publish-after-revalidation pattern for ANN updates
- serialized format version fields and loader rejection of unknown versions
- connectivity or structural checks for graph-based ANN formats

ScratchBird should not copy:

- object-lock lifecycle assumptions tied to Redis in-memory object model
- module persistence APIs as a substitute for page/segment durability rules

### 10.12 Donor Synthesis

Across all donor systems, the recurring hardening themes are:

- separate mutable ingestion from published read-optimized state
- publish readiness explicitly
- never reclaim old state until readers are unquestionably past it
- treat validation as a product feature, not a debugging afterthought
- model cleanup backlogs and maintenance debt as operator-visible facts

For ScratchBird, the adaptation is:

- exact mutable tree families use page-level publication and SMO repair
- storage-attached families use component-level publication and delayed reclaim
- approximate families use candidate generation only, with MGA heap filtering always preserved

## 11. Cross-Index MGA Hardening Requirements

Every ScratchBird index family must obey the following platform-level rules after hardening.

### 11.1 Candidate Semantics

All index results are candidates until MGA visibility is confirmed. No family may treat internal `xmin/xmax`, tombstone state, posting-list flags, summary state, or ANN graph membership as a replacement for heap/version truth. Family-local metadata may accelerate rejection, but it may not become authoritative visibility.

### 11.2 Publication Barriers

Every family must have an explicit publication unit and a reader-horizon rule.

- exact page-managed trees publish through metapage or equivalent root indirection
- storage-attached summaries publish through part/segment registration
- approximate structures publish through generation pointers plus validation metadata

No writer may return success for a structural change, rebuild, or replacement generation until the relevant publication unit is durable and the catalog/control plane reflects it.

### 11.3 Delayed Reclaim

No page, component, or ANN graph generation may become reusable immediately after replacement or unlink. Every family must use either:

- reclaim quarantine tied to a reader horizon
- immutable component retirement tied to generation visibility windows

Direct allocator reuse is forbidden unless the family can prove the old object is unreachable by all readers and validators.

### 11.4 Build-State and Validation Discipline

Every family must expose:

- build state
- validation outcome
- last successful verification epoch or equivalent
- corruption status

Validation is mandatory before publication of a rebuilt generation. Families that cannot self-validate structurally must define a bounded rebuild recommendation path and a diagnostic reason.

### 11.5 Exact, Summary, and Approximate Cleanup Obligations

Exact families:

- must delete only the dead entries or postings proven unreachable

Summary families:

- must resummarize from authoritative storage state or schedule bounded rebuild

Approximate families:

- may soft-delete first, but must surface backlog metrics, rebuild thresholds, and maximum tolerated stale-delete debt

### 11.6 Unique Enforcement

If a family is advertised as unique-capable, uniqueness conflict detection must be MGA-aware and horizon-aware. Aborted or dead versions must not block progress. Active conflicting versions must follow one canonical wait or restart policy. Key-level conflict serialization must be platform-defined, not family-invented.

### 11.7 Metrics and Observability

Every family must emit, at minimum:

- candidate discard count after visibility checks
- dead-entry backlog
- pages or components pending reclaim
- validation failures
- rebuild fallback count
- contention measures appropriate to its concurrency model

Families may add richer metrics, but they may not omit this minimum surface.

### 11.8 Format Versioning

Any incompatible page or component layout change must:

- increment a family-specific format version
- be detectable at open time
- require rebuild or explicit upgrade tooling

Silent interpretation of old bytes under new rules is forbidden.

## 12. Current Maturity Gaps by Hardening Category

### 12.1 Structural Durability

Current repository status:

- no platform-wide restart-repair contract for multi-page structural operations
- B-tree lacks the authoritative metapage and SMO intent system required by the canonical bundle
- several families perform in-place changes without a clear durable publication state machine

Required hardening direction:

- exact families need bounded structural intent or equivalent page-dependency protocol
- storage-attached families need component manifest or publication records
- approximate families need generation metadata and reader-horizon retirement rules

### 12.2 Concurrency Model

Current repository status:

- B-tree comments describe lock coupling using the lock manager rather than a hardened latch protocol
- page-level versus transaction-level synchronization is not cleanly separated
- scan stability during concurrent split, merge, or deletion is not consistently specified across families

Required hardening direction:

- define lightweight latch protocol separate from lock-manager semantics
- preserve maintenance and DDL locks at a higher layer
- formalize scan-stability guarantees for each family

### 12.3 GC Integration

Current repository status:

- platform has the right heap-first cleanup hook
- per-family precision varies
- page reclamation after cleanup is too eager in B-tree
- summary and approximate families need clearer “rebuild instead of mutate” fallback paths

Required hardening direction:

- make heap dead-TID emission precise and reason-tagged
- make exact families delete exact dead state only
- define bounded maintenance debt and rebuild thresholds for non-exact families

### 12.4 Rebuild Safety

Current repository status:

- catalog already supports shadow generations and retirement horizons
- physical families do not consistently expose durable build states or validation-before-publication

Required hardening direction:

- make build and publication state machines family requirements
- route planner/executor through version-aware visibility selection
- tie retired generation cleanup to reader horizon plus validation evidence

### 12.5 Visibility Correctness

Current repository status:

- many families carry `xmin/xmax` and consult transaction visibility
- the platform still mixes current-XID shortcuts, GC “return all” special cases, and incomplete snapshot abstraction
- older `LSMTree` helper code still appears to contain raw `xmin <= xid < xmax` style logic outside the canonical transaction manager

Required hardening direction:

- one canonical visibility API for index families
- no raw family-local visibility math
- explicit rules for shadow-build replay, unique enforcement, and approximate-index candidate filtering

### 12.6 Page-Level Corruption Detection

Current repository status:

- generic index page diagnostics exist
- contract tests exist for page-type and sibling invariants
- family-level structural validators are incomplete

Required hardening direction:

- page checksums plus family validators plus offline deep verification
- corruption states surfaced through catalog and operator metrics
- repair versus rebuild decision framework

### 12.7 Duplicate Entry Handling

Current repository status:

- B-tree and posting-list families store duplicates but do not yet implement full duplicate-pressure policies
- duplicate-heavy workloads are not yet clearly treated as first-class maintenance pressure

Required hardening direction:

- duplicate/posting-list management with compression, spill, and split avoidance policies
- duplicate metrics feeding cost and maintenance surfaces

### 12.8 ANN Recall Stability

Current repository status:

- HNSW exists and stores MGA-oriented state
- there is not yet a strong published model for stale delete backlog, rebuild triggers, or search-quality decay under churn

Required hardening direction:

- deleted-node backlog metrics
- search quality and stale-edge diagnostics
- rebuild or repair triggers before recall drift becomes operationally opaque

### 12.9 Spatial Split Correctness

Current repository status:

- GiST, SP-GiST, and R-tree implementations exist
- split correctness, penalty validation, and spatial partition invariants need stronger validation and conformance coverage

Required hardening direction:

- validator support for overlap, coverage, and partition correctness
- deterministic split test corpora with adversarial geometry

### 12.10 Bitmap Maintenance Cost

Current repository status:

- bitmap DML integration exists
- MGA churn and update-heavy workloads can make bitmap maintenance expensive or stale

Required hardening direction:

- bounded update strategies
- container compaction rules
- cost-based planner reluctance when bitmap maintenance debt is high

### 12.11 Columnstore Cooperation

Current repository status:

- columnstore indexing and segment code exists
- interaction with row-version churn, GC horizons, and exact secondary indexes remains underspecified

Required hardening direction:

- row-group or segment visibility contracts
- coordinated summary rebuild and dead-row pruning
- cost model integration with exact-tree and summary families

## 13. Index Family Analysis

### 13.1 B-tree

#### Architecture

ScratchBird’s B-tree is the most advanced exact-tree family in the repository and the logical anchor for hardening all other exact lookup families. The implementation already has leaf and internal pages, sibling pointers, parent pointers, per-entry MGA fields, prefix compression, split logic, merge logic, bottom-up bulk load, iterator support, and GC compaction. Several other logical families collapse onto this runtime class through `IndexFactory`, which raises its importance further.

The architecture is still pre-hardening in one decisive sense: it treats page mutation as the main truth, with catalog root metadata and in-memory state carrying the rest, instead of using an authoritative B-tree metapage and explicit structural-publication protocol. The implementation also combines page-structure policy, search policy, and mutation policy tightly in one family rather than expressing a separated validator/publication/control-plane design.

#### MGA Implications

B-tree is the exact family where MGA obligations are least negotiable. It is expected to support precise equality and range lookup, uniqueness enforcement, ordered scans, duplicate management, and robust churn handling. In MGA, every update that changes indexed key material creates a new candidate and leaves the old candidate to be reclaimed later. That means heavy update workloads can create duplicate runs, dead-version density, and split pressure even when logical row identity stays stable. This is not edge behavior. It is the steady-state behavior of an MGA tree under real workloads.

That is why the canonical bundle’s compact-before-split and duplicate-pressure rules are not optimization extras. They are essential design rules. A B-tree that only splits when full and compacts “eventually” is correct in a toy sense but unstable under production MGA churn.

#### Comparison with Donor Systems

Firebird contributes the worldview: heap/version truth first, index cleanup second, and validation as a serious operational surface. PostgreSQL contributes the best family-specific invariant discipline, especially around fence semantics, verification, and explicit AM responsibilities. InnoDB contributes the online rebuild and publication-barrier mindset. MongoDB contributes the strongest reusable shadow-build barrier pattern. MariaDB adds metadata defensiveness around online maintenance state.

ScratchBird should therefore implement:

- Firebird-like heap-led exact cleanup
- PostgreSQL-like invariant formalization and validation
- InnoDB/MongoDB-like disciplined rebuild publication

It should not import PostgreSQL WAL or InnoDB redo as core truth. The canonical B-tree intent-record design already gives the correct non-WAL equivalent.

#### Maturity Gaps

The maturity gaps are substantial and concrete:

- no authoritative B-tree metapage
- no durable SMO intent records
- no restart-repair procedure for split, merge, or root change
- no fence/high-key contract
- no split-tolerant descent
- immediate page free after merge rather than reclaim quarantine
- compressed-page search still performs linear decode
- duplicate/posting-list policy incomplete
- validator framework incomplete
- observability incomplete

#### Hardening Requirements

The hardened B-tree specification set should require:

- metapage-based root and build-state publication
- page-format versioning and rebuild trigger for incompatible layouts
- fence/high-key/right-link semantics on every searchable page
- explicit latch protocol with `SEARCH_SHARED`, `WRITE_INTENT`, `WRITE_EXCLUSIVE`, and `MAINTENANCE_EXCLUSIVE`
- bounded SMO intent records with restart repair
- duplicate/posting-list continuation rules
- compact-before-split policy driven by reclaimability score
- conservative merge and deletion with quarantine
- structural validator and corruption/repair surface
- per-index and per-level metrics

#### Specification-Ready Subsystem Design

The hardened B-tree should be specified as four cooperating substructures:

1. Metapage and control record.
   This stores authoritative `root_gpid`, height, first leaf, page counts, build state, publication sequence, active SMO count, pending reclaim count, format version, and verification epoch.

2. Searchable pages.
   Leaf and internal pages carry bounds, right-link truth, local slot/node payloads, compression metadata, and minimal state needed for restart reconciliation. The current `SBBTreePage` and `SBBTreeNode` can inspire field naming, but their binary layout should not be treated as final.

3. Structural-intent journal.
   This is not a general WAL. It is a bounded B-tree-local repair substrate sufficient to complete or reconcile split, merge, root change, and bulk-publish operations after crash.

4. Validator and operator surface.
   Every tree generation must be checkable online and offline. Operators need split-retry counts, right-link chase counts, dead-version density, duplicate pressure, pending reclaim pages, repaired-on-restart counts, and validator outcomes.

#### Work Plan Decomposition

The B-tree work should align directly to the canonical workplan tracks:

- Track A: metapage, publication sequence, SMO intents, restart repair
- Track B: compressed-page search acceleration and separator rigor
- Track C: split-tolerant descent, conservative deletion, reclaim quarantine
- Track D: duplicate pressure and MGA churn accounting
- Track E: build/rebuild state machine and cutover
- Track F: validator, metrics, failpoints, and gates

#### Testing Strategy

Required B-tree testing goes far beyond unit insertion and lookup.

Unit and property tests:

- key encoding, compression, separator truncation, and fence validation
- duplicate run handling
- compact-before-split decisions
- parent/rightmost-child invariants

Concurrency tests:

- concurrent split/search without root restart
- concurrent split plus range scan
- concurrent merge candidate plus active scan
- uniqueness conflict races across active transactions

Crash and recovery tests:

- failpoint at every SMO phase
- restart repair after split, merge, and root publication interruption
- interrupted bulk build and interrupted shadow promotion

Stress and performance tests:

- duplicate-heavy insert churn
- MGA-heavy update churn
- long range scans with concurrent writers
- hot upper-level contention and latch-wait metrics

### 13.2 Hash

#### Architecture

ScratchBird’s hash family includes explicit metapage, directory, and bucket structures, with MGA fields on entries and a split mechanism. This is enough to treat it as a real transactional hash index rather than a toy unordered map on disk. The logical family also carries Redis-style hash-like aliases through the catalog taxonomy.

Hash is simpler than B-tree in ordering terms but not in durability terms. Buckets, overflow state, and directory growth all require publication and reclaim discipline. A hash index under MGA is still a multi-version access method with hot-spot collision behavior.

#### MGA Implications

Hash is an exact lookup family. That means it inherits the exact-family requirements:

- heap visibility remains authoritative
- dead entries must be removed precisely
- hash collision chains or buckets must not silently retain dead blockers forever
- uniqueness-capable hash variants need the same MGA-aware conflict semantics as B-tree

Hash has an additional operational hazard: dead and hot entries accumulate in the same hot buckets that already suffer contention. If dead-entry cleanup is slow, lookup cost becomes unstable in exactly the areas with the highest write traffic.

#### Comparison with Donor Systems

PostgreSQL hash is the clearest donor for a dedicated disk hash AM with validation and cleanup hooks. InnoDB does not provide a directly analogous persistent general-purpose hash index, so its contribution is mostly around publication discipline and delayed cleanup patterns. MongoDB’s validation culture and Redis’s hot-key operational thinking are conceptually useful for collision metrics and corruption handling, but PostgreSQL remains the primary exact donor.

#### Maturity Gaps

Observed and likely gaps are:

- bucket split durability is not yet described as a restart-repairable protocol
- overflow and directory corruption detection is not yet first-class
- hot-bucket contention metrics are not prominent
- uniqueness semantics and wait/restart rules are not platform-standardized
- rebuild and cutover follow generic catalog shadowing but need family-specific validation

#### Hardening Requirements

The hardened hash specification should require:

- authoritative hash metapage with directory epoch, bucket count, split state, and format version
- restart-safe bucket split protocol
- overflow-chain validator
- bucket-local dead-entry density and reclaim metrics
- exact dead-entry deletion with optional bucket compaction
- collision and hot-bucket observability
- shadow rebuild validation before publication

#### Specification-Ready Subsystem Design

Hash should be specified as a bucketed exact AM with split publication discipline analogous in spirit to B-tree SMOs but simpler in topology. Each split or directory growth event should have:

- split descriptor or intent record
- source and destination bucket identities
- directory publication epoch
- optional overflow-page quarantine for reclaimed bucket segments

Search must always:

1. compute hash
2. consult current directory epoch
3. scan the candidate bucket chain
4. exact-compare key bytes for collision resolution
5. heap-check MGA visibility for candidate TIDs

#### Work Plan Decomposition

- define bucket metapage and split-state metadata
- specify exact dead-entry and overflow compaction rules
- add validator and hot-bucket metrics
- implement shadow rebuild validation and cutover

#### Testing Strategy

- collision storm tests
- crash at bucket split phases
- dead-entry cleanup in hot buckets
- checksum and corrupted-overflow-chain diagnostics
- high-concurrency point-lookup and update benchmarks

### 13.3 GIN and Inverted / Text Families

#### Architecture

ScratchBird has two partially overlapping text and posting-list directions:

- a page-managed GIN implementation with pending list, posting list, posting tree, and token extraction support
- a standalone inverted index implementation with segment-style metadata and posting structures

It also has a `FullTextIndex` wrapper, plus extensive aliasing from text-like or sparse-inverted logical types to the standalone inverted runtime class. This is a powerful starting point, but it also means the platform currently lacks one unambiguous family boundary between “transactional posting tree,” “full-text wrapper,” and “storage-attached inverted segments.”

#### MGA Implications

GIN and inverted families are poster children for MGA churn. Updates and deletes create posting-list churn faster than exact scalar trees do because one row may emit many tokens. The family must therefore manage:

- per-posting visibility and dead posting removal
- pending-list or delta backlog
- token-normalization versioning
- shadow rebuild or segment merge without exposing partial publication

Approximation is not acceptable here unless the family is explicitly documented as approximate. Text search ranking may be lossy in scoring, but token visibility cannot be lossy.

#### Comparison with Donor Systems

PostgreSQL GIN contributes pending-list management, posting-tree structure, and vacuum discipline. Cassandra SAI contributes immutable component publication and validation for storage-attached inverted structures. Milvus contributes load-time and file-existence validation for text variants. MongoDB contributes strict text specification and term-format validation. OpenSearch contributes normalization and corruption tooling culture.

The correct ScratchBird synthesis is to separate two subfamilies cleanly:

- transactional page-managed GIN for exact posting maintenance inside the buffer/page world
- storage-attached or segment-managed inverted family for larger merge-based or summary-like text paths

#### Maturity Gaps

The repository currently shows several unresolved issues:

- `FULLTEXT` is inconsistently mapped between wrapper, inverted runtime, and at least one GIN-open path
- pending-list and posting-tree backlog metrics are not standardized
- rebuild/publication semantics are not yet one family contract
- tokenizer and normalization versioning are not surfaced as format/publication dependencies
- delete precision and maintenance debt are not yet bounded operationally

#### Hardening Requirements

The hardened specification should require:

- one canonical mapping for each logical family: GIN, FULLTEXT, INVERTED, sparse variants, trie-like text aliases
- exact posting deletion for page-managed GIN
- bounded pending-list or delta-list backlog with forced merge thresholds
- tokenization and normalization format versioning
- build validation before publish
- deep validator for posting trees, dictionaries, and segments
- corruption and parse-error counters visible to operators

#### Specification-Ready Subsystem Design

Define two explicit contracts.

Transactional GIN contract:

- metapage with pending-list head, posting-tree root, format version, build state
- exact posting maintenance for insert/delete/update
- pending-list merge state machine
- posting-tree validator

Storage-attached inverted contract:

- segment manifest or component registry
- immutable segment build, validate, then publish
- delta/deletes tracked separately until merge or rebuild
- token and analyzer version stored with each published generation

The `FULLTEXT` wrapper should become a front-door API surface, not an ambiguous physical family. It should resolve to either transactional GIN or storage-attached inverted explicitly through capability metadata.

#### Work Plan Decomposition

- resolve family mapping and `FULLTEXT` ambiguity first
- separate transactional and storage-attached inverted specs
- define token-format versioning and analyzer drift rules
- add posting-list and pending-list metrics
- implement validator and rebuild publication surfaces

#### Testing Strategy

- token explosion and skew tests
- delete-heavy posting churn tests
- analyzer-version mismatch open tests
- corrupt posting tree / corrupt segment manifest tests
- shadow rebuild with concurrent text updates

### 13.4 GiST

#### Architecture

ScratchBird’s GiST implementation exposes the right conceptual pieces: operator class, predicate representation, extensible insert/search/remove, and nearest-neighbor support. That is a strong sign that the repository intends GiST as a real generalized search tree, not a one-off spatial wrapper.

GiST, however, is only production-grade when its operator-class rules, split logic, and lossy semantics are specified as carefully as B-tree’s total-order semantics. A generalized tree without explicit validation is a corruption and correctness trap.

#### MGA Implications

GiST frequently returns candidates that require heap recheck because bounding predicates are lossy. Under MGA, that means every candidate must survive both:

- structural navigation correctness
- heap/version visibility correctness

Delete and update churn also change bounding predicates at internal nodes. If parent predicates are not recomputed correctly after dead-entry cleanup or page redistribution, the tree can remain structurally connected but semantically wrong.

#### Comparison with Donor Systems

PostgreSQL GiST is the primary donor because it has the mature vocabulary for operator classes, consistent/recheck semantics, and split/penalty contracts. OpenSearch contributes good ideas for geometry normalization and input validation. Firebird contributes validation seriousness even though it does not expose a GiST equivalent in the same way.

#### Maturity Gaps

The repository currently lacks a fully hardened contract for:

- operator-class invariants
- lossy versus exact recheck semantics surfaced to planner and executor
- split correctness and bounding-predicate validator
- crash-safe publication for structural changes
- nearest-neighbor stability under concurrent mutation

#### Hardening Requirements

The hardened GiST specification should require:

- a formal operator-class contract for `consistent`, `union`, `penalty`, `picksplit`, and optional distance support
- explicit “requires recheck” semantics in planner/runtime metadata
- metapage and page-format versioning
- structural publication rules for split and parent-bound update
- validator for coverage, bounding correctness, and orphan detection

#### Specification-Ready Subsystem Design

Specify GiST as a generalized balanced tree with:

- metapage holding root, height, opclass version, and build/validation state
- internal entries storing bounding predicates plus child references
- leaf entries storing predicate payload plus TID
- exact heap recheck rules for lossy opclasses
- conservative deletion and rebuild rules when precise predicate shrink is expensive or unsafe

The validator must be able to prove:

- every child predicate is covered by the parent bounding predicate
- no orphan or unreachable page exists
- split output predicates partition or cover according to opclass rules

#### Work Plan Decomposition

- formalize operator-class API and capability metadata
- implement metapage and publication rules
- add structural validator and lossy-recheck surfaces
- add nearest-neighbor maintenance debt metrics

#### Testing Strategy

- adversarial predicate overlap cases
- update/delete churn affecting internal bounding predicates
- concurrent split/search with forced heap rechecks
- opclass fuzzing and validator corpus

### 13.5 SP-GiST

#### Architecture

SP-GiST is a partitioned search tree for data whose structure is better modeled by decomposed partitions than by bounding unions. ScratchBird already contains inner and leaf tuple structures plus operator-class variants for quad and text-style partitioning.

This family can be excellent for skewed or partitionable data, but only if partition invariants are explicit and verifiable. An SP-GiST without strong routing guarantees will degrade into silent omission or duplication risk.

#### MGA Implications

Under MGA, partitioned trees must handle:

- dead leaf entries without invalidating routing state
- updates that move a row from one partition path to another
- long scans or searches during concurrent node split or redirect

Because routing is partition-based rather than total-order-based, the family needs a strict rule about when a route may be redirected and how readers observe either pre-change or post-change state without losing candidates.

#### Comparison with Donor Systems

PostgreSQL SP-GiST is again the main donor because it defines a mature space-partitioned AM. Neo4j’s spatial decomposition ideas and OpenSearch’s input normalization provide adjunct inspiration, but PostgreSQL provides the most directly transferable partition-tree discipline.

#### Maturity Gaps

Current likely gaps:

- route invariants are not yet deeply specified
- redirect and split publication rules are not hardened
- validator coverage for partition completeness and non-overlap is not visible
- planner/executor lossy or exact semantics are not strongly surfaced

#### Hardening Requirements

The hardened specification should require:

- explicit partition invariant definitions for every operator class
- redirect-safe split publication
- validator for route completeness, overlap, and child reachability
- metapage/build-state/versioning rules
- dead-leaf backlog and redirect metrics

#### Specification-Ready Subsystem Design

Specify SP-GiST as:

- metapage plus partition-tree pages
- inner tuples that own routing partitions and child references
- leaf tuples carrying TID plus local predicate payload
- redirect markers or equivalent state for safe concurrent route changes
- heap recheck rules where partitioning is lossy

The design should explicitly bound how long redirect state may persist and when compaction or rebuild is required.

#### Work Plan Decomposition

- define operator-class route invariants
- add redirect/split publication and validator rules
- expose route-stability metrics
- integrate shadow rebuild and version-aware cutover

#### Testing Strategy

- skewed text and quad-partition datasets
- repeated update path movement
- concurrent split/search with redirect chasing
- validator-driven corruption injection tests

### 13.6 Bitmap

#### Architecture

ScratchBird’s bitmap family uses dictionary, root, and container-style pages, plus compressed bitmap support through RLE/Roaring-like helpers. This is a useful family for low-cardinality filtering, and the repository already treats it as a first-class DML-maintained index rather than a purely derived analytical artifact.

Bitmap indexes, however, are operationally tricky under MGA. They can be very compact for mostly static low-cardinality data, yet expensive under frequent updates because one row-version change can require toggling bits across multiple value containers while old row versions remain temporarily visible or at least not yet reclaimable.

#### MGA Implications

Bitmap results are candidate sets, not visibility truth. Under MGA, a bitmap family must either:

- maintain exact per-row-version membership and rely on heap filtering for final visibility, or
- maintain a published bitmap plus delta/tombstone overlay that is reconciled before a bit can be reclaimed

The worst design is a bitmap that eagerly clears bits on delete or update without regard to reader horizon. That would create false negatives. The second-worst design is a bitmap that only ever sets bits and never compacts them, which will make update-heavy workloads degenerate quickly.

#### Comparison with Donor Systems

There is no exact one-to-one donor in the local clone set comparable to Oracle-style bitmap indexing, but several systems provide relevant partial lessons. PostgreSQL’s bitmap scan framework demonstrates the importance of treating bitmap outputs as candidate sets. Redis provides compact bitmap manipulation and operational visibility ideas. Search and inverted systems demonstrate container-level compaction and posting cleanup patterns. ScratchBird should synthesize these rather than pretending the family is “just an array of bits.”

#### Maturity Gaps

Current likely gaps:

- update-heavy cost model and maintenance debt are underspecified
- exact dead-bit deletion versus delayed tombstone reconciliation is not fully formalized
- container compaction and rebuild thresholds are not prominent
- planner does not yet reason deeply about bitmap staleness or maintenance cost

#### Hardening Requirements

The hardened bitmap specification should require:

- exact candidate semantics with no false negatives from premature clear
- delta or tombstone overlay when direct in-place mutation is unsafe
- container density and dead-bit metrics
- compaction and rebuild thresholds
- validation for dictionary/container linkage and cardinality sanity
- cost-model hooks for update penalty and bitmap fanout

#### Specification-Ready Subsystem Design

The preferred design is a hybrid:

- stable compressed containers represent published value membership
- mutable delta containers capture recent set/clear operations
- a compaction job merges deltas into stable containers after horizon-safe reconciliation
- bitmap scans read published plus delta state, then heap-filter candidates

This gives the family a clean MGA story while preserving bitmap scan efficiency on mostly read-heavy data.

#### Work Plan Decomposition

- define container publication and delta reconciliation rules
- add maintenance debt metrics
- add validator and rebuild path
- integrate planner penalties for update-heavy tables

#### Testing Strategy

- low-cardinality high-update churn
- high-density versus sparse-container extremes
- AND/OR/NOT combination correctness under dead-bit backlog
- corrupted dictionary/container linkage tests

### 13.7 BRIN and Zonemap

#### Architecture

ScratchBird’s BRIN implementation already includes pages, ranges, and revmap-oriented logic. The catalog also aliases `ZONEMAP` and `BLOOM`-style logical families onto the BRIN runtime class. That means ScratchBird already recognizes that not all “indexes” are exact row locators. Some are summaries or skipping aids.

This family should be treated as summary infrastructure with transactional integration, not as a weaker B-tree. The specification must say that explicitly.

#### MGA Implications

BRIN and zonemap summaries are especially sensitive to MGA misunderstandings. A summary range cannot simply delete a range entry because one dead TID appears inside it. The canonical spec is correct: summary families must resummarize from authoritative storage or schedule bounded rebuild. Blindly tombstoning an entire range is not acceptable steady-state behavior.

Under MGA, summaries must also cope with:

- append-driven growth
- updates that invalidate summary bounds without making the entire range worthless
- delete churn that changes selectivity but not necessarily min/max truth

#### Comparison with Donor Systems

PostgreSQL BRIN provides the clearest AM pattern, especially its autosummarization model and distinction between unsummarized and summarized ranges. ClickHouse min-max indexes show how a skip summary should cooperate with immutable parts. DuckDB row-group zonemaps demonstrate how summary metadata can be coupled tightly to storage segments and still remain understandable and verifiable.

#### Maturity Gaps

Current gaps likely include:

- insufficient distinction between exact and summary maintenance obligations
- aliasing of zonemap and Bloom logical types without a fully explicit capability story
- limited stale-summary metrics
- incomplete autosummarization and rebuild decision framework

#### Hardening Requirements

The hardened BRIN/zonemap specification should require:

- explicit summary state for each range or row-group
- resummarization from authoritative storage, not blind summary deletion
- false-positive density and stale-summary metrics
- pending unsummarized-range queue
- storage-segment alignment rules for columnstore cooperation
- validator for revmap and range-summary consistency

#### Specification-Ready Subsystem Design

Specify BRIN/zonemap as:

- metapage with pages-per-range, autosummarize policy, format version, and queue head
- revmap mapping ranges to summary tuples
- range state machine: unsummarized, summarized, stale, rebuilding
- optional cooperation surface with columnstore row groups or heap page groups

The family must expose whether a summary is:

- exact for nullness or presence checks
- lossy for range filtering
- stale but still usable
- stale enough to require rebuild before planner trust

#### Work Plan Decomposition

- formalize exact-versus-summary capability reporting
- add stale-summary and false-positive metrics
- define resummarize and rebuild thresholds
- align row-group and range boundaries with columnstore and table layout

#### Testing Strategy

- append-only growth with autosummarize
- update churn invalidating min/max bounds
- stale-summary planner regression tests
- revmap corruption and recovery diagnostics

### 13.8 R-tree and Spatial

#### Architecture

ScratchBird has an explicit R-tree implementation with bounding boxes, reinsertion support, and condense-tree logic. It also carries MongoDB and Redis geo logical aliases that map to spatial runtime classes. This is a meaningful spatial foundation, but spatial indexes are only as trustworthy as their split correctness, geometry normalization, and validator coverage.

#### MGA Implications

Spatial indexes are frequently lossy candidate generators. Under MGA, this means:

- heap visibility is still mandatory
- geometry normalization must happen before index insertion to avoid semantic drift
- delete/update churn can leave parent minimum bounding rectangles too large or stale if shrink logic is weak
- long spatial scans must not miss candidates during concurrent split, reinsert, or condense operations

#### Comparison with Donor Systems

The best donor ideas come from multiple places. GiST-style generalized validation from PostgreSQL is conceptually relevant. MongoDB contributes strict geometry validation and key-generation limits. Neo4j contributes space-filling-curve decomposition ideas for coarse prefilters. OpenSearch contributes geometry parser rigor and normalization. ScratchBird should synthesize those patterns while keeping its own page-oriented spatial runtime.

#### Maturity Gaps

Current gaps likely include:

- insufficient geometry normalization and invalid-shape rejection rules
- limited validator support for MBR containment and child coverage
- unclear crash-safe publication for reinsertion and condense operations
- limited spatial split-quality metrics

#### Hardening Requirements

The hardened spatial specification should require:

- canonical geometry normalization before key generation
- MBR validator for parent-child coverage
- split and reinsertion publication rules
- delete/condense quarantine rules
- metrics for overlap, fanout, reinsertion count, and dead-entry backlog

#### Specification-Ready Subsystem Design

Specify the spatial family with:

- metapage holding root, dimension model, geometry normalization version, and build/validation state
- leaf entries storing canonicalized geometry key plus TID
- internal entries storing MBR plus child pointer
- optional coarse space-filling prefilter metadata where helpful
- validator capable of proving child coverage, orphan absence, and sane overlap statistics

#### Work Plan Decomposition

- formalize geometry normalization and error handling
- add split/condense publication rules
- add spatial validator and overlap metrics
- integrate shadow rebuild and repair guidance

#### Testing Strategy

- adversarial overlapping rectangles and polygons
- malformed geometry rejection
- concurrent insert/delete with long spatial scan
- crash during condense or reinsertion

### 13.9 HNSW and Vector ANN

#### Architecture

ScratchBird currently uses `HnswIndex` as the primary runtime for a very broad logical vector family surface, including HNSW itself and many logical types named as IVF, PQ, SQ, NSG, DiskANN, ScaNN, and others. This is a major honesty issue for the future specification set. A catalog can expose aliases for compatibility, but a hardened spec must never imply that multiple distinct ANN algorithms already exist when they currently collapse to one runtime implementation.

The existing HNSW code already stores nodes, neighbor lists, and MGA-oriented version fields. This is a real starting point. It is not yet a mature ANN platform.

#### MGA Implications

ANN families are approximate only in candidate generation. They are never approximate in visibility. Therefore:

- every returned candidate must still pass heap visibility checks
- delete/update churn must not silently leave old vectors effectively immortal
- recall must remain bounded and observable as stale deletes accumulate
- build/cutover must publish a coherent graph generation, not a half-updated one

ANN structures are especially vulnerable to maintenance debt because stale edges and deleted nodes degrade quality gradually rather than causing immediate correctness failures. That makes observability and rebuild thresholds essential.

#### Comparison with Donor Systems

Milvus is the strongest donor for mutable-versus-sealed ANN generations, delete deltas, and delayed reclaim. Neo4j contributes strong configuration validation and work-synced population. Redis vector sets contribute prepared-in-background/commit-after-revalidation patterns plus serialized-format versioning and graph checks. OpenSearch contributes validation and corruption-handling culture, though not its refresh-based visibility model.

#### Maturity Gaps

Current repository gaps include:

- one runtime class advertised as many logical algorithms
- no strong published distinction between mutable delta state and sealed graph generations
- limited stale-delete backlog metrics
- limited graph validator and connectivity checks
- unclear repair or rebuild thresholds when recall drifts

#### Hardening Requirements

The hardened vector specification should require:

- honest capability reporting: logical aliases only where semantics are truly shared
- vector dimension, metric, and quantization validation
- published graph generation plus mutable delta or delete overlay
- stale-delete backlog metrics
- graph validator for connectivity, level structure, and serialization sanity
- rebuild triggers tied to backlog, recall drift, or corruption findings

#### Specification-Ready Subsystem Design

Specify vector indexing as two layers:

1. Mutable ingestion layer.
   New vectors and deletes land in a delta structure that is MGA-aware and cheap to update.

2. Published ANN generation.
   A validated HNSW graph generation is published atomically through generation metadata. Search consults the published graph plus deltas, then heap-filters candidates.

Logical family naming must be conservative. Until ScratchBird has true IVF/PQ/SQ/DiskANN runtimes, the catalog may retain compatibility aliases, but the operator and specification surfaces must state that the current physical runtime class is HNSW-based.

#### Work Plan Decomposition

- separate logical marketing names from physical runtime truth
- define mutable delta plus published-generation contract
- add validator and stale-delete metrics
- define rebuild policy and fallback scan rules

#### Testing Strategy

- recall-under-delete-churn benchmarks
- configuration mismatch and serialization-version tests
- graph corruption and connectivity diagnostics
- concurrent build/publish with active search

### 13.10 Columnstore and Storage-Attached Indexing

#### Architecture

ScratchBird’s columnstore code already includes metadata pages, data pages, segments, and scan iterators. In the current registry it is still treated as a page-based runtime class, but conceptually it behaves closer to a storage-attached segmented structure than to a point-mutated exact tree.

This family is important not only on its own, but also because it should cooperate with BRIN/zonemap summaries, exact secondary indexes, and table or row-group visibility tracking.

#### MGA Implications

Columnar or storage-attached structures are naturally inclined toward immutable published segments plus mutable delete or delta overlays. That fits MGA well if done honestly:

- new versions append into new segment or delta state
- old row versions remain until GC horizon permits removal
- published segments are immutable and easy to validate
- dead-row pruning and segment merge happen through controlled maintenance publication

Trying to force page-local eager in-place updates into this family would be the wrong design.

#### Comparison with Donor Systems

Cassandra SAI, ClickHouse parts, DuckDB row groups, and Milvus sealed segments all point to the same durable pattern: build immutable components, validate them, publish them, then retire old ones only after readers are past them. DuckDB and ClickHouse also show the importance of summary synergy and row-group statistics.

#### Maturity Gaps

Current gaps likely include:

- insufficiently explicit published-segment versus mutable-delta state
- incomplete row-group or segment visibility rules
- validator and checksum model not fully unified
- weak cooperation model with BRIN/zonemap and exact secondary indexes

#### Hardening Requirements

The hardened columnstore specification should require:

- immutable published segments
- delete vectors or delta buffers
- segment manifest with generation metadata
- checksum and validator support
- explicit row-group or segment visibility rules
- integration hooks for zonemap/BRIN summaries and optimizer costing

#### Specification-Ready Subsystem Design

Specify columnstore indexing as:

- segment manifest or metapage describing active published segments
- append/delta region for fresh writes
- delete vector per segment or per row-group
- maintenance merge that rebuilds a new published segment generation
- optional shared summary interfaces with BRIN/zonemap

The family should advertise when it is:

- exact for projected included values
- lossy for pruning
- stale due to pending delete-vector consolidation

#### Work Plan Decomposition

- define segment publication and manifest rules
- add delete-vector and segment-staleness metrics
- align row-group summaries with BRIN/zonemap
- add validator and rebuild/repair guidance

#### Testing Strategy

- append-heavy ingest
- delete-vector accumulation and merge
- row-group pruning accuracy
- corrupted segment manifest or checksum mismatch tests

### 13.11 Write-Optimized Families: LSM, ART, and Trie-Like Structures

#### Architecture

ScratchBird currently has a real `LSMTreeIndex` runtime with memtable, SSTable, Bloom, flush, scan, and cleanup support. It also still contains older `LSMTree` helper code with visibility behavior that appears older and less canonical. Meanwhile `ART` is currently a logical alias to B-tree runtime, and `TRIE` is currently a logical alias to the inverted runtime class.

This means the write-optimized family space is partially real and partially aspirational. The specification set must reflect that honestly.

#### MGA Implications

Write-optimized families live or die by publication and compaction rules under active readers. Under MGA:

- memtable or delta state can contain uncommitted or not-yet-horizon-safe entries
- flush and compaction must publish immutable generations atomically
- tombstones must remain until visibility horizon allows discard
- old component generations must remain readable until all relevant readers are past them

This is extremely similar, at the control-plane level, to shadow rebuild and columnstore segment publication.

#### Comparison with Donor Systems

Cassandra provides the clearest storage-attached LSM and compaction discipline. DuckDB provides useful ART verification and delta-attach patterns. ClickHouse provides immutable-part replacement intuition. Redis radix trees are useful for compact trie layout thinking, but not as a durability model.

#### Maturity Gaps

Key gaps include:

- dual LSM code paths with inconsistent visibility assumptions
- lack of one canonical manifest/publication contract
- tombstone backlog and compaction-debt metrics not yet central
- ART and trie logical names exposed without true distinct runtime implementations

#### Hardening Requirements

The hardened specification should require:

- one canonical LSM implementation path
- no raw visibility math outside the transaction manager
- manifest-based immutable generation publication
- tombstone retention tied to MGA horizon
- validator for SSTable components and manifests
- honest capability reporting for ART and trie logical families

#### Specification-Ready Subsystem Design

LSM should be specified as:

- mutable memtable
- immutable SSTable generations plus manifest
- flush, compaction, and generation-retirement state machine
- Bloom and auxiliary metadata attached to immutable generations
- validator for component headers, checksums, and manifest linkage

ART should remain a logical alias until ScratchBird implements a true ART runtime. If a future true ART is built, it should inherit exact-tree MGA rules and validator obligations, not define a separate visibility model.

Trie-like logical families should also remain aliases until a real radix/trie runtime exists. When implemented for production, they should likely share more with transactional or storage-attached inverted structures than with simplistic prefix dictionaries.

#### Work Plan Decomposition

- retire or align older non-canonical LSM helper code
- define manifest and compaction publication rules
- add tombstone-backlog and compaction-debt metrics
- defer true ART and trie implementations until the exact and inverted platforms are hardened

#### Testing Strategy

- flush and compaction under active readers
- tombstone storm workloads
- manifest corruption and restart recovery
- mixed memtable plus SSTable scan correctness

## 14. Structural Durability Model

ScratchBird needs one platform durability taxonomy that can be specialized by family class without collapsing into “one WAL fits all” thinking.

The recommended taxonomy is:

- exact mutable tree families: metapage plus bounded structural-intent durability
- mutable hash families: metapage plus bucket-split publication records
- storage-attached summary and columnar families: manifest/segment publication
- ANN families: published generation plus mutable delta/delete overlay

The common rule is that publication is explicit and reclaim is delayed. The family-specific mechanism changes, but the publication-and-retirement semantics do not.

### 14.1 Split Protocol for Exact Trees

For B-tree and any future exact tree family with sibling navigation, the split protocol must make two legal states visible to readers:

- pre-split page still owns the full key range
- left page redirects to right page through durable right-link and high-key truth

Required phases:

1. `PREPARE_RIGHT`
   - allocate right page
   - write fully initialized right page image
   - persist SMO intent before it becomes reachable

2. `LINK_RIGHT`
   - update left page high key and right link
   - update neighbor sibling metadata as needed
   - right page must already be valid on disk

3. `PUBLISH_PARENT`
   - insert separator or publish new root
   - parent publication may recurse

4. `CLEAR_INTENT`
   - clear intent only after metapage counters and parent visibility are durable

Ordering invariants:

- right page is durable before left page tells readers to chase to it
- left page redirect is durable before parent publication relies on it
- intent survives until the operation is either fully published or restart-repairable

### 14.2 Merge Protocol

Merge is not symmetric with split and must be more conservative. It is a maintenance path, not a normal write path.

Required phases:

1. `MARK_DELETE_CANDIDATE`
   - victim page becomes non-newly-publishable for future descent

2. `REDIRECT_RIGHTLINKS`
   - sibling navigation and surviving page bounds are updated so scans cannot get trapped

3. `DELETE_PARENT_SEPARATOR`
   - parent downlink or separator cleanup is durably recorded

4. `QUARANTINE_FREE`
   - victim is unreachable from the active tree but still not allocator-reusable

5. `CLEAR_INTENT`
   - only after reader-horizon and parent-safety conditions hold

The current code path that immediately frees a merged B-tree page must be replaced with this model.

### 14.3 Root Changes

Every exact tree family needs explicit root-publication semantics:

1. allocate new root page
2. install child links and local metadata
3. durably publish root through metapage and increment publication sequence
4. retire old root as an ordinary reachable-but-old structure until safe reclamation
5. clear intent or publication record

The root is not “whoever the catalog says today.” The root is what the family metapage has durably published.

### 14.4 Rebuild Cutover

Rebuild cutover is the platform bridge between catalog lifecycle and physical durability.

For page-managed families:

1. create shadow generation in catalog
2. create physical control page or metapage in `INIT`
3. build provisional structure
4. validate structure
5. publish root or manifest through family control page
6. promote catalog generation visibility
7. retire old generation
8. reclaim old pages only after catalog and reader horizon allow it

For storage-attached families:

1. write new component set into staging location
2. validate components and checksums
3. publish manifest/generation pointer
4. update catalog visibility
5. retire old manifest generation
6. reclaim old components after reference horizon passes

### 14.5 Crash Recovery Rules

At startup, ScratchBird must recover indexes through family control structures, not guesswork.

Required startup behavior:

1. load catalog metadata and family control pages/manifests
2. detect active build, SMO, or publication records
3. perform deterministic reconciliation:
   - complete operation
   - roll forward to safe post-publication state
   - quarantine incomplete new state
   - mark family generation invalid when repair proof is impossible
4. emit diagnostics and counters

Hard rules:

- no ambiguous page or component may be returned to the allocator
- no generation may be treated as active unless validation/publication evidence exists
- repaired-on-restart events must be observable

## 15. Concurrency Model

### 15.1 Separate Latches from Locks

ScratchBird should explicitly separate:

- latches: short critical-section protection for pages, buckets, segments, graphs, or manifests
- locks: transaction or DDL/maintenance coordination visible to higher layers

The current B-tree uses lock-manager page locks as a concurrency backbone. That is acceptable for early correctness, but it is not the right mature model. Hardened families need lightweight latches and a smaller set of transactional locks:

- table DDL locks
- maintenance locks for build/cutover
- unique key conflict locks
- diagnostic locks where needed

### 15.2 Exact Tree Latch Protocol

For B-tree and similar families:

- `SEARCH_SHARED` for normal descent and scan
- `WRITE_INTENT` when descending toward a target mutation
- `WRITE_EXCLUSIVE` for local page modification
- `MAINTENANCE_EXCLUSIVE` for merge/deletion/reclamation phases

Rules:

- readers may hold at most two latches during descent
- readers may release parent after child fence bounds prove correctness
- writers may descend optimistically and upgrade near the target page
- right-link chase must avoid root restart

### 15.3 Storage-Attached and Component Families

For columnstore, BRIN-like summaries, LSM, and segment-based inverted or ANN families, concurrency is publication-oriented rather than page-surgery-oriented.

Rules:

- mutable deltas use their own short latches
- published segments or generations are immutable
- readers pin generation or manifest references
- writers publish new generations rather than mutating old published ones in place

### 15.4 Scan Stability

Every family must provide a written scan-stability guarantee.

Minimum platform guarantee:

- no false negatives due to concurrent structural change
- readers may see either pre-change or post-change structure
- readers may need heap recheck or duplicate elimination, but they may not miss visible qualifying rows

Exact trees achieve this through fence/right-link semantics.
Storage-attached families achieve it through pinned manifest generations.
Approximate families achieve it through published generation plus delta overlay, followed by heap visibility filtering.

### 15.5 Concurrent Mutation Handling

Exact trees:

- page-local mutation with split/merge publication

Summary families:

- record invalidation or stale-summary marking, then later resummarization

Approximate families:

- add to mutable overlay, tombstone old candidate, later rebuild or compact published generation

The platform should not force a single mutation style across all families. It should force only the publication and visibility guarantees.

### 15.6 Unique Conflict Handling

Unique-capable families must share one conflict policy:

1. acquire key-level conflict serialization primitive
2. inspect candidate conflicts through family lookup
3. heap-check MGA visibility
4. wait or restart if conflicting version belongs to another active transaction
5. retain conflict serialization until commit or abort

This policy must be defined at platform level even if B-tree is the first exact implementation to use it robustly.

## 16. Index Cleanup and GC

### 16.1 Dead Entry Lifecycle

The hardened platform should define one dead-entry state machine:

1. live candidate
2. logically dead but still horizon-visible to some readers
3. reclaim-eligible at exact-entry level
4. physically removed from active structure
5. page/component generation retired and awaiting reclaim
6. allocator- or storage-reusable

Families may skip or combine phases internally, but they may not bypass the visibility and horizon rules.

### 16.2 Exact Families

Exact families must delete only the entries or postings proven dead. This includes:

- B-tree leaf TIDs or posting continuations
- hash bucket entries
- exact posting lists in GIN or similar structures

Exact dead-entry deletion must be safe under active readers and must not require whole-family rebuild as a normal maintenance path.

### 16.3 Summary Families

Summary families such as BRIN and zonemap structures should not pretend to support exact dead-entry deletion. Their maintenance contract is:

- mark summary stale or invalidated
- resummarize from authoritative storage
- rebuild if stale debt grows too large

### 16.4 Approximate Families

Approximate families may soft-delete and defer heavy repair, but they must expose:

- stale-delete backlog
- rebuild threshold
- last rebuild age
- recall quality indicators or proxies

An approximate family that cannot explain its stale-delete debt is not production-ready.

### 16.5 Compaction Rules

Compaction should exist at three levels:

- page-local compaction for exact page-managed families
- container or posting compaction for bitmap and inverted families
- generation merge or rebuild for storage-attached and approximate families

Compaction may occur before split, after GC, or on maintenance cadence, but it must remain subordinate to visibility truth and reader-horizon safety.

### 16.6 Rebuild Triggers

Rebuild should be mandatory or strongly recommended when one of the following holds:

- validation failure
- unrecoverable structural inconsistency
- excessive dead-version density with poor reclaimability
- ANN stale-delete backlog beyond threshold
- summary false-positive density beyond threshold
- format upgrade or incompatible configuration change

Rebuild should be a first-class operator-visible decision, not an implementation shame path.

## 17. Observability

### 17.1 Platform-Wide Metrics

ScratchBird should expose a uniform index telemetry vocabulary. Required cross-family metrics include:

- `index_generation_active`
- `index_generation_retired_pending_reclaim`
- `index_build_state`
- `index_last_validation_epoch`
- `index_validation_failure_count`
- `index_visibility_discard_count`
- `index_dead_entry_backlog`
- `index_rebuild_fallback_count`
- `index_corruption_detected_count`

### 17.2 Exact Tree Metrics

Required B-tree and exact-tree metrics:

- split count by level
- merge candidate count
- right-link chase count
- split retry count
- dead-version density
- compact-before-split count
- duplicate pressure
- latch wait count and wait time by level
- pages pending reclaim quarantine

Hash adds:

- bucket split count
- overflow chain depth
- hot-bucket collision count

### 17.3 Posting and Bitmap Metrics

Required GIN/inverted/bitmap metrics:

- pending-list length
- posting-tree depth
- dictionary load failures
- parse or tokenization error count
- bitmap container count and density
- stale posting backlog

### 17.4 Summary Metrics

Required BRIN/zonemap and columnstore-summary metrics:

- unsummarized range count
- stale range count
- false-positive density
- average pages pruned per probe
- row-group stale-summary count

### 17.5 ANN Metrics

Required vector metrics:

- deleted-node backlog
- active graph generation age
- build or rebuild latency
- candidate recheck discard count
- graph validation failures
- recall proxy or quality warning threshold crossings

### 17.6 Operator Events

Metrics are necessary but insufficient. The platform also needs structured events:

- generation published
- generation retired
- rebuild started, validated, failed, aborted, completed
- validator detected corruption
- restart repaired unfinished structural operation
- reclaim blocked by reader horizon
- ANN quality threshold exceeded

## 18. Validation and Repair Framework

### 18.1 Validation Modes

Every family should support four validation modes:

1. open-time sanity check
2. light online health scan
3. deep offline validator
4. pre-publication validator for rebuild/cutover

The same family may use increasingly expensive proofs at each level, but all four levels should exist.

### 18.2 Corruption Classes

The framework should classify corruption explicitly:

- page/header/checksum corruption
- parent/child linkage corruption
- sibling or right-link corruption
- summary-manifest inconsistency
- posting-tree or dictionary corruption
- ANN graph serialization or connectivity corruption
- format-version mismatch

This classification should drive both operator messaging and automatic repair policy.

### 18.3 Repair Versus Rebuild

Repair is appropriate when:

- the family has a bounded structural-intent or manifest repair path
- restart can prove one safe legal state
- damage is localized and does not undermine semantic trust

Rebuild is mandatory when:

- validator cannot prove a single safe legal state
- corruption touches large portions of exact-tree ordering or ANN graph connectivity
- format-version upgrade requires reinterpretation of bytes
- summary state is too stale or inconsistent to trust incrementally

### 18.4 Validator Outputs

Validator outputs should include:

- family and physical generation ID
- pages or components scanned
- issues by severity
- repairability assessment
- recommended action
- timestamps and validation epoch

These outputs should be storable as evidence for promotion gates and operational audits.

### 18.5 Repair Workflow

Recommended workflow:

1. light validator detects anomaly
2. deep validator confirms and classifies it
3. engine marks generation degraded or invalid depending on severity
4. operator chooses repair or rebuild if not automatic
5. shadow generation built and validated
6. publish new generation and retire old one

The key design point is that validation and repair are not ad hoc admin scripts. They are part of the production index subsystem.

## 19. Testing Strategy

### 19.1 Current Test Evidence in the Repository

ScratchBird already has meaningful index-related tests. Examples include:

- B-tree unit tests for compression, iterator behavior, MGA compliance, GC, rightmost-child logic, and delete-parent updates
- integration tests for BRIN, bitmap, GIN, GiST, HNSW, R-tree, SP-GiST, multi-index MGA behavior, shadow rebuild, columnstore, and LSM integration
- contract tests for index factory registry, index page layout contracts, index runtime GC contracts, and corruption error surfaces
- stress tests for columnstore and LSM workloads

This is important because it means the platform is not starting from zero. The missing piece is not “have tests.” The missing piece is “organize tests into hardening gates that match the intended durability and MGA model.”

### 19.2 Required Test Layers

The hardened index program should use the following layers.

Unit and property tests:

- binary layout serialization and versioning
- key encoding, tokenization, geometry normalization
- per-family visibility edge cases
- separator, split-point, duplicate, and compaction decisions

Contract tests:

- catalog capability registry truth
- family exact/lossy/approximate declarations
- validator result schema
- observability metric schema stability

Concurrency tests:

- reader/writer overlap on hot exact-tree paths
- shadow build plus concurrent DML
- summary invalidation under concurrent scan
- ANN query plus delta update overlap

Crash/restart tests:

- failpoint at every structural-publication phase
- interrupted build, cutover, compaction, manifest publish, and reclaim
- startup repair path validation

Fuzz and adversarial corpus:

- corrupted pages and manifests
- malformed text and geometry inputs
- ANN serialization and graph corruption
- operator-class contract fuzzing

Stress and soak:

- duplicate-heavy exact-tree churn
- delete-heavy text posting churn
- ANN stale-delete accumulation
- LSM tombstone storms
- long-running mixed workloads with GC and maintenance enabled

Performance and benchmark:

- point lookup latency under churn
- range scan stability and cost
- summary false-positive rate and pruning benefit
- ANN recall/latency tradeoff under maintenance debt
- rebuild throughput and cutover latency

### 19.3 Promotion Gates

Each hardened family should ship only after satisfying gate evidence:

- correctness gate
- crash-safety gate
- validator/repair gate
- observability gate
- workload-specific stress gate

For B-tree specifically, the section 31 `T31-G15-*` evidence model from the canonical workplan should become the first fully enforced gate set.

### 19.4 Required Tooling

The test program needs supporting tooling:

- deterministic failpoint injection
- reproducible crash harness
- page/component corruption injector
- validator evidence packager
- long-run soak metric snapshotter

## 20. Implementation Roadmap

### 20.1 Phase 0: Specification Hygiene and Taxonomy Cleanup

Objectives:

- resolve B-tree spec precedence explicitly
- mark reverse-engineered in-repo docs as non-authoritative where they diverge
- resolve `FULLTEXT` runtime mapping ambiguity
- identify and deprecate or align the older non-canonical `LSMTree` visibility logic
- define a capability vocabulary for exact, lossy, approximate, summary, and storage-attached families

Exit criteria:

- one published evidence-precedence note
- no unresolved contradiction on B-tree control-plane rules
- no ambiguous logical-to-physical family mapping for production-exposed types

### 20.2 Phase 1: Platform Control Plane Hardening

Objectives:

- extend catalog and runtime capability reporting
- ensure planner and executor can request version-visible physical generations
- define validator, metrics, and maintenance interfaces
- define family control-page or manifest requirements

Exit criteria:

- version-aware generation selection wired through planning and execution
- metrics schema frozen
- validator interfaces agreed

### 20.3 Phase 2: B-tree Structural Foundation

Objectives:

- metapage implementation
- SMO intent records
- restart repair
- split-tolerant descent and latch protocol skeleton
- reclaim quarantine

Exit criteria:

- B-tree restart repair report passes
- no immediate free after merge
- root publication is metapage-driven

### 20.4 Phase 3: B-tree Search, Churn, and Build Hardening

Objectives:

- compressed-page bounded search
- separator rigor
- duplicate-pressure and compact-before-split policy
- build/rebuild states and cutover
- validator and observability completion

Exit criteria:

- B-tree section 31 hardening gates pass
- operator metrics stable
- contradiction register closed

### 20.5 Phase 4: Exact and Generalized Tree Roll-Down

Objectives:

- hash metapage and split durability
- GIN transactional posting-tree hardening
- GiST/SP-GiST operator-class and validator hardening
- R-tree spatial normalization and validator hardening

Exit criteria:

- each family has control-page or manifest publication
- each family has deep validator
- each family advertises exact/lossy semantics accurately

### 20.6 Phase 5: Summary and Storage-Attached Families

Objectives:

- BRIN/zonemap resummarization model
- bitmap delta plus compaction model
- columnstore segment publication and delete-vector semantics
- summary/segment observability

Exit criteria:

- no summary family depends on blind dead-entry deletion
- false-positive/staleness metrics are surfaced

### 20.7 Phase 6: ANN and Write-Optimized Families

Objectives:

- HNSW published-generation plus delta overlay
- graph validator and backlog thresholds
- canonical LSM manifest and compaction publication
- honest aliasing policy for ART and trie

Exit criteria:

- vector logical families accurately declare runtime truth
- LSM visibility paths fully canonicalized
- ANN rebuild thresholds and quality metrics are in place

### 20.8 Phase 7: Optimizer and Operational Integration

Objectives:

- make optimizer maintenance-state aware
- surface degraded, invalid, stale, and rebuilding states cleanly
- complete operator command and evidence packaging

Exit criteria:

- planner avoids stale or degraded families when appropriate
- repair/rebuild workflows are operator-complete

## 21. Risk Analysis

### 21.1 Corruption Risks

Premature reclaim:

- exact-tree pages or ANN generations reused before all readers are past them
- mitigation: quarantine plus reader-horizon accounting

Ambiguous publication:

- root or manifest update partially durable
- mitigation: metapage/manifest publication sequence and intent records

Format drift:

- bytes interpreted under incompatible spec revisions
- mitigation: explicit format versioning and rebuild requirements

Logical/physical mismatch:

- catalog claims a family semantics that the runtime does not actually implement
- mitigation: honest capability registry and alias policy

### 21.2 Concurrency Risks

Page-lock misuse:

- lock manager used where lightweight latches are required
- mitigation: explicit latch subsystem or family-local latch surfaces

Split/scan false negatives:

- readers descend stale parent paths and do not chase rightward safely
- mitigation: fence/high-key/right-link contract

Unique race anomalies:

- active conflicting versions not serialized consistently
- mitigation: platform-wide key conflict protocol

### 21.3 Operational Risks

Invisible maintenance debt:

- dead-entry backlog, stale summaries, ANN stale deletes, or compaction debt accumulate without operator awareness
- mitigation: required metrics and alerts

Rebuild storms:

- families trigger rebuild too often or with poor prioritization
- mitigation: threshold-based policy and scheduling controls

Planner over-trust:

- optimizer chooses degraded or stale families too aggressively
- mitigation: quality and maintenance-state aware costing

### 21.4 Program Risks

Over-specification before platform cleanup:

- producing detailed family specs before resolving control-plane contradictions will create churn
- mitigation: lock specification precedence first

Alias debt:

- too many logical families promise differentiated behavior while sharing one runtime
- mitigation: honest semantics until distinct implementations exist

## 22. Final Architecture Recommendation

ScratchBird should harden its index subsystem as one platform with three physical family classes and one shared control plane.

Shared control plane:

- catalog logical-versus-physical generation model
- family capability registry
- validator and repair framework
- unified observability vocabulary
- generation visibility selection for planner and executor

Exact mutable access methods:

- B-tree as the gold-standard exact tree
- hash, GIN transactional postings, GiST, SP-GiST, and R-tree adopting the same publication, validation, and reclaim discipline with family-specific physical rules

Storage-attached and summary methods:

- BRIN/zonemap, bitmap, columnstore, storage-attached inverted, and LSM manifests using immutable published generations plus mutable deltas or stale markers

Approximate/vector methods:

- published ANN generations plus mutable delete/insert overlays, always followed by heap visibility checks

The first detailed specifications to write after this report should therefore be:

1. platform capability and visibility contract
2. B-tree metapage and structural publication contract
3. B-tree validator and observability contract
4. generic rebuild/cutover and retired-generation GC contract
5. family-specific roll-down specs in the order exact trees, posting families, summaries, ANN, then write-optimized structures

The core recommendation is not to chase breadth first. Breadth already exists in the catalog and in partial implementations. The correct next move is to turn ScratchBird’s current index assortment into a truthful, observable, restart-repairable, MGA-safe platform where each supported family has an explicit maturity contract and no family is allowed to hide behind vague aliasing or undocumented maintenance behavior.

## Appendix A. Repository Structure Summary

Runtime-critical directories:

- `ScratchBird/include/scratchbird/core`
- `ScratchBird/src/core`
- `ScratchBird/src/catalog`
- `ScratchBird/src/index`
- `ScratchBird/src/optimizer`
- `ScratchBird/src/sblr`
- `ScratchBird/tests/unit`
- `ScratchBird/tests/integration`
- `ScratchBird/tests/stress`

Documentation and specification directories:

- `ScratchBird/docs/documentation`
- `ScratchBird/docs/specifications_old`
- `docs/specifications/18_Index_Framework`
- `docs/specifications/work`

## Appendix B. Runtime Family Inventory and Current Status

B-tree:

- implemented and featureful
- not yet structurally hardened

Hash:

- implemented
- needs split/publication/validator hardening

GIN:

- implemented
- needs pending-list/publication/validator hardening

GiST and SP-GiST:

- implemented
- need operator-class and structural-hardening pass

BRIN and zonemap:

- implemented via BRIN runtime
- need summary lifecycle hardening

Bitmap:

- implemented
- need update/debt/compaction hardening

R-tree:

- implemented
- needs spatial validator and publication hardening

HNSW and vector aliases:

- implemented as HNSW-centric runtime
- need honest capability reporting and ANN generation model

Columnstore:

- implemented
- needs segment publication and delete-vector hardening

LSM:

- implemented, but older helper logic must be aligned or retired

ART and trie:

- catalog-visible aliases only today
- should not receive standalone production specs until true runtimes exist

## Appendix C. B-tree Discrepancy Resolution Rules

Use the following precedence when writing downstream B-tree specs:

1. companion hardening specs override `BTREE_SPEC.md` in protocol detail
2. `BTREE_SPEC.md` remains baseline vocabulary and layout intent
3. live code is evidence of current behavior, not final authority
4. reverse-engineered in-repo docs are historical references only

Items that must be resolved before implementation-spec freeze:

- final page-layout/versioning story
- metapage fields
- fence/high-key representation
- SMO intent record format
- merge quarantine rules
- compressed-page search method
- validator scope and output schema

## Appendix D. Existing Test Surface and Missing Gates

Observed existing test surface includes:

- `test_btree_*`
- `test_hash_index*`
- `test_gin_*`
- `test_brin_*`
- `test_bitmap_*`
- `test_hnsw_*`
- `test_rtree_*`
- `test_spgist_*`
- `test_shadow_index_rebuild.cpp`
- `test_index_gc_runtime_contracts.cpp`
- `test_index_page_*`

Missing or insufficiently explicit gates include:

- deterministic SMO crash harness
- restart repair certification
- family-deep validator gate
- ANN stale-delete quality gate
- summary stale-density gate
- honest alias-capability contract gate

## Appendix E. Donor Borrow and Avoid Matrix

Borrow:

- Firebird: heap-led MGA cleanup and strong validation mindset
- PostgreSQL: AM contracts, validators, BRIN/GIN/GiST discipline
- MariaDB/InnoDB and MySQL/InnoDB: publication barriers and online build thinking
- Cassandra, ClickHouse, DuckDB: immutable component publication and validation
- Milvus, Neo4j, Redis: ANN publication, validation, and delayed reclaim
- MongoDB: shadow build plus delta-drain barrier pattern
- OpenSearch: corruption tooling and strict input normalization

Avoid:

- general WAL adoption as Alpha recovery truth
- Lucene refresh/translog visibility semantics
- oplog-side-write machinery as literal implementation
- object-lock and module-persistence assumptions from Redis

## Appendix F. Detailed Index Implementation Map

This appendix records the current implementation footprint in a specification-friendly format. It is intentionally descriptive rather than normative.

### F.1 B-tree

Primary files:

- `include/scratchbird/core/btree.h`
- `include/scratchbird/core/btree_page.h`
- `src/core/btree.cpp`
- `src/core/btree_page.cpp`
- `src/core/btree_iterator.cpp`

Core classes and structs:

- `BTree`
- `BTreeIterator`
- `SBBTreePage`
- `SBBTreeNode`
- `SBBTreeIndex`

Physical format notes:

- custom page header embedded directly in page body
- page-level UUIDs, level, flags, sibling pointers, parent pointer, rightmost child
- node-level prefix compression and `xmin/xmax`

Mutation path:

- `StorageEngine` routes B-tree-like logical types to `BTree::insert` and delete/mark-delete flows
- splits, parent insertion, root creation, and bulk load are implemented inside the family

GC integration:

- `BTree` implements `IndexGCInterface`
- dead-entry cleanup through `removeDeadEntries()` and `gcCompact()`

Rebuild status:

- bottom-up bulk load exists
- shadow build metadata exists in catalog
- canonical hardened build/publish protocol not yet implemented physically

### F.2 Hash

Primary files:

- `include/scratchbird/core/hash_index.h`
- `src/core/hash_index.cpp`

Core classes and structs:

- `HashIndex`
- `SBHashIndexMetaPage`
- `SBHashDirectoryPage`
- `SBHashBucketPage`
- `HashEntry`

Physical format notes:

- explicit metapage plus directory and bucket pages
- per-entry hash, TID, and MGA fields

Mutation path:

- `StorageEngine` routes hash-like logical types to `HashIndex::insert` and remove paths
- bucket splitting exists

GC integration:

- implements `IndexGCInterface`
- exact dead-entry delete expected

Rebuild status:

- generic shadow rebuild available through catalog
- family-specific split/publication hardening not yet formalized

### F.3 GIN

Primary files:

- `include/scratchbird/core/gin_index.h`
- `src/core/gin_index.cpp`
- `src/core/gin_compression.cpp`
- `src/core/gin_tsvector_ops.cpp`
- `src/sblr/gin_extractors.cpp`

Core classes and structs:

- `GinIndex`
- `SBGinPendingListPage`
- `GinPendingEntry`
- `GinPostingEntry`
- `SBGinPostingListPage`

Physical format notes:

- page-managed pending list and posting structures
- token extraction and posting-tree logic already present
- MGA state exists in pending/posting records

Mutation path:

- `StorageEngine` routes `GIN` directly here
- full-text token extraction is delegated through extractor helpers

GC integration:

- implements `IndexGCInterface`
- exact posting delete or posting-list cleanup expected

Rebuild status:

- generic catalog shadowing available
- dedicated posting-tree validation and publication model still needs hardening

### F.4 GiST

Primary files:

- `include/scratchbird/core/gist_index.h`
- `include/scratchbird/core/gist_box_ops.h`
- `src/core/gist_index.cpp`

Core classes and structs:

- `GiSTIndex`
- `GiSTPredicate`
- `GiSTOperatorClass`
- `SBGiSTPage`
- `SBGiSTEntry`

Physical format notes:

- generalized predicate pages and entries
- extensible operator-class design

Mutation path:

- `StorageEngine` builds `GiSTPredicate` and routes insert/remove/search to family

GC integration:

- implements `IndexGCInterface`

Rebuild status:

- catalog-driven rebuild possible in principle
- operator-class validation/publication hardening incomplete

### F.5 SP-GiST

Primary files:

- `include/scratchbird/core/spgist_index.h`
- `include/scratchbird/core/spgist_quad_ops.h`
- `include/scratchbird/core/spgist_text_ops.h`
- `src/core/spgist_index.cpp`

Core classes and structs:

- `SPGiSTIndex`
- `SPGiSTOperatorClass`
- `SBSPGiSTPage`
- `SBSPGiSTInnerTuple`
- `SBSPGiSTLeafTuple`

Physical format notes:

- partitioned tree structure with inner and leaf tuples

Mutation path:

- `StorageEngine` routes `SPGIST` inserts and deletes directly

GC integration:

- implements `IndexGCInterface`

Rebuild status:

- generic shadow rebuild available
- redirect and route validator semantics still need specification

### F.6 BRIN and Zonemap

Primary files:

- `include/scratchbird/core/brin_index.h`
- `include/scratchbird/core/brin_minmax_ops.h`
- `src/core/brin_index.cpp`

Core classes and structs:

- `BrinIndex`
- `SBBrinPage`
- `SBBrinRange`
- `SBBrinIndex`

Physical format notes:

- page-managed summary ranges and revmap behavior
- `ZONEMAP` and `BLOOM` logical aliases route here

Mutation path:

- `StorageEngine` inserts block-number-aware range summaries

GC integration:

- implements `IndexGCInterface`
- summary maintenance rather than exact dead-entry delete should be canonical

Rebuild status:

- generic shadow rebuild available
- resummarization and stale-summary state need stronger contract

### F.7 Bitmap

Primary files:

- `include/scratchbird/core/bitmap_index.h`
- `include/scratchbird/index/bitmap_rle.h`
- `src/core/bitmap_index.cpp`

Core classes and structs:

- `BitmapIndex`
- `RoaringBitmap`
- `BitmapIndexScanner`
- `SBBitmapIndexMetaPage`
- `SBBitmapDictionaryPage`
- `SBRoaringBitmapRootPage`
- `SBRoaringContainerPage`
- `VersionedBitmapEntry`

Physical format notes:

- compressed bitmap containers and dictionary structure
- versioned entry support indicates MGA-aware intent

Mutation path:

- `StorageEngine` routes low-cardinality logical types here

GC integration:

- implements `IndexGCInterface`

Rebuild status:

- generic shadow rebuild available
- delta/compaction semantics need formal hardening

### F.8 R-tree

Primary files:

- `include/scratchbird/core/rtree.h`
- `include/scratchbird/core/rtree_node.h`
- `include/scratchbird/core/rtree_index.h`
- `src/core/rtree.cpp`
- `src/core/rtree_index.cpp`

Core classes and structs:

- `RTree`
- `RTreeIndex`
- `RTreeNode`
- `BoundingBox`
- `SBRTreePage`
- `SBRTreeEntry`

Physical format notes:

- bounding-box pages and entries with reinsertion and condense support

Mutation path:

- `StorageEngine` routes native spatial and Mongo/Redis geo aliases here

GC integration:

- implements `IndexGCInterface`

Rebuild status:

- generic catalog shadowing exists
- spatial validator and publication discipline still need hardening

### F.9 HNSW and Vector Family

Primary files:

- `include/scratchbird/core/hnsw_index.h`
- `include/scratchbird/core/vector.h`
- `include/scratchbird/index/vector_quantization.h`
- `src/core/hnsw_index.cpp`

Core classes and structs:

- `HnswIndex`
- `SBHnswPage`
- `SBHnswNode`
- `HnswNeighbor`
- `SBHnswIndex`

Physical format notes:

- page-managed graph-like structure with node version fields
- logical vector taxonomy currently collapses to this runtime

Mutation path:

- `StorageEngine` decodes vector payloads and routes many logical vector types here

GC integration:

- implements `IndexGCInterface`

Rebuild status:

- shadow rebuild possible at catalog level
- graph-generation publication and stale-delete repair remain to be hardened

### F.10 Inverted and Full-text Wrapper

Primary files:

- `include/scratchbird/core/inverted_index.h`
- `src/core/inverted_index.cpp`
- `include/scratchbird/core/fulltext_index.h`
- `src/core/fulltext_index.cpp`

Core classes and structs:

- `InvertedIndex`
- `SBInvertedIndexMetaPage`
- `SBInvertedIndexSegmentMeta`
- `SBTermDictionaryPage`
- `SBPostingListPage`
- `SBDocumentStatsPage`
- `FullTextIndex`

Physical format notes:

- standalone inverted metadata and posting structures
- wrapper class exists above this family

Mutation path:

- `StorageEngine` routes `FULLTEXT`, `INVERTED`, wildcard and sparse text-like aliases here today

GC integration:

- `InvertedIndex` implements `IndexGCInterface`

Rebuild status:

- catalog shadowing available
- logical-to-physical mapping must be clarified first

### F.11 Columnstore

Primary files:

- `include/scratchbird/core/columnstore.h`
- `include/scratchbird/core/columnstore_index.h`
- `include/scratchbird/index/columnstore_enhanced.h`
- `src/core/columnstore.cpp`

Core classes and structs:

- `ColumnstoreIndex`
- `SBColumnstoreMetadataPage`
- `SBColumnstorePage`
- `ColumnSegment`
- `ColumnScanIterator`

Physical format notes:

- segmented columnar layout with metadata pages and row-version fields

Mutation path:

- `StorageEngine` routes columnstore-maintained logical types here

GC integration:

- implements `IndexGCInterface`

Rebuild status:

- generic shadowing available
- segment publication and delete-vector discipline still need explicit specification

### F.12 LSM

Primary files:

- `include/scratchbird/core/lsm_tree_index.h`
- `include/scratchbird/core/lsm_tree.h`
- `include/scratchbird/core/lsm_bloom_filter.h`
- `src/core/lsm_tree_index.cpp`
- `src/core/lsm_tree_components.cpp`
- `src/core/lsm_tree.cpp`

Core classes and structs:

- `LSMTreeIndex`
- `Memtable`
- `MemtableEntry`
- `SSTableWriter`
- `SSTableReader`
- `LSMBloomFilter`

Physical format notes:

- file-based memtable and SSTable design
- older helper code path still exists and must be harmonized

Mutation path:

- `StorageEngine` routes `LSM` logical type here

GC integration:

- `LSMTreeIndex` implements `IndexGCInterface`

Rebuild status:

- flush and compaction exist
- manifest/publication discipline needs formal hardening

## Appendix G. Shared Infrastructure Matrix

### G.1 Transaction Visibility and MGA

Current shared component:

- `TransactionManager`

Observed role:

- canonical visibility checks
- current-XID and snapshot support
- horizon queries used by GC and rebuild retirement

Current maturity issue:

- some families still rely on simplified calling conventions or older helper logic

Specification direction:

- one canonical family-facing visibility API
- no raw visibility arithmetic in family code

### G.2 Buffer Pool and Page Manager

Current shared components:

- `BufferPool`
- `PageManager`

Observed role:

- page pin/unpin
- page allocation/freeing
- tablespace-aware GPID handling

Current maturity issue:

- family publication and reclaim semantics are stronger than raw page free/pin primitives

Specification direction:

- retain buffer/page primitives
- add family-level reclaim quarantine and publication metadata rather than overloading allocator behavior

### G.3 Lock Manager

Current shared component:

- `LockManager`

Observed role:

- page locks
- maintenance and DDL synchronization

Current maturity issue:

- exact-tree implementations currently rely on lock-manager primitives where page or structure latches should exist

Specification direction:

- separate latches from transaction/DDL locks

### G.4 Catalog Manager

Current shared component:

- `CatalogManager`

Observed role:

- logical and physical index identity
- shadow rebuild state
- index object cache
- generation retirement horizon

Current maturity issue:

- planner/runtime not yet uniformly using version-visible generation selection

Specification direction:

- make catalog generation visibility the only supported control-plane truth

### G.5 Index Factory

Current shared component:

- `IndexFactory`

Observed role:

- family registration
- runtime class aliasing
- create/open/close dispatch

Current maturity issue:

- logical aliasing is broader than honest runtime differentiation

Specification direction:

- capability registry must declare exactness, approximate behavior, storage model, validator support, and rebuild semantics explicitly

### G.6 Storage Engine

Current shared component:

- `StorageEngine`

Observed role:

- DML to index dispatch
- key extraction integration
- vector decoding and family-specific data preparation

Current maturity issue:

- broad type switches are difficult to harden unless family capabilities are standardized

Specification direction:

- keep dispatch centralized, but drive it through capability metadata and narrower family interfaces

### G.7 Garbage Collector and Sweep

Current shared components:

- `GarbageCollector`
- `SweepManager`
- `tip_compaction`

Observed role:

- heap-first dead tuple discovery
- index cleanup callbacks

Current maturity issue:

- sweep/reclaim components remain partially stubbed and need stronger horizon and maintenance-state reporting

Specification direction:

- make heap dead-TID emission precise
- make index cleanup results measurable
- make reclaim block reasons observable

### G.8 Diagnostics and Tests

Current shared components:

- generic index page diagnostics
- contract tests
- family tests

Observed role:

- basic structural checking
- API and taxonomy contract enforcement

Current maturity issue:

- no single deep validator framework spans exact, summary, and approximate families

Specification direction:

- shared validator result schema and family-specific deep validators

## Appendix H. B-tree Contradiction Cleanup Agenda

The B-tree detailed-spec effort should begin by closing the following contradictions explicitly.

### H.1 Root Publication

Baseline or older behavior:

- root tracked through catalog `root_gpid` and live runtime state

Canonical override:

- B-tree metapage is authoritative

Current code:

- opens from catalog root page and keeps root in `SBBTreeIndex`

Resolution:

- define metapage as root truth
- catalog stores entry point to metapage or validated published root metadata, not the entire publication story

### H.2 Structural Durability Mechanism

Baseline or older behavior:

- ordered page writes and in-memory operation sequencing

Canonical override:

- bounded SMO intents plus restart repair

Current code:

- no SMO intent records

Resolution:

- treat current code as pre-hardening only
- detailed spec must define intent record binary format and restart state machine

### H.3 Search Semantics on Split Pages

Baseline or older behavior:

- traditional descent through internal nodes

Canonical override:

- fence/high-key/right-link split-tolerant descent

Current code:

- sibling pointers exist but no hardened high-key contract

Resolution:

- final spec must make right-link chase mandatory when high-key says target is to the right

### H.4 Compressed Search Method

Baseline or older behavior:

- logical binary search wording in `BTREE_SPEC.md`

Canonical override:

- bounded decode or restart-anchor search

Current code:

- linear reconstruct-and-compare through compressed keys

Resolution:

- final spec must describe one bounded compressed-search method and require metrics proving it

### H.5 Internal-Node Child Pointer Semantics

Baseline or older behavior:

- `leftmost_child_page_id` in header and per-node child semantics described differently

Companion hardening direction:

- separator and pivot rules must preserve exact fence truth

Current code:

- per-node left child plus page-header `btr_rightmost_child`

Resolution:

- downstream specs must pick one exact internal-node representation and define rebuild/upgrade impact explicitly

### H.6 High-Key Representation

Baseline or older behavior:

- high key in special area

Current code:

- no explicit high-key field stored as canonical search truth

Resolution:

- detailed spec must define where high key lives physically, how it is validated, and how it participates in split publication

### H.7 Merge and Reclamation

Baseline or older behavior:

- merge when underfilled, update parent, continue

Canonical override:

- conservative deletion and reclaim quarantine

Current code:

- merged right page is freed promptly

Resolution:

- exact reclaim quarantine protocol must be written before merge code is considered hardened

### H.8 Bulk Build and Rebuild

Baseline or older behavior:

- offline bulk build narrative

Canonical override:

- build states, validation, publish, restart behavior

Current code:

- bottom-up build exists but without state machine or metapage publish

Resolution:

- detailed spec must define build-control records and publish criteria

### H.9 Duplicate Handling

Baseline or older behavior:

- duplicates stored together

Canonical override:

- duplicate-heavy workloads and posting-list management are first-class

Current code:

- multi-TID leaf entry support exists, but full duplicate continuation/split policy does not

Resolution:

- downstream spec must define duplicate run boundaries, overflow/posting continuation policy, and metrics

### H.10 Page Layout Finality

Baseline or older behavior:

- older docs imply a byte-accurate layout

Current code:

- concrete `SBBTreePage` and `SBBTreeNode` layout differs from older docs

Resolution:

- final detailed spec must state whether current layout is preserved with extensions or replaced under a new format version

### H.11 Validation Scope

Baseline or older behavior:

- corruption handling described generically

Canonical override:

- validator and hardening framework is mandatory

Current code:

- partial tests and generic diagnostics, but no full validator

Resolution:

- final detailed spec must enumerate invariants, validator output schema, and corruption classes

### H.12 Observability

Baseline or older behavior:

- metrics only lightly implied

Canonical override:

- observability and gates are mandatory deliverables

Current code:

- no complete B-tree operator metrics surface yet

Resolution:

- detailed spec must define metric names, update points, and alert thresholds alongside the algorithm spec, not after it

