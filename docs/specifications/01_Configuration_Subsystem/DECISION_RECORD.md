# Decision Record

## DR-01-01: file-backed bootstrap remains the pre-mount authority

The bootstrap precedence model remains command-line, then environment, then
file, then default until the database-resident catalog can be opened.

## DR-01-02: post-mount persistent truth is catalog-backed

After mount, promoted scalar settings and listener-topology settings must be
persisted in catalog rows. Bootstrap files and process-local mutation helpers
become substrate and import layers, not the only durable authority.

## DR-01-03: two parser layers are real and both are authoritative in their bootstrap scope

The current code has two distinct layers:
- core Config for simple bootstrap lookup
- service ConfigParser for richer service and listener config

The section treats both as real authority rather than forcing them into one fictional unified parser.

## DR-01-04: generic scalar settings and listener topology are separate durable families

Generic scalar keys belong in `sys.config.*`.
Listener profiles, bindings, emulation bindings, parser-pool policies, runtime
targets, and generation rows belong in dedicated catalog tables and must not be
flattened into generic key-value storage after catalog bootstrap.

## DR-01-05: reload is a reconcile/import path, not a silent overwrite path

Reload is not a universal dynamic configuration system. It is a bootstrap
source reread path with a limited set of live-applied fields plus explicit
reconcile/import behavior for catalog-managed settings. It must not silently
overwrite committed catalog rows.

## DR-01-06: generic SET and SHOW session controls are not this section's persistent config surface

Session variables, autocommit, statement timeout, search path, parser selection, operator strict mode, and transaction settings are runtime controls. They are not a persistent configuration catalog and must not be documented as one.

## DR-01-07: cluster_config_epoch alone is not configuration deployment

`cluster_config_epoch` remains a real cross-node validation anchor, but it does
not by itself prove deployment history, target-local generation, or drift
classification. Those require the remote-management and generation records
defined in section `24`.
