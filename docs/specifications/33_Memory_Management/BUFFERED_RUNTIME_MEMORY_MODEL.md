# Buffered Runtime Memory Model

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the concrete buffered and runtime memory surfaces already proven in current code and the rules that keep those surfaces from collapsing into one undifferentiated cache story.

## Buffered Runtime Matrix

| Domain | Current state | Current truth | Explicit exclusion |
| --- | --- | --- | --- |
| page-resident working set | current | buffer-backed residency, dirty-state, writeback, and thrash tracking are real engine surfaces | not a promise that every access path is pinned or resident |
| storage sidecar cache | current | LSM block cache exists as a separate `64 MiB` default LRU cache keyed by file path and block offset | not part of buffer-pool truth and not an MGA visibility authority |
| permission cache | current | permission cache exists with `1000` default entries, `10s` TTL, user or object invalidation, epoch-aware lookup, and verified-bypass mode | not a long-lived authorization truth source |
| statement cache | current | prepared statement cache exists with per-database and per-connection limits, memory bounding, multiple eviction policies, and schema-aware keys | not permission to reuse across schema or privilege drift |
| translation cache | current | dialect SQL-to-SBLR translation cache exists with `1024` default entries, `64 MiB`, `300s` TTL, and schema/privilege-aware keys | not cross-parser sharing and not cross-privilege reuse |
| query result cache | current | LRU result cache exists with independent entry and memory limits, table-based invalidation, and stats | not durable truth and not exempt from invalidation |
| JIT and artifact runtime | current | compile queue, hotness maps, transient IR/native buffers, and verified artifact metadata are real runtime memory surfaces | not permission for unbounded compile accumulation |
| resident index and accelerator working set | current_plus_reconstructed | resident index and accelerator memory are distinct domains and must be budgeted separately | not anonymous executor scratch |
| parser-side transient memory | current_bounded | parser-adjacent temporary memory exists outside engine durability truth | not a basis for widening core engine ownership claims |

## Current Code-Backed Cache Models

### 1. LSM Block Cache

Current authority includes:
- default `64 MiB` max size
- key `<file_path, block_offset>`
- LRU eviction
- O(1) lookup via hash map
- O(1) insertion plus LRU list maintenance
- file-wide invalidation on SSTable removal or compaction
- mutex-protected thread safety

### 2. Permission Cache

Current authority includes:
- default `1000` entry limit
- default `10s` TTL
- shared-mutex concurrency model
- global and table policy epoch capture
- `CACHED` and `VERIFIED` check modes
- quorum-aware cache bypass
- user-level invalidation
- object-level invalidation
- full invalidation path

Security-critical operations must use fresh verification paths rather than stale-cache trust.

### 3. Statement Cache

Current authority includes:
- default `1000` max statements per database
- default `64 MiB` max memory
- default `100` statements per connection
- default `1 hour` TTL
- minimum TTL `60s`
- maximum TTL `24h`
- supported eviction policies:
  - `LRU`
  - `LFU`
  - `ARC`
  - `FIFO`
- fingerprint-based cache keys
- parameter-signature contribution to cache key
- schema-version contribution to cache key
- privilege-signature contribution to cache key
- statement type filtering:
  - `SELECT`, `INSERT`, `UPDATE`, `DELETE` cached by default
  - `DDL` and `UTILITY` not cached by default

### 4. Translation Cache

Current authority includes:
- default `1024` max entries
- default `64 MiB` max bytes
- default `300s` TTL
- enable or disable flag
- key fields:
  - dialect
  - SQL text
  - schema version
  - privilege signature
- LRU order updates on successful get
- oversize-entry refusal when entry exceeds configured max bytes
- invalidate-all path

This cache is parser-local in semantic terms even if the runtime object is shared by front-door protocol code. It must never become a cross-parser lowering dependency.

### 5. Query Result Cache

Current result-cache authority includes:
- enabled by default
- `64` entry default limit
- `64 MiB` default memory limit
- SHA-256 keying over SQL text or bytecode
- LRU eviction
- per-table invalidation
- invalidate-all path
- statistics for hits, misses, evictions, invalidations, and current footprint

### 6. JIT Artifact Store

Current authority includes:
- verified artifact compatibility key:
  - object UUID
  - canonical SBLR hash
  - target triple
  - CPU feature profile
  - native ABI version
  - compiler identity
  - compiler version
  - optimization profile
  - security policy version
- persistent artifact payloads stored via catalog and TOAST-backed blobs
- signature blob optional but enforceable
- ready-state verification before use
- retired-artifact rejection

This makes JIT artifact state a compatibility-gated persistent runtime surface, not an anonymous heap-only cache.

## Canonical Rules

1. Buffer-backed page memory, storage sidecar caches, permission cache, translation cache, statement cache, result cache, JIT working memory, and resident index memory are separate domains.
2. A cache hit never changes MGA durability or visibility truth.
3. Permission and translation reuse must remain schema-aware, privilege-aware, and policy-epoch-safe.
4. Statement cache reuse must remain schema-version and privilege-signature safe.
5. Result-cache memory must be bounded and invalidatable by table or broad schema events.
6. JIT compile queues and artifact working sets are bounded runtime structures, not hidden background heap.
7. Resident index and accelerator working sets must never be silently charged to unrelated operator scratch memory.
8. Parser-side temporary memory does not widen durability or execution truth.

## Explicit Non-Guarantees

- no claim of a fully closed multi-tier global cache hierarchy
- no promise that every operator family already exposes one uniform memory governor
- no claim that cached query results survive schema truth changes independently of invalidation
- no claim that statement cache, translation cache, and permission cache are mutually interchangeable
