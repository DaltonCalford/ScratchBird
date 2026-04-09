Status: current_authority

# Support Bundle Readiness, Redaction, and Operational Evidence Model

## Purpose

This file defines the operator-facing support-bundle model, including readiness
evaluation, included evidence families, and mandatory redaction behavior.

## Governing rule

Support bundles are observational.

They expose retained evidence and incident state, but they do not themselves
prove automatic repair or seamless failover.

## Readiness state model

The current readiness health states are:

- `READY`
- `DEGRADED`
- `BLOCKED`

## Alert readiness request model

The current alert-readiness request carries:

- evaluation time
- window minutes
- info ack SLA
- warning ack SLA
- critical ack SLA
- warning vulnerability SLA
- critical vulnerability SLA

All readiness SLA values must be non-zero.

## Alert readiness row model

The current readiness row carries:

- event ID
- rule ID
- rule name
- severity
- event state
- event time
- silenced flag
- acked flag
- vulnerability signal flag
- ack overdue flag
- remediation overdue flag
- ack deadline
- remediation deadline
- route count
- target count
- target summary

## Readiness summary model

The current readiness summary carries:

- readiness state
- evaluation time
- open event count
- visible event count
- silenced event count
- actionable event count
- ack overdue count
- remediation overdue count
- critical open count
- active healing run count
- failed healing run count
- high burn count
- critical burn count
- summary text

## Support bundle safety summary

The current bundle safety summary carries:

- readiness summary
- SLO status count
- error budget status count
- autoscale action count
- admission tuning count
- shadow capture manifest count
- page audit finding count
- wal_after segment count
- audit export segment count
- redacted field count
- redaction enforced flag

## Support bundle request model

The current request controls inclusion of:

- SLO status
- error budget status
- autoscale actions
- admission tuning
- shadow capture manifests
- page audit findings
- wal_after segments
- audit export segments

## Support bundle result model

The current result carries:

- bundle ID
- output path
- safety summary
- alert row count
- redacted field count
- redaction enforced flag
- manifest preview

## Redaction rule

Sensitive diagnostic text and fields shall be sanitized before bundle output.

The current code-backed model redacts secrets from:

- alert rule condition text
- alert target endpoint text
- forensic finding details
- shadow capture manifest payloads

The chaos and support-bundle tests prove that bundle output must not expose:

- raw passwords
- raw tokens
- embedded credentials in URLs

## Restart continuity rule

Operational evidence retained in catalog-backed alert and forensic rows remains
readable after restart through support-bundle generation.

The restart and chaos lane proves support bundles can still show:

- operational event identity
- readiness state
- page audit findings
- shadow capture manifest counts

while keeping secret fields redacted.

## Fail-closed rules

The support-bundle subsystem shall not:

1. emit unredacted sensitive fields
2. claim redaction was enforced when secret text remains present
3. silently omit readiness-blocking evidence while reporting a healthy state
