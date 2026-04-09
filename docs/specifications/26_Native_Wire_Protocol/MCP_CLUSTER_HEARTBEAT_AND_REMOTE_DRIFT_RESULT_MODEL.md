# MCP Cluster Heartbeat and Remote Drift Result Model

## Scope

This file defines the canonical public result contract for manager inspection
of:

- manager heartbeat posture
- cluster-fabric link posture
- remote drift posture
- queued remote instruction posture

This file is authoritative for the public manager-side result schema, not for
internal listener-management IPC.

## Governing rule

The manager may expose bounded inspection results through MCP or adjacent
manager-admin result paths.

Canonical rule:

- public manager inspection is structured
- heartbeat, drift, and queue posture are not returned as free-form status text

## Required result families

The public manager inspection lane must remain able to return, at minimum:

- cluster identity
- node identity
- manager heartbeat state
- heartbeat freshness time or freshness class
- link identity
- link state
- link ready time, when present
- unresolved drift count
- unresolved drift class
- queued instruction count
- blocked instruction count
- quarantined instruction count
- last assessment time, when present
- last successful apply time, when present

## Link result rule

Link posture must remain distinguishable from manager heartbeat posture.

Canonical rule:

- a healthy manager does not imply every link is ready
- a ready link does not imply drift-free target posture

## Drift result rule

The result contract must preserve:

- drift class
- drift count
- cutover or apply readiness class, when relevant

Drift must not be flattened into a generic warning string.

## Queue result rule

The result contract must preserve at least:

- queued count
- blocked count
- quarantined count
- applying count, when relevant

## Security and redaction rule

The public inspection path must not expose:

- raw DBBT values
- raw LPREFACE payloads
- raw token-auth material
- raw endpoint credentials

## Fail-closed rules

The public manager result path shall not:

1. return ready while omitting required heartbeat or drift fields
2. merge queue posture into heartbeat posture without preserving distinct
   fields
3. expose sensitive control-plane secrets in public inspection rows
