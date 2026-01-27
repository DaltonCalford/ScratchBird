# Live Migration with Emulated Listener

Status: Draft (Beta). This document defines the end-to-end behavior for live
migration when legacy applications connect to ScratchBird via an emulated
wire protocol listener (Firebird/MySQL/PostgreSQL), while ScratchBird uses a
UDR connector to the legacy database.

## 1. Overview

**Goal:** Allow legacy applications to point at ScratchBird immediately, while
data is migrated in the background and the emulated schema becomes the new
primary database with controlled cutover and audit phases.

This specification is the canonical description for:
- Emulated listener routing during migration.
- Dual-write and dual-read audit behavior.
- Cutover and optional mirror-to-legacy rollback window.

## 2. Actors and Components

- **Legacy App**: Unmodified client (e.g., Firebird client tools).
- **ScratchBird Listener**: Emulated protocol listener for legacy dialect.
- **UDR Connector**: Remote Database UDR connection to legacy DB.
- **Emulated Schema**: `emulated_<server>` schema generated from legacy metadata.
- **Legacy Schema**: `legacy_<server>` foreign schemas and tables.

## 3. High-Level Flow

```
Legacy App
   |
   | (emulated protocol: FB/PG/MySQL)
   v
ScratchBird Listener
   |
   | Migration Router
   +------------------------------+
   |                              |
   v                              v
Legacy DB (UDR)            Emulated Schema (SB)
```

## 4. Migration Modes (State Machine)

```
PROXY_ONLY
   |
   v
EMULATED_BUILD
   |
   v
DUAL_WRITE
   |
   v
DUAL_READ_AUDIT
   |
   v
PRIMARY_EMULATED
   |
   v
MIRROR_LEGACY (optional)
   |
   v
RETIRED
```

### Mode Summary

| Mode | Reads | Writes | Audit | Source of Truth |
|------|-------|--------|-------|-----------------|
| PROXY_ONLY | Legacy | Legacy | None | Legacy |
| EMULATED_BUILD | Legacy | Legacy | None | Legacy |
| DUAL_WRITE | Legacy | Legacy + Emulated | None | Legacy |
| DUAL_READ_AUDIT | Legacy (returns) + Emulated (audit) | Legacy + Emulated | Diff logging | Legacy |
| PRIMARY_EMULATED | Emulated | Emulated (+ optional mirror) | Optional | Emulated |
| MIRROR_LEGACY | Emulated | Emulated + Legacy | Optional | Emulated |
| RETIRED | Emulated | Emulated | None | Emulated |

## 5. Routing Rules

### 5.1 Reads

- **PROXY_ONLY / EMULATED_BUILD / DUAL_WRITE**: Reads execute on legacy only.
- **DUAL_READ_AUDIT**: Reads execute on both legacy and emulated.
  - **Return source**: Legacy (default) or emulated (configurable).
- **PRIMARY_EMULATED / MIRROR_LEGACY / RETIRED**: Reads execute on emulated only.

### 5.2 Writes

- **PROXY_ONLY / EMULATED_BUILD**: Writes execute on legacy only.
- **DUAL_WRITE / DUAL_READ_AUDIT**: Writes execute on legacy first, then emulated.
  - If legacy fails, the write fails and emulated is not modified.
  - If emulated fails after legacy succeeds, attempt compensation:
    - rollback emulated (if possible), log divergence if legacy cannot roll back.
- **PRIMARY_EMULATED / MIRROR_LEGACY**: Writes execute on emulated first.
  - If mirror is enabled, apply to legacy second.
  - Mirror failure is policy-controlled (strict or lenient).

### 5.3 DDL

- DDL is always applied to emulated schema only.
- Legacy schema changes must be reflected via re-introspection or manual admin
  updates to the emulated schema template.

## 6. Audit and Comparison Rules

During **DUAL_READ_AUDIT**, each query is executed against both legacy and
emulated targets, then compared using a selected policy:

- **Row count** (required)
- **Checksum** (recommended, ordered or unordered hash)
- **Sample compare** (optional)

Mismatch handling:
- Log mismatch with query hash, counts, timing, and sample diff.
- If mismatch rate exceeds a threshold, auto-block cutover.

Audit output should be persisted for review, e.g. `sys.migration_audit_log`
(planned), or a user-defined table if system views are not yet available.

## 7. Administrative Controls

Migration control uses the Remote Database UDR admin procedures (canonical):

```sql
CALL sys.migration_begin('legacy_fb', target_schema => 'emulated_fb', mode => 'proxy_only');
CALL sys.migration_begin('legacy_fb', mode => 'emulated_build');
CALL sys.migration_begin('legacy_fb', mode => 'dual_write');
CALL sys.migration_verify('legacy_fb');   -- enters DUAL_READ_AUDIT
CALL sys.migration_cutover('legacy_fb');  -- enters PRIMARY_EMULATED
CALL sys.migration_retire('legacy_fb');   -- enters RETIRED
```

### Policy Options (server-level or migration-level)

- `migration_mode`: current mode (see state machine).
- `audit_mode`: none | sample | full
- `audit_sample_rate`: 0.0 - 1.0 (default 0.01)
- `audit_return_source`: legacy | emulated (default legacy)
- `dual_write_policy`: strict | lenient (default strict)
- `mirror_policy`: strict | lenient (default strict)
- `mirror_legacy`: true | false (default false)

## 8. Cutover Criteria

Cutover should only proceed when:
- Migration backlog is 0 (replication lag is 0).
- Row counts match for all migrated tables.
- Audit mismatch rate is below threshold.
- Admin review is complete and approved.

## 9. Rollback Strategy

- **During DUAL_WRITE / DUAL_READ_AUDIT**: Continue to use legacy as source of
  truth; reset emulated data if needed and restart migration.
- **After PRIMARY_EMULATED**: If mirror-to-legacy is enabled, rollback can
  re-route reads/writes back to legacy using recorded delta windows.

## 10. Compatibility Notes

- Firebird emulation must respect on-commit event semantics (no immediate
  events).
- Legacy procedures/functions are passed through or recompiled into emulated
  PSQL as part of schema translation.

---

**Primary references:**
- `ScratchBird/docs/specifications/Alpha Phase 2/11-Remote-Database-UDR-Specification.md`
- `ScratchBird/docs/specifications/udr_connectors/UDR_CONNECTOR_BASELINE.md`
