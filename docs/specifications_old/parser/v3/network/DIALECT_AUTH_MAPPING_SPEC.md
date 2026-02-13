# Dialect Authentication Mapping Specification

Version: 1.1
Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define how protocol-specific authentication methods map to ScratchBird
authentication providers and security contexts. Parsers handle wire
handshakes but the engine performs credential validation.

## Scope

- PostgreSQL, MySQL, Firebird, ScratchBird native protocols
- Mapping to SB auth providers (password, SCRAM, certificate, MFA)
- Configuration-driven allow/deny for auth methods per protocol

Not covered in this document:
- External identity providers (LDAP/Kerberos/SAML/OAuth) wiring is rejected in V3
- TDS/MSSQL is not supported and MUST be rejected

## Core Rules

1. Parser never validates credentials; it only relays proofs to the engine.
2. Engine validates auth proofs using SB auth providers.
3. Each protocol has an allowlist of permitted auth methods.
4. On success, engine returns SB user_id, role_id, and security context.
5. Any disallowed method MUST be rejected by the parser with a protocol-appropriate error.

## ScratchBird Native

Supported methods (V3):
- PASSWORD
- SCRAM-SHA-256
- CERTIFICATE (if TLS enabled)

Mapping:
- Native PASSWORD -> SB Password provider
- Native SCRAM -> SB SCRAM provider
- Native CERT -> SB Certificate provider

## PostgreSQL (16+)

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

## MySQL (8.x)

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

## Firebird (5.x)

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

## Auth Proof Payload

Parsers MUST send the following fields to the engine for validation:
- `protocol_id:u8`
- `auth_method:string`
- `username:string`
- `database:string` (empty allowed for Firebird)
- `tls_active:bool`
- `tls_peer_identity:opt<string>`
- `proof_bytes:bytes` (method-specific)

## Output Security Context

After validation, engine returns:
- `user_id` (SBDB$KEY_USER)
- `role_id` (SBDB$KEY_ROLE; optional)
- `is_superuser` (bool)
- `default_schema_id` (SBDB$KEY_SCHEMA)
- `policy_epoch_global` / `policy_epoch_table`

Parser must pass these identifiers in every SBLR execution request.

## Error Handling

- Disallowed method -> protocol-specific auth error (no engine call)
- Malformed proof -> `28P01` (auth failed)
- Unsupported protocol -> `SB_NET_UNSUPPORTED_PROTOCOL`

## Related Specs

- `docs/specifications/parser/v3/wire_protocols/*.md`
- `docs/specifications/parser/v3/network/ENGINE_PARSER_IPC_CONTRACT.md`
