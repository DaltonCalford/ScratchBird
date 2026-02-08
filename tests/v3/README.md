# V3 Conformance Tests

This directory hosts V3 conformance tests mapped to V3 specs.

## Structure
- `parser/` grammar, ambiguity, and AST typing tests
- `sblr/` opcode/container/payload/validation tests
- `executor/` execution semantics tests
- `dialect/` emulation and gap assertion tests
- `ipc/` parser/engine IPC contract tests
- `protocol/` wire protocol compliance tests
- `ops/` monitoring, metrics, and ops tests

## Conventions
- Each test file should list the spec(s) it covers at the top.
- Tests should be deterministic and include negative cases.
