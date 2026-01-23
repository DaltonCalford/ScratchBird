# ScratchBird Competitive Feature Gaps (SQL Server, Oracle, Snowflake)

Scope: This report compares the DeepDiveResearch notes against current ScratchBird specs under `ScratchBird/docs/specifications`. It tracks features that are missing, only lightly specified, or clearly less mature than the reference engines.

Owners are assigned by team-role name. Replace with individual owners as work is assigned.

## Availability, replication, and recovery
| Checklist | Gap | Tag | Owner | Milestone | Spec refs | Code audit | Priority rationale |
| --- | --- | --- | --- | --- | --- | --- | --- |
| [ ] | Always On AG/FCI (SQL Server), RAC/Data Guard/GoldenGate (Oracle) parity | Beta | Cluster | Beta-Cluster-1 | `ScratchBird/docs/specifications/beta_requirements/replication/README.md`; `ScratchBird/docs/specifications/Cluster Specification Work/` | Audit: pending | Enterprise HA/DR requirement, depends on cluster foundation. |
| [ ] | Log-based PITR and log shipping | Post-gold | Storage | Post-gold-Recovery-1 | `ScratchBird/docs/specifications/BACKUP_AND_RESTORE.md` | Audit: pending | MGA + temporal tables reduce need; optional for replication/PITR. |
| [ ] | Time travel + zero-copy clone (Snowflake-style) | Post-gold | Storage | Post-gold-Storage-1 | `ScratchBird/docs/specifications/ddl/DDL_TEMPORAL_TABLES.md`; `ScratchBird/docs/specifications/BACKUP_AND_RESTORE.md` | Audit: pending | Developer convenience and analytics workflows, not core engine. |
| [ ] | Automated HA orchestration with synchronized failover semantics | Beta | Cluster | Beta-Cluster-2 | `ScratchBird/docs/specifications/Cluster Specification Work/` | Audit: pending | Requires cluster manager and state coordination. |

## Storage and architecture
| Checklist | Gap | Tag | Owner | Milestone | Spec refs | Code audit | Priority rationale |
| --- | --- | --- | --- | --- | --- | --- | --- |
| [ ] | Storage/compute separation and elastic warehouses | Post-gold | Cloud | Post-gold-Cloud-1 | Spec: MISSING (propose `ScratchBird/docs/specifications/beta_requirements/cluster/STORAGE_COMPUTE_SEPARATION.md`) | Audit: pending | Cloud service feature, not required for Alpha/Beta core. |
| [ ] | Micro-partitioning + automatic clustering/pruning | Beta | Storage | Beta-OLAP-1 | `ScratchBird/docs/specifications/ddl/DDL_TABLE_PARTITIONING.md`; `ScratchBird/docs/specifications/indexes/ZoneMapsIndex.md` | Audit: pending | Large-scale analytic performance; beyond basic partitioning. |
| [ ] | Exadata Smart Scan, storage indexes, HCC | Post-gold | Storage | Post-gold-Storage-2 | `ScratchBird/docs/specifications/compression/COMPRESSION_FRAMEWORK.md` | Audit: pending | Hardware-specific optimization, not core engine. |
| [ ] | In-memory OLTP (Hekaton-style memory-optimized tables) | Beta | Execution | Beta-Perf-1 | `ScratchBird/docs/specifications/sblr/SBLR_EXECUTION_PERFORMANCE_ALPHA.md`; `ScratchBird/docs/specifications/sblr/SBLR_EXECUTION_PERFORMANCE_BETA.md` | Audit: pending | Hot-OLTP performance; requires dedicated storage/executor path. |

## Query optimization and execution
| Checklist | Gap | Tag | Owner | Milestone | Spec refs | Code audit | Priority rationale |
| --- | --- | --- | --- | --- | --- | --- | --- |
| [ ] | Query Store (plan history, forcing, regression tracking) | Beta | Query | Beta-Optimizer-1 | `ScratchBird/docs/specifications/query/QUERY_OPTIMIZER_SPEC.md` | Audit: pending | Operational stability and plan regression control. |
| [ ] | Batch-mode vectorized execution for columnstore | Beta | Execution | Beta-OLAP-2 | `ScratchBird/docs/specifications/indexes/COLUMNSTORE_SPEC.md` | Code: `ScratchBird/src/core/columnstore.cpp:1`; `ScratchBird/src/core/storage_engine.cpp:635` | Columnstore exists but lacks batch-mode engine for competitive analytics. |
| [ ] | Autonomous indexing and auto-tuning | Post-gold | Query | Post-gold-Optimizer-1 | Spec: MISSING (auto-tuning) | Audit: pending | Complex automation; not required for Alpha/Beta core. |

## Security and governance
| Checklist | Gap | Tag | Owner | Milestone | Spec refs | Code audit | Priority rationale |
| --- | --- | --- | --- | --- | --- | --- | --- |
| [ ] | Always Encrypted / client-side column encryption | Post-gold | Security | Post-gold-Security-1 | `ScratchBird/docs/specifications/Security Design Specification/04_ENCRYPTION_KEY_MANAGEMENT.md` | Audit: pending | Requires client driver/key escrow integration. |
| [ ] | SQL Firewall (statement allowlist/learning mode) | Post-gold | Security | Post-gold-Security-2 | Spec: MISSING (SQL firewall) | Audit: pending | Optional hardening feature; not core engine. |
| [ ] | Database Vault + Label Security | Post-gold | Security | Post-gold-Security-3 | Spec: MISSING (label security / vault) | Audit: pending | Enterprise compliance feature. |
| [ ] | Data redaction framework parity (Oracle-level) | Alpha | Security | Alpha-Security-1 | `ScratchBird/docs/specifications/Security Design Specification/03_AUTHORIZATION_MODEL.md` | Code: `ScratchBird/src/core/data_masking.cpp:1`; `ScratchBird/src/sblr/executor.cpp:9266` | Core data protection; existing masking is baseline only. |
| [ ] | Unified governance + data sharing (Snowflake Horizon/Marketplace) | Post-gold | Cloud | Post-gold-Governance-1 | Spec: MISSING (governance/data sharing) | Audit: pending | SaaS governance feature, not core engine. |

## Converged data model and AI
| Checklist | Gap | Tag | Owner | Milestone | Spec refs | Code audit | Priority rationale |
| --- | --- | --- | --- | --- | --- | --- | --- |
| [ ] | JSON relational duality views | Post-gold | Types | Post-gold-Types-1 | `ScratchBird/docs/specifications/types/03_TYPES_AND_DOMAINS.md` | Audit: pending | Advanced JSON ergonomics; not core engine. |
| [ ] | Property graph + SQL/PGQ | Post-gold | Query | Post-gold-Query-1 | Spec: MISSING (property graph/PGQ) | Audit: pending | Specialized workload; optional. |
| [ ] | In-database model hosting and inference | Post-gold | AI/UDR | Post-gold-AI-1 | Spec: MISSING (model hosting/inference) | Audit: pending | Requires model runtime integration. |
| [ ] | Embedding generation + model lifecycle management | Post-gold | AI/Types | Post-gold-AI-2 | `ScratchBird/docs/specifications/indexes/IVFIndex.md` | Audit: pending | Complementary to vector search; not core engine. |

## Cloud-native operations
| Checklist | Gap | Tag | Owner | Milestone | Spec refs | Code audit | Priority rationale |
| --- | --- | --- | --- | --- | --- | --- | --- |
| [ ] | Multi-cloud service layer, auto-scaling compute, result cache tiers | Post-gold | Cloud | Post-gold-Cloud-2 | Spec: MISSING (cloud service layer/warehouses) | Audit: pending | SaaS delivery feature; out of core scope. |
| [ ] | Data marketplace and external sharing primitives | Post-gold | Cloud | Post-gold-Cloud-3 | Spec: MISSING (data sharing/marketplace) | Audit: pending | SaaS ecosystem feature. |

## Notes
- MGA is a deliberate design choice; several enterprise features in SQL Server and Oracle depend on redo/undo log infrastructure and are not part of ScratchBird's Alpha scope. `ScratchBird/docs/specifications/BACKUP_AND_RESTORE.md`
- Write-after log (WAL) can support some enterprise features, but shadow databases, live failover, and distributed data (cross-cluster dispersion) may reduce or remove the need for WAL-based recovery in ScratchBird.
