Status: current_authority

# Auth Plugin Enterprise Performance and Soak Gate Model

## Purpose

This file defines the benchmark and soak gates for enterprise authentication
provider methods.

## Current benchmark authority

The current performance benchmark compares enterprise provider methods against a
baseline secure phase-1-equivalent path.

### Baseline path

The baseline path:

1. executes a deterministic CPU-only baseline routine
2. captures p50 and p95 latency
3. uses that baseline p95 as the comparison anchor for provider methods

### Provider methods currently benchmarked

The current benchmark covers:

1. LDAP bind
2. Kerberos GSSAPI
3. Ident RFC1413
4. RADIUS PAP
5. PAM conversation

## Performance benchmark metrics

For each provider method, the benchmark records:

- connect p50 latency in microseconds
- connect p95 latency in microseconds
- auth p50 latency in microseconds
- auth p95 latency in microseconds
- auth p95 increase percentage over baseline
- resident-set-size delta in bytes

## Performance gate thresholds

The current performance thresholds are:

1. p95 auth latency increase must be less than or equal to `40%`
2. RSS delta must stay within `1 MiB` when RSS sampling is available
3. connect and auth path failure counts must remain zero

## Artifact model

The performance benchmark emits stdout records for:

1. baseline secure phase-1-equivalent metrics
2. one structured record per enterprise provider method

## Soak authority

The current soak lane simulates:

1. mixed provider selection
2. random disconnect events
3. reconnect events
4. injected fault cases
5. successful auth cases
6. denied auth cases

The soak loop currently represents:

- `4 hours` simulated time
- `1 iteration` per simulated second

### Soak gate thresholds

The current soak thresholds are:

1. `unexpected = 0`
2. `success_count > 0`
3. `deny_count > 0`
4. RSS delta must remain within `2 MiB` when RSS sampling is available

## Interpretation rule

This combined lane proves:

1. enterprise provider methods are performance-gated relative to a deterministic baseline
2. method-specific auth paths are soak-tested under disconnect and fault churn
3. memory growth is part of the gate, not a side observation

## Reconstructed required expansion

The rebuild requires future additions for:

1. per-provider timeout and fail-mode distributions
2. provider chain and MFA benchmark variants
3. external-group-sync overhead variants
4. management-plane auth policy change churn during soak
