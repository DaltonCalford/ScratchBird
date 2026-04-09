# Implementation-Ready Rebuild Completion and Freeze Rule

## Scope

This file defines when a rebuilt or recovered ScratchBird specification section
is complete enough to drive implementation without guesswork.

## Governing objective

The rebuild is complete only when another limited implementation agent can act
from canon without needing to infer:

- ownership
- algorithm order
- state transitions
- payload fields
- failure classes
- redaction and privilege boundaries
- drift versus shipped truth

## Section-level completion criteria

A section is implementation-ready only when it contains, where applicable:

1. explicit ownership and scope
2. current code-backed truth
3. required reconstructed behavior
4. explicit implementation drift
5. stepwise algorithms
6. state machines
7. field or payload contracts
8. MGA, locking, ordering, and publication rules
9. failure and refusal rules
10. observability and gate obligations
11. explicit non-guarantees
12. canonical status vocabulary from section `00`

## Cross-section completion criteria

A section is not complete if it still depends on another section for unstated
meaning such that implementers must guess the connection.

Canonical rule:

- adjacent sections may share contracts
- but the owning section must name the dependency explicitly and preserve the
  controlling meaning locally

## Rebuild freeze rule

A rebuilt lane may be marked frozen only when:

1. contradictions are resolved or explicitly recorded as drift
2. planning artifacts no longer contain open ambiguity for that lane
3. canonical files cover the current code-backed and reconstructed-required
   surfaces
4. operator and gate surfaces are aligned with the owning runtime or catalog
   sections

## No-grey-area rule

The presence of any material grey area means the lane is not frozen.

Grey area includes:

- unnamed decision rules
- implicit fallback behavior
- donor-by-analogy assumptions
- hidden privilege expansion
- unspecified status payloads
- unspecified cutover or recovery transitions

## Tree-level completion rule

The full rebuild is complete only when:

1. all active lanes meet the section-level completion criteria
2. remaining implementation drift is explicit and bounded
3. section `00` precedence and contradiction rules are satisfied
4. the planning tree can issue a final freeze snapshot without unresolved
   implementation-driving ambiguity
5. status vocabulary is normalized enough that authority class is not ambiguous
