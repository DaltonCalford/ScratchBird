# Normative Checklist: P2 Parser Plan Stability and Hint Translation Contracts

## Purpose
Define parser-side P2 requirements for deterministic plan stability controls and hint translation without violating engine authority.

## Scope
- Native parser full support.
- Emulated parsers support only where dialect profile explicitly enables equivalent controls.

## Hard Invariants
1. Parser hints are advisory and never override engine safety, authorization, or correctness rules.
2. Parser must emit deterministic hint payload ordering.
3. Unsupported hint semantics must be rejected, not approximated.

## Mandatory Inputs
- Parser capability profile with hint support matrix.
- Stable query-shape canonicalization output.
- Session and role policy context.

## Implementation Checklist

### PH00 Hint Capability Gate
- [ ] Gate each incoming hint by dialect capability profile.
- [ ] Reject unsupported hints with deterministic error code.
- [ ] Record accepted/rejected hints in capability decision log.

Pass condition:
- No unsupported hint reaches engine envelope.

### PH01 Canonical Hint Translation
- [ ] Translate accepted hints into canonical hint records:
  - hint id
  - target scope
  - argument vector
  - precedence class.
- [ ] Serialize canonical hints in deterministic sorted order.

Pass condition:
- Canonical hint payload is byte-stable for identical inputs.

### PH02 Plan Stability Keying
- [ ] Build parser-side stable query shape key from canonical AST and capability version.
- [ ] Attach shape key to engine request envelope.
- [ ] Preserve deterministic key across whitespace/case-equivalent input forms.

Pass condition:
- Query shape identity is stable and reproducible.

### PH03 Tie-Break Policy Propagation
- [ ] Propagate optional tie-break policy selectors from native parser to engine when enabled.
- [ ] For emulated parsers, expose tie-break controls only if dialect-equivalent behavior exists.

Pass condition:
- Tie-break policy propagation respects dialect constraints.

### PH04 Plan Pin and Unpin Control
- [ ] Support plan pin/unpin parser controls for native dialect only unless profile enables emulated equivalent.
- [ ] Map pin/unpin requests to canonical management feature keys.
- [ ] Reject pin requests lacking required privilege.

Pass condition:
- Plan stability controls are privilege-safe and deterministic.

### PH05 Explain/Diagnostics Alignment
- [ ] Ensure parser EXPLAIN output includes tie-break and hint application metadata supplied by engine.
- [ ] Preserve source-span linkage for hint-related errors.

Pass condition:
- Client-visible diagnostics match engine decision context.

### PH06 Stale Hint Policy Recovery
- [ ] If engine rejects hint policy version, refresh parser profile snapshot and rerun translation.
- [ ] Prevent infinite retry by policy-defined retry cap.

Pass condition:
- Stale-hint recovery is deterministic and bounded.

## Negative Requirements
- No parser-side hint execution or simulated plan selection.
- No ambiguous hint merge rules.
- No hidden fallback for unsupported emulated-dialect hints.

## Conformance Gates
- `P2-28-GATE-01`: PH00..PH02 pass for capability gating, canonical hint translation, and shape-key stability.
- `P2-28-GATE-02`: PH03..PH04 pass for tie-break propagation and plan pin/unpin control behavior.
- `P2-28-GATE-03`: PH05..PH06 pass for diagnostics alignment and stale-policy recovery.

## Cross-Section Links
- `23_SBLR_VM_Compiler_and_Executor/NORMATIVE_P2_COST_AWARE_SCHEDULER_AND_TIEBREAK_CHECKLIST.md`
- `31_Conformance_Performance_and_Reliability_Gates/P1_P2_OPTIMIZATION_GATE_PROFILE.md`

## Audit normalization note (2026-03-28)
- This file is treated as a target-state worklist, not as present-day implementation proof.
- Current section `28` source authority is narrower and is centered on the native V3 parser stack plus the shipped Firebird, PostgreSQL, and MySQL parser-family seams.
- Broader normalization-gate, distributed-policy, passthrough, replication, connector, and fabric-parser claims require separate bounded proof before promotion.
