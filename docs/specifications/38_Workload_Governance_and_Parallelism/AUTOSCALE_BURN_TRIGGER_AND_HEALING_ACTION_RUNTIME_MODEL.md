# Autoscale Burn Trigger and Healing Action Runtime Model

## Scope

This file defines the runtime model that connects:

- SLO burn severity
- queue and admission pressure
- autoscale policy
- healing policy
- operator evidence

This file is authoritative for the governance and recovery decision boundary
between observation, admission adjustment, scaling, and healing.

## Current code-backed authority

Current code-backed recovery proves:

1. autoscale policy already includes SLO-burn thresholds
2. autoscale policy already includes queue thresholds
3. SLO profiles, bindings, windows, and burn events are already persisted
4. healing policies, actions, runs, and steps are already persisted

Canonical rule:

- autoscale and healing operate on structured persisted and runtime evidence
- neither lane is authorized to run on undocumented heuristics alone

## Decision inputs

The governance runtime may evaluate, at minimum:

- short burn rate
- long burn rate
- burn severity
- queue depth
- active session count
- active query count
- configured admission limits
- class or policy enablement
- recent healing activity

## Burn-severity classes

The canonical burn-severity vocabulary is:

- `NONE`
- `MODERATE`
- `HIGH`
- `CRITICAL`

Canonical rule:

- severity transitions must be deterministic for the same policy, window, and
  input evidence
- severity classes are first-class control inputs, not UI-only labels

## Action classes

The governance or recovery runtime may derive actions such as:

- no action
- admission tighten
- scale out
- scale out and tighten
- incident page
- healing-policy dispatch

These actions are distinct. One action class does not imply the others.

## Evaluation order

The canonical evaluation order is:

1. load current policy and binding truth
2. load recent telemetry and queue state
3. derive SLO burn posture
4. derive queue-pressure posture
5. evaluate suppression or cooldown boundaries
6. derive autoscale action class
7. derive healing eligibility
8. emit operator evidence and persisted action history

## Cooldown and safety rule

Autoscale and healing decisions must remain bounded by policy and safety
constraints.

Canonical rule:

- repeated action dispatch without cooldown or eligibility checks is
  non-conforming
- healing action execution must remain auditable through persisted run and step
  rows

## Relationship to support bundles

Support-bundle and readiness surfaces may summarize:

- active healing run count
- failed healing run count
- autoscale action count
- admission tuning count

Those summaries are derivative evidence. They do not replace the underlying
persisted governance and healing rows.

## Fail-closed rules

The governance runtime shall not:

1. dispatch autoscale or healing actions without policy identity
2. claim healthy burn posture when required metrics are absent without marking
   metrics absence
3. collapse autoscale and healing into one indistinguishable generic action
4. bypass persisted healing run or step evidence for executed healing actions
