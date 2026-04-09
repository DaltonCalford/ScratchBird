# Section 01 Specification Outline

## Owned topics

1. Bootstrap configuration source precedence
2. Bootstrap-only versus catalog-managed setting boundary
3. Core Config bootstrap contract
4. Service and listener ConfigParser bootstrap/import contract
5. Implemented default value sources and catalog seeding role
6. Service mode, front-door mode, manager proxy, and listener config models
7. Scalar configuration catalog versus dedicated listener-topology family split
8. File-backed reload and reconcile behavior
9. Persistent configuration versus session runtime control boundary
10. Cluster-config epoch and local configuration-generation validation boundary

## Canonical section files

1. CONFIG_CATALOG_AND_BOOTSTRAP.md
2. CONFIG_DEFAULTS.md
3. CONFIG_MODELS_WORKGROUP_AND_CLUSTER.md
4. CONFIG_SQL_SURFACE.md
5. DECISION_RECORD.md
6. DEPENDENCIES.md
7. TEST_CONTRACT.md

## Required guarantees

- bootstrap precedence remains command-line, then environment, then file, then default
- promoted persistent settings become catalog-backed after mount
- service and listener runtime bootstrap still begins from file-backed bootstrap
  config
- reload remains a bootstrap reread plus bounded live-apply and explicit
  reconcile behavior
- generic scalar keys and dedicated listener-topology rows remain separate
  durable families
- cluster configuration claims include epoch validation plus local generation
  and drift tracking where catalog-managed changes are deployed

## Explicit non-guarantees

- no ad hoc unmanaged key space is claimed
- no manager or listener-local truth is claimed
- no silent overwrite of committed catalog-managed state from file reload is
  claimed
- no machine-readable key-owner registry is claimed in the current implementation baseline
