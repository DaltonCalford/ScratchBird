# B-tree Persistent Metadata and Root Management

## Purpose
Define the durable B-tree metapage, root publication contract, and metadata
validation rules.

## Scope
- metapage structure
- root and height publication
- page and reclaim counters
- Bloom metadata anchoring and format versioning

## Hard Invariants
1. Every B-tree has one authoritative metapage.
2. Readers open the tree from the metapage-published root only.
3. Metadata corruption must be diagnosable and may not silently fall back to
   fragile in-memory state.

## Metapage Fields
The metapage stores:
- `root_gpid`
- `tree_height`
- `first_leaf_gpid`
- `page_count`
- `leaf_page_count`
- `pages_pending_reclaim`
- `root_publication_seq`
- `active_smo_count`
- `format_version`
- `bloom_metadata_gpid`
- `build_state`
- `last_verified_epoch`

## Root Publication
Rules:
1. root changes increment `root_publication_seq`
2. metapage publication is durable before client-visible success
3. tree open validates the published root against height and page type

## Validation
Metapage validation must check:
- correct page type and checksum
- root page reachability
- height consistency
- reclaim counters not negative or impossible
- active SMO and build states internally consistent

## Upgrade and Versioning
Any incompatible metapage or pivot format change:
1. increments `format_version`
2. requires rebuild or upgrade tooling
3. must be detectable before tree open proceeds

## Acceptance Criteria
- restart/open does not rely on implicit in-memory root state
- root publication is durable and explicit
- metadata corruption is detectable and diagnosable

## Cross-Section References
- `BTREE_STRUCTURAL_MODIFICATION_DURABILITY_PROTOCOL.md`
- `BTREE_BULK_BUILD_AND_REBUILD_PROTOCOL.md`
- `INDEX_CATALOG_AND_METADATA.md`

## Legacy Mapping
| Historical source | Material preserved here |
| --- | --- |
| `specifications_old/indexes/BTREE_SPEC.md` | implicit root management replaced with explicit metapage contract |
| `specifications_old/indexes/INDEX_ARCHITECTURE.md` | index metadata placement refined for B-tree durability |

## Gap Closure Mapping
- `SB-BTR-008`
