# SysArch, Sysadmin, and Cluster Admin Bootstrap Authority Model

Status: current_authority_with_beta1_beta2_bootstrap_contract

## Purpose

Define the administrative bootstrap authority hierarchy used by installer,
listener, parser, engine, and cluster-control surfaces so those layers do not
invent incompatible meanings for `SysArch`, named `sysadmin` principals, or
cluster-admin authority.

## Hard invariants

1. `SysArch` is a reserved administrative authority surface, not a generic
   user-facing role name that external clients may infer from ordinary catalog
   listings.
2. Named `sysadmin` principals are explicit operator-created identities and are
   members of the `sysadmin` group.
3. `SysArch` is the bootstrap authority that creates, rotates, disables,
   recovers, or removes named `sysadmin` principals unless a higher cluster
   authority owns that scope.
4. Cluster-admin authority supersedes node-local `SysArch` for cluster-scoped
   operations.
5. Bootstrap must fail closed if no valid `SysArch` authentication path exists.
6. The installer, listener, parser, and engine must never treat security
   bootstrap completion as satisfied by the mere presence of a `sysadmin`
   principal when `SysArch` bootstrap has not succeeded.

## Authority scopes

### Local `SysArch`

Local `SysArch` owns:

- first secure bootstrap of a standalone node
- creation of named `sysadmin` principals and their initial authentication
  material
- protected maintenance flows such as create, drop, backup, restore, and
  recovery-entry operations on a standalone node or database
- admission of the first local authentication path when cluster authority is
  absent

### Named `sysadmin` principals

Named `sysadmin` principals are:

- operator-chosen identities whose names are not product-fixed
- members of the `sysadmin` group
- delegated administrators for node, database, or cluster surfaces according to
  current policy
- recoverable and replaceable through `SysArch` or cluster-admin authority

### Cluster-admin authority

Cluster-admin authority owns:

- cluster-member registration and revocation
- cluster-scoped bootstrap policy
- cluster-wide principal, secret, and topology governance
- cluster-wide maintenance operations that supersede local-node authority
- override of node-local `SysArch` for cluster-owned operations

## Standalone rule

In standalone mode:

1. Local `SysArch` is authoritative.
2. Named `sysadmin` principals are optional at install time but strongly
   recommended.
3. If the operator defers named `sysadmin` creation, the installed node must
   still retain a valid local `SysArch` recovery path.

## Cluster rule

When cluster mode is admitted by the release program:

1. Local `SysArch` performs node-local bootstrap only until cluster authority is
   established.
2. Cluster-admin authority becomes the source of truth for cluster-scoped
   security and membership operations.
3. Local `SysArch` may retain a node-local recovery path only when cluster
   policy permits it.
4. Cluster join must fail closed if the node cannot prove the required cluster
   registration identity and policy state.

## Installer obligations

The installer must respect this hierarchy:

1. It must bootstrap at least one `SysArch` authentication path before the
   install can complete.
2. It must create named `sysadmin` principals only through the bootstrap
   authority model defined here.
3. It must clearly distinguish:
   - local `SysArch` bootstrap
   - named `sysadmin` bootstrap
   - future cluster-admin bootstrap
4. It must not present cluster-admin creation as a Beta 1 available action
   unless the release manifest marks cluster mode as available.
5. If cluster surfaces are present but not yet released, the installer may show
   them only as disabled, explanatory surfaces.

## Parser and listener obligations

1. Authentication negotiation may identify a named `sysadmin` principal, but
   that does not erase the reserved bootstrap meaning of `SysArch`.
2. Listener or parser layers must not silently map an unknown donor admin
   principal to local `SysArch`.
3. Cluster-scoped control requests must be rejected or redirected when only
   node-local authority is present.

## Catalog and policy obligations

1. Named `sysadmin` principals must be represented as ordinary principals with
   explicit membership in the `sysadmin` group.
2. `SysArch` may be implemented by reserved internal identity material,
   bootstrap-only principal state, or equivalent security metadata, but it must
   not be downgraded to an undocumented hard-coded username shortcut.
3. Cluster-admin authority must be represented in a way that can supersede
   node-local `SysArch` without ambiguity.

## Deterministic refusal conditions

- `SYSARCH_BOOTSTRAP_REQUIRED`
- `SYSARCH_AUTH_PATH_INVALID`
- `SYSADMIN_BOOTSTRAP_INVALID`
- `CLUSTER_ADMIN_SCOPE_REQUIRED`
- `CLUSTER_ADMIN_NOT_AVAILABLE_IN_RELEASE`
- `CLUSTER_REGISTRATION_POLICY_MISSING`

## Sample bootstrap policy shape

```yaml
security_bootstrap:
  mode: standalone
  sysarch:
    auth_paths:
      - provider: local_password
        enabled: true
        recovery_allowed: true
  sysadmin_bootstrap:
    create_initial_principals: true
    principals:
      - name: alice_admin
        groups: [sysadmin]
      - name: bob_admin
        groups: [sysadmin]
  cluster:
    release_state: planned_disabled
    cluster_admin_bootstrap: not_available
```
