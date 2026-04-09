# Code Area Ownership Map

## Primary Write Scopes

| Ticket | Primary write scope | Conflict surfaces | Parallelization rule |
| --- | --- | --- | --- |
| B1-07-001 | package `07` control files plus consumed section `25`, `30`, `39`, and `41` specs | all package control files | serial only |
| B1-07-002 | CODE_AREA_OWNERSHIP_MAP.md, SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv, README.md, MASTER_TRACKER.md, MASTER_TRACKER.csv, ORDERED_TASK_TICKETS.csv | package tracker files and any spec named in the audit matrix | serial with all later tickets |
| B1-07-003 | src/core/backup_manager.cpp, src/core/database.cpp, src/core/catalog_manager.cpp, src/sblr/executor.cpp, src/server/sb_manager_main.cpp, tests/unit/test_backup_sql_admin_api.cpp, tests/unit/test_backup_tablespace_manifest.cpp, tests/unit/test_restore_validation_rehearsal.cpp, tests/unit/test_shadow_filespaces.cpp, tests/unit/test_tablespace_migration_index_updates.cpp, tests/unit/test_manager_proxy_mcp.cpp | backup, migration, shadow, SQL admin, and manager-status seams | after ownership freeze |
| B1-07-004 | scripts/cross_os/generate_package_manifests.sh, scripts/cross_os/run_portable_lane.sh, scripts/release/verify_repro_build.py, artifacts/cross_os/p6s3w2/package_manifests/, docs/TEST.md, section `41` specs | cloud and platform seams plus shared release-tooling surfaces | after lane A foundation |
| B1-07-005 | package evidence/, package gates/, docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/TEST_CONTRACT.md, docs/specifications/41_Platform_Interface_and_Lifecycle_Management/TEST_CONTRACT.md | shared gate runners, preserved logs, and evidence summaries | after implementation tickets |

## Unsafe Parallel Boundaries

- any ticket that updates SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv
- any ticket that changes the same canonical spec file as another ticket
- any ticket that changes the same gate or benchmark artifact family
- lane A backup or migration runtime edits and lane B release-tooling edits both
  touch package trackers and evidence summaries, so they must merge serially
