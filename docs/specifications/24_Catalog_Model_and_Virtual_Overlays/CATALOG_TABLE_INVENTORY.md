# Catalog Table Inventory

Status: current_authority

## Inventory model

Section `24` tracks current inventory in three authoritative groups.

### Persisted families

Current source proves persisted family presence for groups that include:
- core catalog and schema control-plane rows
- security and authentication control-plane rows
- resource and localization row families
- remote metadata snapshot rows
- cluster fabric metadata rows
- execution-artifact and plan-adjacent rows where current code persists them

### Virtual overlay families

Current source proves virtual overlay exposure for:
- `information_schema`
- `sys_catalog`
- engine-specific overlays registered through the virtual catalog router under profile gating

### Shared exposure families

The following families are current but shared with adjacent sections for deeper runtime meaning:
- metadata and statistics families
- execution-artifact families
- remote connector metadata
- cluster fabric metadata
- security row families

## Fail-closed inventory boundaries

This file does not claim:
- a full branch and changeset family inventory
- universal donor-engine overlay parity
- exhaustive installation-time bundle inventories beyond current loader-backed families
