# Test Results

- ticket_id: DT-015
- status: PARTIAL
- summary: deterministic UUID generation and fixed-ID enforcement are implemented; full authoritative registry parity is still open.

## Validation Commands
- `rg -n "systemDomainNamespaceUuid|uuidV5|canonicalSystemDomainKey|fixed_domain_id" src/core/domain_manager.cpp`
- `cd build/tests && ./scratchbird_tests --gtest_filter='SystemDomainRegistryTest.DeterministicIdsAcrossFreshDatabases'`

## Pass Criteria Evaluation
- Deterministic namespace + v5 algorithm implemented: PASS
- Fixed UUID usage in system bootstrap path: PASS
- Full canonical registry name/UUID parity against authoritative table: PARTIAL
