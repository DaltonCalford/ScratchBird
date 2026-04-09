# Section 34 Table Storage and Access Methods

Status: current_authority

This section owns the canonical ScratchBird access-method model: registry and
scope, heap and primary storage, secondary index access paths, specialized
access-method boundaries, row-store multi-insert and heap-only update
performance rules, and DDL or maintenance interaction rules.

Unsupported method families or narrower-than-generalized behaviors must be
expressed as explicit fail-closed or non-guarantee boundaries in the section
files. This section entry point is not placeholder scaffolding.

## Section scope

- access method registry and scope
- heap and primary storage boundary
- heap multi-insert and heap-only update performance rules
- B-tree and secondary access methods
- specialized access method boundary
- access method DDL, DML, and maintenance interaction

## Audit lookup anchors

Representative section-34 audit anchors are:
- `StorageEngine::createIndexScan(`
- `StorageEngine::filterIndexCandidatesByVisibleHeap(`
- `IndexFactory::lookupCapabilities(`

<!-- AUTO-GENERATED:FILE-LIST:START -->
- [ACCESS_METHOD_DDL_DML_AND_MAINTENANCE_INTERACTION.md](ACCESS_METHOD_DDL_DML_AND_MAINTENANCE_INTERACTION.md)
- [ACCESS_METHOD_REGISTRY_AND_SCOPE.md](ACCESS_METHOD_REGISTRY_AND_SCOPE.md)
- [BTREE_AND_SECONDARY_INDEX_ACCESS_METHODS.md](BTREE_AND_SECONDARY_INDEX_ACCESS_METHODS.md)
- [COLUMNSTORE_ANALYTICAL_STORAGE_AND_SEGMENT_MODEL.md](COLUMNSTORE_ANALYTICAL_STORAGE_AND_SEGMENT_MODEL.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [HEAP_AND_PRIMARY_STORAGE_BOUNDARY.md](HEAP_AND_PRIMARY_STORAGE_BOUNDARY.md)
- [HEAP_MULTI_INSERT_AND_HEAP_ONLY_UPDATE_PERFORMANCE_MODEL.md](HEAP_MULTI_INSERT_AND_HEAP_ONLY_UPDATE_PERFORMANCE_MODEL.md)
- [SPECIALIZED_ACCESS_METHOD_BOUNDARY.md](SPECIALIZED_ACCESS_METHOD_BOUNDARY.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->
