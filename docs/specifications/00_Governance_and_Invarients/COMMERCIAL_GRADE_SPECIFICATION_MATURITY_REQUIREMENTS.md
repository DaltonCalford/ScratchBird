# Commercial Grade Specification Maturity Requirements

This file defines the minimum detail level required for any ScratchBird specification that is expected to drive implementation.

## Governing objective

Every implementation-driving specification must be explicit enough that a limited implementation agent can execute it without guessing.

## Mandatory requirements for implementation-driving specifications

1. The specification must define exact ownership and adjacent handoffs.
2. The specification must define the current implementation truth separately from target-state work.
3. The specification must define any correctness-critical algorithm in stepwise form.
4. The specification must define lifecycle state machines where startup, mutation, retry, teardown, or recovery behavior matters.
5. The specification must define data layouts, field semantics, versioning rules, and compatibility checks where persisted or transmitted structure matters.
6. The specification must define concurrency, ordering, publication, and visibility rules where correctness depends on them.
7. The specification must define failure classes, refusal rules, repair boundaries, and operator-intervention boundaries where error handling matters.
8. The specification must define configuration defaults, ranges, reloadability, and scope of effect where tunables exist.
9. The specification must define observability and test obligations sufficient to prove implementation correctness.
10. The specification must define explicit non-guarantees so donor-engine behavior is not inferred by analogy.

## Ambiguity refusal rule

A specification is not implementation-ready if it relies on terms such as:
- usually
- typically
- may
- if appropriate
- similar to
- as needed

unless the condition and controlling decision rule are made explicit.

## Research rule

If current ScratchBird code does not answer a correctness-critical implementation question, the specification must be expanded using:
- current ScratchBird code and tests
- relevant maintained donor-engine source
- official documentation or primary papers where necessary

Implementation must not proceed by intuition when the specification is incomplete.

## Required supporting artifacts

Any section being promoted to commercial-grade implementation readiness must have supporting work artifacts for:
- source authority
- callsite and enforcement mapping
- donor borrow and avoid decisions
- implementation readiness
- closure freeze state

## Completion rule

A specification is only complete at this standard when no material grey area remains for implementation.
