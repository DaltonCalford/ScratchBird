# Findings: INSTALLATION_AND_INITIALIZATION_SPECIFICATION.md

Spec file: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/INSTALLATION_AND_INITIALIZATION_SPECIFICATION.md`

## Authoritativeness
- The file is labeled "Non-Authoritative Reference" at the top, and it is **not** listed in `docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`. This makes it non-authoritative for V3 conformance.

## Summary
Most of the installation/initialization flow described in the spec (packaging scripts, sb_setup wizard, registry/security database initialization, sb_security certificate tooling, verification script, health check endpoint) is **not implemented** in source. There is partial support for config parsing and a `--check` mode in `sb_server`, and there are test/deployment scripts that approximate some setup steps (directory creation, ad-hoc TLS cert generation), but these are not integrated as a full installer or first-run wizard as specified.

## Implemented (Partial)
- **Config parsing and path discovery**: `ConfigParser` exists with INI parsing and default config search paths; `sb_server` supports `--config` and `--check` (config check and exit).
  - Evidence: `src/server/config_parser.cpp`, `src/server/service_controller.cpp` (help text and `--check`).
- **TLS runtime support**: TLS context and certificate loading/validation exists, but no certificate generation or management CLI is implemented.
  - Evidence: `src/security/tls_context.cpp`.
- **Ad-hoc setup scripts** (not productized): `scripts/setup-test-server.sh` and `scripts/setup-test-server-minimal.sh` create directories and generate self-signed certs for test environments.

## Gaps / Discrepancies
1. **Packaging installation scripts (DEB/RPM preinst/postinst)**
- No packaging scripts or directory creation/permission logic are implemented in the repo as specified. The only near matches are test-server scripts, not installer assets.

2. **Windows MSI (WiX) structure**
- No WiX MSI components or Windows installer definitions are present in source.

3. **TLS certificate generation and `sb_security` tooling**
- Spec defines a certificate generation script and a `sb_security` CLI for certificate operations. No `sb_security` tool or certificate-generation implementation is present in `src/` or `tools/`.

4. **Default configuration template**
- Spec requires a comprehensive `sb_server.conf` template with many sections. Repo only has `sb_config.ini.example` and user documentation; no generator or template matching the spec is present in code.

5. **Config validation**
- Spec defines a validation layer (paths, TLS cert/key match, port availability, memory checks). `ConfigParser` supports validators but none are registered anywhere; no validation logic is wired.

6. **Database registry initialization**
- Spec describes an SQLite-based registry with tables per `DATABASE_REGISTRY_SPECIFICATION.md`. No `DatabaseRegistry` implementation or registry init code exists in source.

7. **Security database initialization**
- Spec describes a separate security database with tables like `security_users` and `security_roles`. No corresponding implementation exists in source.

8. **First-run configuration wizard (`sb_setup`)**
- Spec describes interactive/non-interactive `sb_setup` flows. No `sb_setup` tool or code exists in `src/` or `tools/`.

9. **Post-install verification script**
- Spec defines a `verify_installation.sh` flow. No such script exists in the repo.

10. **Health check endpoint**
- Spec defines a `HealthCheck` service; no server health endpoint/implementation is present in source.

## Notes
- `sb_server` defaults to `/etc/scratchbird/sb_server.conf` and supports `--check`, which partially aligns with the verification step, but there is no full verification script or registry/security DB presence checks.
- User documentation includes installation guidance and configuration references, but these are not implementation artifacts.

## Suggested Next Steps
- Decide whether to formalize installer tooling (DEB/RPM scripts, MSI, sb_setup, sb_security) or downgrade this spec to non-authoritative guidance only.
- If installer tooling is required, define the concrete code ownership and implement `sb_setup` + registry/security DB initialization.
