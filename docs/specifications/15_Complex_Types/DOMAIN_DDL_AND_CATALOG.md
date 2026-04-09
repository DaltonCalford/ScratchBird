# Domain DDL and Catalog

Status: current_authority

## Current authoritative domain control-plane surface

The current code-backed domain authority is DomainManager and its catalog persistence path.

Directly audited APIs include:
- createBasicDomain
- createRecordDomain
- createEnumDomain
- createSetDomain
- createVariantDomain
- createRangeDomain
- createBaseDomain
- createShellDomain
- ensureSystemDomains
- extractField

## Domain-kind capability matrix

| Domain kind | Current authority | Main boundary |
| --- | --- | --- |
| BASIC | runtime supported and catalog backed | exact front-door breadth is not current proof |
| RECORD | runtime supported and catalog backed | exact SQL front door is not current proof |
| ENUM | runtime supported and catalog backed with bounded storage model | current model is VARCHAR-backed metadata, not one separately closed standalone payload family |
| SET | runtime supported and catalog backed with bounded storage model | current model is ARRAY-backed metadata, not one separately closed standalone payload family |
| VARIANT | runtime supported and catalog backed | exact front-door breadth is not current proof |
| RANGE | runtime supported and catalog backed | full surface breadth remains unproven |
| BASE | runtime supported and catalog backed | exact front-door breadth is not current proof |
| SHELL | runtime supported and catalog backed | finalization and front-door closure remain unproven |
| SYSTEM | runtime supported and catalog backed with deterministic bootstrap handling | exhaustive historical registry breadth remains unproven |

## Catalog persistence truth

DomainManager writeDomainRecord proves:
- domain records are persisted through catalog pages rather than being process-local only
- domain create and alter events append a control-plane event before record persistence
- record placement scans existing catalog pages and allocates chained catalog pages as needed
- domain identity conflicts fail closed through the control-plane event path

## Fail-closed boundary

This section does not treat older prose as proof for:
- one exact SQL grammar for every domain kind
- one fully re-audited catalog table and column schema matching historical docs verbatim
- one fully re-audited registry of every historical system or emulation domain UUID row
