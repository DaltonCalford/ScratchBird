# Specification Master Index

Complete index of all reverse-engineered specifications with verified source anchors.

## Statistics

| Metric | Count |
|--------|-------|
| Total Specifications | 49 |
| Subsystems | 8 |
| Status | 🔴 Draft (all) |
| Last Verified | 2026-03-08 |

---

## By Subsystem

### Parser (`parser/`)

| Spec | Description | Lines | Key Topics |
|------|-------------|-------|------------|
| [v3_canonical_grammar.md](parser/v3_canonical_grammar.md) | V3 SQL grammar and parsing rules | ~350 | EBNF grammar, statement types, AST nodes |
| [path_resolution_logic.md](parser/path_resolution_logic.md) | Path resolution (!:, .:, ..:) | ~400 | SchemaPath, path types, resolution algorithm |
| [semantic_binding_flow.md](parser/semantic_binding_flow.md) | Semantic analysis and catalog binding | ~420 | Binding phases, type resolution, validation |

### SBLR - ScratchBird Language Runtime (`sblr/`)

| Spec | Description | Lines | Key Topics |
|------|-------------|-------|------------|
| [v3_opcode_reference.md](sblr/v3_opcode_reference.md) | Complete opcode catalog (877 opcodes) | ~587 | Hex codes, families, operations |
| [v3_container_format.md](sblr/v3_container_format.md) | Binary container serialization | ~402 | Header, sections, alignment, encoding |
| [v3_execution_model.md](sblr/v3_execution_model.md) | Execution engine and dispatch | ~406 | State machine, stack, context |
| [v3_payload_schemas.md](sblr/v3_payload_schemas.md) | Payload structure definitions | ~473 | Field types, schemas, nesting |

### Storage & MGA (`storage/`)

| Spec | Description | Lines | Key Topics |
|------|-------------|-------|------------|
| [version_chain_format.md](storage/version_chain_format.md) | Record version chain structure | ~300 | TupleHeader, back versioning, hints |
| [transaction_lifecycle.md](storage/transaction_lifecycle.md) | Transaction states and TIP | ~310 | State machine, TIP format, commit |
| [mga_visibility_rules.md](storage/mga_visibility_rules.md) | Visibility computation | ~315 | Firebird MGA, isolation levels, hints |
| [gc_sweep_algorithm.md](storage/gc_sweep_algorithm.md) | Garbage collection | ~350 | Sweep phases, safe horizon, compaction |
| [page_layout.md](storage/page_layout.md) | Page structure and layout | ~450 | Header, slot array, alignment |

### Catalog (`catalog/`)

| Spec | Description | Lines | Key Topics |
|------|-------------|-------|------------|
| [bootstrap_sequence.md](catalog/bootstrap_sequence.md) | Catalog initialization | ~280 | 7-phase bootstrap, invariants |
| [uuid_mapping.md](catalog/uuid_mapping.md) | UUID generation and identity | ~240 | UUIDv7, zero UUID, immutability |
| [object_identity_rules.md](catalog/object_identity_rules.md) | Object identification | ~275 | Name resolution, conflicts, canonicalization |
| [catalog_table_layouts.md](catalog/catalog_table_layouts.md) | Catalog table schemas | ~450 | sb_*, sys.* tables, virtual views |

### Security (`security/`)

| Spec | Description | Lines | Key Topics |
|------|-------------|-------|------------|
| **Authentication** ||||
| [authentication_flow.md](security/authentication_flow.md) | Authentication mechanisms | ~411 | SCRAM, MFA, plugins, state machines |
| [auth_plugins.md](security/auth_plugins.md) | All 17+ auth plugins | ~500 | Trust, SCRAM, LDAP, Kerberos, etc. |
| [hba_rules.md](security/hba_rules.md) | HBA rule parsing/matching | ~450 | pg_hba.conf, connection types |
| [password_management.md](security/password_management.md) | Password policies | ~550 | Complexity, expiration, history |
| **Authorization** ||||
| [authorization_model.md](security/authorization_model.md) | Permission system | ~466 | GRANT/REVOKE, cache, view security |
| [privilege_types.md](security/privilege_types.md) | ALL privilege types | ~350 | SELECT, INSERT, REFERENCES, etc. |
| [acl_format.md](security/acl_format.md) | ACL storage format | ~400 | Permission records, encoding |
| [default_privileges.md](security/default_privileges.md) | Default privileges | ~400 | ALTER DEFAULT PRIVILEGES |
| **Row-Level Security** ||||
| [rls_policy_enforcement.md](security/rls_policy_enforcement.md) | RLS policy evaluation | ~483 | Policy evaluation, combining rules |
| [rls_policy_syntax.md](security/rls_policy_syntax.md) | RLS SQL syntax | ~350 | CREATE/ALTER/DROP POLICY |
| [rls_performance.md](security/rls_performance.md) | RLS optimization | ~450 | Predicate pushdown, caching |
| **Column-Level Security** ||||
| [cls_column_masking.md](security/cls_column_masking.md) | Column-Level Security | ~567 | Masking patterns, UTF-8, partial masks |
| [masking_functions.md](security/masking_functions.md) | Masking functions | ~450 | Full, partial, email, credit card |
| **Other Security** ||||
| [ssl_tls.md](security/ssl_tls.md) | SSL/TLS configuration | ~500 | Certificates, cipher suites, mTLS |
| [encryption.md](security/encryption.md) | Data at rest encryption | ~550 | TDE, column encryption, key management |
| [audit_logging.md](security/audit_logging.md) | Security audit logging | ~450 | Authentication, DDL, compliance |

### IPC & Protocol (`ipc/`)

| Spec | Description | Lines | Key Topics |
|------|-------------|-------|------------|
| [wire_protocol.md](ipc/wire_protocol.md) | SBWP frame format | ~497 | Binary layout, message types |
| [ipc_session_lifecycle.md](ipc/ipc_session_lifecycle.md) | Session management | ~516 | Handshake, state transitions |
| [parser_agent_contract.md](ipc/parser_agent_contract.md) | Parser agent protocol | ~559 | Message flow, capabilities |
| [protocol_adapters.md](ipc/protocol_adapters.md) | Wire protocol emulation | ~683 | PostgreSQL, MySQL, Firebird adapters |

### Indexes (`indexes/`)

| Spec | Description | Lines | Key Topics |
|------|-------------|-------|------------|
| [btree_index_format.md](indexes/btree_index_format.md) | B-Tree structure | ~416 | Page types, key format, splits |
| [gin_index_format.md](indexes/gin_index_format.md) | GIN inverted index | ~432 | Entry tree, posting lists, pending |
| [index_dml_integration.md](indexes/index_dml_integration.md) | Index maintenance | ~480 | INSERT/UPDATE/DELETE, GC |

### Type System (`types/`)

| Spec | Description | Lines | Key Topics |
|------|-------------|-------|------------|
| [scalar_types.md](types/scalar_types.md) | Scalar type storage | ~380 | INTEGER, VARCHAR, TIMESTAMP, etc. |
| [complex_types.md](types/complex_types.md) | Complex type storage | ~390 | ARRAY, JSON, VECTOR, spatial |
| [type_coercion_rules.md](types/type_coercion_rules.md) | Type conversion rules | ~410 | Implicit/explicit casts, matrices |

---

## By Cross-Cutting Concern

### Query Processing Pipeline

| Stage | Specification(s) |
|-------|------------------|
| Parse | [parser/v3_canonical_grammar.md](parser/v3_canonical_grammar.md) |
| Bind | [parser/semantic_binding_flow.md](parser/semantic_binding_flow.md) |
| Compile | [sblr/v3_opcode_reference.md](sblr/v3_opcode_reference.md), [sblr/v3_payload_schemas.md](sblr/v3_payload_schemas.md) |
| Execute | [sblr/v3_execution_model.md](sblr/v3_execution_model.md) |

### Transaction Management

| Concern | Specification(s) |
|---------|------------------|
| Lifecycle | [storage/transaction_lifecycle.md](storage/transaction_lifecycle.md) |
| Visibility | [storage/mga_visibility_rules.md](storage/mga_visibility_rules.md) |
| Versioning | [storage/version_chain_format.md](storage/version_chain_format.md) |
| Cleanup | [storage/gc_sweep_algorithm.md](storage/gc_sweep_algorithm.md) |

### Security Enforcement

| Layer | Specification(s) |
|-------|------------------|
| Authentication | [security/authentication_flow.md](security/authentication_flow.md), [security/auth_plugins.md](security/auth_plugins.md), [security/hba_rules.md](security/hba_rules.md), [security/password_management.md](security/password_management.md) |
| Authorization | [security/authorization_model.md](security/authorization_model.md), [security/privilege_types.md](security/privilege_types.md), [security/acl_format.md](security/acl_format.md), [security/default_privileges.md](security/default_privileges.md) |
| Row Security | [security/rls_policy_enforcement.md](security/rls_policy_enforcement.md), [security/rls_policy_syntax.md](security/rls_policy_syntax.md), [security/rls_performance.md](security/rls_performance.md) |
| Column Security | [security/cls_column_masking.md](security/cls_column_masking.md), [security/masking_functions.md](security/masking_functions.md) |
| Transport | [security/ssl_tls.md](security/ssl_tls.md) |
| Encryption | [security/encryption.md](security/encryption.md) |
| Auditing | [security/audit_logging.md](security/audit_logging.md) |

### Storage Layer

| Component | Specification(s) |
|-----------|------------------|
| Page Format | [storage/page_layout.md](storage/page_layout.md) |
| Indexes | [indexes/btree_index_format.md](indexes/btree_index_format.md), [indexes/gin_index_format.md](indexes/gin_index_format.md) |
| Catalog | [catalog/bootstrap_sequence.md](catalog/bootstrap_sequence.md), [catalog/catalog_table_layouts.md](catalog/catalog_table_layouts.md) |

---

## Recently Added

| Date | Specs Added |
|------|-------------|
| 2026-03-08 | 16 comprehensive security specifications |
| 2026-03-08 | All 49 initial specifications |

---

## Legend

| Status | Meaning |
|--------|---------|
| 🔴 Draft | Initial reverse-engineering, not yet fully validated |
| 🟡 Review | Pending review against implementation |
| 🟢 Approved | Validated against current implementation |
| ⚪ Stable | Interface frozen, backward compatibility guaranteed |
| 🚫 Deprecated | Spec obsolete, implementation changing |

---

**Note:** All specifications contain verified source anchors pointing to actual implementation code in `src/` and test code in `tests/`.
