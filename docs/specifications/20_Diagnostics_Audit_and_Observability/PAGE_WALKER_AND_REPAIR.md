# Page Walker and Repair

Status: current_authority

## Purpose

The page walker is the authoritative online and offline validator for storage structure integrity. It validates page headers, checksums, transaction inventory pages, heap chains, and index structure before repair or destructive maintenance may proceed.

## Validation order

For every visited page, the page walker shall execute validation in this order:

1. page address and family admission
2. header field decode and range checks
3. header checksum validation
4. payload or data checksum validation where enabled
5. page generation and repair-marker validation
6. family-local structural validation
7. linkage validation to adjacent pages or chains
8. transaction-state cross-check where the page contains transactional content

## Family-local validation requirements

### Transaction inventory pages

The walker shall validate:

- transaction-state slot ranges
- page-to-page continuity
- header markers for inventory pages
- consistency between cached horizon summaries and materialized inventory pages

### Heap pages

The walker shall validate:

- slot table boundaries and tuple offsets
- tuple-header field ranges
- backversion chain pointers
- deleting or superseding transaction stamp fields
- free-space and special-area boundaries

### Index pages

The walker shall validate:

- page-type family identity
- key ordering according to family rules
- sibling pointers
- child pointers and level identity
- split markers, fence keys, or family-equivalent routing markers
- entry references to heap candidates where cross-check is enabled

## Repair classes

The page walker shall classify findings as:

- `clean`
- `warning_non_destructive`
- `repairable_local`
- `rebuild_required_family`
- `containment_required`

Only `repairable_local` findings may be repaired in-place by the walker. `rebuild_required_family` and `containment_required` must refuse destructive local repair and instead open the explicit rebuild or containment path.

## In-place repair limits

The walker may repair only the following classes without higher-level rebuild:

- repair-marker normalization
- header metadata fields proven reconstructable from the page body
- checksum recomputation after a successful non-destructive canonicalization pass
- free-space metadata recomputation when the tuple area is otherwise valid

The walker must not invent transaction truth, version lineage, or index routing structure.

## Sweep interaction

Sweep must honor walker results:

- `clean` and `warning_non_destructive` pages may proceed to reclaim evaluation
- `repairable_local` pages require successful repair before destructive reclaim
- `rebuild_required_family` and `containment_required` pages block destructive reclaim

## Operator outputs

The page walker shall publish at minimum:

- pages walked by family
- checksum failures by family
- header failures by family
- tuple-chain failures
- index-structure failures
- repaired pages
- quarantined pages
- rebuild-required families
