# Specification Master Index

Complete index of reverse-engineered specifications with verified source anchors.

## Statistics

| Metric | Count |
|--------|-------|
| Total Specifications | 82 |
| Subsystems | 7 |
| Status | 🔴 Draft (all) |
| Last Verified | 2026-03-08 |

---

## By Subsystem

### Catalog (`catalog/` - 18 specs)

| Spec | Description |
|------|-------------|
| [bootstrap_sequence.md](catalog/bootstrap_sequence.md) | Catalog initialization phases |
| [catalog_overview.md](catalog/catalog_overview.md) | Catalog architecture |
| [catalog_pages.md](catalog/catalog_pages.md) | Catalog page layouts |
| [catalog_table_layouts.md](catalog/catalog_table_layouts.md) | System table schemas |
| [character_sets.md](catalog/character_sets.md) | Character set metadata |
| [collations.md](catalog/collations.md) | Collation definitions |
| [columns.md](catalog/columns.md) | Column metadata |
| [constraints.md](catalog/constraints.md) | Constraint definitions |
| [dependency_tracking.md](catalog/dependency_tracking.md) | Object dependencies |
| [domains.md](catalog/domains.md) | Domain definitions |
| [functions.md](catalog/functions.md) | Function/procedure metadata |
| [indexes.md](catalog/indexes.md) | Index metadata (all types) |
| [object_identity_rules.md](catalog/object_identity_rules.md) | Object naming |
| [sequences.md](catalog/sequences.md) | Sequence metadata |
| [tables.md](catalog/tables.md) | Table metadata |
| [triggers.md](catalog/triggers.md) | Trigger metadata |
| [uuid_mapping.md](catalog/uuid_mapping.md) | UUID generation |
| [views.md](catalog/views.md) | View metadata |

### Indexes (`indexes/` - 13 specs)

| Spec | Description |
|------|-------------|
| [btree_index_format.md](indexes/btree_index_format.md) | B-Tree page layout |
| [gin_index_format.md](indexes/gin_index_format.md) | GIN structure |
| [index_bitmap.md](indexes/index_bitmap.md) | Bitmap index |
| [index_brin.md](indexes/index_brin.md) | Block Range Index |
| [index_btree.md](indexes/index_btree.md) | B-Tree index |
| [index_columnstore.md](indexes/index_columnstore.md) | Columnstore |
| [index_fulltext.md](indexes/index_fulltext.md) | Full-text search |
| [index_gin.md](indexes/index_gin.md) | GIN inverted index |
| [index_gist.md](indexes/index_gist.md) | GiST index |
| [index_hash.md](indexes/index_hash.md) | Hash index |
| [index_hnsw.md](indexes/index_hnsw.md) | HNSW vector |
| [index_rtree.md](indexes/index_rtree.md) | R-tree spatial |
| [index_spgist.md](indexes/index_spgist.md) | SP-GiST |

### IPC & Protocol (`ipc/` - 3 specs)

| Spec | Description |
|------|-------------|
| [ipc_session_lifecycle.md](ipc/ipc_session_lifecycle.md) | Session management |
| [parser_agent_contract.md](ipc/parser_agent_contract.md) | Parser agent protocol |
| [wire_protocol.md](ipc/wire_protocol.md) | SBWP wire protocol |

### Parser (`parser/` - 23 specs)

#### Grammar & Resolution
| Spec | Description |
|------|-------------|
| [v3_canonical_grammar.md](parser/v3_canonical_grammar.md) | V3 SQL grammar |
| [path_resolution_logic.md](parser/path_resolution_logic.md) | Path resolution |
| [semantic_binding_flow.md](parser/semantic_binding_flow.md) | Semantic binding |

#### CREATE Statements
| Spec | Description |
|------|-------------|
| [stmt_create_database.md](parser/stmt_create_database.md) | CREATE DATABASE |
| [stmt_create_schema.md](parser/stmt_create_schema.md) | CREATE SCHEMA |
| [stmt_create_table.md](parser/stmt_create_table.md) | CREATE TABLE |
| [stmt_create_index.md](parser/stmt_create_index.md) | CREATE INDEX |
| [stmt_create_view.md](parser/stmt_create_view.md) | CREATE VIEW |
| [stmt_create_sequence.md](parser/stmt_create_sequence.md) | CREATE SEQUENCE |
| [stmt_create_function.md](parser/stmt_create_function.md) | CREATE FUNCTION |
| [stmt_create_trigger.md](parser/stmt_create_trigger.md) | CREATE TRIGGER |

#### ALTER Statements
| Spec | Description |
|------|-------------|
| [stmt_alter_table.md](parser/stmt_alter_table.md) | ALTER TABLE |
| [stmt_alter_index.md](parser/stmt_alter_index.md) | ALTER INDEX |

#### DROP Statements
| Spec | Description |
|------|-------------|
| [stmt_drop_table.md](parser/stmt_drop_table.md) | DROP TABLE |

#### DML Statements
| Spec | Description |
|------|-------------|
| [stmt_select.md](parser/stmt_select.md) | SELECT |
| [stmt_insert.md](parser/stmt_insert.md) | INSERT |
| [stmt_update.md](parser/stmt_update.md) | UPDATE |
| [stmt_delete.md](parser/stmt_delete.md) | DELETE |
| [stmt_merge.md](parser/stmt_merge.md) | MERGE |

#### Utility
| Spec | Description |
|------|-------------|
| [stmt_copy.md](parser/stmt_copy.md) | COPY |
| [stmt_analyze.md](parser/stmt_analyze.md) | ANALYZE |

### SBLR - ScratchBird Language Runtime (`sblr/` - 11 specs)

| Spec | Description |
|------|-------------|
| [v3_opcode_reference.md](sblr/v3_opcode_reference.md) | Opcode catalog (877 opcodes) |
| [v3_container_format.md](sblr/v3_container_format.md) | Container serialization |
| [v3_execution_model.md](sblr/v3_execution_model.md) | Execution engine |
| [v3_payload_schemas.md](sblr/v3_payload_schemas.md) | Payload schemas |
| [opcodes_ddl.md](sblr/opcodes_ddl.md) | DDL opcodes |
| [opcodes_dml.md](sblr/opcodes_dml.md) | DML opcodes |
| [opcodes_expressions.md](sblr/opcodes_expressions.md) | Expression opcodes |
| [opcodes_index.md](sblr/opcodes_index.md) | Index opcodes |
| [opcodes_query.md](sblr/opcodes_query.md) | Query opcodes |
| [opcodes_utility.md](sblr/opcodes_utility.md) | Utility opcodes |

### Security (`security/` - 14 specs)

#### Authentication
| Spec | Description |
|------|-------------|
| [authentication_flow.md](security/authentication_flow.md) | Auth flow |
| [auth_plugins.md](security/auth_plugins.md) | Auth plugins |
| [hba_rules.md](security/hba_rules.md) | HBA rules |
| [password_management.md](security/password_management.md) | Password policies |

#### Authorization
| Spec | Description |
|------|-------------|
| [authorization_model.md](security/authorization_model.md) | Permission system |
| [privilege_types.md](security/privilege_types.md) | Privilege types |
| [acl_format.md](security/acl_format.md) | ACL format |

#### Row-Level Security
| Spec | Description |
|------|-------------|
| [rls_policy_enforcement.md](security/rls_policy_enforcement.md) | RLS enforcement |
| [rls_policy_syntax.md](security/rls_policy_syntax.md) | RLS syntax |
| [rls_performance.md](security/rls_performance.md) | RLS optimization |

#### Column-Level Security
| Spec | Description |
|------|-------------|
| [cls_column_masking.md](security/cls_column_masking.md) | Column masking |
| [masking_functions.md](security/masking_functions.md) | Masking functions |

#### Other Security
| Spec | Description |
|------|-------------|
| [ssl_tls.md](security/ssl_tls.md) | SSL/TLS |
| [audit_logging.md](security/audit_logging.md) | Audit logging |

### Type System (`types/` - 3 specs)

| Spec | Description |
|------|-------------|
| [scalar_types.md](types/scalar_types.md) | Scalar types |
| [complex_types.md](types/complex_types.md) | Complex types |
| [type_coercion_rules.md](types/type_coercion_rules.md) | Type coercion |

---

## Legend

| Status | Meaning |
|--------|---------|
| 🔴 Draft | Initial reverse-engineering |
| 🟡 Review | Pending validation |
| 🟢 Approved | Validated against implementation |
| ⚪ Stable | Interface frozen |
| 🚫 Deprecated | Spec obsolete |

---

**Note:** All 82 specifications contain verified source anchors pointing to actual implementation code in `src/` and test code in `tests/`.
