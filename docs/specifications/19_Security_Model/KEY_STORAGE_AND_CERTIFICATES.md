# Key Storage and Certificates

Status: current_authority

## Storage authority

Bootstrap key and certificate material must originate from operator-controlled trusted sources declared by bootstrap configuration. Catalog tables, ad hoc SQL text, and transient client requests are not authority for bootstrap secrets.

## Required handling rules

- secret sources must be explicitly configured
- file or secret-source permissions must satisfy the configured hardening policy
- certificate chains and private-key pairing must validate before use
- expired, not-yet-valid, or untrusted certificates must be rejected when trust validation is required
- unencrypted secret material must not be copied into ordinary catalog-visible runtime structures

## Reload rule

Reload is permitted only for source types and channels whose lifecycle explicitly supports in-place refresh. Otherwise, restart is required.
