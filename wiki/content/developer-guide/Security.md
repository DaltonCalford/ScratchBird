# Security

**Purpose:** Documents ScratchBird's security architecture - authentication methods, authorization model, encryption, and audit.

**Last Updated:** 2026-01-30

---

## Overview

The engine is the sole authority for authentication and authorization decisions.
Parsers/listeners are untrusted and cannot bypass enforcement.

---

## Authentication Methods

ScratchBird implements multiple authentication mechanisms in the security subsystem (`src/security/`):

| Method | Implementation | Description |
|--------|----------------|-------------|
| SCRAM-SHA-256 | `scram_auth.cpp` | PostgreSQL-compatible password authentication |
| Kerberos/GSSAPI | `kerberos_auth.cpp` | Enterprise single sign-on |
| LDAP | `ldap_auth.cpp` | Directory service authentication |
| OAuth 2.0 | `oauth_auth.cpp` | Token-based authentication |
| SAML | `saml_auth.cpp` | Federated identity |
| MFA | `mfa_auth.cpp` | Multi-factor authentication |
| TLS Certificate | `cert_auth.cpp` | Client certificate authentication |

**Infrastructure:**
- `auth_manager.cpp` - Central authentication orchestration
- `auth_method.cpp` - Authentication method base class
- `tls_context.cpp` - TLS session management

---

## Password Security

| Component | Implementation | Description |
|-----------|----------------|-------------|
| Password hashing | `src/core/password_hash.cpp` | Secure password storage |
| Password policy | `src/core/password_policy.cpp`, `src/security/password_policy.cpp` | Configurable strength and expiration rules |
| Login tracking | `src/core/login_attempt_tracker.cpp` | Brute-force protection |

---

## Authorization

| Component | Implementation | Description |
|-----------|----------------|-------------|
| Permission cache | `src/core/permission_cache.cpp` | Cached privilege lookups |
| View security | `src/security/view_security.cpp` | View-level security enforcement |
| Security quorum | `src/core/security_quorum.cpp` | Multi-party authorization for sensitive operations |

---

## Encryption and Data Protection

| Component | Implementation | Description |
|-----------|----------------|-------------|
| Data encryption | `src/core/data_encryption.cpp` | Transparent data encryption at rest |
| Key management | `src/core/encryption_key_manager.cpp` | Encryption key lifecycle |
| Data masking | `src/core/data_masking.cpp` | Column-level data masking |

---

## Audit

| Component | Implementation | Description |
|-----------|----------------|-------------|
| Audit logger | `src/core/audit_logger.cpp` | Structured audit trail |
| Structured logger | `src/core/structured_logger.cpp` | Structured logging infrastructure |

---

## Primary Specs

- `docs/specifications/Security Design Specification/00_SECURITY_SPEC_INDEX.md`
- `docs/specifications/Security Design Specification/01_SECURITY_ARCHITECTURE.md`
- `docs/specifications/Security Design Specification/03_AUTHORIZATION_MODEL.md`
- `docs/specifications/Security Design Specification/04_ENCRYPTION_KEY_MANAGEMENT.md`
- `docs/specifications/Security Design Specification/08_AUDIT_COMPLIANCE.md`

---

## Related Documents

- [Architecture](Architecture.md) - Trust model and layer boundaries
- [Network and Listeners](Network-Listeners.md) - TLS and connection security
- [User Guide: Security](../user-guides/Security.md) - User-facing security guide
- [Admin: Security](../admin/security.md) - Administration security guide
