# EPFC-028 PostgreSQL parser targeted evidence

- Captured UTC: `2026-03-03T23:20:09Z`
- Scope: targeted PostgreSQL parser lane for extension-policy closure (`CHECKPOINT`, `CLUSTER`, `WAIT FOR LSN` and surrounding parser surface)
- Command:
```bash
ctest -R "PostgreSQLParserTest" --output-on-failure
```
- Result summary:
  - `100% tests passed`
  - `0 tests failed out of 161`
  - Real time: `1.75 sec`
- Raw log artifact:
  - `ScratchBird/tests/compatibility/results/emulation/ctest-epfc028-postgresql-parser-20260303T232009Z.log`

## Notes

This evidence closes targeted parser-lane proof for EPFC-028 mapping changes. Remaining EPFC-028 closure is upstream compatibility harness execution and simulation result-shape confirmation rows tracked under `EEXT-PG-001..005`.
