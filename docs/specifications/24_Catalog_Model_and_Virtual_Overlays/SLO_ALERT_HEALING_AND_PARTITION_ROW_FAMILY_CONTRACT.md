# SLO, Alert, Healing, and Partition Row Family Contract

## Scope

This file defines the canonical persisted row-family contract for:

- SLO profiles
- SLO bindings
- SLO windows
- SLO burn events
- alert rules
- alert targets
- alert routes
- alert events
- alert acknowledgements
- alert silences
- healing policies
- healing actions
- healing parameters
- healing runs
- healing steps
- partition events
- partition members

This file is authoritative for the persisted control and evidence substrate of
operational governance.

## Current code-backed authority

Current code-backed recovery proves that the catalog already persists all of
the following classes:

1. SLO profiles, bindings, windows, and burn events
2. alert rules, targets, routes, events, acknowledgements, and silences
3. healing policies, actions, parameters, runs, and steps
4. partition events and partition members

Canonical rule:

- these row families are not planning-only
- they are persisted cluster or operational evidence substrate already present
  in current code

## SLO row families

### SLO profile

The SLO profile family owns named service objectives, target values, and the
policy identity used during evaluation.

The persisted row family must remain sufficient to bind:

- profile identity
- objective class
- target thresholds
- evaluation window identity
- enabled or disabled state

### SLO binding

The SLO binding family links a profile to a concrete evaluation scope such as:

- node
- role
- service
- workload class

Canonical rule:

- an SLO result is derived from binding plus telemetry plus policy
- it must not exist as unattached free-form text

### SLO window and burn events

The SLO window family persists bounded evaluation windows.

The burn-event family persists notable threshold crossings and burn-severity
classification.

The persisted burn-event family is evidence history, not merely transient
metrics output.

## Alert row families

### Alert rule, target, and route

These row families persist:

- rule identity
- target identity
- route identity
- delivery and escalation posture

Canonical rule:

- alerting policy is catalog-backed
- it is not derived only from external configuration files

### Alert event

The alert-event family is the persisted incident or finding record.

It must remain sufficient to identify:

- originating rule
- severity
- state
- event time
- target summary

### Alert acknowledgement and silence

These families persist operator intervention and bounded suppression state.

Canonical rule:

- acknowledgement and silence history are auditable rows
- they are not ephemeral UI-only state

## Healing row families

### Healing policy and action

The healing-policy and healing-action families persist the bounded automation
contract for operational recovery.

They must remain sufficient to capture:

- policy identity
- action identity
- action kind
- gating or safety posture
- enablement state

### Healing parameters, runs, and steps

The current code-backed substrate already persists:

- parameter rows
- run rows
- step rows

Canonical rule:

- a healing run is auditable
- individual steps are durable evidence
- support bundles and certification lanes may summarize these rows, but may not
  replace them as the primary evidence source

## Partition row families

### Partition event

The partition-event family persists cluster segmentation or connectivity
incidents.

### Partition member

The partition-member family persists the affected member set for a partition
event.

Canonical rule:

- partition diagnostics must remain representable as persisted event plus member
  rows
- split-brain or isolation evidence may not be reduced to log text alone

## Relationship to autoscale and governance

Autoscale and admission policy may consume:

- SLO burn evidence
- queue thresholds
- alert or healing evidence

But autoscale decisions do not replace these row families. They operate on top
of them.

## MGA boundary

All row families in this file are ordinary MGA-governed database state:

- transaction-scoped
- commit-visible by snapshot rules
- restart-visible by durable row state

They are not reconstructed from a WAL replay authority.

## Reconstructed-required behavior

The lost-spec rebuild requires that future operator SQL, tooling, support
bundles, and cluster-control surfaces bind back to these families instead of
creating parallel ad hoc status stores.
