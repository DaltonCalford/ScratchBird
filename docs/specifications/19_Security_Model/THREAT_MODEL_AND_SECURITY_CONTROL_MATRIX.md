# Threat Model and Security Control Matrix

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the principal threat classes and the control families that ScratchBird must use to mitigate them.

## Threat classes

1. bootstrap compromise
2. unauthenticated listener or manager exposure
3. weak authentication or plugin downgrade
4. unauthorized data access at row, column, domain, or object level
5. masking bypass
6. privilege escalation through views, procedures, functions, or packages
7. cluster secret concentration on a single node
8. derivative-lane abuse through archive, shadow, or management surfaces
9. remote management mutation without separate authorization
10. side-channel leakage through observability or tooling

## Control families

- fail-closed bootstrap and provider admission
- separate authentication and authorization
- row-level security
- column-level privileges and visibility controls
- domain-level security and masking policy
- definer-rights sandboxing for emulated-engine schema objects
- audit and forensic logging
- cluster secret partitioning and sealed-share storage
- explicit privilege split between inspect, mutate, rotate, and promote operations

## Non-negotiable rules

- successful authentication does not imply object access
- object execution rights do not imply base-object rights
- masking bypass requires explicit privilege
- cluster secret material must not be stored reconstructibly on one untrusted node by default
- management authorization must be separate from ordinary session access
