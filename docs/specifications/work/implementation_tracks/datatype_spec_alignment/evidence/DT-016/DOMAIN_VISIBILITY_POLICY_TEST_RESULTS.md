# Domain Visibility Policy Test Results

- ticket_id: DT-016
- status: IMPLEMENTED
- summary: visibility gating for system domains is implemented and tested with dialect + emulation-profile policy.

## Scenario Matrix
| Scenario | include_system | dialect_tag | enabled emulation types | expected | result |
| --- | --- | --- | --- | --- | --- |
| Native baseline | true | (empty/native) | postgresql only | native system domains visible | PASS |
| Dialect gate | true | postgresql | postgresql only | `[sb_pg_dom]` visible, `[sb_my_dom]` hidden | PASS |
| Non-system hide | false | postgresql | postgresql only | all `[sb_*]` hidden, user domain visible | PASS |

## Validation Commands
- `rg -n "listDomainsVisible|enforce_emulation_profiles|include_system|dialect_tag" src/core/domain_manager.cpp include/scratchbird/core/domain_manager.h`
- `cd build/tests && ./scratchbird_tests --gtest_filter='SystemDomainRegistryTest.VisibilityGatesSystemDomainsByDialectAndProfile'`

## Pass Criteria Evaluation
- System domains are filtered by `include_system`: PASS
- System domains are filtered by enabled emulation profile set: PASS
- Active dialect surface only includes native + matching origin system domains: PASS
