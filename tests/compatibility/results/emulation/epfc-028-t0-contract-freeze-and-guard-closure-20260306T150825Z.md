# EPFC-028 T0 Contract Freeze and Guard Closure (2026-03-06)

## Scope
Phase `T0` from `EMULATED_TABLESPACE_VIRTUALIZATION_IMPLEMENTATION_PLAN_2026-03-06.md`:
1. Freeze parser->SBLR tablespace payload contracts.
2. Add metadata-only emulation guardrails (no client filesystem routing).
3. Add contract and runtime tests for those guardrails.

## Implementation increments
1. `src/parser/v3_emitter.cpp`
   - `CREATE TABLESPACE` now emits typed payload fields (`flags`, `path`, `name`, `location`, `autoextend_*`, `max_size_mb`, `prealloc_pages`).
   - `ALTER TABLESPACE` now emits structured `alterations[]` entries with typed fields.
   - `ALTER TABLE ... SET TABLESPACE` now emits explicit `online` boolean.
2. `src/sblr/v3_schema.generated.cpp`
   - Finalized schema contracts for `SCHEMA_DDL_CREATE_TABLESPACE`, `TABLESPACE_ALTERATION`, `SCHEMA_DDL_ALTER_TABLESPACE`, and `SCHEMA_DDL_ALTER_TABLE_SET_TABLESPACE`.
3. `include/scratchbird/sblr/executor.h`, `src/sblr/executor.cpp`
   - Added T0 emulated virtual tablespace metadata/binding scaffolding under emulated root namespaces:
     - `<emu_root>.system.tablespaces`
     - `<emu_root>.system.tablespace_bindings`
   - Added runtime virtualization guard: emulated `CREATE TABLESPACE` rewrites client location into `sb://emu-ts/...` token and rejects non-token result.
   - Added typed `online` handling in `ALTER TABLE ... SET TABLESPACE` dispatch path.
4. Tests added/extended:
   - `tests/unit/test_index_emitter_payload_contracts.cpp`
   - `tests/unit/test_sblr_vnext_payload_schema_mapping_contract.cpp`
   - `tests/unit/test_sblr_vnext_executor_dispatch_contract.cpp`
   - `tests/unit/test_tablespace_header_and_files.cpp`

## Validation evidence
1. Build:
   - `cmake --build /home/dcalford/CliWork/ScratchBird/build -j8`
   - Result: success (`scratchbird_tests` built).
2. Targeted T0 test subset:
   - `/home/dcalford/CliWork/ScratchBird/build/tests/scratchbird_tests --gtest_filter='IndexEmitterPayloadContractsTest.*:SBLRVNextPayloadSchemaMappingContractTest.*:SBLRVNextExecutorDispatchContractTest.AlterTablespaceOpcodeRoutesWithoutUnknownOpcodeReject:SBLRVNextExecutorDispatchContractTest.EmulatedCreateTablespaceUsesMetadataTokenAndAvoidsFilesystemTouch:TablespaceHeaderTest.*' --gtest_brief=1`
   - Result: `[==========] 22 tests from 4 test suites ran` and `[  PASSED  ] 22 tests`.

## T0 status
`T0` exit criteria are satisfied for contract freeze + metadata-only guardrail coverage.

## Next phase
Proceed to `T1` lifecycle simulation completion work (full virtual lifecycle + binding semantics and parity maps) while keeping `T0` contracts stable.
