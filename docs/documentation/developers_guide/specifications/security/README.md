# ScratchBird Security Subsystem Specifications

This directory contains comprehensive specifications for the ScratchBird Security subsystem.

## Status

🟡 **In Progress** - Last Verified: 2026-03-08

## Specifications

### Authentication

| Document | Description | Key Components |
|----------|-------------|----------------|
| [authentication_flow.md](./authentication_flow.md) | Authentication flows and methods | HBA, SCRAM, MFA, Plugin Registry |
| [auth_plugins.md](./auth_plugins.md) | All 17+ auth plugins | Trust, Password, SCRAM, LDAP, Kerberos, etc. |
| [hba_rules.md](./hba_rules.md) | pg_hba.conf parsing and matching | Connection types, address matching |
| [password_management.md](./password_management.md) | Password policies and expiration | Complexity, history, lockout |

### Authorization

| Document | Description | Key Components |
|----------|-------------|----------------|
| [authorization_model.md](./authorization_model.md) | Permission system and caching | GRANT/REVOKE, Permission Cache, View Security |
| [privilege_types.md](./privilege_types.md) | ALL privilege types | SELECT, INSERT, UPDATE, REFERENCES, etc. |
| [acl_format.md](./acl_format.md) | Access Control List storage | Permission records, ACL encoding |
| [default_privileges.md](./default_privileges.md) | ALTER DEFAULT PRIVILEGES | Future object permissions |

### Row-Level Security (RLS)

| Document | Description | Key Components |
|----------|-------------|----------------|
| [rls_policy_enforcement.md](./rls_policy_enforcement.md) | RLS policy evaluation | Predicate generation, RESTRICTIVE/PERMISSIVE |
| [rls_policy_syntax.md](./rls_policy_syntax.md) | CREATE/ALTER/DROP POLICY | SQL syntax for policy management |
| [rls_performance.md](./rls_performance.md) | RLS optimization | Predicate pushdown, caching |

### Column-Level Security (CLS)

| Document | Description | Key Components |
|----------|-------------|----------------|
| [cls_column_masking.md](./cls_column_masking.md) | Column masking system | Masking rules, UNMASK privilege |
| [masking_functions.md](./masking_functions.md) | Built-in masking functions | Full, partial, email, credit card |

### Other Security Features

| Document | Description | Key Components |
|----------|-------------|----------------|
| [ssl_tls.md](./ssl_tls.md) | SSL/TLS configuration | Certificates, cipher suites, mTLS |
| [encryption.md](./encryption.md) | Data at rest encryption | TDE, column-level encryption |
| [audit_logging.md](./audit_logging.md) | Security audit logging | Authentication, DDL, compliance |

## Implementation Locations

### Authentication
- `/home/dcalford/CliWork/ScratchBird/src/security/auth_manager.cpp` - Main auth coordinator
- `/home/dcalford/CliWork/ScratchBird/src/security/scram_auth.cpp` - SCRAM-SHA-256/512
- `/home/dcalford/CliWork/ScratchBird/src/security/mfa_auth.cpp` - TOTP/HOTP MFA
- `/home/dcalford/CliWork/ScratchBird/src/security/auth_plugin_manager.cpp` - Plugin registry
- `/home/dcalford/CliWork/ScratchBird/src/security/password_policy.cpp` - Password policies

### Authorization
- `/home/dcalford/CliWork/ScratchBird/src/core/permission_cache.cpp` - Permission caching
- `/home/dcalford/CliWork/ScratchBird/src/security/view_security.cpp` - View security contexts
- `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp` - GRANT/REVOKE operations

### RLS/CLS
- `/home/dcalford/CliWork/ScratchBird/src/core/data_masking.cpp` - Column masking implementation
- `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp` - RLS predicate enforcement
- `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp` - Policy parsing

### SSL/TLS/Encryption
- `/home/dcalford/CliWork/ScratchBird/src/security/tls_context.cpp` - TLS implementation
- `/home/dcalford/CliWork/ScratchBird/src/security/cert_auth.cpp` - Certificate authentication

### Headers
- `/home/dcalford/CliWork/ScratchBird/include/scratchbird/security/auth_manager.h`
- `/home/dcalford/CliWork/ScratchBird/include/scratchbird/security/scram_auth.h`
- `/home/dcalford/CliWork/ScratchBird/include/scratchbird/security/mfa_auth.h`
- `/home/dcalford/CliWork/ScratchBird/include/scratchbird/security/password_policy.h`
- `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/permission_cache.h`
- `/home/dcalford/CliWork/ScratchBird/include/scratchbird/security/view_security.h`
- `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/data_masking.h`
- `/home/dcalford/CliWork/ScratchBird/include/scratchbird/security/tls_config.h`

## Test Files

| Test File | Coverage |
|-----------|----------|
| `/home/dcalford/CliWork/ScratchBird/tests/unit/test_security_issues.cpp` | General security tests |
| `/home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_security_auth_provider_rule_chain_contract.cpp` | Auth provider chains |
| `/home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_security_mfa_policy_enrollment_recovery_contract.cpp` | MFA enrollment/recovery |
| `/home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_security_acl_abac_graph_token_quota_settings_contract.cpp` | ACL/ABAC tests |
| `/home/dcalford/CliWork/ScratchBird/tests/integration/test_security_phase2.cpp` | Phase 2 integration |
| `/home/dcalford/CliWork/ScratchBird/tests/integration/test_security_phase3_3.cpp` | Column security |
| `/home/dcalford/CliWork/ScratchBird/tests/integration/test_security_phase3_4_rls.cpp` | RLS policies |
| `/home/dcalford/CliWork/ScratchBird/tests/integration/test_security_phase3_5_rls_dml.cpp` | RLS DML |

## Specification Template

See [../TEMPLATE.md](../TEMPLATE.md) for the specification format.

## Security Model Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Authentication Layer                      │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐       │
│  │   HBA    │ │  SCRAM   │ │   MFA    │ │  Plugin  │       │
│  │  Rules   │ │  SHA-256 │ │ TOTP/etc │ │ Registry │       │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘       │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                    Authorization Layer                       │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐         │
│  │    GRANT/    │ │   Permission │ │ View Security│         │
│  │    REVOKE    │ │     Cache    │ │  DEFINER/    │         │
│  │              │ │  (LRU+TTL)   │ │   INVOKER    │         │
│  └──────────────┘ └──────────────┘ └──────────────┘         │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                   Fine-Grained Access Control                │
│  ┌──────────────────┐ ┌──────────────────┐                  │
│  │ Row-Level Security│ │ Column-Level     │                  │
│  │ (RLS)             │ │ Security (CLS)   │                  │
│  │ - Policies        │ │ - Column Grants  │                  │
│  │ - Query Rewriting │ │ - Data Masking   │                  │
│  │ - RESTRICTIVE/    │ │ - Full/Partial   │                  │
│  │   PERMISSIVE      │ │   Masking        │                  │
│  └──────────────────┘ └──────────────────┘                  │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                   Data Protection Layer                      │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐         │
│  │  SSL/TLS     │ │  TDE         │ │  Audit       │         │
│  │  Transport   │ │  Encryption  │ │  Logging     │         │
│  └──────────────┘ └──────────────┘ └──────────────┘         │
└─────────────────────────────────────────────────────────────┘
```

## Key Source Anchors Summary

### Authentication Flow
- `auth_manager.cpp:1066` - AuthManager implementation
- `auth_manager.cpp:1193` - startAuthentication()
- `auth_manager.cpp:1417` - continueAuthentication()
- `scram_auth.cpp:176` - parseClientFirst()
- `scram_auth.cpp:243` - parseClientFinal()
- `mfa_auth.cpp:499` - MfaManager::createChallenge()
- `password_policy.cpp:42` - validatePassword()

### Authorization Model
- `permission_cache.cpp:79` - PermissionCache::lookup()
- `permission_cache.cpp:188` - PermissionCache::checkPermission()
- `catalog_manager.cpp:16618` - enforce_permissions()
- `catalog_manager.cpp:47580` - grantPermission()
- `view_security.cpp:153` - ViewSecurityManager::enterView()

### RLS Policy Enforcement
- `catalog_manager.cpp` - RlsPolicyRecord structure
- `sblr/executor.cpp` - applyRlsPredicates()

### CLS Column Masking
- `data_masking.cpp:67` - DataMasking::applyMasking()
- `data_masking.cpp:108` - DataMasking::applyPartialMasking()
- `catalog_manager.cpp:5253` - ColumnPermissionRecord

### SSL/TLS
- `tls_context.cpp:60` - tlsVersionToString()
- `tls_context.cpp:130` - getDefaultCiphers()

## Quick Reference

### Authentication Methods

| Method | Security | Use Case |
|--------|----------|----------|
| trust | ⚠️ Low | Local development only |
| password | ⚠️ Medium | Legacy compatibility |
| scram-sha-256 | 🔒 High | Recommended default |
| scram-sha-512 | 🔒 High | High security |
| ldap | 🔒 Medium-High | Enterprise integration |
| kerberos | 🔒 High | Enterprise SSO |
| cert | 🔒 High | mTLS/service accounts |
| jwt | 🔒 High | Modern applications |

### Privilege Summary

| Privilege | Tables | Columns | Sequences | Schemas | Functions |
|-----------|--------|---------|-----------|---------|-----------|
| SELECT | ✅ | ✅ | ✅ | ❌ | ❌ |
| INSERT | ✅ | N/A | ❌ | ❌ | ❌ |
| UPDATE | ✅ | ✅ | ✅ | ❌ | ❌ |
| DELETE | ✅ | N/A | ❌ | ❌ | ❌ |
| EXECUTE | ❌ | ❌ | ❌ | ❌ | ✅ |
| USAGE | ❌ | ❌ | ✅ | ✅ | ❌ |
| CREATE | ❌ | N/A | ❌ | ✅ | ❌ |

## Contributing

When adding new security features:

1. Update relevant specification documents
2. Add source anchors to implementation
3. Update test coverage tables
4. Update this README with new specs

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial comprehensive security specs | ScratchBird Team |
