Status: current_authority_with_reconstructed_expansion

# Front-Door Manager and Native Port Overhead Benchmark Model

## Purpose

This file defines the benchmark lane that compares direct native-port access
with the optional manager-fronted front-door path.

## Current benchmark authority

The current benchmark proves the following setup model:

1. a manager process is launched as a separate executable
2. the manager binds:
   - a front-door port
   - a native port
3. the benchmark performs a native wire handshake sequence including:
   - connect request
   - connect response
   - auth request
   - auth response
   - query request
4. the manager authentication lane uses a manager auth secret
5. benchmark support code provisions ephemeral ports and retry-based connection establishment

## Benchmark comparison model

The benchmark compares:

1. direct native-port flow
2. manager-fronted front-door flow

The comparison is specifically about protocol-path overhead, not only raw socket
latency.

## Threshold model

The current benchmark surface includes environment-driven threshold controls for:

1. mean overhead ratio maximum
2. p95 overhead ratio maximum

The default thresholds in the current benchmark harness are:

- mean overhead ratio max: `6.0`
- p95 overhead ratio max: `10.0`

## Scope of measured work

The measured work includes:

1. connection establishment
2. protocol handshake
3. authentication exchange
4. simple query request path

## Interpretation rule

This lane exists to bound the cost of the optional manager/front-door mode
relative to the native path.

It shall not be interpreted as:

1. a cluster-wide throughput benchmark
2. a listener-only benchmark
3. proof that front-door mode is free

## Reconstructed required expansion

The rebuild requires future benchmark artifact outputs for:

1. mean ratio
2. p95 ratio
3. direct native latency distribution
4. front-door latency distribution
5. failure-class counts for connect, auth, and query phases

## Boundary rule

This benchmark is valid only when the manager and native control-plane
specifications remain aligned with the actual handshake and authentication flow.
