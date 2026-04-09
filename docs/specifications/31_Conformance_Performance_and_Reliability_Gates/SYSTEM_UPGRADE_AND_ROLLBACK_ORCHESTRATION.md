# System Upgrade and Rollback Orchestration

Status: current_authority

## Purpose

Define the current fail-closed contract for system upgrade compatibility markers, restore-chain rollback validation, mixed-version refusal, and operator-visible upgrade and rollback boundaries.

## Current authority

Current code-backed authority is limited to:
- database format-version and compatibility-floor markers in on-disk headers
- backup manifest rollback and compatibility metadata
- restore-chain validation and fail-closed rollback checkpoint enforcement
- PITR target policy refusal where unsupported modes are requested
- restored-database header reinspection during validation
- wire-protocol version refusal where incompatible peers are detected

## Current model

ScratchBird does not currently prove one unified top-level upgrade orchestrator or live binary rollback controller. The current authoritative behavior is a set of fail-closed validation and refusal surfaces around format compatibility, restore ordering, and protocol compatibility.

## Required rules

1. on-disk format and compatibility-floor markers are authoritative for readable-version decisions
2. restore chains must remain fail-closed on database identity, format-version, and compatibility-floor mismatches
3. restore-chain ordering must begin with FULL and remain monotonic according to current backup-manager validation rules
4. unsupported PITR modes must be rejected rather than approximated
5. incompatible wire versions must be refused during handshake rather than negotiated through undefined downgrade behavior
6. MGA recovery remains state-based and not WAL or redo replay

## Operator boundary

The current operator-facing authority is validation and refusal, not automated orchestration. Operators may rely on:
- backup and restore validation results
- compatibility markers in headers and manifests
- fail-closed version refusal during protocol negotiation

Operators may not infer from this section:
- live in-place binary rollback orchestration
- fully automated mixed-version fleet rollout control
- a universal downgrade workflow controller

## Non-guarantees

- no unified upgrade-orchestration subsystem is claimed here
- no live binary rollback controller is claimed here
- no broad mixed-version cluster-management framework is claimed here
- no WAL-based replay upgrade model is claimed here
