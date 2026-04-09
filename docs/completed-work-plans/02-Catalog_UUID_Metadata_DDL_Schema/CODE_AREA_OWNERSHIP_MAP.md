# Code Area Ownership Map

## Primary Write Scopes

| Ticket | Primary write scope | Conflict surfaces | Parallelization rule |
| --- | --- | --- | --- |
| B1-02-001 | assigned section specs plus this package | all package control files | serial only |
| B1-02-002 | docs/specifications/01_Configuration_Subsystem/README.md, docs/specifications/24_Catalog_Model_and_Virtual_Overlays/README.md, docs/specifications/37_Statistics_Metadata_and_Schema_DDL/README.md, docs/work-plans/02-Catalog_UUID_Metadata_DDL_Schema/SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv, docs/work-plans/02-Catalog_UUID_Metadata_DDL_Schema/CODE_AREA_OWNERSHIP_MAP.md | package control files plus canonical audit-anchor READMEs | serial with implementation tickets |
| B1-02-003 | include/scratchbird/core/config.h, src/core/config.cpp, include/scratchbird/server/config_parser.h, src/server/config_parser.cpp, src/server/service_controller.cpp, include/scratchbird/core/ondisk.h, include/scratchbird/core/uuidv7.h, src/core/uuidv7.cpp, src/core/database.cpp, src/core/heap_page.cpp, include/scratchbird/core/catalog_manager.h, src/core/catalog_manager.cpp, src/sblr/executor.cpp | bootstrap/catalog seams, runtime config entrypoints, UUID/catalog root materialization | after ownership freeze |
| B1-02-004 | include/scratchbird/core/connection_context.h, src/core/connection_context.cpp, include/scratchbird/core/catalog_manager.h, src/core/catalog_manager.cpp, include/scratchbird/catalog/sys_catalog.h, src/catalog/sys_catalog.cpp, include/scratchbird/catalog/virtual_catalog.h, src/catalog/virtual_catalog.cpp, src/sblr/executor.cpp | catalog publication APIs, metadata invalidation, sys overlay exposure, DDL entrypoints | after lane A foundation |
| B1-02-005 | metadata and DDL gates | shared gate runners | after implementation tickets |

## Unsafe Parallel Boundaries

- any ticket that updates SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv
- any ticket that changes the same canonical spec file as another ticket
- any ticket that changes the same gate or benchmark artifact family
- any ticket that changes include/scratchbird/core/catalog_manager.h or src/core/catalog_manager.cpp
