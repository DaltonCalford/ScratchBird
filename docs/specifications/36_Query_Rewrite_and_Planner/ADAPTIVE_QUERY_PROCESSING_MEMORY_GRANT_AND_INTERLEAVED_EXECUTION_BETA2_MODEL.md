# Adaptive Query Processing Memory Grant and Interleaved Execution Beta 2 Model

Status: reconstructed_required_beta2

## Purpose

Define the Beta 2 adaptive processing model needed to move beyond strong static planning into commercial adaptive optimization.

## Adaptive processing scope

This file governs:

- adaptive join choice
- post-execution memory grant correction
- staged or interleaved execution for CE-sensitive branches
- stronger batch, analytic, and parallel planning coherence

It does not weaken MGA visibility, transaction semantics, or correctness rules.

## Adaptive join framework

Beta 2 adaptive joins shall be legal only when the planner publishes:

1. a bounded candidate set of legal join alternatives
2. a runtime switching threshold or threshold family
3. safe materialization or checkpoint rules for the switching point
4. stable diagnostics describing the chosen threshold

At minimum the adaptive framework shall support switching between:

- nested loop
- hash join

Merge join may participate only when the required ordering or sort posture is preserved.

## Runtime checkpoint rule

Adaptive switching requires explicit row-count checkpoints.
The executor must not silently replace a join method without:

- a planner-published adaptive branch
- a runtime threshold comparison
- a traceable branch selection event

## Memory grant planning

Beta 2 planning shall assign operator-aware memory grants for at least:

- hash join
- sort
- aggregate
- batch or vectorized upper stages where applicable

The grant model must preserve:

- requested bytes
- effective granted bytes
- spill risk class
- workload-governance outcome

## Memory grant feedback

After execution, Beta 2 shall record:

- requested memory
- granted memory
- actual memory consumed
- spill or no-spill result
- operator class
- reuse safety posture

That feedback may influence future replanning only through explicit persisted or in-memory feedback lanes defined by policy.

## Interleaved execution

For CE-sensitive branches, Beta 2 may support staged execution where:

1. a subplan is executed or partially executed
2. actual cardinality is captured
3. dependent planning choices are resumed under the captured count

Interleaved execution is legal only when:

- the stage boundary is published by the planner
- transactional semantics remain unchanged
- the branch result does not leak beyond the current statement context

## Parallel and batch coherence

Adaptive Beta 2 planning must integrate:

- partial aggregate and final aggregate choices
- gather and gather-merge choices
- worker-count-sensitive cost variants
- batch-friendly operator variants where the executor supports them

## Safety and refusal rules

Adaptive behavior must be refused when:

- no legal alternative branch exists
- switching would cross a semantic or exactness boundary
- runtime governance disallows the adaptive variant
- diagnostics cannot disclose the switching reason

## Diagnostics requirements

Every adaptive-capable execution must expose:

- published adaptive alternatives
- threshold identity
- actual observed row counts at checkpoint
- branch chosen
- grant requested, granted, and used
- spill outcome and correction recommendation

## Non-guarantees

- this file does not claim current ScratchBird already performs adaptive joins
- this file does not authorize silent runtime replanning outside explicit planner-published adaptive branches
