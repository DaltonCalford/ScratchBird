# Beta 2 Workload Trace Capture Privacy And Redaction Model

## Purpose

Define privacy, redaction, retention, and operator controls for production
workload capture and replay artifacts.

## Governing rules

1. Sensitive material is classified before export.
2. Exported workload packs may contain plaintext secrets only under explicit
   privileged policy.
3. Redaction state is machine-readable and replay-visible.

## Classification classes

- `PUBLIC`
- `INTERNAL`
- `CONFIDENTIAL`
- `SECRET`
- `PROTECTED_QUERY_PAYLOAD`

## Redaction actions

- `KEEP`
- `MASK`
- `TOKENIZE`
- `DROP`

## Required metadata

Every captured parameter or text-bearing field shall carry:

- classification class
- redaction action
- redaction policy uuid
- token family if tokenized
- replay usability flag

## Export rules

1. `SECRET` and `PROTECTED_QUERY_PAYLOAD` default to `TOKENIZE` or `DROP`.
2. Plaintext export of those classes requires a privileged explicit override.
3. Export manifests must record whether replay fidelity is reduced by
   redaction.

## Refusal rules

- `WORKLOAD_EXPORT_POLICY_MISSING`
- `WORKLOAD_EXPORT_SECRET_PLAINTEXT_REFUSED`
- `WORKLOAD_REPLAY_REDACTION_CLASS_UNSUPPORTED`

## Metrics

- redacted field count
- tokenized field count
- dropped field count
- export refusal count

## Cross-section requirements

- section 20 owns privacy policy and audit evidence
- section 31 owns replay pack and rehearsal gates
- section 19 owns protected-query payload classification
