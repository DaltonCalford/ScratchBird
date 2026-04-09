# Memory Ownership and Allocator Boundaries

Status: current_authority_beta1

## Purpose

This file defines the Beta 1 memory domains, typed allocator boundaries, general
allocator backend contract, and NUMA or hugepage policy for ScratchBird.

## Core rule

ScratchBird does not have a single undifferentiated heap. Memory must be owned
by domain and allocator class so the engine can enforce admission, invalidation,
pressure response, and safe reclamation without weakening MGA correctness.

## Canonical memory domains

### 1. `buffer_pool_domain`
Owner:
- storage and page-management runtime

Contains:
- page frames
- pin counts
- dirty tracking
- writeback staging

### 2. `storage_sidecar_cache_domain`
Owner:
- storage-family runtimes such as LSM and auxiliary index sidecars

Contains:
- LSM SSTable block cache
- storage-family local block or lookup sidecars

### 3. `transaction_and_visibility_domain`
Owner:
- transaction manager and visibility code

Contains:
- snapshots
- transaction inventory working sets
- visibility scratch
- lineage walkers
- conflict and lock bookkeeping

### 4. `catalog_and_metadata_domain`
Owner:
- catalog and metadata subsystem

Contains:
- catalog rows
- schema epoch structures
- dependency caches
- permission cache
- policy epoch tracking
- stats metadata
- helper result material

### 5. `parser_and_front_door_domain`
Owner:
- parser workers and protocol front-door surfaces

Contains:
- dialect-local ASTs
- normalization state
- protocol decode buffers
- translation cache keyed by dialect, SQL, schema version, and privilege signature

### 6. `connection_and_statement_cache_domain`
Owner:
- connection pool and prepared statement cache runtime

Contains:
- cached prepared statement metadata
- per-database statement cache
- per-connection statement cache slices
- fingerprint and parameter-signature material

### 7. `executor_runtime_domain`
Owner:
- compiler, planner, and executor

Contains:
- execution state
- expression scratch
- operator-local row material
- runtime plan state

### 8. `result_cache_domain`
Owner:
- query result cache runtime

Contains:
- cached result payloads
- result-cache indexing and invalidation state

### 9. `temp_and_spill_domain`
Owner:
- temporary table, workfile, and spill runtimes

Contains:
- temp tables
- sort spill
- hash spill
- overflow buffers
- workfiles

### 10. `resident_index_domain`
Owner:
- resident-by-design index families

Contains:
- HNSW resident graph state
- IVF resident centroid and posting structures
- vector-flat resident state when enabled
- resident mutation side structures

### 11. `jit_metadata_domain`
Owner:
- SBLR JIT runtime

Contains:
- compile queues
- hotness state
- transient compilation buffers
- verified native artifact metadata
- object-link and relocation buffers

### 12. `jit_code_domain`
Owner:
- SBLR JIT runtime

Contains:
- executable code pages
- published code handles
- retired code pending reclaim

### 13. `accelerator_domain`
Owner:
- GPU or other accelerator providers

Contains:
- GPU-resident search sidecars
- batch staging buffers
- accelerator-local adjacency or distance buffers derived from canonical CPU state

## Ownership rules

1. A domain must have a clear owner subsystem.
2. Cross-domain borrowing must be explicit and bounded.
3. Resident index and accelerator memory must never be silently charged to
   unrelated executor scratch or parser scratch.
4. Temporary or spill memory must not starve durability-critical or
   visibility-critical domains without explicit admission control.
5. Permission cache, translation cache, statement cache, result cache, and
   storage sidecar caches are separate domains even when they all behave like
   bounded caches.
6. JIT metadata and published code are different domains.
7. Verified native artifacts are compatibility-keyed catalog-backed state, not
   anonymous heap state.

## Allocator backend contract

ScratchBird shall support a pluggable general allocator backend underneath the
typed allocators.

Beta 1 required backend interface:

```cpp
struct GeneralAllocatorBackend {
  virtual void* alloc(size_t size, size_t alignment) = 0;
  virtual void free(void* ptr) = 0;
  virtual void* realloc(void* ptr, size_t size, size_t alignment) = 0;
  virtual size_t usableSize(void* ptr) = 0;
  virtual void scavenge() = 0;
  virtual BackendStats stats() = 0;
  virtual ~GeneralAllocatorBackend() = default;
};
```

## Backend selection rule

Beta 1 required backend policy:

1. `mimalloc` is the default general allocator backend when available.
2. `tcmalloc` shall be supported as an alternate backend.
3. the platform allocator shall remain a fallback.
4. `jemalloc` may be supported in profiling builds, but is not the default
   Beta 1 backend.

The backend does not replace `SbArena`, `SbSlab`, `SbPageBackedArena`, or
`SbCodeHeap`.

## NUMA policy

The process shall use local-first NUMA placement for:

- buffer pool frames
- resident index working sets
- page-backed temporary arenas
- code heaps

Rules:

1. connection, statement, and operator allocations prefer the current worker's
   NUMA node
2. remote-node borrowing is allowed only after local-node soft pressure
3. remote borrowing may not exceed `25%` of the child node's hard limit without
   operator-visible pressure markers

## Hugepage policy

Hugepages shall be preferred for:

- buffer pool frame arrays
- large resident-index page groups
- page-backed temporary arenas above `8 MiB`
- executable code heaps above `16 MiB`

Hugepages are not required for:

- tiny slabs
- short-lived parser arenas
- small per-statement scratch arenas

## Allocation class mapping

| Allocation class | Required allocator |
| --- | --- |
| AST and parse normalization | `SbArena` |
| binder and planner nodes | `SbArena` |
| runtime descriptor tables | `SbSlab` |
| spillable operator payloads | `SbPageBackedArena` |
| temporary workfile page metadata | `SbPageBackedArena` plus `SbSlab` |
| cache entry headers | `SbSlab` |
| cache payloads | domain-specific allocator, usually `SbArena` or page-backed |
| JIT compile scratch | `SbArena` or `SbGenerationArena` in `jit_metadata_domain` |
| executable code | `SbCodeHeap` |
| tracker metadata | `SbGenerationArena` or `SbSlab` |

## Current code-backed domain anchors

Current code-backed anchors include:

- `LSMBlockCache`: separate storage-sidecar cache, default `64 MiB`, keyed by `<file_path, block_offset>`
- `PermissionCache`: separate metadata-domain cache, default `1000` entries, `10s` TTL, global and table policy epoch aware
- `TranslationCache`: separate parser/front-door cache, default `1024` entries, `64 MiB`, `300s` TTL
- `StatementCache`: separate connection and prepared statement cache domain, default `1000` statements per database, `64 MiB`, and `100` per connection
- `QueryResultCache`: separate result-cache domain already captured elsewhere in section `33`
- `JitArtifactStore`: verified artifact persistence keyed by object, SBLR hash, target triple, CPU profile, ABI, compiler identity, compiler version, optimization profile, and security policy version

## MGA interaction

MGA increases pressure on storage-adjacent and visibility domains because
multiple record versions may remain relevant concurrently. Memory policy must
therefore account for:

- version-chain traversal
- long snapshot retention
- GC lag
- resident index cleanup lag
- policy-epoch and schema-epoch invalidation races

## Beta 1 required controls

1. Resident vector and accelerator families must receive explicit domain budgets
   and admission rules.
2. Permission cache and translation cache must remain privilege and epoch aware;
   cross-user or cross-epoch reuse is non-conforming.
3. Statement cache memory must remain distinct from result-cache memory and
   distinct from JIT metadata and code memory.
4. Treating resident vector state, accelerator state, executable code pages, or
   verified native artifacts as anonymous executor scratch is non-conforming.
