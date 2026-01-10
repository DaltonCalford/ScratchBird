# Plan 08 - Protocol Conformance Testing (Emulated Engines)

## Scope
Establish a full protocol conformance test suite for Firebird/MySQL/PostgreSQL native clients, with trace-based validation against expected wire behavior.

## Priority
P0 (Alpha requirement).

## References
- `docs/archive/2026-01-09/findings/firebird_wire_protocol_gaps.md`
- `docs/archive/2026-01-09/findings/mysql_wire_protocol_gaps.md`
- `docs/archive/2026-01-09/findings/postgresql_wire_protocol_gaps.md`
- `docs/specifications/wire_protocols/firebird_wire_protocol.md`
- `docs/specifications/wire_protocols/mysql_wire_protocol.md`
- `docs/specifications/wire_protocols/postgresql_wire_protocol.md`
- `include/scratchbird/testing/ProtocolTester.h`

## Order of Implementation
1) Golden trace capture for baseline clients.
2) Protocol fuzzing harness per engine.
3) Integration tests with official client libraries.
4) Regression suite for known edge cases, transaction state, and gap items.

## Concrete Code Touchpoints (Exact Files + Functions)
- Test harness:
  - `include/scratchbird/testing/ProtocolTester.h`
  - `src/testing/ProtocolTester.cpp`
- Trace capture/diff tooling:
  - `tools/proto_trace_capture.cpp` (new)
  - `tools/proto_trace_diff.cpp` (new)
- Tests:
  - `tests/unit/test_wire_protocol.cpp` (extend for trace validation)
  - `tests/unit/test_protocol_adapter_dialects.cpp` (extend to run trace capture)
- Trace storage:
  - `tests/protocol_traces/firebird/`
  - `tests/protocol_traces/mysql/`
  - `tests/protocol_traces/postgresql/`

## Implementation Tasks
- Define golden handshake/auth/query traces for each engine.
- Add golden traces for transaction state transitions:
  - Firebird COMMIT/ROLLBACK RETAINING.
  - MySQL autocommit ON/OFF and explicit START TRANSACTION.
  - PostgreSQL ReadyForQuery status transitions (I/T/E).
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
- Transaction-state traces (autocommit, explicit blocks, retaining).

## Acceptance Criteria
- Golden trace diffs pass for all three protocols.
- Native client integration tests pass with no fallback behavior.
- All gap items are mapped to at least one automated test.

## Implementation Notes (Concrete)
- **Trace capture**: record byte-level captures for handshake/auth/query flows using deterministic test clients.
- **Diff tooling**: canonicalize timestamps, nonces, and salts before diffing.
- **Client set**: use official CLI/client libs per engine for integration tests.
- **Fuzzing**: focus on length fields, auth messages, and cancellation flows.

## Full Implementation Detail (No Ambiguity)
### 1) Golden Traces
- Store binary traces in:
  - `tests/protocol_traces/firebird/handshake.bin`
  - `tests/protocol_traces/mysql/handshake.bin`
  - `tests/protocol_traces/postgresql/handshake.bin`
- Include at least:
  - Success handshake/auth
  - Auth failure
  - Simple query
  - Syntax error
  - Transaction state flows:
    - Firebird COMMIT RETAINING + ROLLBACK RETAINING
    - MySQL autocommit ON + OFF (two consecutive statements)
    - PostgreSQL BEGIN/COMMIT with ReadyForQuery status changes

### 2) Trace Capture Tool
- `tools/proto_trace_capture`:
  - Connects to server with official client.
  - Captures raw bytes (both directions) to a trace file.
  - Outputs JSON metadata: protocol, timestamp, client version.

### 3) Trace Diff Tool
- `tools/proto_trace_diff`:
  - Normalizes dynamic fields (timestamps, salts, random nonces).
  - Compares packet-by-packet and emits mismatch report.

### 4) ProtocolTester Integration
- Use `ProtocolTester` to run standard connection/query/type tests:
  - `runConnectionTests()`
  - `runQueryTests()`
  - `runTypeTests()`
  - Protocol-specific suites (COPY, ping, attach, etc).

### 5) Fuzz Harness
- Use AFL/libFuzzer-style harness:
  - Target adapter `parseMessage()` methods.
  - Seed corpus from golden traces.
  - Guard against infinite loops and long reads.

## Concrete Tooling Layout
- `tools/proto_trace_capture`
- `tools/proto_trace_diff`
- `tests/protocol_traces/firebird/`, `tests/protocol_traces/mysql/`, `tests/protocol_traces/postgresql/`

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
