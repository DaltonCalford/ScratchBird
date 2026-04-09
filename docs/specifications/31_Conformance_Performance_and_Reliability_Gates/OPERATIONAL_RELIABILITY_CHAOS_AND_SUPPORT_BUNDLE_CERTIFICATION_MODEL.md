Status: current_authority

# Operational Reliability Chaos and Support Bundle Certification Model

## Purpose

This file defines the certification lanes for operational support bundles,
redaction, restart continuity, deterministic chaos outcomes, and governance
evidence completeness.

## Scope

This file is authoritative for:

- support-bundle readiness certification
- support-bundle redaction certification
- restart continuity of operational evidence
- chaos certification where operator-facing evidence must remain deterministic
- governance-summary certification inside operational evidence bundles

## Support bundle certification lane

The current support-bundle certification lane proves:

1. support bundle generation succeeds against a live database
2. readiness evaluation can block when operational evidence warrants it
3. bundle output includes safety counts for:
   - shadow capture manifests
   - page audit findings
   - SLO status rows
   - error-budget status rows
   - autoscale action rows
   - admission tuning rows
4. redaction is enforced
5. redacted field count is positive when sensitive material is present

### Required support-bundle artifact contract

The certification lane must preserve, at minimum, the following artifact
families:

- bundle manifest identity
- readiness summary
- safety summary
- alert evidence rows
- forensic or page-audit evidence rows
- governance summary counts
- redaction summary

If any required artifact family is absent, the certification lane fails.

## Restart continuity lane

The current chaos/restart certification proves:

1. alert evidence is preserved across restart
2. forensic evidence is preserved across restart
3. support bundle generation after restart can still read that evidence
4. redaction remains enforced after restart
5. governance-summary evidence remains queryable after restart when the
   underlying rows remain present

## Secret-redaction gate

The current gate requires that bundle contents do not contain raw secret values
such as:

- passwords
- shadow-secret values
- embedded credential strings in URLs

The current test also requires the presence of:

- the affected event identity
- at least one `<redacted>` marker

## Reliability chaos lane

The current reliability chaos file also proves the presence of deterministic
chaos and recovery-oriented certification surfaces for:

1. deadlock victim resolution
2. IPC/network fault handling
3. sweep and operational evidence collection
4. restart and evidence continuity
5. governance evidence retention in support-bundle summaries

This file treats those as operational reliability certification inputs rather
than isolated unit tests.

### Determinism rule

For a fixed scenario seed, topology, and injected failure class, the gate must
produce the same outcome class and the same required evidence-family presence.
The exact timestamps may differ, but outcome classification and evidence schema
must remain stable.

## Workload governance certification coupling

Operational reliability certification is coupled to governance and SLO
inspection. The current code-backed lane includes:

1. admission status snapshots
2. SLO status snapshots
3. error-budget status snapshots
4. support-bundle safety summaries that count governance-related evidence

The rebuilt canon requires that governance evidence not be relegated to an
optional appendix. It is part of the operational certification surface.

## Pass criteria

The current certification lane requires:

1. bundle generation success
2. blocked readiness when critical evidence exists
3. positive evidence counts where injected evidence exists
4. redaction enforcement
5. no raw secret leakage in generated bundle contents
6. governance evidence counts present when governance rows were included in the
   request
7. restart continuity preserved for required operational evidence families

## Failure classes

The certification lane must at minimum distinguish:

- redaction failure
- missing evidence family
- readiness misclassification
- restart continuity regression
- chaos outcome non-determinism
- governance-summary omission

## Reconstructed required expansion

The rebuild requires future additions for:

1. explicit artifact-schema tables for every emitted bundle family
2. explicit chaos matrices for deadlock, IPC, sweep, restart, and governance
   disturbance lanes
3. tighter coupling between chaos outcomes and operator-facing incident
   summaries
4. tighter alignment with section `20` support-bundle readiness row contracts
