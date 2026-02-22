# V3 Partial Items Workplan Input (Beta 0.1.0)
Last modified: 2026-02-19

Purpose:
- Consolidated list of all currently partial or not-available items from `docs/user-documentation/language-guide`.
- Intended as direct input for implementation planning to close v3 before/after beta 0.1.0.

Source roots:
- `docs/user-documentation/language-guide/`
- `docs/user-documentation/language-guide/TODO_BETA_0_2_0.md`

## 1. DDL Lifecycle Gaps (Command Surface)

### 1.1 Management
- TABLESPACE: missing SHOW and DESCRIBE lifecycle phases.
  - Source: `docs/user-documentation/language-guide/ddl/management/tablespace/README.md`

### 1.2 Data Storage
- TYPE: missing SHOW and DESCRIBE lifecycle phases; create runtime closure still partial.
  - Source: `docs/user-documentation/language-guide/ddl/data-storage/type/README.md`
- SEARCH INDEX: ALTER is partial (`REBUILD` only); missing SHOW and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/data-storage/search-index/README.md`
- VECTOR INDEX: ALTER is partial (`REBUILD` only); missing SHOW and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/data-storage/vector-index/README.md`
- MEASUREMENT: missing SHOW, DESCRIBE, and DROP.
  - Source: `docs/user-documentation/language-guide/ddl/data-storage/measurement/README.md`
- ACCESS METHOD: create-only surface (ALTER/SHOW/DESCRIBE/DROP not available).
  - Source: `docs/user-documentation/language-guide/ddl/data-storage/access-method/README.md`
- STATISTICS: create-only surface (ALTER/SHOW/DESCRIBE/DROP not available).
  - Source: `docs/user-documentation/language-guide/ddl/data-storage/statistics-object/README.md`
- TRANSFORM: create-only surface (ALTER/SHOW/DESCRIBE/DROP not available).
  - Source: `docs/user-documentation/language-guide/ddl/data-storage/transform/README.md`

### 1.3 Programmability
- EXCEPTION: missing ALTER, SHOW, and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/programmability/exception/README.md`
- UDR: missing ALTER, SHOW, and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/programmability/udr/README.md`

### 1.4 Security
- USER: missing SHOW and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/security/user/README.md`
- ROLE: missing ALTER and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/security/role/README.md`
- GROUP: missing ALTER, SHOW, and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/security/group/README.md`
- POLICY: missing SHOW and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/security/policy/README.md`
- TOKEN: missing SHOW and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/security/token/README.md`
- QUOTA PROFILE: missing SHOW and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/security/quota-profile/README.md`
- CONNECTION RULE: missing SHOW and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/security/connection-rule/README.md`

### 1.5 Integration
- REPLICATION CHANNEL: missing SHOW and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/integration/replication-channel/README.md`
- PUBLICATION: missing ALTER, SHOW, and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/integration/publication/README.md`
- SUBSCRIPTION: missing ALTER, SHOW, and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/integration/subscription/README.md`
- CDC TABLE: missing SHOW and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/integration/cdc-table/README.md`
- DATABASE CONNECTION: missing SHOW and DESCRIBE; runtime currently normalized through admin/config key path.
  - Source: `docs/user-documentation/language-guide/ddl/integration/database-connection/README.md`
- FOREIGN DATA WRAPPER: create-only surface (ALTER/SHOW/DESCRIBE/DROP not available).
  - Source: `docs/user-documentation/language-guide/ddl/integration/foreign-data-wrapper/README.md`
- FOREIGN SERVER: missing SHOW and DESCRIBE; runtime closure not fully canonicalized.
  - Source: `docs/user-documentation/language-guide/ddl/integration/foreign-server/README.md`
- FOREIGN TABLE: missing SHOW and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/integration/foreign-table/README.md`
- USER MAPPING: missing ALTER, SHOW, and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/integration/user-mapping/README.md`
- SYNONYM: missing SHOW and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/integration/synonym/README.md`
- EXTENSION: missing SHOW and DESCRIBE.
  - Source: `docs/user-documentation/language-guide/ddl/integration/extension/README.md`

### 1.6 Cluster And Service
- CUBE: parser/emitter command lifecycle exists, runtime semantic bridge remains partial.
  - Source: `docs/user-documentation/language-guide/ddl/cluster-and-service/cube/README.md`
- CLUSTER WORKLOAD/ADMISSION objects: command lifecycle exists, runtime bridge remains partial.
  - Source: `docs/user-documentation/language-guide/ddl/cluster-and-service/*/README.md`

## 2. Admin / Runtime Bridge Partials

- BACKUP/RESTORE runtime closure partial.
  - Source: `docs/user-documentation/language-guide/admin/connectivity-and-operations/backup-restore/README.md`
- VALIDATE/ANALYZE maintenance runtime closure partial.
  - Source: `docs/user-documentation/language-guide/admin/connectivity-and-operations/validate-analyze-maintenance/README.md`
- SWEEP/VACUUM runtime closure partial.
  - Source: `docs/user-documentation/language-guide/admin/connectivity-and-operations/sweep-vacuum/README.md`
- RESYNC REPLICATION CHANNEL runtime closure partial.
  - Source: `docs/user-documentation/language-guide/admin/connectivity-and-operations/resync-replication/README.md`
- CLUSTER CONTROL runtime semantic bridge partial.
  - Source: `docs/user-documentation/language-guide/admin/cluster-and-service/cluster-control/README.md`
- SERVICE CHANNEL runtime semantic bridge partial.
  - Source: `docs/user-documentation/language-guide/admin/cluster-and-service/service-channel/README.md`
- NoSQL bridge families runtime partial:
  - CQL: `docs/user-documentation/language-guide/admin/nosql-bridges/cql-family/README.md`
  - MONGO: `docs/user-documentation/language-guide/admin/nosql-bridges/mongo-family/README.md`
  - CYPHER: `docs/user-documentation/language-guide/admin/nosql-bridges/cypher-family/README.md`
  - REDIS: `docs/user-documentation/language-guide/admin/nosql-bridges/redis-family/README.md`
  - MILVUS: `docs/user-documentation/language-guide/admin/nosql-bridges/milvus-family/README.md`
- CREATE DATABASE EMULATED runtime path is partial/mixed and needs normalization.
  - Source: `docs/user-documentation/language-guide/admin/emulation/create-database-emulated/README.md`

## 3. PSQL Gaps

- SET TERM: parser/emitter supported, but script delimiter splitting remains external workflow.
  - Source: `docs/user-documentation/language-guide/psql/routine-structure/set-term/README.md`
- Dollar-quoted routine bodies (`$$ ... $$`): not available in native v3.
  - Source: `docs/user-documentation/language-guide/psql/routine-structure/dollar-quoted-bodies/README.md`
- TRY/CATCH block syntax: not available in native v3.
  - Source: `docs/user-documentation/language-guide/psql/error-handling/try-block/README.md`

## 4. DML Runtime Gaps

- SELECT subquery-membership remains partial:
  - `IN (subquery)`
  - `NOT IN (subquery)`
  - Source: `docs/user-documentation/language-guide/dml/query/select/statement.md`

## 5. Data-Type / Cast / Operator / Function Partials

- Wide numeric operations partial for some `DIV`, modulo, and bitwise operator combinations (`INT256`/`UINT256`/`DECIMAL256`).
  - Source: `docs/user-documentation/language-guide/data-types/families/numeric.md`
  - Source: `docs/user-documentation/language-guide/data-types/casts/implicit-casts.md`
  - Source: `docs/user-documentation/language-guide/data-types/operators/logical-bitwise.md`
- JSON existence operator specificity partial: `?`, `?|`, `?&` currently collapse to generic exists opcode mapping.
  - Source: `docs/user-documentation/language-guide/data-types/operators/pattern-json-array.md`
- Subquery membership operator partial (`IN (subquery)` / `NOT IN (subquery)`).
  - Source: `docs/user-documentation/language-guide/data-types/operators/arithmetic-comparison.md`
- Value window functions partial (`LAG`, `LEAD`, `FIRST_VALUE`, `LAST_VALUE`, `NTH_VALUE`) due fallback opcode path.
  - Source: `docs/user-documentation/language-guide/functions/window/value-window-functions.md`
- Aggregate DISTINCT parser closure partial (`COUNT(DISTINCT ...)` parser gap noted).
  - Source: `docs/user-documentation/language-guide/functions/aggregate/core-aggregates.md`

## 6. Cross-Cutting TODOs (0.2.0 Gate)

- Cube runtime semantic bridge closure (replace `IRX_0406` reject path with handlers/tests).
- Admin and NoSQL semantic bridge closure (positive execution handlers/tests by family).
- Lifecycle closure for remaining partial object families.

Source:
- `docs/user-documentation/language-guide/TODO_BETA_0_2_0.md`
