# Normative Checklist: P1 Parser Distributed Policy and Telemetry Contracts

## Purpose
Define parser-side requirements for distributed read policy mapping, policy-safe transport to engine, and telemetry propagation.

## Scope
- Native parser and all emulated parsers with distributed-read exposure enabled.
- Plan-only and execute request classes.

## Hard Invariants
1. Parser never decides consistency semantics beyond profile-mapped policy translation.
2. Parser never bypasses engine-side policy validation.
3. Parser telemetry output must follow security redaction policies.

## Mandatory Inputs
- Frozen parser request context.
- Dialect capability profile and distributed policy profile.
- Parser telemetry exposure profile.

## Implementation Checklist

### PD00 Distributed Policy Capability Gate
- [ ] Gate all distributed read options by dialect profile.
- [ ] Reject unsupported policy hints deterministically.
- [ ] Persist gate decisions in capability log.

Pass condition:
- No unsupported distributed option reaches canonical AST.

### PD01 Policy Mapping to Canonical Fields
- [ ] Map dialect policy controls to canonical fields:
  - consistency class
  - speculative retry mode
  - repair mode
- [ ] Reject ambiguous or conflicting controls.

Pass condition:
- Canonical distributed policy payload is complete and valid.

### PD02 Parser-to-Engine Policy Envelope
- [ ] Emit distributed policy fields in engine request envelope.
- [ ] Include policy profile version and correlation id.
- [ ] Preserve deterministic field order.

Pass condition:
- Engine receives identical policy payload for identical parser inputs.

### PD03 Result and Event Mapping
- [ ] Decode distributed read acknowledgements and events.
- [ ] Map verification, repair, and degraded-mode events to dialect-native diagnostics.
- [ ] Preserve correlation ids in mapped outputs.

Pass condition:
- Distributed lifecycle is visible and traceable to parser clients.

### PD04 Telemetry Request and Response Flow
- [ ] Gate telemetry request options by parser exposure profile.
- [ ] Decode telemetry samples and map fields to client format.
- [ ] Reject telemetry fields hidden by profile.

Pass condition:
- Telemetry flow is deterministic and profile-compliant.

### PD05 Redaction and Security
- [ ] Redact restricted fields before client egress:
  - internal node addresses
  - internal object uuids when policy forbids exposure
  - internal policy ids unless diagnostic role allows.
- [ ] Enforce role-based telemetry detail levels.

Pass condition:
- Telemetry and event outputs do not leak restricted data.

### PD06 Stale Policy or Version Handling
- [ ] If engine rejects stale policy version, rerun parser policy mapping with current profile snapshot.
- [ ] Preserve deterministic retry behavior and retry counters.

Pass condition:
- Stale-policy recovery is deterministic.

## Negative Requirements
- No parser-side implicit consistency downgrade.
- No event emission without correlation id.
- No unredacted telemetry egress when exposure policy forbids.

## Conformance Gates
- `P1-28-GATE-01`: PD00..PD02 pass for policy gating, mapping, and envelope transport.
- `P1-28-GATE-02`: PD03..PD04 pass for event and telemetry mapping correctness.
- `P1-28-GATE-03`: PD05..PD06 pass for security redaction and stale-version recovery.

## Cross-Section Links
- `23_SBLR_VM_Compiler_and_Executor/NORMATIVE_P1_DISTRIBUTED_READ_CACHE_AND_TELEMETRY_CHECKLIST.md`
- `26_Native_Wire_Protocol/NORMATIVE_P1_WIRE_DISTRIBUTED_READ_AND_TELEMETRY_CHECKLIST.md`

## Audit normalization note (2026-03-28)
- This file is treated as a target-state worklist, not as present-day implementation proof.
- Current section `28` source authority is narrower and is centered on the native V3 parser stack plus the shipped Firebird, PostgreSQL, and MySQL parser-family seams.
- Broader normalization-gate, distributed-policy, passthrough, replication, connector, and fabric-parser claims require separate bounded proof before promotion.
