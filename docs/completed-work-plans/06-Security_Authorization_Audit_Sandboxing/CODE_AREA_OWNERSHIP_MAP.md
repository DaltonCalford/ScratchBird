# Code Area Ownership Map

## Primary Write Scopes

| Ticket | Primary write scope | Conflict surfaces | Parallelization rule |
| --- | --- | --- | --- |
| B1-06-001 | assigned section specs plus this package | all package control files | serial only |
| B1-06-002 | docs/specifications/19_Security_Model/README.md, docs/specifications/20_Diagnostics_Audit_and_Observability/README.md, SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv | package control files and section readmes | serial only |
| B1-06-003 | src/security/auth_manager.cpp, src/security/auth_plugin_manager.cpp, src/core/auth_provider.cpp, src/server/server_session.cpp, src/core/catalog_manager.cpp, src/core/permission_cache.cpp, src/core/data_masking.cpp, src/core/domain_manager.cpp, src/security/view_security.cpp, src/security/tls_context.cpp, src/core/encryption_key_manager.cpp | shared security seams | after ownership freeze |
| B1-06-004 | src/core/audit_logger.cpp, src/core/secure_diagnostics.cpp, src/core/structured_logger.cpp, src/core/support_bundle_builder.cpp, src/core/observability_contract.cpp | audit and operator status surfaces | after lane A foundation |
| B1-06-005 | security and observability gates | shared gate runners and support-bundle surfaces | after implementation tickets |

## Unsafe Parallel Boundaries

- any ticket that updates SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv
- any ticket that changes the same canonical spec file as another ticket
- any ticket that changes the same gate or benchmark artifact family
