# Configuration Control Surface

## Canonical persistent configuration authority

Persistent configuration has two durable families after mount:
- scalar catalog-managed settings stored in `sys.config.key`,
  `sys.config.value`, and `sys.config.change_log`
- dedicated listener-topology, emulation-binding, parser-pool, runtime-target,
  and generation rows defined by section `24`

Bootstrap files, environment, and command-line remain authoritative only for:
- pre-mount startup and break-glass behavior
- first-mount seeding
- explicit reconcile or import workflows

For catalog-managed settings, file reload alone is never the only durable
record.

## Required reconstructed unified management layer

The cluster layer is the authoritative remote-control plane for promoted
configuration and administrative settings.

Required remote-management rule:
- remote changes are queued, assessed, deployed, queried, and audited through
  the cluster layer
- the server-local manager receives and dispatches approved work
- the engine-owned admin surface validates and binds the request
- the target database persists the accepted local setting change
- the cluster layer persists the deployment instruction and outcome history

For cluster-managed settings, file reload alone is not sufficient as the only
durable record.

Required managed setting families include, at minimum:
- listener topology and emulation loading
- parser pool policy
- plugins and extension enablement
- authentication configuration
- security configuration
- memory allocation and resource budgets
- other promoted engine and runtime settings

The manager and listener are transport and execution seams only.
They are not the durable source of configuration truth.

## Required scalar management surfaces

The canonical scalar management surfaces are:
- `SHOW CONFIG`
- `ALTER SYSTEM SET section.key = value`
- `ALTER SYSTEM RESET section.key`
- `CONFIG HISTORY`
- `CONFIG RELOAD`

Current code now proves the bounded Beta 1 durable form of the scalar surfaces
above. Remote-management orchestration remains a separate concern, but accepted
local scalar mutations are no longer transient-only.

### `ALTER SYSTEM SET`

Canonical rules:
- superuser or equivalently authorized admin only
- database and catalog must be available
- key must be non-empty and registered in `sys.config.key`
- generic scalar key space is limited to scalar catalog-managed settings; it
  must not be used to mutate listener profile, binding, emulation-binding, or
  parser-pool rows after catalog bootstrap
- type, range, allowed-value, scope, restart, and cluster-management
  constraints must be validated before mutation is staged
- accepted mutation writes `sys.config.value` and `sys.config.change_log`
  transactionally
- accepted mutation advances the relevant configuration generation only on
  commit
- hot-apply is optional per key and may occur only after the durable catalog
  write is accepted

Current code-backed substrate:
- validates registered catalog-managed keys
- writes `sys.config.value` and `sys.config.change_log`
- rejects dedicated listener-topology keys from the generic scalar path
- reconciles bootstrap-derived effective values after durable mutation
- may invoke bounded apply hooks for currently wired keys such as scheduler and
  dormant-transaction policy settings

### `ALTER SYSTEM RESET`

Canonical rules:
- remove the active scalar override for the target scope
- write a durable history entry
- advance configuration generation on commit
- fall back to inherited or registry-default value after commit publication

Current code-backed surface:
- retires the durable override row
- appends a durable history record
- republishes the inherited or bootstrap value in `SHOW CONFIG`

### `CONFIG RELOAD`

Canonical authority for reload:
- re-read bootstrap sources
- reapply only bootstrap-only keys directly where live reload is supported
- seed missing catalog-managed rows during first-mount or explicit import or
  reconcile workflows
- refuse silent overwrite of committed catalog-managed scalar rows or dedicated
  listener-topology rows
- emit durable reconcile or refusal evidence when an external bootstrap value
  conflicts with committed catalog truth

Current code-backed surface:
- reloads the bootstrap file when present
- reruns `bootstrapConfigurationCatalog(...)`
- preserves committed catalog-managed scalar overrides while reseeding missing
  catalog rows

### `SHOW CONFIG`

Canonical rules:
- expose effective scalar configuration values together with scope, source,
  pending-restart state, and committed generation
- report registry-default values distinctly from explicit override rows
- expose only scalar catalog-managed settings; dedicated listener-topology rows
  are not flattened into synthetic generic keys

Current code-backed surface:
- reads config key and value catalogs
- resolves effective bootstrap-versus-catalog value source
- exposes catalog-managed scalar truth without flattening dedicated listener
  topology families

### `CONFIG HISTORY`

Canonical rules:
- read the durable `sys.config.change_log`
- expose changed key, scope, old value, new value, actor, reason, time, and
  committed configuration generation

Current code-backed surface:
- reads the durable config history catalog and exposes committed change-log
  entries to SQL clients

## Dedicated topology mutation boundary

The following are not generic `ALTER SYSTEM SET` keys after catalog bootstrap:
- listener profile identity and enablement
- bind address and port tuples
- emulation-family bindings
- parser-pool policy rows
- runtime-target rows
- listener generation and drift rows

These surfaces mutate through dedicated catalog object or admin flows, or
through remote-management instructions, but their durable truth still lives in
catalog rows.

## Runtime controls adjacent to this section

ScratchBird exposes runtime controls through generic `SET`, `SHOW`, and admin
paths, but those are not the persistent configuration subsystem.

Examples include:
- autocommit visibility
- statement timeout
- search path and current schema
- parser selection
- operator strict mode
- transaction isolation and related transaction settings

These are session or transaction runtime surfaces owned by adjacent sections.

## Listener-control boundary

The canonical listener-control route is:
- cluster layer or engine admin surface decides policy
- engine or admin surface decides policy
- controller issues bounded listener-management request
- listener executes only the approved runtime action

If SQL-admin listener control is promoted later, it must route through that
same engine-to-controller-to-listener seam rather than turning the listener
into the source of configuration truth.

If cluster-managed remote control is used, it must route:
- cluster layer
- server-local manager
- engine admin surface
- controller when listener work is required
- listener runtime action

No remote control plane may bypass engine validation and authorization.

## Boundary

This section rejects any claim that post-mount persistent truth remains only
file-backed or listener-local.

It also rejects any claim that generic scalar key storage can replace the
dedicated listener-topology families.
