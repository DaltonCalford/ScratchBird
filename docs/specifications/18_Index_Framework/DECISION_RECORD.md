# Decision Record - 18_Index_Framework

## Scope
- Index types and layouts.
- Transaction visibility effects.
- Maintenance and optimization stats.
- B-tree hardening requirements for crash safety, concurrency, reclamation, and
  diagnostic maturity.

## Invariants
- Indexes obey MGA visibility rules.
- Index entries reference specific row versions (TIDs).
- B-tree structural correctness must not depend on general-purpose WAL.
- Split, merge, root publication, and reclaim operations must remain
  restart-reconstructable from page state plus B-tree metadata.

## Decisions
- Canonical index types include B-tree, Hash, GIN, GiST, SP-GiST, BRIN, FULLTEXT, and SPATIAL.
- B-tree is mandatory for Alpha; other types are defined and required when a dialect feature depends on them.
- Emulated index types map to canonical implementations only when semantics are preserved.
- B-tree next-generation hardening uses:
  - split-tolerant right-link and high-key semantics
  - restart-anchor search over compressed pages
  - metapage-based root publication and structural intent tracking
  - conservative deletion/reclamation with epoch quarantine
- Hash index uses linear hashing with split bucket.
- TID layout is 16 bytes: `page_id`, `slot_id`, `version_id`, `table_uuid_hash`, `reserved`.
- Key encoding uses per-segment encoding with null sort byte and comparator inversion for descending order.
- Unique enforcement uses per-key locks held to commit.
- Online and concurrent index builds are defined but rejected in Alpha.
- Descending order supports `invert_compare` and `bytewise_complement` modes, set at index creation time (native only).
- Optional write-buffer or hot-key acceleration is post-hardening scope only and
  may not ship before crash-safe SMO, metapage, and verification gates pass.

## Alternatives Considered
- Index-specific storage engines (rejected).
- General WAL-based B-tree recovery (rejected for Alpha core because MGA and
  explicit structural intents remain the authoritative model).
- Aggressive non-empty page merge by default (rejected until stronger
  split-tolerant and scan-safe proofs exist).

## Open Questions
- None.

## References
- `18_Index_Framework/BTREE_STRUCTURAL_MODIFICATION_DURABILITY_PROTOCOL.md`
- `18_Index_Framework/BTREE_CONCURRENCY_AND_SPLIT_TOLERANT_DESCENT.md`

## Update 2026-03-28: normalization decisions
- The broad section `18` family list remains a valid catalog and parser exposure surface, but it is not the same thing as independently proven physical implementations.
- Runtime authority is centered on `IndexFactory` registry entries, runtime-class mapping, and executor routing, not on the older exhaustive per-family narrative alone.
- Current code proves a smaller shared runtime-class set than the section previously implied.
- Generic operator support is explicitly bounded:
  - `GIN` requires specialized operators
  - `FULLTEXT` and related inverted families require specialized text operators
  - `HNSW` and related vector families require k-NN style operators
  - `COLUMNSTORE` requires specialized scan operations
  - `BRIN` generic scans are block-range oriented and do not directly return TIDs
- Broad online, concurrent, rebalance, relocate, and family-universal maintenance guarantees remain narrower than the current code proof and are therefore left `partial`.
