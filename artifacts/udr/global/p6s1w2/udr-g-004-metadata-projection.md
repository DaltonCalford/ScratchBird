# UDR-G-004 Metadata Snapshot + Projection Runtime
Last-Modified: 2026-02-23

## Scope
Start `UDR-G-004`:
1. Persist metadata snapshot rows for remote metadata/show opcode execution paths.
2. Emit result-set payloads for `SHOW_REMOTE_*` query opcodes through executor.
3. Establish baseline for object/column projection population in subsequent `G4` slices.

## Baseline
1. `UDR-G-003` is complete: runtime ABI entry points + policy preflight + bounded dispatch are implemented.
2. Remote metadata catalog APIs already exist in `CatalogManager`:
   - `upsert/list/getRemoteMetadataSnapshotCatalogEntry`
   - `upsert/list/getRemoteMetadataObjectCatalogEntry`
   - `upsert/list/getRemoteMetadataColumnCatalogEntry`
3. Before this slice, metadata opcodes `ANALYZE/REFRESH/IMPORT` still returned deterministic `REMOTE_2311` rejections.

## Implemented in this cycle
1. `src/sblr/executor.cpp` metadata opcode runtime closure:
   - `SBLR3_ANALYZE_REMOTE_SERVER`, `SBLR3_REFRESH_REMOTE_METADATA`, `SBLR3_IMPORT_FOREIGN_SCHEMA` now dispatch through `sys_remote_query_bound` (default SQL `SELECT 1` when command text is absent) instead of hard reject.
2. Snapshot persistence wiring:
   - added post-dispatch metadata snapshot persistence helper in executor.
   - snapshot sequence is derived from existing connector snapshot rows.
   - persisted snapshot status is `COMPLETE` with deterministic `catalog_hash`, `object_count`, and `column_count`.
   - failures map to deterministic `REMOTE_2313` rejection.
3. Result-set emission:
   - added conversion bridge from `scratchbird::udr::RemoteResultSet` to executor `ResultSet`.
   - `SHOW_REMOTE_*` and metadata query opcodes now return result sets when columns/rows are present.

## Closure update (2026-02-23)
1. Metadata projection persistence is now complete for projection opcodes:
   - `SBLR3_SHOW_REMOTE_OBJECTS` persists rows into `remote_metadata_object`.
   - `SBLR3_SHOW_REMOTE_COLUMNS` persists rows into `remote_metadata_object` + `remote_metadata_column`.
2. Projection synthesis from persisted snapshots is now implemented:
   - executor synthesizes canonical result sets (`remote_path`, object/column fields, local mapping path) from catalog snapshot rows.
3. Runtime fallback is now implemented for projection opcodes:
   - when remote runtime dispatch fails, latest `COMPLETE` snapshot is loaded and returned instead of failing.
4. Payload schema routing bug closed:
   - `schemaForOpcode()` now gives explicit generated mappings precedence over broad `SBLR3_SHOW_*` fallback routing, so `SHOW_REMOTE_*` uses `SCHEMA_CONTROL_COMMAND` as required.
   - this unblocks remote server target extraction from `object_name` / `object_path` for `SHOW_REMOTE_*`.

## Validation Evidence
1. Build:
   - `cmake --build /home/dcalford/CliWork/ScratchBird/build --target scratchbird_tests -j$(nproc)` (pass)
2. Unit tests:
   - `/home/dcalford/CliWork/ScratchBird/build/tests/scratchbird_tests --gtest_color=no --gtest_filter="*RemoteOpcodeFamilyRoutesWithoutDeterministicBridgeReject*"` (pass)
   - `/home/dcalford/CliWork/ScratchBird/build/tests/scratchbird_tests --gtest_color=no --gtest_filter="*ShowRemoteObjectsFallsBackToCatalogSnapshotWhenRemoteRuntimeFails*:*ShowRemoteColumnsFallsBackToCatalogSnapshotWhenRemoteRuntimeFails*"` (pass)
   - `/home/dcalford/CliWork/ScratchBird/build/tests/scratchbird_tests --gtest_color=no --gtest_filter="*SBLRVNextPayloadSchemaMappingContractTest*"` (pass)
   - `/home/dcalford/CliWork/ScratchBird/build/tests/test_udr_connector_factory --gtest_color=no` (pass)

## Remaining G4 Work
1. None for `UDR-G-004` baseline scope.

## Status
1. `UDR-G-004`: COMPLETED.
2. Gate status: `UDR-GATE-04` pass for this slice.
