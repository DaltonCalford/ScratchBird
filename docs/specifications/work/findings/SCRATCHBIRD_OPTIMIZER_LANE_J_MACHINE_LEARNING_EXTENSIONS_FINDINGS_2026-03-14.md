# ScratchBird Optimizer Lane J Findings

Lane: J

Topic: Machine-learning extensions

Status: First-pass findings

Date: 2026-03-14

Primary planning inputs:
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_PROGRAM_2026-03-14.md`
- `SCRATCHBIRD_OPTIMIZER_RESEARCH_AGENT_OPERATIONS_2026-03-14.md`

## 1. Scope and Lane Objective

Lane J defines the future-only ML roadmap for the optimizer.

The objective is not to replace the classical optimizer in the first implementation waves. The objective is to determine where ML can safely assist after the deterministic optimizer reaches strong maturity.

This lane covers:

- learned cardinality residuals
- plan steering and hint selection
- learned calibration support
- advisory-only ranking of plan variants

## 2. ScratchBird Current-State Baseline

ScratchBird currently has no first-class ML optimizer subsystem.

What exists today:

- statistics collection
- cardinality feedback scaffolding
- cost-model profiles
- rich plan payloads and diagnostics

These are the prerequisites for future ML work, not ML features themselves.

Inference:

- ScratchBird is correctly positioned to postpone ML until the classical optimizer has a stable data and observability foundation

## 3. Donor-Engine Research Synthesis

The donor landscape here is literature-heavy rather than code-heavy.

Useful lessons from the literature:

- Bao shows that plan steering can work when ML selects among bounded optimizer choices rather than inventing plans from scratch
- learned cardinality work shows promise, but robustness under drift and workload change remains a major issue
- Microsoft and related production studies reinforce that steering and bounded learning are more practical than end-to-end optimizer replacement

What ScratchBird should adopt later:

- advisory-only steering over bounded candidate sets
- residual correction models over deterministic cardinality estimates
- offline-trained, online-inference-light models with strong fallback behavior

What ScratchBird should reject for early waves:

- opaque end-to-end learned plan generation
- any model that becomes a correctness dependency

## 4. Primary Literature and Official-Document Synthesis

The strongest primary sources point in the same direction.

- Bao argues for learning to steer existing optimizers rather than discarding classical wisdom
- learned cardinality benchmark literature shows that robustness and operational constraints remain unsolved enough that ML should not replace deterministic estimation early
- later work such as Balsa and Microsoft steering studies confirms that learned methods can be useful, but only when the feature store, observability, and fallback rules are mature

Practical implication for ScratchBird:

- ML belongs behind a stable optimizer contract
- the first useful ML wave is advisory, bounded, and easily disabled

## 5. Normalized Algorithm Packet

Recommended future ML stages:

1. collect stable optimizer telemetry and feedback data
2. define feature extraction over query shape, statistics, and plan alternatives
3. train offline residual or steering models
4. expose model inference only as an advisory layer
5. preserve deterministic fallback for every decision

First future targets:

- cardinality residual correction
- plan-family steering across a bounded candidate set
- calibration adjustment suggestions

## 6. Formula and Heuristic Packet

Required ML safety heuristics:

- if model confidence is low, ignore the model
- if feature coverage is incomplete, ignore the model
- if model advice conflicts with hard legality or safety rules, ignore the model
- if offline validation does not beat deterministic baseline on held-out workloads, do not enable the model

Recommended confidence gate:

- only apply model advice when expected gain exceeds a configured margin over deterministic choice

## 7. ScratchBird Contract Draft

Required future contract additions:

- optimizer feature-store schema
- `MLAdvice` record with confidence, model version, and suggested action
- advisory-only interface between model output and classical planner
- rollback and disable switches

Planner guarantees:

- legality and correctness always remain classical
- every ML-influenced decision is surfaced in diagnostics
- deterministic fallback remains available at all times

## 8. Validation and Benchmark Packet

Required validation families:

- held-out workload validation
- drift and stale-statistics resilience
- mixed-workload evaluation
- advisory win-rate versus baseline
- no-regression fallback validation when the model is disabled

Key metrics:

- win rate over deterministic baseline
- tail-regression rate
- confidence calibration quality
- drift sensitivity

## 9. Adopt/Adapt/Reject/Defer Matrix

- `Adopt`: advisory-only steering and residual correction as the long-term direction
- `Adapt`: feature-store and telemetry design from production ML optimizer work
- `Reject`: correctness-critical ML in early implementation waves
- `Defer`: any ML rollout until the classical optimizer and observability system are stable

## 10. Open Questions and Integration Dependencies

- Lane D and Lane G must stabilize deterministic estimator and cost outputs first
- Lane K must expose enough telemetry to train and audit models
- Lane L must define the benchmark and drift-validation regime

## 11. Recommended Next-Step Specification Tasks

- define optimizer telemetry fields required for ML later
- specify feature-store schema and retention policy
- define advisory interface and fallback rules
- create a future-only ML roadmap document separate from the core optimizer spec
