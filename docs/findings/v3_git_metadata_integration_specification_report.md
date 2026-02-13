# V3 Git Metadata Integration Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/core/GIT_METADATA_INTEGRATION_SPECIFICATION.md`

## Summary
- This document is explicitly labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- It defines a comprehensive GitOps-style workflow for schema/version control, including SQL commands, system tables, config keys, and libgit2 integration. These requirements must be validated only if/where they appear in authoritative specs.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Conformance Checklist (Defer to Authoritative Specs)
Items below are captured for cross-reference only and should be verified against authoritative specs (if any cover Git integration):

[ ] Git integration SQL surface: INIT/SHOW STATUS/PULL/PUSH/CHECKOUT/CREATE BRANCH/LOG; EXPORT/IMPORT SCHEMA; migration commands; diff/conflict commands.
[ ] Deterministic schema export rules and manifest mapping (`schema/_manifest.json`).
[ ] Config surface in `.scratchbird.yml` and `sb_config.ini`, including canonical `repository.repo_*` keys and legacy aliases.
[ ] System tables: `SYS$DDL_HISTORY`, `SYS$MIGRATIONS`, `SYS$MIGRATION_LOCK`, `SYS$GIT_CONFIG`, `SYS$GIT_STATE`, `SYS$GIT_LOCK`, `SYS$GIT_SYNC_HISTORY`.
[ ] DDL tracking and Git commit linking rules (auto_commit behavior, dirty state handling).
[ ] Migration generation/validation/apply/rollback semantics and checksum rules.
[ ] Conflict detection and resolution strategies.
[ ] Security model for Git privileges and credential handling.
[ ] Error codes (`GIT###`, `MIG###`) and recovery procedures.
[ ] Implementation constraints (libgit2 only, SHA-1/SHA-256 support).

## Notes
- If Git integration is intended to be authoritative for V3, it must be added to the inventory and the non-authoritative banner removed.
