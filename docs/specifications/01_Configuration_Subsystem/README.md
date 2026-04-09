# Section 01: Configuration Subsystem

Status: current_authority_with_reconstructed_expansion

## Current canonical authority

ScratchBird configuration is defined by a split authority model.

There are two active bootstrap or import layers:
- core Config for simple section and key lookup with command-line,
  environment, file, then default precedence
- service ConfigParser for richer service and listener config parsing with
  include handling, env expansion, and typed parsing helpers

There are two post-mount durable management families:
- scalar configuration rows defined by section `24`
  `CONFIGURATION_CATALOG_SCHEMA.md`
- dedicated listener topology, emulation binding, parser-pool, runtime-target,
  and generation rows defined by section `24`
  `LISTENER_TOPOLOGY_PARSER_POOL_AND_EMULATION_BINDING_CATALOG_MODEL.md`

The current section owns:
- bootstrap configuration source precedence
- first-mount seeding and reconcile rules that move bootstrap values into the
  durable catalog model
- implemented default value sources and registry-default fallback behavior
- service and listener configuration section models
- bootstrap-only versus catalog-managed setting boundaries
- file-backed reload and reconcile behavior and its boundaries
- the distinction between persistent configuration and session or transaction runtime controls
- cluster-config epoch and configuration-generation validation boundaries
- the rule that manager and listener are execution seams rather than durable
  configuration truth

This section does not claim:
- ad hoc unmanaged configuration keys outside the catalog registry
- silent file reload overwrite of committed catalog-managed settings
- manager or listener transport authority that bypasses engine validation and
  authorization
- a generated key-owner registry
- session or transaction runtime controls as the persistent configuration
  subsystem

## Current implementation baseline

- bootstrap configuration source precedence is still file-backed
- command-line overrides environment, environment overrides file, file overrides hardcoded defaults
- service and listener runtime config are parsed from file-backed config sections
- reload currently means re-reading config files and applying only the
  implemented reloadable subset
- current code now proves catalog-backed scalar configuration through
  `ALTER SYSTEM SET`, `ALTER SYSTEM RESET`, `SHOW CONFIG`, and
  `CONFIG HISTORY`
- current code now proves bootstrap reconciliation into dedicated
  listener-topology row families at open and reload time through the database
  bootstrap path
- dedicated listener-topology keys remain fail-closed outside the generic
  scalar `ALTER SYSTEM` surface
- generic SET and SHOW session controls are not the configuration subsystem; they are session or transaction runtime controls owned by adjacent sections
- the file-backed bootstrap substrate remains authoritative only for pre-mount
  and reconcile behavior and no longer acts as the sole persistent authority
  after mount

## Primary audit lookup anchors

- `src/core/config.cpp` search `Config::loadFile(` for the file-backed
  bootstrap precedence substrate.
- `src/core/config.cpp` search `Config::reload(` for the current reload path
  and fail-closed boundaries.
- `include/scratchbird/server/config_parser.h` search `class ConfigParser` for
  the richer service and listener bootstrap parser authority.
- `src/server/service_controller.cpp` search `Extended listener profile
  sections:` for the live listener-profile config parsing substrate.
- `src/core/database.cpp` search `bootstrapConfigurationCatalog(` for the
  bootstrap-to-catalog reconcile path that seeds scalar and listener-topology
  durable families.
- `src/sblr/executor.cpp` search `appendConfigChangeLogCatalogEntry(` for the
  live durable `ALTER SYSTEM` mutation and history path.

## File index

- CONFIG_CATALOG_AND_BOOTSTRAP.md
- CONFIG_DEFAULTS.md
- CONFIG_MODELS_WORKGROUP_AND_CLUSTER.md
- CONFIG_SQL_SURFACE.md
- DECISION_RECORD.md
- DEPENDENCIES.md
- SPEC_OUTLINE.md
- TEST_CONTRACT.md

## File Index
<!-- AUTO-GENERATED:FILE-LIST:START -->
- [CONFIG_CATALOG_AND_BOOTSTRAP.md](CONFIG_CATALOG_AND_BOOTSTRAP.md)
- [CONFIG_DEFAULTS.md](CONFIG_DEFAULTS.md)
- [CONFIG_MODELS_WORKGROUP_AND_CLUSTER.md](CONFIG_MODELS_WORKGROUP_AND_CLUSTER.md)
- [CONFIG_SQL_SURFACE.md](CONFIG_SQL_SURFACE.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->

## Maintenance
- Update file list with `../skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh`.
