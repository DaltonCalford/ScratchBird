# Plan 08 - Protocol Conformance Testing (Emulated Engines)

## Scope
Establish a full protocol conformance test suite for Firebird/MySQL/PostgreSQL native clients, with trace-based validation against expected wire behavior.

## Priority
P0 (Alpha requirement).

## References
- `docs/findings/firebird_wire_protocol_gaps.md`
- `docs/findings/mysql_wire_protocol_gaps.md`
- `docs/findings/postgresql_wire_protocol_gaps.md`
- `docs/specifications/wire_protocols/firebird_wire_protocol.md`
- `docs/specifications/wire_protocols/mysql_wire_protocol.md`
- `docs/specifications/wire_protocols/postgresql_wire_protocol.md`
## Order of Implementation
1) Golden trace capture for baseline clients.
2) Protocol fuzzing harness per engine.
3) Integration tests with official client libraries.
4) Regression suite for known edge cases and gap items.

## Implementation Tasks
- Define golden handshake/auth/query traces for each engine.
- Build trace capture and diff tools for wire protocol validation.
- Create client integration tests using official client libs.
- Implement fuzz testing for handshake/auth/message framing.
- Add regression tests for all gap items in protocol findings docs.

## Required Data/Schema Changes
- None (test tooling only).

## Completion Checklist (Developer)
- [ ] Golden traces exist for Firebird/MySQL/PostgreSQL.
- [ ] Automated trace diff passes for handshake/auth/query flows.
- [ ] Integration tests run with native clients for each engine.
- [ ] Fuzzing harness exists with crash/timeout protections.
- [ ] Regression tests cover all protocol gap items.

## Completion Checklist (Auditor)
- [ ] Trace diffs show exact match with expected protocol behavior.
- [ ] Native client suites pass without protocol fallbacks.
- [ ] Fuzzing results show no protocol parsing crashes.
- [ ] Gap items documented as closed with test evidence.

## Testing Requirements
- Golden trace repository per engine.
- Client integration tests (isql/FlameRobin, mysql CLI/Connector, psql/libpq).
- Fuzz tests for message framing/auth/cancel/copy.

## Acceptance Criteria
- Golden trace diffs pass for all three protocols.
- Native client integration tests pass with no fallback behavior.
- All gap items are mapped to at least one automated test.

## Implementation Notes (Concrete)
- **Trace capture**: record byte-level captures for handshake/auth/query flows using a deterministic test client.
- **Diff tooling**: canonicalize timestamps and nonces before diffing.
- **Client set**: use official CLI/client libs per engine for integration tests.
- **Fuzzing**: focus on length fields, auth messages, and cancellation flows.

## Expanded API/Schema Details
- **Trace artifacts**: store golden traces per engine in `tests/protocol_traces/<engine>/`.
- **Diff tool**: add CLI under `tools/` to compare traces and emit pass/fail summaries.
- **Fuzz harness**: add per-engine fuzz driver with corpus seeds from golden traces.

## Full Implementation Detail (No Ambiguity)
- **Golden traces**:
  - Capture handshake/auth/query flows for each protocol version supported.
  - Include success and error flows, including auth failure and syntax error.
- **Diff tooling**:
  - Normalize nonces, timestamps, session IDs, and random salts before diffing.
  - Output per-packet mismatch reports.
- **Integration tests**:
  - Use official client libs (isql/mysql/psql) and require zero fallbacks.
- **Fuzzing**:
  - Focus on length fields, auth messages, and cancellation flows.
  - Include corpus seeds for malformed packets and boundary lengths.

## Concrete Tooling Layout
- `tools/proto_trace_capture` (captures byte-level traces).
- `tools/proto_trace_diff` (canonicalizes and diffs traces).
- `tests/protocol_traces/firebird/`, `tests/protocol_traces/mysql/`, `tests/protocol_traces/postgresql/`.

## Concrete Test Cases
- Compare golden trace vs live trace for each engine handshake.
- Execute a multi-result query and verify packet sequence.
- Run fuzz harness for 10k iterations without crash.

## Common Failure Patterns
- Implemented only in executor/parser; `CatalogManager` direct calls still bypass logic.
- Cache updates without on-disk persistence or load path; restart loses behavior.
- Switch statements or enum mappings missing new values, producing `<unknown>` and wrong behavior.
- CASCADE/RESTRICT or config gating ignored; dependency checks bypassed or inconsistent.
- Tests cover happy-path only; missing restart, negative, and concurrency/lock-order cases.
- Spec deviations introduced without explicit config flags or documentation.
