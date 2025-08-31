# Phase 2 — Space Management and Allocation ✅ **COMPLETED**

Status: **FULLY IMPLEMENTED**

## Goals (high-level spec → concrete plan)

- Implement global space management primitives: PIP, TIP, Space Catalog, and Extents.
- Provide deterministic page growth and multi-segment expansion via `FileMap`.
- Introduce free page maps and basic free-space accounting for reclaim on drop/truncate.
- Integrate the Allocator with PIP (and seed TIP), resilient to crashes within Phase 2 constraints.
- Tablespace placement scaffolding: ability to choose target space for relations (minimal routing only).
- Exit: deterministic growth; reclaim on drop/truncate; allocator soak tests clean.

## Non-goals (deferred to later phases)

- Full MVCC/TIP semantics (Phase 3) beyond initial TIP seeding.
- Vacuum and global Free Space Map policy (Phase 15) beyond simple reclaim.
- Full WAL/recovery for allocator (later phases); Phase 2 targets minimal crash resilience.
- Update/delete/HOT chains (Phases 3/7).

---

## ODS and on-disk structures (extensions)

1) Page Types (confirm/reuse from Phase 1)
- `PageType::Pip` — Pointer/Free Page Map
- `PageType::Tip` — Transaction Inventory Page (seed only in Phase 2)
- `PageType::SpaceCatalog` — records per-space metadata (page size, segment count, extents)
2) PIP layout
- Fixed page-size–aware bitmap. Each bit corresponds to a logical page in the same space.
- Helpers exist in `ods.h`: `pagesPerPIP(page_size)`, `bytesBitPIP(page_size)`.
- Bit convention: 0 = free/unused, 1 = allocated (or reserved). Consider a small header region for PIP metadata (version, space_id, first_page_covered).
- PIP chain: space has one or more PIP pages covering increasing logical page ranges; `PageHeader::next` can link PIPs in ascending coverage.
3) TIP layout (Phase 2 seed)
- `transPerTIP(page_size)` helper exists. We will seed TIP page 0 with all-zero (idle) transactions up to a bootstrap bound.
- No MVCC enforcement in Phase 2; TIP primarily exists to establish layout and page reservation.
4) Space Catalog
- Global registry in page space 1 describing:
  - page size
  - PIP root page(s)
  - TIP root page(s)
  - segment count and growth policy
  - next extent id (monotonic)
- Stored in one or more `SpaceCatalog` pages. A small index (linear or fixed slot) is acceptable for Phase 2.
5) Extents
- Extent = N contiguous pages (default 8) for locality. Track extent id → first page.
- PIP manages per-page allocation; the Space Catalog tracks the extent boundaries for auditing and tooling.
6) Tablespaces (scaffold)
- Support `space_id` > 1 with a separate `FileMap` root (subdirectory) mapping.
- Phase 2: allow relation creation to specify `space_id` (default 1). No online moves.

---

## Allocator design

Responsibilities

- Allocate/free logical pages in a given `space_id`.
- Maintain/consult PIP to find free pages; grow file segments deterministically.
- Optionally prefetch and batch-allocate extents for faster insert-heavy workloads.

Interfaces (C++)

- `class Allocator` (new):
  - `Allocator(FileMap* fmap, std::uint16_t space_id, std::uint32_t page_size)`
  - `std::uint32_t allocate_page(PageType type)` — returns logical page number
  - `void free_page(std::uint32_t page_no)` — marks page free; clears header minimally
  - `std::uint32_t allocate_extent(std::size_t num_pages)` — returns first page of contiguous block
  - `void persist_pip()` — flushes modified PIP pages
  - Crash safety note: Page body is written first, then PIP bit set (idempotent on crash).

Internal behavior

- Keeps an in-memory cursor to current PIP coverage; scans for a zero bit.
- On exhaustion, allocates a new segment if needed (`FileMap::ensure_capacity`) and adds a new PIP page.
- Deterministic growth policy: allocate by ascending page number within space; allocate extents aligned on extent boundaries.
- `free_page`: clear slot directory or mark header zeroed; reset PIP bit → 0.

Crash resilience (Phase 2 scope)

- Allocation order: write page header/body → set PIP bit → write page checksum.
- On crash leaving page initialized but PIP unset: page considered free; safe to reuse.
- On crash leaving PIP set but page garbage: `heap_check/page_dump` will catch; later WAL phases will harden this path. In Phase 2 tests, ensure allocator can recover by simple re-initialization when claimed-but-empty is detected.

---

## Free space tracking

Per-page free bytes

- Already computed via `HeapPageCodec::free_bytes` for heap data pages.
- Phase 2 adds a coarse per-page free-space class recorded in an in-memory map updated on insert/remove (remove/HOT deferred to Phase 3/7). For Phase 2, expose hooks but only rely on it for page selection hints.

Global Free Space Map (FSM)

- Defer full FSM persistence to Phase 15.
- Phase 2: simple in-memory hint map keyed by page_no with periodic recompute in tools.

Reclaim on drop/truncate

- Drop/truncate will iterate relation pages and call `Allocator::free_page` for heap data and overflow pages; HeapRoot is freed last.
- Tests verify PIP bits cleared and capacity reclaimed (next allocation reuses freed ranges deterministically).

---

## Multi-segment growth & tablespace placement

Multi-segment growth

- Use existing `FileMap::ensure_capacity` to add segments when PIP needs to cover higher logical pages.
- Each new segment is preallocated to `pages_per_segment * page_size` for deterministic IO layout (respecting FileOptions).

Tablespace placement (scaffold)

- Add `space_id` to relation creation options (default 1). `Allocator` instance bound to that space id.
- Keep a simple mapping in Space Catalog for existing spaces; tooling to dump per-space usage.

---

## Integration points

HeapRelation

- Replace direct page-number bumping with `Allocator::allocate_page(HeapData/HeapOverflow)`.
- For relation create: allocate HeapRoot via allocator in target space.
- For drop/truncate: iterate and `free_page`.

Pager/BufferCache (scaffold only)

- No cache policy changes; guards to prevent free-page read unless re-allocated.

WAL (stubs)

- Reserve WAL record type ids for AllocatePage/FreePage (no-op emit in Phase 2).

Config & Telemetry

- `engine.config`: extent size, pages_per_segment, allow_sparse, preallocate_mb, default_tablespace.
- Monitoring counters: pages_allocated, pages_freed, extents_created, segments_created.

Tools

- Extend `page_dump` to show PIP coverage and bits per page range.
- Add `space_info` CLI (optional) for per-space stats (pages, segments, free/alloc counts).

---

## Test plan

Unit tests

- PIP roundtrip: create space, allocate 1k pages, verify bits, free half, verify reuse order.
- Extent alignment: allocate extent sized N, verify contiguous range and boundary alignment.
- Multi-segment: push allocation beyond first segment; assert new segment files exist and mapping is correct.
- Tablespace: create two spaces; allocate pages in each; verify independent PIP chains.

Integration tests

- Heap create/insert with allocator-backed pages; maintain previous Phase 1 behavior with deterministic page numbers.
- Drop/truncate: insert rows → drop relation → verify PIP cleared and next relation reuses pages from lowest free ranges.

Soak tests

- Long-running allocator churn: allocate/free cycles with randomized sizes; ensure no leaks, deterministic progression, and stable performance.

Crash-resilience simulations (Phase 2 scope)

- Inject crash between page write and PIP bit set; ensure allocator reuses page safely.
- Inject crash after PIP set but before page checksum; run validator to detect anomalies; re-initialize page on next allocation.

---

## Milestones & exit criteria mapping

M1: Data structures & primitives

- Implement PIP/TIP/SpaceCatalog pages; helpers in ODS; `Allocator` skeleton; unit tests for PIP bit operations and mapping.

M2: Deterministic growth & multi-segment

- Integrate allocator with `FileMap` expansion; extent policy; tests for segment growth and alignment.

M3: Relation integration & reclaim

- Wire `HeapRelation` to use `Allocator`; implement drop/truncate reclaim; tests verifying reuse and determinism.

M4: Soak & crash-injection

- Allocator soak test; basic crash-injection points with predictable recovery; tooling updates (`page_dump`).

Exit (Phase 2 complete)

- Deterministic growth across segments; allocator passes soak; drop/truncate reclaims; no persistent corruption in PIP; tools reflect accurate space state.

---

## Work breakdown (sequenced tasks)

1) ODS updates
- Finalize `PageType::Pip`, `PageType::Tip`, `PageType::SpaceCatalog` semantics
- Add minimal on-disk headers for PIP and SpaceCatalog payloads
2) PIP implementation
- Implement `PipManager` (optional helper) or embed into `Allocator`
- Bit-level operations: locate bit for logical page; scan for next zero; set/clear bits; checksum page before write
3) Allocator (MVP)
- Allocate single page; grow PIP coverage; ensure `FileMap::ensure_capacity` called
- Deterministic policy and extent alignment
4) Multi-segment growth tests
- Force crossing of segment boundary; assert new segment created and PIP for that range exists
5) Space Catalog (MVP)
- Persist root pointers to PIP/TIP; segment count; next extent id
- Dump via `page_dump`
6) Integrate with `HeapRelation`
- Replace direct bumping with allocator calls for HeapRoot/HeapData/HeapOverflow
- Update create/drop/truncate code paths
7) Reclaim tests
- Drop relation and verify freed pages are reused by the next relation
8) Soak & crash injection
- Long-run allocator churn; inject crash windows; verify recovery expectations
9) Tooling polish
- `page_dump` understands PIP/SpaceCatalog; add optional `space_info` CLI (if small)

---

## Risks & mitigations

- Partial writes: order operations so page is valid before PIP bit set; add simple detection/reinit on reuse
- Fragmentation: use extent alignment to preserve locality; future FSM/vacuum will defragment
- Concurrency: Phase 2 single-writer assumption; latching and WAL will land in later phases
