# Security Quorum, Cluster Secret, and MFA Inspection Result Model

## Scope

This file defines the canonical public inspection result contract for:

- cluster-secret quorum posture
- shard-collection posture
- unlock posture
- MFA policy and enrollment posture
- step-up requirement posture

## Governing rule

Inspection results for security quorum and MFA must remain structured.

Canonical rule:

- cluster-secret, quorum, and MFA inspection is not free-form diagnostic text
- public result schemas must preserve stable fields and stable posture classes

## Cluster-secret quorum result family

The public inspection lane must remain able to return, at minimum:

- database identity
- profile identity
- active key identity
- minimum shard threshold
- currently collected shard count
- security-quorum failure mode
- security-quorum decision
- unlock readiness class
- break-glass allowed flag
- last unlock result, when present
- last unlock time, when present

## Shard-state result family

The public inspection lane must remain able to return, at minimum:

- shard identity
- holder identity
- collection posture
- last collection time, when present
- refusal or failure class, when present

## MFA posture result family

The public inspection lane must remain able to return, at minimum:

- account identity
- MFA policy identity
- factor class
- enrolled flag
- primary-enrollment flag
- step-up required flag
- step-up TTL, when present
- recovery-code allowed flag
- break-glass allowed flag

## Security boundary

The public inspection path must not expose:

- raw shard material
- raw reconstructed secret values
- raw MFA seed material
- raw recovery codes

## Distinction rule

The result contract must preserve visible separation between:

- policy posture
- enrollment posture
- current unlock or quorum posture
- break-glass or recovery posture

No implementation may collapse these into one generic "security ready" flag.

## Fail-closed rules

The public inspection path shall not:

1. report unlock-ready without reporting quorum decision and threshold posture
2. report MFA-ready while omitting enrollment or policy posture
3. expose secret or seed material in inspection results
