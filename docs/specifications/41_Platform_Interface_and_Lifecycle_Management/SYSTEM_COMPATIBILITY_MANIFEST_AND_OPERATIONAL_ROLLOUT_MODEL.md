# System Compatibility Manifest and Operational Rollout Model

Status: reconstructed_required_with_current_substrate

## Purpose

Define one compatibility manifest spanning on-disk, catalog, protocol, backup, index, JIT artifact, and operational rollout axes.

## Manifest axes

| Axis | Scope |
| --- | --- |
| `storage_format` | page families, headers, checksums, page sizes |
| `catalog_shape` | system tables, metadata epochs, shared-definition rows |
| `protocol_surface` | native wire payloads, listener/manager control payloads, driver-visible contracts |
| `backup_restore` | logical export/import, physical image compatibility, optional WAL rollforward applicability |
| `index_family` | admitted families, family metadata payloads, compatibility requirements for existing durable structures |
| `jit_artifact` | provider signature, toolchain signature, cache invalidation keys |
| `operational_rollout` | offline upgrade class, bounded mixed-version class, refused combinations, downgrade class |

## Rollout classes

- `IN_PLACE_COMPATIBLE`
- `OFFLINE_UPGRADE_REQUIRED`
- `LOGICAL_EXPORT_IMPORT_REQUIRED`
- `REBUILD_REQUIRED`
- `REFUSED`

## Rules

1. Every storage-affecting change shall publish a manifest update before release.
2. Downgrade remains fail closed unless the manifest explicitly marks the path reversible.
3. Mixed-version operation is allowed only for explicitly admitted combinations in the manifest.
4. JIT artifact reuse requires manifest compatibility on engine, toolchain, provider, and signature axes.
5. Index-family compatibility claims must identify whether structures are reusable, rebuild-required, or refused.

## Operational rollout rule

A rollout is supported only when all required axes classify the target pair as compatible under the same rollout class.
If any required axis is `REFUSED`, the rollout is refused.
