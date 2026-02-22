# Auth Hardening Update (2026-02-21)

Implemented in current cycle:
- Bootstrap hardening: token proof, single-use consume/revoke, null-catalog rejection.
- Catalog-backed auth policy and lockout enforcement.
- Token auth (AuthKey scope/expiry/revocation) and MFA continuation/step-up hooks.
- Peer identity mapping enforcement for local IPC auth.
- Expanded auth audit events for policy decisions, token usage/revocation, and managed preface decisions.

Primary touchpoints:
- `src/core/auth_provider.cpp`
- `src/core/catalog_manager.cpp`
- `src/server/server_session.cpp`
- `src/core/audit_logger.cpp`
