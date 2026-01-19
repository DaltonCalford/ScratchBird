# Dialect Authentication Mapping Specification

Version: 1.0
Status: Draft (Alpha IP layer)
Last Updated: January 2026

## Purpose

Define how protocol-specific authentication methods map to ScratchBird
authentication providers and security contexts. Parsers handle wire
handshakes but the engine performs credential validation.

## Scope

- PostgreSQL, MySQL, Firebird, ScratchBird native protocols
- Mapping to SB auth providers (password, SCRAM, certificate, MFA)
- Configuration-driven allow/deny for auth methods per protocol

Out of scope:
- External identity providers (LDAP/Kerberos/SAML/OAuth) wiring (next phase)

## Core Rules

1. Parser never validates credentials; it only relays proofs to the engine.
2. Engine validates auth proofs using SB auth providers.
3. Each protocol has an allowlist of permitted auth methods.
4. On success, engine returns SB user_id, role_id, and security context.

## ScratchBird Native

Supported methods (Alpha):
- PASSWORD
- SCRAM-SHA-256
- CERTIFICATE (if TLS enabled)

Mapping:
- Native PASSWORD -> SB Password provider
- Native SCRAM -> SB SCRAM provider
- Native CERT -> SB Certificate provider

## PostgreSQL

Protocol methods:
- AUTH_OK
- AUTH_CLEAR_TEXT
- AUTH_MD5
- AUTH_SASL (SCRAM)

Mapping:
- AUTH_CLEAR_TEXT -> SB Password provider (if allowed)
- AUTH_MD5 -> SB Password provider (MD5 proof)
- AUTH_SASL(SCRAM) -> SB SCRAM provider

Notes:
- AUTH_OK is only valid for trusted local connections.
- SCRAM-PLUS and channel binding require TLS info to be passed to engine.

## MySQL

Protocol plugins:
- mysql_native_password
- caching_sha2_password
- sha256_password

Mapping:
- mysql_native_password -> SB Password provider (legacy hash)
- caching_sha2_password -> SB SCRAM or Password provider (config)
- sha256_password -> SB Password provider

Notes:
- TLS requirement is configuration-driven; parser must report tls_active.

## Firebird

Protocol methods:
- SRP
- Legacy

Mapping:
- SRP -> SB Password provider (SRP proof)
- Legacy -> SB Password provider (legacy hash) if enabled

## Configuration

Per-protocol allowlist (example):

```
protocol.postgres.auth_methods = ["scram-sha-256", "md5"]
protocol.mysql.auth_plugins = ["caching_sha2_password"]
protocol.firebird.auth_plugins = ["srp"]
protocol.native.auth_methods = ["password", "scram-sha-256", "certificate"]
```

If a client requests a disallowed method, the parser must return a
protocol-specific error without exposing engine details.

## Output Security Context

After validation, engine returns:
- user_id
- role_id (if specified)
- is_superuser
- default_schema_id
- policy_epoch_global / policy_epoch_table

Parser must pass these identifiers in every SBLR execution request.

## Related Specs

- docs/specifications/Security Design Specification/AUTH_CORE_FRAMEWORK.md
- docs/specifications/Security Design Specification/AUTH_PASSWORD_METHODS.md
- docs/specifications/Security Design Specification/AUTH_CERTIFICATE_TLS.md
- docs/specifications/wire_protocols/*.md
