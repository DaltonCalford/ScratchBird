# Unique Features and Competitive Advantages (Spec Review)

## Purpose
Summarize **unique or differentiating features** called out across Alpha and
Beta specifications, with pointers to the source specs. This is a **spec-based
summary** (not an implementation audit).

## Alpha Differentiators (Specified for Alpha)

### MGA core (no mandatory WAL)
- Multi-Generational Architecture with snapshot visibility and fast commit
  semantics; write-after log optional.
- **Competitive angle:** avoids the write-ahead log dependency and reduces
  reader/writer blocking in OLTP workloads.
- References:
  - `docs/specifications/transaction/TRANSACTION_MGA_CORE.md`
  - `docs/specifications/transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md`

### SBLR bytecode VM + untrusted parser model
- SQL is compiled to SBLR, verified by the engine before execution.
- Parsers are treated as untrusted clients; engine enforces safety rules.
- **Competitive angle:** smaller injection surface and more deterministic
  execution; strict separation between dialect parsers and engine.
- References:
  - `docs/specifications/sblr/Appendix_A_SBLR_BYTECODE.md`
  - `docs/specifications/sblr/SBLR_OPCODE_REGISTRY.md`
  - `docs/specifications/parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md`

### UUID-centric object identity
- All SQL objects are identified by UUID internally for execution and auditing.
- **Competitive angle:** stable identity across schema changes and consistent
  audit/log references.
- References:
  - `docs/specifications/types/UUID_IDENTITY_COLUMNS.md`
  - `docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`

### Schema path resolution + recursive namespace model
- Multi-level schema paths, ordered search paths, and strict namespace rules.
- Emulated sessions can be scoped to emulated roots without polluting native
  namespaces.
- **Competitive angle:** strong multi-tenant isolation and clear namespace
  governance for emulated dialects.
- References:
  - `docs/specifications/catalog/SCHEMA_PATH_RESOLUTION.md`
  - `docs/specifications/catalog/SCHEMA_PATH_SECURITY_DEFAULTS.md`

### Domain system with security and quality rules
- Domains act as "smart types" with constraints, masking, validation, and
  quality pipelines bound to the type itself.
- **Competitive angle:** enforcement moves from application to type system,
  reducing drift across DML and PSQL.
- References:
  - `docs/specifications/types/DDL_DOMAINS_COMPREHENSIVE.md`
  - `docs/specifications/types/03_TYPES_AND_DOMAINS.md`

### Integrated Git metadata support (Alpha optional)
- Built-in Git metadata tracking for schema/migration history and audit.
- **Competitive angle:** GitOps-style database governance without external
  tooling glue.
- Reference:
  - `docs/specifications/core/GIT_METADATA_INTEGRATION_SPECIFICATION.md`

### Listener/pool/parser isolation for emulation
- Dedicated listeners per dialect, parser pool model, and strict protocol
  translation to SBLR.
- **Competitive angle:** emulation parity without merging dialect semantics
  into core engine behavior.
- References:
  - `docs/specifications/network/`
  - `docs/specifications/wire_protocols/`

### Online tablespace migration (Alpha/Beta boundary)
- Online migration semantics defined; Alpha uses the foundation for tablespace
  awareness.
- **Competitive angle:** lower downtime for storage rebalancing.
- Reference:
  - `docs/specifications/storage/TABLESPACE_ONLINE_MIGRATION.md`

### Job scheduler (Alpha)
- Native job scheduler, cron support, security hooks, and audit integration.
- **Competitive angle:** built-in scheduling without external cron/agent layers.
- References:
  - `docs/specifications/scheduler/ALPHA_SCHEDULER_SPECIFICATION.md`
  - `docs/specifications/scheduler/SCHEDULER_JOB_RUNNER_CANONICAL_SPEC.md`

## Beta Differentiators (Planned)

### Cluster replication and distributed MGA
- UUIDv8-HLC conflict resolution, leaderless quorum replication, and
  time-partitioned Merkle forests.
- **Competitive angle:** replication model designed around MGA rather than
  retrofitting MVCC.
- References:
  - `docs/specifications/beta_requirements/replication/uuidv7-optimized/`
  - `docs/specifications/Cluster Specification Work/`

### Military-grade cluster security
- Cluster CA policy, identity bundles, node trust, and enforcement primitives.
- **Competitive angle:** strong trust boundaries and explicit security posture
  for multi-node deployments.
- References:
  - `docs/specifications/Cluster Specification Work/SBCLUSTER-03-CA-POLICY.md`
  - `docs/specifications/Cluster Specification Work/SBCLUSTER-04-SECURITY-BUNDLE.md`

### Multi-model / NoSQL language coverage
- Catalog models for document, graph, key-value, column-family, and time-series
  data, plus dedicated language specs (AQL, CQL, Cypher, Gremlin, SPARQL, etc.).
- **Competitive angle:** SQL-first engine with multi-model extensions rather
  than separate NoSQL systems.
- References:
  - `docs/specifications/beta_requirements/nosql/`
  - `docs/specifications/beta_requirements/nosql/languages/`

### Remote DB UDRs + live migration workflows
- Connect to MySQL/PostgreSQL/FirebirdSQL/MSSQL/ODBC/JDBC without external
  client libraries; supports passthrough and dual-write migration workflows.
- **Competitive angle:** built-in migration and coexistence tooling.
- References:
  - `docs/specifications/remote_database_udr/`

### Parallel execution + distributed query
- Parallel execution architecture and distributed query planning.
- **Competitive angle:** scale-out without losing MGA semantics.
- References:
  - `docs/specifications/query/PARALLEL_EXECUTION_ARCHITECTURE.md`
  - `docs/specifications/Cluster Specification Work/SBCLUSTER-06-DISTRIBUTED-QUERY.md`

### Ecosystem breadth (drivers/tools/apps)
- P0/P1/P2 driver coverage, ODBC/JDBC, BI tools, application-specific
  connectors.
- **Competitive angle:** compatibility breadth with strict protocol parity.
- Reference:
  - `docs/specifications/beta_requirements/README.md`

### Signed runtime bundles and secure build chains
- Signed runtime bundle plans and build validation for hardened deployments.
- **Competitive angle:** reduced supply-chain risk for regulated environments.
- References:
  - `docs/specifications/deployment/INSTALLATION_AND_BUILD_SPECIFICATION.md`

## Notes
- Alpha items are either implemented or queued for completion before public
  release. For live status, see:
  - `docs/planning/ALPHA_COMPLETION_MASTER_PLAN.md`
  - `docs/findings/ALPHA_BETA_SCOPE_STATUS.md`
