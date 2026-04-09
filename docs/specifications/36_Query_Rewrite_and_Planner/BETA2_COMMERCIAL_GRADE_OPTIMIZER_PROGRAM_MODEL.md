# Beta 2 Commercial-Grade Optimizer Program Model

Status: reconstructed_required_beta2

## Purpose

This file converts the 2026-03-23 optimization findings report into canonical Beta 2 optimizer requirements.

It does not claim the current engine already implements this full program.
It defines the staged Beta 2 optimizer maturity ladder that future implementation and certification work must follow.

## Current canonical baseline already closed

The current canon already defines:

- planner candidate enumeration and tie-break discipline
- cost, cardinality, and statistics dependency boundaries
- plan-cache identity and immutable publication rules
- primary-class optimizer parity for all shipped index families
- family-native metrics packets and no-ignored-index rules
- GPU and resident-index costing boundaries

Beta 2 work begins above that baseline. It is not a rewrite of the Alpha planner contract.

## Beta 2 optimizer problem statement

The remaining commercial-grade gap is not "does a cost-based planner exist."
The remaining gap is:

- planner unification
- richer physical-property-aware search
- broader trusted native access-method competition
- persisted plan governance
- adaptive query processing
- parameter-sensitive multi-plan behavior
- workload-aware optimizer feedback and regression control

## Beta 2 staged maturity ladder

Beta 2 optimizer work is divided into four staged sub-milestones.

### B2-M1 Commercial Static Optimizer

Required outcome:

- one canonical planner API and one canonical planning pipeline
- physical-property-aware search that preserves multiple candidates by ordering, exchange, and exactness posture
- merge-aware and order-native search, not only late wrapper comparison
- trusted native treatment for exact and exact-plus-recheck access families
- mixed workload competition across ordered, summary, and columnar paths

Owning files:

- `PLANNER_UNIFICATION_PHYSICAL_PROPERTY_AND_ACCESS_TRUST_BETA2_MODEL.md`
- current section `18` family planner and metrics canon

### B2-M2 Commercial Governed Optimizer

Required outcome:

- persistent plan-store equivalent
- plan baselines and forcing governance
- CE versioning, confidence, and fallback reporting
- production regression capture and regression disposition workflow

Owning files:

- `COMMERCIAL_PLAN_STORE_BASELINES_AND_REGRESSION_GOVERNANCE_BETA2_MODEL.md`
- section `31` Beta 2 optimizer certification gates

### B2-M3 Commercial Adaptive Optimizer

Required outcome:

- adaptive join framework
- memory grant planning plus post-execution correction
- staged or interleaved execution for CE-sensitive branches
- stronger parallel and batch-aware physical planning

Owning files:

- `ADAPTIVE_QUERY_PROCESSING_MEMORY_GRANT_AND_INTERLEAVED_EXECUTION_BETA2_MODEL.md`

### B2-M4 Commercial Self-Tuning Optimizer

Required outcome:

- parameter-sensitive multi-plan optimization
- persisted optimizer feedback tables
- DOP feedback and workload-aware resource integration
- automatic tuning loop with bounded authority

Owning files:

- `PARAMETER_SENSITIVE_PLAN_OPTIMIZATION_AND_WORKLOAD_FEEDBACK_BETA2_MODEL.md`

## Release-claim rule

Beta 2 is a staged optimizer program.
A product claim may only advertise the highest completed sub-milestone that has passed section `31` certification.

Examples:

- claiming "commercial static optimizer" requires B2-M1 completion only
- claiming "commercial governed optimizer" requires B2-M1 and B2-M2 completion
- claiming "commercial adaptive optimizer" requires B2-M1 through B2-M3 completion
- claiming "commercial self-tuning optimizer" requires B2-M1 through B2-M4 completion

## Hard invariants

1. Beta 2 optimizer work must preserve MGA correctness and visibility semantics.
2. Plan-store, feedback, and adaptive behavior must not mutate published plan-cache entries in place.
3. Every shipped index family remains a primary optimizer class even before the full Beta 2 ladder is complete.
4. Adaptive behavior may improve plan quality, but it must not weaken determinism, observability, or safety controls.
5. Workload governance integration must honor the engine resource envelope instead of bypassing it.

## Explicit non-authority rule

This file is the Beta 2 optimizer program contract.
It does not by itself prove current implementation parity.
Implementation claims require the corresponding section `31` gate evidence.
