# Vector Index Residency and Accelerator Mirror Model

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the canonical runtime model for vector and ANN index families that maintain a durable database image and one or more in-memory search structures.

It exists because ScratchBird now has code-backed durable vector index surfaces and a reconstructed requirement that vector indexes, once admitted for active runtime use, should remain memory-resident for low-latency search while keeping the database image authoritative.

## Family scope

This file governs the residency and mirror model for:

- `HNSW`
- shared vector ANN aliases routed through the current HNSW-backed runtime
- routed IVF-class surfaces while they still share the vector runtime
- routed flat-vector surfaces while they still share the vector runtime
- optional accelerator mirrors such as `GPU_CAGRA`

## Truth model

The authoritative truth order is:

1. heap and version-chain truth under MGA
2. durable vector-index database image
3. CPU-resident canonical vector runtime state
4. accelerator-resident derivative mirrors

Consequences:

- a vector graph or flat-search structure in memory is a runtime acceleration surface, not the primary truth store
- accelerator memory is derivative of the CPU-resident canonical runtime state
- WAL-after, shadow, archive, or accelerator export surfaces remain derivative and non-authoritative

## Current code-backed baseline

Current code authority proves:

- a durable HNSW page family with MGA xmin/xmax fields on pages and nodes
- stable heap TID references from vector nodes
- soft delete through xmax and later reclaim
- configurable HNSW graph parameters including `M`, `ef_construction`, and `ef_search`
- vector quantization support surfaces for `SQ8`, `SQ4`, `PQ`, `OPQ`, and `BINARY`
- SIMD-aware CPU distance paths and vector helper surfaces

Current code does not yet prove a universal first-use-to-always-resident shared vector runtime across all vector families. That is reconstructed required behavior below.

## Required reconstructed residency model

### Admission rule

A vector family designated as resident must:

1. load its durable canonical image on first runtime use or explicit preload
2. materialize a CPU-resident canonical search state
3. keep that state resident until one of the legal retirement conditions occurs

Legal retirement conditions are:

- engine shutdown or restart
- explicit operator unload
- policy-driven memory pressure eviction
- detected non-conforming or corrupt resident state
- structural incompatibility after catalog or format change

### Publication rule

The shared resident state visible to other transactions must reflect committed truth only.

Allowed models:

- local staged mutation plus commit-time publication
- commit-time replay into resident state
- post-commit resident refresh from durable image

Disallowed model:

- exposing uncommitted shared resident vector changes as globally visible search truth

### First-use rule

If the resident state is absent and a query reaches a resident vector family:

1. validate durable metadata and index identity
2. load the durable image into CPU-resident canonical form
3. mark the family cold-started
4. execute with cold-start cost and observability attribution
5. retain the resident state for subsequent requests unless policy forces eviction

## Write and flush ordering

### Insert and update

For committed vector-bearing row changes the canonical order is:

1. create new heap/version truth under MGA
2. update the durable vector-index image in transactional form
3. commit the transaction
4. publish the committed change into shared resident CPU state
5. refresh or enqueue any accelerator mirror update

### Delete

For delete or superseded-version removal the canonical order is:

1. create delete or new-version heap truth under MGA
2. mark the durable vector entry or node obsolete through transactional metadata
3. commit the transaction
4. mark the resident structure entry stale or deleted for post-commit readers
5. reclaim only after heap and version-chain proof allows cleanup

### Checkpoint and flush rule

Resident vector state is not the durable checkpoint authority.

Required rule:

- committed structural changes must be durable in the database image before any resident-only optimization claims are treated as recoverable state

Resident flush or mirror refresh may lag as a derivative lane, but lag must be observable and bounded by policy.

## Accelerator mirror model

### CPU-before-GPU rule

Accelerator state must be derived from CPU-resident canonical vector state, not treated as an independent durable format.

### Admission procedure

A conforming accelerator mirror procedure is:

1. admit and validate CPU-resident canonical state
2. validate accelerator provider identity, ABI, target, metric-family support, and resource budget
3. allocate accelerator memory
4. transform or copy canonical state into accelerator-native layout
5. publish accelerator-ready status only after validation succeeds

### Failure rule

If the accelerator mirror fails:

- keep CPU-resident state authoritative for runtime search
- degrade to CPU search or exact fallback according to family policy
- never mark durable vector truth invalid solely because the accelerator mirror failed

## Quantization and compression rule

Quantization may reduce resident memory, but it does not change MGA truth.

Required rule:

- quantized resident search state is a runtime representation of durable vector truth
- family claims of exactness, recall, or rerank quality must account for quantization loss
- quantization metadata and codebooks must remain identity-bound to the resident state that uses them

## Required optimizer-visible metrics

Every resident vector family must publish at least:

- resident state presence flag
- resident bytes reserved and used
- cold-load count
- cold-load latency
- resident node or vector count
- pending resident refresh debt
- pending durable flush debt when applicable
- candidate expansion count
- post-filter exact-distance reject rate
- MGA visibility reject rate
- stale-entry or reclaim debt
- quantization mode and compression ratio when used
- metrics freshness and confidence

If an accelerator mirror exists, it must also publish:

- accelerator-ready state
- accelerator-resident bytes
- accelerator load or rebuild latency
- accelerator fallback count
- accelerator reset or eviction count

## Planner contract

The planner must treat vector-family residency as a first-class cost dimension.

Required rule:

- a warm resident vector family and a cold resident vector family are not cost-equivalent
- if residency is absent and a cold load is required, costing must include that cold-load penalty or degrade the path to conservative ranking
- if accelerator readiness is absent, planner claims that depend on accelerator-only latency envelopes are invalid

## Recovery and rebuild rule

On restart or resident-state loss:

- the durable database image is authoritative
- resident CPU state must be reloaded or rebuilt from durable state
- accelerator mirrors must be rebuilt from CPU-resident canonical state
- no WAL-style log replay narrative is allowed to replace the durable MGA image as truth

## Non-authority and rejection rules

The following claims are incorrect:

- resident vector memory is the durable truth store
- accelerator memory may bypass the CPU-resident canonical state
- uncommitted vector updates may publish directly into shared resident state
- vector reclaim may remove stale entries before heap and MGA proof allow it
- cold and warm vector paths are planner-equivalent by default
