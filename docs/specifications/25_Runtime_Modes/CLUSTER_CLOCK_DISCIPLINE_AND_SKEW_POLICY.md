# Cluster Clock Discipline and Skew Policy

Status: unsupported_boundary

## Current authority

Current proof is limited to configuration and validation for bounded listener
and control-plane skew checks such as:
- `dbbt_clock_skew_ms`
- `issued_at_ms` / `expires_at_ms` validation windows on `DBBT`
- manager-to-listener acceptance windows for managed binding

These checks are server-local binding and replay-safety checks.
They are not cluster-wide clock governance.

## Non-authority

This file is not current implementation authority for:
- cluster-wide clock discipline
- global skew remediation
- leadership, lease, or consensus-coupled time policy
- node-to-node heartbeat timestamp arbitration

Do not implement a cluster clock subsystem from this file.
