# Alert, Healing, Partition, and Autoscale Observability Model

## Scope

This file defines the operator-visible observability contract for:

- SLO burn posture
- alert activity
- healing activity
- partition activity
- autoscale and admission-tuning activity

## Current code-backed authority

Current code-backed recovery proves that operational evidence already includes
persisted and queryable families for:

- SLO burn and evaluation rows
- alert rows
- healing rows
- partition rows

It also proves that support-bundle or readiness summaries already count several
governance-related evidence families.

## Required operator outputs

The operator inspection lane must remain able to surface, at minimum:

- current burn severity
- burn event counts
- open alert counts by severity
- acknowledgement overdue counts
- remediation overdue counts
- active healing run counts
- failed healing run counts
- autoscale action counts
- admission-tuning action counts
- partition-event counts

## Partition observability

Partition observability must distinguish:

- current degraded or isolated posture
- persisted event identity
- affected member count
- recovery or healing action presence

Partition status must not be reduced to a single generic connectivity flag.

## Healing observability

Healing observability must distinguish:

- policy identity
- action identity
- run identity
- current run state
- failed step counts
- completed step counts

If any current runtime surface exposes only coarse summaries, that is
implementation drift against this canonical model.

## Autoscale and governance observability

The operator lane must preserve a visible separation between:

- SLO burn posture
- queue pressure
- admission-tuning actions
- autoscale actions

Canonical rule:

- a later implementation may correlate these classes
- it may not collapse them into one undifferentiated "governance unhealthy"
  output

## Support-bundle coupling

Support bundles and readiness summaries must remain able to count and summarize
the evidence families in this file.

They are derivative evidence and may be redacted where needed, but they may not
omit the existence of these evidence classes while still claiming a complete
operational picture.

## Fail-closed rules

The observability layer shall not:

1. report active healing without durable healing run evidence
2. report partition health without a way to distinguish degraded from recovered
   posture
3. report autoscale or tuning actions without preserving action counts or
   identities
4. claim governance health while suppressing known burn, alert, healing, or
   partition evidence classes
