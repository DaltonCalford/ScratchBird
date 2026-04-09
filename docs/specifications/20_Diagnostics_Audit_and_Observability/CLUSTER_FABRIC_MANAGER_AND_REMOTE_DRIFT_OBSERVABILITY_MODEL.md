# Cluster Fabric, Manager, and Remote Drift Observability Model

## Scope

This file defines the operator-visible observability contract for:

- manager heartbeat posture
- cluster-fabric link posture
- cluster-fabric queue posture
- remote drift posture
- fabric event and error activity

## Current code-backed authority

Current code-backed recovery proves:

1. cluster-fabric link, session, task, event, and error substrate already
   exists
2. failure-detector and clock-policy substrate already exists
3. manager DBBT, LPREFACE, and MCP control path already exists

The observability lane must therefore expose more than generic up/down status.

## Required operator outputs

The operator-visible observability lane must remain able to surface, at
minimum:

- manager heartbeat state
- manager heartbeat freshness
- link count by state
- ready-link count
- degraded-link count
- queued task count
- blocked task count
- failed task count
- unresolved drift count
- quarantined drift count
- fabric event count by class
- fabric error count by class

## Separation rule

Operator outputs must preserve visible separation between:

- manager heartbeat health
- link readiness
- queued work posture
- remote drift posture
- event/error activity

Canonical rule:

- these may be correlated
- they may not be collapsed into one generic "cluster manager unhealthy" output

## Error and redaction rule

Error activity may be counted, classified, and summarized, but public
observability and bundle lanes must preserve the catalog redaction rules for
sensitive control-plane data.

## Support-bundle coupling

Support bundles and readiness summaries may summarize this evidence family, but
they must not suppress the existence of drift, blocked queue, or fabric-error
activity while still claiming a complete operational picture.

## Fail-closed rules

The observability layer shall not:

1. report healthy manager posture while drift or blocked instruction posture is
   hidden
2. report link readiness without preserving per-link or per-state evidence
3. expose raw sensitive control-plane payloads as observability text
