### Task: FDW/DBLINK documentation authoring

Goal: Author FDW core and adapters, database links, and catalog/security integration.

Input:
- `fdw*.h/.cpp`, `database_link.*`, `fdw_catalog.*`, `fdw_security.*`
- ProjectPlan Phase 10

Steps:
1. Describe FDW SPI and adapter architecture.
2. Document CSV/JSON/PostgreSQL adapters with capabilities and limits.
3. Add Implementation References per adapter.
4. Cover DDL and catalog objects; security and error handling.

Output:
- Updated `subprojects/fdw/index.md` with adapter subsections.

Validation:
- Each adapter has at least two anchors: catalog integration and query execution.
