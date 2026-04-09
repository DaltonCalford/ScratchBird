# Rewrite Stage Model

This file defines the required rewrite-stage order and ownership boundary for ScratchBird.

## Rewrite ownership boundary

Rewrite begins after front-door parsing has produced a validated internal statement form and ends before final path selection becomes immutable.
Rewrite does not own SQL tokenization, parser error recovery, executor runtime adaptation, or storage-layer recovery decisions.

## Rewrite stage inventory

| Stage | Required input | Required output | Ownership boundary |
| --- | --- | --- | --- |
| parser-adjacent normalization handoff | validated front-door AST or lowered internal query form | canonical semantic input for rewrite | parser owns syntax; rewrite owns only normalized semantic form |
| semantic binding and capability freeze | normalized semantic query plus catalog and permission context | bound query shape and capability envelope | rewrite may not invent catalog bindings not proven by semantic analysis |
| rewrite-before-search contract freeze | bound query shape | immutable rewrite contract id, owner pass id, terminal pass id, and frozen marker | after freeze, later stages may annotate but may not silently change semantic query shape |
| bounded logical transform pass | bound query shape and rewrite contract | transformed query shape preserving semantic equivalence | transform must declare its class and preserve binding contract |
| access-family lowering preparation | transformed predicates and relation metadata | planner-family lowering requests and queryability or recheck annotations | rewrite may classify access families; planner decides final path selection |
| plan-payload annotation rewrite | chosen runtime plan and emitted plan payload | annotated select payload carrying runtime plan contract data | this is payload rewrite, not semantic query rewrite |

## Required stage order algorithm

1. Accept only queries that have already passed parser-level validation and semantic lowering.
2. Produce a canonical semantic query representation.
3. Bind object, column, and capability references against current metadata and privilege context.
4. Freeze the rewrite-before-search contract and publish the contract id, owner pass id, terminal pass id, and frozen flag.
5. Run only declared logical transforms that preserve the bound semantic contract.
6. Build planner-family lowering requests from the transformed predicate and access metadata shape.
7. Hand the transformed and lowered shape to the planner search and path-construction stages.
8. After plan selection is complete, perform payload annotation rewrite to embed runtime-plan metadata into the emitted select payload.
9. Do not reopen semantic rewrite after the frozen contract is published unless the query is fully invalidated and rebuilt from stage 1.

## Transformation admission rules

A rewrite transform is admissible only if all of the following are true:

1. it declares a transform class
2. it preserves bound object identity and semantic result shape
3. it records enough contract metadata for downstream plan hashing and traceability
4. it does not depend on unsupported late executor feedback
5. it does not require an unowned parser or storage-side behavior to be correct

If any one of those conditions fails, the transform must not run.

## Implemented transform families admitted by this section

| Transform family | Required status | Required semantics |
| --- | --- | --- |
| simplification and normalization | supported | canonicalize query shape without changing result semantics |
| access-family lowering | supported | lower generalized predicate or access intent into planner-family requests, queryability state, and recheck requirements |
| common subexpression analysis or elimination | supported where explicitly invoked | identify reusable expression structure without widening semantic scope |
| runtime payload plan annotation | supported | rewrite emitted payload to carry runtime plan metadata after search |
| materialized-view rewrite | present but disabled | implementation must treat current MV rewrite surface as non-authoritative until it returns an actual rewritten tree and is promoted by spec |

## Freeze and mutation rules

1. The rewrite-before-search contract is the last semantic rewrite checkpoint.
2. After the contract is frozen, later stages may add plan metadata, plan hashes, or tracing attributes, but they may not silently reorder semantic clauses, change relation identity, or inject new logical operators.
3. Any request to mutate the semantic query after freeze must invalidate the current planning attempt and restart from the beginning of the rewrite pipeline.
4. Payload annotation is allowed after plan choice because it changes execution metadata, not semantic query meaning.

## Explicit exclusions

- no donor-style global rule system
- no implicit executor-side adaptive rewrite pipeline
- no hidden statistics-driven rewrite layer beyond explicitly admitted transforms
- no active materialized-view rewrite claim while the current surface remains stub or pending
- no untracked rewrite pass permitted outside the declared stage order
