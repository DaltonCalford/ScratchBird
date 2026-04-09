# Configuration Models and Cluster State

## Current deployment vocabulary

The current code-backed deployment vocabulary is:
- single-database versus multi-database service mode
- direct versus manager-proxy front-door mode
- listener mode and manager-coupled listener behavior

These remain real bootstrap and runtime choices, but they are no longer the
only durable authority for promoted settings after mount.

## Persistent configuration ownership model

After mount, durable configuration truth is split into:
- scalar catalog-managed settings in `sys.config.*`
- dedicated listener profile, binding, emulation, parser-pool, runtime-target,
  and generation rows

Bootstrap files remain the seed or break-glass vocabulary, not the only
durable truth after mount.

## Manager proxy model

Current manager proxy configuration includes concrete service-facing fields such as:
- bind addresses and ports
- owner database
- listener identifier
- DBBT timing and replay-cache controls
- internal native listener bind settings

## Listener mode model

Listeners currently derive mode and runtime behavior from the same file-backed configuration model used by service startup and reload.

## Cluster state boundary

Current cluster-related configuration authority is limited to:
- durable cluster identity publication
- node identity publication
- cluster_config_epoch publication and mismatch checks
- target-local configuration generation for catalog-managed settings
- remote-management deployment history and drift state in section `24`

The canonical model is no longer epoch-validation alone.

Required rules:
1. target-local durable settings publish a new local configuration generation on
   commit
2. remote deployment history must record the local generation assessed and the
   local generation applied
3. mismatch handling must distinguish cluster-config epoch mismatch from local
   generation lag, refusal, or drift
4. manager and listener may report capability or apply outcomes, but they are
   not the only durable record

## Explicit unsupported claims

This section does not claim current support for:
- manager or listener-local truth outranking catalog rows
- silent file import that overwrites committed catalog state
- unrestricted arbitrary key replication outside the catalog registry
- full distributed consensus as a requirement for every configuration action
