# IMP-18 Implementation Checklist

## Ticket
- ID: IMP-18
- Section: 18_Index_Framework
- Gate Contract: docs/specifications/18_Index_Framework/TEST_CONTRACT.md

## Inputs
- docs/specifications/18_Index_Framework/SPEC_OUTLINE.md
- docs/specifications/18_Index_Framework/INDEX_*.md
- docs/specifications/18_Index_Framework/*_SPEC.md
- docs/specifications/18_Index_Framework/FULLTEXT_PG_TSCONFIG.md
- docs/specifications/18_Index_Framework/FULLTEXT_RANKING_MODES.md
- docs/specifications/18_Index_Framework/DIALECT_COMPATIBILITY_MATRIX.md
- docs/specifications/18_Index_Framework/TEST_CONTRACT.md

## Ordered Tasks
1. Implement deterministic contracts for core index families (B-tree, hash, GIN, GiST, SP-GiST, BRIN).
2. Implement deterministic contracts for text/spatial and analytics structures.
3. Implement deterministic contracts for vector/index variants and specialized structures.
4. Implement deterministic contracts for engine-specific index behaviors (MongoDB/Neo4j/Cassandra/Redis).
5. Implement MGA visibility/GC/security behavior, metrics/costing, and maintenance/rebalance/relocate contracts.
6. Implement health scan contracts (light and diagnostic).
7. Implement DDL feature contracts and fulltext ranking/ts_config checks.
8. Implement required, negative, performance, and compatibility test suites and evidence capture.

## Exit Criteria
- Required tests pass.
- Gate result is pass.
- Traceability maps requirements to deterministic artifacts.
