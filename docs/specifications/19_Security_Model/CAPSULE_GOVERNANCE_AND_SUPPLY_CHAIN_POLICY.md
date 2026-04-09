# Capsule Governance and Supply Chain Policy

Status: current_authority

## Current authority

Current shipped authority is limited to local trust decisions over admitted modules, packages, or capsules used by the running system and its controlled tooling surfaces.

## Required rules

- admission requires an explicitly trusted source or signature root when signature policy is enabled
- unsigned or untrusted artifacts are rejected when trust enforcement is configured
- local admission policy must be deterministic and auditable

## Unsupported boundary

Remote federation, automatic policy sync from untrusted networks, and autonomous supply-chain governance beyond current local trust policy are unsupported.
