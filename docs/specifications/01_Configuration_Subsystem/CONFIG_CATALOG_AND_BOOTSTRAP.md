# Configuration Bootstrap and Authority

## Canonical authority split

Configuration authority has two phases.

### Phase 1: pre-mount bootstrap authority

Before the database-resident catalog can be opened, configuration is sourced
from:
1. command-line overrides
2. environment overrides
3. configuration file values
4. hardcoded defaults

Core bootstrap lookup is owned by core `Config`.

This phase is authoritative only for:
- locating and opening the service or database
- break-glass and bootstrap-only startup behavior
- first-mount seeding of the durable configuration catalog
- explicit reconcile or import decisions initiated after reload

### Phase 2: post-mount durable authority

After mount, durable truth for promoted settings is catalog-backed.

The post-mount authority families are:
- scalar settings in section `24`
  `CONFIGURATION_CATALOG_SCHEMA.md`
- dedicated listener profile, binding, emulation, parser-pool, runtime-target,
  and generation rows in section `24`
  `LISTENER_TOPOLOGY_PARSER_POOL_AND_EMULATION_BINDING_CATALOG_MODEL.md`

Manager and listener processes may execute approved changes, but they are not
the durable source of configuration truth.

## Core Config contract

Core `Config` remains the simple bootstrap configuration authority for pre-mount
consumers and for bounded import or reconcile workflows.

Current proved capabilities:
- initialize from a config file path
- store command-line overrides by section and key
- read string, integer, unsigned integer, boolean, and double values
- enumerate sections
- reload from the remembered file path
- clear in-memory state

Required Beta 1 role after this package:
- remain the bootstrap substrate used to seed or reconcile durable catalog
  state
- stop being the only persistent authority for promoted post-mount settings

## Service and listener parser contract

Service and listener runtime config use the richer service `ConfigParser`
layer.

Current proved capabilities in that layer include:
- file parsing with section awareness
- include handling
- environment expansion
- typed parsing helpers for sizes and durations
- normalized section-name handling
- strict parse modes for service-facing config

Required Beta 1 role after this package:
- parse bootstrap files and import candidates
- seed missing scalar config rows and dedicated listener-topology rows on first
  mount
- support reconcile or import workflows without silently overwriting committed
  catalog truth

## First-mount seeding rule

On first mount of a database or managed instance:
- scalar promoted settings must be seeded into `sys.config.key`,
  `sys.config.value`, and `sys.config.change_log` where applicable
- listener profile and parser-pool seed values must be translated into the
  dedicated listener-topology row families rather than remaining only in
  listener-local files
- seed writes are ordinary transactional catalog writes and publish only on
  commit

## Reopen and restart rule

On reopen:
- catalog-backed values are authoritative for promoted settings
- bootstrap files may still supply bootstrap-only or break-glass values
- file, environment, or command-line values for catalog-managed settings must
  not silently overwrite committed catalog rows

## Reload and reconcile rule

Reload continues to reread the remembered bootstrap sources, but its role is
bounded:
- bootstrap-only keys may reapply directly where the implementation supports
  live reload
- missing catalog-managed rows may be seeded during first-mount or explicit
  reconcile workflows
- existing committed catalog-managed rows may be changed only through durable
  catalog mutation, not by blind in-memory overwrite

## Current section model

Current service-facing config is centered on implemented file sections such as:
- server
- network
- listener dot profile sections
- manager
- drivers
- memory
- logging
- statistics
- audit
- security

Those file sections are the bootstrap or import vocabulary, not the only
durable truth after mount.

## Explicit rejected interpretations

This section rejects the following as canonical Beta 1 behavior:
- file-backed settings remaining the only durable truth after mount
- manager or listener transport becoming the authority because it can carry a
  change request
- generic key-value storage replacing the dedicated listener-topology row
  families
