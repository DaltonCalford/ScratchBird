# Security

**Last Updated:** 2026-01-30

ScratchBird enforces security in the engine. Parsers and listeners are treated as
untrusted and cannot bypass authentication, authorization, or auditing.

## Security levels (0-6)

ScratchBird supports seven security levels that progressively add requirements.
Summary highlights:

- Level 0 (Open): No auth, no authorization, no encryption.
- Level 1 (Authenticated): Authentication + RBAC/GBAC + ownership.
- Level 2 (Encrypted): Adds encryption at rest and backup encryption.
- Level 3 (Policy-Controlled): Adds row/column security and external auth support.
- Level 4 (Audited): Full audit logging and tamper evidence.
- Level 5 (Network-Hardened): TLS/mTLS required for network access.
- Level 6 (Cluster-Hardened): Quorum and multi-party controls.

In strict mode, direct SQL access to sys.* is denied; use engine-owned virtual
views and SHOW/DESCRIBE commands instead.

## Authentication

ScratchBird implements multiple authentication methods in the Alpha codebase:

**Password-based:**
- SCRAM-SHA-256 (PostgreSQL-compatible)
- Local password authentication with hashing
- Host-based authentication (HBA) rules
- Account lockout and password policy enforcement

**External authentication:**
- LDAP / Active Directory (`ldap_auth.cpp`)
- Kerberos / GSSAPI (`kerberos_auth.cpp`)
- OAuth 2.0 (`oauth_auth.cpp`)
- SAML (`saml_auth.cpp`)

**Additional methods:**
- Multi-factor authentication / MFA (`mfa_auth.cpp`)
- TLS client certificate authentication (`cert_auth.cpp`)
- Login attempt tracking for brute-force protection (`login_attempt_tracker.cpp`)

## Authorization model

ScratchBird combines RBAC, GBAC, and object-level ACLs:

- Direct user privileges (always active).
- One active role at a time (SET ROLE) for role privileges.
- Groups are cumulative and always active.
- PUBLIC privileges apply to all authenticated users.
- Object ownership grants implicit full control to the owner.

Role composition is supported via GRANT ROLE TO ROLE, enabling hierarchical
roles and privilege inheritance.

## Row and column security

- Row-Level Security (RLS) restricts row visibility by policy.
- Column-Level Security (CLS) restricts column access and supports masking.
- Policies are required at security levels 3 and above.

## Encryption and transport

Depending on security level and configuration, ScratchBird supports:

- Transparent Data Encryption (TDE).
- Write-after log encryption (WAL terminology used for compatibility).
- Encrypted backups.
- TLS for network connections, mTLS for administrative access.

## Auditing

Audit events include authentication, authorization, and schema operations.
Higher security levels require chain-hashed audit logs for tamper evidence.

## References

- `docs/specifications/Security Design Specification/00_SECURITY_SPEC_INDEX.md`
- `docs/specifications/Security Design Specification/09_SECURITY_LEVELS.md`
- `docs/specifications/Security Design Specification/02_IDENTITY_AUTHENTICATION.md`
- `docs/specifications/Security Design Specification/EXTERNAL_AUTHENTICATION_DESIGN.md`
- `docs/specifications/Security Design Specification/03_AUTHORIZATION_MODEL.md`
- `docs/specifications/Security Design Specification/ROLE_COMPOSITION_AND_HIERARCHIES.md`
