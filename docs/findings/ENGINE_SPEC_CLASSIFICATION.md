# Engine Spec Classification (Core vs Non-Core)

## Purpose
Classify all specifications into core engine scope vs non-core (parser/network/cluster/driver),
so we can consolidate engine requirements and compare against implementation.

## Categories
- **Core Engine**: storage, catalog, transactions, indexes, SBLR execution, DDL/DML execution semantics, core security, scheduler, monitoring, etc.
- **Non-Core**: parsers, network/listeners, wire protocols, drivers/clients, tools, deployment, remote DB UDRs/connectors.
- **Non-Core (Beta/Cluster)**: cluster and beta-only specs.
- **Mixed**: contains both core and non-core content; core portions should be extracted into the unified spec.
- **Reference/Archive**: background or historical docs, not normative for Alpha.

## Inventory
| Specification | Category | Notes |
|---|---|---|
| `Alpha Phase 2/00-Implementation-Roadmap.md` | Non-Core (Beta/Cluster) |  |
| `Alpha Phase 2/01-Architecture-Overview.md` | Non-Core (Beta/Cluster) |  |
| `Alpha Phase 2/02-Clock-Synchronization-Specification.md` | Non-Core (Beta/Cluster) |  |
| `Alpha Phase 2/03-Distributed-MVCC-Specification.md` | Non-Core (Beta/Cluster) |  |
| `Alpha Phase 2/04-Replication-Protocol-Specification.md` | Non-Core (Beta/Cluster) |  |
| `Alpha Phase 2/05-Wire-Protocol-Integration-Specification.md` | Non-Core (Beta/Cluster) |  |
| `Alpha Phase 2/06-Ingestion-Layer.md` | Non-Core (Beta/Cluster) |  |
| `Alpha Phase 2/07-OLAP-Tier.md` | Non-Core (Beta/Cluster) |  |
| `Alpha Phase 2/08-Deployment-Guide.md` | Non-Core (Beta/Cluster) |  |
| `Alpha Phase 2/09-Monitoring-Observability.md` | Non-Core (Beta/Cluster) |  |
| `Alpha Phase 2/10-UDR-System-Specification.md` | Non-Core (Beta/Cluster) |  |
| `Alpha Phase 2/11-Remote-Database-UDR-Specification.md` | Non-Core (Beta/Cluster) |  |
| `Alpha Phase 2/11a-Connection-Pool-Implementation.md` | Non-Core (Beta/Cluster) |  |
| `Alpha Phase 2/11b-PostgreSQL-Client-Implementation.md` | Non-Core (Beta/Cluster) |  |
| `Alpha Phase 2/11c-MySQL-Client-Implementation.md` | Non-Core (Beta/Cluster) |  |
| `Alpha Phase 2/11d-MSSQL-Client-Implementation.md` | Non-Core (Beta/Cluster) |  |
| `Alpha Phase 2/11e-Firebird-Client-Implementation.md` | Non-Core (Beta/Cluster) |  |
| `Alpha Phase 2/Discussion_Notes.md` | Non-Core (Beta/Cluster) |  |
| `Alpha Phase 2/README.md` | Non-Core (Beta/Cluster) |  |
| `BACKUP_AND_RESTORE.md` | Core Engine |  |
| `Cluster Specification Work/README.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/SBCLUSTER-00-GUIDING-PRINCIPLES.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/SBCLUSTER-01-CLUSTER-CONFIG-EPOCH.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/SBCLUSTER-02-MEMBERSHIP-AND-IDENTITY.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/SBCLUSTER-03-CA-POLICY.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/SBCLUSTER-04-SECURITY-BUNDLE.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/SBCLUSTER-05-SHARDING.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/SBCLUSTER-06-DISTRIBUTED-QUERY.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/SBCLUSTER-07-REPLICATION.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/SBCLUSTER-08-BACKUP-AND-RESTORE.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/SBCLUSTER-09-SCHEDULER.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/SBCLUSTER-10-OBSERVABILITY.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/SBCLUSTER-11-SHARD-MIGRATION-AND-REBALANCING.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/SBCLUSTER-AI-HANDOFF.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/SBCLUSTER-IMPLEMENTATION-BOUNDARY.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/SBCLUSTER-NORMATIVE-LANGUAGE.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/SBCLUSTER-SUMMARY.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/SBCLUSTER-THREAT-MODEL.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/sbsec_handoff_summary.md` | Non-Core (Beta/Cluster) |  |
| `Cluster Specification Work/scratch_bird_cluster_architecture_security_specifications_draft.md` | Non-Core (Beta/Cluster) |  |
| `FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md` | Non-Core (Parser) |  |
| `MEMORY_MANAGEMENT.md` | Core Engine |  |
| `MYSQL_PARSER_IMPLEMENTATION_GAPS.md` | Non-Core (Parser) |  |
| `PARSER_REMAPPING_AND_IMPLEMENTATION_STRATEGY.md` | Non-Core (Parser) |  |
| `PERFORMANCE_BENCHMARKS.md` | Core Engine |  |
| `POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md` | Non-Core (Parser) |  |
| `README.md` | Non-Core (Index) | Specification index only. |
| `Security Design Specification/00_SECURITY_SPEC_INDEX.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/01_SECURITY_ARCHITECTURE.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/02_IDENTITY_AUTHENTICATION.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/03_AUTHORIZATION_MODEL.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/04.01_KEY_LIFECYCLE_STATE_MACHINES.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/04.02_KEY_MATERIAL_HANDLING_REQUIREMENTS.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/04.03_NONCE_IV_SPECIFICATION.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/04_ENCRYPTION_KEY_MANAGEMENT.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/05.A_IPC_WIRE_FORMAT_AND_EXAMPLES.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/05_IPC_SECURITY.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/06.01_QUORUM_PROPOSAL_CANONICALIZATION_HASHING.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/06.02_QUORUM_EVIDENCE_AND_AUDIT_COUPLING.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/06_CLUSTER_SECURITY.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/07_NETWORK_PRESENCE_BINDING.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/08.01_AUDIT_EVENT_CANONICALIZATION.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/08.02_AUDIT_CHAIN_VERIFICATION_CHECKPOINTS.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/08_AUDIT_COMPLIANCE.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/09_SECURITY_LEVELS.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/10_RELEASE_INTEGRITY_PROVENANCE.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/AUTH_CERTIFICATE_TLS.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/AUTH_CORE_FRAMEWORK.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/AUTH_ENTERPRISE_LDAP_KERBEROS.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/AUTH_MODERN_OAUTH_MFA.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/AUTH_PASSWORD_METHODS.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/EXTERNAL_AUTHENTICATION_DESIGN.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/README.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/ROLE_COMPOSITION_AND_HIERARCHIES.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/archive security work/00_SECURITY_SPEC_INDEX.md` | Archive |  |
| `Security Design Specification/archive security work/01_SECURITY_ARCHITECTURE.md` | Archive |  |
| `Security Design Specification/archive security work/02_IDENTITY_AUTHENTICATION.md` | Archive |  |
| `Security Design Specification/archive security work/03_AUTHORIZATION_MODEL.md` | Archive |  |
| `Security Design Specification/archive security work/04_ENCRYPTION_KEY_MANAGEMENT.md` | Archive |  |
| `Security Design Specification/archive security work/05_IPC_SECURITY.md` | Archive |  |
| `Security Design Specification/archive security work/06_CLUSTER_SECURITY.md` | Archive |  |
| `Security Design Specification/archive security work/07_NETWORK_PRESENCE_BINDING.md` | Archive |  |
| `Security Design Specification/archive security work/08_AUDIT_COMPLIANCE.md` | Archive |  |
| `Security Design Specification/archive security work/09_SECURITY_LEVELS.md` | Archive |  |
| `Security Design Specification/archive security work/Beta Task -Distributed Secret Sharing Implementation Specification.md` | Archive |  |
| `Security Design Specification/archive security work/Engine Internal Security.md` | Archive |  |
| `Security Design Specification/archive security work/SECURITY_IMPLIMENTATION_DETAILS.md` | Archive |  |
| `Security Design Specification/archive security work/SECURITY_SYSTEM_SPECIFICATION.md` | Archive |  |
| `Security Design Specification/archive security work/Security Hardening Guide.md` | Archive |  |
| `Security Design Specification/archive security work/draft_security_architecture_specification.md` | Archive |  |
| `Security Design Specification/contributor_security_rules.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/sbsec_alpha_base.md` | Mixed | Contains engine security + cluster/network security sections. |
| `Security Design Specification/supportability_contract.md` | Mixed | Contains engine security + cluster/network security sections. |
| `TEMPORARY_TABLES_SPECIFICATION.md` | Core Engine |  |
| `V2_PARSER_FIREBIRD_ALIGNMENT_SPECIFICATION.md` | Non-Core (Parser) |  |
| `V2_PARSER_INDEX_TYPE_COMPLETENESS.md` | Non-Core (Parser) |  |
| `admin/README.md` | Non-Core |  |
| `admin/SB_ADMIN_CLI_SPECIFICATION.md` | Non-Core |  |
| `admin/SB_SERVER_NETWORK_CLI_SPECIFICATION.md` | Non-Core |  |
| `api/CLIENT_LIBRARY_API_SPECIFICATION.md` | Non-Core |  |
| `api/CONNECTION_POOLING_SPECIFICATION.md` | Non-Core |  |
| `api/README.md` | Non-Core |  |
| `archive/AnalysisOfBestParsingStructures.md` | Archive |  |
| `archive/DraftQueryOptimizationSpecification.md` | Archive |  |
| `archive/README.md` | Archive |  |
| `archive/Specification for a Multi-Generational Database Architecture.md` | Archive |  |
| `beta_requirements/00_DRIVERS_AND_INTEGRATIONS_INDEX.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/COMPLETION_STATUS.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/DIRECTORY_STRUCTURE_CREATED.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/ai-ml/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/applications/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/big-data-streaming/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/builds/00_BUILD_REQUIREMENTS_INDEX.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/builds/01_LINUX_NATIVE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/builds/02_WINDOWS_NATIVE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/builds/03_MACOS_NATIVE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/builds/10_LINUX_TO_WINDOWS.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/builds/11_LINUX_TO_MACOS.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/builds/12_WINDOWS_TO_LINUX.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/builds/20_APPIMAGE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/builds/23_DEB.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/builds/24_RPM.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/builds/27_BREW.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/builds/30_DOCKER.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/builds/40_GITHUB_ACTIONS.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/builds/COMPLETE_BUILD_ENVIRONMENT_SETUP.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/cloud-container/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/cloud-container/docker/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/cloud-container/docker/SPECIFICATION.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/connectivity/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/connectivity/odbc/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/connectivity/odbc/SPECIFICATION.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/DRIVER_BASELINE_SPEC.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/cpp/API_REFERENCE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/cpp/COMPATIBILITY_MATRIX.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/cpp/IMPLEMENTATION_PLAN.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/cpp/MIGRATION_GUIDE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/cpp/SPECIFICATION.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/cpp/TESTING_CRITERIA.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/dotnet-csharp/API_REFERENCE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/dotnet-csharp/COMPATIBILITY_MATRIX.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/dotnet-csharp/IMPLEMENTATION_PLAN.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/dotnet-csharp/MIGRATION_GUIDE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/dotnet-csharp/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/dotnet-csharp/SPECIFICATION.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/dotnet-csharp/TESTING_CRITERIA.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/golang/API_REFERENCE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/golang/COMPATIBILITY_MATRIX.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/golang/IMPLEMENTATION_PLAN.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/golang/MIGRATION_GUIDE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/golang/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/golang/SPECIFICATION.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/golang/TESTING_CRITERIA.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/java-jdbc/API_REFERENCE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/java-jdbc/COMPATIBILITY_MATRIX.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/java-jdbc/IMPLEMENTATION_PLAN.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/java-jdbc/MIGRATION_GUIDE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/java-jdbc/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/java-jdbc/SPECIFICATION.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/java-jdbc/TESTING_CRITERIA.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/nodejs-typescript/API_REFERENCE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/nodejs-typescript/COMPATIBILITY_MATRIX.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/nodejs-typescript/IMPLEMENTATION_PLAN.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/nodejs-typescript/MIGRATION_GUIDE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/nodejs-typescript/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/nodejs-typescript/SPECIFICATION.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/nodejs-typescript/TESTING_CRITERIA.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/pascal-delphi/API_REFERENCE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/pascal-delphi/COMPATIBILITY_MATRIX.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/pascal-delphi/IMPLEMENTATION_PLAN.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/pascal-delphi/MIGRATION_GUIDE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/pascal-delphi/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/pascal-delphi/SPECIFICATION.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/pascal-delphi/TESTING_CRITERIA.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/php/API_REFERENCE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/php/COMPATIBILITY_MATRIX.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/php/IMPLEMENTATION_PLAN.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/php/MIGRATION_GUIDE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/php/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/php/SPECIFICATION.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/php/TESTING_CRITERIA.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/python/API_REFERENCE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/python/COMPATIBILITY_MATRIX.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/python/IMPLEMENTATION_PLAN.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/python/MIGRATION_GUIDE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/python/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/python/SPECIFICATION.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/python/TESTING_CRITERIA.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/r/API_REFERENCE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/r/COMPATIBILITY_MATRIX.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/r/IMPLEMENTATION_PLAN.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/r/MIGRATION_GUIDE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/r/SPECIFICATION.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/r/TESTING_CRITERIA.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/ruby/API_REFERENCE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/ruby/COMPATIBILITY_MATRIX.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/ruby/IMPLEMENTATION_PLAN.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/ruby/MIGRATION_GUIDE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/ruby/SPECIFICATION.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/ruby/TESTING_CRITERIA.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/rust/API_REFERENCE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/rust/COMPATIBILITY_MATRIX.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/rust/IMPLEMENTATION_PLAN.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/rust/MIGRATION_GUIDE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/rust/SPECIFICATION.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/drivers/rust/TESTING_CRITERIA.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/optional/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/optional/STORAGE_ENCODING_OPTIMIZATIONS.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/orms-frameworks/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/replication/BETA_REPLICATION_ARCHITECTURE_FINDINGS.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/replication/README.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/replication/REPLICATION_AND_SHADOW_PROTOCOLS.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/replication/WAL_IMPLEMENTATION.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/replication/uuidv7-optimized/00_BETA_REPLICATION_INDEX.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/replication/uuidv7-optimized/00_REPLICATION_INDEX.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/replication/uuidv7-optimized/01_CORE_ARCHITECTURE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/replication/uuidv7-optimized/01_UUIDV8_HLC_ARCHITECTURE.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/replication/uuidv7-optimized/02_LEADERLESS_QUORUM_REPLICATION.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/replication/uuidv7-optimized/03_SCHEMA_DRIVEN_COLOCATION.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/replication/uuidv7-optimized/04_TIME_PARTITIONED_MERKLE_FOREST.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/replication/uuidv7-optimized/05_MGA_INTEGRATION.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/replication/uuidv7-optimized/06_IMPLEMENTATION_PHASES.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/replication/uuidv7-optimized/07_TESTING_STRATEGY.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/replication/uuidv7-optimized/08_MIGRATION_OPERATIONS.md` | Non-Core (Beta/Cluster) |  |
| `beta_requirements/tools/README.md` | Non-Core (Beta/Cluster) |  |
| `catalog/CATALOG_CORRECTION_PLAN.md` | Core Engine |  |
| `catalog/COMPONENT_MODEL_AND_RESPONSIBILITIES.md` | Core Engine |  |
| `catalog/README.md` | Core Engine |  |
| `catalog/SCHEMA_PATH_RESOLUTION.md` | Core Engine |  |
| `catalog/SCHEMA_PATH_SECURITY_DEFAULTS.md` | Core Engine |  |
| `catalog/SYSTEM_CATALOG_STRUCTURE.md` | Core Engine |  |
| `compression/COMPRESSION_FRAMEWORK.md` | Core Engine |  |
| `compression/README.md` | Core Engine |  |
| `core/CACHE_AND_BUFFER_ARCHITECTURE.md` | Core Engine |  |
| `core/CORE_IMPLEMENTATION_SPECS_SUMMARY.md` | Core Engine |  |
| `core/ENGINE_CORE_UNIFIED_SPEC.md` | Core Engine |  |
| `core/GIT_METADATA_INTEGRATION_SPECIFICATION.md` | Core Engine |  |
| `core/IMPLEMENTATION_RECOMMENDATIONS.md` | Core Engine |  |
| `core/INTERNAL_FUNCTIONS.md` | Core Engine |  |
| `core/LIVE_MIGRATION_PASSTHROUGH_SPECIFICATION.md` | Non-Core (Migration/Remote) | Live migration passthrough and CDC routing. |
| `core/README.md` | Core Engine |  |
| `core/THREAD_SAFETY.md` | Core Engine |  |
| `core/Y_VALVE_ARCHITECTURE.md` | Non-Core (Network) | Deprecated Y-Valve design; superseded by listener/pool. |
| `core/design_limits.md` | Core Engine |  |
| `ddl/02_DDL_STATEMENTS_OVERVIEW.md` | Core Engine |  |
| `ddl/09_DDL_FOREIGN_DATA.md` | Core Engine |  |
| `ddl/CASCADE_DROP_SPECIFICATION.md` | Core Engine |  |
| `ddl/DDL_DATABASES.md` | Core Engine |  |
| `ddl/DDL_EVENTS.md` | Core Engine |  |
| `ddl/DDL_EXCEPTIONS.md` | Core Engine |  |
| `ddl/DDL_FUNCTIONS.md` | Core Engine |  |
| `ddl/DDL_INDEXES.md` | Core Engine |  |
| `ddl/DDL_PACKAGES.md` | Core Engine |  |
| `ddl/DDL_PROCEDURES.md` | Core Engine |  |
| `ddl/DDL_ROLES_AND_GROUPS.md` | Core Engine |  |
| `ddl/DDL_ROW_LEVEL_SECURITY.md` | Core Engine |  |
| `ddl/DDL_SCHEMAS.md` | Core Engine |  |
| `ddl/DDL_SEQUENCES.md` | Core Engine |  |
| `ddl/DDL_TABLES.md` | Core Engine |  |
| `ddl/DDL_TABLE_PARTITIONING.md` | Core Engine |  |
| `ddl/DDL_TEMPORAL_TABLES.md` | Core Engine |  |
| `ddl/DDL_TRIGGERS.md` | Core Engine |  |
| `ddl/DDL_USER_DEFINED_RESOURCES.md` | Core Engine |  |
| `ddl/DDL_VIEWS.md` | Core Engine |  |
| `ddl/EXTRACT_AND_ALTER_ELEMENT.md` | Core Engine |  |
| `ddl/README.md` | Core Engine |  |
| `deployment/README.md` | Non-Core |  |
| `deployment/SYSTEMD_SERVICE_SPECIFICATION.md` | Non-Core |  |
| `dml/04_DML_STATEMENTS_OVERVIEW.md` | Core Engine |  |
| `dml/DML_COPY.md` | Core Engine |  |
| `dml/DML_DELETE.md` | Core Engine |  |
| `dml/DML_INSERT.md` | Core Engine |  |
| `dml/DML_MERGE.md` | Core Engine |  |
| `dml/DML_SELECT.md` | Core Engine |  |
| `dml/DML_UPDATE.md` | Core Engine |  |
| `dml/DML_XML_JSON_TABLES.md` | Core Engine |  |
| `dml/README.md` | Core Engine |  |
| `drivers/ALPHA_DRIVER_BOOTSTRAP.md` | Non-Core |  |
| `drivers/FlameRobin_Specification_for_AI.md` | Non-Core |  |
| `drivers/JDBC_DRIVER_SPECIFICATION.md` | Non-Core |  |
| `drivers/ODBC_DRIVER_SPECIFICATION.md` | Non-Core |  |
| `drivers/README.md` | Non-Core |  |
| `drivers/firebird_spec.md` | Non-Core |  |
| `drivers/jdbc_jni_spec.md` | Non-Core |  |
| `drivers/mssql_spec.md` | Non-Core |  |
| `drivers/mysql_mariadb_spec.md` | Non-Core |  |
| `drivers/odbc_generic_spec.md` | Non-Core |  |
| `drivers/postgresql_spec.md` | Non-Core |  |
| `drivers/postgresql_technical.md` | Non-Core |  |
| `drivers/unified_interface_spec.md` | Non-Core |  |
| `future/C_API_IMPLEMENTATION_GUIDE.md` | Non-Core (Beta/Cluster) |  |
| `future/C_API_SPECIFICATION.md` | Non-Core (Beta/Cluster) |  |
| `future/ERROR_HANDLING.md` | Non-Core (Beta/Cluster) |  |
| `future/README.md` | Non-Core (Beta/Cluster) |  |
| `indexes/AdvancedIndexes.md` | Core Engine |  |
| `indexes/BloomFilterIndex.md` | Core Engine |  |
| `indexes/COLUMNSTORE_SPEC.md` | Core Engine |  |
| `indexes/INDEX_ARCHITECTURE.md` | Core Engine |  |
| `indexes/INDEX_GC_PROTOCOL.md` | Core Engine |  |
| `indexes/INDEX_IMPLEMENTATION_GUIDE.md` | Core Engine |  |
| `indexes/INDEX_IMPLEMENTATION_SPEC.md` | Core Engine |  |
| `indexes/IVFIndex.md` | Core Engine |  |
| `indexes/InvertedIndex.md` | Core Engine |  |
| `indexes/LSM_TREE_ARCHITECTURE.md` | Core Engine |  |
| `indexes/LSM_TREE_SPEC.md` | Core Engine |  |
| `indexes/README.md` | Core Engine |  |
| `indexes/ZoneMapsIndex.md` | Core Engine |  |
| `network/CONTROL_PLANE_PROTOCOL_SPEC.md` | Non-Core |  |
| `network/DIALECT_AUTH_MAPPING_SPEC.md` | Non-Core |  |
| `network/ENGINE_PARSER_IPC_CONTRACT.md` | Non-Core |  |
| `network/NETWORK_LAYER_SPEC.md` | Non-Core |  |
| `network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md` | Non-Core |  |
| `network/PARSER_AGENT_SPEC.md` | Non-Core |  |
| `network/README.md` | Non-Core |  |
| `network/WIRE_PROTOCOL_SPECIFICATIONS.md` | Non-Core |  |
| `network/Y_VALVE_DESIGN_PRINCIPLES.md` | Non-Core |  |
| `operations/LISTENER_POOL_METRICS.md` | Non-Core (Network) | Listener/pool metrics; not core engine. |
| `operations/MONITORING_DIALECT_MAPPINGS.md` | Core Engine |  |
| `operations/MONITORING_SQL_VIEWS.md` | Core Engine |  |
| `operations/OID_MAPPING_STRATEGY.md` | Core Engine |  |
| `operations/PROMETHEUS_METRICS_REFERENCE.md` | Core Engine |  |
| `operations/README.md` | Core Engine |  |
| `parser/01_SQL_DIALECT_OVERVIEW.md` | Non-Core |  |
| `parser/05_PSQL_PROCEDURAL_LANGUAGE.md` | Non-Core |  |
| `parser/08_PARSER_AND_DEVELOPER_EXPERIENCE.md` | Non-Core |  |
| `parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md` | Non-Core |  |
| `parser/MYSQL_PARSER_SPECIFICATION.md` | Non-Core |  |
| `parser/POSTGRESQL_PARSER_IMPLEMENTATION.md` | Non-Core |  |
| `parser/POSTGRESQL_PARSER_SPECIFICATION.md` | Non-Core |  |
| `parser/README.md` | Non-Core |  |
| `parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md` | Non-Core |  |
| `parser/ScratchBird Master Grammar Specification v2.0.md` | Non-Core |  |
| `parser/ScratchBird SQL Language Specification - Master Document.md` | Non-Core |  |
| `query/PARALLEL_EXECUTION_ARCHITECTURE.md` | Core Engine |  |
| `query/QUERY_OPTIMIZER_SPEC.md` | Core Engine |  |
| `query/README.md` | Core Engine |  |
| `reference/README.md` | Reference |  |
| `reference/UUIDv7 Replication System Design.md` | Reference |  |
| `reference/firebird/FirebirdReferenceDocument.md` | Reference |  |
| `reference/firebird/README.md` | Reference |  |
| `reference/firebird/firebird_docs_split/00_Preface_and_ToC.md` | Reference |  |
| `reference/firebird/firebird_docs_split/01_About_Firebird_5.0.md` | Reference |  |
| `reference/firebird/firebird_docs_split/02_SQL_Language_Structure.md` | Reference |  |
| `reference/firebird/firebird_docs_split/03_Data_Types_and_Subtypes.md` | Reference |  |
| `reference/firebird/firebird_docs_split/04_Common_Language_Elements.md` | Reference |  |
| `reference/firebird/firebird_docs_split/05_DDL_Statements.md` | Reference |  |
| `reference/firebird/firebird_docs_split/06_DML_Statements.md` | Reference |  |
| `reference/firebird/firebird_docs_split/07_PSQL_Statements.md` | Reference |  |
| `reference/firebird/firebird_docs_split/08_Built_in_Scalar_Functions.md` | Reference |  |
| `reference/firebird/firebird_docs_split/09_Aggregate_Functions.md` | Reference |  |
| `reference/firebird/firebird_docs_split/10_Window_Functions.md` | Reference |  |
| `reference/firebird/firebird_docs_split/11_System_Packages.md` | Reference |  |
| `reference/firebird/firebird_docs_split/12_Context_Variables.md` | Reference |  |
| `reference/firebird/firebird_docs_split/13_Transaction_Control.md` | Reference |  |
| `reference/firebird/firebird_docs_split/14_Security.md` | Reference |  |
| `reference/firebird/firebird_docs_split/15_Management_Statements.md` | Reference |  |
| `reference/firebird/firebird_docs_split/App_A_Supplementary_Info.md` | Reference |  |
| `reference/firebird/firebird_docs_split/App_B_Exception_Codes.md` | Reference |  |
| `reference/firebird/firebird_docs_split/App_C_Reserved_Words.md` | Reference |  |
| `reference/firebird/firebird_docs_split/App_D_System_Tables.md` | Reference |  |
| `reference/firebird/firebird_docs_split/App_E_Monitoring_Tables.md` | Reference |  |
| `reference/firebird/firebird_docs_split/App_F_Security_Tables.md` | Reference |  |
| `reference/firebird/firebird_docs_split/App_G_Plugin_Tables.md` | Reference |  |
| `reference/firebird/firebird_docs_split/App_H_Charsets_and_Collations.md` | Reference |  |
| `reference/firebird/firebird_docs_split/App_I_License.md` | Reference |  |
| `reference/firebird/firebird_docs_split/App_J_Document_History.md` | Reference |  |
| `remote_database_udr/01-CORE_TYPES.md` | Non-Core |  |
| `remote_database_udr/02-CONNECTION_POOL.md` | Non-Core |  |
| `remote_database_udr/03-POSTGRESQL_ADAPTER.md` | Non-Core |  |
| `remote_database_udr/04-MYSQL_ADAPTER.md` | Non-Core |  |
| `remote_database_udr/05-MSSQL_FIREBIRD_ADAPTERS.md` | Non-Core |  |
| `remote_database_udr/06-QUERY_EXECUTION.md` | Non-Core |  |
| `remote_database_udr/07-SCHEMA_INTROSPECTION.md` | Non-Core |  |
| `remote_database_udr/08-SQL_SYNTAX.md` | Non-Core |  |
| `remote_database_udr/09-MIGRATION_WORKFLOWS.md` | Non-Core |  |
| `remote_database_udr/README.md` | Non-Core |  |
| `sblr/Appendix_A_SBLR_BYTECODE.md` | Core Engine |  |
| `sblr/FIREBIRD_BLR_FIXTURES.md` | Core Engine |  |
| `sblr/FIREBIRD_BLR_TO_SBLR_MAPPING.md` | Core Engine |  |
| `sblr/FIREBIRD_TRANSACTION_MODEL_SPEC.md` | Core Engine |  |
| `sblr/README.md` | Core Engine |  |
| `sblr/SBLR_DOMAIN_PAYLOADS.md` | Core Engine |  |
| `sblr/SBLR_EXECUTION_PERFORMANCE_ALPHA.md` | Core Engine |  |
| `sblr/SBLR_EXECUTION_PERFORMANCE_BETA.md` | Core Engine |  |
| `sblr/SBLR_EXECUTION_PERFORMANCE_RESEARCH.md` | Core Engine |  |
| `sblr/SBLR_OPCODE_REGISTRY.md` | Core Engine |  |
| `scheduler/ALPHA_SCHEDULER_SPECIFICATION.md` | Core Engine |  |
| `scheduler/README.md` | Core Engine |  |
| `scheduler/SCHEDULER_JOB_RUNNER_CANONICAL_SPEC.md` | Core Engine |  |
| `storage/EXTENDED_PAGE_SIZES.md` | Core Engine |  |
| `storage/HEAP_TOAST_INTEGRATION.md` | Core Engine |  |
| `storage/MGA_IMPLEMENTATION.md` | Core Engine |  |
| `storage/ON_DISK_FORMAT.md` | Core Engine |  |
| `storage/README.md` | Core Engine |  |
| `storage/STORAGE_ENGINE_BUFFER_POOL.md` | Core Engine |  |
| `storage/STORAGE_ENGINE_MAIN.md` | Core Engine |  |
| `storage/STORAGE_ENGINE_PAGE_MANAGEMENT.md` | Core Engine |  |
| `storage/TABLESPACE_ONLINE_MIGRATION.md` | Core Engine |  |
| `storage/TABLESPACE_SPECIFICATION.md` | Core Engine |  |
| `storage/TOAST_LOB_STORAGE.md` | Core Engine |  |
| `testing/ALPHA3_TEST_PLAN.md` | Unclassified | Manual review needed. |
| `testing/README.md` | Unclassified | Manual review needed. |
| `tools/README.md` | Non-Core |  |
| `tools/SB_ISQL_CLI_SPECIFICATION.md` | Non-Core |  |
| `tools/SB_TOOLING_NETWORK_SPEC.md` | Non-Core |  |
| `transaction/07_TRANSACTION_AND_SESSION_CONTROL.md` | Core Engine |  |
| `transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md` | Core Engine |  |
| `transaction/README.md` | Core Engine |  |
| `transaction/TRANSACTION_DISTRIBUTED.md` | Core Engine |  |
| `transaction/TRANSACTION_LOCK_MANAGER.md` | Core Engine |  |
| `transaction/TRANSACTION_MAIN.md` | Core Engine |  |
| `transaction/TRANSACTION_MGA_CORE.md` | Core Engine |  |
| `triggers/README.md` | Core Engine |  |
| `triggers/TRIGGER_CONTEXT_VARIABLES.md` | Core Engine |  |
| `types/03_TYPES_AND_DOMAINS.md` | Core Engine |  |
| `types/DATA_TYPE_PERSISTENCE_AND_CASTS.md` | Core Engine |  |
| `types/DDL_DOMAINS_COMPREHENSIVE.md` | Core Engine |  |
| `types/MULTI_GEOMETRY_TYPES_SPEC.md` | Core Engine |  |
| `types/POSTGRESQL_ARRAY_TYPE_SPEC.md` | Core Engine |  |
| `types/README.md` | Core Engine |  |
| `types/TIMEZONE_SYSTEM_CATALOG.md` | Core Engine |  |
| `types/UUID_IDENTITY_COLUMNS.md` | Core Engine |  |
| `types/character_sets_and_collations.md` | Core Engine |  |
| `udr/10-UDR-System-Specification.md` | Core Engine |  |
| `udr/README.md` | Core Engine |  |
| `udr_connectors/README.md` | Non-Core |  |
| `udr_connectors/UDR_CONNECTOR_BASELINE.md` | Non-Core |  |
| `udr_connectors/firebird_udr/SPECIFICATION.md` | Non-Core |  |
| `udr_connectors/local_files_udr/SPECIFICATION.md` | Non-Core |  |
| `udr_connectors/local_scripts_udr/SPECIFICATION.md` | Non-Core |  |
| `udr_connectors/mysql_udr/SPECIFICATION.md` | Non-Core |  |
| `udr_connectors/postgresql_udr/SPECIFICATION.md` | Non-Core |  |
| `wire_protocols/README.md` | Non-Core |  |
| `wire_protocols/firebird_wire_protocol.md` | Non-Core |  |
| `wire_protocols/mysql_wire_protocol.md` | Non-Core |  |
| `wire_protocols/postgresql_wire_protocol.md` | Non-Core |  |
| `wire_protocols/scratchbird_native_wire_protocol.md` | Non-Core |  |
| `wire_protocols/tds_wire_protocol.md` | Non-Core |  |
